# E-Ink Shopping List Board

A touchscreen e-ink shopping list for the kitchen, built on the LILYGO T5 4.7" E-Paper (ESP32-S3) board. Tick everyday items on the glass when you run out, add anything else from your phone, and email or text the list to whoever is doing the shopping, all without an app or a cloud account.

<!-- Add a photo of the board here, e.g. ![The board on the fridge](docs/board.jpg) -->

## Features

- Tap everyday items (milk, eggs, bread and so on) right on the screen when you run out.
- Add anything else from a phone on your home WiFi (scan the Join QR code on the board). It shows up on the board within a few seconds, with the name of whoever added it.
- Email the list to someone, from the board's Send button or from your phone. Set up 2 or 3 people and you pick who gets it.
- Text the list from your phone: one tap opens your Messages app with the list ready to send. No SMS account or cost.
- Copy the whole list into Notes from the web app with one tap.
- Keeps working when the WiFi is down, and reconnects on its own.
- No code editing. WiFi and everything else is set up from your phone.
- Optional Pushover notifications and wireless (OTA) updates.

E-ink suits this really well. It's readable in any light and keeps showing the list even with the power off.

## Hardware

- **LILYGO T5 4.7" E-Paper, ESP32-S3 touch version** (LilyGo calls it "T5-4.7-S3"). Make sure it's the **S3** version with **touch**. The older ESP32 version needs different code.
- A USB-C cable and power supply.
- Optional: a case or frame, and a way to mount it.

## Install the firmware (the easy way)

No Arduino IDE needed. You'll need a computer with **Chrome or Edge** (Safari, Firefox and phones can't do this) and a USB-C cable that carries data.

1. Go to the installer page: **<https://lonesoulsurfer.github.io/eink-shopping-list/>**
2. **Hold down the BOOT button** (the middle button on the board) while you plug the board into your computer, then let go. Without this, the board usually won't show up in the list.
3. Click **Install**, pick the board from the list and click **Connect**.
4. When asked about erasing: tick **Erase** the first time you install on a board. When **updating** a board that already runs the shopping list, leave it **unticked**, or you'll lose your list, WiFi and email settings.
5. When it's finished, the installer closes and a green **Installed** box appears. Unplug the board and plug it back in to start it. If it ever sits on "Wrapping up" for more than a minute, the firmware is already installed: close the window and unplug/replug.

If Chrome says "No compatible devices found", the board isn't in install mode. Unplug it and do step 2 again. If it still doesn't show up, flip the USB-C plug over, or try another USB port or cable.

To **update** later, do the same thing with Erase unticked. The web app shows the board's firmware version at the bottom of the page, so you can tell whether it's up to date.

## Build it yourself (Arduino IDE)

Only needed if you want to change the code. Everything's in [`firmware/eink-shopping-list`](firmware/eink-shopping-list).

### 1. Board package

In the Arduino IDE (tested with 2.3.x), open **Boards Manager**, find **esp32 by Espressif Systems**, and install **version 2.0.15**. Any version from 2.0.5 to 2.0.15 should work.

> **Do not use 3.x.** The WiFi and TLS code targets the 2.0.x API. If the IDE offers to update board packages, click Later. If it's been updated by accident, you'll usually see errors like `invalid conversion from 'int*' to 'int32_t*'`: go back to Boards Manager and install 2.0.15 again.

### 2. Libraries

| Library | Where to get it | Compile-tested version |
|---|---|---|
| **LilyGo-EPD47**, the **esp32s3 branch** | [GitHub](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47/tree/esp32s3): choose the `esp32s3` branch, then *Code > Download ZIP*, then in the IDE *Sketch > Include Library > Add .ZIP Library* | esp32s3 branch |
| ESP Async WebServer (by ESP32Async) | Library Manager | 3.12.1 |
| Async TCP (by ESP32Async) | Library Manager | 3.5.0 |
| ArduinoJson (by Benoit Blanchon) | Library Manager | 7.4.2 |
| WiFiManager (by tzapu) | Library Manager | 2.0.17 |
| SensorLib (by Lewis He) | Library Manager | 0.5.0 |

The LilyGo library's **master branch is for the older ESP32 board**. You need the `esp32s3` branch. It also provides the fonts, so nothing extra is needed for those.

### 3. Tools settings

Select **ESP32S3 Dev Module** (not "LilyGo T-Display-S3"), then set:

| Setting | Value |
|---|---|
| **USB CDC On Boot** | Enabled |
| CPU Frequency | 240MHz (WiFi) |
| **Flash Size** | **16MB (128Mb)** |
| **Partition Scheme** | **16M Flash (3MB APP/9.9MB FATFS)** |
| **PSRAM** | **OPI PSRAM** |
| **Upload Mode** | UART0 / Hardware CDC |
| **USB Mode** | CDC and JTAG |
| Upload Speed | 921600 |
| Erase All Flash Before Sketch Upload | **Disabled** (Enabled wipes your saved list and settings) |

The two most common mistakes:

- **PSRAM disabled.** The display can't allocate its memory, so the board boots but never draws anything.
- **"Sketch too big" with a 1310720-byte maximum.** The partition scheme is still on the default. Set Flash Size first, then the partition scheme.

These settings can quietly reset to defaults when you change the board, update or reinstall the board package, or open the sketch from a new folder. The IDE remembers them per sketch folder, so when you update the code, replace the files inside your existing folder rather than unzipping a fresh copy somewhere new. Check them again before every upload if anything has changed.

### 4. Upload

Download this repository (**Code > Download ZIP** at <https://github.com/lonesoulsurfer/eink-shopping-list>, then unzip), open `firmware/eink-shopping-list/eink-shopping-list.ino`, and upload.

- **Check a port is selected.** The board name at the top of the IDE shows in bold with a USB symbol when a board and port are selected. Plain text with no symbol means no port: pick it under Tools > Port.
- **Use the USB port.** Under Tools > Port, pick the COM port (Windows) or `/dev/cu...` port (Mac), not a network address. A network port means wireless upload, which asks for the update password from the setup page.
- **When it finishes, restart the board:** press RESET once, or unplug the USB-C cable and plug it back in. The ESP32-S3 can stay in its bootloader after an upload, which looks like "nothing happened". If the board has a battery connected, unplugging USB won't restart it.
- **The first compile is slow,** often 5 to 10 minutes, because it builds the whole ESP32 core. Later compiles are much faster unless you change board settings. If you're not sure it's working, check Task Manager for `cc1plus.exe` using CPU.

**If the upload fails with "port doesn't exist" or "device which does not exist was specified"**, the ESP32-S3's USB port has disappeared, usually because the board restarted (or keeps crashing) and Windows gave it a new COM number. Put it into upload mode so it holds still: unplug it, **hold BOOT while plugging it back in**, then let go. Pick the COM port that appears and upload again, then unplug and replug when it's done. If no port appears at all, try another cable; some USB-C cables only carry power.

**If the screen doesn't change after uploading**, open Tools > Serial Monitor at 115200 baud and restart the board. The messages show how far it got. A crash right at the start almost always means PSRAM isn't set to OPI PSRAM.

Build warnings you can ignore:

- `send_P ... is deprecated` and `beginResponse_P ... is deprecated`
- `TouchDrvGT911.hpp is deprecated`
- `getPoint ... is deprecated`

These are newer library APIs that the code doesn't use yet, kept deliberately so it works with both older and newer library versions.

## First-time setup

1. Power the board. It shows **"Setup needed"** with a QR code.
2. Scan the QR code with your phone, or join the WiFi network **ShoppingList-Setup** by hand. A setup page opens. If it doesn't, browse to `192.168.4.1`.
3. Pick your home WiFi and enter its password. Optionally, fill in:
   - **Pushover**: App Token and User Key, for "Need milk" notifications ([pushover.net](https://pushover.net); paid after a free trial).
   - **Send button email**: see below.
   - **Wireless update password**: 8+ characters to enable OTA uploads. Leave blank to keep OTA off.
4. Save. The board joins your WiFi and shows the list.

## Using it

**On the board**

- **Everyday items:** tap one to tick it ("we need this"), tap again to untick.
- **Join:** shows a QR code that opens the web app on a phone, plus the board's address to type in if the camera won't scan.
- **Send:** emails the list. With several recipients set up, it asks who to send it to.
- Status messages appear next to the title: *Sent to (name)*, *Send failed*, *No WiFi*, *Nothing to send*.

**In the web app** (scan the Join QR code, or open `http://shoppinglist.local`, with the phone on the same WiFi)

- Add and remove everyday items, tick them, and add "Also need" items. The app asks your name once, so the board can show who added each item.
- **Copy list for shopping:** copies the list for Notes or a message, then asks whether to clear the board.
- **Email list to (name):** one button per person with an email address, once email is set up. The board sends it, and the page tells you when it's gone (or why it didn't).
- **Text list to (name):** one button per person with a mobile number. It opens your phone's Messages app with their number and the list filled in; tap Send. The text comes from your own number, so replies come back to you. The board itself never sends texts.
- **Clear 'Also need'** and **Clear everything** both ask for confirmation first.
- **Sending the list:** set up to 3 people, each with an email address, a mobile number, or both, plus the sending Gmail account (only needed for email). People with an email appear on the board's Send button and the phone's Email buttons; people with a mobile number get a Text button.
- **Re-run WiFi setup:** forgets the WiFi only. Other settings are kept.

### Add it to your phone's home screen

Saving the web app to your home screen gives it an icon and opens it full screen, like an app. It's an icon that opens the page, not a live widget. Open the page first (scan the Join QR code), then:

- **iPhone, Safari:** tap the **...** button at the bottom (or the Share button, if you can see it), then **Share** > **Add to Home Screen**. If it isn't in the list, scroll down and tap **View More** or **Edit Actions** to find it. If there's an **Open as Web App** switch, turn it on. Tap **Add**. If Safari's toolbar has disappeared, tap the bottom edge of the screen to bring it back.
- **iPhone, Chrome:** tap the **Share** icon at the right of the address bar, then **Add to Home Screen**. (It isn't in Chrome's **...** menu.)
- **Android, Chrome:** tap the **...** menu (top right), then **Add to Home screen** or **Install app**.

Private and incognito tabs can't add to the home screen; use a normal tab. The shortcut saves the board's IP address, so see the note below about keeping that address fixed.

## Email and text setup

**Texts** need no setup beyond a mobile number: add it next to the person's name in the web app's **Sending the list** section and tap Save. Australian numbers like `0412 345 678` are fine; spaces and dashes are removed. For someone overseas, use the `+` country code format.

**Email** is sent by the board, which takes a few more steps:


The list is sent **from a Gmail account** using an **app password**. Gmail no longer accepts normal passwords from devices.

1. Turn on **2-Step Verification** for the Gmail account (Google Account > Security).
2. Create an app password at <https://myaccount.google.com/apppasswords>. Copy the 16-letter code.
3. In the web app's **Sending the list** section, enter the Gmail address, the app password, and up to 3 people. Recipients can use any email provider. Save.

> **Use a separate Gmail account for the board if you can.** An app password can also *read* that mailbox, and it's stored on the board, where someone with physical access and a USB cable could recover it. You can revoke an app password at any time from your Google account. Changing the account's main password also revokes it, so Send will fail until you enter a new one.

The board verifies Google's TLS certificate before sending. The Google root certificates in `google_roots.h` expire in **June 2036**.

## Notes

- The web app only works on your home WiFi, since the board itself serves it. Use Send, or a Text button, to get the list to someone who's out.
- After a firmware update, if the web app still looks the same as before, close it fully and open it again (on iPhone, swipe it away in the app switcher). Home-screen web apps can keep the old page open. From 1.0.1 the board tells phones not to keep old copies, so this should only be needed once.
- The Join QR code uses the board's IP address (like `http://192.168.0.162`) rather than `shoppinglist.local`, because many phones, especially Android ones, can't open `.local` addresses and just show a blank page. iPhones and computers usually manage `shoppinglist.local` fine.
- If people save the web app to their home screen, it saves that IP address. Most routers keep giving a device the same address, but if the board's address ever changes, those shortcuts stop working; scan the Join code again to fix it. To stop it changing, set up a "DHCP reservation" for the board in your router's settings.
- If you change your router or WiFi password, power-cycle the board. When it can't find its saved network it opens the setup hotspot (3 minutes at a time) until you set it up again or the old network comes back. If the board is already running when you change the router, it just shows "No WiFi" until you power-cycle it.
- The limits are 20 "Also need" items, 18 everyday items and 40 characters per item, to stay inside what the ESP32 can safely store. If there are more items than fit on screen, the last spot says "+N more on phone".
- There's no login. Anyone on your home WiFi can change the list or the email recipients. That's fine at home, but don't use it on a shared network.
- Phones block the normal clipboard on plain `http://` pages, so the web app uses an older copy method. It works on iPhone. If a phone refuses, you get a box to copy from by hand, and the board isn't cleared.
- The screen is drawn rotated 180 degrees so the USB-C port sits on the left. If you'd rather have it on the right, set `SCREEN_ROTATE_180` to `0` in `device_settings.h`. Touch follows automatically.
- To wipe the board (say, before giving it away), upload once with *Erase All Flash Before Sketch Upload* enabled, then turn it back off.

## Tested so far

<!-- Update this list before publishing -->
- Tested on hardware: touch, everyday items, Join QR, web app add/tick/clear, Copy (iPhone), adding to the iPhone home screen, Send by email (single recipient), offline behaviour, "No WiFi", the 180-degree screen rotation, the web installer (install and update with Erase unticked, settings kept), wireless (OTA) updates, and the Text buttons from an iPhone home-screen web app.
- Built but not yet tested on hardware: emailing from the phone, Send recipient picker (2 or 3 recipients), Pushover notifications, copying on Android, router-off recovery.

## Project layout

```
firmware/eink-shopping-list/
  eink-shopping-list.ino   main sketch: boot, loop, touch handling, Send
  display_render.*         everything drawn on the e-ink panel
  touch_input.*            GT911 touch, one tap per press
  wifi_setup.*             non-blocking WiFi + setup hotspot (WiFiManager)
  web_server.*             the web app's API
  webpage.h                the web app itself (HTML/JS, served by the board)
  list_store.*, favorites.*  the two lists, saved to flash
  app_config.*             settings (email, Pushover, OTA), saved to flash
  email_send.*, google_roots.h  Gmail SMTP with certificate checking
  send_request.*           lets the phone ask the board to send an email
  notify.*, notify_queue.*   Pushover notifications
  store_lock.h, store_limits.h, redraw_flag.*  thread safety, size limits
  qrcode_ricmoo.*          QR code generator (Richard Moore, MIT)
  device_settings.h        network name and firmware version
docs/                      the web installer (GitHub Pages): index.html, manifest.json,
  firmware/<version>/      and the built firmware for each release
tools/make_release.py      copies a new build into docs/ for the installer
```

## Making a new release

1. Make your changes and bump `FIRMWARE_VERSION` in `device_settings.h`.
2. In the Arduino IDE, check the Tools settings above (especially PSRAM: OPI PSRAM), then use **Sketch > Export Compiled Binary**.
3. From the repository folder run `python tools/make_release.py`. It copies the build into `docs/firmware/<version>/`, updates `docs/manifest.json` and the version on the installer page, and stops with a message if the version wasn't bumped or the flash/partition settings were wrong.
4. Upload the new `docs/firmware/<version>/` folder and the changed `docs/manifest.json` and `docs/index.html` to GitHub. The installer page picks up the new version within a few minutes.

The installer writes the firmware in four parts at the same addresses as the Arduino IDE, skipping the area where the board keeps its settings, so updating (without Erase) keeps the list, WiFi and email settings.

## Licence

The source code in this repository is MIT licensed, see [LICENSE](LICENSE). The ready-built firmware in `docs/firmware/` also contains the [LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47/tree/esp32s3) display library, which is GPL-3.0, so that compiled firmware is distributed under the GPL-3.0. The complete source for it is this repository plus the libraries listed above.

## Credits

- [LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47), the display driver and fonts, based on [epdiy](https://github.com/vroland/epdiy)
- [QRCode](https://github.com/ricmoo/QRCode) by Richard Moore
- [WiFiManager](https://github.com/tzapu/WiFiManager) by tzapu
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) and AsyncTCP by ESP32Async
- [ArduinoJson](https://arduinojson.org) by Benoit Blanchon
- [SensorLib](https://github.com/lewisxhe/SensorLib) by Lewis He

---

By [lonesoulsurfer](https://github.com/lonesoulsurfer). Full build guide on [Instructables](https://www.instructables.com/member/lonesoulsurfer/).
