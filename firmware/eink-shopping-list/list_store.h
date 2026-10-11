#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct ListItem {
  String id;       // short unique id, e.g. "a3f9"
  String text;      // "buy eggs"
  String addedBy;    // optional, e.g. "Sarah"
  bool done = false;
};

// Persists the list in NVS (Preferences) as a JSON blob so it survives
// power loss / reboot. Source of truth for the whole device.
//
// Thread-safe: every public method locks an internal mutex, because the
// web server task and loop() both use this. getItems() returns a copy so
// callers (e.g. the renderer) never hold the lock while drawing.
// begin() must be called before any other task can touch the store.
class ListStore {
 public:
  void begin();

  // Returns the new item's id, or "" if the list is full (MAX_ITEMS).
  // Text and name are trimmed to the limits in store_limits.h.
  String addItem(const String& text, const String& addedBy);
  size_t count();

  bool removeItem(const String& id);
  bool toggleDone(const String& id);
  void clearAll();

  std::vector<ListItem> getItems();
  String toJson();  // for the /api/list endpoint

 private:
  // Private helpers assume the caller already holds mutex_. Public methods
  // must never call other public methods, or they'd deadlock on the
  // (non-recursive) mutex.
  String toJsonLocked();
  void save();
  void load();

  SemaphoreHandle_t mutex_ = nullptr;
  std::vector<ListItem> items_;
};
