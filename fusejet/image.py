#!/usr/bin/env python3

from PIL import Image
import PIL
import os
import sys

def prepare(image_path: str, width: int, height: int):
    if not os.path.exists(image_path):
        print(f'fusejet: error: file not found: {image_path}')
        sys.exit(1)

    if not os.path.isfile(image_path):
        print(f'fusejet: error: image path does not specify a regular file: {image_path}')
        sys.exit(1)

    try:
        orig = Image.open(image_path)
    except PIL.UnidentifiedImageError:
        print(f'fusejet: error: could not identify image: {image_path}')
        sys.exit(1)

    resized = orig.resize((width, height))

    quantized = resized.quantize(30, dither=Image.Dither.NONE)

    return quantized

def show_image(image):
    image.show()
