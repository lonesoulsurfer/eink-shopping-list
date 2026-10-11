#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct FavoriteItem {
  String id;
  String text;   // "Milk"
  bool needed = false;  // toggled on when someone taps it (fridge is empty)
};

// Permanent, recurring items (milk, eggs, bread...) that always live on the
// board. Unlike ListStore items, these never get deleted when exported -
// only their "needed" flag resets. Tap the checkbox next to one (on the
// e-ink touch glass, or in the web app) to flag "we need this."
//
// Thread-safe: every public method locks an internal mutex, because the
// web server task and loop() (touch) both modify this. begin() must be
// called before any other task can touch the store.
class FavoritesStore {
 public:
  void begin();  // seeds a default set on first boot

  std::vector<FavoriteItem> getItems();

  // Flips the item's "needed" flag. Returns false if the id doesn't exist.
  // On success, fills nowNeeded with the NEW state and text with the item's
  // name - read in the same locked step as the flip, so the caller can
  // decide whether to notify without another task toggling in between.
  bool toggleNeeded(const String& id, bool& nowNeeded, String& text);

  void resetAllNeeded();  // called after a shopping trip is exported

  // Returns the new id, or "" if there are already MAX_FAVORITES.
  String addFavorite(const String& text);
  size_t count();
  bool removeFavorite(const String& id);

  String toJson();

 private:
  // Private helpers assume the caller already holds mutex_. Public methods
  // must never call other public methods, or they'd deadlock on the
  // (non-recursive) mutex.
  String toJsonLocked();
  void save();
  void load();
  void seedDefaults();

  SemaphoreHandle_t mutex_ = nullptr;
  std::vector<FavoriteItem> items_;
};
