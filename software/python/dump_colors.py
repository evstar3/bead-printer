#!/usr/bin/env python3

import sys
from serial import Serial

from fusejet.color import sensor_data_to_sRGB
from fusejet.comms import ArduinoController

def main(port):

    ser = Serial(port=port, baudrate=9600)
    controller = ArduinoController(ser)
    while True:
        try:
            spectrum = controller.read_spectrum()
            print(sensor_data_to_sRGB(spectrum))
        except KeyboardInterrupt:
            break


if __name__ == '__main__':
    if (len(sys.argv) < 2):
        print('expecting port', file=sys.stderr)
        exit(1)

    port = sys.argv[1]
    main(port)
