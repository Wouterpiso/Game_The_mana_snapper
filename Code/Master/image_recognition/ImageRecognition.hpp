/**
 * @file ImageRecognition.hpp
 * @brief WiFi setup and card-title recognition through the OCR server.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Connects the Master to the configured WiFi network.
 */
void imageRecognitionInit();

/**
 * @brief Waits until the board has an IP address.
 * @return True when the connection is ready, false after about 10 seconds.
 */
bool imageRecognitionEnsureWifi();

/**
 * @brief Sends a JPEG to the OCR server and returns the card title.
 *
 * The first recognised text line of a card is its title, so that line is used
 * as the card name. Blocks until the server answers.
 *
 * @param imageData JPEG bytes.
 * @param imageLength Number of JPEG bytes.
 * @param name Buffer that receives the recognised title.
 * @param nameSize Size of @p name in bytes.
 * @return True when a title was recognised.
 */
bool imageRecognitionGetCardName(const uint8_t* imageData, size_t imageLength, char* name,
                                 size_t nameSize);
