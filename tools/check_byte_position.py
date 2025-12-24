#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
在 TXT 文件中显示指定字节位置附近的文本和基本诊断信息。

用法:
  python tools/check_byte_position.py <path/to/book.txt> <byte_pos> [--ctx N]

输出: 显示目标字节前后 ctx 字节（默认300），并打印该位置前后的 UTF-8 解码预览。
"""
import sys
import os
import argparse

def preview(path, pos, ctx=300):
    size = os.path.getsize(path)
    start = max(0, pos - ctx)
    read_len = min(size - start, ctx*2 + 1)
    with open(path,'rb') as f:
        f.seek(start)
        b = f.read(read_len)
    s = b.decode('utf-8','replace')
    # Build cumulative byte offsets for each character in the decoded preview
    cum_bytes = []  # byte offset of the start of each char, relative to `start`
    acc = 0
    for ch in s:
        cum_bytes.append(acc)
        acc += len(ch.encode('utf-8'))
    rel = pos - start
    # find the character index that contains the requested byte position
    char_index = None
    for i,off in enumerate(cum_bytes):
        # if the next char start is greater than rel, current char contains rel
        next_off = cum_bytes[i+1] if i+1 < len(cum_bytes) else acc
        if off <= rel < next_off:
            char_index = i
            break
    if char_index is None:
        # position is at or beyond preview end
        if len(cum_bytes) == 0:
            char_index = 0
        else:
            char_index = len(cum_bytes)-1
    # print header
    print('file:', path)
    print('file size:', f'{size:,}', 'bytes')
    print('requested pos:', pos, 'relative start:', start, 'read_len:', read_len)
    print('-'*80)

    # Show text window centered on the target character
    win_before = 120
    win_after = 120
    st = max(0, char_index - win_before)
    ed = min(len(s), char_index + win_after)
    window = s[st:ed]
    # Print the windowed text and a caret line indicating the target character
    print('Text window (context):')
    print('-----')
    print(window)
    print('-----')
    # Build caret line in monospaced characters by counting characters (not bytes)
    caret_pos_chars = char_index - st
    caret_line = ' ' * caret_pos_chars + '^'
    print(caret_line)

    # Show byte-level hex around the requested byte position for exact matching
    hex_start = max(0, rel - 48)
    hex_len = min(128, read_len - hex_start)
    raw_all = b
    hex_bytes = raw_all[hex_start: hex_start + hex_len]
    hex_line = ' '.join(f'{x:02X}' for x in hex_bytes)
    ascii_line = ''.join((chr(x) if 32 <= x < 127 else '.') for x in hex_bytes)
    print('\nhex around position (bytes):')
    print(f'byte range: {start+hex_start} .. {start+hex_start+len(hex_bytes)-1}')
    print(hex_line)
    print(ascii_line)

    # diagnostic: is pos at UTF-8 codepoint boundary?
    full_bytes = open(path,'rb').read()
    if pos < len(full_bytes):
        b0 = full_bytes[pos]
        is_continuation = (b0 & 0xC0) == 0x80
        print(f"byte at pos: 0x{b0:02X} ({'continuation' if is_continuation else 'start'})")
    else:
        print('requested pos is at or beyond EOF')

    print(f'Character index in preview: {char_index}, byte offset within preview (approx): {cum_bytes[char_index] if len(cum_bytes)>0 else 0}')
    print(f'Byte difference from start: {rel} (pos-start)')

def main():
    p = argparse.ArgumentParser()
    p.add_argument('txt', help='path to txt file')
    p.add_argument('pos', type=int, help='byte position to inspect')
    p.add_argument('--ctx', type=int, default=300, help='context bytes before/after')
    args = p.parse_args()
    if not os.path.isfile(args.txt):
        print('txt not found:', args.txt); sys.exit(2)
    preview(args.txt, args.pos, args.ctx)

if __name__ == '__main__':
    main()
