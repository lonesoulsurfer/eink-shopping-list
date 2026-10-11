#include "send_request.h"
#include "store_lock.h"

static SemaphoreHandle_t mutex_ = nullptr;
static uint32_t nextId = 1;
static uint32_t currentId = 0;
static SendState currentState = SEND_STATE_IDLE;
static String currentName;
static size_t pendingIndex = 0;

void send_request_init() {
  if (!mutex_) mutex_ = xSemaphoreCreateMutex();
}

uint32_t send_request_submit(size_t recipientIndex, const String& name) {
  StoreLock lock(mutex_);
  if (currentState == SEND_STATE_QUEUED || currentState == SEND_STATE_SENDING) return 0;
  currentId = nextId++;
  currentState = SEND_STATE_QUEUED;
  currentName = name;
  pendingIndex = recipientIndex;
  return currentId;
}

void send_request_status(uint32_t& id, SendState& state, String& name) {
  StoreLock lock(mutex_);
  id = currentId;
  state = currentState;
  name = currentName;
}

bool send_request_take(size_t& recipientIndex) {
  StoreLock lock(mutex_);
  if (currentState != SEND_STATE_QUEUED) return false;
  recipientIndex = pendingIndex;
  currentState = SEND_STATE_SENDING;
  return true;
}

void send_request_set_state(SendState state) {
  StoreLock lock(mutex_);
  currentState = state;
}

const char* send_state_name(SendState state) {
  switch (state) {
    case SEND_STATE_QUEUED:   return "queued";
    case SEND_STATE_SENDING:  return "sending";
    case SEND_STATE_SENT:     return "sent";
    case SEND_STATE_FAILED:   return "failed";
    case SEND_STATE_EMPTY:    return "empty";
    case SEND_STATE_NO_WIFI:  return "nowifi";
    case SEND_STATE_COOLDOWN: return "cooldown";
    default:                  return "idle";
  }
}
