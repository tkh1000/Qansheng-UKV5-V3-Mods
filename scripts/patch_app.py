#!/usr/bin/env python3
"""patch_app.py -- Wire Messenger + Scrambler TX hooks into Fusion's app.c.
Uses FW_APP_C env var set by probe_structure.py, or searches firmware tree.
"""
import re, sys, os

def find_file(name, root="firmware"):
    for d, _, files in os.walk(root):
        if name in files:
            return os.path.join(d, name)
    return None

path = os.environ.get("FW_APP_C") or find_file("app.c")
if not path or not os.path.exists(path):
    print(f"ERROR: app.c not found (FW_APP_C={os.environ.get('FW_APP_C')})")
    sys.exit(1)

print(f"Patching: {path}")
with open(path) as f:
    src = f.read()

# Determine include path style (App/ vs app/ vs just the filename)
src_dir = os.path.dirname(path)  # e.g. firmware/app
rel     = os.path.relpath(src_dir, "firmware")  # e.g. App/app
# For #include in app.c: app.c uses -I firmware/App/ so only the last
# path component is needed (e.g. "app/messenger.h")
rel_inc = os.path.basename(src_dir)  # e.g. "app" or "App"
changes = 0

# 1. Includes -- use the relative dir we found
msg_inc = f'#include "{rel_inc}/messenger.h"'
scr_inc = f'#include "{rel_inc}/scrambler.h"'
set_inc_pattern = re.compile(
    r'(#include\s+"' + re.escape(rel) + r'/settings\.h"'
    r'|#include\s+"settings\.h")',
    re.IGNORECASE
)

if "messenger.h" not in src:
    new_inc = (f'\n#ifdef ENABLE_MESSENGER\n#include "{rel_inc}/messenger.h"\n#endif\n'
               f'#ifdef ENABLE_SCRAMBLER\n#include "{rel_inc}/scrambler.h"\n#endif')
    src, n = set_inc_pattern.subn(r'\1' + new_inc, src, count=1)
    if n:
        changes += 1
    else:
        # fallback: add after first #include line
        first = src.find('#include')
        eol   = src.find('\n', first)
        src   = src[:eol+1] + new_inc + src[eol+1:]
        changes += 1

# 2. Init calls
if "MESSENGER_Init" not in src:
    for anchor in ["SETTINGS_LoadSettings();", "SETTINGS_Load();", "APP_Init();"]:
        if anchor in src:
            src = src.replace(anchor,
                anchor
                + "\n#ifdef ENABLE_MESSENGER\n    MESSENGER_Init();\n#endif"
                + "\n#ifdef ENABLE_SCRAMBLER\n"
                  "    SCRAMBLER_Init((ScramblerMode_t)gEeprom.ScramblerMode);\n"
                  "#endif",
                1)
            changes += 1
            break

# 3. 10ms timeslice
if "MESSENGER_TimeSlice10ms" not in src:
    for anchor in ["APP_TimeSlice10ms", "TimeSlice10ms", "gReducedService"]:
        if anchor in src:
            idx   = src.find(anchor)
            brace = src.find("{", idx)
            if brace != -1:
                src = (src[:brace+1]
                       + "\n#ifdef ENABLE_MESSENGER\n    MESSENGER_TimeSlice10ms();\n#endif"
                       + src[brace+1:])
                changes += 1
            break

# 4. Key handler
if "MESSENGER_HandleKey" not in src:
    for anchor in ["APP_HandleKey(", "KEYBOARD_HandleKey(", "APP_ProcessKey("]:
        if anchor in src:
            idx   = src.find(anchor)
            brace = src.find("{", idx)
            if brace != -1:
                src = (src[:brace+1]
                       + "\n#ifdef ENABLE_MESSENGER\n"
                         "    if (MESSENGER_HandleKey(Key, bKeyPressed, bKeyHeld)) return;\n"
                         "#endif"
                       + src[brace+1:])
                changes += 1
            break

# 5. Scrambler TX enable on PTT press
if "SCRAMBLER_Enable" not in src:
    for anchor in ["RADIO_PrepareTX();", "APP_StartTX();", "gFlagStartTx = true;"]:
        if anchor in src:
            src = src.replace(anchor,
                anchor
                + "\n#ifdef ENABLE_SCRAMBLER\n"
                  "    if (gEeprom.ScramblerMode != 0) SCRAMBLER_Enable();\n"
                  "#endif",
                1)
            changes += 1
            break

# 6. Scrambler TX disable on PTT release
if "SCRAMBLER_Disable" not in src:
    for anchor in ["RADIO_EndTX();", "APP_EndTX();", "gFlagEndTx = true;"]:
        if anchor in src:
            src = src.replace(anchor,
                "#ifdef ENABLE_SCRAMBLER\n    SCRAMBLER_Disable();\n#endif\n    " + anchor,
                1)
            changes += 1
            break

with open(path, "w") as f:
    f.write(src)
print(f"app.c patched -- {changes} change(s)")
