#!/usr/bin/env python3

from PIL import Image, ImagePalette
from collections import defaultdict
import random
import itertools
import colorsys

from fusejet.comms import ArduinoController

class PrintJob():
    BEAD_PALETTE = [
        (255, 0, 0), # red
        (255, 106, 0), # orange
        (255, 212, 0), # yellow
        (0, 191, 15), # neon green
        (0, 126, 216), # blue razzberry
        (95, 35, 178), # magenta
        (255, 255, 255), # white
        (0, 0, 0), # black
        (212, 126, 229) # SAK pink
    ]

    def __init__(self, image_fp, dimension, serial, classifier) -> None:
        self.controller = ArduinoController(serial)
        self.initialize_job_state(image_fp, dimension)
        self.classifier = classifier

    def initialize_job_state(self, image_fp, dimension):
        self.pos = (0, 0)
        self.placed = {}

        self.prepared_image = self.prepare_image(image_fp, dimension)

        # initialize to_place collection
        self.to_place = defaultdict(set)
        for col in range(self.prepared_image.height):
            for row in range(self.prepared_image.width):
                color = self.prepared_image.getpixel((row, col))

                # only want to place non-transparent colors
                if color != self.prepared_image.info['transparency']:
                    self.to_place[color].add((row, col))

        print(self.to_place)

    def prepare_image(self, image_fp, dimension):
        im = Image.open(image_fp).convert('RGBA')

        width = min(dimension[0], im.width)
        height = min(dimension[1], im.height)
        im = im.resize((width, height))

        # create new RBG palette by truncating alpha values
        palette = ImagePalette.ImagePalette(
            mode='RGBA',
            palette=list(itertools.chain(*PrintJob.BEAD_PALETTE))
        )

        transparent_pixels = set()
        for x in range(im.width):
            for y in range(im.height):
                r, g, b, a = im.getpixel((x, y))

                # 0 is fully transparent, 255 is fully opaque
                if a < 200:
                    transparent_pixels.add((x, y))

        temp_im = Image.new(size=(0, 0), mode='P')
        temp_im.putpalette(palette)

        im = im.convert('RGB').quantize(palette=temp_im)

        while True:
            transparent_color = tuple(random.randrange(256) for _ in range(3))
            if transparent_color not in im.palette.colors:
                break

        im.info['transparency'] = im.palette.getcolor(transparent_color)

        for pos in transparent_pixels:
            im.putpixel(pos, im.info['transparency'])

        return im

    def start(self):
        self.controller.start()

    def is_done(self):
        return len(self.to_place) == 0

    def euclidean_distance(a, b):
        assert len(a) == len(b)
        return pow(sum(pow(ai - bi, 2) for ai, bi in zip(a, b)), 0.5)

    def closest_hsv(self, input_hsv):
        hsv_colors = [colorsys.rgb_to_hsv(*map(lambda x: x / 255, color)) for color in self.prepared_image.palette.colors]

        def normalize(rgb):
            return tuple(map(lambda x: x / 255, rgb))

        def hsv_distance(hsv0, hsv1):
            dh = min(abs(hsv1[0] - hsv0[0]), 1 - abs(hsv1[0] - hsv0[0]))
            ds = abs(hsv1[1] - hsv0[1])
            dv = abs(hsv1[2] - hsv0[2])

            return pow(dh * dh + ds * ds + dv * dv, 0.5)

        hsv_palette = {}
        for color, index in self.prepared_image.palette.colors.items():
            hsv_palette[color] = (colorsys.rgb_to_hsv(*normalize(color)), index)

        stats = [(kvp[1], rgb, kvp[0], hsv_distance(input_hsv, kvp[0])) for rgb, kvp in hsv_palette.items() if kvp[1] in self.to_place]

        res = min(stats, key=lambda x: x[3])

        print(res)

        cutoff = 0.25
        return res[1] if res[3] < cutoff else None

    def place_bead(self):
        spectrum = self.controller.read_spectrum()

        hsv = self.classifier.classify(spectrum)

        closest_hsv = self.closest_hsv(hsv)

        if closest_hsv is None:
            self.controller.reject()
        else: 
            palette_index = self.prepared_image.palette.colors[closest_hsv]

            placement = min(self.to_place[palette_index], key=lambda x: PrintJob.euclidean_distance(x, self.pos))
            self.controller.drop((placement[0] + 1, placement[1]))

            self.placed[placement] = palette_index
            self.to_place[palette_index].remove(placement)
            if not self.to_place[palette_index]:
                del self.to_place[palette_index]
