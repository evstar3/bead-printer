#/usr/bin/env python3

def debug_read(n_bytes):
    return bytes(map(int, input(f'<< ').split(' ')))

def debug_write(data: bytes):
    if data == b'0':
        print('>> reject')
    elif len(data) > 1:
        print(f'>> place at ({data[0]}, {data[1]})')
