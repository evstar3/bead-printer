#! /usr/bin/env python3

import json

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.decomposition import PCA

# Load the data from colors.txt
with open("colors.txt", "r") as file:
    data = [json.loads(line) for line in file]

# Extract spectra and labels
spectra = np.array([entry["spectrum"] for entry in data])
labels = np.array([entry["label"] for entry in data])

# Define colors for labels
color_map = [
    "red",
    "orange",
    "yellow",
    "green",
    "blue",
    "magenta",
    "lightgrey",
    "black",
    "pink",
]

# Perform PCA
pca = PCA(n_components=2)
principal_components = pca.fit_transform(spectra)

# Convert to DataFrame for easy plotting
df_pca = pd.DataFrame(
    {
        "PC1": principal_components[:, 0],
        "PC2": principal_components[:, 1],
        "Label": labels,
    }
)


# Print label and PCA values for each point
# for i, row in df_pca.iterrows():
#     print(
#         f"[{i+1}] Label: {row['Label']}, PC1: {row['PC1']:.2f}, PC2: {row['PC2']:.2f}"
#     )

# Plot PCA results
plt.figure(figsize=(8, 6))
for label in np.unique(labels):
    subset = df_pca[df_pca["Label"] == label]
    plt.scatter(
        subset["PC1"],
        subset["PC2"],
        label=f"Label {label}",
        color=color_map[label % len(color_map)],
    )

plt.xlabel("Principal Component 1")
plt.ylabel("Principal Component 2")
plt.title("PCA of Spectra")
# plt.legend()
plt.grid()
plt.show()
