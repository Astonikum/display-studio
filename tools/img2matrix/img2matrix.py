#!/usr/bin/env python3
"""Print a PNG as a matrix of color symbols."""

import argparse
from pathlib import Path

from PIL import Image


COLORS = {
    (255, 255, 255): "W",
    (0, 0, 0): "B",
    (255, 0, 255): "N",
    (0, 255, 0): "WF",
    (0, 0, 255): "BF",
}


def closest_color(pixel):
    return min(
        COLORS,
        key=lambda color: sum(
            (channel - reference) ** 2
            for channel, reference in zip(pixel, color)
        ),
    )


def print_matrix(image_path: Path) -> None:
    with Image.open(image_path) as image:
        image = image.convert("RGB")
        for y in range(image.height):
            row = []
            for x in range(image.width):
                color = image.getpixel((x, y))
                row.append(COLORS[closest_color(color)])
            print("{" + ", ".join(row) + "},")


def main() -> None:
    parser = argparse.ArgumentParser(description="Преобразовать PNG в матрицу цветов")
    parser.add_argument("image", type=Path, help="путь к PNG-файлу")
    args = parser.parse_args()
    print_matrix(args.image)


if __name__ == "__main__":
    main()
