
from pathlib import Path
import shutil

archivo = Path("images/happy.rgb565")

# Crear copia de seguridad
shutil.copy2(archivo, archivo.with_suffix(".rgb565.bak"))

# Leer imagen RGB565 (120 × 120, little-endian)
datos = bytearray(archivo.read_bytes())

ANCHO = 120

# Dibujar un cuadrado rojo de 12 × 12 píxeles en el centro
color_rojo = bytes([0x00, 0xF8])  # RGB565 little-endian

for y in range(54, 66):
    for x in range(54, 66):
        posicion = (y * ANCHO + x) * 2
        datos[posicion:posicion + 2] = color_rojo

archivo.write_bytes(datos)

print("Imagen modificada correctamente.")
print(f"Tamaño: {len(datos)} bytes")
