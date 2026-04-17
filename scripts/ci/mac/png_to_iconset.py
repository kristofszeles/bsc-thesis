#!/usr/bin/env python3
"""Write an .iconset folder for iconutil from a PNG (Pillow, nearest-neighbor only)."""
import os
import sys

try:
    from PIL import Image
except ImportError as e:
    print("Install Pillow: pip install pillow", file=sys.stderr)
    raise SystemExit(1) from e

# (width, height, filename) — layout required by iconutil.
SPECS = [
    (16, 16, "icon_16x16.png"),
    (32, 32, "icon_16x16@2x.png"),
    (32, 32, "icon_32x32.png"),
    (64, 64, "icon_32x32@2x.png"),
    (128, 128, "icon_128x128.png"),
    (256, 256, "icon_128x128@2x.png"),
    (256, 256, "icon_256x256.png"),
    (512, 512, "icon_256x256@2x.png"),
    (512, 512, "icon_512x512.png"),
    (1024, 1024, "icon_512x512@2x.png"),
]


def main() -> None:
    if len(sys.argv) != 3:
        print("usage: png_to_iconset.py <input.png> <output.iconset_dir>", file=sys.stderr)
        raise SystemExit(2)
    src, out_dir = sys.argv[1], sys.argv[2]
    if not out_dir.endswith(".iconset"):
        print("error: output must be a path ending in .iconset", file=sys.stderr)
        raise SystemExit(2)
    os.makedirs(out_dir, exist_ok=True)
    im = Image.open(src).convert("RGBA")
    try:
        nearest = Image.Resampling.NEAREST  # type: ignore[attr-defined]
    except AttributeError:
        nearest = Image.NEAREST
    for w, h, name in SPECS:
        im.resize((w, h), nearest).save(os.path.join(out_dir, name), format="PNG")


if __name__ == "__main__":
    main()
