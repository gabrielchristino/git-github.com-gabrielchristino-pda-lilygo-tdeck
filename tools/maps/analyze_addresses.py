"""Measure a compact offline address index for the existing Pinheiros map.

Analysis artifacts only; does not change the map, firmware or device.
Input: IBGE CNEFE municipal GeoJSON ZIP (September 2024 edition).
"""
import argparse
from collections import Counter
import datetime
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import unicodedata
import zipfile
import zlib

from build_pinheiros import BBOX, project

ROOT = Path(__file__).resolve().parents[2]
SOURCE_URL = ('https://ftp.ibge.gov.br/Cadastro_Nacional_de_Enderecos_para_Fins_Estatisticos/'
              'Censo_Demografico_2022/Arquivos_CNEFE/GeoJSON/Municipio_20240910/'
              'qg_810_endereco_Munic3550308.json.zip')


def canonical(text):
    text = ''.join(c for c in unicodedata.normalize('NFKD', text.upper())
                   if not unicodedata.combining(c))
    text = re.sub(r'[^A-Z0-9]+', ' ', text).strip()
    for short, full in [('AV ', 'AVENIDA '), ('R ', 'RUA '), ('PTE ', 'PONTE '),
                        ('EST ', 'ESTRADA '), ('TV ', 'TRAVESSA ')]:
        if text.startswith(short):
            return full + text[len(short):]
    return text


def features(stream):
    """Read the 2.3 GB collection without loading it into memory."""
    buffer = stream.read(65536)
    start = re.search(r'"features"\s*:\s*\[', buffer)
    if not start:
        raise ValueError('Missing GeoJSON features array')
    position = start.end()
    decoder = json.JSONDecoder()
    while True:
        while position < len(buffer) and buffer[position] in ' \r\n\t,':
            position += 1
        if position == len(buffer):
            buffer, position = stream.read(65536), 0
            if not buffer:
                raise ValueError('Unexpected end of GeoJSON')
            continue
        if buffer[position] == ']':
            # Finish reading the ZIP member to validate its CRC.
            while stream.read(1024 * 1024):
                pass
            return
        try:
            feature, end = decoder.raw_decode(buffer, position)
        except json.JSONDecodeError:
            extra = stream.read(65536)
            if not extra:
                raise
            buffer, position = buffer[position:] + extra, 0
            if len(buffer) > 4_000_000:
                raise ValueError('Unexpectedly large feature')
            continue
        yield feature
        position = end
        if position >= 65536:
            buffer, position = buffer[position:] + stream.read(65536), 0


class Strings:
    def __init__(self):
        self.data = bytearray(b'\0')
        self.offsets = {'': 0}

    def add(self, text):
        if text not in self.offsets:
            self.offsets[text] = len(self.data)
            self.data.extend(text.encode('utf-8') + b'\0')
        return self.offsets[text]


def pack(groups, display, output):
    """16-byte records; per-street index permits bounded binary lookup.

    Record: street ID u32, number-string offset u32, x/y i16, CEP+quality u32.
    Index: display-name offset, normalized-name offset, first record, count (u32).
    CEP uses 28 bits; original geocoding level occupies the top four bits.
    Full number text (including modifiers) is stored in a deduplicated pool.
    """
    streets = sorted({key[0] for key in groups})
    ids = {street: i for i, street in enumerate(streets)}
    strings, numbers = Strings(), Strings()
    records, index = bytearray(), bytearray()
    ordered = sorted(groups.items())
    first, previous = 0, None
    street_first = {}
    for (street, number, cep), item in ordered:
        if street != previous:
            street_first[street] = first
            previous = street
        x, y, level = item['x'], item['y'], item['level']
        assert -32768 <= x <= 32767 and -32768 <= y <= 32767
        assert 0 <= cep < (1 << 28) and 0 <= level < 16
        records.extend(struct.pack('<IIhhI', ids[street], numbers.add(number), x, y, cep | (level << 28)))
        first += 1
    for i, street in enumerate(streets):
        begin = street_first[street]
        end = street_first[streets[i+1]] if i+1 < len(streets) else len(groups)
        index.extend(struct.pack('<IIII', strings.add(display[street]), strings.add(street), begin, end-begin))
    header = struct.pack('<8s6I', b'PDAADR01', len(groups), len(streets), 16,
                         len(index), len(strings.data), len(numbers.data))
    payload = header + index + strings.data + numbers.data + records
    output.write_bytes(payload)
    # Verify all records against the representation written to disk.
    assert len(payload) == 32 + 16*len(streets) + len(strings.data) + len(numbers.data) + 16*len(groups)
    offset = len(payload)-len(records)
    for i, ((street, number, cep), item) in enumerate(ordered):
        sid, num, x, y, packed = struct.unpack_from('<IIhhI', payload, offset+i*16)
        assert streets[sid] == street and (x, y) == (item['x'], item['y'])
        assert (packed & 0x0FFFFFFF) == cep and packed >> 28 == item['level']
        end = numbers.data.index(0, num)
        assert numbers.data[num:end].decode('utf-8') == number
    return {'records': len(groups), 'street_names': len(streets), 'record_bytes': len(records),
            'street_index_bytes': len(index), 'street_strings_bytes': len(strings.data),
            'number_strings_bytes': len(numbers.data), 'total_bytes': len(payload),
            'zlib_whole_file_bytes_for_comparison_only': len(zlib.compress(payload, 9)),
            'file': str(output.relative_to(ROOT)), 'sha256': hashlib.sha256(payload).hexdigest()}


def read_packed(source):
    payload = source.read_bytes()
    magic, count, streets, size, index_size, names_size, numbers_size = struct.unpack_from('<8s6I', payload)
    assert magic == b'PDAADR01' and size == 16 and index_size == streets*16
    names_at, numbers_at = 32+index_size, 32+index_size+names_size
    records_at = numbers_at+numbers_size
    assert records_at+count*16 == len(payload)
    def name_at(offset):
        end = payload.index(0, names_at+offset)
        return payload[names_at+offset:end].decode('utf-8')
    street_info = [struct.unpack_from('<IIII', payload, 32+i*16) for i in range(streets)]
    return payload, count, street_info, names_at, numbers_at, records_at, name_at


def block_compression(source, output):
    """Measure an indexed, lossless layout with <=128 records per zlib block.

    This is an analysis prototype, not an ESP32 reader. Each block needs at
    most 1536 bytes of uncompressed record buffer. No whole-map inflation.
    """
    payload, count, street_info, names_at, numbers_at, records_at, _ = read_packed(source)
    strings = payload[names_at:records_at]
    street_index, blocks, index = bytearray(), bytearray(), bytearray()
    block_count = 0
    for sid, (name, normalized, first, length) in enumerate(street_info):
        first_block = block_count
        for begin in range(first, first+length, 128):
            n = min(128, first+length-begin)
            raw = bytearray()
            for row in range(begin, begin+n):
                record = payload[records_at+row*16:records_at+(row+1)*16]
                assert struct.unpack_from('<I', record)[0] == sid
                raw.extend(record[4:])  # Street is determined by the index.
            compressed = zlib.compress(raw, 9)
            assert zlib.decompress(compressed) == raw
            first_num = struct.unpack_from('<I', raw)[0]
            last_num = struct.unpack_from('<I', raw, (n-1)*12)[0]
            index.extend(struct.pack('<IIIIHH', len(blocks), len(compressed), first_num, last_num, n, len(raw)))
            blocks.extend(compressed)
            block_count += 1
        street_index.extend(struct.pack('<IIIII', name, normalized, first_block, block_count-first_block, length))
    header = struct.pack('<8s6I', b'PDAADB01', count, len(street_info), block_count,
                         numbers_at-names_at, records_at-numbers_at, 128)
    encoded = header+street_index+index+strings+blocks
    output.write_bytes(encoded)
    assert len(street_index) == len(street_info)*20 and len(index) == block_count*20
    return {'records': count, 'street_names': len(street_info), 'blocks': block_count,
            'max_uncompressed_record_buffer': 1536, 'total_bytes': len(encoded),
            'compressed_record_bytes': len(blocks), 'index_bytes': len(street_index)+len(index),
            'string_bytes': len(strings), 'file': str(output.relative_to(ROOT)),
            'sha256': hashlib.sha256(encoded).hexdigest(),
            'status': 'Prototype measured and roundtrip-checked on host; ESP32 reader not implemented.'}


def corridor(source, output, radius=1000):
    """Filter quality 1/2 addresses by straight-line distance to Rio Pinheiros."""
    data = json.loads((ROOT/'build/maps/pinheiros-osm.json').read_bytes())
    segments, grid = [], {}
    for way in data['elements']:
        tags = way.get('tags', {})
        if tags.get('waterway') not in ('river', 'canal') or 'PINHEIROS' not in canonical(tags.get('name', '')):
            continue
        geometry = way.get('geometry', [])
        for a, b in zip(geometry, geometry[1:]):
            if not a or not b:
                continue
            ax, ay = project(a['lat'], a['lon'])
            bx, by = project(b['lat'], b['lon'])
            dx, dy = bx-ax, by-ay
            index = len(segments)
            segments.append((ax, ay, dx, dy, dx*dx+dy*dy))
            for x in range(math.floor((min(ax, bx)-radius)/2000), math.floor((max(ax, bx)+radius)/2000)+1):
                for y in range(math.floor((min(ay, by)-radius)/2000), math.floor((max(ay, by)+radius)/2000)+1):
                    grid.setdefault((x, y), []).append(index)
    if not segments:
        raise ValueError('No named Pinheiros river geometry')
    del data
    payload, _, street_info, _, numbers_at, records_at, name_at = read_packed(source)
    display, groups = {}, {}
    numbers = {}
    for sid, (name_offset, norm_offset, first, length) in enumerate(street_info):
        name, norm = name_at(name_offset), name_at(norm_offset)
        for row in range(first, first+length):
            record_sid, number_offset, x, y, packed = struct.unpack_from('<IIhhI', payload, records_at+row*16)
            assert record_sid == sid
            inside = False
            for segment in grid.get((math.floor(x/2000), math.floor(y/2000)), []):
                ax, ay, dx, dy, squared = segments[segment]
                px, py = x-ax, y-ay
                t = min(1.0, max(0.0, (px*dx+py*dy)/squared)) if squared else 0.0
                if (px-t*dx)**2+(py-t*dy)**2 <= radius**2:
                    inside = True
                    break
            if not inside:
                continue
            if number_offset not in numbers:
                end = payload.index(0, numbers_at+number_offset)
                numbers[number_offset] = payload[numbers_at+number_offset:end].decode('utf-8')
            number = numbers[number_offset]
            display[norm] = name
            groups[(norm, number, packed & 0x0FFFFFFF)] = {'x': x, 'y': y, 'level': packed >> 28}
    result = pack(groups, display, output)
    result.update(distance_to_named_river_meters=radius, geometry_segments=len(segments),
                  river_geometry_length_meters=round(sum(math.sqrt(s[4]) for s in segments)),
                  geocoding_levels=[1, 2], scope='Only the existing map bbox and Sao Paulo municipality')
    return result


def repack():
    destination = ROOT/'data/maps/pinheiros-address-analysis.json'
    report = json.loads(destination.read_text(encoding='utf-8'))
    output = ROOT/'build/maps'
    for label, source in [('all_levels', 'pinheiros-addresses-cnefe'),
                          ('original_or_modified_levels_1_2', 'pinheiros-addresses-cnefe-level12')]:
        report[label]['indexed_block_compression'] = block_compression(output/(source+'.bin'), output/(source+'-blocks.bin'))
        print(label, report[label]['indexed_block_compression']['total_bytes'], flush=True)
    for radius in (1000, 2000, 3000):
        stem = 'pinheiros-addresses-cnefe-corridor%dkm' % (radius//1000)
        narrow = corridor(output/'pinheiros-addresses-cnefe-level12.bin', output/(stem+'.bin'), radius)
        narrow['indexed_block_compression'] = block_compression(output/(stem+'.bin'), output/(stem+'-blocks.bin'))
        report['corridor_%dkm_quality_1_2' % (radius//1000)] = narrow
        print(json.dumps(narrow, ensure_ascii=False, indent=2), flush=True)
    limitation = 'Indexed block files are host prototypes, not an implemented firmware feature.'
    if limitation not in report['limitations']:
        report['limitations'].append(limitation)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'build/maps/cnefe-3550308-20240910.zip')
    parser.add_argument('--repack', action='store_true', help='Measure block compression and a 1 km river corridor from existing analysis files')
    args = parser.parse_args()
    if args.repack:
        repack()
        return
    counts, levels = Counter(), Counter()
    groups, display = {}, {}
    with zipfile.ZipFile(args.source) as archive:
        members = [i for i in archive.infolist() if i.filename.endswith('.json')]
        if len(members) != 1:
            raise ValueError('Expected exactly one GeoJSON member')
        with archive.open(members[0]) as zipped, io.TextIOWrapper(zipped, encoding='utf-8-sig') as stream:
            for feature in features(stream):
                counts['municipal_records'] += 1
                if counts['municipal_records'] % 500000 == 0:
                    print('Read', counts['municipal_records'], 'records; inside', counts['inside_bbox'], flush=True)
                geometry = feature.get('geometry') or {}
                if geometry.get('type') != 'Point':
                    counts['not_point'] += 1
                    continue
                lon, lat = geometry['coordinates'][:2]
                if not (math.isfinite(lat) and math.isfinite(lon)):
                    counts['invalid_coordinate'] += 1
                    continue
                if not (BBOX[0] <= lat <= BBOX[2] and BBOX[1] <= lon <= BBOX[3]):
                    continue
                counts['inside_bbox'] += 1
                props = feature['properties']
                level = int(props.get('NV_GEO_COORD') or 0)
                levels[str(level)] += 1
                address = str(props.get('LOGRAD_NUM') or '').strip()
                street, separator, number = address.rpartition(',')
                street, number = street.strip(), re.sub(r'\s+', ' ', number.strip().upper())
                if not separator or not street:
                    counts['missing_street_or_separator'] += 1
                    continue
                numeric = re.match(r'^(\d+)', number)
                if not numeric or int(numeric[1]) == 0:
                    counts['missing_or_nonpositive_number'] += 1
                    continue
                counts['with_street_and_number'] += 1
                name = canonical(street)
                if not name:
                    raise ValueError('Empty canonical street')
                display.setdefault(name, street)
                cep_text = re.sub(r'\D', '', str(props.get('CEP') or ''))
                cep = int(cep_text) if cep_text else 0
                x, y = (round(v) for v in project(lat, lon))
                # Do not merge the same street+number in different postal areas.
                key = name, number, cep
                existing = groups.get(key)
                if existing is None:
                    groups[key] = {'x': x, 'y': y, 'level': level, 'n': 1,
                                   'minx': x, 'maxx': x, 'miny': y, 'maxy': y}
                else:
                    existing['n'] += 1
                    existing['minx'] = min(existing['minx'], x)
                    existing['maxx'] = max(existing['maxx'], x)
                    existing['miny'] = min(existing['miny'], y)
                    existing['maxy'] = max(existing['maxy'], y)
                    if 0 < level < (existing['level'] or 99):
                        existing.update(x=x, y=y, level=level)
    quality = Counter(str(item['level']) for item in groups.values())
    wide = sum((item['maxx']-item['minx'])**2+(item['maxy']-item['miny'])**2 > 100**2
               for item in groups.values())
    good = {k: v for k, v in groups.items() if v['level'] in (1, 2)}
    output = ROOT/'build/maps'
    full = pack(groups, display, output/'pinheiros-addresses-cnefe.bin')
    reliable = pack(good, display, output/'pinheiros-addresses-cnefe-level12.bin')
    firmware = (ROOT/'.pio/build/tdeck/firmware.bin').stat().st_size
    app_limit = 0x640000
    with args.source.open('rb') as source_file:
        source_hash = hashlib.file_digest(source_file, 'sha256').hexdigest()
    report = {'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'source_url': SOURCE_URL, 'source_archive_bytes': args.source.stat().st_size,
              'source_sha256': source_hash,
              'source_scope': 'Municipality of Sao Paulo (3550308), Censo 2022, GeoJSON 2024-09-10',
              'bbox_south_west_north_east': BBOX, 'counts': dict(counts),
              'geocoding_levels_in_bbox': dict(levels), 'deduplicated_levels': dict(quality),
              'deduplicated_keys': 'canonical street + complete number text + CEP; apartments/complements ignored',
              'groups_with_coordinate_bbox_diagonal_above_100m': wide,
              'all_levels': full, 'original_or_modified_levels_1_2': reliable,
              'firmware_bytes': firmware, 'app_partition_bytes': app_limit,
              'remaining_app_bytes': app_limit-firmware,
              'fits_all_levels_without_code': full['total_bytes'] <= app_limit-firmware,
              'fits_levels_1_2_without_code': reliable['total_bytes'] <= app_limit-firmware,
              'limitations': ['Only Sao Paulo municipality, not all adjacent municipalities touched by the map bbox.',
                              '2022 census addresses are not guaranteed current or complete.',
                              'Geocoding levels 3-6 are estimates or larger-area references.',
                              'Apartment coordinates may be modified; point is not a verified building entrance.',
                              'Same street/number/CEP can have multiple coordinate clusters; inspect wide groups before importing.',
                              'The zlib number is comparison only: no on-device block index or decompressor is implemented.',
                              'No firmware or device change was made by this analysis.']}
    destination = ROOT/'data/maps/pinheiros-address-analysis.json'
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
