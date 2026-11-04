"""Lossless flash string tables shared by the OSM and CNEFE generators."""
BLOCK_SIZE = 16


def encode_strings(values):
    names = sorted(set(v.encode('utf-8') if isinstance(v, str) else v for v in values))
    if len(names) > 65536 or any(b'\0' in name for name in names):
        raise ValueError('Invalid compact flash names')
    data, blocks, previous = bytearray(), [], b''
    for index, name in enumerate(names):
        prefix = 0
        if index % BLOCK_SIZE == 0:
            blocks.append(len(data))
        else:
            while prefix < min(len(previous), len(name), 255) and previous[prefix] == name[prefix]:
                prefix += 1
        data.append(prefix)
        data.extend(name[prefix:]+b'\0')
        previous = name
    if len(data) >= 1 << 24:
        raise ValueError('Compact names exceed flash offsets')
    # Check every name at every position within a block, including UTF-8 whose
    # common byte prefix can end inside a multibyte codepoint.
    for index, expected in enumerate(names):
        at, decoded = blocks[index//BLOCK_SIZE], b''
        for _ in range(index % BLOCK_SIZE+1):
            prefix = data[at]
            end = data.index(0, at+1)
            decoded = decoded[:prefix]+data[at+1:end]
            at = end+1
        if decoded != expected:
            raise ValueError('Lossless flash name verification failed')
    return {name: i for i, name in enumerate(names)}, bytes(data), blocks


def write_strings(out, symbol, data, blocks, count):
    out.write(f'static const uint8_t {symbol}Data[] = {{\n')
    for start in range(0, len(data), 24):
        out.write(','.join(map(str, data[start:start+24]))+',\n')
    out.write(f'}};\nstatic const FlashOffset24 {symbol}Blocks[] = {{\n')
    for start in range(0, len(blocks), 24):
        out.write(','.join(map(str, blocks[start:start+24]))+',\n')
    out.write(f'}};\nstatic const FlashStrings {symbol} = {{{symbol}Data,{symbol}Blocks,sizeof({symbol}Data),{count}}};\n')
