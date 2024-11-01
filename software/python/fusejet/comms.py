#!/usr/bin/env python3

# Serial.write(...) sends LSB first -> little endian

import struct

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

    def read_spectrum(self):
        float_bytes = self.serial.read(24)
        raw = struct.unpack('<ffffff', float_bytes)

        # total = 1
        total = sum(raw)

        data = {
            450: raw[0] / total,
            500: raw[1] / total,
            550: raw[2] / total,
            570: raw[3] / total,
            600: raw[4] / total,
            650: raw[5] / total,
        }

        return data

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


