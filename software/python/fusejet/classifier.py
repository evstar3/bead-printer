#!/usr/bin/env python3

import json
import pickle
import numpy as np
from scipy.cluster.vq import whiten, kmeans, vq, kmeans2
from pathlib import Path

class BeadClassifier():
    def from_save(path: Path):
        self = BeadClassifier()

        with path.open('rb') as fp:
            centroids, hue_map = pickle.load(fp)

        self.centroids = centroids
        self.map = hue_map

        return self

    def from_kmeans(path: Path, k=9):
        self = BeadClassifier()

        with path.open('r') as fp:
            spectrums = np.array([json.loads(line)['spectrum'] for line in fp])

        centroids, _ = kmeans2(spectrums, k, minit='++')

        self.centroids = centroids
        self.map = {i: None for i in range(len(centroids))}

        return self

    def save(self, path: Path):
        with path.open('wb') as fp:
            pickle.dump((self.centroids, self.map), fp)

    def classify(self, spectrum):
        dist_2 = np.sum((self.centroids - spectrum)**2, axis=1)

        index = np.argmin(dist_2)
        result = self.map[index]

        if result is None:
            hue_str = input('No saved RGB triple. Enter best hue match (from HSV): ')
            result = tuple(map(int, hue_str.split()))
            if len(result) != 3:
                raise RuntimeError
            self.map[index] = result

        return result, dist_2[index]
