#!/usr/bin/env python3

import argparse
import sys
import os
import json

from fusejet import comms
from fusejet.classifier import KMeansBeadClassifier, KnnBeadClassifier
from serial import Serial
from pathlib import Path

def main():
    # parse command line arguments
    parser = argparse.ArgumentParser(
        prog='assign_colors'
    )

    parser.add_argument(
        '-p', '--port',
        type=str
    )
    parser.add_argument(
        '-o', '--outfile',
        type=Path,
        default='classifier.out'
    )
    parser.add_argument(
        '--classifier',
        type=str,
        required = True
    )
    parser.add_argument(
        '--data',
        type=Path
    )
    parser.add_argument(
        '--save',
        type=Path
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

    if args.port:
        SerialClass = Serial
        ser_args = {'port': args.port, 'baudrate': 115200}
    else:
        SerialClass = comms.DebugSerial
        ser_args = {}

    with SerialClass(**ser_args) as ser:
        controller = comms.ArduinoController(ser)

        controller.start()

        try:
            while True:
                spectrum = controller.read_spectrum()
                color = classifier.classify(spectrum)
                print(color)
                classifier.save(args.outfile)
                controller.reject()
        except (KeyboardInterrupt, EOFError):
            controller.stop()

if __name__ == '__main__':
    main()
