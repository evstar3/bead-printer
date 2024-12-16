#!/usr/bin/env python3

import argparse
import sys
import os

from fusejet import print_job
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
        default=Path('bead_color_data.txt')
    )

    args = parser.parse_args()

    if args.port:
        SerialClass = Serial
        ser_args = {'port': args.port, 'baudrate': 115200}
    else:
        SerialClass = comms.DebugSerial
        ser_args = {}

    with args.outfile.open('a') as fp:
        with SerialClass(**ser_args) as ser:
            controller = comms.ArduinoController(ser)

            try:
                while True:
                    fp.write(str(controller.read_spectrum()))
                    fp.write('\n')
                    fp.flush()
            except (KeyboardInterrupt, EOFError):
                pass

if __name__ == '__main__':
    main()
