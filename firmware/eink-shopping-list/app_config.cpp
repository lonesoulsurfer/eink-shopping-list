#include "app_config.h"
#include "store_lock.h"
#include "store_limits.h"   // utf8Truncate
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static const char* NVS_NAMESPACE = "shopcfg";
static SemaphoreHandle_t cfgMutex = nullptr;

static String pushoverToken;
static String pushoverKey;
static String otaPassword;
static String emailFrom;
static String emailAppPass;
static std::vector<Recipient> recipients;

String config_compact(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') out += c;
  }
  return out;
}

String config_name_from_email(const String& email) {
  int at = email.indexOf('@');
  String name = at > 0 ? email.substring(0, at) : email;
  return utf8Truncate(name, MAX_RECIPIENT_NAME_BYTES);
}

String config_clean_phone(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') out += c;
    else if (c == '+' && out.length() == 0) out += c;
    else if (c == ' ' || c == '-' || c == '.' || c == '(' || c == ')') continue;
    else return "";  // letters or other junk: not a phone number
  }
  size_t digits = out.length() - (out.startsWith("+") ? 1 : 0);
  if (digits < 6 || digits > 15) return "";
  return out;
}

static String recipientsToJson() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& r : recipients) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = r.name;
    o["email"] = r.email;
    if (r.phone.length() > 0) o["phone"] = r.phone;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

void config_load() {
  if (!cfgMutex) cfgMutex = xSemaphoreCreateMutex();
  StoreLock lock(cfgMutex);
  prefs.begin(NVS_NAMESPACE, true);
  pushoverToken = prefs.getString("pushover_token", "");
  pushoverKey = prefs.getString("pushover_key", "");
  otaPassword = prefs.getString("ota_pass", "");
  emailFrom = prefs.getString("mail_from", "");
  emailAppPass = prefs.getString("mail_pass", "");
  String rcptJson = prefs.getString("mail_rcpts", "");
  prefs.end();

  recipients.clear();
  JsonDocument doc;
  if (rcptJson.length() > 0 && !deserializeJson(doc, rcptJson)) {
    for (JsonObject o : doc.as<JsonArray>()) {
      if (recipients.size() >= MAX_RECIPIENTS) break;
      Recipient r;
      r.name = o["name"].as<String>();
      r.email = o["email"] | "";
      r.phone = config_clean_phone(o["phone"] | "");
      if (r.email.indexOf('@') <= 0) r.email = "";
      if (r.email.length() > 0 || r.phone.length() > 0) recipients.push_back(r);
    }
  }
}

void config_save() {
  StoreLock lock(cfgMutex);
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("pushover_token", pushoverToken);
  prefs.putString("pushover_key", pushoverKey);
  prefs.putString("ota_pass", otaPassword);
  prefs.putString("mail_from", emailFrom);
  prefs.putString("mail_pass", emailAppPass);
  prefs.putString("mail_rcpts", recipientsToJson());
  prefs.end();
}

String config_get_pushover_token() { StoreLock l(cfgMutex); return pushoverToken; }
String config_get_pushover_key() { StoreLock l(cfgMutex); return pushoverKey; }
void config_set_pushover_token(const String& token) { StoreLock l(cfgMutex); pushoverToken = token; }
void config_set_pushover_key(const String& key) { StoreLock l(cfgMutex); pushoverKey = key; }

String config_get_ota_password() { StoreLock l(cfgMutex); return otaPassword; }
void config_set_ota_password(const String& password) { StoreLock l(cfgMutex); otaPassword = password; }
bool config_ota_enabled() { StoreLock l(cfgMutex); return otaPassword.length() >= OTA_PASSWORD_MIN_LEN; }

String config_get_email_from() { StoreLock l(cfgMutex); return emailFrom; }
String config_get_email_app_password() { StoreLock l(cfgMutex); return emailAppPass; }

void config_set_email_sender(const String& from, const String& appPassword) {
  StoreLock l(cfgMutex);
  emailFrom = utf8Truncate(config_compact(from), MAX_EMAIL_BYTES);
  emailAppPass = config_compact(appPassword);
}

std::vector<Recipient> config_get_recipients() { StoreLock l(cfgMutex); return recipients; }

std::vector<Recipient> config_get_email_recipients() {
  StoreLock l(cfgMutex);
  std::vector<Recipient> out;
  for (const auto& r : recipients)
    if (r.email.length() > 0) out.push_back(r);
  return out;
}

void config_set_recipients(const std::vector<Recipient>& list) {
  StoreLock l(cfgMutex);
  recipients.clear();
  for (const auto& in : list) {
    if (recipients.size() >= MAX_RECIPIENTS) break;
    Recipient r;
    r.email = utf8Truncate(config_compact(in.email), MAX_EMAIL_BYTES);
    if (r.email.indexOf('@') <= 0) r.email = "";
    r.phone = utf8Truncate(config_clean_phone(in.phone), MAX_PHONE_BYTES);
    if (r.email.length() == 0 && r.phone.length() == 0) continue;
    String name = in.name;
    name.trim();
    r.name = name.length() > 0 ? utf8Truncate(name, MAX_RECIPIENT_NAME_BYTES)
           : r.email.length() > 0 ? config_name_from_email(r.email)
           : r.phone;
    recipients.push_back(r);
  }
}

bool config_email_enabled() {
  StoreLock l(cfgMutex);
  if (emailFrom.indexOf('@') <= 0 || emailAppPass.length() == 0) return false;
  for (const auto& r : recipients)
    if (r.email.length() > 0) return true;
  return false;
}
