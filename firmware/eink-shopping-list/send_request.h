#pragma once
#include <Arduino.h>

// Lets the phone web app ask the board to email the list.
//
// Sending takes several seconds (talking to Gmail), so it can't happen
// inside a web request: that would block the web server for everyone. So
// the web handler only files a request here, loop() picks it up and does
// the actual send (exactly like the board's own Send button), and the
// phone polls for the result. Safe to call from both tasks.

enum SendState {
  SEND_STATE_IDLE,      // nothing requested yet
  SEND_STATE_QUEUED,    // phone asked, loop() hasn't started it yet
  SEND_STATE_SENDING,   // talking to Gmail
  SEND_STATE_SENT,      // done
  SEND_STATE_FAILED,    // Gmail refused or couldn't be reached
  SEND_STATE_EMPTY,     // nothing on the list to send
  SEND_STATE_NO_WIFI,   // board isn't connected
  SEND_STATE_COOLDOWN,  // same list sent to the same person under 2 min ago
};

void send_request_init();  // call once from setup()

// Web side. Returns a request id (1, 2, 3...), or 0 if a send is already
// queued or in progress.
uint32_t send_request_submit(size_t recipientIndex, const String& name);

// Web side: the latest request's id, state and recipient name.
void send_request_status(uint32_t& id, SendState& state, String& name);

// loop() side: takes the queued request, if any.
bool send_request_take(size_t& recipientIndex);

// loop() side: records progress / the result of the current request.
void send_request_set_state(SendState state);

const char* send_state_name(SendState state);  // "sent", "failed", ...
