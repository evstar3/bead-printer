#!/usr/bin/env python3

# Serial.write(...) sends LSB first -> little endian

import struct
from fusejet.color import sensor_data_to_sRGB

class ArduinoController():
    def __init__(self, serial):
        self.serial = serial

    def start(self):
        self.serial.write(struct.pack("B", 0))

    def stop(self):
        self.serial.write(struct.pack("B", 1))

    def drop(self, pos):
        self.serial.write(struct.pack("BBB", 2, *pos))

    def reject(self):
        self.serial.write(struct.pack("B", 3))

    def read_color(self):
        float_bytes = self.serial.read(24)
        raw = struct.unpack('<ffffff', float_bytes)

        data = {
            650: raw[0],
            600: raw[1],
            570: raw[2],
            550: raw[3],
            500: raw[4],
            450: raw[5],
        }

        return sensor_data_to_sRGB(data)

class DebugSerial():
    def read(self, length):
        return struct.pack('<ffffff', *map(float, input('spectral distribution (violet to red): ').split()))

    def write(self, data):
        if data[0] == 0:
            print('START')
        elif data[0] == 1:
            print('STOP')
        elif data[0] == 2:
            print(f'DROP {data[1]} {data[2]}')
        elif data[0] == 3:
            print('REJECT')


