#!/usr/bin/env python3

from PIL import Image, ImagePalette
from collections import defaultdict
import random
import itertools

from fusejet.comms import ArduinoController

class PrintJob():
    COLOR_LIST = [
    #    V  B  G  Y  O  R
        (0, 0, 0, 0, 0, 0),
    ]

    def __init__(self, image_fp, width, height, serial) -> None:
        self.arduino_controller = ArduinoController(serial)

        im = Image.open(image_fp)
        im = im.resize((width, height))
        im = im.convert('RGBA')
        im = im.quantize(
            colors=30,
            dither=Image.Dither.NONE
        )

        # find an unused color in the palette to use as the transparent color
        # probability of collision = (1 / 256) ** 2
        #                          = 0.000015
        while True:
            transparent_color = tuple(random.randrange(256) for _ in range(3)) + (0,)
            if transparent_color not in im.palette.colors:
                break

        # allocate new color in palette
        im.palette.getcolor(transparent_color)

        # create new RBG palette by truncating alpha values
        new_palette = ImagePalette.ImagePalette(
            mode='RGB',
            palette=list(itertools.chain.from_iterable(key[0:3] for key, value in im.palette.colors.items()))
        )

        # set image's transparent color
        im.info['transparency'] = new_palette.colors[transparent_color[0:3]]

        # map RGBA values to RGB values based on alpha channel cutoff
        # fully transparent at 0, fully opaque at 255
        cutoff = 100
        color_by_index = {val: key for key, val in im.palette.colors.items()}
        new_data = [val if color_by_index[val][3] > cutoff else im.info['transparency'] for val in im.getdata()]
        im.putdata(new_data)

        self.prepared_image = im

        self.to_place = defaultdict(set)
        self.placed   = {}

        for row in range(self.prepared_image.height):
            for col in range(self.prepared_image.width):
                color = im.getpixel((row, col))

                # only want to place non-transparent colors
                if color != im.info['transparency']:
                    self.to_place[color].add((row, col))

        self.pos = (0, 0)

    def confirm(self) -> bool:
        print('fusejet: showing prepared image')
        # self.prepared_image.show()

        response = input('fusejet: print prepared image? ').lower()

        return response == 'yes' or response == 'y'

    def euclidean_distance(a, b) -> int:
        assert len(a) == len(b)
        return sum((ax - bx) ** 2 for ax, bx in zip(a, b))

    def classify_color(self, color: tuple[int, int, int]) -> int | None:
        min_index, min_dist = min(((i, PrintJob.euclidean_distance(color, c)) for i, c in enumerate(self.palette)), key=lambda x: x[1])

        print(min_dist)

        cutoff = 100
        if min_dist < cutoff:
            return min_index

        return None

    def is_done(self):
        return len(self.to_place) == 0

    def place_bead(self):
        color = self.arduino_controller.read_color()

        palette_index = self.classify_color(color)

        if (palette_index is None) or (palette_index not in self.to_place):
            self.arduino_controller.reject()
        else: 
            placement = min(self.to_place[palette_index], key=lambda x: PrintJob.euclidean_distance(x, self.pos))
            self.arduino_controller.drop(placement)

            self.placed[placement] = palette_index
            self.to_place[palette_index].remove(placement)
            if not self.to_place[palette_index]:
                del self.to_place[palette_index]
