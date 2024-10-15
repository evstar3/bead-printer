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

    def read_color(self):
        float_bytes = self.serial.read(24)
        colors = struct.unpack('<' + 'f'*6, float_bytes)
        if __debug__:
            print(colors)
        return colors
