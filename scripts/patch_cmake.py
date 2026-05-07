#!/usr/bin/env python3
"""patch_cmake.py -- Append Messenger/Scrambler options to Fusion's CMakeLists.txt.

Fusion defines its executable as add_executable(${EXE_NAME} ...) where EXE_NAME
is set earlier in the file. We detect EXE_NAME and the source/helper dir prefixes
from the existing CMakeLists content so the patch is always consistent.
"""
import re, sys, os

path = "firmware/CMakeLists.txt"

with open(path) as f:
    src = f.read()

if "ENABLE_MESSENGER" in src:
    print("CMakeLists.txt already patched -- skipping")
    sys.exit(0)

# ---- Detect the executable variable / target name --------------------------
# Fusion uses: set(EXE_NAME firmware) then add_executable(${EXE_NAME} ...)
# The correct way to reference it in our appended code is ${EXE_NAME}.

# Check if the CMakeLists uses a variable for the exe name
exe_var_match = re.search(r'set\s*\(\s*(\w+)\s+\w+\s*\)', src)
add_exe_match  = re.search(r'add_executable\(\s*(\$\{)?(\w+)\}?\s', src)

if add_exe_match:
    if add_exe_match.group(1):  # it's a ${VAR} reference
        target_ref = "${" + add_exe_match.group(2) + "}"
    else:
        target_ref = add_exe_match.group(2)  # literal name like f4hwn
else:
    # Fallback: use ${EXE_NAME} which is Fusion's known variable
    target_ref = "${EXE_NAME}"

print(f"Using target reference: '{target_ref}'")

# ---- Detect source and helper directory prefixes ---------------------------
# Read the CMakeLists to see what case it uses for source paths
src_prefix    = "app"   if "app/"    in src else "App"
helper_prefix = "Helper" if "Helper/" in src else ("helper" if "helper/" in src else "Helper")

print(f"Source prefix: '{src_prefix}/'  Helper prefix: '{helper_prefix}/'")

# ---- Build the addition ----------------------------------------------------
addition = (
    "\n"
    "# ---- Messenger port additions ----------------------------------------\n"
    'option(ENABLE_MESSENGER          "FSK SMS Messenger"                   OFF)\n'
    'option(ENABLE_MESSENGER_FSK_MUTE "Mute audio during FSK packet RX"    ON)\n'
    'option(ENABLE_ENCRYPTION         "ChaCha20-256 encryption"            OFF)\n'
    'option(ENABLE_SCRAMBLER          "Voice frequency-inversion scrambler" OFF)\n'
    "\n"
    "if(ENABLE_MESSENGER)\n"
    f"    target_compile_definitions({target_ref} PRIVATE ENABLE_MESSENGER=1)\n"
    f"    target_sources({target_ref} PRIVATE\n"
    f"        {src_prefix}/messenger.c\n"
    f"        {src_prefix}/bk4819_fsk.c\n"
    f"        {helper_prefix}/fsk.c\n"
    "    )\n"
    "    if(ENABLE_MESSENGER_FSK_MUTE)\n"
    f"        target_compile_definitions({target_ref} PRIVATE ENABLE_MESSENGER_FSK_MUTE=1)\n"
    "    endif()\n"
    "    if(ENABLE_ENCRYPTION)\n"
    f"        target_compile_definitions({target_ref} PRIVATE ENABLE_ENCRYPTION=1)\n"
    f"        target_sources({target_ref} PRIVATE {helper_prefix}/crypto.c)\n"
    "    endif()\n"
    f"    target_link_libraries({target_ref} m)\n"
    "endif()\n"
    "\n"
    "if(ENABLE_SCRAMBLER)\n"
    f"    target_compile_definitions({target_ref} PRIVATE ENABLE_SCRAMBLER=1 SCRAMBLER_IMPL=1)\n"
    f"    target_link_libraries({target_ref} m)\n"
    "endif()\n"
    "# -----------------------------------------------------------------------\n"
)

with open(path, "a") as f:
    f.write(addition)

print(f"CMakeLists.txt patched (target='{target_ref}')")
with open(path) as f:
    lines = f.readlines()
print("Last 25 lines:")
print("".join(lines[-25:]))
