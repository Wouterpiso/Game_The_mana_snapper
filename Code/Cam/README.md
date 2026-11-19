# ESP32-CAM

AI-Thinker ESP32-CAM firmware. It grabs a small JPEG every ~300 ms and sends it
to the Master over UART (the camera's TX pad, GPIO1) as:

    A5 5A E7 C3 | 4-byte little-endian length | JPEG bytes

The Master (UART2, RX on IO26 / TX on IO25) forwards frames to the Screen while
the ADD CARD page is open.

Wiring between the boards:

    ESP32-CAM U0T (IO1) -> Master IO26 (camera RX)
    ESP32-CAM GND       -> Master GND

The Master listens on IO26 and, when nothing arrives during the preview, also
retries on IO25, so either of those two pins works.

Flash with `idf.py -p <port> flash` from this folder. The camera streams VGA
(640x480) JPEG at 921600 baud. OCR needs that resolution because the card title
is only a few pixels tall at QVGA; the Screen halves the frame for the preview.
