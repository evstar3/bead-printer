#!/usr/bin/env python3

import struct
import sys

def send_color(fileobj, V, B, G, Y, O, R):
    return fileobj.write(struct.pack('<' + 'f'*6, V, B, G, Y, O, R))

if __name__ == '__main__':
    for line in sys.stdin:
        send_color(sys.stdout.buffer, *(float(tok) for tok in line.split()))
