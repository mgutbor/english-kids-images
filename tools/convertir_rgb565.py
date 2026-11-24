
import re
import struct
from pathlib import Path

IMAGES = [
    "ugly",
    "beautiful",
    "happy",
    "sad",
    "old",
    "young",
]

EXPECTED_PIXELS = 120 * 120

def convert_image(name):

    source = Path(f"{name}.h")
    destination = Path(f"{name}.rgb565")

    if not source.exists():
        print(f"ERROR: No existe {source}")
        return

    content = source.read_text(encoding="utf-8")

    # Eliminar comentarios C/C++.
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.S)
    content = re.sub(r"//.*", "", content)

    # Localizar el array.
    pattern = rf"\b{re.escape(name)}\s*\[\s*\d+\s*\][^=]*=\s*\{{(.*?)\}}"

    match = re.search(pattern, content, flags=re.S)

    if not match:
        print(f"ERROR: No encuentro el array {name}")
        return

    array_content = match.group(1)

    # Extraer valores hexadecimales o decimales.
    values = re.findall(
        r"0[xX][0-9a-fA-F]+|\b\d+\b",
        array_content
    )

    pixels = [int(value, 0) for value in values]

    if len(pixels) != EXPECTED_PIXELS:
        print(
            f"ERROR: {name}: {len(pixels)} píxeles "
            f"(esperados {EXPECTED_PIXELS})"
        )
        return

    if any(pixel > 65535 for pixel in pixels):
        print(f"ERROR: {name}: valor RGB565 fuera de rango")
        return

    # Guardar como uint16 little-endian.
    binary = struct.pack(
        "<" + "H" * len(pixels),
        *pixels
    )

    destination.write_bytes(binary)

    print(
        f"OK: {destination} "
        f"({len(binary):,} bytes)"
    )


for image in IMAGES:
    convert_image(image)

print("\nConversión finalizada.")
