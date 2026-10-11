// Shopping list e-ink board - LILYGO T5 4.7" E-Paper, ESP32-S3 touch version
// ("T5-4.7-S3"). Not the older non-S3 T5 4.7, which needs different code.
//
// ARDUINO IDE SETUP
//   Board package: esp32 by Espressif, v2.0.5 to 2.0.15 (tested on 2.0.15).
//   Do NOT update to 3.x - the WiFi/TLS code here targets the 2.0.x API.
//
//   Tools settings (from LilyGo's own S3 guide; the starred ones matter):
//     Board: ESP32S3 Dev Module *            USB CDC On Boot: Enabled
//     Flash Size: 16MB (128Mb) *             USB Mode: CDC and JTAG *
//     Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS) *
//     PSRAM: OPI PSRAM *    (without it the display can't allocate its
//                            framebuffer and the board won't draw anything)
//     Upload Mode: UART0 / Hardware CDC *    Upload Speed: 921600
//     Erase All Flash Before Sketch Upload: Disabled (Enabled wipes your
//                                           saved WiFi, list and settings)
//   After uploading, press RESET once - the S3 can stay in its bootloader.
//
// LIBRARIES
//   - LilyGo-EPD47 - the esp32s3 BRANCH, not master
//     (github.com/Xinyuan-LilyGO/LilyGo-EPD47/tree/esp32s3, Add .ZIP Library).
//     Also provides the fonts (firasans.h, roboto12.h).
//   - ESPAsyncWebServer + AsyncTCP (tested with the ESP32Async forks)
//   - ArduinoJson v7
//   - WiFiManager by tzapu (tested with 2.0.17)
//   - SensorLib (Library Manager) - GT911 touch driver. Pins for this board:
//     SDA=18, SCL=17, TOUCH_INT=47 (from LilyGo's utilities.h).
//   QR codes use qrcode_ricmoo.h/.c in this folder, not an installed library:
//   the ESP32 core has its own, different "qrcode.h" that clashes by name
//   with Richard Moore's QRCode library, so it's renamed and kept local.
//
// SETUP - no code editing needed
//   First boot opens a WiFi hotspot, "ShoppingList-Setup". Join it from a
//   phone (or scan the QR on the screen) and a setup page opens. Enter your
//   WiFi, plus optional extras: Pushover notifications, a Gmail account for
//   the board's Send button, and a password for wireless (OTA) updates.
//   Re-run any time from the web app. See wifi_setup.h for how it behaves
//   when the router is down or replaced.

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "device_settings.h"
#include "app_config.h"
#include "wifi_setup.h"
#include "list_store.h"
#include "favorites.h"
#include "display_render.h"
#include "web_server.h"
#include "touch_input.h"
#include "redraw_flag.h"
#include "notify_queue.h"
#include "notify.h"
#include "email_send.h"
#include "send_request.h"

ListStore store;
FavoritesStore favorites;

// Web edits wait a few seconds before redrawing (several people may be
// adding items from phones at once, and each refresh takes ~1.5s). Taps on
// the glass redraw immediately, so a tap feels as responsive as the Join
// button. Trade-off: touch isn't read during the
// ~1.5s refresh, so a second tap made mid-refresh is ignored and needs
// repeating once the screen has updated.
static const unsigned long WEB_REDRAW_DELAY_MS = 3000;
static const unsigned long TOUCH_REDRAW_DELAY_MS = 0;
static unsigned long redrawDueMs = 0;
static bool redrawPending = false;

// Dedicated QR screen: opened by Join, closed by a tap anywhere or by the
// timeout. 30s rather than 15s - getting a phone out, opening the camera
// and focusing can easily take longer than 15s, and tap-to-close means a
// long timeout costs nothing. Change here if you disagree.
static const unsigned long QR_SCREEN_TIMEOUT_MS = 30000;
static bool qrScreenActive = false;
static unsigned long qrShownAtMs = 0;

// --- "Send" button: email the list to a nominated person ---
// One recipient: Send emails straight away. Two or three: Send opens a
// "Send the list to:" screen with a button per name (closes itself after
// PICKER_TIMEOUT_MS). Recipients are set in the web app's settings.
//
// Status shown next to the title. "Sent to <name>" is only shown while the
// list still matches what was emailed - once anyone changes it, the line
// goes away, so nobody assumes an out-of-date email covered the new items.
enum SendStatus { SEND_NONE, SEND_OK, SEND_FAILED };
static SendStatus sendStatus = SEND_NONE;
static String lastSentBody;
static String lastSentTo;      // recipient email, for the cooldown
static String lastSentName;    // recipient name, for the status line
static unsigned long lastSendMs = 0;
static bool hasSent = false;
static String statusBuf;       // backing store for currentStatusText()
static String sendingToName;   // non-empty only while an email is going out
static bool emptyNotice = false;  // "Nothing to send" after Send on an empty list
// Re-sending an UNCHANGED list to the SAME person is blocked for this long
// (stops a bored kid sending 30 identical emails). A changed list, or a
// different person, can always be sent.
static const unsigned long SEND_COOLDOWN_MS = 2UL * 60 * 1000;

static const unsigned long PICKER_TIMEOUT_MS = 30000;
static bool pickerActive = false;
static unsigned long pickerShownAtMs = 0;

// --- WiFi state as the display sees it ---
// setupScreenShowing: first-time setup instructions are on the panel (no
// saved WiFi). Nothing may redraw over them until setup completes.
// noWifiShown: the "No WiFi" status is on the panel. Shown only after WiFi
// has been down for NO_WIFI_STATUS_DELAY_MS, so a normal boot (connects in
// a few seconds) never costs an extra refresh.
static bool setupScreenShowing = false;
static bool noWifiShown = false;
static const unsigned long NO_WIFI_STATUS_DELAY_MS = 60000;

static volatile bool otaInProgress = false;
static bool otaEnabled = false;

static void scheduleRedraw(unsigned long delayMs) {
  redrawDueMs = millis() + delayMs;
  redrawPending = true;
}

void setupOTA() {
  // Off unless a password was set in the setup portal - a published build
  // must never accept firmware under a shared default password.
  if (!config_ota_enabled()) {
    Serial.println("OTA disabled - set a wireless update password (8+ chars) in the");
    Serial.println("setup portal to enable it (web app: Re-run setup).");
    return;
  }
  otaEnabled = true;
  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  ArduinoOTA.setPassword(config_get_ota_password().c_str());

  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    Serial.println("OTA update starting...");
    // Put the panel to sleep for the duration - a firmware upload can
    // take 10-30s over WiFi and we don't want a mid-transfer redraw or
    // touch event interrupting it (or fighting it for CPU/timing).
    display_sleep();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update done, rebooting...");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error [%u]\n", error);
    otaInProgress = false;
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready - in Arduino IDE, Tools > Port should now also");
  Serial.printf("show a network port for \"%s\" once your computer is on\n", MDNS_HOSTNAME);
  Serial.println("the same WiFi. Select it instead of the USB port to flash wirelessly.");
}

// Everything that needs the network, started once, the first time WiFi
// connects after boot - which may be seconds after power-up, or much later
// if the router was down. Until then the board runs fully offline: list,
// touch and flash storage all work.
void startNetworkServices() {
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.printf("mDNS up: http://%s.local\n", MDNS_HOSTNAME);
  }
  web_server_begin(&store, &favorites);
  setupOTA();
}

// Plain-text list for the email. Built from locked copies of both stores.
String buildListText(const std::vector<FavoriteItem>& favs,
                     const std::vector<ListItem>& items, int& countOut) {
  String body;
  countOut = 0;
  for (const auto& f : favs) {
    if (!f.needed) continue;
    body += "- " + f.text + "\n";
    countOut++;
  }
  for (const auto& i : items) {
    if (i.done) continue;
    body += "- " + i.text;
    if (i.addedBy.length() > 0) body += "  (" + i.addedBy + ")";
    body += "\n";
    countOut++;
  }
  return body;
}

const char* currentStatusText(const String& currentBody) {
  if (sendingToName.length() > 0) {
    statusBuf = "Sending to " + sendingToName + "...";
    return statusBuf.c_str();
  }
  if (noWifiShown) return "No WiFi";
  if (emptyNotice && currentBody.length() == 0) return "Nothing to send";
  if (sendStatus == SEND_FAILED) return "Send failed";
  if (sendStatus == SEND_OK && currentBody == lastSentBody) {
    statusBuf = "Sent to " + lastSentName;  // display fits it to the space
    return statusBuf.c_str();
  }
  return nullptr;
}

void redrawNow() {
  auto favs = favorites.getItems();
  auto items = store.getItems();
  int count;
  String body = buildListText(favs, items, count);
  display_render(favs, items, config_email_enabled(), currentStatusText(body));
}

// Emails the list to recipient `index`. Blocking (~2-5s of SMTP, then one
// ~1.5s refresh); taps during it are lost, which is fine - the screen is
// about to change anyway. Used by the board's Send button, the picker and
// the phone web app. With mustRedraw (an overlay like the picker or Join
// QR was on screen), it ALWAYS redraws the list at the end, even if nothing
// was sent, so the overlay never stays up. Returns what happened, for the
// phone app's status message.
SendState sendListByEmail(size_t index, bool mustRedraw) {
  const bool fromPicker = mustRedraw;
  auto favs = favorites.getItems();
  auto items = store.getItems();
  int count;
  String body = buildListText(favs, items, count);
  std::vector<Recipient> rcpts = config_get_recipients();

  if (index >= rcpts.size()) {  // settings changed under us
    if (fromPicker) redrawNow();
    return SEND_STATE_FAILED;
  }
  const Recipient& to = rcpts[index];

  if (count == 0) {
    // Say so on screen - otherwise the tap looks like it did nothing.
    Serial.println("Send: list is empty, nothing to send.");
    if (!emptyNotice || fromPicker) {
      emptyNotice = true;
      redrawPending = false;
      redrawNow();
    }
    return SEND_STATE_EMPTY;
  }
  emptyNotice = false;
  if (!wifi_setup_connected()) {
    Serial.println("Send: no WiFi.");
    // Show "No WiFi" now rather than waiting for the usual delay. Skip the
    // refresh only if it's already on screen AND the picker isn't up.
    if (!noWifiShown || fromPicker) {
      noWifiShown = true;
      redrawPending = false;
      redrawNow();
    }
    return SEND_STATE_NO_WIFI;
  }
  if (hasSent && body == lastSentBody && to.email == lastSentTo &&
      millis() - lastSendMs < SEND_COOLDOWN_MS) {
    Serial.println("Send: identical list sent to them under 2 min ago - ignored.");
    if (fromPicker) redrawNow();
    return SEND_STATE_COOLDOWN;
  }

  // Feedback FIRST, like Join: the screen changes the moment Send is tapped.
  // Without this, the 2-5s Gmail conversation happened before any visible
  // change, so the button felt like it needed holding down.
  sendingToName = to.name;
  redrawPending = false;
  redrawNow();

  String subject = "Shopping list (" + String(count) + (count == 1 ? " item)" : " items)");
  String fullBody = "From the kitchen board:\n\n" + body;
  String err;
  SendState result;
  Serial.printf("Send: emailing list to %s...\n", to.name.c_str());
  if (email_send(to.email, subject, fullBody, err)) {
    result = SEND_STATE_SENT;
    Serial.println("Send: OK");
    sendStatus = SEND_OK;
    lastSentBody = body;
    lastSentTo = to.email;
    lastSentName = to.name;
    lastSendMs = millis();
    hasSent = true;
  } else {
    Serial.printf("Send: FAILED - %s\n", err.c_str());
    sendStatus = SEND_FAILED;
    result = SEND_STATE_FAILED;
  }
  sendingToName = "";
  redrawPending = false;  // this redraw includes any pending changes
  redrawNow();
  return result;
}

// Send button: straight to the only recipient, or the picker for several.
void onSendButton() {
  std::vector<Recipient> rcpts = config_get_recipients();
  if (rcpts.empty()) return;  // button shouldn't be showing
  if (rcpts.size() == 1) {
    sendListByEmail(0, false);
    return;
  }
  std::vector<String> names;
  for (const auto& r : rcpts) names.push_back(r.name);
  pickerActive = true;
  pickerShownAtMs = millis();
  display_render_send_picker(names, PICKER_TIMEOUT_MS / 1000);
}

void closePicker() {
  pickerActive = false;
  redrawPending = false;  // the redraw below includes any pending changes
  redrawNow();
}

// The Join QR uses the board's IP address rather than shoppinglist.local:
// many phones (a lot of Android ones in particular) can't look up .local
// names, so the page just stays blank. The IP is read fresh each time, so
// the code stays right even if the router hands out a new address.
// Falls back to the .local name if WiFi isn't connected yet.
static String joinUrl() {
  if (wifi_setup_connected()) {
    return "http://" + WiFi.localIP().toString() + "/";
  }
  return String("http://") + MDNS_HOSTNAME + ".local/";
}

static void openQrScreen() {
  qrScreenActive = true;
  qrShownAtMs = millis();
  display_render_qr_screen(joinUrl(), QR_SCREEN_TIMEOUT_MS / 1000);
}

static void closeQrScreen() {
  qrScreenActive = false;
  redrawPending = false;  // the redraw below picks up any changes made
                          // from phones while the QR was showing
  redrawNow();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Order matters: the stores' mutexes and the notify queue must exist
  // before touch handling or the web server can reach them from another
  // task. Both of those start further down.
  notify_queue_init();
  send_request_init();
  store.begin();
  favorites.begin();
  display_init();
  touch_init();
  config_load();

  // Saved WiFi: show the list straight away - it's fully usable offline, and
  // WiFi connects in the background from loop(). No saved WiFi: show the
  // setup instructions until someone completes setup.
  if (wifi_setup_has_saved_credentials()) {
    redrawNow();
  } else {
    display_render_setup_screen();
    setupScreenShowing = true;
  }
  wifi_setup_start();  // returns immediately

  Serial.printf("Shopping list board, firmware %s\n", FIRMWARE_VERSION);
  Serial.println("Ready (WiFi connecting in the background).");
}

void loop() {
  // --- WiFi: connect / setup hotspot, never blocking ---
  if (wifi_setup_poll()) {
    startNetworkServices();  // first connection since boot
    if (setupScreenShowing) {
      setupScreenShowing = false;
      scheduleRedraw(0);  // replace the setup instructions with the list
    }
  }

  // "No WiFi" status line: shown after a sustained outage, cleared on
  // reconnect. Never drawn over the setup instructions.
  static unsigned long wifiDownSinceMs = 0;
  if (wifi_setup_connected()) {
    wifiDownSinceMs = 0;
    if (noWifiShown) {
      noWifiShown = false;
      scheduleRedraw(0);
    }
  } else {
    if (wifiDownSinceMs == 0) wifiDownSinceMs = millis() | 1;
    if (!noWifiShown && !setupScreenShowing &&
        millis() - wifiDownSinceMs > NO_WIFI_STATUS_DELAY_MS) {
      noWifiShown = true;
      scheduleRedraw(0);
    }
  }

  if (otaEnabled) ArduinoOTA.handle();

  // --- "Re-run setup" requested from the web app. Done here, not in
  // the web handler, and only after a 1s grace period so the "ok" response
  // reaches the phone before the board drops off the network. ---
  static unsigned long resetSeenAtMs = 0;
  if (web_server_reset_requested()) {
    if (resetSeenAtMs == 0) {
      resetSeenAtMs = millis() | 1;  // |1 so a millis() of 0 can't read as "unset"
      Serial.println("Reset requested from web app - restarting into setup in 1s.");
    } else if (millis() - resetSeenAtMs > 1000) {
      wifi_setup_reset_and_restart();  // does not return
    }
  }

  if (otaInProgress) {
    // Don't touch the display or handle taps mid-flash - just let the
    // OTA transfer finish (or fail) undisturbed.
    return;
  }

  // --- Physical touch on the e-ink glass ---
  TapPoint tap;
  if (touch_poll(tap)) {
    TouchTarget target;
    bool hit = display_hit_test(tap.x, tap.y, target);
    // Handy when checking touch on a new board: if taps log "miss" with
    // coordinates offset from where you pressed, the touch swap/mirror
    // calibration in touch_input.cpp is off, not the hit targets.
    Serial.printf("Tap (%d,%d) -> %s\n", tap.x, tap.y,
                  !hit ? "miss"
                  : target.type == TOUCH_FAVORITE ? target.id.c_str()
                  : target.type == TOUCH_JOIN_BUTTON ? "join"
                  : target.type == TOUCH_SEND_BUTTON ? "send"
                  : target.type == TOUCH_PICK_RECIPIENT ? "pick-recipient"
                  : target.type == TOUCH_CANCEL_PICKER ? "cancel-picker" : "close-qr");
    if (hit) {
      switch (target.type) {
        case TOUCH_FAVORITE: {
          bool nowNeeded = false;
          String text;
          if (favorites.toggleNeeded(target.id, nowNeeded, text) && nowNeeded) {
            queue_notification(text);
          }
          scheduleRedraw(TOUCH_REDRAW_DELAY_MS);
          break;
        }
        case TOUCH_JOIN_BUTTON:
          openQrScreen();  // immediate - one deliberate tap
          break;
        case TOUCH_CLOSE_QR:
          closeQrScreen();
          break;
        case TOUCH_SEND_BUTTON:
          onSendButton();
          break;
        case TOUCH_PICK_RECIPIENT:
          pickerActive = false;
          sendListByEmail((size_t)target.id.toInt(), true);
          break;
        case TOUCH_CANCEL_PICKER:
          closePicker();
          break;
      }
    }
  }

  if (qrScreenActive && (millis() - qrShownAtMs > QR_SCREEN_TIMEOUT_MS)) {
    closeQrScreen();
  }
  if (pickerActive && (millis() - pickerShownAtMs > PICKER_TIMEOUT_MS)) {
    closePicker();
  }

  // --- Send requested from the phone web app. Same send as the board's
  // button; if the Join QR or the picker is up, it's closed first so the
  // board shows the list with "Sending to ..." like a normal send. ---
  size_t phoneSendIndex;
  if (send_request_take(phoneSendIndex)) {
    bool overlayUp = qrScreenActive || pickerActive;
    qrScreenActive = false;
    pickerActive = false;
    Serial.println("Send: requested from the web app.");
    send_request_set_state(sendListByEmail(phoneSendIndex, overlayUp));
  }

  // --- Send any queued Pushover notifications (never from inside an
  // AsyncWebServer callback - see notify_queue.h for why). While WiFi is
  // down they stay queued (up to 8) and go out once it's back. ---
  String pendingNotify;
  if (wifi_setup_connected() && consume_pending_notification(pendingNotify)) {
    notify_needed(pendingNotify);
  }

  // --- Debounced redraw. Web edits arrive via request_redraw(); touch
  // edits call scheduleRedraw() directly above. Held off entirely while
  // the QR screen is up so a phone edit doesn't yank it away mid-scan;
  // closeQrScreen() redraws with the latest state instead. ---
  if (consume_redraw_request()) {
    scheduleRedraw(WEB_REDRAW_DELAY_MS);
  }
  if (redrawPending && !qrScreenActive && !pickerActive && !setupScreenShowing &&
      (long)(millis() - redrawDueMs) >= 0) {  // rollover-safe
    redrawPending = false;
    redrawNow();
  }

  delay(50);
}
