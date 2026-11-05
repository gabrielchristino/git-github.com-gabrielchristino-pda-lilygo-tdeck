"""Validate the portable Portuguese classifier against the frozen PyTorch export."""
import argparse
import json
from pathlib import Path
import subprocess
import struct
import sys
import unicodedata

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.pio-tools/ai'))
import numpy as np
from train_pt import features


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--training', type=Path, required=True)
    args = parser.parse_args()
    rows = json.loads((args.training / 'reference.json').read_text(encoding='utf-8'))
    profile = struct.unpack_from('<I', (args.training / 'intent_pt.bin').read_bytes(), 28)[0]
    additional = ['BATERIA, AGORA!', 'quanto é 123+456?', 'Qual é a carga da bateria?',
                  unicodedata.normalize('NFD', 'Qual é a carga da bateria?'), 'horário—agora',
                  'Anote não esquecer a chave', 'ação çãõ ü ñ', '☀ ☺', '% $ !', 'á' * 40]
    text = '\n'.join([row['text'] for row in rows] + additional) + '\n'
    executable = ROOT / 'build/pt_host.exe'
    subprocess.run(['g++', '-O2', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'include'),
        str(ROOT / 'tools/ai/pt_host.cpp'), str(ROOT / 'src/core/intent_pt.cpp'), '-o', str(executable)], check=True)
    run = subprocess.run([str(executable), str(args.training / 'intent_pt.bin')], input=text,
                         text=True, encoding='utf-8', capture_output=True, check=True)
    outputs = [json.loads(line) for line in run.stdout.splitlines()]
    assert len(outputs) == len(rows) + len(additional)
    feature_error = probability_error = 0
    for reference, actual in zip(rows, outputs):
        assert actual['predicted'] == reference['predicted'], reference['text']
        feature_error = max(feature_error, float(np.max(np.abs(np.array(reference['features']) - actual['features']))))
        probability_error = max(probability_error, float(np.max(np.abs(np.array(reference['probabilities']) - actual['probabilities']))))
    for sentence, actual in zip(additional, outputs[len(rows):]):
        feature_error = max(feature_error, float(np.max(np.abs(features(sentence, profile) - actual['features']))))
    assert feature_error < 1e-6 and probability_error < 1e-5
    summary = {'cases': len(rows), 'label_parity': len(rows), 'additional_feature_cases': len(additional),
               'max_feature_error': feature_error, 'max_probability_error': probability_error}
    (args.training / 'cpp-validation.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
