#pragma once
#include "list_store.h"
#include "favorites.h"
#include <vector>

// Uses the LilyGo-EPD47 library (github.com/Xinyuan-LilyGO/LilyGo-EPD47),
// a fork of vroland/epdiy. This is a PARALLEL-bus panel, not SPI - do not
// substitute GxEPD2 here, it won't drive this display.
//
// Install via: Sketch -> Include Library -> Add .ZIP Library, using the
// zip from the LilyGo-EPD47 repo (or Library Manager search "LilyGo EPD47"
// if it's listed there by the time you build this).

enum TouchTargetType {
  TOUCH_FAVORITE,
  TOUCH_JOIN_BUTTON,
  TOUCH_CLOSE_QR,   // whole-screen target while the QR screen is up
  TOUCH_SEND_BUTTON,
  TOUCH_PICK_RECIPIENT,   // id = recipient index ("0".."2")
  TOUCH_CANCEL_PICKER,
};

struct TouchTarget {
  String id;  // favorite id; empty/unused for the join button
  TouchTargetType type;
  int x, y, w, h;  // bounding box in panel pixel coordinates
};

void display_init();

// First-time setup (no WiFi saved yet): plain instructions for joining the
// setup hotspot from a phone. No touch targets are registered while this is
// on screen. (A board that HAS saved WiFi but can't reach it keeps showing
// the list instead, with a "No WiFi" status line - see the .ino.)
void display_render_setup_screen();

// Full redraw of the list screen: title, Send + Join buttons, everyday-items
// checkboxes, ad-hoc list. Call sparingly - see note in .cpp about
// ghosting on this panel.
//   showSendButton: only when email is configured.
//   statusText: short line next to the title ("List sent", "Send failed"),
//               or nullptr/"" for none.
void display_render(const std::vector<FavoriteItem>& favorites,
                     const std::vector<ListItem>& items,
                     bool showSendButton,
                     const char* statusText);

// Dedicated full-screen QR for joinUrl, drawn large enough to scan easily.
// Registers a single whole-screen TOUCH_CLOSE_QR target. timeoutSec is
// only used for the on-screen "closes in N seconds" text; the caller
// owns the actual timer.
void display_render_qr_screen(const String& joinUrl, int timeoutSec);

// "Send the list to:" screen - one big button per recipient name, plus
// Cancel. Registers TOUCH_PICK_RECIPIENT / TOUCH_CANCEL_PICKER targets.
// timeoutSec is only for the on-screen text; the caller owns the timer.
void display_render_send_picker(const std::vector<String>& names, int timeoutSec);

// Hit-test a raw touch point against the targets drawn by the last
// display_render() call. Returns true and fills outTarget on a hit.
bool display_hit_test(int x, int y, TouchTarget& outTarget);

void display_sleep();
