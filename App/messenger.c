/* messenger.c — FSK SMS Messenger for UV-K5 V3 / UV-K1
 * Ported from kamilsss655/uv-k5-firmware-custom (DP32G030)
 * Target: armel/uv-k1-k5v3-firmware-custom (PY32F071)
 *
 * HAL notes (V3 vs V1 differences handled here):
 *  - BK4819 SPI: identical chip, use Fusion's BK4819_WriteRegister / ReadRegister
 *  - UI/display: identical ST7565 LCD, same Fusion draw primitives
 *  - Keyboard: same key constants (KEY_0..KEY_9, KEY_STAR, KEY_F, KEY_MENU, KEY_EXIT)
 *  - Settings: read from Fusion's g_eeprom.Settings struct (extended in settings.h)
 *  - No direct MCU register access in this file.
 */

#ifdef ENABLE_MESSENGER

#include "messenger.h"
#include "Helper/fsk.h"
#ifdef ENABLE_ENCRYPTION
#include "Helper/crypto.h"
#endif

/* Fusion headers — adjust paths if Fusion's include layout differs */
#include "App/bk4819.h"          /* BK4819_WriteRegister, BK4819_ReadRegister  */
#include "App/settings.h"        /* g_eeprom, t_Settings                       */
#include "App/keyboard.h"        /* KEY_* constants                            */
#include "App/uart.h"            /* UART_printf (debug, optional)              */
#include "UI/ui.h"               /* UI_PrintStringXY, UI_DisplayClear, etc.    */
#include "driver/st7565.h"       /* ST7565_BlitFullScreen                      */

#include <string.h>
#include <stdio.h>

/* ---- Global state --------------------------------------------------------- */
MessengerState_t g_Messenger;

/* ---- T9 / multi-tap character maps --------------------------------------- */

/* Each entry: characters cycled through for that key (multi-tap) */
static const char * const KB_UPPER[] = {
    " 0",      /* 0 */
    "1.,?!",   /* 1 */
    "ABC2@",   /* 2 */
    "DEF3",    /* 3 */
    "GHI4",    /* 4 */
    "JKL5",    /* 5 */
    "MNO6",    /* 6 */
    "PQRS7",   /* 7 */
    "TUV8",    /* 8 */
    "WXYZ9",   /* 9 */
};

static const char * const KB_LOWER[] = {
    " 0",      /* 0 */
    "1.,?!",   /* 1 */
    "abc2@",   /* 2 */
    "def3",    /* 3 */
    "ghi4",    /* 4 */
    "jkl5",    /* 5 */
    "mno6",    /* 6 */
    "pqrs7",   /* 7 */
    "tuv8",    /* 8 */
    "wxyz9",   /* 9 */
};

/* In numeric mode, key N types the digit N */

/* Last key pressed (for multi-tap timing) */
static uint8_t  s_last_digit_key  = 0xFF;
static uint8_t  s_last_digit_idx  = 0;

/* ---- Internal helpers ----------------------------------------------------- */

static bool IsWideChannel(void)
{
    /* Fusion stores bandwidth in g_eeprom.VfoInfo[].bwMode or similar.
     * Adjust the field name to match Fusion's actual settings struct. */
    return (g_eeprom.VfoInfo[g_eeprom.TX_VFO].CHANNEL_BANDWIDTH == 0);
    /* 0 = wide (25 kHz), 1 = narrow (12.5 kHz) in Fusion */
}

static FskModulation_t GetModulation(void)
{
    return (FskModulation_t)g_eeprom.Settings.MsgModulation;
}

static void PushHistory(const char *text, MsgDir_t dir)
{
    MsgEntry_t *e = &g_Messenger.history[g_Messenger.history_head];
    strncpy(e->text, text, MSG_INPUT_MAX);
    e->text[MSG_INPUT_MAX] = '\0';
    e->dir   = dir;
    e->valid = true;

    g_Messenger.history_head = (g_Messenger.history_head + 1) % MSG_HISTORY_MAX;
    if (g_Messenger.history_count < MSG_HISTORY_MAX)
        g_Messenger.history_count++;
}

/* ---- Public API ----------------------------------------------------------- */

void MESSENGER_Init(void)
{
    memset(&g_Messenger, 0, sizeof(g_Messenger));

    if (g_eeprom.Settings.MsgRx) {
        MESSENGER_BK4819_SetupRX(GetModulation(), IsWideChannel());
    }
}

void MESSENGER_Show(void)
{
    g_Messenger.open = true;
    g_Messenger.pending_rx = false;
    MESSENGER_DrawScreen();
}

void MESSENGER_Hide(void)
{
    g_Messenger.open = false;
}

/* ---- Keyboard input ------------------------------------------------------- */

/* Return the character for a given digit key + tap count in the current mode */
static char GetCharForKey(uint8_t digit, uint8_t tap_idx, KbMode_t mode)
{
    if (mode == KB_MODE_NUMERIC)
        return '0' + digit;

    const char *map = (mode == KB_MODE_UPPER) ? KB_UPPER[digit] : KB_LOWER[digit];
    uint8_t map_len = (uint8_t)strlen(map);
    return map[tap_idx % map_len];
}

bool MESSENGER_HandleKey(uint8_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (!g_Messenger.open) return false;
    if (!bKeyPressed)      return true;   /* consume key-up events silently */

    /* --- F (long press) = clear all messages --- */
    if (Key == KEY_F && bKeyHeld) {
        memset(&g_Messenger, 0, sizeof(g_Messenger));
        g_Messenger.open = true;
        MESSENGER_DrawScreen();
        return true;
    }

    /* --- F (short press) = backspace --- */
    if (Key == KEY_F) {
        if (g_Messenger.input_len > 0)
            g_Messenger.input[--g_Messenger.input_len] = '\0';
        s_last_digit_key = 0xFF;
        MESSENGER_DrawScreen();
        return true;
    }

    /* --- STAR = cycle keyboard mode --- */
    if (Key == KEY_STAR) {
        g_Messenger.kb_mode = (g_Messenger.kb_mode + 1) % KB_MODE_COUNT;
        s_last_digit_key = 0xFF;
        MESSENGER_DrawScreen();
        return true;
    }

    /* --- UP = recall last sent message --- */
    if (Key == KEY_UP) {
        /* Walk history backwards to find a TX entry */
        for (uint8_t i = 1; i <= g_Messenger.history_count; i++) {
            uint8_t idx = (g_Messenger.history_head + MSG_HISTORY_MAX - i) % MSG_HISTORY_MAX;
            if (g_Messenger.history[idx].valid && g_Messenger.history[idx].dir == MSG_DIR_TX) {
                strncpy(g_Messenger.input, g_Messenger.history[idx].text, MSG_INPUT_MAX);
                g_Messenger.input_len = (uint8_t)strlen(g_Messenger.input);
                break;
            }
        }
        s_last_digit_key = 0xFF;
        MESSENGER_DrawScreen();
        return true;
    }

    /* --- MENU = transmit --- */
    if (Key == KEY_MENU) {
        if (g_Messenger.input_len > 0)
            MESSENGER_Transmit();
        return true;
    }

    /* --- EXIT = close messenger --- */
    if (Key == KEY_EXIT) {
        MESSENGER_Hide();
        return true;
    }

    /* --- Digit keys (0-9) = character input --- */
    if (Key <= KEY_9) {
        if (g_Messenger.input_len >= MSG_INPUT_MAX) return true;

        uint8_t digit = (Key == KEY_0) ? 0 : Key;  /* KEY_0..KEY_9 = 0..9 */

        uint8_t tap_idx = 0;
        if (Key == s_last_digit_key && g_Messenger.input_len > 0) {
            /* Same key pressed again — cycle to next character */
            s_last_digit_idx++;
            /* Overwrite last typed character */
            g_Messenger.input_len--;
            tap_idx = s_last_digit_idx;
        } else {
            s_last_digit_idx = 0;
            tap_idx = 0;
        }

        char c = GetCharForKey(digit, tap_idx, g_Messenger.kb_mode);
        g_Messenger.input[g_Messenger.input_len++] = c;
        g_Messenger.input[g_Messenger.input_len]   = '\0';
        s_last_digit_key = Key;

        MESSENGER_DrawScreen();
        return true;
    }

    return false;  /* key not consumed */
}

/* ---- Transmit ------------------------------------------------------------- */

void MESSENGER_Transmit(void)
{
    if (g_Messenger.input_len == 0) return;
    if (g_Messenger.tx_in_progress) return;

    g_Messenger.tx_in_progress = true;

    /* Copy input to payload buffer */
    uint8_t payload[FSK_MSG_MAX_LEN];
    uint8_t payload_len = g_Messenger.input_len;
    memcpy(payload, g_Messenger.input, payload_len);

#ifdef ENABLE_ENCRYPTION
    if (g_eeprom.Settings.MsgEnc) {
        CRYPTO_Encrypt(payload, payload_len, g_eeprom.Settings.EncKey);
    }
#endif

    FskPacket_t pkt;
    if (!FSK_BuildPacket(&pkt, payload, payload_len)) {
        g_Messenger.tx_in_progress = false;
        return;
    }

    /* Set up BK4819 for FSK TX */
    MESSENGER_BK4819_DisableFSK();
    MESSENGER_BK4819_SetupTX(GetModulation(), IsWideChannel());
    MESSENGER_BK4819_TxPacket(&pkt);
    MESSENGER_BK4819_DisableFSK();

    /* Re-enable RX if setting is on */
    if (g_eeprom.Settings.MsgRx)
        MESSENGER_BK4819_SetupRX(GetModulation(), IsWideChannel());

    /* Save to history */
    PushHistory(g_Messenger.input, MSG_DIR_TX);

    /* Clear input */
    memset(g_Messenger.input, 0, sizeof(g_Messenger.input));
    g_Messenger.input_len = 0;
    s_last_digit_key = 0xFF;

    g_Messenger.tx_in_progress = false;
    MESSENGER_DrawScreen();
}

/* ---- Receive (polled from TimeSlice10ms) ----------------------------------- */

/* BK4819 REG_0C interrupt status — bit 14 = FSK FIFO almost full */
#define BK4819_REG_0C_FSK_RX_FLAG (1u << 14)

void MESSENGER_TimeSlice10ms(void)
{
    if (!g_eeprom.Settings.MsgRx) return;
    if (g_Messenger.tx_in_progress) return;

    /* Poll interrupt register */
    uint16_t irq = BK4819_ReadRegister(BK4819_REG_0C);
    if (!(irq & BK4819_REG_0C_FSK_RX_FLAG)) return;

    /* Clear interrupt */
    BK4819_WriteRegister(BK4819_REG_02, 0);

    /* Read bytes from FSK RX FIFO (REG_5B) one at a time */
    uint8_t *dst = (uint8_t *)&g_Messenger.rx_packet;

    while (g_Messenger.rx_byte_count < FSK_PACKET_SIZE) {
        uint16_t fifo_status = BK4819_ReadRegister(BK4819_REG_5C);
        if (!(fifo_status & 0x0001)) break;  /* FIFO empty */

        uint16_t word = BK4819_ReadRegister(BK4819_REG_5B);
        /* High byte first (BK4819 is big-endian on FIFO) */
        if (g_Messenger.rx_byte_count < FSK_PACKET_SIZE)
            dst[g_Messenger.rx_byte_count++] = (uint8_t)(word >> 8);
        if (g_Messenger.rx_byte_count < FSK_PACKET_SIZE)
            dst[g_Messenger.rx_byte_count++] = (uint8_t)(word & 0xFF);
    }

    if (g_Messenger.rx_byte_count < FSK_PACKET_SIZE) return;

    /* Full packet received — validate */
    g_Messenger.rx_byte_count = 0;

    if (!FSK_ValidatePacket(&g_Messenger.rx_packet)) {
        /* Bad CRC — discard */
        MESSENGER_BK4819_DisableFSK();
        MESSENGER_BK4819_SetupRX(GetModulation(), IsWideChannel());
        return;
    }

    /* Decrypt if needed */
    uint8_t payload[FSK_MSG_MAX_LEN];
    uint8_t payload_len = g_Messenger.rx_packet.length;
    memcpy(payload, g_Messenger.rx_packet.payload, payload_len);

#ifdef ENABLE_ENCRYPTION
    if (g_eeprom.Settings.MsgEnc) {
        CRYPTO_Decrypt(payload, payload_len, g_eeprom.Settings.EncKey);
    }
#endif

    payload[payload_len] = '\0';

    /* Store in history */
    PushHistory((char *)payload, MSG_DIR_RX);

    /* Set notification flag */
    g_Messenger.pending_rx = true;

    /* Auto-ACK */
    if (g_eeprom.Settings.MsgAck) {
        const char *ack = "ACK";
        uint8_t ack_buf[3];
        memcpy(ack_buf, ack, 3);
        FskPacket_t ack_pkt;
        if (FSK_BuildPacket(&ack_pkt, ack_buf, 3)) {
            MESSENGER_BK4819_DisableFSK();
            MESSENGER_BK4819_SetupTX(GetModulation(), IsWideChannel());
            MESSENGER_BK4819_TxPacket(&ack_pkt);
            MESSENGER_BK4819_DisableFSK();
            MESSENGER_BK4819_SetupRX(GetModulation(), IsWideChannel());
        }
    }

    /* If screen is open, refresh */
    if (g_Messenger.open)
        MESSENGER_DrawScreen();
}

/* ---- Display -------------------------------------------------------------- */

/* Mode label strings */
static const char * const KB_MODE_LABEL[] = {"ABC", "abc", "123"};

void MESSENGER_DrawScreen(void)
{
    /* Clear display buffer */
    UI_DisplayClear();

    /* --- Header row (row 0) --- */
    /* Show keyboard mode indicator and TX indicator */
    char header[MSG_DISPLAY_COLS + 1];
    snprintf(header, sizeof(header), "MSG [%s]%s",
             KB_MODE_LABEL[g_Messenger.kb_mode],
             g_Messenger.tx_in_progress ? " TX" : "   ");
    UI_PrintStringXY(header, 0, 0);

    /* --- History (rows 1 to MSG_DISPLAY_ROWS-1) --- */
    /* Show the most recent (MSG_DISPLAY_ROWS-1) messages */
    uint8_t display_count = MSG_DISPLAY_ROWS - 1;
    if (display_count > g_Messenger.history_count)
        display_count = g_Messenger.history_count;

    for (uint8_t row = 0; row < display_count; row++) {
        /* Most recent at top of history area */
        uint8_t hist_back = display_count - 1 - row;
        uint8_t idx = (g_Messenger.history_head + MSG_HISTORY_MAX - 1 - hist_back) % MSG_HISTORY_MAX;
        if (!g_Messenger.history[idx].valid) continue;

        char line[MSG_DISPLAY_COLS + 1];
        /* Show > for TX, < for RX */
        char prefix = (g_Messenger.history[idx].dir == MSG_DIR_TX) ? '>' : '<';
        snprintf(line, sizeof(line), "%c%s", prefix, g_Messenger.history[idx].text);
        UI_PrintStringXY(line, 0, row + 1);
    }

    /* --- Input row (last row) --- */
    char input_line[MSG_DISPLAY_COLS + 2];
    snprintf(input_line, sizeof(input_line), ">%s_", g_Messenger.input);
    input_line[MSG_DISPLAY_COLS] = '\0';
    UI_PrintStringXY(input_line, 0, MSG_DISPLAY_ROWS);

    /* Push to LCD */
    ST7565_BlitFullScreen();
}

/* Small envelope icon (8×5 px bitmap, drawn at status bar) */
/* Envelope outline: top edge + V shape */
static const uint8_t ENVELOPE_ICON[] = {
    0b11111110,  /* top edge */
    0b10000010,  /* sides */
    0b10111010,  /* V shape */
    0b10000010,
    0b11111110,  /* bottom edge */
};

void MESSENGER_DrawStatusIcon(void)
{
    if (!g_Messenger.pending_rx) return;

    /* Fusion's UI functions: adjust X/Y coordinates to match status bar layout.
     * This places the icon at the far right of the top status bar row.
     * Use UI_DrawBitmap or equivalent from Fusion's ui.h.          */
    /* Placeholder: draw a simple 'E' if bitmap API isn't available */
    UI_PrintStringXY("M", 120, 0);   /* 'M' for message — replace with bitmap */
}

#endif /* ENABLE_MESSENGER */
