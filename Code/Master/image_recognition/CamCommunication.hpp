/**
 * @file CamCommunication.hpp
 * @brief Frame protocol between the ESP32-CAM and the Master.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Sets up the UART that receives camera frames.
 */
void camCommunicationInit();

/**
 * @brief Reads waiting camera bytes and handles complete frames.
 *
 * The newest small preview frame is kept for the Screen and forwarded when it
 * asked for one.
 */
void camCommunicationUpdate();

/**
 * @brief Tells the camera link that the Screen is ready for a frame.
 *
 * The Screen shows one frame at a time, so the stream is not pushed faster
 * than it can be drawn.
 */
void camCommunicationRequestFrame();

/**
 * @brief Arms the SNAP capture.
 *
 * The next preview frame is sent to the Screen as CAM_PHOTO. CAPTURE_FAILED is
 * sent when no preview frame arrives in time.
 */
void camCommunicationStartCapture();

/**
 * @brief Stops waiting for the capture frame.
 */
void camCommunicationCancelCapture();
