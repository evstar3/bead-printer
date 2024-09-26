#!/usr/bin/env python3

from PIL import Image, ImagePalette
import PIL
import os
import sys
import itertools

BEAD_COLORS = [
    (0, 0, 0),
    (170, 0, 0),
    (0, 170, 0),
    (170, 85, 0),
    (0, 0, 170),
    (170, 0, 170),
    (0, 170, 170),
    (170, 170, 170),
    (255, 255, 255)
]

def prepare(image_path: str, width: int, height: int):
    try:
        orig = Image.open(image_path)
    except PIL.UnidentifiedImageError:
        print(f'fusejet: error: could not identify image: {image_path}')
        sys.exit(1)

    # resize
    resized = orig.resize((width, height))

    # quantize
    palette_image = Image.new('P', (16, 16))
    palette_image.putpalette(itertools.chain.from_iterable(BEAD_COLORS))
    quantized = resized.quantize(len(BEAD_COLORS), palette=palette_image, dither=Image.Dither.NONE)

    return quantized

def show_image(image):
    image.show()
