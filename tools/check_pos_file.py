#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
检查 .pos 页面索引文件并显示目标字节附近的页面起点信息。
用法:
  python tools/check_pos_file.py <path/to/book.txt.pos> [target_byte]

如果不提供 target_byte，将显示文件摘要（前/后若干页）。
"""
import sys
import os
import struct

def load_positions(path):
    with open(path,'rb') as f:
        data = f.read()
    n = len(data)
    if n == 0:
        raise ValueError('empty pos/page file')

    # Detect device .page binary header (magic 'BPG1')
    if n >= 12 and data[0:4] == b'BPG1':
        # Format: magic(4) ver(1) reserved(3) count(uint32) offsets[count] (uint32 little-endian)
        ver = data[4]
        # count at offset 8
        count = struct.unpack_from('<I', data, 8)[0]
        # remaining bytes should be count * 4 (but we are tolerant)
        expected = 12 + count * 4
        if n < 12:
            raise ValueError('invalid .page file: too small')
        # Calculate actual count from remaining size if mismatch
        actual_remaining = n - 12
        actual_count = actual_remaining // 4
        if actual_count != count:
            # use actual_count as authoritative
            count = actual_count
        positions = []
        for i in range(count):
            off = struct.unpack_from('<I', data, 12 + i*4)[0]
            positions.append(off)
        return positions

    # Fallback: raw numeric file (.pos) containing uint32 or uint64 sequence
    if n % 8 == 0:
        cnt = n // 8
        fmt = f'<{cnt}Q'
        return list(struct.unpack(fmt, data))
    elif n % 4 == 0:
        cnt = n // 4
        fmt = f'<{cnt}I'
        return list(struct.unpack(fmt, data))
    else:
        # Also accept newline-separated decimal positions (legacy text format)
        try:
            txt = data.decode('utf-8')
            parts = [int(x.strip()) for x in txt.split() if x.strip()]
            if parts:
                return parts
        except Exception:
            pass
        raise ValueError('unknown pos/page file format')


def find_best_idx(positions, target):
    lo, hi = 0, len(positions)
    best = 0
    while lo < hi:
        mid = (lo + hi) // 2
        if positions[mid] <= target:
            best = mid
            lo = mid + 1
        else:
            hi = mid
    return best


def human(n):
    return f"{n:,}"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    path = sys.argv[1]
    if not os.path.isfile(path):
        print('file not found:', path)
        sys.exit(2)
    positions = load_positions(path)
    print('pos file:', path)
    print('total pages:', len(positions))
    if len(sys.argv) < 3:
        # summary: first 10 and last 10
        head = min(10, len(positions))
        for i in range(head):
            print(f'{i:6d}  {human(positions[i])}')
        if len(positions) > 20:
            print('   ...')
            for i in range(max(head, len(positions)-10), len(positions)):
                print(f'{i:6d}  {human(positions[i])}')
        return
    try:
        target = int(sys.argv[2])
    except Exception as e:
        print('invalid target byte:', sys.argv[2])
        sys.exit(2)
    idx = find_best_idx(positions, target)
    start = max(0, idx - 5)
    end = min(len(positions), idx + 6)
    print(f'target byte: {human(target)} -> matched page index: {idx} (1-based display: {idx+1})')
    print('-'*60)
    print(f"{'page':>6} {'index':>6} {'byte pos':>15}")
    for i in range(start, end):
        marker = '<-- target' if i == idx else ''
        print(f'{i+1:6d} {i:6d} {human(positions[i]):>15} {marker}')
    print('-'*60)
    # additional stats
    total_text_bytes = positions[-1]
    avg = total_text_bytes / len(positions) if positions else 0
    print('text bytes(last page start):', human(total_text_bytes))
    print(f'avg page size (approx): {int(avg):,} bytes')

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print('ERROR:', e)
        raise
