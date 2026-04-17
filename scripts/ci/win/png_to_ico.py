#!/usr/bin/env python3
"""Write a multi-resolution Windows .ico from a PNG (requires Pillow)."""
import sys

try:
    from PIL import Image
except ImportError as e:
    print("Install Pillow: pip install pillow", file=sys.stderr)
    raise SystemExit(1) from e


def main() -> None:
    if len(sys.argv) != 3:
        print("usage: png_to_ico.py <input.png> <output.ico>", file=sys.stderr)
        raise SystemExit(2)
    src, dst = sys.argv[1], sys.argv[2]
    im = Image.open(src).convert("RGBA")
    try:
        nearest = Image.Resampling.NEAREST  # type: ignore[attr-defined]
    except AttributeError:
        nearest = Image.NEAREST
    # Nearest-neighbor: pass one Image per size; Pillow only uses LANCZOS when it must scale itself.
    sizes = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    frames = [im.resize(s, nearest) for s in sizes]
    # IcoImagePlugin uses the *first* image's size as the max; largest frame must be first.
    pairs = list(zip(sizes, frames))
    pairs.sort(key=lambda p: p[0][0], reverse=True)
    sizes_o = [p[0] for p in pairs]
    frames_o = [p[1] for p in pairs]
    frames_o[0].save(
        dst,
        format="ICO",
        sizes=sizes_o,
        append_images=frames_o[1:],
    )


if __name__ == "__main__":
    main()
