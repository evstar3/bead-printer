#!/usr/bin/env python3

import json
import pickle
import numpy as np
from scipy.cluster.vq import whiten, kmeans, vq, kmeans2
from sklearn.neighbors import KNeighborsClassifier
from pathlib import Path

class KMeansBeadClassifier():
    def from_save(path: Path):
        self = KMeansBeadClassifier()

        with path.open('rb') as fp:
            centroids, hue_map = pickle.load(fp)

        self.centroids = centroids
        self.map = hue_map

        return self

    def from_data(path: Path, k=9):
        self = KMeansBeadClassifier()

        with path.open('r') as fp:
            spectrums = np.array([json.loads(line)['spectrum'] for line in fp])

        centroids, _ = kmeans2(spectrums, k, minit='++')

        self.centroids = centroids
        self.map = {i: None for i in range(len(self.centroids))}

        return self

    def save(self, path: Path):
        with path.open('wb') as fp:
            pickle.dump((self.centroids, self.map), fp)

    def classify(self, spectrum):
        dist_2 = np.sum((self.centroids - spectrum)**2, axis=1)

        index = np.argmin(dist_2)
        result = self.map[index]

        if result is None:
            rgb_str = input('No saved RGB triple. Enter best match: ')
            result = tuple(map(int, rgb_str.split()))
            assert len(result) == 3
            self.map[index] = result

        return result

class KnnBeadClassifier():
    def from_save(path: Path):
        self = KnnBeadClassifier()

        with path.open('rb') as fp:
            neigh, rgb_map = pickle.load(fp)

        self.neigh = neigh
        self.map = rgb_map

        return self

    def from_data(path: Path, k=9):
        self = KnnBeadClassifier()

        spectrums = []
        labels = []
        with path.open('r') as fp:
            for line in fp:
                obj = json.loads(line)
                spectrums.append(obj['spectrum'])
                labels.append(obj['label'])

        self.neigh = KNeighborsClassifier(n_neighbors=3)
        self.neigh.fit(spectrums, labels)

        self.map = {i: None for i in range(k)}

        return self

    def save(self, path: Path):
        with path.open('wb') as fp:
            pickle.dump((self.neigh, self.map), fp)

    def classify(self, spectrum):
        index = self.neigh.predict([spectrum])[0]
        result = self.map[int(index)]

        print(result)

        if result is None:
            rgb_str = input('No saved RGB triple. Enter best match: ')
            result = tuple(map(int, rgb_str.split()))
            assert len(result) == 3
            self.map[index] = result

        return result
