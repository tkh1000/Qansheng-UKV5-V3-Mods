#!/usr/bin/env python3
"""patch_audio.py — Wire de-scrambler into Fusion's RX audio path.

Frequency inversion is self-inverse: SCRAMBLER_ProcessSample() de-scrambles
received audio using the exact same function as TX scrambling.
Probes App/audio.c, App/app.c, App/bk4819.c for ADC reads and injects the hook.
"""
import re, sys, os

CANDIDATES = [
    "firmware/App/audio.c",
    "firmware/App/app.c",
    "firmware/App/bk4819.c",
]
ADC_PATTERNS = [
    "HAL_ADC_GetValue",
    "ADC_GetValue",
    "ADC1->DR",
    "hadc1",
]

# Find the first candidate file that contains an ADC read
target = None
for c in CANDIDATES:
    if os.path.exists(c):
        with open(c) as f:
            txt = f.read()
        if any(p in txt for p in ADC_PATTERNS):
            target = c
            break

if target is None:
    print("WARNING: No audio-sample file found — RX de-scrambler needs manual wiring")
    print("         TX scrambling still works. See patch/app_patch.c for instructions.")
    sys.exit(0)   # non-fatal

print(f"Patching RX de-scrambler into: {target}")

with open(target) as f:
    src = f.read()

if "SCRAMBLER_ProcessSample" in src:
    print("Already patched — skipping")
    sys.exit(0)

changes = 0

# 1. Add scrambler.h include
if "scrambler.h" not in src:
    src = re.sub(
        r'(#include\s+"[^"]+\.h")',
        r'\1\n#ifdef ENABLE_SCRAMBLER\n#include "App/scrambler.h"\n#endif',
        src, count=1
    )
    changes += 1

# 2. Wrap every ADC read with de-scrambler processing.
#    The same SCRAMBLER_ProcessSample() that scrambles TX audio de-scrambles
#    RX audio — frequency inversion is its own inverse.
ADC_RE = re.compile(
    r'((?:uint16_t|int16_t|uint32_t)\s+(\w+)\s*=\s*'
    r'(?:HAL_ADC_GetValue\([^)]*\)|ADC_GetValue\([^)]*\)|ADC1->DR)\s*;)'
)

def wrap_adc(m):
    stmt = m.group(1)
    var  = m.group(2)
    return (
        stmt
        + "\n#ifdef ENABLE_SCRAMBLER\n"
          "    if (g_eeprom.Settings.ScramblerMode != 0) {\n"
          "        /* De-scramble: frequency inversion is self-inverse */\n"
        + f"        int16_t _s = SCRAMBLER_ProcessSample({var});\n"
        + f"        {var} = (uint16_t)(_s + 2048); /* re-centre to 12-bit unsigned */\n"
          "    }\n"
          "#endif"
    )

new_src, n = ADC_RE.subn(wrap_adc, src)
if n:
    src = new_src
    changes += n
    print(f"  Injected de-scrambler after {n} ADC read(s)")

# 3. Hook SCRAMBLER_Enable onto squelch-open (resets BFO phase cleanly)
if "SCRAMBLER_Enable" not in src:
    for sq in ["SQUELCH_Open", "SquelchOpen(", "g_SquelchOpen"]:
        if sq in src:
            idx = src.find(sq)
            eol = src.find("\n", idx)
            src = (
                src[:eol]
                + "\n#ifdef ENABLE_SCRAMBLER\n"
                  "    if (g_eeprom.Settings.ScramblerMode != 0) SCRAMBLER_Enable();\n"
                  "#endif"
                + src[eol:]
            )
            changes += 1
            print(f"  SCRAMBLER_Enable hooked onto squelch-open ({sq})")
            break

# 4. Hook SCRAMBLER_Disable onto squelch-close
if "SCRAMBLER_Disable" not in src:
    for sq in ["SQUELCH_Close", "SquelchClosed(", "g_SquelchClose"]:
        if sq in src:
            idx = src.find(sq)
            eol = src.find("\n", idx)
            src = (
                src[:eol]
                + "\n#ifdef ENABLE_SCRAMBLER\n    SCRAMBLER_Disable();\n#endif"
                + src[eol:]
            )
            changes += 1
            print(f"  SCRAMBLER_Disable hooked onto squelch-close ({sq})")
            break

with open(target, "w") as f:
    f.write(src)

print(f"RX de-scrambler patch complete — {changes} change(s) in {target}")
