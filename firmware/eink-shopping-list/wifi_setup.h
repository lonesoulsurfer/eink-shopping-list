#pragma once
#include <Arduino.h>

// Network setup with zero code editing, and it never blocks the board.
//
// Saved WiFi: the board shows the list and responds to touch immediately
// while WiFi connects in the background. If the saved network can't be
// reached for WIFI_STA_TRY_MS (router rebooting, router replaced), the
// "ShoppingList-Setup" hotspot + setup page opens for WIFI_PORTAL_TIMEOUT_S,
// then closes and the board goes back to quietly retrying - repeating
// forever, so it recovers by itself once the router is back, and anyone
// who needs to re-run setup can still catch the hotspot.
//
// No saved WiFi (first boot, or after a reset from the web app): the
// hotspot stays open until someone completes setup.
//
// The setup hotspot only runs until the FIRST successful connection after
// boot. After that, a dropped connection is left to the ESP32's automatic
// reconnect - the web app owns port 80 by then, so the setup page can't
// run alongside it. To re-run setup on a board that's been running (e.g.
// new router), power-cycle it, or use the web app's reset button.
//
// Uses the WiFiManager library (tzapu/WiFiManager, tested with 2.0.17).

static const unsigned long WIFI_STA_TRY_MS = 60000;
static const int WIFI_PORTAL_TIMEOUT_S = 180;

// Call once from setup(), after config_load(). Returns immediately.
void wifi_setup_start();

// Call every loop(). Returns true exactly once: on the first successful
// connection since boot (start the web server, OTA, mDNS then).
bool wifi_setup_poll();

bool wifi_setup_connected();

// True if WiFi credentials are saved in flash. Unlike WiFi.SSID() - which
// only reports the network you're CURRENTLY connected to, so it's always
// empty at boot - this reads the saved station config.
bool wifi_setup_has_saved_credentials();

// Forgets the saved WiFi network (other settings are kept and pre-filled)
// and restarts into the setup hotspot. Wired to a button in the web app.
void wifi_setup_reset_and_restart();
