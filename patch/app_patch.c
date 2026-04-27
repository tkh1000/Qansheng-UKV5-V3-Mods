/* app_patch.c — Integration hooks for Messenger into Fusion's app.c
 * ==========================================================================
 * HOW TO APPLY:
 *   These snippets go into Fusion's  App/app.c  and  App/action.c
 *   Instructions show exactly where each piece goes.
 * ==========================================================================
 */

/* ---- 1. In app.c: APP_Init() --------------------------------------------- */
/* After all other init calls, before the main loop: */
/*
    #ifdef ENABLE_MESSENGER
        MESSENGER_Init();
    #endif
    #ifdef ENABLE_SCRAMBLER
        SCRAMBLER_Init((ScramblerMode_t)g_eeprom.Settings.ScramblerMode);
    #endif
*/

/* ---- 2. In app.c: APP_TimeSlice10ms() ------------------------------------ */
/* Inside the 10ms time-slice function, after existing periodic tasks: */
/*
    #ifdef ENABLE_MESSENGER
        MESSENGER_TimeSlice10ms();
    #endif
*/

/* ---- 3. In app.c: APP_HandleKey() or equivalent -------------------------- */
/* At the TOP of the key handler, before the main switch/case: */
/*
    #ifdef ENABLE_MESSENGER
        if (MESSENGER_HandleKey(Key, bKeyPressed, bKeyHeld))
            return;   // messenger consumed the key
    #endif
*/

/* ---- 4. In action.c: ACTION_Messenger ------------------------------------ */
/* Add a new action ID in the action enum (App/action.h):
 *   ACTION_MESSENGER,
 *
 * Then in ACTION_DoAction():
 */
/*
    #ifdef ENABLE_MESSENGER
    case ACTION_MESSENGER:
        MESSENGER_Show();
        break;
    #endif
*/

/* ---- 5. Bind the action to F+MENU key combo ------------------------------ */
/* In app.c or keyboard.c, find the combo key handler.
 * Fusion uses a function/side-key action table. Locate the
 * "FuncKey" handling and add:
 */
/*
    // F + MENU = open messenger
    if (Key == KEY_MENU && bFKeyHeld) {
        #ifdef ENABLE_MESSENGER
            ACTION_DoAction(ACTION_MESSENGER);
        #endif
        return;
    }
*/

/* ---- 6. In ui/status.c (or wherever the status bar is drawn) ------------- */
/* At the end of the status bar draw function: */
/*
    #ifdef ENABLE_MESSENGER
        MESSENGER_DrawStatusIcon();
    #endif
*/

/* ---- 7. In app.c: display update loop ------------------------------------ */
/* When deciding what screen to draw: */
/*
    #ifdef ENABLE_MESSENGER
    if (g_Messenger.open) {
        MESSENGER_DrawScreen();
        return;
    }
    #endif
*/

/* ---- 8. Scrambler PTT hook ----------------------------------------------- */
/* In app.c, find the PTT press handler: */
/*
    #ifdef ENABLE_SCRAMBLER
    // On PTT press (start TX):
    if (g_eeprom.Settings.ScramblerMode != SCRAMBLER_OFF) {
        SCRAMBLER_Enable();
    }
    // On PTT release (end TX):
    SCRAMBLER_Disable();
    #endif
*/

/* ---- 9. Scrambler ADC/audio hook ----------------------------------------- */
/* If Fusion processes audio samples in an ISR or DMA callback (check
 * App/audio.c or similar), insert:
 */
/*
    #ifdef ENABLE_SCRAMBLER
    // In the audio sample callback / ADC DMA complete ISR:
    extern int16_t SCRAMBLER_ProcessSample(uint16_t raw_adc);
    uint16_t raw = HAL_ADC_GetValue(&hadc1);     // PY32F071 12-bit ADC
    int16_t  out = SCRAMBLER_ProcessSample(raw); // process
    // Write 'out' back to BK4819 mic input DAC or audio output path
    // (implementation depends on Fusion's audio pipeline)
    #endif
*/
