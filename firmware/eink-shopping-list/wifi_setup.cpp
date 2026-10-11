#include "wifi_setup.h"
#include "app_config.h"
#include <WiFiManager.h>
#include <esp_wifi.h>

static WiFiManager wm;

enum WifiState { ST_STA_TRYING, ST_PORTAL, ST_CONNECTED_ONCE };
static WifiState state = ST_STA_TRYING;
static unsigned long stateSinceMs = 0;
static bool hadSavedCredentials = false;

// Portal fields. Created once in wifi_setup_start() and kept for the life of
// the program: with a non-blocking portal, WiFiManager reads them long after
// the function that registered them has returned.
static WiFiManagerParameter* pushoverTokenParam = nullptr;
static WiFiManagerParameter* pushoverKeyParam = nullptr;
static WiFiManagerParameter* otaPasswordParam = nullptr;
static WiFiManagerParameter* mailFromParam = nullptr;
static WiFiManagerParameter* mailPassParam = nullptr;
static WiFiManagerParameter* mailToParam = nullptr;

bool wifi_setup_has_saved_credentials() {
  // The driver must be initialised before esp_wifi_get_config() returns
  // real data - before that it fails and leaves the struct untouched.
  if (WiFi.getMode() == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);
  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return false;
  return conf.sta.ssid[0] != 0;
}

bool wifi_setup_connected() {
  return WiFi.status() == WL_CONNECTED;
}

static void addPortalFields() {
  // Pre-filled with the current values, so re-running setup (e.g. just to
  // change one setting) doesn't mean retyping everything.
  static WiFiManagerParameter pushoverInfo(
      "<p style='font-size:0.85em;color:#555;margin-bottom:6px;'>"
      "Optional: get a phone notification the moment someone marks an "
      "item needed. Install <a href='https://pushover.net' target='_blank'>"
      "Pushover</a> (free trial, then a one-time purchase), open it to "
      "find your key, and paste it in below. Leave blank to skip."
      "</p>");
  pushoverTokenParam = new WiFiManagerParameter(
      "pushover_token", "Pushover App Token (optional)",
      config_get_pushover_token().c_str(), 63);
  pushoverKeyParam = new WiFiManagerParameter(
      "pushover_key", "Pushover User or Group Key (optional)",
      config_get_pushover_key().c_str(), 63);

  static WiFiManagerParameter otaInfo(
      "<p style='font-size:0.85em;color:#555;margin:14px 0 6px;'>"
      "Advanced, optional: set a password (8+ characters) to allow "
      "wireless firmware updates from the Arduino IDE. Leave blank to keep "
      "wireless updates switched off. You'll need this password when "
      "uploading, so note it down.</p>");
  // type='password' masks it on screen. The hotspot is open, so this
  // travels unencrypted during setup - the same as the WiFi password on
  // the same page, and only while someone is setting up.
  otaPasswordParam = new WiFiManagerParameter(
      "ota_password", "Wireless update password (optional)",
      config_get_ota_password().c_str(), 63,
      "type='password' autocomplete='new-password'");

  static WiFiManagerParameter mailInfo(
      "<p style='font-size:0.85em;color:#555;margin:14px 0 6px;'>"
      "Optional: a <b>Send</b> button on the board emails the list to one "
      "person. Needs a Gmail account to send from, with 2-Step Verification "
      "on, and an <b>app password</b> (Google Account &gt; Security &gt; App "
      "passwords) - your normal password won't work. An app password can "
      "also read that mailbox and is stored on this board, so a separate "
      "Gmail account just for the board is safest. Leave blank to skip.</p>");
  mailFromParam = new WiFiManagerParameter(
      "mail_from", "Send from (Gmail address)",
      config_get_email_from().c_str(), 95, "type='email' autocomplete='off'");
  mailPassParam = new WiFiManagerParameter(
      "mail_pass", "Gmail app password (16 letters)",
      config_get_email_app_password().c_str(), 39,
      "type='password' autocomplete='new-password'");
  // The setup page sets the FIRST recipient only; more (up to 3, with
  // names) are added in the web app's settings section.
  std::vector<Recipient> rcpts = config_get_recipients();
  String firstTo = rcpts.empty() ? String("") : rcpts[0].email;
  mailToParam = new WiFiManagerParameter(
      "mail_to", "Send list to (email address - add more later in the web app)",
      firstTo.c_str(), 95, "type='email' autocomplete='off'");

  wm.addParameter(&pushoverInfo);
  wm.addParameter(pushoverTokenParam);
  wm.addParameter(pushoverKeyParam);
  wm.addParameter(&otaInfo);
  wm.addParameter(otaPasswordParam);
  wm.addParameter(&mailInfo);
  wm.addParameter(mailFromParam);
  wm.addParameter(mailPassParam);
  wm.addParameter(mailToParam);
}

// Copies what was typed into the portal into our own settings. Only called
// after someone actually completed the portal - a plain reconnect with
// saved WiFi never touches these settings.
static void savePortalFields() {
  config_set_pushover_token(String(pushoverTokenParam->getValue()));
  config_set_pushover_key(String(pushoverKeyParam->getValue()));

  String ota = String(otaPasswordParam->getValue());
  ota.trim();
  if (ota.length() > 0 && ota.length() < OTA_PASSWORD_MIN_LEN) {
    Serial.printf("OTA password too short (%u chars, need %u) - wireless updates stay OFF.\n",
                  (unsigned)ota.length(), (unsigned)OTA_PASSWORD_MIN_LEN);
  }
  config_set_ota_password(ota);
  config_set_email_sender(String(mailFromParam->getValue()), String(mailPassParam->getValue()));

  // First recipient from the page; any others (set in the web app) are kept.
  String to = config_compact(String(mailToParam->getValue()));
  std::vector<Recipient> rcpts = config_get_recipients();
  if (to.indexOf('@') > 0) {
    if (rcpts.empty()) {
      rcpts.push_back({config_name_from_email(to), to});
    } else if (rcpts[0].email != to) {
      rcpts[0] = {config_name_from_email(to), to};  // new address, new default name
    }
  } else if (!rcpts.empty()) {
    rcpts.erase(rcpts.begin());  // field cleared: remove the first recipient
  }
  config_set_recipients(rcpts);
  config_save();
}

static void enterStaTrying() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();  // no arguments = the network saved in flash
  state = ST_STA_TRYING;
  stateSinceMs = millis();
  Serial.println("WiFi: connecting to saved network...");
}

static void enterPortal() {
  // With saved WiFi the portal closes again after a while so the board can
  // go back to trying the saved network. Without saved WiFi there's nothing
  // else to try, so it stays open until someone completes setup.
  wm.setConfigPortalTimeout(hadSavedCredentials ? WIFI_PORTAL_TIMEOUT_S : 0);
  wm.startConfigPortal("ShoppingList-Setup");  // returns at once (non-blocking)
  state = ST_PORTAL;
  stateSinceMs = millis();
  Serial.println("WiFi: setup hotspot \"ShoppingList-Setup\" open.");
}

void wifi_setup_start() {
  addPortalFields();
  wm.setConfigPortalBlocking(false);
  // Only used when someone submits new details in the portal: how long to
  // try the network they just entered before showing the page again.
  wm.setConnectTimeout(30);
  // Deliberately NOT calling wm.setAPClientCheck(true). That keeps the
  // portal alive while any device is joined to the hotspot - and a phone
  // that did setup once may auto-rejoin the open "ShoppingList-Setup"
  // network, which would pin the portal open indefinitely. The default
  // web-client check (active page use extends the timeout) is enough to
  // protect someone mid-setup.

  hadSavedCredentials = wifi_setup_has_saved_credentials();
  if (hadSavedCredentials) enterStaTrying();
  else enterPortal();
}

bool wifi_setup_poll() {
  switch (state) {
    case ST_CONNECTED_ONCE:
      // Runtime drops are handled by the ESP32's own auto-reconnect.
      return false;

    case ST_STA_TRYING:
      if (WiFi.status() == WL_CONNECTED) {
        state = ST_CONNECTED_ONCE;
        Serial.print("WiFi: connected, IP ");
        Serial.println(WiFi.localIP());
        return true;
      }
      if (millis() - stateSinceMs > WIFI_STA_TRY_MS) {
        Serial.println("WiFi: saved network not reachable.");
        enterPortal();
      }
      return false;

    case ST_PORTAL:
      if (wm.process()) {
        // Someone completed setup and the new network connected. The
        // portal has already shut down, freeing port 80 for the web app.
        savePortalFields();
        state = ST_CONNECTED_ONCE;
        Serial.print("WiFi: set up and connected, IP ");
        Serial.println(WiFi.localIP());
        return true;
      }
      if (!wm.getConfigPortalActive()) {
        // Timed out with nobody completing setup - back to retrying the
        // saved network (only reachable when there IS a saved network).
        Serial.println("WiFi: setup hotspot timed out, retrying saved network.");
        enterStaTrying();
      }
      return false;
  }
  return false;
}

void wifi_setup_reset_and_restart() {
  // Forgets the saved WiFi network ONLY. Every other setting (email,
  // recipients, Pushover, OTA password) is kept and shown pre-filled on the
  // setup page, so re-running setup after a router change doesn't mean
  // retyping everything. To wipe the board completely (e.g. before giving
  // it away), upload with Tools > Erase All Flash Before Sketch Upload.
  wm.resetSettings();
  delay(500);
  ESP.restart();
}
