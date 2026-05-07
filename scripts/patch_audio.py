#!/usr/bin/env python3
"""patch_audio.py -- Wire RX de-scrambler into Fusion's audio path."""
import re, sys, os

def find_file(name, root="firmware"):
    for d, _, files in os.walk(root):
        if name in files:
            return os.path.join(d, name)
    return None

src_dir = os.environ.get("FW_SRC_DIR", "")
bk4819  = os.environ.get("FW_BK4819_C", "") or find_file("bk4819.c")

CANDIDATES = []
for name in ["audio.c", "app.c", "bk4819.c"]:
    p = find_file(name)
    if p:
        CANDIDATES.append(p)

ADC_PATTERNS = ["HAL_ADC_GetValue", "ADC_GetValue", "ADC1->DR", "hadc1"]

target = None
for c in CANDIDATES:
    with open(c) as f:
        if any(p in f.read() for p in ADC_PATTERNS):
            target = c
            break

if target is None:
    print("WARNING: No audio-sample file found -- RX de-scrambler needs manual wiring")
    print("TX scrambling still works. See patch/app_patch.c")
    sys.exit(0)

print(f"Patching RX de-scrambler into: {target}")
with open(target) as f:
    src = f.read()

if "SCRAMBLER_ProcessSample" in src:
    print("Already patched -- skipping")
    sys.exit(0)

rel = os.path.relpath(os.path.dirname(target), "firmware")
changes = 0

if "scrambler.h" not in src:
    src = re.sub(r'(#include\s+"[^"]+\.h")',
                 r'\1\n#ifdef ENABLE_SCRAMBLER\n#include "' + rel + r'/scrambler.h"\n#endif',
                 src, count=1)
    changes += 1

ADC_RE = re.compile(
    r'((?:uint16_t|int16_t|uint32_t)\s+(\w+)\s*=\s*'
    r'(?:HAL_ADC_GetValue\([^)]*\)|ADC_GetValue\([^)]*\)|ADC1->DR)\s*;)'
)
def wrap_adc(m):
    stmt, var = m.group(1), m.group(2)
    return (stmt
        + f"\n#ifdef ENABLE_SCRAMBLER\n"
          f"    if (gEeprom.ScramblerMode != 0) {{\n"
          f"        int16_t _s = SCRAMBLER_ProcessSample({var});\n"
          f"        {var} = (uint16_t)(_s + 2048);\n"
          f"    }}\n"
          f"#endif")

new_src, n = ADC_RE.subn(wrap_adc, src)
if n:
    src = new_src; changes += n
    print(f"  De-scrambler injected after {n} ADC read(s)")

if "SCRAMBLER_Enable" not in src:
    for sq in ["SQUELCH_Open", "SquelchOpen(", "g_SquelchOpen"]:
        if sq in src:
            idx = src.find(sq); eol = src.find("\n", idx)
            src = (src[:eol]
                + "\n#ifdef ENABLE_SCRAMBLER\n"
                  "    if (gEeprom.ScramblerMode != 0) SCRAMBLER_Enable();\n"
                  "#endif"
                + src[eol:])
            changes += 1; break

if "SCRAMBLER_Disable" not in src:
    for sq in ["SQUELCH_Close", "SquelchClosed(", "g_SquelchClose"]:
        if sq in src:
            idx = src.find(sq); eol = src.find("\n", idx)
            src = (src[:eol]
                + "\n#ifdef ENABLE_SCRAMBLER\n    SCRAMBLER_Disable();\n#endif"
                + src[eol:])
            changes += 1; break

with open(target, "w") as f:
    f.write(src)
print(f"RX de-scrambler patch complete -- {changes} change(s) in {target}")
