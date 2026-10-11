#pragma once

// Device settings that rarely need changing. Nothing personal goes here:
// WiFi, email, Pushover and the wireless-update password are all entered
// on the board's setup page (see wifi_setup.h), not in code.

// Firmware version: shown at the bottom of the phone web app and printed
// to the Serial Monitor at startup. Bump it for each release.
#define FIRMWARE_VERSION "1.0.0"

// Network name: the web app is at http://shoppinglist.local
// Change it if you run more than one board on the same network.
#define MDNS_HOSTNAME "shoppinglist"

// Screen orientation. 1 = rotated 180 degrees, so the USB-C port is on the
// LEFT when the screen is the right way up. 0 = the panel's native
// orientation (USB-C on the right). Touch is flipped to match automatically.
#define SCREEN_ROTATE_180 1
