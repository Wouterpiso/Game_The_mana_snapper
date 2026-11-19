# Screen (CYD, TPM408-2.8)

Touch UI and JPEG rendering for the 320x240 ST7789 panel. The Screen talks to
the Master over UART2 at 921600 baud and draws:

- home menu (ADD CARD / COLLECTION / DECKS),
- card name keyboard and printing picker,
- card detail, legalities and other fields,
- collection pages with an IMPORT/EXPORT menu,
- live camera preview, SNAP photo and the OCR demo result.

Display: `components/st7789_display/St7789Display.*` (SPI2, full-frame DMA
backbuffer). Touch: `components/drivers/TouchReader.*` (XPT2046, SCLK GPIO25,
MOSI GPIO32, MISO GPIO39, CS GPIO33, IRQ GPIO36).

Build/flash instructions and wiring are in [../README.md](../README.md).
