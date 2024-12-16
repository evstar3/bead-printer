#!/usr/bin/env python3

import argparse
import sys
import os

from serial import Serial
from pathlib import Path

import fusejet.print_job
import fusejet.comms
from fusejet.classifier import KnnBeadClassifier, KMeansBeadClassifier

MAX_HEIGHT = 29
MAX_WIDTH  = 29

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
        '-D', '--dimension',
        type=int,
        nargs=2,
        default=(29, 29)
    )
    parser.add_argument(
        '--classifier',
        type=str,
        required = True,
        choices=['kmeans', 'knn']
    )
    parser.add_argument(
        '--data',
        type=Path,
        help='Path to text file containing labeled bead data'
    )
    parser.add_argument(
        '--save',
        type=Path,
        help='Path to binary file containing pretrained classifer'
    )

    args = parser.parse_args()

    if (args.data and args.save) or not (args.data or args.save):
        parser.print_help(file=sys.stderr)
        sys.exit(1)

    if args.classifier == 'kmeans':
        classifier_cls = KMeansBeadClassifier
    elif args.classifier == 'knn':
        classifier_cls = KnnBeadClassifier

    if args.data:
        classifier = classifier_cls.from_data(args.data)
    elif args.save:
        classifier = classifier_cls.from_save(args.save)

    # validate width and height
    args.dimension = tuple(args.dimension)
    if args.dimension and (args.dimension[0] < 1 or args.dimension[0] > MAX_WIDTH):
        print(f'fusejet: error: WIDTH must be in range 1..{MAX_WIDTH}', file=sys.stderr)
        sys.exit(1)
    if args.dimension and (args.dimension[1] < 1 or args.dimension[1] > MAX_HEIGHT):
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
        SerialClass = fusejet.comms.DebugSerial
        ser_args = {}

    with SerialClass(**ser_args) as ser:
        job = fusejet.print_job.PrintJob(args.image_path, args.dimension, ser, classifier)
        job.prepared_image.save('prepared.png')

        while input() != 'yes':
            pass

        job.start()

        while not job.is_done():
            job.place_bead()

        job.controller.stop()

if __name__ == '__main__':
    main()
