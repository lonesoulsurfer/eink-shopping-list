#pragma once
#include <Arduino.h>

// Sends "Need <itemText>" as a Pushover push notification. Only call this
// from loop() - never from an AsyncWebServer callback - since the HTTPS
// handshake can take a second or two. See notify_queue.h for the pattern
// used to get item names from callbacks to loop() safely.
void notify_needed(const String& itemText);
