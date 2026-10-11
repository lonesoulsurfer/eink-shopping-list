#!/usr/bin/env python3
"""
Copies a freshly built firmware into docs/firmware/<version>/ and points the
web installer (docs/manifest.json) at it.

1. In the Arduino IDE, set the Tools settings from the README, then
   Sketch > Export Compiled Binary. This creates a "build" folder inside
   firmware/eink-shopping-list/.
2. Bump FIRMWARE_VERSION in firmware/eink-shopping-list/device_settings.h
   BEFORE exporting, so the new version is inside the firmware.
3. From the repository folder, run:   python tools/make_release.py
4. Commit/upload the new docs/firmware/<version>/ folder and docs/manifest.json.

Needs only Python 3, nothing to install. Safe to run more than once.
"""
import json, re, shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKETCH = ROOT / "firmware" / "eink-shopping-list"
DOCS = ROOT / "docs"

def fail(msg):
    print("ERROR: " + msg)
    sys.exit(1)

# Version from device_settings.h
m = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', (SKETCH / "device_settings.h").read_text())
if not m:
    fail("couldn't find FIRMWARE_VERSION in device_settings.h")
version = m.group(1)

# Exported build files (Arduino IDE puts them in build/<board-id>/)
apps = sorted(SKETCH.glob("build/*/eink-shopping-list.ino.bin"), key=lambda p: p.stat().st_mtime)
if not apps:
    fail("no exported build found. In the Arduino IDE use Sketch > Export Compiled Binary first.")
app = apps[-1]
boot = app.with_name("eink-shopping-list.ino.bootloader.bin")
part = app.with_name("eink-shopping-list.ino.partitions.bin")
for f in (boot, part):
    if not f.exists():
        fail(f"missing {f.name} next to {app.name}")

if version.encode() not in app.read_bytes():
    fail(f"version {version} isn't inside the exported firmware. Bump FIRMWARE_VERSION, "
         "then export again.")

# Catch wrong Tools settings: the partition table must match the original
# 16MB layout, and the bootloader must be set for 16MB flash.
previous = sorted(p for p in (DOCS / "firmware").iterdir() if p.is_dir() and p.name != version)
if previous and part.read_bytes() != (previous[0] / "partitions.bin").read_bytes():
    fail("the partition table doesn't match earlier releases. Check Tools > Flash Size (16MB) "
         "and Partition Scheme (16M Flash, 3MB APP/9.9MB FATFS), then export again.")
if boot.read_bytes()[3] >> 4 != 4:
    fail("the bootloader isn't set for 16MB flash. Check Tools > Flash Size, then export again.")

# boot_app0.bin never changes; reuse the copy from the first release.
boot_app0 = DOCS / "firmware" / "1.0.0" / "boot_app0.bin"
if not boot_app0.exists():
    fail("docs/firmware/1.0.0/boot_app0.bin is missing")

out = DOCS / "firmware" / version
out.mkdir(parents=True, exist_ok=True)
shutil.copy(boot, out / "bootloader.bin")
shutil.copy(part, out / "partitions.bin")
if out != boot_app0.parent:
    shutil.copy(boot_app0, out / "boot_app0.bin")
shutil.copy(app, out / "eink-shopping-list.bin")

manifest_path = DOCS / "manifest.json"
manifest = json.loads(manifest_path.read_text())
manifest["version"] = version
for p in manifest["builds"][0]["parts"]:
    p["path"] = re.sub(r"firmware/[^/]+/", f"firmware/{version}/", p["path"])
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")

page = DOCS / "index.html"
page.write_text(re.sub(r'(<strong id="ver">)[^<]*(</strong>)', rf"\g<1>{version}\g<2>", page.read_text()))

print(f"Release {version} ready in docs/firmware/{version}/ and docs/manifest.json updated.")
print("Remember: PSRAM must be OPI PSRAM when exporting (the script can't check that).")
