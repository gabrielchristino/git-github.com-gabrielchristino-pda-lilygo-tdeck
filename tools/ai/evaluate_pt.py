"""Evaluate frozen classifier weights on a JSON/JSONL set without any training."""
import argparse
import json
from pathlib import Path
import struct
from train_pt import np, torch, tensorize, evaluate, features, LABELS, policy_matrix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--cases', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    torch.set_num_threads(2)
    binary = args.model.read_bytes()
    magic, dim, hidden, count, threshold, margin, version = struct.unpack('<8sIIIffI', binary[:32])
    assert magic in (b'PDAINT01', b'PDAINT02') and (dim, hidden, count) == (1024,32,9) and version == 5
    offset = 32
    if magic == b'PDAINT02':
        limits = struct.unpack_from('<18f', binary, 32)
        threshold, margin = list(limits[:9]), list(limits[9:])
        offset = 104
    floats = np.frombuffer(binary[offset:], dtype='<f4').copy()
    assert len(floats) == dim * hidden + hidden + hidden * count + count
    w1, b1, w2, b2 = np.split(floats, [dim * hidden, dim * hidden + hidden, dim * hidden + hidden + hidden * count])
    text = args.cases.read_text(encoding='utf-8')
    rows = json.loads(text) if args.cases.suffix == '.json' else [json.loads(line) for line in text.splitlines()]
    x, y = tensorize(rows, version)
    with torch.no_grad():
        h = torch.tanh(x @ torch.from_numpy(w1.reshape(hidden, dim)).T + torch.from_numpy(b1))
        probabilities = (h @ torch.from_numpy(w2.reshape(count, hidden)).T + torch.from_numpy(b2)).softmax(1).numpy()
    pred, top, gap, metrics = evaluate(probabilities, y.numpy(), threshold, margin, policy_matrix(rows, version))
    results = [{**row, 'predicted': LABELS[int(p)], 'raw': LABELS[int(probs.argmax())],
                'score': float(s), 'margin': float(g), 'correct': int(p) == int(target)}
               for row, probs, p, s, g, target in zip(rows, probabilities, pred, top, gap, y)]
    summary = {'metrics': metrics, 'raw_correct': int((probabilities.argmax(1) == y.numpy()).sum()),
               'per_label': {label: {'cases': sum(r['label'] == label for r in results),
                    'correct': sum(r['label'] == label and r['correct'] for r in results)} for label in LABELS}}
    args.output.mkdir(parents=True, exist_ok=False)
    for name, value in [('summary.json', summary), ('evaluation.json', results), ('reference.json', [
        {'text': row['text'], 'features': features(row['text'], version).tolist(), 'probabilities': probs.tolist(),
         'predicted': LABELS[int(p)]} for row, probs, p in zip(rows, probabilities, pred)])]:
        (args.output / name).write_bytes((json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))
    (args.output / 'intent_pt.bin').write_bytes(binary)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
