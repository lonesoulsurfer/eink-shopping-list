#include "notify_queue.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// FreeRTOS queue of fixed-size messages: it copies plain bytes, needs no
// heap after creation, and is safe across tasks by design. (A std::vector
// guarded by a critical section would allocate memory with interrupts
// disabled, which ESP-IDF advises against.)
struct NotifyMsg {
  char text[64];
};

static const UBaseType_t QUEUE_DEPTH = 8;
static QueueHandle_t queue = nullptr;

void notify_queue_init() {
  if (!queue) queue = xQueueCreate(QUEUE_DEPTH, sizeof(NotifyMsg));
  if (!queue) Serial.println("notify_queue: xQueueCreate failed");
}

void queue_notification(const String& itemText) {
  if (!queue) return;
  NotifyMsg msg;
  strlcpy(msg.text, itemText.c_str(), sizeof(msg.text));
  if (xQueueSend(queue, &msg, 0) != pdTRUE) {
    Serial.println("notify_queue: full, dropping notification");
  }
}

bool consume_pending_notification(String& outText) {
  if (!queue) return false;
  NotifyMsg msg;
  if (xQueueReceive(queue, &msg, 0) != pdTRUE) return false;
  outText = msg.text;
  return true;
}
