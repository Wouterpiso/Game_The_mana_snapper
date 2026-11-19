# The Mana Snapper

Card scanner and collection manager built on three ESP32 boards plus a small
Flask server. Point the camera at a Magic: The Gathering card, the OCR server
reads its title, Scryfall supplies the card data and the Screen shows the
collection.

## Projects

| Folder       | Board                    | Role                                                        |
| ------------ | ------------------------ | ----------------------------------------------------------- |
| `Cam/`       | ESP32-CAM (AI-Thinker)   | Streams VGA JPEG frames over UART.                          |
| `Master/`    | ESP32                    | WiFi, OCR requests, Scryfall, collection storage, protocol. |
| `Screen/`    | CYD (ESP32 + TPM408-2.8) | Touch UI, ST7789 panel and JPEG rendering.                  |
| `paddleOCR/` | PC                       | Flask OCR/export/import server.                             |

UART links (921600 baud):

```
ESP32-CAM IO1 (U0T)  -> Master IO26 (UART2 RX)
Master IO25 / IO26   -> Screen IO27 / IO22 (UART2 on the Screen)
All boards share GND.
```

## Requirements

- ESP-IDF **5.4** (with the `export` script sourced for your shell).
- Python 3.10+ with `flask`, `paddleocr`, `opencv-python` and `numpy`.
- All three boards on the same 2.4 GHz WiFi network as the PC running the OCR
  server.

## OCR server

```bash
cd paddleOCR
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install flask paddleocr opencv-python numpy
python server.py
```

The server listens on `0.0.0.0:5000` and writes the exported collection to
`paddleOCR/collection_export.json`. Allow port 5000 through the PC firewall.

## Configuration

WiFi credentials and the server address are Kconfig options, not hardcoded:

```bash
cd Master
idf.py menuconfig
# Mana Snapper -> Wi-Fi SSID / Wi-Fi password / Card server base URL
```

`Card server base URL` is the server without a path, for example
`http://192.168.1.79:5000`. The Master derives `/ocr`, `/export` and `/import`
from it. Set `Wi-Fi SSID` and `Wi-Fi password` to the network shared with the PC.

## Build and flash order

Flash in this order so every board is already listening when the next one boots:

```bash
cd Cam    && idf.py build flash monitor     # 1. camera
cd ../Master && idf.py build flash monitor  # 2. master
cd ../Screen && idf.py build flash monitor  # 3. screen
```

Pass `-p <port>` when several boards are connected. The Monitor can be left
open per board; camera frames and OCR results are logged on the Master.

> **Note:** the Master packs the OCR demo images (`Master/test_Image/`) into its
> `storage` SPIFFS partition, and `idf.py flash` rewrites that partition. Always
> `flash` the Master after the Cam and *before* the Screen, and expect the saved
> collection to be replaced by the demo collection after every Master flash. Use
> the Screen's IMPORT/EXPORT menu (or `paddleOCR/server.py`) to keep a copy on
> the PC.
