#!/usr/bin/env python3
"""patch_settings.py — Add Messenger + Scrambler fields to t_Settings in settings.h"""
import re, sys

path = "firmware/App/settings.h"

with open(path) as f:
    src = f.read()

if "MsgModulation" in src:
    print("settings.h already patched — skipping")
    sys.exit(0)

new_fields = (
    "\n"
    "#ifdef ENABLE_MESSENGER\n"
    "    uint8_t  MsgModulation;\n"
    "    uint8_t  MsgEnc;\n"
    "    uint8_t  MsgRx;\n"
    "    uint8_t  MsgAck;\n"
    "    uint8_t  EncKey[32];\n"
    "#endif\n"
    "#ifdef ENABLE_SCRAMBLER\n"
    "    uint8_t  ScramblerMode;\n"
    "#endif\n"
)

# Insert before the closing brace of the settings typedef struct
m = re.search(r'(\}\s*(?:t_)?[Ss]ettings\s*;)', src)
if not m:
    print("ERROR: cannot find t_Settings closing brace — patch manually")
    sys.exit(1)

src = src[:m.start()] + new_fields + src[m.start():]

# Bump EEPROM config version so first boot re-initialises defaults
src = re.sub(
    r'(#define\s+EEPROM_CONFIG_VERSION\s+)(\d+)',
    lambda m: m.group(1) + str(int(m.group(2)) + 1),
    src
)

with open(path, "w") as f:
    f.write(src)

print("settings.h patched OK")
