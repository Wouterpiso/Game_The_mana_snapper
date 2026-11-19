# The ManaSnapper

The ManaSnapper is a portable Magic: The Gathering card scanner and collection
manager. Three ESP32 boards work together: a camera board frames a card, a
master board looks it up on Scryfall and keeps the collection, and a touch-screen
board shows the result and drives the menu. A small Flask service on a PC adds
OCR and JSON import/export.

The project was built as a school assignment and the MVP is complete.

## What works (MVP)

- Live camera preview and a SNAP photo on the 2.8" touch screen (demo flow).
- Card lookup by name through the on-screen keyboard.
- OCR demo: the PC reads the title from one of nine stored test photos.
- Scryfall data: printings, card details, legalities and card images.
- Local collection: add a card, merge duplicates by amount, detail view,
  remove one copy or all copies.
- Import and export the collection as JSON with the PC service.
- A DECKS page is present as a placeholder.

Live photo-to-OCR is not part of the MVP; the scan page shows a demo notice.

## Hardware

| Board | Hardware | Role |
| --- | --- | --- |
| Cam | AI-Thinker ESP32-CAM (4 MB flash) | Captures JPEG frames and streams them over UART. |
| Master | ESP32 dev board (2 MB flash) | WiFi, OCR requests, Scryfall, collection storage, UART routing. |
| Screen | CYD 2.8" TPM408-2.8 (ESP32, ST7789, XPT2046) | Touch UI, drawing and JPEG rendering. |
| PC | Any computer with WiFi and Python | PaddleOCR and the Flask service for `/ocr`, `/export` and `/import`. |
| Power | 18650 cell, TP4056 charger, MT3608 boost converter | Portable supply for the device. |

## How it works

```
ESP32-CAM --JPEG frames (UART 921600)--> Master --commands/results (UART 921600)--> Screen
                                           |
                                           +-- WiFi --> PC: Flask + PaddleOCR
                                           +-- HTTPS -> Scryfall API
```

The Master is the hub. It reads UART lines from the Screen, performs the request
(Scryfall lookup, OCR, storage or file transfer) and sends the result back. The
collection stays on the device; only the JPEGs used for OCR and the exported
JSON are sent to the PC.

## Repository layout

| Path | Contents |
| --- | --- |
| [`Code/`](Code/README.md) | Firmware for the three boards and the PC service, with build instructions. |
| [`Documentation/`](Documentation/README.md) | The ontwikkeldocument (Markdown and PDF) and supporting research. |
| [`Media/`](Media/README.md) | Diagrams and photos used in the documentation. |
| `.clang-format` | Code style used for all C++ sources. |

## Quick start

1. Start the PC service:

   ```bash
   cd Code/paddleOCR
   python -m venv .venv
   .venv\Scripts\activate          # Windows
   pip install flask paddleocr paddlepaddle opencv-python numpy
   python server.py
   ```

   Leave the server running in its own terminal.

2. In a second terminal, set the WiFi credentials and the PC address in the
   Master:

   ```bash
   cd Code/Master
   idf.py menuconfig
   # Mana Snapper -> Wi-Fi SSID / Wi-Fi password / Card server base URL
   ```

3. From the repository root, build and flash in order: Cam, then Master, then
   Screen.

   ```bash
   cd Code/Cam && idf.py build flash monitor
   cd ../Master && idf.py build flash monitor
   cd ../Screen && idf.py build flash monitor
   ```

The full setup, wiring tables, protocol summary and troubleshooting are in
[`Code/README.md`](Code/README.md).

## Documentation

- [Ontwikkeldocument (Markdown)](Documentation/Ontwikkel_document/Ontwikkeldocument-The-ManaSnapper.md)
- [Ontwikkeldocument (PDF)](Documentation/Ontwikkel_document/Ontwikkeldocument-The-ManaSnapper.pdf)

## Notes and limitations

- The collection is stored in SPIFFS on the Master; the microSD module is not
  wired up yet.
- Flashing the Master with `idf.py flash` rewrites its SPIFFS partition, so the
  saved collection is replaced by the demo collection. Export the collection
  first, or use `idf.py app-flash` when only the application changed.
- The Master application partition is 1 MB and almost full (about 1% free).
  Large libraries cannot be added without repartitioning.
- The OCR demo reads stored test photos; the live scan-to-OCR path is future
  work.
- DECKS is a placeholder page.

## Credits

Team The ManaEngineer. Card data and images are provided by
[Scryfall](https://scryfall.com); Magic: The Gathering is property of Wizards of
the Coast. Built with ESP-IDF, cJSON, esp_jpeg, esp32-camera, PaddleOCR and
Flask. This is a non-commercial educational project.
