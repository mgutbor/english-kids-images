
import sys
import struct
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
COLLECTION = sys.argv[1] if len(sys.argv) > 1 else "basic-adjectives"

IMAGE_DIR = ROOT / "collections" / COLLECTION / "images"
OUTPUT_DIR = ROOT / "tmp" / "preview" / COLLECTION

SIZE = 120
EXPECTED_SIZE = SIZE * SIZE * 2

OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

for source in sorted(IMAGE_DIR.glob("*.rgb565")):
    data = source.read_bytes()

    if len(data) != EXPECTED_SIZE:
        print(f"ERROR: {source.name}: tamaño incorrecto")
        continue

    pixels = struct.unpack("<" + "H" * (SIZE * SIZE), data)

    rgb = []

    for pixel in pixels:
        r = (pixel >> 11) & 0x1F
        g = (pixel >> 5) & 0x3F
        b = pixel & 0x1F

        # Expandir componentes a 8 bits.
        r = (r << 3) | (r >> 2)
        g = (g << 2) | (g >> 4)
        b = (b << 3) | (b >> 2)

        rgb.append((r, g, b))

    image = Image.new("RGB", (SIZE, SIZE))
    image.putdata(rgb)

    destination = OUTPUT_DIR / f"{source.stem}.png"
    image.save(destination)

    print(f"OK: {destination}")

print("\nConversión finalizada.")