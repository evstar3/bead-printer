#!/usr/bin/env python3

import argparse
import sys
import os
import json

from fusejet import comms
from serial import Serial
from pathlib import Path

def main():
    # parse command line arguments
    parser = argparse.ArgumentParser(
        prog='bead_color_getter'
    )

    parser.add_argument(
        '-p', '--port',
        type=str
    )

    parser.add_argument(
        '-o', '--outfile',
        type=Path,
        default=Path('util/colors.txt')
    )

    args = parser.parse_args()

    if args.port:
        SerialClass = Serial
        ser_args = {'port': args.port, 'baudrate': 115200}
    else:
        SerialClass = comms.DebugSerial
        ser_args = {}

    args.outfile.touch()

    with args.outfile.open('r') as fp:
        lines = len(fp.readlines())

    with args.outfile.open('a') as fp:
        with SerialClass(**ser_args) as ser:
            controller = comms.ArduinoController(ser)

            try:
                while True:
                    spectrum = controller.read_spectrum()
                    print(lines, spectrum)
                    if (input() != 'n'):
                        fp.write(json.dumps(spectrum) + '\n')
                        lines += 1
                    controller.reject()
            except (KeyboardInterrupt, EOFError):
                pass

if __name__ == '__main__':
    main()
