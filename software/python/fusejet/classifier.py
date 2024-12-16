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
            self.map = pickle.load(fp)

        self.centroids = np.array([np.array(list(centroid)) for centroid in self.map])

        return self

    def from_kmeans(path: Path, k=30):
        self = BeadClassifier()

        with path.open('r') as fp:
            spectrums = np.array([list(json.loads(line).values()) for line in fp])

        centroids, _ = kmeans2(spectrums, k, minit='++')

        self.centroids = centroids
        self.map = {i: None for i in range(len(centroids))}

        return self

    def save(self, path: Path):
        with path.open('wb') as fp:
            pickle.dump(self.map, fp)

    def classify(self, spectrum):
        dist_2 = np.sum((self.centroids - spectrum)**2, axis=1)

        index = np.argmin(dist_2)

        result = self.map[index]

        if result is None:
            hsv_str = input('No saved RGB triple. Enter best color match (HSV):')
            result = hsv_str.split()
            if len(result) != 3:
                raise RuntimeError('expecting three space-separated integers')

            self.map[index] = result

        return result
