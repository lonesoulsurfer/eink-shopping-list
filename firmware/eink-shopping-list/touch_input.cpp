// ============================================================================
// Matches the official LilyGo-EPD47 bundled "touch" example (File -> Examples
// -> LilyGo EPD47 -> touch) - uses SensorLib's TouchDrvGT911 driver rather
// than raw I2C register reads. Pin numbers below are copied directly from
// that library's own utilities.h, ESP32-S3 block:
//   BOARD_SCL=17, BOARD_SDA=18, TOUCH_INT=47
// ============================================================================

#include "touch_input.h"
#include "epd_driver.h"   // for EPD_WIDTH / EPD_HEIGHT
#include "device_settings.h"  // SCREEN_ROTATE_180
#include <Wire.h>
#include <TouchDrvGT911.hpp>  // Library Manager: "SensorLib" by Lewis He

static const int BOARD_SDA_PIN = 18;
static const int BOARD_SCL_PIN = 17;
static const int TOUCH_INT_PIN = 47;

static TouchDrvGT911 touch;
static bool touchReady = false;

// Edge detection rather than a time debounce. With a time debounce, a held
// finger fires a new "tap" every interval, and a finger still on the glass
// after a blocking e-paper refresh (1-2s) registers as a fresh tap - e.g.
// opening the Join QR screen and instantly closing it again.
//
// A tap fires once on press, then nothing until the finger has been off the
// glass for RELEASE_MS. The release window (rather than a single no-touch
// read) covers the GT911 occasionally returning "no point" mid-press when
// polled between its own scan frames.
static bool pressed = false;
static unsigned long lastSeenTouchMs = 0;
static const unsigned long RELEASE_MS = 150;

void touch_init() {
  Wire.begin(BOARD_SDA_PIN, BOARD_SCL_PIN);

  // Wake the touch controller in case it was left in sleep state.
  pinMode(TOUCH_INT_PIN, OUTPUT);
  digitalWrite(TOUCH_INT_PIN, HIGH);

  // GT911's reset pin uses a hardware pull-up on this board, so the I2C
  // address can't be selected in software - scan for whichever address
  // it's actually responding on (0x14 or 0x5D).
  uint8_t touchAddress = 0;
  Wire.beginTransmission(0x14);
  if (Wire.endTransmission() == 0) touchAddress = 0x14;
  Wire.beginTransmission(0x5D);
  if (Wire.endTransmission() == 0) touchAddress = 0x5D;

  if (touchAddress == 0) {
    Serial.println("Touch: failed to find GT911 - check wiring.");
    touchReady = false;
    return;
  }

  touch.setPins(-1, TOUCH_INT_PIN);
  if (!touch.begin(Wire, touchAddress, BOARD_SDA_PIN, BOARD_SCL_PIN)) {
    Serial.println("Touch: begin() failed.");
    touchReady = false;
    return;
  }

  touch.setMaxCoordinates(EPD_WIDTH, EPD_HEIGHT);
  // Fixes up axis orientation between the touch digitizer and the panel's
  // pixel grid - taken directly from the library's own example.
  touch.setSwapXY(true);
  touch.setMirrorXY(false, true);

  touchReady = true;
  Serial.println("Touch ready.");
}

bool touch_poll(TapPoint& point) {
  if (!touchReady) return false;

  unsigned long now = millis();
  int16_t x, y;
  uint8_t touched = touch.getPoint(&x, &y, 1);

  if (touched) {
    lastSeenTouchMs = now;
    if (pressed) return false;  // same press still in progress
    pressed = true;
#if SCREEN_ROTATE_180
    // The display is drawn rotated 180 degrees (see display_render.cpp),
    // so flip the tap the same way to keep it in drawing coordinates.
    x = EPD_WIDTH - 1 - x;
    y = EPD_HEIGHT - 1 - y;
#endif
    point.x = x;
    point.y = y;
    return true;
  }

  if (pressed && (now - lastSeenTouchMs > RELEASE_MS)) {
    pressed = false;  // finger genuinely lifted
  }
  return false;
}
