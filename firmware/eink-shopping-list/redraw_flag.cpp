#include "redraw_flag.h"

static volatile bool g_redraw_pending = false;

void request_redraw() {
  g_redraw_pending = true;
}

bool consume_redraw_request() {
  if (g_redraw_pending) {
    g_redraw_pending = false;
    return true;
  }
  return false;
}
