#pragma once

#include <stddef.h>
#include <stdint.h>

void camCommunicationInit();
// Reads waiting camera bytes. The newest small preview frame is kept for the
// screen and forwarded when it asked for one.
void camCommunicationUpdate();
// The screen shows one frame at a time, so it asks for the next frame when it
// is ready for it instead of being flooded with the camera stream.
void camCommunicationRequestFrame();
// SNAP (scan demo): the next preview frame is sent to the screen as CAM_PHOTO.
// CAPTURE_FAILED is sent when no preview frame arrived in time.
void camCommunicationStartCapture();
// EXIT / failed transfer: stop waiting for the capture frame.
void camCommunicationCancelCapture();
