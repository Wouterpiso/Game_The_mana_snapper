# Master ESP32

Central board: WiFi station, PaddleOCR client, Scryfall lookups, the card
collection and the UART protocol towards the Cam and the Screen.

- `States/StateMachine.*` — card flow: name lookup, printing picker, add,
  detail, remove, import/export.
- `API/API.*` — Scryfall search plus collection export/import.
- `collection/Data.*` — in-memory collection persisted to
  `/storage/collection.json` (SPIFFS).
- `image_recognition/*` — WiFi and OCR requests.
- `display/DisplayCommunication.*` — line protocol to the Screen.
- `main/Kconfig.projbuild` — WiFi SSID/password and server base URL.

Build/flash instructions and wiring are in [../README.md](../README.md).
