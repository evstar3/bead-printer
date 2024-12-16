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
        prog='dump_colors'
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

    if not args.outfile.exists():
        args.outfile.touch()

    with args.outfile.open('r') as fp:
        lines = len(fp.readlines())

    with args.outfile.open('a') as fp:
        with SerialClass(**ser_args) as ser:
            controller = comms.ArduinoController(ser)

            try:
                while True:
                    spectrum = controller.read_spectrum()
                    inp = input(f'{lines}: {spectrum}\nlabel: ').strip()
                    try:
                        label = int(inp)
                        fp.write(json.dumps({'label': label, 'spectrum': spectrum}) + '\n')
                        lines += 1
                    except:
                        pass
                    controller.reject()
            except (KeyboardInterrupt, EOFError):
                pass

if __name__ == '__main__':
    main()
