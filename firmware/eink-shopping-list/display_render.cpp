#include "display_render.h"
#include "qr_render.h"
#include "epd_driver.h"
#include "device_settings.h"
// Fonts ship in the LilyGo-EPD47 library's src folder (esp32s3 branch) -
// no copies needed in the sketch folder.
#include "firasans.h"   // main text
#include "roboto12.h"   // smaller section labels

static uint8_t* framebuffer = nullptr;
static std::vector<TouchTarget> g_touch_targets;

// LILYGO's own docs warn this panel ghosts / can take permanent damage
// under heavy partial refresh. We do a full clear+redraw every call, and
// the caller (main.ino) debounces rapid-fire edits so we're not hammering
// the panel on every keystroke or tap.
static const int MARGIN_X = 24;
static const int LINE_HEIGHT = 40;
static const int TITLE_Y = 60;
static const int CHECKBOX_SIZE = 26;
// QR screen: 33 modules * 10px = 330px (~36mm on this panel). The old
// 3px overlay was ~10mm across - too small for many phone cameras.
static const int QR_SCREEN_MODULE_SIZE = 10;
static const int JOIN_TOUCH_PAD = 10;   // extra touch slop around Join only
static const int JOIN_BTN_W = 90;
static const int JOIN_BTN_H = 34;
static const int SEND_BTN_W = 112;  // "Send" measures 91px in FiraSans
static const int BTN_GAP = 16;
static const int NUM_COLS = 3;

// "Also need" rows use the smaller Roboto12 (same size as the section
// labels), so twice as many fit: 18 visible with 12 everyday items, vs 9
// in FiraSans. Change ADHOC_FONT to &FiraSans (and ADHOC_LINE_HEIGHT to
// 40, ADHOC_ASCENDER to 39) for bigger text and ~12 visible items.
#define ADHOC_FONT ((GFXfont*)&Roboto12)
static const int ADHOC_LINE_HEIGHT = 30;  // Roboto12 advance_y is 29
static const int ADHOC_ASCENDER = 24;     // Roboto12 ascender
static const int ADHOC_DESCENDER = 7;     // Roboto12 descender (magnitude)

// 912px usable width / 3 columns = 304px each. Long item/added-by names
// (roughly >18 chars at this font size) may run close to or past the next
// column - not measured precisely here since exact glyph widths depend on
// the FiraSans metrics baked into the library. Worth eyeballing once it's
// on real hardware with your actual household's item names.
// --- Text measuring -------------------------------------------------------
// All widths come from the library's own get_text_bounds, so layout checks
// match what's drawn. Fixed strings on each screen were also measured
// against the real font tables at design time; anything a user types
// (item names, "added by" names) is fitted at runtime with fitText().
static int textWidth(const GFXfont* font, const char* s) {
  int32_t x = 0, y = 0, x1, y1, w, h;
  get_text_bounds(font, s, &x, &y, &x1, &y1, &w, &h, NULL);
  return w;
}

// Returns text unchanged if it fits in maxW pixels, otherwise shortened
// with "..." so it never runs into the next column or off the panel.
static String fitText(const GFXfont* font, const String& text, int maxW) {
  if (textWidth(font, text.c_str()) <= maxW) return text;
  String t = text;
  while (t.length() > 0) {
    t.remove(t.length() - 1);
    // don't leave half of a multi-byte UTF-8 character behind
    while (t.length() > 0 && ((uint8_t)t[t.length() - 1] & 0xC0) == 0x80) t.remove(t.length() - 1);
    String candidate = t;
    candidate.trim();
    candidate += "...";
    if (textWidth(font, candidate.c_str()) <= maxW) return candidate;
  }
  return "...";
}

// Draws a label horizontally centred in a button box.
static void drawButtonLabel(int boxX, int boxW, int baselineY, const char* label) {
  int32_t lx = boxX + (boxW - textWidth((GFXfont*)&FiraSans, label)) / 2;
  int32_t ly = baselineY;
  writeln((GFXfont*)&FiraSans, label, &lx, &ly, framebuffer);
}

static int columnWidth() {
  return (EPD_WIDTH - 2 * MARGIN_X) / NUM_COLS;
}

static int columnX(int col) {
  return MARGIN_X + col * columnWidth();
}

// Sends the finished framebuffer to the panel, rotated 180 degrees first if
// SCREEN_ROTATE_180 is set. Everything is drawn in normal coordinates and
// flipped only here, so no layout code needs to know about rotation.
// Each byte holds two 4-bit pixels, so a 180-degree turn is: reverse the
// byte order, then swap the two pixels within each byte. The framebuffer
// is cleared at the start of every draw, so flipping it in place is safe.
static void pushFrame() {
#if SCREEN_ROTATE_180
  const size_t n = EPD_WIDTH * EPD_HEIGHT / 2;
  for (size_t i = 0, j = n - 1; i < j; i++, j--) {
    uint8_t a = framebuffer[i], b = framebuffer[j];
    framebuffer[i] = (uint8_t)((b << 4) | (b >> 4));
    framebuffer[j] = (uint8_t)((a << 4) | (a >> 4));
  }
  // n is even, so there's no middle byte left unswapped.
#endif
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

void display_init() {
  epd_init();
  framebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}

void display_render_setup_screen() {
  if (!framebuffer) return;
  g_touch_targets.clear();

  epd_poweron();
  epd_clear();
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  int32_t cursor_x = MARGIN_X;
  int32_t cursor_y = TITLE_Y;
  writeln((GFXfont*)&FiraSans, "Setup needed", &cursor_x, &cursor_y, framebuffer);
  epd_draw_hline(MARGIN_X, TITLE_Y + 14, EPD_WIDTH - 2 * MARGIN_X, 0x00, framebuffer);

  int contentY = TITLE_Y + 14 + 36;

  // --- Right column: scan-to-join WiFi QR code ---
  // Standard "WiFi QR" format most phone cameras recognize natively -
  // scanning it joins the "ShoppingList-Setup" network directly, no
  // manual searching or typing. T:nopass because the setup hotspot is
  // deliberately open (no password) so it's reachable without friction;
  // if you ever add a password to it, this string needs P:<password>
  // added too, and a longer QR version to fit it.
  // No text label above the code: there's no room for one in this column,
  // and step 1 of the instructions already says "Scan the QR code".
  // 5px modules = 165px (~18mm), big enough for phone cameras, and still
  // clear of the instruction text, which ends around x=740.
  int qrModuleSize = 5;
  int qrPixels = QR_MODULES * qrModuleSize;
  int qrX = EPD_WIDTH - MARGIN_X - qrPixels;
  int qrY = contentY - 24;  // top-align with the first instruction line
  draw_qr_code(framebuffer, "WIFI:T:nopass;S:ShoppingList-Setup;;", qrX, qrY, qrModuleSize);

  // --- Left column: step-by-step instructions, for anyone whose camera
  //     doesn't support WiFi QR codes or who'd rather just read steps ---
  const char* lines[] = {
    "1. Scan the QR code, or manually join:",
    "   ShoppingList-Setup",
    "",
    "2. A setup page opens automatically",
    "   (or browse to 192.168.4.1)",
    "",
    "3. Enter your WiFi + optional Pushover",
    "   details (see pushover.net)",
  };
  int y = contentY;
  for (const char* line : lines) {
    cursor_x = MARGIN_X;
    cursor_y = y;
    writeln((GFXfont*)&FiraSans, line, &cursor_x, &cursor_y, framebuffer);
    y += LINE_HEIGHT;
  }

  pushFrame();
  epd_poweroff();
}

static void drawCheckboxRow(int x, int y, bool checked, const String& label) {
  epd_draw_rect(x, y, CHECKBOX_SIZE, CHECKBOX_SIZE, 0x00, framebuffer);
  if (checked) {
    epd_fill_rect(x + 5, y + 5, CHECKBOX_SIZE - 10, CHECKBOX_SIZE - 10, 0x00, framebuffer);
  }
  int32_t cursor_x = x + CHECKBOX_SIZE + 14;
  int32_t cursor_y = y + CHECKBOX_SIZE - 6;
  // Room for the name: the rest of the column, minus a small gutter.
  int maxW = columnWidth() - 8 - (CHECKBOX_SIZE + 14);
  String fitted = fitText((GFXfont*)&FiraSans, label, maxW);
  writeln((GFXfont*)&FiraSans, fitted.c_str(), &cursor_x, &cursor_y, framebuffer);
}

void display_render_qr_screen(const String& joinUrl, int timeoutSec) {
  if (!framebuffer) return;
  g_touch_targets.clear();

  epd_poweron();
  epd_clear();
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  int32_t cursor_x = MARGIN_X;
  int32_t cursor_y = TITLE_Y;
  writeln((GFXfont*)&FiraSans, "Open the list on your phone", &cursor_x, &cursor_y, framebuffer);
  epd_draw_hline(MARGIN_X, TITLE_Y + 14, EPD_WIDTH - 2 * MARGIN_X, 0x00, framebuffer);

  int qrPixels = QR_MODULES * QR_SCREEN_MODULE_SIZE;
  int qrX = MARGIN_X + 16;   // left edge; background is white, so the
  int qrY = TITLE_Y + 40;    // quiet zone around the code is automatic
  draw_qr_code(framebuffer, joinUrl.c_str(), qrX, qrY, QR_SCREEN_MODULE_SIZE);

  // The address in readable form, for phones that won't scan the code:
  // "http://192.168.0.162/" -> "192.168.0.162". Longest possible line,
  // "Or type: 192.168.100.200", measures 443px of the 526px available.
  String shortUrl = joinUrl;
  shortUrl.replace("http://", "");
  if (shortUrl.endsWith("/")) shortUrl.remove(shortUrl.length() - 1);
  String typeLine = "Or type: " + shortUrl;

  char timeoutLine[48];
  snprintf(timeoutLine, sizeof(timeoutLine), "Closes by itself in %d s", timeoutSec);
  const char* lines[] = {
    "1. Phone on home WiFi",         // measured widths, FiraSans:
    "2. Point camera at the code",   // widest line 512px, room 526px
    "3. Tap the link that pops up",
    typeLine.c_str(),
    "Tap anywhere to close",
    timeoutLine,
  };
  int textX = qrX + qrPixels + 40;
  int y = qrY + 40;
  for (const char* line : lines) {
    cursor_x = textX;
    cursor_y = y;
    writeln((GFXfont*)&FiraSans, line, &cursor_x, &cursor_y, framebuffer);
    y += LINE_HEIGHT;
  }

  TouchTarget closeTarget;
  closeTarget.type = TOUCH_CLOSE_QR;
  closeTarget.x = 0;
  closeTarget.y = 0;
  closeTarget.w = EPD_WIDTH;
  closeTarget.h = EPD_HEIGHT;
  g_touch_targets.push_back(closeTarget);

  pushFrame();
  epd_poweroff();
}

void display_render(const std::vector<FavoriteItem>& favorites,
                     const std::vector<ListItem>& items,
                     bool showSendButton,
                     const char* statusText) {
  if (!framebuffer) return;
  g_touch_targets.clear();

  epd_poweron();
  epd_clear();
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  // --- Title ---
  int32_t cursor_x = MARGIN_X;
  int32_t cursor_y = TITLE_Y;
  writeln((GFXfont*)&FiraSans, "Shopping List", &cursor_x, &cursor_y, framebuffer);

  // --- Join button, top-right - opens the dedicated QR screen ---
  int btnX = EPD_WIDTH - MARGIN_X - JOIN_BTN_W;
  int btnY = 16;
  epd_draw_rect(btnX, btnY, JOIN_BTN_W, JOIN_BTN_H, 0x00, framebuffer);
  drawButtonLabel(btnX, JOIN_BTN_W, btnY + JOIN_BTN_H - 9, "Join");

  TouchTarget joinBtn;
  joinBtn.type = TOUCH_JOIN_BUTTON;
  joinBtn.x = btnX - JOIN_TOUCH_PAD;
  joinBtn.y = btnY - JOIN_TOUCH_PAD;
  joinBtn.w = JOIN_BTN_W + 2 * JOIN_TOUCH_PAD;
  joinBtn.h = JOIN_BTN_H + 2 * JOIN_TOUCH_PAD;
  g_touch_targets.push_back(joinBtn);

  // --- Send button, left of Join - emails the list (only if configured) ---
  if (showSendButton) {
    int sendX = btnX - BTN_GAP - SEND_BTN_W;
    epd_draw_rect(sendX, btnY, SEND_BTN_W, JOIN_BTN_H, 0x00, framebuffer);
    drawButtonLabel(sendX, SEND_BTN_W, btnY + JOIN_BTN_H - 9, "Send");

    // Padded like Join, except its right edge stops exactly where Join's
    // padded area starts, so the two can never both claim a tap.
    TouchTarget sendBtn;
    sendBtn.type = TOUCH_SEND_BUTTON;
    sendBtn.x = sendX - JOIN_TOUCH_PAD;
    sendBtn.y = btnY - JOIN_TOUCH_PAD;
    sendBtn.w = (btnX - JOIN_TOUCH_PAD) - sendBtn.x;
    sendBtn.h = JOIN_BTN_H + 2 * JOIN_TOUCH_PAD;
    g_touch_targets.push_back(sendBtn);
  }

  // --- Status line between the title and the buttons ---
  // Fitted to the gap so a long "Sent to <name>" can't run under a button.
  if (statusText && statusText[0]) {
    int32_t sx = MARGIN_X + 300;  // clear of "Shopping List" (256px measured)
    int32_t sy = TITLE_Y;
    int rightEdge = (showSendButton ? btnX - BTN_GAP - SEND_BTN_W : btnX) - BTN_GAP;
    String fitted = fitText((GFXfont*)&FiraSans, String(statusText), rightEdge - sx);
    writeln((GFXfont*)&FiraSans, fitted.c_str(), &sx, &sy, framebuffer);
  }

  epd_draw_hline(MARGIN_X, TITLE_Y + 14, EPD_WIDTH - 2 * MARGIN_X, 0x00, framebuffer);
  int y = TITLE_Y + 14 + 30;

  // --- Everyday items: tap the box when you notice you're out ---
  cursor_x = MARGIN_X;
  cursor_y = y;
  writeln((GFXfont*)&Roboto12, "Everyday items - tap when needed", &cursor_x, &cursor_y, framebuffer);
  // Next row's text top (checkbox row: baseline = rowY + 20, FiraSans
  // ascender 39) must clear this label's descender (7) with a few px spare.
  y += 30;

  int favRowsPerColumn = (favorites.size() + NUM_COLS - 1) / NUM_COLS;  // ceil
  int favStartY = y;

  for (size_t i = 0; i < favorites.size(); i++) {
    int col = i / favRowsPerColumn;
    int row = i % favRowsPerColumn;
    int rowX = columnX(col);
    int rowY = favStartY + row * LINE_HEIGHT;
    if (rowY > EPD_HEIGHT - LINE_HEIGHT) continue;  // ran out of room

    // Whole cell is tappable (checkbox + label), not just the 26px box -
    // that box plus 10px pad was only ~5mm square on this panel, which is
    // why favorites felt dead while the much bigger Join button didn't.
    // Cells tile exactly (LINE_HEIGHT tall, column width wide) so
    // neighbouring rows can never both claim the same tap.
    TouchTarget t;
    t.id = favorites[i].id;
    t.type = TOUCH_FAVORITE;
    t.x = rowX;
    t.y = rowY - (LINE_HEIGHT - CHECKBOX_SIZE) / 2;
    t.w = columnWidth() - 8;
    t.h = LINE_HEIGHT;
    g_touch_targets.push_back(t);

    drawCheckboxRow(rowX, rowY, favorites[i].needed, favorites[i].text);
  }

  y = favStartY + favRowsPerColumn * LINE_HEIGHT;

  y += 16;
  epd_draw_hline(MARGIN_X, y, EPD_WIDTH - 2 * MARGIN_X, 0x00, framebuffer);
  y += 30;

  // --- Ad-hoc list from the web app, also 3 columns ---
  cursor_x = MARGIN_X;
  cursor_y = y;
  writeln((GFXfont*)&Roboto12, "Also need", &cursor_x, &cursor_y, framebuffer);
  // Rows below are drawn at their text baseline: row ascender + this
  // label's descender (7) + a few px spare.
  y += ADHOC_ASCENDER + 7 + 5;

  if (items.empty()) {
    cursor_x = MARGIN_X;
    cursor_y = y;
    writeln(ADHOC_FONT, "Nothing extra right now.", &cursor_x, &cursor_y, framebuffer);
  } else {
    // Rows that fit per column: the last row's baseline plus its descender
    // must stay on the panel.
    int maxRows = (EPD_HEIGHT - ADHOC_DESCENDER - y) / ADHOC_LINE_HEIGHT + 1;
    if (maxRows < 1) maxRows = 1;  // guard: favorites section could in
                                    // theory push y this close to the
                                    // bottom; without this, i / maxRows
                                    // below would divide by zero
    int adHocStartY = y;
    // If there are more items than slots, the last slot says "+N more"
    // instead of silently dropping them - otherwise someone adding item 10
    // from a phone sees nothing change on the board and thinks it failed.
    size_t slots = (size_t)maxRows * NUM_COLS;
    bool overflow = items.size() > slots;
    size_t shown = overflow ? slots - 1 : items.size();
    if (overflow) {
      int col = shown / maxRows;
      int row = shown % maxRows;
      char more[32];
      snprintf(more, sizeof(more), "+%u more on phone", (unsigned)(items.size() - shown));
      int32_t mx = columnX(col);
      int32_t my = adHocStartY + row * ADHOC_LINE_HEIGHT;
      writeln((GFXfont*)&Roboto12, more, &mx, &my, framebuffer);
    }
    for (size_t i = 0; i < shown; i++) {
      int col = i / maxRows;
      int row = i % maxRows;

      int rowX = columnX(col);
      int rowY = adHocStartY + row * ADHOC_LINE_HEIGHT;

      // Item, then "(who added it)" after it on the same baseline, both in
      // ADHOC_FONT (Roboto12 - the same size as the "Also need" label).
      // The item always gets priority - it's what you're shopping for. The
      // name uses whatever room is left (up to NAME_MAX_W) and is left off
      // entirely if less than NAME_MIN_W remains.
      const int NAME_GAP = 8;
      const int NAME_MIN_W = 50;   // below this a name is just "(S...)"
      const int NAME_MAX_W = 110;  // "(Sarah)" = 81px, "(Christopher)" = 147px
      int room = columnWidth() - 8;
      String line = (items[i].done ? "[x] " : "[ ] ") + items[i].text;
      line = fitText(ADHOC_FONT, line, room);
      int itemW = textWidth(ADHOC_FONT, line.c_str());

      String name;
      int nameRoom = room - itemW - NAME_GAP;
      if (items[i].addedBy.length() > 0 && nameRoom >= NAME_MIN_W) {
        name = fitText((GFXfont*)&Roboto12, "(" + items[i].addedBy + ")",
                       min(nameRoom, NAME_MAX_W));
      }

      int32_t lx = rowX;
      int32_t ly = rowY;
      writeln(ADHOC_FONT, line.c_str(), &lx, &ly, framebuffer);
      if (name.length() > 0) {
        // Placed by measurement rather than relying on writeln's cursor.
        int32_t nx = rowX + itemW + NAME_GAP;
        int32_t ny = rowY;
        writeln((GFXfont*)&Roboto12, name.c_str(), &nx, &ny, framebuffer);
      }
    }
  }

  pushFrame();
  epd_poweroff();
}

void display_render_send_picker(const std::vector<String>& names, int timeoutSec) {
  if (!framebuffer) return;
  g_touch_targets.clear();

  epd_poweron();
  epd_clear();
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  int32_t cx = MARGIN_X;
  int32_t cy = TITLE_Y;
  writeln((GFXfont*)&FiraSans, "Send the list to:", &cx, &cy, framebuffer);
  epd_draw_hline(MARGIN_X, TITLE_Y + 14, EPD_WIDTH - 2 * MARGIN_X, 0x00, framebuffer);

  // Big, well-separated buttons: this screen is used standing at the
  // fridge, often by kids, so targets are generous (520x80px, ~57x9mm).
  const int BTN_W = 520, BTN_H = 80, BTN_SPACING = 100, FIRST_Y = 100;
  const int bx = (EPD_WIDTH - BTN_W) / 2;
  for (size_t i = 0; i < names.size() && i < 3; i++) {
    int by = FIRST_Y + i * BTN_SPACING;
    epd_draw_rect(bx, by, BTN_W, BTN_H, 0x00, framebuffer);
    epd_draw_rect(bx + 1, by + 1, BTN_W - 2, BTN_H - 2, 0x00, framebuffer);  // 2px border
    String label = fitText((GFXfont*)&FiraSans, names[i], BTN_W - 40);
    drawButtonLabel(bx, BTN_W, by + 55, label.c_str());

    TouchTarget t;
    t.id = String((int)i);
    t.type = TOUCH_PICK_RECIPIENT;
    t.x = bx; t.y = by; t.w = BTN_W; t.h = BTN_H;
    g_touch_targets.push_back(t);
  }

  // Cancel sits below the name buttons, smaller, so it's not hit by accident.
  const int CAN_W = 240, CAN_H = 60, CAN_Y = 420;
  const int canX = (EPD_WIDTH - CAN_W) / 2;
  epd_draw_rect(canX, CAN_Y, CAN_W, CAN_H, 0x00, framebuffer);
  drawButtonLabel(canX, CAN_W, CAN_Y + 45, "Cancel");
  TouchTarget cancel;
  cancel.type = TOUCH_CANCEL_PICKER;
  cancel.x = canX; cancel.y = CAN_Y; cancel.w = CAN_W; cancel.h = CAN_H;
  g_touch_targets.push_back(cancel);

  char footer[40];
  snprintf(footer, sizeof(footer), "Closes by itself in %d s", timeoutSec);
  int32_t fx = (EPD_WIDTH - textWidth((GFXfont*)&Roboto12, footer)) / 2;
  int32_t fy = 520;
  writeln((GFXfont*)&Roboto12, footer, &fx, &fy, framebuffer);

  pushFrame();
  epd_poweroff();
}

bool display_hit_test(int x, int y, TouchTarget& outTarget) {
  // No global padding: any slop is baked into each target's own rect, so
  // tiled favorite cells don't overlap each other.
  for (const auto& t : g_touch_targets) {
    if (x >= t.x && x < t.x + t.w &&
        y >= t.y && y < t.y + t.h) {
      outTarget = t;
      return true;
    }
  }
  return false;
}

void display_sleep() {
  epd_poweroff();
}
