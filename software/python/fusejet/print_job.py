#!/usr/bin/env python3

from PIL import Image, ImagePalette
from collections import defaultdict
import random
import itertools
import colour

from fusejet.comms import ArduinoController

class PrintJob():
    def __init__(self, image_fp, width, height, serial, classifier) -> None:
        self.controller = ArduinoController(serial)
        self.initialize_job_state(image_fp, width, height)
        self.classifier = classifier

    def initialize_job_state(self, image_fp, width, height):
        self.pos = (0, 0)
        self.placed = {}

        self.prepared_image = self.prepare_image(image_fp, width, height)

        # initialize to_place collection
        self.to_place = defaultdict(set)
        for row in range(self.prepared_image.height):
            for col in range(self.prepared_image.width):
                color = self.prepared_image.getpixel((row, col))

                # only want to place non-transparent colors
                if color != self.prepared_image.info['transparency']:
                    self.to_place[color].add((row, col))

    def prepare_image(self, image_fp, width, height):
        im = Image.open(image_fp).resize((width, height)).convert('RGBA').quantize(colors=30, dither=Image.Dither.NONE)

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

        im.putpalette(new_palette)

        return im

    def start(self):
        self.controller.start()

    def is_done(self):
        return len(self.to_place) == 0

    def euclidiean_distance(a, b):
        assert len(a) == len(b)
        return pow(sum(pow(ai - bi, 2) for ai, bi in zip(a, b)), 0.5)

    def closest_hue(self, hue):
        hue, dist = min(((hue, pow(colour.rgb_to_hsv(hue)[0] - hue, 2)) for hue in self.to_place), key=lambda x: x[1])

        cutoff = 500
        return hue if dist < cutoff else None

    def place_bead(self):
        spectrum = self.controller.read_spectrum()

        hue = self.classifier.classify(list(spectrum))

        closest_hue = self.closest_hue(hue)
        if closest_hue is None:
            self.controller.reject()
        else: 
            palette_index = self.prepared_image.palette.colors[closest_hue]

            placement = min(self.to_place[palette_index], key=lambda x: PrintJob.euclidean_distance(x, self.pos))
            self.controller.drop(placement)

            self.placed[placement] = palette_index
            self.to_place[palette_index].remove(placement)
            if not self.to_place[palette_index]:
                del self.to_place[palette_index]
