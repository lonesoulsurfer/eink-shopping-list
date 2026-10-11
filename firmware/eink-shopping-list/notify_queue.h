#pragma once
#include <Arduino.h>

// An item becoming "needed" can be triggered from two different places:
// the AsyncWebServer callback (runs on its own FreeRTOS task) or the
// touch handler in loop(). Either way, queue it here rather than sending
// the Pushover HTTPS request directly - an HTTPS handshake can take a
// second or two, and blocking the web server's task for that long risks
// stalling other requests or tripping the watchdog. loop() drains this
// queue and does the actual network call.

// Call once from setup(), before the web server starts.
void notify_queue_init();

// Safe from any task. Never blocks: if the queue is full (8 pending), the
// notification is dropped rather than stalling the caller. Item text over
// 63 chars is truncated in the notification only, not in the stored list.
void queue_notification(const String& itemText);

// Call every loop() iteration. Returns true (and fills outText) if
// there's a queued notification to send.
bool consume_pending_notification(String& outText);
