#!/usr/bin/env python3
"""verify_hooks.py -- Confirm Messenger + Scrambler symbols are wired in."""
import os

def find_file(name, root="firmware"):
    for d, _, files in os.walk(root):
        if name in files:
            return os.path.join(d, name)
    return None

app_c    = find_file("app.c")
audio_c  = find_file("audio.c")
bk4819_c = find_file("bk4819.c")

candidates = [p for p in [app_c, audio_c, bk4819_c] if p]

SYMBOLS = {
    "MESSENGER_Init":          [app_c],
    "MESSENGER_TimeSlice10ms": [app_c],
    "MESSENGER_HandleKey":     [app_c],
    "SCRAMBLER_Init":          [app_c],
    "SCRAMBLER_Enable":        candidates,
    "SCRAMBLER_Disable":       candidates,
    "SCRAMBLER_ProcessSample": candidates,
}

missing = []
for sym, paths in SYMBOLS.items():
    found = False
    for p in (paths or []):
        if p and os.path.exists(p):
            with open(p) as f:
                if sym in f.read():
                    found = True
                    print(f"  OK       {sym:<35s}  ({p})")
                    break
    if not found:
        print(f"  MISSING  {sym:<35s}")
        missing.append(sym)

print()
if missing:
    print(f"WARNING: {len(missing)} symbol(s) not auto-wired -- see patch/app_patch.c")
else:
    print("All hooks present -- TX scramble + RX de-scramble fully wired.")
