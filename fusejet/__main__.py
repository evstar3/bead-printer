#!/usr/bin/env python3

import argparse
import threading
import sys

import fusejet.image
import fusejet.comm

MAX_HEIGHT = 32
MAX_WIDTH = 32

def main():
    # parse command line arguments
    parser = argparse.ArgumentParser(
        prog='fusejet',
        description='Fuse bead printer interface'
    )

    parser.add_argument('image_path')
    parser.add_argument(
        '-W', '--width',
        default=MAX_WIDTH,
        type=int
    )
    parser.add_argument(
        '-H', '--height',
        default=MAX_HEIGHT,
        type=int
    )

    args = parser.parse_args()

    # validate width and height
    if (args.width < 1 or args.width > MAX_WIDTH):
        print(f'fusejet: error: WIDTH must be in range 1..{MAX_WIDTH}', file=sys.stderr)
        sys.exit(1)
    if (args.height < 1 or args.height > MAX_HEIGHT):
        print(f'fusejet: error: HEIGHT must be in range 1..{MAX_HEIGHT}', file=sys.stderr)
        sys.exit(1)

    # read and transform image
    prepared_image = fusejet.image.prepare(args.image_path, args.width, args.height)

    # confirm image with user

    # begin printing: spawn threads
    # - reader
    # - writer
    # - progress reporter

    # exit
    
if __name__ == '__main__':
    main()
