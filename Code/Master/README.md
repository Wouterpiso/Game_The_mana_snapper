# Master esp32

This is where the code for the master esp32 goes (the one without the camara).

Code explinations will come soon.
﻿# Master controller

The master ESP32 owns application state, card data, recognition results, and
communication with the screen, camera, and other peripherals.

Build with PlatformIO:

```text
pio run
pio test -e native
```

The `src/` tree is split by responsibility so protocol and state-machine code
can be tested independently from ESP32 hardware links.