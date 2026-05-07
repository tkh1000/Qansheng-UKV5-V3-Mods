#!/usr/bin/env python3
"""patch_cmake.py -- Append Messenger/Scrambler options to Fusion's CMakeLists.txt.

Source and helper directory prefixes are derived from the probe step's
FW_SRC_DIR / FW_HELPER_DIR env vars so they always match where files
were actually copied, rather than guessing from CMakeLists content.
"""
import re, sys, os

path = "firmware/CMakeLists.txt"

with open(path) as f:
    src = f.read()

if "ENABLE_MESSENGER" in src:
    print("CMakeLists.txt already patched -- skipping")
    sys.exit(0)

# ---- Target name -----------------------------------------------------------
# Fusion uses add_executable(${EXE_NAME} ...) where EXE_NAME is a cache var.
# We write ${EXE_NAME} so it resolves correctly at configure time.
m = re.search(r'add_executable\(\s*(\$\{)?(\w+)\}?', src)
if m and m.group(1):                      # found ${VAR} form
    target_ref = "${" + m.group(2) + "}"
elif m:                                   # found literal name
    target_ref = m.group(2)
else:
    target_ref = "${EXE_NAME}"            # Fusion's known variable
print(f"Target reference: '{target_ref}'")

# ---- Source and helper prefixes --------------------------------------------
# Prefer the probe-detected paths (set by probe_structure.py via GITHUB_ENV).
# Fall back to scanning CMakeLists content if not available.

fw_src_dir = os.environ.get("FW_SRC_DIR", "")         # e.g. firmware/app
fw_helper_dir = os.environ.get("FW_HELPER_DIR", "")   # e.g. firmware/Helper

if fw_src_dir:
    # Make relative to firmware/ — that's what CMakeLists.txt paths need
    src_prefix = os.path.relpath(fw_src_dir, "firmware")
    print(f"Source prefix from FW_SRC_DIR: '{src_prefix}'")
else:
    # Scan CMakeLists for existing source references
    if re.search(r'\bapp/', src):
        src_prefix = "app"
    elif re.search(r'\bApp/', src):
        src_prefix = "App"
    else:
        src_prefix = "app"
    print(f"Source prefix from scan: '{src_prefix}'")

if fw_helper_dir:
    helper_prefix = os.path.relpath(fw_helper_dir, "firmware")
    print(f"Helper prefix from FW_HELPER_DIR: '{helper_prefix}'")
else:
    if "Helper/" in src:
        helper_prefix = "Helper"
    elif "helper/" in src:
        helper_prefix = "helper"
    else:
        helper_prefix = "Helper"
    print(f"Helper prefix from scan: '{helper_prefix}'")

print(f"src='{src_prefix}/'  helper='{helper_prefix}/'")

# ---- Append the patch ------------------------------------------------------
addition = (
    "\n"
    "# ---- Messenger port additions ----------------------------------------\n"
    'option(ENABLE_MESSENGER          "FSK SMS Messenger"                   OFF)\n'
    'option(ENABLE_MESSENGER_FSK_MUTE "Mute audio during FSK packet RX"    ON)\n'
    'option(ENABLE_ENCRYPTION         "ChaCha20-256 encryption"            OFF)\n'
    'option(ENABLE_SCRAMBLER          "Voice frequency-inversion scrambler" OFF)\n'
    "\n"
    "if(ENABLE_MESSENGER)\n"
    f"    target_include_directories({target_ref} PRIVATE\n"
    f"        ${{CMAKE_SOURCE_DIR}}/{helper_prefix}\n"
    "    )\n"
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

print(f"CMakeLists.txt patched OK")
with open(path) as f:
    lines = f.readlines()
print("Last 25 lines:")
print("".join(lines[-25:]))
