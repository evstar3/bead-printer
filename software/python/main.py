#!/usr/bin/env python3

import argparse
import sys
import os

from serial import Serial
from pathlib import Path

import fusejet.print_job
import fusejet.comms
from fusejet.classifier import BeadClassifier

MAX_HEIGHT = 32
MAX_WIDTH  = 32

def main():
    # parse command line arguments
    parser = argparse.ArgumentParser(
        prog='fusejet',
        description='Fuse bead printer interface'
    )

    parser.add_argument('image_path')
    parser.add_argument(
        '-p', '--port',
        type=str
    )
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
    parser.add_argument(
        '--from-kmeans',
        type=Path
    )
    parser.add_argument(
        '--from-save',
        type=Path
    )

    args = parser.parse_args()

    # validate classifier
    if (args.from_save and args.from_kmeans) or not (args.from_save or args.from_kmeans):
        print('fusejet: exactly one of --from-kmeans or --from-save is required', file=sys.stderr)
        sys.exit(1)
    elif args.from_save:
        classifier = BeadClassifier.from_save(args.from_save)
    elif args.from_kmeans:
        classifier = BeadClassifier.from_kmeans(args.from_kmeans)

    # validate width and height
    if (args.width < 1 or args.width > MAX_WIDTH):
        print(f'fusejet: error: WIDTH must be in range 1..{MAX_WIDTH}', file=sys.stderr)
        sys.exit(1)
    if (args.height < 1 or args.height > MAX_HEIGHT):
        print(f'fusejet: error: HEIGHT must be in range 1..{MAX_HEIGHT}', file=sys.stderr)
        sys.exit(1)

    # read and transform image
    if not os.path.exists(args.image_path):
        print(f'fusejet: error: file not found: {args.image_path}')
        sys.exit(1)

    if not os.path.isfile(args.image_path):
        print(f'fusejet: error: image path does not specify a regular file: {args.image_path}')
        sys.exit(1)

    if args.port:
        SerialClass = Serial
        ser_args = {'port': args.port, 'baudrate': 115200}
    else:
        SerialClass = comms.DebugSerial
        ser_args = {}

    with SerialClass(**ser_args) as ser:
        job = print_job.PrintJob(args.image_path, args.width, args.height, ser, classifier)

        job.start()

        while not job.is_done():
            job.place_bead()

        job.arduino_controller.stop()

if __name__ == '__main__':
    main()
