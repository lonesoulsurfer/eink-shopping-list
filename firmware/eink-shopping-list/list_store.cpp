#include "list_store.h"
#include "store_lock.h"
#include "store_limits.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static const char* NVS_NAMESPACE = "shoplist";
static const char* NVS_KEY = "items_json";

void ListStore::begin() {
  if (!mutex_) mutex_ = xSemaphoreCreateMutex();
  StoreLock lock(mutex_);
  load();
}

String ListStore::addItem(const String& text, const String& addedBy) {
  StoreLock lock(mutex_);
  if (items_.size() >= MAX_ITEMS) return "";
  ListItem item;
  bool unique;
  do {
    item.id = randomId("");
    unique = true;
    for (const auto& it : items_) if (it.id == item.id) { unique = false; break; }
  } while (!unique);
  item.text = utf8Truncate(text, MAX_ITEM_TEXT_BYTES);
  item.addedBy = utf8Truncate(addedBy, MAX_NAME_BYTES);
  item.done = false;
  items_.push_back(item);
  save();
  return item.id;
}

bool ListStore::removeItem(const String& id) {
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

bool ListStore::toggleDone(const String& id) {
  StoreLock lock(mutex_);
  for (auto& item : items_) {
    if (item.id == id) {
      item.done = !item.done;
      save();
      return true;
    }
  }
  return false;
}

void ListStore::clearAll() {
  StoreLock lock(mutex_);
  items_.clear();
  save();
}

size_t ListStore::count() {
  StoreLock lock(mutex_);
  return items_.size();
}

std::vector<ListItem> ListStore::getItems() {
  StoreLock lock(mutex_);
  return items_;  // copy made while locked
}

String ListStore::toJson() {
  StoreLock lock(mutex_);
  return toJsonLocked();
}

String ListStore::toJsonLocked() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto& item : items_) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = item.id;
    o["text"] = item.text;
    o["addedBy"] = item.addedBy;
    o["done"] = item.done;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

void ListStore::save() {
  prefs.begin(NVS_NAMESPACE, false);
  String json = toJsonLocked();
  if (prefs.putString(NVS_KEY, json) == 0) {
    Serial.printf("ListStore: SAVE FAILED (%u bytes) - list won't survive a reboot\n",
                  (unsigned)json.length());
  }
  prefs.end();
}

void ListStore::load() {
  prefs.begin(NVS_NAMESPACE, true);
  String raw = prefs.getString(NVS_KEY, "[]");
  prefs.end();

  items_.clear();
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, raw);
  if (err) return;  // corrupt/empty -> start fresh

  for (JsonObject o : doc.as<JsonArray>()) {
    ListItem item;
    item.id = o["id"].as<String>();
    item.text = o["text"].as<String>();
    item.addedBy = o["addedBy"].as<String>();
    item.done = o["done"].as<bool>();
    items_.push_back(item);
  }
}
