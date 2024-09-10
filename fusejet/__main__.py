#!/usr/bin/env python3

import argparse
import threading

MAX_HEIGHT = 32
MAX_WIDTH = 32

def main():
    # parse command line arguments
    # - image source
    # - output dimensions
    parser = argparse.ArgumentParser(
        prog='fusejet',
        description='Fuse bead printer interface'
    )

    parser.add_argument('image_path')
    parser.add_argument(
        '-W', '--width',
        default=MAX_WIDTH,
        metavar=f'1..{MAX_WIDTH}',
        type=int
    )
    parser.add_argument(
        '-H', '--height',
        default=MAX_HEIGHT,
        metavar=f'1..{MAX_HEIGHT}',
        type=int
    )

    args = parser.parse_args()

    # read and transform image

    # confirm image with user

    # begin printing: spawn threads
    # - reader
    # - writer
    # - progress reporter

    # exit
    
if __name__ == '__main__':
    main()
