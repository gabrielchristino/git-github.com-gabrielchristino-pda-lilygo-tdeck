"""Train a small Portuguese intent classifier on original synthetic PDA phrases (CPU).

This is a text classifier, not a generative chatbot. Never reads PDA documents.
Evaluation is a separate step after model/threshold selection has finished.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.pio-tools/ai'))
import numpy as np
import torch
from torch import nn
from text_features import LABELS, DIM, normalize, features
from policy import permits
from check_assets import spec_hashes

HIDDEN = 32


def tensorize(rows, profile=5):
    return (torch.from_numpy(np.stack([features(row['text'], profile) for row in rows])),
            torch.tensor([LABELS.index(row['label']) for row in rows]))


def evaluate(probabilities, labels, threshold, margin, allowed=None):
    ordered = probabilities.argsort(axis=1)
    pred = ordered[:, -1]
    top = probabilities[np.arange(len(pred)), pred]
    gap = top - probabilities[np.arange(len(pred)), ordered[:, -2]]
    threshold = np.asarray(threshold)
    margin = np.asarray(margin)
    limits = threshold[pred] if threshold.ndim else threshold
    gaps = margin[pred] if margin.ndim else margin
    final = np.where((top >= limits) & (gap >= gaps), pred, 0)
    if allowed is not None:
        final = np.where(allowed[np.arange(len(pred)), pred], final, 0)
    known = labels != 0
    metrics = {'cases': len(labels), 'known': int(known.sum()), 'unknown': int((~known).sum()),
        'correct': int((final == labels).sum()), 'known_correct': int(((final == labels) & known).sum()),
        'known_rejected': int(((final == 0) & known).sum()),
        'wrong_accepted': int(((final != labels) & (final != 0)).sum()),
        'unknown_false_accept': int(((final != 0) & ~known).sum())}
    accepted = metrics['known_correct'] + metrics['wrong_accepted']
    metrics['accepted'] = accepted
    metrics['accepted_precision'] = metrics['known_correct'] / accepted if accepted else None
    metrics['known_coverage'] = metrics['known_correct'] / metrics['known'] if metrics['known'] else None
    return final, top, gap, metrics


def select_gate(probabilities, labels, target_precision=1.0, per_class=False, allowed=None):
    if per_class:
        raw = probabilities.argmax(1)
        thresholds, margins = [1.01], [.5]
        for label in range(1, len(LABELS)):
            subset = raw == label
            choice = select_gate(probabilities[subset], labels[subset], target_precision, allowed=allowed[subset] if allowed is not None else None) if subset.any() else (0, 1.01, .5)
            thresholds.append(choice[1]); margins.append(choice[2])
        metrics = evaluate(probabilities, labels, thresholds, margins, allowed)[3]
        return metrics['known_correct'], thresholds, margins
    choice = None
    for threshold in [.8, .85, .9, .95, .97, .99, 1.01]:
        for margin in [.1, .2, .3, .4, .5]:
            _, _, _, metrics = evaluate(probabilities, labels, threshold, margin, allowed)
            accepted = metrics['known_correct'] + metrics['wrong_accepted']
            precision = metrics['known_correct'] / accepted if accepted else 1.0
            if precision >= target_precision:
                candidate = (metrics['known_correct'], threshold, margin)
                if choice is None or candidate > choice:
                    choice = candidate
    return choice


def policy_matrix(rows, profile=5):
    if profile != 5:
        raise ValueError("Only feature profile 5 is supported")
    return np.array([[permits(normalize(row["text"]).split(), label) for label in range(len(LABELS))] for row in rows])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--epochs', type=int, default=180)
    parser.add_argument('--seeds', type=int, nargs='+', default=[7, 19, 41])
    parser.add_argument('--dataset', type=Path, default=ROOT / 'data/ai/dataset', help='Frozen directory with manifest and train/calibration/evaluation.jsonl')
    parser.add_argument('--selection', choices=['loss', 'coverage'], default='coverage',
                        help='Calibration-only checkpoint objective; coverage respects target precision')
    parser.add_argument('--feature-profile', type=int, choices=[5], default=5)
    parser.add_argument('--learning-rate', type=float, default=.005)
    parser.add_argument('--target-precision', type=float, default=.99,
                        help='Minimum empirical accepted precision on calibration (default .99)')
    parser.add_argument('--per-class-gates', action='store_true', help='Calibrate each predicted intention separately')
    args = parser.parse_args()
    if not 0 < args.target_precision <= 1:
        parser.error('--target-precision must be in (0, 1]')
    args.output.mkdir(parents=True, exist_ok=False)
    torch.set_num_threads(2)
    started = time.perf_counter()
    runtime_spec = spec_hashes()
    manifest = json.loads((args.dataset / 'manifest.json').read_text(encoding='utf-8'))
    loaded = {}
    for name in ['train', 'calibration', 'evaluation']:
        path = args.dataset / (name + '.jsonl')
        assert hashlib.sha256(path.read_bytes()).hexdigest() == manifest['files'][path.name]['sha256'], path
        loaded[name] = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
    train, validation, heldout = (loaded[name] for name in ['train', 'calibration', 'evaluation'])
    evaluation_path = args.dataset / 'evaluation.jsonl'
    assert not ({normalize(row['text']) for row in train} & {normalize(row['text']) for row in validation})
    assert not ({row['family'] for row in train} & {row['family'] for row in validation})
    test_texts = {normalize(row['text']) for row in heldout}
    assert not test_texts & {normalize(row['text']) for row in train + validation}
    assert all(row['label'] in LABELS and 0 < len(row['text'].encode('utf-8')) <= 80 for row in train + validation + heldout)
    for name, rows in [('train', train), ('calibration', validation)]:
        (args.output / (name + '.jsonl')).write_bytes(''.join(json.dumps(row, ensure_ascii=False) + '\n' for row in rows).encode('utf-8'))
    tx, ty = tensorize(train, args.feature_profile); vx, vy = tensorize(validation, args.feature_profile)
    validation_policy = policy_matrix(validation, args.feature_profile)
    counts = torch.bincount(ty, minlength=len(LABELS)).float()
    weights = len(train) / (len(LABELS) * counts)
    best, runs = None, []
    for seed in args.seeds:
        torch.manual_seed(seed)
        model = nn.Sequential(nn.Linear(DIM, HIDDEN), nn.Tanh(), nn.Dropout(.1), nn.Linear(HIDDEN, len(LABELS)))
        optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate, weight_decay=.01)
        loss_fn = nn.CrossEntropyLoss(weight=weights)
        local_best, stale = None, 0
        for epoch in range(args.epochs):
            model.train()
            order = torch.randperm(len(tx))
            for offset in range(0, len(tx), 64):
                indices = order[offset:offset + 64]
                optimizer.zero_grad(set_to_none=True)
                loss = loss_fn(model(tx[indices]), ty[indices]); loss.backward(); optimizer.step()
            model.eval()
            with torch.no_grad():
                logits = model(vx)
                validation_loss = nn.functional.cross_entropy(logits, vy).item()
            coverage = select_gate(logits.softmax(1).detach().numpy(), vy.numpy(), args.target_precision, args.per_class_gates, validation_policy)[0] if args.selection == 'coverage' else 0
            rank = (coverage, -validation_loss)
            improved = local_best is None or (validation_loss < local_best['loss'] - 1e-5
                       if args.selection == 'loss' else rank > local_best['rank'])
            if improved:
                local_best = {'loss': validation_loss, 'rank': rank, 'coverage': coverage,
                    'epoch': epoch + 1, 'state': copy.deepcopy(model.state_dict())}; stale = 0
            else: stale += 1
            if stale >= 25: break
        runs.append({'seed': seed, 'epochs_run': epoch + 1, 'selected_epoch': local_best['epoch'],
                     'validation_loss': local_best['loss'], 'calibration_accepted_correct': local_best['coverage']})
        print(runs[-1], flush=True)
        if best is None or local_best['rank'] > best['rank']:
            best = {**local_best, 'seed': seed}
    model.load_state_dict(best['state']); model.eval()
    with torch.no_grad(): probabilities = model(vx).softmax(1).numpy()
    choice = select_gate(probabilities, vy.numpy(), args.target_precision, args.per_class_gates, validation_policy)
    _, threshold, margin = choice
    _, _, _, calibration_metrics = evaluate(probabilities, vy.numpy(), threshold, margin, validation_policy)
    # Heldout weights/labels never influence checkpoint or threshold selection.
    state = model.state_dict()
    header = struct.pack('<8sIIIffI', b'PDAINT02' if args.per_class_gates else b'PDAINT01', DIM, HIDDEN, len(LABELS),
                         0 if args.per_class_gates else threshold, 0 if args.per_class_gates else margin, args.feature_profile)
    if args.per_class_gates: header += struct.pack('<18f', *(threshold + margin))
    payload = b''.join(state[name].detach().numpy().astype('<f4').tobytes() for name in ['0.weight', '0.bias', '3.weight', '3.bias'])
    binary = header + payload
    if spec_hashes() != runtime_spec:
        raise RuntimeError('Runtime specification changed during training; rerun with stable sources')
    (args.output / 'intent_pt.bin').write_bytes(binary)
    report = {'kind': 'Portuguese intent classifier, not generative language model', 'labels': LABELS,
              'training_cases': len(train), 'calibration_cases': len(validation), 'heldout_cases': len(heldout),
              'training_dataset_sha256': hashlib.sha256((args.output / 'train.jsonl').read_bytes()).hexdigest(),
              'evaluation_sha256': hashlib.sha256(evaluation_path.read_bytes()).hexdigest(),
              'calibration_dataset_sha256': hashlib.sha256((args.output / 'calibration.jsonl').read_bytes()).hexdigest(),
              'dataset': str(args.dataset),
              'checkpoint_selection': args.selection,
              'feature_profile': args.feature_profile, 'evaluation_deferred': True,
              'data_revision': manifest.get('version'), 'runtime_spec_sha256': runtime_spec,
              'learning_rate': args.learning_rate,
              'target_calibration_precision': args.target_precision,
              'per_class_gates': args.per_class_gates,
              'source_dataset_files': manifest['files'],
              'seeds': runs, 'selected_seed': best['seed'], 'selected_epoch': best['epoch'],
              'threshold': threshold, 'margin': margin, 'parameters': sum(p.numel() for p in model.parameters()),
              'model_bytes': len(binary), 'model_sha256': hashlib.sha256(binary).hexdigest(),
              'elapsed_seconds': round(time.perf_counter() - started, 3),
              'calibration': calibration_metrics,
              'deployment_approved': False, 'scope': 'Intent labels only; no argument extraction or action execution'}
    (args.output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
