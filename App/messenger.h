/* messenger.h — SMS-style FSK Messenger for UV-K5 V3 / UV-K1
 * Ported from kamilsss655/uv-k5-firmware-custom (DP32G030)
 * Target: armel/uv-k1-k5v3-firmware-custom (PY32F071)
 *
 * Compile-time gates:
 *   ENABLE_MESSENGER     — include entire messenger subsystem
 *   ENABLE_ENCRYPTION    — include ChaCha20 encryption support
 *   ENABLE_MESSENGER_FSK_MUTE — mute audio while FSK packet RX in progress
 */
#ifndef MESSENGER_H
#define MESSENGER_H

#ifdef ENABLE_MESSENGER

#include <stdint.h>
#include <stdbool.h>
#include "Helper/fsk.h"

/* ---- Configuration -------------------------------------------------------- */

#define MSG_HISTORY_MAX   5    /* number of messages kept in history ring     */
#define MSG_INPUT_MAX     FSK_MSG_MAX_LEN
#define MSG_DISPLAY_ROWS  4    /* text rows on screen (below header)          */
#define MSG_DISPLAY_COLS  14   /* characters per row at 6×8 px font           */

/* Keyboard input modes */
typedef enum {
    KB_MODE_UPPER  = 0,
    KB_MODE_LOWER  = 1,
    KB_MODE_NUMERIC= 2,
    KB_MODE_COUNT
} KbMode_t;

/* Message direction */
typedef enum {
    MSG_DIR_RX = 0,
    MSG_DIR_TX = 1,
} MsgDir_t;

/* One stored message */
typedef struct {
    char     text[MSG_INPUT_MAX + 1];  /* null-terminated */
    MsgDir_t dir;
    bool     valid;
} MsgEntry_t;

/* Global messenger state (placed in BSS — zero-init on boot) */
typedef struct {
    MsgEntry_t history[MSG_HISTORY_MAX];
    uint8_t    history_head;           /* ring buffer head */
    uint8_t    history_count;

    char       input[MSG_INPUT_MAX + 1]; /* current compose buffer */
    uint8_t    input_len;
    KbMode_t   kb_mode;

    bool       open;                   /* messenger screen active */
    bool       pending_rx;             /* new message received, not viewed */
    bool       tx_in_progress;
    bool       rx_in_progress;

    FskPacket_t rx_packet;             /* scratch buffer for incoming packet */
    uint8_t     rx_byte_count;         /* bytes received so far              */
} MessengerState_t;

extern MessengerState_t g_Messenger;

/* ---- Public API ----------------------------------------------------------- */

/** Call once from main init. Sets up BK4819 FSK RX if MsgRx setting is on. */
void MESSENGER_Init(void);

/** Open the messenger UI screen. Call from action handler. */
void MESSENGER_Show(void);

/** Close the messenger UI screen without transmitting. */
void MESSENGER_Hide(void);

/** Process a key press while the messenger screen is active.
 *  @return true if key was consumed by messenger (don't pass to main handler) */
bool MESSENGER_HandleKey(uint8_t Key, bool bKeyPressed, bool bKeyHeld);

/** Transmit the current compose buffer over the active VFO.
 *  Handles encryption if enabled and MsgEnc setting is on. */
void MESSENGER_Transmit(void);

/** Called every 10 ms from the main time-slice loop.
 *  Polls BK4819 FSK interrupt flag and processes incoming bytes. */
void MESSENGER_TimeSlice10ms(void);

/** Render the messenger screen. Call from UI update loop when messenger is open. */
void MESSENGER_DrawScreen(void);

/** Draw the envelope notification icon in the status bar.
 *  Call from the main status bar draw function when pending_rx is set. */
void MESSENGER_DrawStatusIcon(void);

/* ---- BK4819 FSK wrappers (implemented in bk4819_fsk.c) ------------------- */
void MESSENGER_BK4819_SetupTX(FskModulation_t mod, bool wide_channel);
void MESSENGER_BK4819_SetupRX(FskModulation_t mod, bool wide_channel);
void MESSENGER_BK4819_TxPacket(const FskPacket_t *pkt);
void MESSENGER_BK4819_DisableFSK(void);

#endif /* ENABLE_MESSENGER */
#endif /* MESSENGER_H */
