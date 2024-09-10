#!/usr/bin/env python3

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



