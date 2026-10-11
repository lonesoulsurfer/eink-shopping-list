#include "email_send.h"
#include "app_config.h"
#include "google_roots.h"
#include <WiFiClientSecure.h>
#include <base64.h>

static const char* SMTP_HOST = "smtp.gmail.com";
static const uint16_t SMTP_PORT = 465;          // implicit TLS
static const unsigned long REPLY_TIMEOUT_MS = 10000;

// Reads one CRLF-terminated line with a hard deadline (Stream's own
// timeout handling differs between WiFiClient versions, so don't rely on it).
static bool readLine(WiFiClientSecure& c, String& line, unsigned long deadline) {
  line = "";
  while ((long)(millis() - deadline) < 0) {
    while (c.available()) {
      char ch = c.read();
      if (ch == '\n') return true;
      if (ch != '\r') line += ch;
    }
    if (!c.connected()) return false;
    delay(5);
  }
  return false;
}

// SMTP replies can span several lines ("250-..." continues, "250 ..." ends).
static bool expect(WiFiClientSecure& c, int code, String& err) {
  unsigned long deadline = millis() + REPLY_TIMEOUT_MS;
  String line;
  while (readLine(c, line, deadline)) {
    if (line.length() < 3) continue;
    if (line.length() > 3 && line[3] == '-') continue;  // more lines follow
    if (line.substring(0, 3).toInt() == code) return true;
    err = line;  // e.g. "535-5.7.8 Username and Password not accepted"
    return false;
  }
  err = "timeout waiting for " + String(code);
  return false;
}

static bool cmd(WiFiClientSecure& c, const String& line, int code, String& err) {
  c.print(line);
  c.print("\r\n");
  return expect(c, code, err);
}

// Makes free text safe to put in the message: no bare CR/LF (they could end
// headers or the DATA block early), and "dot-stuffing" so a line starting
// with "." isn't read as end-of-message.
static String smtpSafeBody(const String& body) {
  String out;
  out.reserve(body.length() + 32);
  bool lineStart = true;
  for (size_t i = 0; i < body.length(); i++) {
    char ch = body[i];
    if (ch == '\r') continue;
    if (ch == '\n') { out += "\r\n"; lineStart = true; continue; }
    if (lineStart && ch == '.') out += '.';
    out += ch;
    lineStart = false;
  }
  return out;
}

bool email_send(const String& to, const String& subject, const String& body, String& err) {
  String from = config_get_email_from();
  String pass = config_get_email_app_password();
  if (from.indexOf('@') <= 0 || pass.length() == 0 || to.indexOf('@') <= 0) {
    err = "email not configured";
    return false;
  }

  WiFiClientSecure c;
  c.setCACert(GOOGLE_ROOT_CAS);
  if (!c.connect(SMTP_HOST, SMTP_PORT)) {
    err = "connect/TLS failed (no internet, or certificate check failed)";
    return false;
  }

  bool ok =
      expect(c, 220, err) &&
      cmd(c, "EHLO shoppinglist", 250, err) &&
      cmd(c, "AUTH LOGIN", 334, err) &&
      cmd(c, base64::encode(from), 334, err) &&
      cmd(c, base64::encode(pass), 235, err) &&
      cmd(c, "MAIL FROM:<" + from + ">", 250, err) &&
      cmd(c, "RCPT TO:<" + to + ">", 250, err) &&
      cmd(c, "DATA", 354, err);

  if (ok) {
    c.print("From: Shopping List <" + from + ">\r\n");
    c.print("To: <" + to + ">\r\n");
    c.print("Subject: " + subject + "\r\n");
    c.print("MIME-Version: 1.0\r\n");
    c.print("Content-Type: text/plain; charset=UTF-8\r\n");
    c.print("Content-Transfer-Encoding: 8bit\r\n");
    c.print("\r\n");
    c.print(smtpSafeBody(body));
    ok = cmd(c, "\r\n.", 250, err);  // CRLF . CRLF ends the message
  }

  c.print("QUIT\r\n");  // best effort; reply not needed
  c.stop();
  return ok;
}
