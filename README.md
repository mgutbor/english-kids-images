# English Kids

Juego educativo para ESP32-S3 con pantalla ILI9341 y dos botones.

## Hardware

- ESP32-S3-DevKitC-1 N16R8
- Pantalla ILI9341 2.8"
- Botón rojo: GPIO6
- Botón amarillo: GPIO7

## Instalación

1. Abrir el proyecto con PlatformIO.
2. Crear `secrets.h` con las credenciales WiFi.
3. Compilar y cargar el firmware.
4. Subir el sistema de archivos LittleFS si es una instalación nueva.

## Colecciones

El catálogo se obtiene de `collections.json`.

Los contenidos se descargan desde:

collections/{id}/content.json

Las imágenes se almacenan localmente en LittleFS.

## Selección

- Rojo: colección anterior.
- Amarillo: colección siguiente.
- Ambos botones: confirmar.
- Mantener ambos botones durante 2 segundos: abrir selector.

La colección seleccionada se guarda en:

/selected_collection.json

## Seguridad

LittleFS no se formatea automáticamente.