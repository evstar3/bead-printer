#!/usr/bin/env python3

# Serial.write(...) sends LSB first -> little endian

import struct

class ArduinoController():
    def __init__(self, fileobj):
        self.fileobj = fileobj

    def start(self):
        self.fileobj.write(struct.pack("B", 2))

    def stop(self):
        self.fileobj.write(struct.pack("B", 3))

    def goto(self, x, y):
        self.fileobj.write(struct.pack("BBB", 0, x, y))

    def drop(self, x, y):
        self.fileobj.write(struct.pack("BBB", 1, x, y))

    def read_color(self):
        float_bytes = self.fileobj.read(24)
        colors = struct.unpack('<' + 'f'*6, float_bytes)
        print(colors)
        return colors
