#include "notify.h"
#include "app_config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static String urlEncode(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else if (c == ' ') {
      out += '+';
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

void notify_needed(const String& itemText) {
  String token = config_get_pushover_token();
  String userKey = config_get_pushover_key();
  if (token.length() == 0 || userKey.length() == 0) {
    return;  // not configured - silently skip
  }

  WiFiClientSecure client;
  // Skips certificate validation. The connection is still encrypted, so
  // nobody can passively read it - but without validation, someone able to
  // intercept traffic on your network could impersonate Pushover and
  // capture the app token + user key sent below. Accepted here as low-stakes
  // (worst case: they can send you notifications). The Gmail path in
  // email_send.cpp DOES validate, because its password is far more valuable.
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://api.pushover.net/1/messages.json")) {
    Serial.println("Pushover: failed to begin HTTPS connection");
    return;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "token=" + urlEncode(token) +
                "&user=" + urlEncode(userKey) +
                "&title=" + urlEncode("Shopping list") +
                "&message=" + urlEncode("Need " + itemText);

  int code = http.POST(body);
  Serial.printf("Pushover notify \"%s\" -> HTTP %d\n", itemText.c_str(), code);
  http.end();
}
