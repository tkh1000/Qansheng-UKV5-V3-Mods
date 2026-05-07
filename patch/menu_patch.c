/* menu_patch.c — Menu integration for Messenger + Scrambler
 * ==========================================================================
 * HOW TO APPLY:
 *   These snippets go into Fusion's  App/menu.c  and  App/menu.h
 *   Search for existing menu items like "BatSave", "TxPwr", etc. to find
 *   the correct insertion points.
 * ==========================================================================
 */

/* ---- 1. In App/menu.h — add to the menu ID enum ------------------------- */
/*
    #ifdef ENABLE_MESSENGER
        MENU_MSGMOD,    // FSK modulation selection
        MENU_MSGRX,     // enable/disable FSK RX listener
        MENU_MSGACK,    // auto-ACK on receive
        MENU_MSGENC,    // encryption on/off
        MENU_ENCKEY,    // encryption key (hex entry)
    #endif
    #ifdef ENABLE_SCRAMBLER
        MENU_SCRAMBL,   // scrambler mode selection
    #endif
*/

/* ---- 2. In App/menu.c — add to the menu item name table ------------------ */
/* Find the array like:  static const char MenuNames[][7] = { ... };
   Add at the end (before the terminator):
*/
/*
    #ifdef ENABLE_MESSENGER
        "MsgMod",
        "MsgRx",
        "MsgAck",
        "MsgEnc",
        "EncKey",
    #endif
    #ifdef ENABLE_SCRAMBLER
        "Scrambl",
    #endif
*/

/* ---- 3. In MENU_ShowCurrentSetting() — add render cases ------------------ */
/*
    #ifdef ENABLE_MESSENGER
    case MENU_MSGMOD: {
        static const char * const mod_labels[] = {"FSK450","FSK700","AFSK1k2"};
        uint8_t m = g_eeprom.Settings.MsgModulation;
        if (m >= 3) m = 0;
        strcpy(String, mod_labels[m]);
        break;
    }
    case MENU_MSGRX:
        strcpy(String, g_eeprom.Settings.MsgRx  ? "ON" : "OFF");
        break;
    case MENU_MSGACK:
        strcpy(String, g_eeprom.Settings.MsgAck ? "ON" : "OFF");
        break;
    case MENU_MSGENC:
        strcpy(String, g_eeprom.Settings.MsgEnc ? "ON" : "OFF");
        break;
    case MENU_ENCKEY:
        // Show first 4 bytes of key as hex
        sprintf(String, "%02X%02X%02X%02X",
                g_eeprom.Settings.EncKey[0], g_eeprom.Settings.EncKey[1],
                g_eeprom.Settings.EncKey[2], g_eeprom.Settings.EncKey[3]);
        break;
    #endif

    #ifdef ENABLE_SCRAMBLER
    case MENU_SCRAMBL: {
        static const char * const scr_labels[] = {"OFF","2000","2500","2700","3000"};
        uint8_t m = g_eeprom.Settings.ScramblerMode;
        if (m >= SCRAMBLER_COUNT) m = 0;
        strcpy(String, scr_labels[m]);
        break;
    }
    #endif
*/

/* ---- 4. In MENU_AcceptSetting() — add save cases ------------------------- */
/*
    #ifdef ENABLE_MESSENGER
    case MENU_MSGMOD:
        g_eeprom.Settings.MsgModulation = (uint8_t)(g_MenuCursor % 3);
        // Re-configure FSK RX with new modulation
        if (g_eeprom.Settings.MsgRx) {
            MESSENGER_BK4819_DisableFSK();
            MESSENGER_BK4819_SetupRX(
                (FskModulation_t)g_eeprom.Settings.MsgModulation,
                IsWideChannel());
        }
        break;
    case MENU_MSGRX:
        g_eeprom.Settings.MsgRx = (uint8_t)(g_MenuCursor & 1);
        if (g_eeprom.Settings.MsgRx)
            MESSENGER_BK4819_SetupRX(GetModulation(), IsWideChannel());
        else
            MESSENGER_BK4819_DisableFSK();
        break;
    case MENU_MSGACK:
        g_eeprom.Settings.MsgAck = (uint8_t)(g_MenuCursor & 1);
        break;
    case MENU_MSGENC:
        g_eeprom.Settings.MsgEnc = (uint8_t)(g_MenuCursor & 1);
        break;
    // ENCKEY editing is handled by a special sub-screen — see messenger.c
    #endif

    #ifdef ENABLE_SCRAMBLER
    case MENU_SCRAMBL:
        g_eeprom.Settings.ScramblerMode = (uint8_t)(g_MenuCursor % SCRAMBLER_COUNT);
        SCRAMBLER_SetMode((ScramblerMode_t)g_eeprom.Settings.ScramblerMode);
        break;
    #endif
*/

/* ---- 5. Save to EEPROM --------------------------------------------------- */
/* After changing any setting, call:
 *   SETTINGS_SaveSettings();
 * (Fusion already does this in MENU_AcceptSetting for all menu items)
 */
