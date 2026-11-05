"""Verify the single shipped model, frozen data and matching runtime sources."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = ROOT / 'data/ai'
SPEC = ['tools/ai/text_features.py', 'tools/ai/policy.py',
        'src/core/intent_pt.cpp', 'include/pda/intent_pt.h',
        'src/core/intent_pt_folds.inc', 'src/core/intent_pt_short.inc']

def digest(path, source=False):
    data = Path(path).read_bytes()
    if source: data = data.replace(b'\r\n', b'\n')
    return hashlib.sha256(data).hexdigest()

def spec_hashes():
    return {name: digest(ROOT / name, True) for name in SPEC}

def check(refresh=False):
    meta = json.loads((DIRECTORY / 'intent_pt.json').read_text(encoding='utf-8'))
    binary = (DIRECTORY / 'intent_pt.bin').read_bytes()
    if len(binary) != meta['model_bytes'] or digest(DIRECTORY / 'intent_pt.bin') != meta['model_sha256']:
        raise ValueError('Weights differ from their training report')
    magic, inputs, hidden, labels, threshold, margin, profile = struct.unpack_from('<8sIIIffI', binary)
    if magic not in (b'PDAINT01', b'PDAINT02') or (inputs, hidden, labels, profile) != (1024,32,9,5):
        raise ValueError('Unsupported model dimensions or feature profile')
    if meta.get('runtime_spec_sha256') != spec_hashes():
        raise ValueError('Preprocessing/runtime changed: train a matching model or document a verified equivalent refactor')
    manifest = json.loads((DIRECTORY / 'dataset/manifest.json').read_text(encoding='utf-8'))
    assets = ['data/ai/intent_pt.bin', 'data/ai/intent_pt.json', 'data/ai/user_regression.json', 'data/ai/dataset/manifest.json']
    for name in ['train.jsonl', 'calibration.jsonl', 'evaluation.jsonl']:
        path = DIRECTORY / 'dataset' / name
        info = manifest['files'][name]
        rows = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
        if digest(path) != info['sha256'] or len(rows) != info['cases']:
            raise ValueError('Frozen dataset changed: ' + name)
        if any(row['label'] not in meta['labels'] or not 0 < len(row['text'].encode('utf-8')) <= 80 for row in rows):
            raise ValueError('Dataset label/input boundary: ' + name)
        if meta['source_dataset_files'][name]['sha256'] != info['sha256']:
            raise ValueError('Dataset differs from the model training source: ' + name)
        assets.append('data/ai/dataset/' + name)
    actual = {'model_revision': meta.get('version', meta.get('data_revision')),
              'feature_profile': profile, 'sources': spec_hashes(),
              'assets': {name: digest(ROOT / name) for name in assets}}
    path = DIRECTORY / 'integrity.json'
    if refresh:
        path.write_bytes((json.dumps(actual, indent=2) + '\n').encode('utf-8'))
    elif json.loads(path.read_text(encoding='utf-8')) != actual:
        raise ValueError('Asset manifest mismatch; review changes before --refresh')
    return actual

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh', action='store_true', help='Refresh manifest after a reviewed model/data promotion; never bypasses weight/source checks')
    args = parser.parse_args()
    check(args.refresh)
    print('Current PT model, frozen dataset and runtime specification OK')
