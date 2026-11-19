#pragma once

#include <stddef.h>
#include <stdint.h>

void imageRecognitionInit();

// Waits (up to ~10 s) for the WiFi connection to have an IP address.
bool imageRecognitionEnsureWifi();

// Sends a JPEG to the OCR server and copies the first recognised text line
// (the card title) into `name`. Blocks until the server answers.
bool imageRecognitionGetCardName(const uint8_t* imageData, size_t imageLength,
                                 char* name, size_t nameSize);
