#pragma once
#include <Arduino.h>

struct TapPoint {
  int x;
  int y;
};

void touch_init();

// Non-blocking - call every loop() iteration. Returns true (and fills
// point) at most once per physical tap.
bool touch_poll(TapPoint& point);
