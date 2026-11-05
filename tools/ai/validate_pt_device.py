"""Measure Portuguese intent labels on ESP32-S3; outputs never execute actions."""
import argparse
import json
from pathlib import Path
import statistics
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from device_connection import Connection, fields


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--training', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--model-id', default='pt-current', help='Require the diagnostic firmware to identify this model')
    args = parser.parse_args()
    reference = json.loads((args.training / 'evaluation.json').read_text(encoding='utf-8'))
    args.output.mkdir(parents=True, exist_ok=False)
    log = (args.output / 'serial.txt').open('w', encoding='utf-8')
    conn = Connection(args.port, log)

    def memory():
        response, _ = conn.command('PDA MEM', 'MEM DONE')
        return {name: fields(next(line for line in response.splitlines() if line.startswith('MEM ' + name + ' ')))
                for name in ['INTERNAL', 'PSRAM', 'PROBE']}

    def games():
        text, _ = conn.command('PDA GAMES', 'GAME I=9')
        rows = [line for line in text.splitlines() if line.startswith('GAME I=')]
        assert len(rows) == 10
        return rows

    results = []
    try:
        conn.command('PDA HOME', 'PDA HOME OK')
        before_games = games()
        conn.command('PDA PT RUN ', 'PT INPUT ERROR')
        conn.command('PDA PT RUN ' + 'x' * 81, 'PT INPUT ERROR')
        before = memory()
        conn.command('PDA MEM RESET', 'MEM RESET OK')
        for row in reference:
            response, ack = conn.command('PDA PT RUN ' + row['text'], 'PT RESULT')
            actual = fields(next(line for line in response.splitlines() if line.startswith('PT RESULT ')))
            assert actual['EXECUTED'] == 0 and actual['LABEL'] == row['predicted'], (row, actual)
            if args.model_id:
                assert actual.get('MODEL') == args.model_id, actual
            assert abs(float(actual['SCORE']) - row['score']) < 1e-5
            assert abs(float(actual['MARGIN']) - row['margin']) < 1e-5
            results.append({'text': row['text'], 'expected': row['label'], **actual, 'ack_ms': ack})
        conn.command('PDA PT RUN ' + '+' * 80, 'PT RESULT')
        conn.command('PDA MEM CHECK', 'MEM CHECK OK=1')
        after = memory()
        assert after['PSRAM']['FREE'] == before['PSRAM']['FREE']
        assert games() == before_games
        summary = {'label_parity': len(results), 'minimum_us': min(r['US'] for r in results),
                   'median_us': statistics.median(r['US'] for r in results),
                   'maximum_us': max(r['US'] for r in results), 'memory_before': before,
                   'memory_after': after, 'games_identical': True, 'actions_executed': 0,
                   'model_id': args.model_id}
        (args.output / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(summary, indent=2), flush=True)
    finally:
        try: conn.command('PDA HOME', 'PDA HOME OK')
        finally: conn.port.close(); log.close()


if __name__ == '__main__':
    main()
