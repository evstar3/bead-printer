#!/usr/bin/env python3

from PIL import Image
import itertools
from collections import defaultdict
from collections.abc import Iterable

class PrintJob():
    DEFAULT_PALETTE = [
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

    def __init__(self, fp, width: int, height: int, read_fn, write_fn, palette: list[tuple[int, int, int]] = None) -> None:
        self.read_fn = read_fn
        self.write_fn = write_fn

        if not palette:
            self.palette = PrintJob.DEFAULT_PALETTE
        else:
            self.palette = palette

        palette_image: Image = Image.new('P', (1, 1)) # dummy image to hold palette
        palette_image.putpalette(itertools.chain.from_iterable(self.palette))
        self.prepared_image: Image = Image.open(fp).resize((width, height)).quantize(len(self.palette), palette=palette_image, dither=Image.Dither.NONE)

        self.to_place = defaultdict(set)
        self.placed   = {}

        for row in range(self.prepared_image.height):
            for col in range(self.prepared_image.width):
                color = self.prepared_image.getpixel((row, col))
                self.to_place[color].add((row, col))

        self.pos = (0, 0)

    def confirm(self) -> bool:
        print('fusejet: showing prepared image')
        self.prepared_image.show()

        response = input('fusejet: print prepared image? ').lower()

        return response == 'yes' or response == 'y'

    def euclidean_distance(a: Iterable[int], b: Iterable[int]) -> int:
        assert len(a) == len(b)
        return sum((ax - bx) ** 2 for ax, bx in zip(a, b))

    def classify_color(self, color: tuple[int, int, int]) -> int | None:
        min_index, min_dist = min(((i, PrintJob.euclidean_distance(color, c)) for i, c in enumerate(self.palette)), key=lambda x: x[1])

        cutoff = 100
        if min_dist < cutoff:
            return min_index

        return None

    def is_done(self):
        return len(self.to_place) == 0

    def place_bead(self):
        color = tuple(self.read_fn(3))

        palette_index = self.classify_color(color)

        if (not palette_index) or (palette_index not in self.to_place):
            self.reject_bead()
        else: 
            placement = min(self.to_place[palette_index], key=lambda x: PrintJob.euclidean_distance(x, self.pos))
            self.drop_bead(placement)

            self.placed[placement] = palette_index
            self.to_place[palette_index].remove(placement)
            if not self.to_place[palette_index]:
                del self.to_place[palette_index]

    def reject_bead(self):
        self.write_fn(b'0')

    def drop_bead(self, placement):
        self.write_fn(b'1')
        self.write_fn(bytes(placement))
        self.pos = placement
        

        
