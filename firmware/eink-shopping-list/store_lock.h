#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Scoped lock for the list/favorites stores. Both are touched from two
// FreeRTOS tasks - Arduino loop() (touch taps, redraws) and AsyncTCP (web
// handlers) - which can run in parallel on the S3's two cores. Every public
// store method holds one of these for its whole body.
//
// The mutex is created in the store's begin(), which setup() calls before
// the web server or touch handling start, so it always exists by the time
// a second task can reach it. A null handle (method called before begin())
// is treated as "single-threaded still" and not locked.
class StoreLock {
 public:
  explicit StoreLock(SemaphoreHandle_t m) : m_(m) {
    if (m_) xSemaphoreTake(m_, portMAX_DELAY);
  }
  ~StoreLock() {
    if (m_) xSemaphoreGive(m_);
  }
  StoreLock(const StoreLock&) = delete;
  StoreLock& operator=(const StoreLock&) = delete;

 private:
  SemaphoreHandle_t m_;
};
