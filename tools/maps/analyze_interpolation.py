"""Measure and cross-validate sparse, approximate house-number interpolation.

Uses existing CNEFE analysis files; no network or device changes. Coordinates
are compared with held-out census points, not independently verified entrances.
"""
import argparse
from bisect import bisect_left
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import zlib

from analyze_addresses import ROOT, read_packed, Strings
from build_pinheiros import BBOX, project
from flash_strings import encode_strings, write_strings


def load():
    source = ROOT/'build/maps/pinheiros-addresses-cnefe-level12.bin'
    payload, _, streets, _, numbers_at, records_at, name_at = read_packed(source)
    names, groups, counts, numbers = [], {}, Counter(), {}
    west, north = project(BBOX[2], BBOX[1])
    east, south = project(BBOX[0], BBOX[3])
    bounds = math.ceil(west), math.ceil(north), math.floor(east), math.floor(south)
    for sid, (name, _, first, length) in enumerate(streets):
        names.append(name_at(name))
        rows = defaultdict(dict)
        for row in range(first, first+length):
            _, offset, x, y, packed = struct.unpack_from('<IIhhI', payload, records_at+row*16)
            clipped_x = max(bounds[0], min(bounds[2], x))
            clipped_y = max(bounds[1], min(bounds[3], y))
            if (clipped_x, clipped_y) != (x, y):
                counts['one_meter_boundary_quantization_clamped'] += 1
            x, y = clipped_x, clipped_y
            counts['input_addresses'] += 1
            if offset not in numbers:
                end = payload.index(0, numbers_at+offset)
                text = payload[numbers_at+offset:end].decode('utf-8')
                numbers[offset] = int(text) if re.fullmatch(r'\d+', text) else None
            number = numbers[offset]
            if number is None or not 0 < number <= 65535:
                counts['excluded_modifier_or_large_number'] += 1
                continue
            key = packed & 0x0FFFFFFF, number % 2
            previous = rows[key].get(number)
            if previous and ((previous[1]-x)**2+(previous[2]-y)**2 > 200**2):
                rows[key][number] = (number, previous[1], previous[2], True)
            elif not previous:
                rows[key][number] = (number, x, y, False)
        for key, points in rows.items():
            valid = sorted((n, x, y) for n, x, y, conflict in points.values() if not conflict)
            counts['conflicting_coordinates_excluded'] += sum(p[3] for p in points.values())
            if valid:
                groups[(sid, *key)] = valid
    counts['numeric_addresses_after_dedup'] = sum(len(points) for points in groups.values())
    counts['street_cep_parity_groups'] = len(groups)
    return names, groups, dict(counts)


def chains(points):
    current = []
    for point in points:
        if current:
            n, x, y = current[-1]
            gap = point[0]-n
            distance = math.hypot(point[1]-x, point[2]-y)
            # Refuse to bridge large unobserved ranges or spatial jumps. A
            # query outside the observed range is never extrapolated.
            if gap > 600 or distance > 600 or distance > max(250, gap*2.5+75):
                yield current
                current = []
        current.append(point)
    if current:
        yield current


def simplify(points, tolerance):
    if len(points) < 3:
        return points
    keep, stack = {0, len(points)-1}, [(0, len(points)-1)]
    while stack:
        first, last = stack.pop()
        a, b = points[first], points[last]
        worst, split = tolerance*tolerance, None
        for i in range(first+1, last):
            p = points[i]
            t = (p[0]-a[0])/(b[0]-a[0])
            error = (p[1]-a[1]-t*(b[1]-a[1]))**2+(p[2]-a[2]-t*(b[2]-a[2]))**2
            if error > worst:
                worst, split = error, i
        if split is not None:
            keep.add(split)
            stack.extend(((first, split), (split, last)))
    return [points[i] for i in sorted(keep)]


def estimate(anchors, number):
    if number < anchors[0][0] or number > anchors[-1][0] or number % 2 != anchors[0][0] % 2:
        return None
    index = bisect_left(anchors, (number, -32769, -32769))
    if index < len(anchors) and anchors[index][0] == number:
        return anchors[index][1:]
    a, b = anchors[index-1], anchors[index]
    t = (number-a[0])/(b[0]-a[0])
    return a[1]+t*(b[1]-a[1]), a[2]+t*(b[2]-a[2])


def make_model(groups, tolerance):
    model = []
    for (sid, cep, parity), points in sorted(groups.items()):
        for chain in chains(points):
            model.append((sid, cep, parity, simplify(chain, tolerance)))
    return model


def error_stats(errors):
    if not errors:
        return {'count': 0}
    errors = sorted(errors)
    def percentile(q):
        return round(errors[round((len(errors)-1)*q)], 1)
    return {'count': len(errors), 'median_m': percentile(.5), 'p90_m': percentile(.9),
            'p95_m': percentile(.95), 'p99_m': percentile(.99),
            'within_100m_percent': round(sum(e<=100 for e in errors)*100/len(errors), 2),
            'within_200m_percent': round(sum(e<=200 for e in errors)*100/len(errors), 2)}


def validate(groups, tolerance):
    train, test = {}, []
    for (sid, cep, parity), points in groups.items():
        kept = []
        for p in points:
            # Split unique address numbers before fitting or splitting chains.
            if zlib.crc32(struct.pack('<III', sid, cep, p[0])) % 5 == 0:
                test.append((sid, cep, parity, p))
            else:
                kept.append(p)
        if kept:
            train[sid, cep, parity] = kept
    model = make_model(train, tolerance)
    lookup = defaultdict(list)
    for sid, cep, parity, anchors in model:
        lookup[sid, parity].append((cep, anchors))
    errors, known_cep_errors = [], []
    counters = Counter()
    for sid, cep, parity, (number, x, y) in test:
        matches, known = [], []
        for candidate_cep, anchors in lookup[sid, parity]:
            predicted = estimate(anchors, number)
            if predicted is not None:
                matches.append(predicted)
                if candidate_cep == cep:
                    known.append(predicted)
        if len(matches) == 1:
            errors.append(math.hypot(matches[0][0]-x, matches[0][1]-y))
        elif len(matches) > 1:
            counters['ambiguous_without_cep'] += 1
        else:
            counters['outside_fitted_ranges_without_cep'] += 1
        if len(known) == 1:
            known_cep_errors.append(math.hypot(known[0][0]-x, known[0][1]-y))
    return {'tolerance_m': tolerance, 'held_out_addresses': len(test),
            'training_addresses': sum(map(len, train.values())),
            'unambiguous_without_cep': error_stats(errors),
            'unambiguous_coverage_percent': round(len(errors)*100/len(test), 2),
            'known_cep_diagnostic': error_stats(known_cep_errors), **dict(counters)}


def pack(names, model, destination):
    pool, street_index = Strings(), bytearray()
    groups, anchors = bytearray(), bytearray()
    postcodes = sorted({row[1] for row in model})
    postcode_ids = {cep: i for i, cep in enumerate(postcodes)}
    postcode_data = struct.pack('<%dI' % len(postcodes), *postcodes)
    assert len(postcodes) < 65536
    assert len(names) < 65536
    starts = defaultdict(list)
    for sid, cep, parity, points in model:
        assert len(points) <= 65535 and points[0][0] % 2 == parity
        starts[sid].append(len(groups)//8)
        groups.extend(struct.pack('<IHH', len(anchors)//6, len(points), postcode_ids[cep]))
        for number, x, y in points:
            anchors.extend(struct.pack('<Hhh', number, x, y))
    for sid, name in enumerate(names):
        indices = starts[sid]
        street_index.extend(struct.pack('<III', pool.add(name), indices[0] if indices else 0, len(indices)))
    header = struct.pack('<8s8I', b'PDAINT02', len(names), len(groups)//8, len(anchors)//6,
                         len(pool.data), len(postcodes), 8, 6, 12)
    payload = header+street_index+pool.data+postcode_data+groups+anchors
    destination.write_bytes(payload)
    group_at = 40+len(street_index)+len(pool.data)+len(postcode_data)
    anchor_at = group_at+len(groups)
    for i, (sid, cep, parity, points) in enumerate(model):
        first, length, stored_cep = struct.unpack_from('<IHH', payload, group_at+i*8)
        assert length == len(points) and postcodes[stored_cep] == cep
        for j, point in enumerate(points):
            assert struct.unpack_from('<Hhh', payload, anchor_at+(first+j)*6) == tuple(point)
    return {'total_bytes': len(payload), 'streets': len(names), 'chains': len(model),
            'anchors': len(anchors)//6, 'street_index_bytes': len(street_index),
            'street_string_bytes': len(pool.data), 'chain_bytes': len(groups),
            'postcode_bytes': len(postcode_data),
            'anchor_bytes': len(anchors), 'file': str(destination.relative_to(ROOT)),
            'sha256': hashlib.sha256(payload).hexdigest()}


def varuint(value):
    if not 0 <= value <= 131070:
        raise ValueError('Address delta exceeds compact encoding')
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return result


def read_varuint(data, pos):
    value = 0
    for shift in (0, 7, 14):
        byte = data[pos]
        pos += 1
        value |= (byte & 127) << shift
        if byte < 128:
            return value, pos
    raise ValueError('Invalid address delta')


def compact_anchors(payload, ranges_at, anchors_at, count):
    ranges, encoded = [], bytearray()
    next_anchor = 0
    for i in range(count):
        first, length, postcode = struct.unpack_from('<IHH', payload, ranges_at+i*8)
        if first != next_anchor or not 1 <= length <= 255:
            raise ValueError('Invalid compact address range')
        offset = len(encoded)
        previous = (0, 0, 0)
        expected = []
        for j in range(length):
            point = struct.unpack_from('<Hhh', payload, anchors_at+(first+j)*6)
            if not point[0] or (j and point[0] <= previous[0]):
                raise ValueError('Address numbers must increase')
            encoded.extend(varuint(point[0]-previous[0]))
            for axis in (1, 2):
                delta = point[axis]-previous[axis]
                encoded.extend(varuint(2*delta if delta >= 0 else -2*delta-1))
            expected.append(point)
            previous = point
        # Verify every number and coordinate against the unchanged source.
        pos, previous = offset, (0, 0, 0)
        for point in expected:
            number, pos = read_varuint(encoded, pos)
            x, pos = read_varuint(encoded, pos)
            y, pos = read_varuint(encoded, pos)
            decoded = (previous[0]+number, previous[1]+((x >> 1) ^ -(x & 1)),
                       previous[2]+((y >> 1) ^ -(y & 1)))
            if decoded != point:
                raise ValueError('Lossless address verification failed')
            previous = decoded
        assert pos == len(encoded)
        ranges.append((offset, length, postcode))
        next_anchor += length
    if len(encoded) >= 1 << 24 or anchors_at+next_anchor*6 != len(payload):
        raise ValueError('Address data exceeds compact layout or has trailing records')
    return ranges, encoded


def export_cpp(source, destination):
    payload = source.read_bytes()
    magic, ns, nr, na, name_bytes, nc, sr, sa, ss = struct.unpack_from('<8s8I', payload)
    assert magic == b'PDAINT02' and (sr, sa, ss) == (8, 6, 12)
    names_at = 40+ns*12
    cep_at = names_at+name_bytes
    ranges_at = cep_at+nc*4
    anchors_at = ranges_at+nr*8
    assert anchors_at+na*6 == len(payload)
    ranges, anchors = compact_anchors(payload, ranges_at, anchors_at, nr)
    names = payload[names_at:cep_at]
    name_ids, name_data, name_blocks = encode_strings(names[:-1].split(b'\0'))
    if name_bytes >= 1 << 24 or nr >= 1 << 24 or nc > 65536:
        raise ValueError('Address table exceeds compact flash offsets')
    with destination.open('w', encoding='ascii', newline='\n') as out:
        out.write('// Generated from IBGE CNEFE 2022, GeoJSON 2024-09-10. APPROXIMATE only.\n')
        out.write('// Sparse number ranges, same parity; never extrapolate. See docs/ENDERECOS-APROXIMADOS.md.\n')
        out.write('static const AddressStreet addressStreets[] = {\n')
        for i in range(ns):
            street = struct.unpack_from('<III', payload, 40+i*12)
            if street[0] >= name_bytes or street[1]+street[2] > nr or street[2] > 65535:
                raise ValueError('Invalid compact street record')
            end = names.index(0, street[0])
            out.write('{%d,%d,%d},\n' % (name_ids[names[street[0]:end]], street[1], street[2]))
        out.write('};\n')
        write_strings(out, 'addressNames', name_data, name_blocks, len(name_ids))
        out.write('static const uint32_t addressPostcodes[] = {\n')
        for i in range(nc):
            out.write('%d,\n' % struct.unpack_from('<I', payload, cep_at+i*4)[0])
        out.write('};\nstatic const AddressRange addressRanges[] = {\n')
        for record in ranges:
            out.write('{%d,%d,%d},\n' % record)
        out.write('};\nstatic const uint8_t addressAnchors[] = {\n')
        for i in range(0, len(anchors), 24):
            out.write(','.join(map(str, anchors[i:i+24]))+',\n')
        out.write('};\n')
    print('Generated', destination, 'from', source, 'lossless anchors:', na*6, '->', len(anchors), 'bytes', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tolerances', nargs='+', type=int, default=[25, 50, 100])
    parser.add_argument('--export', type=Path, help='Export an existing PDAINT02 binary as firmware C++ data')
    args = parser.parse_args()
    if args.export:
        export_cpp(args.export, ROOT/'src/core/map_addresses.inc')
        return
    names, groups, counts = load()
    print(json.dumps(counts), flush=True)
    results = []
    for tolerance in args.tolerances:
        result = validate(groups, tolerance)
        model = make_model(groups, tolerance)
        result['full_model'] = pack(names, model, ROOT/('build/maps/pinheiros-interpolation-%dm.bin' % tolerance))
        result['fits_current_app_before_new_code'] = result['full_model']['total_bytes'] < 2942784
        results.append(result)
        print(json.dumps(result, indent=2), flush=True)
    report = {'source': 'Existing IBGE CNEFE 2022 municipal GeoJSON 2024-09-10, levels 1/2 only',
              'source_analysis': 'data/maps/pinheiros-address-analysis.json', 'counts': counts,
              'tests': results,
              'limitations': ['Held-out errors are relative to census coordinates, not independently verified entrances.',
                              'Pure integer house numbers 1..65535 only; modifiers are excluded.',
                              'No extrapolation outside observed range; same parity only.',
                              'Ambiguous street/number ranges need CEP or multiple displayed results.',
                              'Current source scope is Sao Paulo municipality inside the existing map bbox.',
                              'This script changes neither firmware nor device.']}
    (ROOT/'data/maps/pinheiros-interpolation-analysis.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
