
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IMAGES_DIR = ROOT / "images"
OUTPUT = ROOT / "version.json"

VERSION = "1.0.2"

EXPECTED_IMAGES = [
    "ugly.rgb565",
    "beautiful.rgb565",
    "happy.rgb565",
    "sad.rgb565",
    "old.rgb565",
    "young.rgb565",
]

images = []

for filename in EXPECTED_IMAGES:
    path = IMAGES_DIR / filename

    if not path.exists():
        raise FileNotFoundError(f"No existe: {path}")

    data = path.read_bytes()

    if len(data) != 120 * 120 * 2:
        raise ValueError(
            f"{filename}: tamaño incorrecto ({len(data)} bytes)"
        )

    images.append({
        "file": filename,
        "path": f"images/{filename}",
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest()
    })

manifest = {
    "version": VERSION,
    "images": images
}

OUTPUT.write_text(
    json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
    encoding="utf-8"
)

print(f"Manifiesto generado: {OUTPUT}")
print(f"Versión: {VERSION}")
print(f"Imágenes: {len(images)}")
