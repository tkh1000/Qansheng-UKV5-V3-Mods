#!/usr/bin/env python3
"""probe_structure.py -- Find the actual source directory layout of Fusion firmware.
Writes GITHUB_ENV vars so subsequent steps use correct paths.
"""
import os, sys

FIRMWARE_ROOT = "firmware"

def find_file(name, root=FIRMWARE_ROOT):
    for dirpath, _, files in os.walk(root):
        if name in files:
            return os.path.join(dirpath, name)
    return None

app_c    = find_file("app.c")
settings = find_file("settings.h")
bk4819_c = find_file("bk4819.c")

print("=== Firmware source structure probe ===")
print(f"  app.c      : {app_c}")
print(f"  settings.h : {settings}")
print(f"  bk4819.c   : {bk4819_c}")

src_dir = None
if app_c:
    src_dir = os.path.dirname(app_c)
elif settings:
    src_dir = os.path.dirname(settings)

if src_dir is None:
    print("ERROR: Cannot determine source directory")
    sys.exit(1)

print(f"  src_dir    : {src_dir}")

print("\n=== Firmware directory tree (top 2 levels) ===")
for entry in sorted(os.listdir(FIRMWARE_ROOT)):
    full = os.path.join(FIRMWARE_ROOT, entry)
    if os.path.isdir(full):
        children = sorted(os.listdir(full))
        print(f"  {entry}/")
        for c in children[:6]:
            print(f"    {c}")
        if len(children) > 6:
            print(f"    ...({len(children)} total)")
    else:
        print(f"  {entry}")

github_env = os.environ.get("GITHUB_ENV", "")
if github_env:
    with open(github_env, "a") as f:
        f.write(f"FW_SRC_DIR={src_dir}\n")
        f.write(f"FW_APP_C={app_c or ''}\n")
        f.write(f"FW_SETTINGS_H={settings or ''}\n")
        f.write(f"FW_BK4819_C={bk4819_c or ''}\n")
    print(f"\nGITHUB_ENV written: FW_SRC_DIR={src_dir}")
