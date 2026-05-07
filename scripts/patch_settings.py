#!/usr/bin/env python3
"""patch_settings.py — Add Messenger + Scrambler fields to the EEPROM settings struct.

Finds the struct by scanning for known fields it must contain (e.g. SquelchLevel,
TX_VFO) rather than matching its type name, which varies across firmware forks:
  Egzumer / F4HWN / Fusion:  t_EEPROM_Settings
  Some forks:                 t_Config, EEPROM_Config_t, t_Settings, ...
"""
import re, sys

path = "firmware/App/settings.h"

with open(path) as f:
    src = f.read()

if "MsgModulation" in src:
    print("settings.h already patched -- skipping")
    sys.exit(0)

# Debug: show all typedef struct closing lines in the file
print("Scanning settings.h for typedef struct close patterns...")
for m in re.finditer(r'\}\s*(\w+)\s*;', src):
    print(f"  Found: }}  {m.group(1)};  at char {m.start()}")

# Strategy 1: match the typedef struct that contains known settings fields.
# These field names appear in every fork of this firmware family.
KNOWN_FIELDS = [
    "SquelchLevel", "TX_VFO", "DualWatch", "CrossBand",
    "BACKLIGHT", "BacklightTime", "TxOffset", "RxOffset",
    "Roger", "ScanResumeMode", "AutoKeypadLock",
    "BatteryCalibration", "BatteryType",
]

STRUCT_RE = re.compile(
    r'(typedef\s+struct\s*(?:\w+\s*)?\{)(.*?)(\}\s*\w+\s*;)',
    re.DOTALL
)

best_match = None
best_score = 0

for m in STRUCT_RE.finditer(src):
    body = m.group(2)
    score = sum(1 for f in KNOWN_FIELDS if f in body)
    print(f"  Struct ending '{m.group(3).strip()}': score={score}/{len(KNOWN_FIELDS)}")
    if score > best_score:
        best_score = score
        best_match = m

insert_pos = None

if best_match and best_score >= 2:
    insert_pos = best_match.start(3)
    print(f"\nSelected struct: '{best_match.group(3).strip()}' (score {best_score})")
else:
    # Strategy 2: explicit name list for known firmware forks
    print("\nStrategy 1 failed -- trying explicit name patterns...")
    NAME_PATTERNS = [
        r'\}\s*t_EEPROM_Settings\s*;',
        r'\}\s*EEPROM_Config_t\s*;',
        r'\}\s*t_EEPROM_Config\s*;',
        r'\}\s*t_Config\s*;',
        r'\}\s*Config_t\s*;',
        r'\}\s*(?:t_)?[Ss]ettings\s*;',
        r'\}\s*\w*[Ss]etting\w*\s*;',
    ]
    for pat in NAME_PATTERNS:
        m = re.search(pat, src)
        if m:
            insert_pos = m.start()
            print(f"  Matched: {pat}")
            break

if insert_pos is None:
    # Strategy 3: last typedef struct close before EEPROM_VERSION define
    print("  Trying EEPROM_VERSION anchor...")
    ver_match = re.search(r'#define\s+EEPROM_CONFIG_VERSION', src)
    if ver_match:
        before = src[:ver_match.start()]
        last = None
        for m in re.finditer(r'\}\s*\w+\s*;', before):
            last = m
        if last:
            insert_pos = last.start()
            print(f"  Found last struct close before EEPROM_VERSION at char {insert_pos}")

if insert_pos is None:
    print("\nERROR: Could not locate settings struct.")
    print("Open firmware/App/settings.h, find the typedef struct containing")
    print("fields like SquelchLevel, TX_VFO, DualWatch, and insert the fields")
    print("from patch/settings_patch.h before its closing '} TypeName;' line.")
    sys.exit(1)

# Insert the new fields before the closing brace
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

src = src[:insert_pos] + new_fields + src[insert_pos:]

# Bump EEPROM config version so first boot reinitialises defaults
bumped = False
def bump(m):
    global bumped
    bumped = True
    return m.group(1) + str(int(m.group(2)) + 1)

src = re.sub(r'(#define\s+EEPROM_CONFIG_VERSION\s+)(\d+)', bump, src)
if not bumped:
    print("WARNING: EEPROM_CONFIG_VERSION not found -- settings may not reinitialise on first boot")

with open(path, "w") as f:
    f.write(src)

print("settings.h patched OK")
