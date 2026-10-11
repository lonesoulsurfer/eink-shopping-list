#include "favorites.h"
#include "store_lock.h"
#include "store_limits.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static const char* NVS_NAMESPACE = "shopfavs";
static const char* NVS_KEY = "favs_json";
static const char* NVS_SEEDED_KEY = "seeded";

// Caller holds the lock (or is still single-threaded, in begin()).
static String uniqueFavId(const std::vector<FavoriteItem>& items) {
  while (true) {
    String id = randomId("f");
    bool clash = false;
    for (const auto& it : items) if (it.id == id) { clash = true; break; }
    if (!clash) return id;
  }
}

void FavoritesStore::seedDefaults() {
  // Starter set - edit/add/remove from the web app once it's running,
  // this is just so the board isn't blank on first boot.
  static const char* defaults[] = {"Milk", "Eggs", "Bread", "Butter", "Coffee",
                                    "Chocolate", "Fruit", "Cheese", "Tea",
                                    "Margarine", "Toothpaste", "Body wash"};
  for (auto name : defaults) {
    FavoriteItem item;
    item.id = uniqueFavId(items_);
    item.text = name;
    item.needed = false;
    items_.push_back(item);
  }
  save();
}

void FavoritesStore::begin() {
  if (!mutex_) mutex_ = xSemaphoreCreateMutex();
  StoreLock lock(mutex_);
  load();
  prefs.begin(NVS_NAMESPACE, true);
  bool seeded = prefs.getBool(NVS_SEEDED_KEY, false);
  prefs.end();
  if (!seeded) {
    seedDefaults();
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBool(NVS_SEEDED_KEY, true);
    prefs.end();
  }
}

std::vector<FavoriteItem> FavoritesStore::getItems() {
  StoreLock lock(mutex_);
  return items_;  // copy made while locked
}

bool FavoritesStore::toggleNeeded(const String& id, bool& nowNeeded, String& text) {
  StoreLock lock(mutex_);
  for (auto& item : items_) {
    if (item.id == id) {
      item.needed = !item.needed;
      nowNeeded = item.needed;
      text = item.text;
      save();
      return true;
    }
  }
  return false;
}

void FavoritesStore::resetAllNeeded() {
  StoreLock lock(mutex_);
  for (auto& item : items_) item.needed = false;
  save();
}

String FavoritesStore::addFavorite(const String& text) {
  StoreLock lock(mutex_);
  if (items_.size() >= MAX_FAVORITES) return "";
  FavoriteItem item;
  item.id = uniqueFavId(items_);
  item.text = utf8Truncate(text, MAX_ITEM_TEXT_BYTES);
  item.needed = false;
  items_.push_back(item);
  save();
  return item.id;
}

bool FavoritesStore::removeFavorite(const String& id) {
  StoreLock lock(mutex_);
  for (size_t i = 0; i < items_.size(); i++) {
    if (items_[i].id == id) {
      items_.erase(items_.begin() + i);
      save();
      return true;
    }
  }
  return false;
}

size_t FavoritesStore::count() {
  StoreLock lock(mutex_);
  return items_.size();
}

String FavoritesStore::toJson() {
  StoreLock lock(mutex_);
  return toJsonLocked();
}

String FavoritesStore::toJsonLocked() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto& item : items_) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = item.id;
    o["text"] = item.text;
    o["needed"] = item.needed;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

void FavoritesStore::save() {
  prefs.begin(NVS_NAMESPACE, false);
  String json = toJsonLocked();
  if (prefs.putString(NVS_KEY, json) == 0) {
    Serial.printf("FavoritesStore: SAVE FAILED (%u bytes) - changes won't survive a reboot\n",
                  (unsigned)json.length());
  }
  prefs.end();
}

void FavoritesStore::load() {
  prefs.begin(NVS_NAMESPACE, true);
  String raw = prefs.getString(NVS_KEY, "[]");
  prefs.end();

  items_.clear();
  JsonDocument doc;
  if (deserializeJson(doc, raw)) return;

  for (JsonObject o : doc.as<JsonArray>()) {
    FavoriteItem item;
    item.id = o["id"].as<String>();
    item.text = o["text"].as<String>();
    item.needed = o["needed"].as<bool>();
    items_.push_back(item);
  }
}
