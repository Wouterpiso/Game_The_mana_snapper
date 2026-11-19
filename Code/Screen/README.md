# Screen firmware (CYD TPM408-2.8)

The Screen is the user interface of The ManaSnapper: a 2.8" 320 x 240 touch
display that talks to the Master over UART. It draws the menus, the camera
preview, card details and collection pages, and sends every user action to the
Master as a single command line.

## Hardware

| Part | Detail |
| --- | --- |
| Panel | TPM408-2.8, 2.8", 320 x 240, **ST7789** controller (not ILI9341). |
| LCD bus | SPI2, MOSI 13, SCLK 14, CS 15, DC 2, no reset line, backlight on GPIO21. |
| LCD speed | 30 MHz, 16-bit RGB565. |
| Touch | XPT2046 on SPI3, SCLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 (active low). |
| Master link | UART2, TX 22, RX 27, 921600 baud, 8N1. |

## Display behaviour

The panel is initialised as an ST7789 with landscape orientation: axes swapped,
X mirrored, colour inversion off, RGB element order and big-endian pixel data.
Drawing happens in a full-frame backbuffer of 320 x 240 x 2 = **153,600 bytes**
in DMA-capable RAM. `present()` pushes the complete frame in one transfer, which
prevents tearing and flicker.

That backbuffer is the reason this firmware must stay lean. The CYD has very
little spare DRAM, so adding another large buffer (image cache, second frame,
big lookup tables) can make the allocation fail and leave the screen blank or
showing random colours. Images are decoded and drawn one at a time for the same
reason.

## Layout

| Path | Contents |
| --- | --- |
| `main/main.cpp` | UART, line protocol, JPEG receiving, page state and the UI loop. |
| `main/MenuScreen.*` | Drawing and hit-testing for every page. |
| `components/st7789_display/` | ST7789 driver, drawing primitives and `present()`. |
| `components/drivers/` | XPT2046 touch reader with raw-range calibration. |
| `components/entities/` | Start screen entity. |
| `components/sprites/` | Logo and sprite data. |
| `img/` | Logo source image and converted C array. |
| `partitions.csv` | 3 MB application partition. |

The only external component is `espressif/esp_jpeg` (declared in
`main/idf_component.yml`), used to decode the JPEGs sent by the Master.

## Pages

- **Start screen** - logo; the first touch opens the menu.
- **Home** - ADD CARD, COLLECTION and DECKS, with EXIT.
- **ADD CARD** - TYPE NAME, SCAN DEMO and OCR DEMO.
- **Keyboard** - on-screen keyboard; sends the typed title to the Master and
  shows ADDED or FAILED.
- **Printings** - list of printings offered by the Master; pick one to add.
- **Scan demo** - live camera preview, SNAP, BACK and a demo notice. The SNAP
  photo is shown but not sent to OCR in the MVP.
- **OCR demo** - reads one of the nine stored test photos and shows the
  recognised text, or OCR FAILED.
- **Collection** - list with colours and amounts, card detail, card image,
  legalities, other fields, remove-confirmation and the IMPORT/EXPORT menu.
- **DECKS** - placeholder page.

Every menu and content page has a visible EXIT or BACK control. Import and export wait up to 30
seconds for the Master; the printing list waits up to 40 seconds; an interrupted
image transfer is dropped after 3 seconds. Incoming JPEGs are limited to 64 KB
on this side.

## Build and flash

From this folder:

```bash
idf.py set-target esp32      # only for a fresh checkout without sdkconfig
idf.py build
idf.py -p COM5 flash monitor
```

The application partition is 3 MB, with roughly 4% free in the current build.
The display backbuffer is allocated at boot; if the screen stays dark, check the
Monitor for the backbuffer allocation error before changing the panel settings.

## Notes

- The TPM408-2.8 needs the ST7789 driver and the mirror/orientation settings in
  `St7789Display.cpp`. Driving it as an ILI9341 produces wrong colours or an
  offset image.
- Touch coordinates are mapped with the fixed raw range 200-3800 in
  `TouchReader.cpp`. If taps land consistently off-target, adjust those values.
- The Screen never stores data; it only renders what the Master sends.

## Related

- [Firmware and PC service overview](../README.md)
- [Master firmware](../Master/README.md)
- [Cam firmware](../Cam/README.md)
