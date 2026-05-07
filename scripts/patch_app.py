#!/usr/bin/env python3
"""patch_app.py — Wire Messenger + Scrambler TX hooks into Fusion's app.c"""
import re, sys

path = "firmware/App/app.c"

with open(path) as f:
    src = f.read()

changes = 0

# 1. Includes after settings.h
if "messenger.h" not in src:
    src = re.sub(
        r'(#include\s+"App/settings\.h")',
        (r'\1\n'
         r'#ifdef ENABLE_MESSENGER\n#include "App/messenger.h"\n#endif\n'
         r'#ifdef ENABLE_SCRAMBLER\n#include "App/scrambler.h"\n#endif'),
        src, count=1
    )
    changes += 1

# 2. Init calls after settings load
if "MESSENGER_Init" not in src:
    for anchor in ["SETTINGS_LoadSettings();", "SETTINGS_Load();", "APP_Init();"]:
        if anchor in src:
            src = src.replace(
                anchor,
                (anchor
                 + "\n#ifdef ENABLE_MESSENGER\n    MESSENGER_Init();\n#endif"
                 + "\n#ifdef ENABLE_SCRAMBLER\n"
                   "    SCRAMBLER_Init((ScramblerMode_t)g_eeprom.Settings.ScramblerMode);\n"
                   "#endif"),
                1
            )
            changes += 1
            break

# 3. 10 ms timeslice — inject at top of function body
if "MESSENGER_TimeSlice10ms" not in src:
    for anchor in ["APP_TimeSlice10ms", "TimeSlice10ms", "gReducedService"]:
        if anchor in src:
            idx   = src.find(anchor)
            brace = src.find("{", idx)
            if brace != -1:
                src = (src[:brace + 1]
                       + "\n#ifdef ENABLE_MESSENGER\n    MESSENGER_TimeSlice10ms();\n#endif"
                       + src[brace + 1:])
                changes += 1
            break

# 4. Key handler — consume messenger keypresses first
if "MESSENGER_HandleKey" not in src:
    for anchor in ["APP_HandleKey(", "KEYBOARD_HandleKey(", "APP_ProcessKey("]:
        if anchor in src:
            idx   = src.find(anchor)
            brace = src.find("{", idx)
            if brace != -1:
                src = (src[:brace + 1]
                       + "\n#ifdef ENABLE_MESSENGER\n"
                         "    if (MESSENGER_HandleKey(Key, bKeyPressed, bKeyHeld)) return;\n"
                         "#endif"
                       + src[brace + 1:])
                changes += 1
            break

# 5. Scrambler TX enable on PTT press
if "SCRAMBLER_Enable" not in src:
    for anchor in ["RADIO_PrepareTX();", "APP_StartTX();", "gFlagStartTx = true;"]:
        if anchor in src:
            src = src.replace(
                anchor,
                (anchor
                 + "\n#ifdef ENABLE_SCRAMBLER\n"
                   "    if (g_eeprom.Settings.ScramblerMode != 0) SCRAMBLER_Enable();\n"
                   "#endif"),
                1
            )
            changes += 1
            break

# 6. Scrambler TX disable on PTT release
if "SCRAMBLER_Disable" not in src:
    for anchor in ["RADIO_EndTX();", "APP_EndTX();", "gFlagEndTx = true;"]:
        if anchor in src:
            src = src.replace(
                anchor,
                "#ifdef ENABLE_SCRAMBLER\n    SCRAMBLER_Disable();\n#endif\n    " + anchor,
                1
            )
            changes += 1
            break

with open(path, "w") as f:
    f.write(src)

print(f"app.c patched — {changes} change(s)")
