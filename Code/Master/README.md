# Master firmware

The Master is the hub of The ManaSnapper. It connects to the Cam and the Screen
over UART, joins the WiFi network, talks to Scryfall and the PC OCR service, and
owns the card collection.

## Responsibilities

- Receive JPEG frames from the Cam and forward previews/photos to the Screen.
- Serve the Screen's line-based command protocol and send results back.
- Look up card names and printings on Scryfall.
- Send JPEGs to the PC for OCR and parse the recognised title.
- Keep the collection in memory and persist it to SPIFFS after every change.
- Import and export the collection through the PC service.

Most Screen requests are handled immediately: the Master enters a state, does
the work, sends the result and returns to `Idle`. The camera path is
asynchronous: `CAPTURE` arms a photo and the next preview frame is sent to the
Screen when it arrives. The states are defined in
[`States/StateMachine.hpp`](States/StateMachine.hpp).

## Layout

| Path | Contents |
| --- | --- |
| `main/main.cpp` | Initialises the modules and runs the request loop. |
| `States/StateMachine.*` | Request handling and the state enum. |
| `API/API.*` | Scryfall search, printings, card images, export and import. |
| `collection/Data.*` | cJSON collection and SPIFFS persistence. |
| `display/DisplayCommunication.*` | UART1 protocol with the Screen. |
| `image_recognition/CamCommunication.*` | UART2 frame receiver for the Cam. |
| `image_recognition/ImageRecognition.*` | WiFi station and the OCR POST request. |
| `main/Kconfig.projbuild` | WiFi and server settings. |
| `test_Image/` | Nine OCR demo photos, packed into the storage partition. |
| `partitions.csv` | 1 MB application and 896 KB storage partition. |

## Boot sequence

`app_main` initialises the UARTs for the Screen and the Cam, then WiFi + NVS,
loads the saved collection (or the built-in demo card) and starts the loop. Each
Screen request is handled synchronously, one at a time. Network work is bounded
by time-outs: 12 seconds for OCR and 15 seconds for Scryfall.

## Connections

| Link | Pins | Notes |
| --- | --- | --- |
| Master -> Screen | IO17 (UART1 TX) -> IO27; IO16 (UART1 RX) <- IO22 | 921600 baud, 8N1. |
| Cam -> Master | IO26 (UART2 RX), fallback IO25 | 921600 baud; the Cam TX is IO1. |
| Ground | - | All three boards share GND. |

## Configuration

Use `idf.py menuconfig` and open the **Mana Snapper** menu:

| Option | Meaning |
| --- | --- |
| Wi-Fi SSID | Network shared with the PC. Empty by default. |
| Wi-Fi password | Password for that network. Empty by default. |
| Card server base URL | PC address without a path, for example `http://192.168.1.79:5000`. |

The Master appends `/ocr`, `/export` and `/import` to the base URL. The values
end up in the generated `sdkconfig`. Keep real credentials out of version
control; set them locally with menuconfig instead.

## Storage

The collection is stored at `/storage/collection.json` in the `storage` SPIFFS
partition. The same partition holds the demo photos used by the OCR flow.

> **Flashing wipes the storage partition.** `idf.py flash` writes the packaged
> SPIFFS image over the saved collection. Export the collection to the PC first,
> or use `idf.py app-flash` when only the application changed and the partition
> table stayed the same.

The SD card module is not connected yet. The storage code is isolated in
`collection/Data.cpp`, so moving to SD only requires changing the partition,
mount point and path.

## Build and flash

From this folder:

```bash
idf.py set-target esp32      # only for a fresh checkout without sdkconfig
idf.py menuconfig            # set WiFi and server URL
idf.py build flash monitor
```

Use `-p COM5` on Windows or `-p /dev/ttyUSB0` on Linux when needed. The
application partition is 1 MB and the current firmware leaves about 1% free, so
watch the size warning at the end of the build. If the link fails with a
cryptography or mbedtls archive copy error, run the build once more; that error
is a transient file lock on Windows.

The Monitor shows WiFi reconnects, invalid or missing camera frames and
OCR/Scryfall failures. The Cam's own USB log is silent after boot, so the Master
log and the Screen preview are the places to debug the camera link.

## Related

- [Firmware and PC service overview](../README.md)
- [Screen firmware](../Screen/README.md)
- [Cam firmware](../Cam/README.md)
- [Collection code](collection/Data.cpp)
