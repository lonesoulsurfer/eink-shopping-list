# Third-party notices

The project code uses the MIT License (see [LICENSE](LICENSE)). These parts come from other projects and keep their own licences.


- firmware/eink-shopping-list/qrcode_ricmoo.c / .h: QRCode by Richard Moore,
  MIT License (copyright notice retained in the files).
- firmware/eink-shopping-list/google_roots.h: Google Trust Services root
  certificates, taken from Mozilla's CA store (public data).
- LilyGo-EPD47 (installed separately, not included in the source) is GPL-3.0.
  It is compiled into the ready-built firmware in docs/firmware/, so that
  firmware is distributed under the GPL-3.0. Its complete corresponding source
  is this repository plus the libraries listed in README.md.
- docs/firmware/*/boot_app0.bin and bootloader.bin come from the Arduino-ESP32
  core by Espressif (LGPL-2.1 / Apache-2.0).
