#!/usr/bin/env python3
"""patch_settings.py -- Add Messenger + Scrambler fields to the EEPROM settings struct.
Finds the file via FW_SETTINGS_H env var set by probe_structure.py,
or falls back to searching the firmware tree.
"""
import re, sys, os

def find_file(name, root="firmware"):
    for d, _, files in os.walk(root):
        if name in files:
            return os.path.join(d, name)
    return None

path = os.environ.get("FW_SETTINGS_H") or find_file("settings.h")
if not path or not os.path.exists(path):
    print(f"ERROR: settings.h not found (FW_SETTINGS_H={os.environ.get('FW_SETTINGS_H')})")
    sys.exit(1)

print(f"Patching: {path}")
with open(path) as f:
    src = f.read()

if "MsgModulation" in src:
    print("Already patched -- skipping")
    sys.exit(0)

# Debug: show all struct-close lines
print("Struct close candidates:")
for m in re.finditer(r'\}\s*(\w+)\s*;', src):
    print(f"  }}  {m.group(1)};")

# Strategy 1: find the struct that contains the most known settings fields
KNOWN = ["SquelchLevel","TX_VFO","DualWatch","CrossBand","BACKLIGHT",
         "BacklightTime","TxOffset","Roger","ScanResumeMode","AutoKeypadLock",
         "BatteryCalibration","BatteryType","ModulationMode","FreqOffsetDir"]

best_pos, best_score = None, 0
for m in re.finditer(r'typedef\s+struct\s*(?:\w+\s*)?\{(.*?)\}\s*\w+\s*;', src, re.DOTALL):
    score = sum(1 for f in KNOWN if f in m.group(1))
    end   = m.end() - len(m.group(0)) + m.start(0) + m.group(0).rfind('}')
    close = src[m.start(0) + m.group(0).rfind('}'):]
    name  = re.match(r'\}\s*(\w+)\s*;', close)
    print(f"  Struct '{name.group(1) if name else '?'}': score={score}")
    if score > best_score:
        best_score = score
        best_pos   = m.start(0) + m.group(0).rfind('}')

if best_score < 2:
    # Strategy 2: explicit name list
    for pat in [r'\}\s*t_EEPROM_Settings\s*;', r'\}\s*EEPROM_Config_t\s*;',
                r'\}\s*t_EEPROM_Config\s*;',   r'\}\s*t_Config\s*;',
                r'\}\s*(?:t_)?[Ss]ettings\s*;',r'\}\s*\w*[Ss]etting\w*\s*;']:
        m = re.search(pat, src)
        if m:
            best_pos = m.start()
            print(f"  Matched fallback pattern: {pat}")
            break

if best_pos is None:
    # Strategy 3: last struct close before EEPROM_CONFIG_VERSION
    ver = re.search(r'#define\s+EEPROM_CONFIG_VERSION', src)
    if ver:
        last = None
        for m in re.finditer(r'\}\s*\w+\s*;', src[:ver.start()]):
            last = m
        if last:
            best_pos = last.start()
            print("  Used EEPROM_VERSION anchor fallback")

if best_pos is None:
    print("ERROR: Could not locate settings struct closing brace")
    print("Manually add fields from patch/settings_patch.h to settings.h")
    sys.exit(1)

new_fields = (
    "\n#ifdef ENABLE_MESSENGER\n"
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
src = src[:best_pos] + new_fields + src[best_pos:]

bumped = [False]
def bump(m):
    bumped[0] = True
    return m.group(1) + str(int(m.group(2)) + 1)
src = re.sub(r'(#define\s+EEPROM_CONFIG_VERSION\s+)(\d+)', bump, src)
if not bumped[0]:
    print("WARNING: EEPROM_CONFIG_VERSION not found")

with open(path, "w") as f:
    f.write(src)
print("settings.h patched OK")
