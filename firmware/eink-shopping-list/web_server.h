#pragma once
#include "list_store.h"
#include "favorites.h"

void web_server_begin(ListStore* store, FavoritesStore* favorites);

// True once a phone has asked (via the web app) to wipe WiFi/Pushover
// setup. The handler only raises this flag; loop() performs the actual
// reset + restart, after a short grace period so the HTTP response has
// actually gone out. See the /api/reset-config handler for why.
bool web_server_reset_requested();
