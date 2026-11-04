"""Fetch OSM line geometry and compile a bounded, image-free T-Deck map."""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import urllib.parse
import urllib.request
from flash_strings import encode_strings, write_strings

ROOT = Path(__file__).resolve().parents[2]
BBOX = (-23.755, -46.81, -23.52, -46.59)  # south, west, north, east
ORIGIN = (-23.64, -46.70)
MX = 111319.49079327358 * math.cos(math.radians(ORIGIN[0]))
MY = 111319.49079327358
KINDS = {'motorway': 3, 'trunk': 3, 'primary': 3, 'secondary': 2,
         'tertiary': 2, 'residential': 1, 'unclassified': 1, 'living_street': 1,
         'motorway_link': 2, 'trunk_link': 2, 'primary_link': 2,
         'secondary_link': 1, 'tertiary_link': 1, 'cycleway': 4}


def project(lat, lon):
    return ((lon - ORIGIN[1]) * MX, (ORIGIN[0] - lat) * MY)


def clip(a, b, bounds):
    xmin, ymin, xmax, ymax = bounds
    dx, dy = b[0]-a[0], b[1]-a[1]
    lo, hi = 0.0, 1.0
    for p, q in ((-dx, a[0]-xmin), (dx, xmax-a[0]), (-dy, a[1]-ymin), (dy, ymax-a[1])):
        if p == 0:
            if q < 0:
                return None
        else:
            t = q/p
            if p < 0:
                lo = max(lo, t)
            else:
                hi = min(hi, t)
            if lo > hi:
                return None
    return ((round(a[0]+lo*dx), round(a[1]+lo*dy)), (round(a[0]+hi*dx), round(a[1]+hi*dy)))


def simplify(points, tolerance=2.0):
    # Iterative Douglas-Peucker: bounded stack even for very long OSM ways.
    keep = {0, len(points)-1}
    stack = [(0, len(points)-1)]
    while stack:
        first, last = stack.pop()
        a, b = points[first], points[last]
        dx, dy = b[0]-a[0], b[1]-a[1]
        length = dx*dx + dy*dy
        best, split = tolerance*tolerance, None
        for i in range(first+1, last):
            px, py = points[i][0]-a[0], points[i][1]-a[1]
            t = min(1.0, max(0.0, (px*dx+py*dy)/length)) if length else 0
            distance = (px-t*dx)**2 + (py-t*dy)**2
            if distance > best:
                best, split = distance, i
        if split is not None:
            keep.add(split)
            stack.extend(((first, split), (split, last)))
    return [points[i] for i in sorted(keep)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--source', type=Path, default=ROOT/'build/maps/pinheiros-osm.json')
    parser.add_argument('--endpoint', default='https://overpass-api.de/api/interpreter')
    args = parser.parse_args()
    args.source.parent.mkdir(parents=True, exist_ok=True)
    query = '[out:json][timeout:120];(way["highway"~"^(' + '|'.join(KINDS) + ')$"]' + str(BBOX).replace(' ', '') + ';'
    query += 'way["waterway"~"^(river|canal)$"]' + str(BBOX).replace(' ', '') + ';'
    query += 'way["railway"~"^(rail|subway|light_rail)$"]' + str(BBOX).replace(' ', '') + ';);out tags geom;'
    if args.download:
        request = urllib.request.Request(args.endpoint,
            data=urllib.parse.urlencode({'data': query}).encode(),
            headers={'User-Agent': 'TDeck-PDA-vector-map/1.0 (personal offline OSM extract)'})
        with urllib.request.urlopen(request, timeout=150) as response:
            payload = response.read(80_000_001)
        if len(payload) > 80_000_000:
            raise ValueError('Extract exceeds 80 MB limit')
        data = json.loads(payload)
        if 'remark' in data:
            raise ValueError(data['remark'])
        args.source.write_bytes(payload)
    else:
        payload = args.source.read_bytes()
        data = json.loads(payload)
    if 'remark' in data:
        raise ValueError(data['remark'])
    x0, y0 = project(BBOX[2], BBOX[1])
    x1, y1 = project(BBOX[0], BBOX[3])
    bounds = (math.ceil(x0), math.ceil(y0), math.floor(x1), math.floor(y1))
    points, features, names = [], [], bytearray(b'\0')
    offsets = {'': 0}
    way_ids = set()
    for way in sorted(data.get('elements', []), key=lambda w: w['id']):
        tags, geom = way.get('tags', {}), way.get('geometry', [])
        if len(geom) < 2:
            continue
        kind = 0 if tags.get('waterway') in ('river', 'canal') else 5 if tags.get('railway') else KINDS.get(tags.get('highway'))
        if kind is None:
            continue
        name = tags.get('name', '')
        for full, short in (('Avenida ', 'Av. '), ('Rua ', 'R. '), ('Ponte ', 'Pte. '), ('Estrada ', 'Est. ')):
            if name.startswith(full):
                name = short + name[len(full):]
        name = name.encode('utf-8')[:100].decode('utf-8', errors='ignore')
        if name not in offsets:
            offsets[name] = len(names)
            names.extend(name.encode('utf-8') + b'\0')
        chunks, current = [], []
        for pa, pb in zip(geom, geom[1:]):
            if pa is None or pb is None:
                if len(current)>1: chunks.append(current)
                current=[]
                continue
            segment = clip(project(pa['lat'], pa['lon']), project(pb['lat'], pb['lon']), bounds)
            if segment is None:
                if len(current)>1: chunks.append(current)
                current=[]
                continue
            a, b = segment
            if current and current[-1] != a:
                if len(current)>1: chunks.append(current)
                current=[]
            if not current: current.append(a)
            if b != current[-1]: current.append(b)
        if len(current)>1: chunks.append(current)
        for chunk in chunks:
            chunk = simplify(chunk)
            if len(chunk)<2: continue
            if len(chunk)>255: raise ValueError('Way too long for compact feature record')
            xs, ys = zip(*chunk)
            if min(xs)<-32768 or min(ys)<-32768 or max(xs)>32767 or max(ys)>32767:
                raise ValueError('Geometry outside int16 range')
            features.append((len(points), len(chunk), kind, offsets[name], min(xs), min(ys), max(xs), max(ys)))
            points.extend(chunk)
            way_ids.add(way['id'])
    if len(features)<100 or len(points)>350000:
        raise ValueError('Unexpected extract size')
    if len(features)>65536 or len(points)>=1<<24 or len(names)>=1<<24:
        raise ValueError('Extract exceeds compact flash index limits')
    name_ids, name_data, name_blocks = encode_strings(offsets)
    offset_ids = {offset: name_ids[name.encode('utf-8')] for name, offset in offsets.items()}
    features = [(first, count, kind, offset_ids[name], xmin, ymin, xmax, ymax)
                for first, count, kind, name, xmin, ymin, xmax, ymax in features]
    cell_meters=2000
    columns=(bounds[2]-bounds[0])//cell_meters+1
    rows=(bounds[3]-bounds[1])//cell_meters+1
    cells=[[] for _ in range(columns*rows)]
    for index,feature in enumerate(features):
        _,_,_,_,xmin,ymin,xmax,ymax=feature
        for cy in range((ymin-bounds[1])//cell_meters,(ymax-bounds[1])//cell_meters+1):
            for cx in range((xmin-bounds[0])//cell_meters,(xmax-bounds[0])//cell_meters+1):
                cells[cy*columns+cx].append(index)
    output = ROOT/'src/core/map_data.inc'
    with output.open('w', encoding='utf-8', newline='\n') as out:
        out.write('// Generated by tools/maps/build_pinheiros.py; OpenStreetMap contributors, ODbL 1.0.\n')
        out.write('const Point points[] = {\n')
        for x,y in points: out.write(f'{{{x},{y}}},\n')
        out.write('};\nconst Feature features[] = {\n')
        for f in features: out.write('{'+','.join(map(str,f))+'},\n')
        out.write('};\n')
        write_strings(out, 'mapNames', name_data, name_blocks, len(name_ids))
        out.write('const size_t pointCount = sizeof(points)/sizeof(points[0]);\n')
        out.write('const size_t featureCount = sizeof(features)/sizeof(features[0]);\n')
        out.write('const Bounds coverage = {'+','.join(map(str,bounds))+'};\n')
        out.write(f'const int gridMeters={cell_meters}, gridColumns={columns}, gridRows={rows};\n')
        out.write('const SpatialCell spatialCells[] = {\n')
        offset=0
        for cell in cells:
            out.write(f'{{{offset},{len(cell)}}},\n')
            offset+=len(cell)
        out.write('};\nconst uint16_t spatialIds[] = {\n')
        for cell in cells:
            for offset in range(0,len(cell),24):
                out.write(','.join(map(str,cell[offset:offset+24]))+',\n')
        out.write('};\nconst size_t spatialIdCount=sizeof(spatialIds)/sizeof(spatialIds[0]);\n')
    manifest = {'region':'São Paulo / Marginal Pinheiros, Morumbi, Santo Amaro e Interlagos',
        'bbox_south_west_north_east': BBOX, 'origin_lat_lon': ORIGIN,
        'features': len(features), 'points': len(points), 'named_features':len(offsets)-1,
        'osm_ways':len(way_ids), 'osm_timestamp':data.get('osm3s',{}).get('timestamp_osm_base'),
        'source_sha256':hashlib.sha256(payload).hexdigest(), 'endpoint':args.endpoint,
        'query':query, 'coordinate_units':'meters, local equirectangular projection, east/south positive',
        'simplification_tolerance_m':2, 'license':'ODbL 1.0',
        'spatial_index':{'cell_meters':cell_meters,'columns':columns,'rows':rows,'references':sum(map(len,cells))},
        'attribution':'© OpenStreetMap contributors', 'license_url':'https://www.openstreetmap.org/copyright',
        'generated_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    dest=ROOT/'data/maps/pinheiros.json'
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:manifest[k] for k in ('features','points','named_features','osm_timestamp')},ensure_ascii=False))


if __name__=='__main__':
    main()
