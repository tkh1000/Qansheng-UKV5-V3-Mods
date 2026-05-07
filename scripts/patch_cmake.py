#!/usr/bin/env python3
"""patch_cmake.py — Append Messenger/Scrambler options to Fusion's CMakeLists.txt"""
import sys

path = "firmware/CMakeLists.txt"

with open(path) as f:
    src = f.read()

if "ENABLE_MESSENGER" in src:
    print("CMakeLists.txt already patched — skipping")
    sys.exit(0)

addition = (
    "\n"
    "# ---- Messenger port additions ----------------------------------------\n"
    'option(ENABLE_MESSENGER          "FSK SMS Messenger"                   OFF)\n'
    'option(ENABLE_MESSENGER_FSK_MUTE "Mute audio during FSK packet RX"    ON)\n'
    'option(ENABLE_ENCRYPTION         "ChaCha20-256 encryption"            OFF)\n'
    'option(ENABLE_SCRAMBLER          "Voice frequency-inversion scrambler" OFF)\n'
    "\n"
    "if(ENABLE_MESSENGER)\n"
    "    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_MESSENGER=1)\n"
    "    target_sources(${PROJECT_NAME} PRIVATE\n"
    "        App/messenger.c\n"
    "        App/bk4819_fsk.c\n"
    "        Helper/fsk.c\n"
    "    )\n"
    "    if(ENABLE_MESSENGER_FSK_MUTE)\n"
    "        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_MESSENGER_FSK_MUTE=1)\n"
    "    endif()\n"
    "    if(ENABLE_ENCRYPTION)\n"
    "        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_ENCRYPTION=1)\n"
    "        target_sources(${PROJECT_NAME} PRIVATE Helper/crypto.c)\n"
    "    endif()\n"
    "    target_link_libraries(${PROJECT_NAME} m)\n"
    "endif()\n"
    "\n"
    "if(ENABLE_SCRAMBLER)\n"
    "    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SCRAMBLER=1 SCRAMBLER_IMPL=1)\n"
    "    target_link_libraries(${PROJECT_NAME} m)\n"
    "endif()\n"
    "# -----------------------------------------------------------------------\n"
)

with open(path, "a") as f:
    f.write(addition)

print("CMakeLists.txt patched OK")
with open(path) as f:
    lines = f.readlines()
print("Last 20 lines:")
print("".join(lines[-20:]))
