#!/usr/bin/env python3
"""patch_cmake.py -- Append Messenger/Scrambler options to Fusion's CMakeLists.txt.
Detects the actual add_executable target name and source directory layout.
"""
import re, sys, os

path = "firmware/CMakeLists.txt"

with open(path) as f:
    src = f.read()

if "ENABLE_MESSENGER" in src:
    print("CMakeLists.txt already patched -- skipping")
    sys.exit(0)

# ---- Detect executable target name ----------------------------------------
# Fusion uses 'f4hwn'; other forks may differ.
m = re.search(r'add_executable\(\s*(\w+)', src)
if m:
    target = m.group(1)
    print(f"Detected target: '{target}'")
else:
    m2 = re.search(r'project\(\s*(\w+)', src)
    target = m2.group(1) if m2 else "f4hwn"
    print(f"Fallback target: '{target}'")

# ---- Detect source directory (app/ vs App/) --------------------------------
# Check what the existing CMakeLists.txt uses for its own sources
if re.search(r'\bapp/', src, re.IGNORECASE):
    # See which case is actually used
    if 'app/' in src and 'App/' not in src:
        src_prefix = "app"
    elif 'App/' in src and 'app/' not in src:
        src_prefix = "App"
    else:
        src_prefix = "app"  # default to lowercase
else:
    src_prefix = "app"

# Also detect Helper vs helper
if 'Helper/' in src:
    helper_prefix = "Helper"
elif 'helper/' in src:
    helper_prefix = "helper"
else:
    helper_prefix = "Helper"

print(f"Source prefix: '{src_prefix}/'   Helper prefix: '{helper_prefix}/'")

addition = (
    "\n"
    "# ---- Messenger port additions ----------------------------------------\n"
    'option(ENABLE_MESSENGER          "FSK SMS Messenger"                   OFF)\n'
    'option(ENABLE_MESSENGER_FSK_MUTE "Mute audio during FSK packet RX"    ON)\n'
    'option(ENABLE_ENCRYPTION         "ChaCha20-256 encryption"            OFF)\n'
    'option(ENABLE_SCRAMBLER          "Voice frequency-inversion scrambler" OFF)\n'
    "\n"
    "if(ENABLE_MESSENGER)\n"
    f"    target_compile_definitions({target} PRIVATE ENABLE_MESSENGER=1)\n"
    f"    target_sources({target} PRIVATE\n"
    f"        {src_prefix}/messenger.c\n"
    f"        {src_prefix}/bk4819_fsk.c\n"
    f"        {helper_prefix}/fsk.c\n"
    "    )\n"
    "    if(ENABLE_MESSENGER_FSK_MUTE)\n"
    f"        target_compile_definitions({target} PRIVATE ENABLE_MESSENGER_FSK_MUTE=1)\n"
    "    endif()\n"
    "    if(ENABLE_ENCRYPTION)\n"
    f"        target_compile_definitions({target} PRIVATE ENABLE_ENCRYPTION=1)\n"
    f"        target_sources({target} PRIVATE {helper_prefix}/crypto.c)\n"
    "    endif()\n"
    f"    target_link_libraries({target} m)\n"
    "endif()\n"
    "\n"
    "if(ENABLE_SCRAMBLER)\n"
    f"    target_compile_definitions({target} PRIVATE ENABLE_SCRAMBLER=1 SCRAMBLER_IMPL=1)\n"
    f"    target_link_libraries({target} m)\n"
    "endif()\n"
    "# -----------------------------------------------------------------------\n"
)

with open(path, "a") as f:
    f.write(addition)

print(f"CMakeLists.txt patched OK (target='{target}', src='{src_prefix}/', helper='{helper_prefix}/')")
with open(path) as f:
    lines = f.readlines()
print("Last 25 lines:")
print("".join(lines[-25:]))
