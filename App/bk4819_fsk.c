/* bk4819_fsk.c — BK4819 FSK modem setup for UV-K5 V3 Messenger
 * Ported from kamilsss655/uv-k5-firmware-custom
 *
 * The BK4819 radio chip is IDENTICAL on V1 and V3 hardware.
 * Fusion already provides BK4819_WriteRegister() / BK4819_ReadRegister().
 * This file only adds the FSK-specific register sequences on top.
 *
 * Register reference (BK4819 programming manual):
 *   REG_58  FSK enable + configuration
 *   REG_59  FSK TX/RX sub-mode + preamble
 *   REG_5A  FSK TX data FIFO (write)
 *   REG_5B  FSK RX data FIFO (read)
 *   REG_5C  Sync word HIGH + FIFO status
 *   REG_5D  Sync word LOW
 *   REG_70  TX audio deviation override
 *   REG_47  TX audio path
 *   REG_30  TX enable / RX enable bits
 */

#ifdef ENABLE_MESSENGER

#include "messenger.h"
#include "Helper/fsk.h"
#include "App/bk4819.h"   /* Fusion's BK4819 register API */
#include <string.h>

/* ---- Register bit-field constants ---------------------------------------- */

/* REG_58 mode bits (bits [11:9]) */
#define REG58_MODE_OFF   (0u << 9)
#define REG58_MODE_TX    (1u << 9)
#define REG58_MODE_RX    (2u << 9)
#define REG58_MODE_TXRX  (6u << 9)

/* REG_30 TX/RX enable bit positions (verify against Fusion's bk4819.h) */
#define REG30_TX_ENABLE  (1u << 10)
#define REG30_RX_ENABLE  (1u <<  9)

/* Sync word used by kamilsss655 firmware — must match on both radios */
#define FSK_SYNC_HIGH    0x2DD4
#define FSK_SYNC_LOW     0x0000

/* Number of padding/silence words to send before packet (FIFO warm-up) */
#define FSK_TX_PREAMBLE_WORDS  4

/* ---- Helpers -------------------------------------------------------------- */

static void BK4819_SetSyncWord(void)
{
    BK4819_WriteRegister(BK4819_REG_5C, FSK_SYNC_HIGH);
    BK4819_WriteRegister(BK4819_REG_5D, FSK_SYNC_LOW);
}

/* Poll until FSK TX FIFO is not full (bit 1 of REG_5C = FIFO full flag).
 * Returns false on timeout (shouldn't happen in normal use). */
static bool WaitFifoNotFull(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(BK4819_ReadRegister(BK4819_REG_5C) & 0x0002))
            return true;
    }
    return false;
}

/* ---- Public API ----------------------------------------------------------- */

void MESSENGER_BK4819_DisableFSK(void)
{
    /* Write 0 to FSK mode bits in REG_58 to disable */
    uint16_t reg = BK4819_ReadRegister(BK4819_REG_58);
    reg &= ~(0x7u << 9);           /* clear mode bits */
    reg &= ~(1u << 15);            /* clear FSK enable */
    BK4819_WriteRegister(BK4819_REG_58, reg);

    /* Restore TX deviation to default (no override) */
    BK4819_WriteRegister(BK4819_REG_70, 0x00E0);   /* default Fusion value */
}

void MESSENGER_BK4819_SetupTX(FskModulation_t mod, bool wide_channel)
{
    /* 1. Override TX audio deviation for FSK */
    BK4819_WriteRegister(BK4819_REG_70, FSK_GetDeviationReg(mod, wide_channel));

    /* 2. Bypass TX audio mic path — FSK uses baseband data, not audio */
    /* REG_47 bit 14 = TX audio from microphone; clear it for FSK      */
    uint16_t r47 = BK4819_ReadRegister(BK4819_REG_47);
    r47 &= ~(1u << 14);
    BK4819_WriteRegister(BK4819_REG_47, r47);

    /* 3. Set sync word */
    BK4819_SetSyncWord();

    /* 4. Configure REG_58 for TX, modulation, preamble length */
    uint16_t cfg = FSK_GetConfigReg(mod);
    cfg |= REG58_MODE_TX;
    cfg |= (1u << 12);   /* undocumented bit, set in all working implementations */
    BK4819_WriteRegister(BK4819_REG_58, cfg);

    /* 5. Enable FSK (bit 15 in REG_58) */
    BK4819_WriteRegister(BK4819_REG_58, cfg | (1u << 15));

    /* 6. Enable RF TX via REG_30 */
    uint16_t r30 = BK4819_ReadRegister(BK4819_REG_30);
    r30 |= REG30_TX_ENABLE;
    r30 &= ~REG30_RX_ENABLE;
    BK4819_WriteRegister(BK4819_REG_30, r30);
}

void MESSENGER_BK4819_SetupRX(FskModulation_t mod, bool wide_channel)
{
    /* 1. Set sync word */
    BK4819_SetSyncWord();

    /* 2. Configure REG_58 for RX, modulation, preamble length */
    uint16_t cfg = FSK_GetConfigReg(mod);
    cfg |= REG58_MODE_RX;
    cfg |= (1u << 12);
    BK4819_WriteRegister(BK4819_REG_58, cfg);

    /* 3. Enable FSK */
    BK4819_WriteRegister(BK4819_REG_58, cfg | (1u << 15));

    /* 4. Enable RX via REG_30 */
    uint16_t r30 = BK4819_ReadRegister(BK4819_REG_30);
    r30 |= REG30_RX_ENABLE;
    r30 &= ~REG30_TX_ENABLE;
    BK4819_WriteRegister(BK4819_REG_30, r30);

    /* 5. Clear FSK interrupt flag */
    BK4819_WriteRegister(BK4819_REG_02, 0);

    (void)wide_channel;  /* bandwidth affects deviation only (TX), not RX config */
}

void MESSENGER_BK4819_TxPacket(const FskPacket_t *pkt)
{
    const uint8_t *src  = (const uint8_t *)pkt;
    uint16_t       total = sizeof(FskPacket_t);

    /* Send preamble (idle 0xAA words) to prime FIFO */
    for (uint8_t i = 0; i < FSK_TX_PREAMBLE_WORDS; i++) {
        WaitFifoNotFull();
        BK4819_WriteRegister(BK4819_REG_5A, 0xAAAA);
    }

    /* Send packet bytes in 16-bit words (big-endian to BK4819 FIFO) */
    for (uint16_t i = 0; i < total; i += 2) {
        uint16_t word = ((uint16_t)src[i] << 8);
        if (i + 1 < total)
            word |= src[i + 1];
        WaitFifoNotFull();
        BK4819_WriteRegister(BK4819_REG_5A, word);
    }

    /* Wait for TX FIFO to drain (FIFO empty flag = bit 0 of REG_5C) */
    for (uint32_t i = 0; i < 1000000; i++) {
        if (BK4819_ReadRegister(BK4819_REG_5C) & 0x0001)
            break;  /* empty */
    }
}

#endif /* ENABLE_MESSENGER */
