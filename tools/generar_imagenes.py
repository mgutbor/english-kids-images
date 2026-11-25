
#!/usr/bin/env python3

import argparse
import base64
import json
import os
import struct
import subprocess
import sys
import urllib.error
import urllib.request

from io import BytesIO
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent.parent
COLLECTIONS_DIR = ROOT / "collections"

MODEL = "@cf/black-forest-labs/flux-1-schnell"

IMAGE_SIZE = 120
EXPECTED_SIZE = IMAGE_SIZE * IMAGE_SIZE * 2


# =====================================
# ESTILO VISUAL COMÚN
# =====================================

STYLE = """
Preschool educational flat vector illustration.
Simple geometric shapes.
Clean, bold, dark outlines.
Bright harmonious solid colors.
Friendly children's picture-book aesthetic.
Single clearly recognizable subject.
Centered composition.
Pure white background.
Minimal visual detail.

PICTURE ONLY.

COMPOSITION:
- The main subject must occupy 85% to 95% of the image area.
- Make the subject as large as possible.
- Close-up composition.
- Center the subject.
- Use almost the entire available canvas.
- Leave only a very small white margin around the subject.
- Do not make the subject look small or distant.
- Do not add unnecessary empty space.
- The subject must remain completely inside the image boundaries.
- Avoid cropping or cutting off any part of the subject.

Absolutely no typography.
No text.
No letters.
No words.
No numbers.
No labels.
No captions.
No signs.
No logos.
No watermarks.
No signatures.
No speech bubbles.
No decorative elements resembling writing.
No clothing.
No accessories containing symbols.
No title area.
No header.
No footer.
"""


# =====================================
# ILUSTRACIONES POR PALABRA
# =====================================

SUBJECTS = {

    "ugly": (
        "A funny, quirky and friendly little green monster. "
        "Unusual face, asymmetrical features, "
        "one big eye and one smaller eye. "
        "Cute and harmless appearance."
    ),

    "beautiful": (
        "A colorful decorative flower with five large petals, "
        "a bright yellow center and a green stem. "
        "Elegant and harmonious shape."
    ),

    "happy": (
        "A cute cartoon sun with a big joyful smiling face, "
        "two happy eyes and a wide smile. "
        "Simple facial features."
    ),

    "sad": (
        "A simple cartoon blue rain cloud with two large "
        "expressive eyes, a downward curved mouth "
        "and three blue raindrops. "
        "The cloud is alone on a plain white background."
    ),

    "old": (
        "A friendly elderly cartoon person with grey hair, "
        "visible wrinkles and a warm smile. "
        "Simple face and recognizable age features."
    ),

    "young": (
        "A cute cartoon teddy bear cub sitting on the floor, "
        "with round ears, big eyes and a small smiling mouth. "
        "The bear has no clothes or accessories. "
        "The bear is alone on a plain white background."
    ),
    "ball": (
        "A large colorful spherical playground toy, "
        "made of bright red, blue and yellow curved panels, "
        "with a smooth round shape. "
        "A simple children's play object."
    ),
    "orange": (
        "A large solid geometric sphere, "
        "with a vivid warm amber color, "
        "smooth surface and simple rounded shape. "
        "One single object centered in the image."
    ),
    "chair": (
        "A large simple wooden piece of furniture for sitting, "
        "with four straight legs, a flat seat and a vertical backrest. "
        "Bright cheerful solid colors, rounded edges."
    ),
}


# =====================================
# CREDENCIALES CLOUDFLARE
# =====================================

def get_credentials():

    account_id = os.getenv("CLOUDFLARE_ACCOUNT_ID")
    token = os.getenv("CLOUDFLARE_API_TOKEN")

    if not account_id or not token:
        raise RuntimeError(
            "Faltan CLOUDFLARE_ACCOUNT_ID "
            "o CLOUDFLARE_API_TOKEN"
        )

    return account_id, token


# =====================================
# GENERACIÓN DE IMAGEN
# =====================================

def generate_image(prompt):

    account_id, token = get_credentials()

    url = (
        "https://api.cloudflare.com/client/v4/accounts/"
        f"{account_id}/ai/run/{MODEL}"
    )

    payload = json.dumps({
        "prompt": prompt
    }).encode("utf-8")

    request = urllib.request.Request(
        url,
        data=payload,
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json"
        },
        method="POST"
    )

    try:

        with urllib.request.urlopen(
            request,
            timeout=180
        ) as response:

            result = json.loads(response.read())

    except urllib.error.HTTPError as error:

        details = error.read().decode(
            "utf-8",
            errors="replace"
        )

        raise RuntimeError(
            f"Error Cloudflare HTTP {error.code}: {details}"
        )

    except urllib.error.URLError as error:

        raise RuntimeError(
            f"Error de conexión con Cloudflare: {error}"
        )

    if not result.get("success"):

        raise RuntimeError(
            f"Cloudflare: {result.get('errors')}"
        )

    image_base64 = result["result"]["image"]

    return base64.b64decode(image_base64)


# =====================================
# CONVERSIÓN RGB565 LITTLE-ENDIAN
# =====================================

def process_image(image_data):

    image = Image.open(
        BytesIO(image_data)
    ).convert("RGB")

    # Recorte cuadrado centrado.
    width, height = image.size

    side = min(width, height)

    left = (width - side) // 2
    top = (height - side) // 2

    image = image.crop((
        left,
        top,
        left + side,
        top + side
    ))

    # Redimensionado.
    image = image.resize(
        (IMAGE_SIZE, IMAGE_SIZE),
        Image.Resampling.LANCZOS
    )

    # Conversión RGB565.
    pixels = []

    for r, g, b in image.get_flattened_data():

        pixel = (
            ((r >> 3) << 11) |
            ((g >> 2) << 5) |
            (b >> 3)
        )

        pixels.append(pixel)

    binary = struct.pack(
        "<" + "H" * len(pixels),
        *pixels
    )

    if len(binary) != EXPECTED_SIZE:

        raise RuntimeError(
            f"Tamaño incorrecto: {len(binary)} bytes"
        )

    return binary


# =====================================
# GUARDAR CONTENT.JSON
# =====================================

def save_content(content_file, content):

    temporary = content_file.with_suffix(".json.tmp")

    temporary.write_text(
        json.dumps(
            content,
            indent=2,
            ensure_ascii=False
        ) + "\n",
        encoding="utf-8"
    )

    temporary.replace(content_file)


# =====================================
# GENERAR COLECCIÓN
# =====================================

def generate_collection(
    collection_id,
    overwrite=False,
    word_filter=None
):

    collection_dir = COLLECTIONS_DIR / collection_id

    content_file = collection_dir / "content.json"

    images_dir = collection_dir / "images"

    if not content_file.exists():

        raise FileNotFoundError(content_file)

    content = json.loads(
        content_file.read_text(encoding="utf-8")
    )

    words = content.get("words")

    if not isinstance(words, list):

        raise ValueError(
            "La colección no contiene palabras válidas"
        )

    # Validar palabra solicitada.
    if word_filter:

        valid_ids = [
            word.get("id")
            for word in words
        ]

        if word_filter not in valid_ids:

            raise ValueError(
                f"La palabra '{word_filter}' "
                f"no existe en {collection_id}"
            )

    images_dir.mkdir(
        parents=True,
        exist_ok=True
    )

    print(f"\nColección: {collection_id}")

    print(f"Palabras: {len(words)}")

    if word_filter:
        print(f"Palabra seleccionada: {word_filter}")

    print()

    generated = 0
    skipped = 0
    failed = 0

    for word in words:

        word_id = word["id"]

        # Filtrar palabra individual.
        if word_filter and word_id != word_filter:
            continue

        spanish = word.get("spanish", "")
        english = word.get("english", "")

        subject = SUBJECTS.get(
            word_id,
            f"a simple cartoon illustration of {english}"
        )

        filename = f"{word_id}.rgb565"

        destination = images_dir / filename

        # No sobrescribir imágenes existentes.
        if destination.exists() and not overwrite:

            print(f"SKIP: {word_id} (ya existe)")

            skipped += 1

            image = word.get("image")

            if isinstance(image, dict):

                image["file"] = filename

            else:

                word["image"] = {
                    "file": filename
                }

            continue

        # Prompt sin repetir el nombre del concepto.
        prompt = (
            f"Create a single flat vector cartoon illustration: "
            f"{subject}. "
            f"{STYLE}"
        )

        print(
            f"Generando: {word_id} ({english})..."
        )

        try:

            image_data = generate_image(prompt)

            binary = process_image(image_data)

            # Guardado temporal y sustitución atómica.
            temporary = destination.with_suffix(".tmp")

            temporary.write_bytes(binary)

            temporary.replace(destination)

            word["image"] = {
                "file": filename
            }

            print(
                f"  OK: {filename} "
                f"({len(binary):,} bytes)"
            )

            generated += 1

        except Exception as error:

            print(f"  ERROR: {error}")

            failed += 1

    # Guardar content.json.
    save_content(content_file, content)

    print("\n===== RESULTADO =====")

    print(f"Generadas: {generated}")
    print(f"Existentes: {skipped}")
    print(f"Errores: {failed}")

    # Actualizar manifiestos.
    if generated or skipped:

        print("\nActualizando manifiestos...")

        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools" / "generar_manifest.py")
            ],
            check=True
        )

    print("\nProceso finalizado.")


# =====================================
# ARGUMENTOS CLI
# =====================================

def main():

    parser = argparse.ArgumentParser(
        description="Generador de imágenes English Kids"
    )

    parser.add_argument(
        "--collection",
        required=True,
        help="Identificador de la colección"
    )

    parser.add_argument(
        "--word",
        help="Generar únicamente una palabra"
    )

    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Sobrescribir imágenes existentes"
    )

    args = parser.parse_args()

    generate_collection(
        collection_id=args.collection,
        overwrite=args.overwrite,
        word_filter=args.word
    )


if __name__ == "__main__":

    main()
