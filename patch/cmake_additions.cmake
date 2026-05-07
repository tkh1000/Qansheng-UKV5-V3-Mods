# cmake_additions.cmake — Paste this block into the target repo's CMakeLists.txt
# =============================================================================
# HOW TO APPLY:
#   1. Open  target/CMakeLists.txt
#   2. Find the section where options like ENABLE_SPECTRUM, ENABLE_NOAA are defined
#   3. Paste the OPTION block below next to those options
#   4. Find where target_sources() is called (or where .c files are listed)
#   5. Paste the target_sources / target_compile_definitions blocks there
# =============================================================================

# ---- Feature options --------------------------------------------------------
option(ENABLE_MESSENGER
    "Include FSK SMS Messenger (requires BK4819 FSK modem)"
    OFF)

option(ENABLE_MESSENGER_FSK_MUTE
    "Mute received audio while FSK packet is being received"
    ON)

option(ENABLE_ENCRYPTION
    "ChaCha20-256 message encryption for Messenger (requires ENABLE_MESSENGER)"
    OFF)

option(ENABLE_SCRAMBLER
    "Frequency-inversion voice scrambler (requires ADC audio access)"
    OFF)

# ---- Conditional source files -----------------------------------------------
if(ENABLE_MESSENGER)
    message(STATUS "  [+] Messenger FSK enabled")

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ENABLE_MESSENGER=1
    )

    target_sources(${PROJECT_NAME} PRIVATE
        # New files from this port
        App/messenger.c
        App/bk4819_fsk.c
        Helper/fsk.c
    )

    if(ENABLE_MESSENGER_FSK_MUTE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            ENABLE_MESSENGER_FSK_MUTE=1
        )
        message(STATUS "  [+] Messenger FSK audio mute enabled")
    endif()
endif()

if(ENABLE_ENCRYPTION)
    if(NOT ENABLE_MESSENGER)
        message(WARNING "ENABLE_ENCRYPTION requires ENABLE_MESSENGER — ignoring")
    else()
        message(STATUS "  [+] ChaCha20 encryption enabled")
        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_ENCRYPTION=1)
        target_sources(${PROJECT_NAME} PRIVATE Helper/crypto.c)
        # Link math library for scrambler sine LUT (needed if sinf() is used)
        target_link_libraries(${PROJECT_NAME} m)
    endif()
endif()

if(ENABLE_SCRAMBLER)
    message(STATUS "  [+] Voice scrambler enabled")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ENABLE_SCRAMBLER=1
        SCRAMBLER_IMPL=1   # activates implementation in scrambler.h
    )
    # scrambler.h contains both declaration and implementation guarded by SCRAMBLER_IMPL
    # Alternatively split into scrambler.c — then add:
    # target_sources(${PROJECT_NAME} PRIVATE App/scrambler.c)
    target_link_libraries(${PROJECT_NAME} m)   # for sinf()
endif()

# ---- CMakePresets additions (paste into CMakePresets.json) ------------------
# Add these preset objects to the "buildPresets" (or "configurePresets") array:
#
# {
#   "name": "Fusion-Messenger",
#   "displayName": "UV-K5 V3 / K1 — Messenger + Encryption",
#   "inherits": "Fusion",
#   "cacheVariables": {
#     "ENABLE_MESSENGER":          {"type": "BOOL", "value": "ON"},
#     "ENABLE_MESSENGER_FSK_MUTE": {"type": "BOOL", "value": "ON"},
#     "ENABLE_ENCRYPTION":         {"type": "BOOL", "value": "ON"},
#     "ENABLE_SCRAMBLER":          {"type": "BOOL", "value": "OFF"}
#   }
# },
# {
#   "name": "Fusion-Messenger-Scrambler",
#   "displayName": "UV-K5 V3 / K1 — Messenger + Encryption + Scrambler",
#   "inherits": "Fusion-Messenger",
#   "cacheVariables": {
#     "ENABLE_SCRAMBLER": {"type": "BOOL", "value": "ON"}
#   }
# }
