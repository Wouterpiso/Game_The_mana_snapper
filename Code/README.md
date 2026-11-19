# The ManaSnapper - firmware and PC service

This folder contains the complete software for The ManaSnapper:

- **Cam** - ESP32-CAM firmware that streams JPEG frames to the Master.
- **Master** - ESP32 firmware for WiFi, PaddleOCR requests, Scryfall lookups,
  the collection and the UART protocol towards the Cam and the Screen.
- **Screen** - CYD firmware with the touch UI, the ST7789 panel and JPEG
  rendering.
- **paddleOCR** - Flask service on a PC for OCR and collection import/export.

The MVP lets a user browse a collection, type a card name on the touch screen,
run an OCR demo on stored photos, look cards up on Scryfall and exchange the
collection with the PC. Live photo-to-OCR is not part of the MVP; the scan page
shows a demo notice.

## Architecture

```
ESP32-CAM --JPEG frames (UART)--> Master --commands and results (UART)--> Screen
                                    |
                                    +-- WiFi ----> PC: Flask + PaddleOCR
                                    +-- HTTPS ---> Scryfall API
```

The Master owns the collection and performs every request; the Screen only sends
commands and draws what comes back. Both UART links run at 921600 baud, 8N1, with
hardware flow control disabled.

## Projects

| Folder | Board | Role |
| --- | --- | --- |
| [`Cam/`](Cam/README.md) | AI-Thinker ESP32-CAM | Camera and JPEG streaming. |
| [`Master/`](Master/README.md) | ESP32 dev board | Central controller, storage, network. |
| [`Screen/`](Screen/README.md) | CYD TPM408-2.8 | Touch UI and display. |
| [`paddleOCR/`](paddleOCR/README.md) | PC | OCR and JSON import/export service. |

## Wiring

| Link | Pins |
| --- | --- |
| Cam -> Master | ESP32-CAM IO1 (U0T) -> Master IO26 (UART2 RX). The Master also retries IO25 when no camera data arrives. |
| Master -> Screen | Master IO17 (UART1 TX) -> Screen IO27 (UART2 RX). |
| Screen -> Master | Screen IO22 (UART2 TX) -> Master IO16 (UART1 RX). |
| Ground | All boards share GND. |

Power comes from the 18650 cell with the TP4056 charger and MT3608 boost
converter; see the
[ontwikkeldocument](../Documentation/Ontwikkel_document/Ontwikkeldocument-The-ManaSnapper.md)
for the full power design.

## Requirements

- ESP-IDF **5.4** with the `esp32` target. Run the ESP-IDF export script in every
  new terminal, or use the ESP-IDF terminal shortcut.
- Three ESP32 boards: 4 MB flash on the Cam and the Screen, 2 MB on the Master.
  The Cam works best with PSRAM and falls back to internal RAM when it is absent.
- 64-bit Python 3.10 or newer on the PC, with pip and internet access for the
  PaddleOCR models.
- The Master and the PC on the same 2.4 GHz WiFi network.

## PC service

```bash
cd paddleOCR
python -m venv .venv
.venv\Scripts\activate            # Windows
# source .venv/bin/activate       # Linux / macOS
python -m pip install --upgrade pip
pip install flask paddleocr paddlepaddle opencv-python numpy
python server.py
```

The service listens on `0.0.0.0:5000`, so the Master can reach it over WiFi.
Allow port 5000 through the PC firewall. On the first start PaddleOCR downloads
its models; this can take a while and needs internet. The trained models are
cached for later runs. A CPU build of PaddlePaddle is enough; a GPU build works
too but is not required.

| Endpoint | Method | Request | Result |
| --- | --- | --- | --- |
| `/ocr` | POST (multipart) | JPEG in the `image` field | `{"words": ["...", "..."]}` |
| `/export` | POST (JSON) | Complete collection document | Writes `collection_export.json` next to `server.py`. |
| `/import` | GET | - | Returns the last exported document, or 404 when none exists. |

See [`paddleOCR/README.md`](paddleOCR/README.md) for details and test commands.

## Configuration

WiFi credentials and the server address are Kconfig options, not hardcoded:

```bash
cd Master
idf.py menuconfig
# Mana Snapper -> Wi-Fi SSID / Wi-Fi password / Card server base URL
```

`Card server base URL` is the base address without a path, for example
`http://192.168.1.79:5000`. The Master appends `/ocr`, `/export` and `/import`.
Find the PC address with `ipconfig` on Windows or `ip addr` on Linux. Leave no
trailing slash.

The options are stored in the generated `sdkconfig` of the Master. Do not commit
a `sdkconfig` that contains real WiFi credentials.

## Build and flash

From this folder, flash in this order so each board is already listening when
the next one boots:

```bash
cd Cam
idf.py build flash monitor         # 1. camera

cd ../Master
idf.py build flash monitor         # 2. master

cd ../Screen
idf.py build flash monitor         # 3. screen
```

Use `-p COM5` (Windows) or `-p /dev/ttyUSB0` (Linux) when several boards are
connected. Leave `monitor` with `Ctrl+]`. The Cam goes silent after it routes
UART1 to GPIO1; that is expected. The Screen preview is the quickest check that
frames arrive, and the Master log reports camera problems.

`idf.py` needs the ESP-IDF environment. If the command is not found, source
`export.ps1`/`export.sh` from the ESP-IDF installation first.

## Storage and flash budget

The Master keeps the collection in SPIFFS:

| Item | Value |
| --- | --- |
| Partition label | `storage` |
| Mount point | `/storage` |
| File | `/storage/collection.json` |
| Partition size | 896 KB (`0xE0000`) |
| Demo photos | `/storage/test1.jpeg` to `/storage/test9.jpeg` |

The nine demo photos live in [`Master/test_Image/`](Master/test_Image) and are
packed into the storage partition by `main/CMakeLists.txt`. The microSD module is
not wired up yet; the storage layer is isolated so moving to SD only changes the
partition, mount point and path.

| Board | App partition | Firmware size | Free |
| --- | --- | --- | --- |
| Master | 1 MB | `0xfea30` | about 1% |
| Screen | 3 MB | `0x2de420` | about 4% |
| Cam | default large app | `0x57d50` | about 77% |

The Master partition is nearly full. Adding a component or a large lookup table
can make the build fail; repartitioning is the only structural fix.

**Flashing replaces the storage image.** `idf.py flash` writes both the app and
the SPIFFS image, so the collection returns to the demo card and the test photos
are restored. Export the collection from the Screen before flashing the Master,
or use `idf.py app-flash` when only the application changed and the partition
layout stayed the same.

## Protocol summary

The Screen sends one newline-terminated line per request:

| Command | Meaning |
| --- | --- |
| `ADD_CARD_TITLE <name>` | Look up a typed card name. |
| `ADD_CARD_SET <index>` | Choose a printing from the previous offer. |
| `CARD_INFO <index>` | Request details for a collection entry. |
| `CARD_IMAGE <index>` | Request the card image of an entry. |
| `CARD_REMOVE <index>` | Remove all copies of an entry. |
| `CARD_REMOVE_ONE <index>` | Remove one copy of an entry. |
| `COLLECTION`, `DECKS` | Open the collection or decks page. |
| `IMPORT`, `EXPORT` | Transfer the collection with the PC service. |
| `CAPTURE`, `CAPTURE_ABORT`, `CAM_FRAME` | Camera preview and SNAP demo. |
| `OCR_DEMO` | Recognise one of the stored test photos. |
| `IDLE` | No pending request; also clears a printing offer. |

The Master answers with data lines and result lines, for example
`COLLECTION_LIST <json>`, `ADD_CARD_SETS <json>`, `CARD_INFO <json>`,
`ADD_CARD_DONE`, `ADD_CARD_FAILED`, `CARD_REMOVE_DONE`, `CARD_REMOVE_FAILED`,
`IMPORT_DONE`, `EXPORT_DONE`, `OCR_DEMO_RESULT <test> <text>` and
`CAPTURE_FAILED`. Binary images follow a header: `CARD_IMAGE <length>\n` or
`CAM_IMAGE <length>\n` / `CAM_PHOTO <length>\n`, followed by that many JPEG
bytes. The exact parser is in
[`Master/display/DisplayCommunication.cpp`](Master/display/DisplayCommunication.cpp)
and the Screen side in [`Screen/main/main.cpp`](Screen/main/main.cpp).

## Collection format

The collection uses the shape of a Scryfall list response, extended with an
`amount`:

```json
{
  "object": "list",
  "total_cards": 1,
  "has_more": false,
  "next_page": null,
  "data": [
    {
      "name": "Test Card",
      "colors": ["W"],
      "amount": 1
    }
  ]
}
```

The Master validates that imported documents contain a `data` array before it
replaces the collection. The export contains the same document, so a file can be
imported again without conversion.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Screen shows random colours or stays blank | The full-frame backbuffer needs about 153.6 KB of DMA-capable DRAM. Remove large buffers before suspecting the panel. |
| Camera frames never arrive | Check IO1 to IO26, the common ground and the camera power. While no data arrives, the Master alternates between IO26 and IO25 every 4 seconds. |
| No camera or board logs on USB | Expected on the Cam after boot; UART1 takes over GPIO1. The Screen preview is the quickest check that frames still arrive. |
| OCR always fails | The Master must reach the PC: check the server URL, port 5000, the firewall and that both are on the same 2.4 GHz network. |
| Collection resets after flashing | `flash` rewrites the storage partition. Export first, or use `app-flash` for app-only updates. |
| Master build fails after adding code | The app partition is almost full. Remove code or repartition. |
| Touch feels offset | The XPT2046 raw range is calibrated to 200-3800 in `TouchReader.cpp`; tune it for the panel. |

## Documentation

- [Ontwikkeldocument (Markdown)](../Documentation/Ontwikkel_document/Ontwikkeldocument-The-ManaSnapper.md)
- [Ontwikkeldocument (PDF)](../Documentation/Ontwikkel_document/Ontwikkeldocument-The-ManaSnapper.pdf)
- [Project overview](../README.md)
