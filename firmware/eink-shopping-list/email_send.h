#pragma once
#include <Arduino.h>

// Sends a plain-text email through Gmail's SMTP server (smtp.gmail.com:465,
// TLS) using the account + app password entered in the setup portal.
//
// Gmail needs an "app password" (16 characters, generated in the Google
// account's security settings; requires 2-Step Verification) - the normal
// account password is rejected.
//
// Google's certificate IS verified (see google_roots.h), unlike the
// Pushover call - the app password can read the whole mailbox, so it must
// never be sent to an impostor server.
//
// Blocking: typically 2-5s. Call from loop() only, never from a web handler.
// Returns true on success; on failure fills err with a short reason for
// Serial (the SMTP reply line, or "connect failed" / "timeout").
bool email_send(const String& to, const String& subject, const String& body, String& err);
