#pragma once
#include <Arduino.h>

// Install "QRCode" by Richard Moore via Library Manager (github.com/ricmoo/QRCode).
// Draws a small QR code into the given framebuffer at (originX, originY),
// pointing at the given text/URL.

// Version 4 = 33x33 modules - comfortably fits both the short join URL
// and the longer "WIFI:T:nopass;S:...;;" string used for scan-to-join.
// Callers use this constant (rather than a hardcoded 29 or 33) to size
// and position the QR block consistently.
static const int QR_MODULES = 33;

void draw_qr_code(uint8_t* framebuffer, const char* text, int originX, int originY, int moduleSize);
