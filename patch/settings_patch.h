/* settings_patch.h — EEPROM settings extensions for Messenger + Scrambler
 * ==========================================================================
 * HOW TO APPLY:
 *   1. Open  target/App/settings.h  in your fork
 *   2. Find  typedef struct { ... } t_Settings;
 *   3. Add the fields below at the END of the struct, before the closing }
 *   4. Bump EEPROM_CONFIG_VERSION by +1 (forces re-init on first flash)
 *   5. Add default values in SETTINGS_LoadDefaults() — see defaults below
 * ==========================================================================
 */

/* ---------- Paste into t_Settings struct (at the END) --------------------- */

#ifdef ENABLE_MESSENGER
    uint8_t  MsgModulation;      /* FskModulation_t: 0=FSK450 1=FSK700 2=AFSK1.2K */
    uint8_t  MsgEnc;             /* 0 = plaintext, 1 = ChaCha20 encrypted          */
    uint8_t  MsgRx;              /* 0 = FSK modem off, 1 = listen for messages      */
    uint8_t  MsgAck;             /* 0 = off, 1 = auto-send "ACK" on receipt         */
    uint8_t  EncKey[32];         /* ChaCha20 256-bit shared key                     */
#endif

#ifdef ENABLE_SCRAMBLER
    uint8_t  ScramblerMode;      /* ScramblerMode_t: 0=off 1..4=inversion freq      */
#endif

/* ---------- Paste into SETTINGS_LoadDefaults() ----------------------------- */
/*
    #ifdef ENABLE_MESSENGER
        s->MsgModulation = FSK_MOD_700;   // medium conditions default
        s->MsgEnc        = 0;
        s->MsgRx         = 1;             // listen by default
        s->MsgAck        = 0;
        memset(s->EncKey, 0, sizeof(s->EncKey));
        // Set a non-zero default key so user has something to start with:
        s->EncKey[0] = 0x55; s->EncKey[1] = 0xAA;
    #endif
    #ifdef ENABLE_SCRAMBLER
        s->ScramblerMode = SCRAMBLER_OFF;
    #endif
*/

/* ---------- Bump EEPROM version ------------------------------------------- */
/* Find in settings.h:
 *   #define EEPROM_CONFIG_VERSION  N
 * Change to:
 *   #define EEPROM_CONFIG_VERSION  (N + 1)
 */
