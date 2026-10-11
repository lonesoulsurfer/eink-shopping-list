#include "web_server.h"
#include "webpage.h"
#include "redraw_flag.h"
#include "notify_queue.h"
#include "wifi_setup.h"
#include "app_config.h"
#include "send_request.h"
#include "device_settings.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static ListStore* g_store = nullptr;
static FavoritesStore* g_favorites = nullptr;
static volatile bool g_resetRequested = false;  // single byte: atomic on ESP32

bool web_server_reset_requested() { return g_resetRequested; }

static bool parseBody(AsyncWebServerRequest* req, uint8_t* data, size_t len, JsonDocument& doc) {
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    req->send(400, "application/json", "{\"error\":\"bad json\"}");
    return false;
  }
  return true;
}

void web_server_begin(ListStore* store, FavoritesStore* favorites) {
  g_store = store;
  g_favorites = favorites;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", PAGE_HTML);
  });

  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "application/json", MANIFEST_JSON);
  });

  // --- Ad-hoc list ---

  server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", g_store->toJson());
  });

  server.on("/api/add", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      String text = doc["text"] | "";
      String who = doc["addedBy"] | "";
      text.trim();
      if (text.length() == 0) {
        req->send(400, "application/json", "{\"error\":\"empty text\"}");
        return;
      }
      String id = g_store->addItem(text, who);
      if (id.length() == 0) {  // MAX_ITEMS reached - see store_limits.h
        req->send(409, "application/json", "{\"error\":\"list full\"}");
        return;
      }
      request_redraw();
      req->send(200, "application/json", "{\"id\":\"" + id + "\"}");
    });

  server.on("/api/toggle", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      String id = doc["id"] | "";
      bool ok = g_store->toggleDone(id);
      if (ok) request_redraw();
      req->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
    });

  server.on("/api/remove", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      String id = doc["id"] | "";
      bool ok = g_store->removeItem(id);
      if (ok) request_redraw();
      req->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
    });

  server.on("/api/clear-all", HTTP_POST, [](AsyncWebServerRequest* req) {
    g_store->clearAll();
    request_redraw();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // --- Everyday items (favorites) ---

  server.on("/api/favorites", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", g_favorites->toJson());
  });

  server.on("/api/favorites/toggle", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      String id = doc["id"] | "";
      bool nowNeeded = false;
      String text;
      bool ok = g_favorites->toggleNeeded(id, nowNeeded, text);
      if (ok) {
        if (nowNeeded) {
          // Just turned ON, not off - worth notifying about.
          queue_notification(text);
        }
        request_redraw();
      }
      req->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
    });

  server.on("/api/favorites/add", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      String text = doc["text"] | "";
      text.trim();
      if (text.length() == 0) {
        req->send(400, "application/json", "{\"error\":\"empty text\"}");
        return;
      }
      String id = g_favorites->addFavorite(text);
      if (id.length() == 0) {  // MAX_FAVORITES reached - see store_limits.h
        req->send(409, "application/json", "{\"error\":\"list full\"}");
        return;
      }
      request_redraw();
      req->send(200, "application/json", "{\"id\":\"" + id + "\"}");
    });

  server.on("/api/favorites/remove", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      String id = doc["id"] | "";
      bool ok = g_favorites->removeFavorite(id);
      if (ok) request_redraw();
      req->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
    });

  // --- Shopping trip export: clears ad-hoc list AND resets favorite flags ---

  server.on("/api/export-complete", HTTP_POST, [](AsyncWebServerRequest* req) {
    g_store->clearAll();
    g_favorites->resetAllNeeded();
    request_redraw();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // --- Send button settings: sender Gmail + up to 3 named recipients ---
  // The app password is write-only: GET only says whether one is saved, it
  // never sends it back to the phone. Anyone on the home WiFi can change
  // these (the web app has no login) - acceptable for a household board.

  server.on("/api/settings/email", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["from"] = config_get_email_from();
    doc["passwordSet"] = config_get_email_app_password().length() > 0;
    doc["maxRecipients"] = (int)MAX_RECIPIENTS;
    JsonArray arr = doc["recipients"].to<JsonArray>();
    for (const auto& r : config_get_recipients()) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = r.name;
      o["email"] = r.email;
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/settings/email", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;

      String from = config_compact(doc["from"] | "");
      if (from.length() > 0 && from.indexOf('@') <= 0) {
        req->send(400, "application/json", "{\"error\":\"The Gmail address doesn't look right.\"}");
        return;
      }
      // Blank password field = keep the saved one (the page never has it).
      String pass = doc["password"] | "";
      if (config_compact(pass).length() == 0) pass = config_get_email_app_password();

      std::vector<Recipient> list;
      for (JsonObject o : doc["recipients"].as<JsonArray>()) {
        Recipient r;
        r.name = o["name"] | "";
        r.email = config_compact(o["email"] | "");
        if (r.email.length() == 0) continue;  // blank row
        if (r.email.indexOf('@') <= 0) {
          req->send(400, "application/json",
                    "{\"error\":\"One of the recipient addresses doesn't look right.\"}");
          return;
        }
        list.push_back(r);
      }
      if (list.size() > MAX_RECIPIENTS) {
        req->send(400, "application/json", "{\"error\":\"Up to 3 recipients.\"}");
        return;
      }

      config_set_email_sender(from, pass);
      config_set_recipients(list);
      config_save();
      request_redraw();  // the Send button may appear or disappear
      req->send(200, "application/json", "{\"ok\":true}");
    });

  // --- Email the list from the phone ---
  // Only files the request; loop() does the actual send (see send_request.h)
  // and the page polls /api/send-status for the result.

  server.on("/api/send", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      if (!parseBody(req, data, len, doc)) return;
      if (!config_email_enabled()) {
        req->send(400, "application/json", "{\"error\":\"Email isn't set up yet.\"}");
        return;
      }
      int index = doc["recipient"] | -1;
      std::vector<Recipient> rcpts = config_get_recipients();
      if (index < 0 || (size_t)index >= rcpts.size()) {
        req->send(400, "application/json", "{\"error\":\"Unknown recipient.\"}");
        return;
      }
      uint32_t id = send_request_submit((size_t)index, rcpts[index].name);
      if (id == 0) {
        req->send(409, "application/json", "{\"error\":\"Already sending - try again in a moment.\"}");
        return;
      }
      req->send(200, "application/json", "{\"id\":" + String(id) + "}");
    });

  server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"version\":\"" FIRMWARE_VERSION "\"}");
  });

  server.on("/api/send-status", HTTP_GET, [](AsyncWebServerRequest* req) {
    uint32_t id;
    SendState state;
    String name;
    send_request_status(id, state, name);
    JsonDocument doc;
    doc["id"] = id;
    doc["state"] = send_state_name(state);
    doc["name"] = name;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // --- Re-run setup (forgets WiFi only), no computer needed ---

  server.on("/api/reset-config", HTTP_POST, [](AsyncWebServerRequest* req) {
    // send() only QUEUES the response - the AsyncTCP task transmits it
    // after this handler returns. So this handler must return promptly and
    // must not restart the board or tear down WiFi itself: delaying here
    // would block the very task that sends the response, and erasing the
    // WiFi config from inside the network stack's own task is asking for
    // trouble. loop() does the reset ~1s later instead.
    req->send(200, "application/json", "{\"ok\":true}");
    g_resetRequested = true;
  });

  server.begin();
}
