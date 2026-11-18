#pragma once

#include <stddef.h>
#include <stdint.h>

void imageRecognitionInit();
bool imageRecognitionSubmit(const uint8_t* imageData, size_t imageLength);
