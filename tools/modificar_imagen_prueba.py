
from pathlib import Path
import random
import shutil

# ==========================================
# CONFIGURACIÓN
# ==========================================

ARCHIVO = Path("images/happy.rgb565")

ANCHO = 120
ALTO = 120
TAMANO_ESPERADO = ANCHO * ALTO * 2

# ==========================================
# FUNCIONES RGB565
# ==========================================

def color_rgb565(r, g, b):
    """Convierte RGB888 a RGB565 little-endian."""

    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F

    valor = (r5 << 11) | (g6 << 5) | b5

    return bytes([
        valor & 0xFF,
        (valor >> 8) & 0xFF
    ])


# ==========================================
# MODIFICACIÓN ALEATORIA
# ==========================================

def modificar_imagen():

    if not ARCHIVO.exists():
        raise FileNotFoundError(
            f"No existe el archivo: {ARCHIVO}"
        )

    datos = bytearray(ARCHIVO.read_bytes())

    if len(datos) != TAMANO_ESPERADO:
        raise ValueError(
            f"Tamaño incorrecto: {len(datos)} bytes"
        )

    # Copia de seguridad única del original.
    backup = ARCHIVO.with_suffix(".rgb565.original.bak")

    if not backup.exists():
        shutil.copy2(ARCHIVO, backup)
        print(f"Copia original guardada: {backup}")

    # Color aleatorio
    r = random.randint(40, 255)
    g = random.randint(40, 255)
    b = random.randint(40, 255)

    color = color_rgb565(r, g, b)

    # Tamaño aleatorio
    lado = random.randint(8, 20)

    # Posición aleatoria, evitando los bordes
    x = random.randint(0, ANCHO - lado)
    y = random.randint(0, ALTO - lado)

    # Dibujar cuadrado
    for py in range(y, y + lado):
        for px in range(x, x + lado):

            posicion = (py * ANCHO + px) * 2

            datos[posicion:posicion + 2] = color

    # Guardar imagen modificada
    ARCHIVO.write_bytes(datos)

    print("\nImagen modificada correctamente")
    print("--------------------------------")
    print(f"Archivo: {ARCHIVO}")
    print(f"Color RGB: ({r}, {g}, {b})")
    print(f"Posición: X={x}, Y={y}")
    print(f"Tamaño cuadrado: {lado}x{lado}")
    print(f"Tamaño archivo: {len(datos)} bytes")
    print("--------------------------------")


if __name__ == "__main__":
    modificar_imagen()
