# ESP32-CAM firmware

Streams JPEG preview frames from an AI-Thinker ESP32-CAM to the Master over UART.
The camera only sends; there is no command channel back.

## Behaviour

- Sensor: QVGA (320 x 240), JPEG quality 22.
- The firmware grabs and sends a frame, then waits 40 ms before the next one.
  The effective frame rate depends on the JPEG size and the 921600 baud link.
- UART1 at 921600 baud, 8N1, no flow control, TX on GPIO1 (the U0T pad).
- Frame format:

  ```
  A5 5A E7 C3 | 4-byte little-endian length | JPEG bytes
  ```

- The VGA frame size in the camera configuration only sizes the driver's
  internal buffers. `app_main` switches the sensor to QVGA after initialisation,
  so the link and the Master only see QVGA frames.
- When PSRAM is missing or fails to initialise, the firmware retries in internal
  RAM with a single frame buffer. The preview keeps working at QVGA.

The Master stores the newest frame as the preview and forwards it to the Screen
when the scan page asks for one. A SNAP request forwards the next preview frame
as the photo; the Cam itself does not store anything.

## Wiring

| ESP32-CAM | Master | Note |
| --- | --- | --- |
| GPIO1 / U0T (TX) | GPIO26 (UART2 RX) | The Master also accepts GPIO25 as camera RX and retries between the two pins. |
| GND | GND | Required; without a common ground the frames are corrupted or absent. |
| Power | - | Power the module through its normal supply. |

Only one data wire is needed: pick GPIO26 or GPIO25 on the Master and keep the
camera on GPIO1.

## Build and flash

From this folder:

```bash
idf.py set-target esp32      # only for a fresh checkout without sdkconfig
idf.py build
idf.py -p COM5 flash monitor
```

Use `/dev/ttyUSB0` or similar instead of `COM5` on Linux. The project needs
4 MB flash; `sdkconfig.defaults` selects the large single-app layout and PSRAM.

Once UART1 is routed to GPIO1, the USB monitor of the Cam goes silent because it
shares that pin with the console. This is expected. Check the live preview on the
Screen to confirm that frames arrive; the Master log reports camera problems.

## Related

- [Firmware and PC service overview](../README.md)
- [Master firmware](../Master/README.md)
- [Frame receiver](../Master/image_recognition/CamCommunication.cpp)
