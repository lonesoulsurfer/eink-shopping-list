#pragma once
#include <Arduino.h>
#include <vector>

// Settings entered through the setup page (wifi_setup.h) or the web app's
// settings section - never hand-edited in code. Persisted in flash (NVS).
//
// Thread-safe: the web app can change settings from the web server's task
// while loop() reads them, so every getter/setter takes an internal lock.
// config_load() creates that lock; call it in setup() before WiFi starts.

void config_load();
void config_save();

// --- Pushover (optional "Need milk" notifications) ---
String config_get_pushover_token();
String config_get_pushover_key();
void config_set_pushover_token(const String& token);
void config_set_pushover_key(const String& key);

// --- Wireless (OTA) updates ---
// Empty or shorter than OTA_PASSWORD_MIN_LEN means OTA is OFF - the
// default, so a published build never ships with a shared password.
static const size_t OTA_PASSWORD_MIN_LEN = 8;
String config_get_ota_password();
void config_set_ota_password(const String& password);
bool config_ota_enabled();

// --- Send button: email the list ---
// Sent FROM one Gmail account (needs an app password), TO one of up to
// MAX_RECIPIENTS named people, picked on the board when there's more than
// one. The Send button only appears once a sender and at least one
// recipient are set.
static const size_t MAX_RECIPIENTS = 3;
static const size_t MAX_RECIPIENT_NAME_BYTES = 16;
static const size_t MAX_EMAIL_BYTES = 64;

struct Recipient {
  String name;   // shown on the board's "Send the list to:" buttons
  String email;
};

String config_get_email_from();
String config_get_email_app_password();
void config_set_email_sender(const String& from, const String& appPassword);
std::vector<Recipient> config_get_recipients();
void config_set_recipients(const std::vector<Recipient>& recipients);
bool config_email_enabled();

// Cleans a pasted address or app password: removes spaces/tabs and any
// CR/LF (which could inject lines into the SMTP conversation). Google shows
// app passwords as "abcd efgh ijkl mnop", so spaces must go.
String config_compact(const String& s);

// A default display name from an address: "alex.smith@example.com" -> "alex.smith".
String config_name_from_email(const String& email);
