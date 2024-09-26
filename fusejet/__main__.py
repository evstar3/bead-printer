#!/usr/bin/env python3

import argparse
import threading
import sys

import fusejet.image
import fusejet.comms

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
    if not os.path.exists(image_path):
        print(f'fusejet: error: file not found: {image_path}')
        sys.exit(1)

    if not os.path.isfile(image_path):
        print(f'fusejet: error: image path does not specify a regular file: {image_path}')
        sys.exit(1)

    prepared_image = fusejet.image.prepare(args.image_path, args.width, args.height)

    # confirm image with user
    print('fusejet: showing prepared image')
    fusejet.image.show_image(prepared_image)

    response = input('fusejet: print prepared image? ').lower()
    if (not (response == 'yes' or response =='y')):
        sys.exit(0)

    # begin printing: spawn threads
    # - communication with controller and print logic
    # - progress reporter / IO

    # exit
    
if __name__ == '__main__':
    main()
