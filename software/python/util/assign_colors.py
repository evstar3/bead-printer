#!/usr/bin/env python3

import argparse
import sys
import os
import json

from fusejet import comms
from fusejet import classifier
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
        '--from-kmeans',
        type=Path
    )
    parser.add_argument(
        '--from-save',
        type=Path
    )

    args = parser.parse_args()

    if (args.from_save and args.from_kmeans) or not (args.from_save or args.from_kmeans):
        sys.exit(1)
    elif args.from_save:
        classifier = BeadClassifier.from_save(args.from_save)
    elif args.from_kmeans:
        classifier = BeadClassifier.from_kmeans(args.from_kmeans)

    if args.port:
        SerialClass = Serial
        ser_args = {'port': args.port, 'baudrate': 115200}
    else:
        SerialClass = comms.DebugSerial
        ser_args = {}

    with SerialClass(**ser_args) as ser:
        controller = comms.ArduinoController(ser)

        try:
            while True:
                spectrum = controller.read_spectrum()
                color = classifier.classify(list(spectrum))
                print(color)
                classifier.save(args.outfile)
        except (KeyboardInterrupt, EOFError):
            pass

if __name__ == '__main__':
    main()
