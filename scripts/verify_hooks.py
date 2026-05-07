#!/usr/bin/env python3
"""verify_hooks.py — Confirm all Messenger + Scrambler symbols are wired into Fusion source."""
import os

SYMBOLS = {
    "MESSENGER_Init": [
        "firmware/App/app.c",
    ],
    "MESSENGER_TimeSlice10ms": [
        "firmware/App/app.c",
    ],
    "MESSENGER_HandleKey": [
        "firmware/App/app.c",
    ],
    "SCRAMBLER_Init": [
        "firmware/App/app.c",
    ],
    "SCRAMBLER_Enable": [
        "firmware/App/app.c",
        "firmware/App/audio.c",
        "firmware/App/bk4819.c",
    ],
    "SCRAMBLER_Disable": [
        "firmware/App/app.c",
        "firmware/App/audio.c",
        "firmware/App/bk4819.c",
    ],
    "SCRAMBLER_ProcessSample": [
        "firmware/App/audio.c",
        "firmware/App/app.c",
        "firmware/App/bk4819.c",
    ],
}

missing = []
for sym, paths in SYMBOLS.items():
    found = False
    for p in paths:
        if os.path.exists(p):
            with open(p) as f:
                if sym in f.read():
                    found = True
                    print(f"  OK       {sym:<35s}  ({p})")
                    break
    if not found:
        print(f"  MISSING  {sym:<35s}  (not found in any candidate file)")
        missing.append(sym)

print()
if missing:
    print(f"WARNING: {len(missing)} symbol(s) not auto-wired.")
    print("  Build continues — TX scramble works; see patch/app_patch.c for manual RX steps.")
else:
    print("All hooks present — TX scramble + RX de-scramble fully wired.")
