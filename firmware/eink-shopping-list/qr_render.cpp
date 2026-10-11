#include "qr_render.h"
#include "epd_driver.h"
#include "qrcode_ricmoo.h"

void draw_qr_code(uint8_t* framebuffer, const char* text, int originX, int originY, int moduleSize) {
  // Version 4 QR (33x33 modules) - see QR_MODULES in qr_render.h. Fits a
  // short URL or the "WIFI:T:nopass;S:...;;" join string comfortably at
  // low error-correction. Bump the version (and QR_MODULES to match) if
  // you ever encode something longer, like a secured-network WiFi string
  // with a long password.
  const int qrVersion = 4;
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, text);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        epd_fill_rect(originX + x * moduleSize, originY + y * moduleSize,
                       moduleSize, moduleSize, 0x00, framebuffer);
      }
    }
  }
}
