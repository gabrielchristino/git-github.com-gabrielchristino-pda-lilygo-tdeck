"""Generate original Sokoban layouts (by reversible pulls) and unique Sudokus.

Deterministic development-time generator; no solving is needed on the ESP32.
The reverse Sokoban paths are retained only in the native test fixtures.
"""
from pathlib import Path
import random

ROOT = Path(__file__).resolve().parents[1]
DIRS = [(0, -1, 'w'), (-1, 0, 'a'), (0, 1, 's'), (1, 0, 'd')]
INVERSE = dict(zip('wasd', 'sdwa'))


def warehouse(index):
    rng = random.Random(5000 + index)
    walls = {y * 8 + x for y in range(8) for x in range(8)
             if x in (0, 7) or y in (0, 7)}
    # Interior islands make the later rooms less open.
    for pos in ([18, 45] if index % 3 == 1 else [21, 42] if index % 3 == 2 else []):
        walls.add(pos)
    floor = set(range(64)) - walls
    targets = set(rng.sample(sorted(floor - {9, 14, 49, 54}), 2 + index // 4))
    boxes = set(targets)
    player = rng.choice(sorted(floor - boxes))
    path, pulls = [], 0
    for _ in range(1200):
        dx, dy, key = rng.choice(DIRS)
        step = dx + 8 * dy
        nxt = player + step
        if nxt not in floor or nxt in boxes:
            continue
        behind = player - step
        if behind in boxes:
            boxes.remove(behind)
            boxes.add(player)
            pulls += 1
        player = nxt
        path.append(key)
    if pulls < 8 or len(boxes - targets) < 2:
        raise RuntimeError(f'unscrambled warehouse {index}')
    board = ''.join('#' if p in walls else '*' if p in boxes & targets
                    else '$' if p in boxes else '+' if p == player and p in targets
                    else '@' if p == player else '.' if p in targets else ' '
                    for p in range(64))
    solution = ''.join(INVERSE[k] for k in reversed(path))
    # Independent forward simulation proves every generated level is solvable.
    p, b = player, set(boxes)
    for key in solution:
        dx, dy, _ = next(d for d in DIRS if d[2] == key)
        n = p + dx + 8 * dy
        assert n in floor
        if n in b:
            beyond = n + dx + 8 * dy
            assert beyond in floor and beyond not in b
            b.remove(n)
            b.add(beyond)
        p = n
    assert b == targets
    return board, solution


def solution_count(board, maximum=2):
    best, options = -1, []
    for pos, value in enumerate(board):
        if value:
            continue
        row, col = divmod(pos, 9)
        used = set(board[row * 9:row * 9 + 9])
        used.update(board[col::9])
        used.update(board[r * 9 + c] for r in range(row // 3 * 3, row // 3 * 3 + 3)
                    for c in range(col // 3 * 3, col // 3 * 3 + 3))
        candidates = [n for n in range(1, 10) if n not in used]
        if not candidates:
            return 0
        if best < 0 or len(candidates) < len(options):
            best, options = pos, candidates
            if len(options) == 1:
                break
    if best < 0:
        return 1
    count = 0
    for n in options:
        board[best] = n
        count += solution_count(board, maximum - count)
        if count >= maximum:
            break
    board[best] = 0
    return count


def large_warehouse(index):
    """Append larger, connected warehouses without changing any legacy index."""
    width, height = 16, 12
    for attempt in range(100):
        rng = random.Random(20000 + index * 100 + attempt)
        walls = {y * width + x for y in range(height) for x in range(width)
                 if x in (0, width - 1) or y in (0, height - 1)}
        # Short partitions create passages while leaving space to maneuver boxes.
        for x in (5, 10):
            gap = 2 + rng.randrange(7)
            for y in range(2, 10):
                if abs(y - gap) > 1 and rng.randrange(4):
                    walls.add(y * width + x)
        floor = set(range(width * height)) - walls
        reachable, pending = set(), [next(iter(floor))]
        while pending:
            pos = pending.pop()
            if pos in reachable:
                continue
            reachable.add(pos)
            pending.extend(pos + dx + width * dy for dx, dy, _ in DIRS
                           if pos + dx + width * dy in floor - reachable)
        if reachable != floor:
            continue
        targets = set(rng.sample(sorted(p for p in floor
                                       if p % width not in (1, width - 2)
                                       and p // width not in (1, height - 2)),
                                 4 + (index % 12) // 4))
        boxes = set(targets)
        player = rng.choice(sorted(floor - boxes))
        path, pulls = [], 0
        for _ in range(3000):
            dx, dy, key = rng.choice(DIRS)
            step = dx + width * dy
            nxt = player + step
            if nxt not in floor or nxt in boxes:
                continue
            if player - step in boxes:
                boxes.remove(player - step)
                boxes.add(player)
                pulls += 1
            player = nxt
            path.append(key)
        if pulls < 20 or len(boxes - targets) < 3:
            continue
        board = ''.join('#' if p in walls else '*' if p in boxes & targets
                        else '$' if p in boxes else '+' if p == player and p in targets
                        else '@' if p == player else '.' if p in targets else ' '
                        for p in range(width * height))
        solution = ''.join(INVERSE[k] for k in reversed(path))
        pos, replay_boxes = player, set(boxes)
        for key in solution:
            dx, dy, _ = next(d for d in DIRS if d[2] == key)
            step = dx + width * dy
            nxt = pos + step
            assert nxt in floor
            if nxt in replay_boxes:
                assert nxt + step in floor - replay_boxes
                replay_boxes.remove(nxt)
                replay_boxes.add(nxt + step)
            pos = nxt
        assert replay_boxes == targets
        return board, solution
    raise RuntimeError(f'could not generate large warehouse {index}')


def sudoku(index):
    rng = random.Random(9000 + index)
    def shuffled(values):
        return rng.sample(list(values), len(values))
    rows = [b * 3 + r for b in shuffled(range(3)) for r in shuffled(range(3))]
    cols = [b * 3 + c for b in shuffled(range(3)) for c in shuffled(range(3))]
    digits = shuffled(range(1, 10))
    answer = [digits[(r * 3 + r // 3 + c) % 9] for r in rows for c in cols]
    board = answer.copy()
    remaining = 81
    for pos in shuffled(range(81)):
        old, board[pos] = board[pos], 0
        if solution_count(board.copy()) == 1:
            remaining -= 1
        else:
            board[pos] = old
        if remaining <= (38 if index < 8 else 32):
            break
    assert solution_count(board.copy()) == 1
    return ''.join(map(str, board)), ''.join(map(str, answer))


def main():
    legacy = [warehouse(i) for i in range(12)]
    # Keep phases 13-24 unchanged; replace the first twelve with new large puzzles.
    warehouses = [large_warehouse(i) for i in range(12, 24)] + [large_warehouse(i) for i in range(12)]
    sudokus = [sudoku(i) for i in range(16)]
    text = '// Generated by tools/generate_game_levels.py; original puzzle data.\n'
    text += 'static const char *const warehouseLevels[] = {\n'
    text += ''.join(f'    "{b}",\n' for b, _ in warehouses) + '};\n'
    text += 'static const char *const legacyWarehouseLevels[] = {\n'
    text += ''.join(f'    "{b}",\n' for b, _ in legacy) + '};\n'
    text += 'static const char *const sudokuPuzzles[] = {\n'
    text += ''.join(f'    "{b}",\n' for b, _ in sudokus) + '};\n'
    text += 'static const char *const sudokuAnswers[] = {\n'
    text += ''.join(f'    "{a}",\n' for _, a in sudokus) + '};\n'
    (ROOT / 'src/core/game_levels.h').write_text(text, encoding='utf-8')
    text = '// Generated test-only solutions for original warehouse levels.\n'
    text += 'static const char *const warehouseSolutions[] = {\n'
    text += ''.join(f'    "{s}",\n' for _, s in warehouses) + '};\n'
    text += 'static const char *const legacyWarehouseBoards[] = {\n'
    text += ''.join(f'    "{b}",\n' for b, _ in legacy) + '};\n'
    text += 'static const char *const legacyWarehouseSolutions[] = {\n'
    text += ''.join(f'    "{s}",\n' for _, s in legacy) + '};\n'
    (ROOT / 'tests/game_solutions.h').write_text(text, encoding='utf-8')
    print(f'{len(warehouses)} solvable Sokobans, {len(sudokus)} unique Sudokus')


if __name__ == '__main__':
    main()
