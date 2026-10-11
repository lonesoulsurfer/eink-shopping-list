#pragma once

// Anything that changes list/favorites state (web API handler or a touch
// tap) calls request_redraw(). main.ino's loop() debounces these into a
// single e-paper refresh a few seconds after the last change.
void request_redraw();
bool consume_redraw_request();
