#!/usr/bin/env python3

import struct
import sys

if __name__ == '__main__':
    print('y', flush=True)

    for line in sys.stdin:
        colors = tuple(float(tok) for tok in line.split())
        if len(colors) != 6:
            continue

        sys.stdout.buffer.write(struct.pack('<' + 'f'*6, *colors))
        sys.stdout.flush()
