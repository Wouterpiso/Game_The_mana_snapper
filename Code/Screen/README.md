Code for the TPM408-2.8 small screen display.

The firmware displays the `TheManaSnapper` logo on a white screen and changes
the display to black after a touch is detected. The XPT2046 touch controller
uses the standard CYD wiring: SCLK GPIO25, MOSI GPIO32, MISO GPIO39, CS GPIO33,
and IRQ GPIO36.