#pragma once
#include <Arduino.h>
#include <esp_random.h>

// Both lists are saved to flash as one JSON string each, and ESP-IDF caps a
// single saved string at 4000 bytes. A save over that FAILS: the board would
// keep showing the list, but the next power cut would bring back an older
// one. The caps below keep the worst
// case under 4000 bytes - checked with every character a quote mark, which
// JSON saves as two bytes (\"), so even deliberate abuse can't overflow:
//   Also need: 20 items x 40-byte text + 16-byte name  -> 3281 bytes worst
//   Everyday:  18 items x 40-byte text                 -> ~2100 bytes worst
// The board can't display more than about 9 "Also need" items at once
// anyway (see the "+N more" line in display_render.cpp).
static const size_t MAX_ITEMS = 20;
static const size_t MAX_FAVORITES = 18;
static const size_t MAX_ITEM_TEXT_BYTES = 40;
static const size_t MAX_NAME_BYTES = 16;

// Cuts s to at most maxBytes without splitting a multi-byte UTF-8
// character (emoji, accented letters) in half.
static inline String utf8Truncate(const String& s, size_t maxBytes) {
  if (s.length() <= maxBytes) return s;
  size_t cut = maxBytes;
  while (cut > 0 && ((uint8_t)s[cut] & 0xC0) == 0x80) cut--;
  return s.substring(0, cut);
}

// Random id, checked for uniqueness by the caller - two items sharing an id
// would mean tapping one toggles the other.
static inline String randomId(const char* prefix) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%s%06lx", prefix, (unsigned long)(esp_random() & 0xFFFFFF));
  return String(buf);
}
