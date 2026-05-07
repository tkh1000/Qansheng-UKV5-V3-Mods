#!/usr/bin/env python3
"""probe_structure.py -- Find Fusion's source layout and export paths to GITHUB_ENV."""
import os, sys

FIRMWARE_ROOT = "firmware"

def find_file(name, root=FIRMWARE_ROOT):
    for dirpath, _, files in os.walk(root):
        if name in files:
            return os.path.join(dirpath, name)
    return None

def find_dir(name, root=FIRMWARE_ROOT):
    """Find a directory by name (case-insensitive match)."""
    for dirpath, dirs, _ in os.walk(root):
        for d in dirs:
            if d.lower() == name.lower():
                return os.path.join(dirpath, d)
    return None

app_c      = find_file("app.c")
settings_h = find_file("settings.h")
bk4819_c   = find_file("bk4819.c")

print("=== Firmware source structure probe ===")
print(f"  app.c      : {app_c}")
print(f"  settings.h : {settings_h}")
print(f"  bk4819.c   : {bk4819_c}")

# Primary source directory (where app.c lives)
src_dir = os.path.dirname(app_c) if app_c else (
          os.path.dirname(settings_h) if settings_h else None)

if src_dir is None:
    print("ERROR: Cannot determine source directory")
    sys.exit(1)

# Helper directory (look for Helper/ or helper/ sibling)
parent = os.path.dirname(src_dir)
helper_dir = None
for candidate in [os.path.join(parent, "Helper"),
                  os.path.join(parent, "helper"),
                  os.path.join(src_dir, "Helper"),
                  os.path.join(src_dir, "helper")]:
    if os.path.isdir(candidate):
        helper_dir = candidate
        break

if helper_dir is None:
    # Create it alongside the source dir
    helper_dir = os.path.join(parent, "Helper")
    os.makedirs(helper_dir, exist_ok=True)
    print(f"  Created helper dir: {helper_dir}")

src_prefix    = os.path.relpath(src_dir,    FIRMWARE_ROOT)
helper_prefix = os.path.relpath(helper_dir, FIRMWARE_ROOT)

print(f"  src_dir    : {src_dir}  (prefix='{src_prefix}')")
print(f"  helper_dir : {helper_dir}  (prefix='{helper_prefix}')")

# Show directory tree
print("\n=== Firmware directory tree ===")
for entry in sorted(os.listdir(FIRMWARE_ROOT)):
    full = os.path.join(FIRMWARE_ROOT, entry)
    if os.path.isdir(full):
        children = sorted(os.listdir(full))
        print(f"  {entry}/  ({len(children)} files)")
        for c in children[:4]:
            print(f"    {c}")
        if len(children) > 4:
            print(f"    ...({len(children)} total)")

# Export to GITHUB_ENV
github_env = os.environ.get("GITHUB_ENV", "")
if github_env:
    with open(github_env, "a") as f:
        f.write(f"FW_SRC_DIR={src_dir}\n")
        f.write(f"FW_HELPER_DIR={helper_dir}\n")
        f.write(f"FW_SRC_PREFIX={src_prefix}\n")
        f.write(f"FW_HELPER_PREFIX={helper_prefix}\n")
        f.write(f"FW_APP_C={app_c or ''}\n")
        f.write(f"FW_SETTINGS_H={settings_h or ''}\n")
        f.write(f"FW_BK4819_C={bk4819_c or ''}\n")
    print(f"\nGITHUB_ENV: FW_SRC_DIR={src_dir}, FW_HELPER_DIR={helper_dir}")
    print(f"            FW_SRC_PREFIX={src_prefix}, FW_HELPER_PREFIX={helper_prefix}")
