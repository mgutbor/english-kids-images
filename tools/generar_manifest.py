
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COLLECTIONS_DIR = ROOT / "collections"

EXPECTED_IMAGE_SIZE = 120 * 120 * 2


def calculate_image_info(image_path):
    if not image_path.exists():
        return {
            "available": False,
            "size": None,
            "sha256": None
        }

    data = image_path.read_bytes()

    if len(data) != EXPECTED_IMAGE_SIZE:
        print(
            f"AVISO: {image_path.name}: "
            f"tamaño incorrecto ({len(data)} bytes)"
        )
        return {
            "available": False,
            "size": None,
            "sha256": None
        }

    return {
        "available": True,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest()
    }


def process_collection(collection_dir):
    content_file = collection_dir / "content.json"

    if not content_file.exists():
        print(f"AVISO: No existe {content_file}")
        return

    content = json.loads(content_file.read_text(encoding="utf-8"))

    if not isinstance(content.get("words"), list):
        raise ValueError(
            f"La colección {collection_dir.name} "
            "no contiene una lista de palabras válida"
        )

    print(f"\nColección: {collection_dir.name}")

    for word in content["words"]:
        image = word.get("image")

        if not image:
            print(f"  {word.get('id')}: sin imagen")
            continue

        filename = image if isinstance(image, str) else image.get("file")

        if not filename or Path(filename).name != filename:
            raise ValueError(
                f"Referencia de imagen no válida: {filename}"
            )

        image_path = collection_dir / "images" / filename
        info = calculate_image_info(image_path)

        if isinstance(image, str):
            word["image"] = {"file": filename, **info}
        else:
            image.update(info)

        status = "OK" if info["available"] else "NO DISPONIBLE"
        print(f"  {word.get('id')}: {status}")

    content_file.write_text(
        json.dumps(content, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8"
    )

    print(f"  Actualizado: {content_file}")


def main():
    if not COLLECTIONS_DIR.exists():
        raise FileNotFoundError(
            f"No existe el directorio: {COLLECTIONS_DIR}"
        )

    collections = sorted(
        path for path in COLLECTIONS_DIR.iterdir()
        if path.is_dir()
    )

    if not collections:
        raise ValueError("No hay colecciones disponibles")

    for collection_dir in collections:
        process_collection(collection_dir)

    print("\nGeneración completada.")


if __name__ == "__main__":
    main()
