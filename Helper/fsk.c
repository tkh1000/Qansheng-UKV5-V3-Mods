/* fsk.c — FSK packet framing implementation
 * Ported from kamilsss655/uv-k5-firmware-custom
 * Pure C — no MCU-specific code here.
 */
#include "fsk.h"
#include <string.h>

/* ---- CRC-16/CCITT-FALSE --------------------------------------------------- */
uint16_t FSK_CRC16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

/* ---- Packet builder ------------------------------------------------------- */
bool FSK_BuildPacket(FskPacket_t *pkt, const uint8_t *data, uint8_t len)
{
    if (!pkt || !data || len == 0 || len > FSK_MSG_MAX_LEN)
        return false;

    pkt->preamble[0] = 0xAA;
    pkt->preamble[1] = 0xAA;
    pkt->sync        = 0x2DD4;
    pkt->length      = len;
    memset(pkt->payload, 0, FSK_MSG_MAX_LEN);
    memcpy(pkt->payload, data, len);

    /* CRC covers length byte + payload */
    uint8_t crc_buf[FSK_MSG_MAX_LEN + 1];
    crc_buf[0] = len;
    memcpy(crc_buf + 1, pkt->payload, FSK_MSG_MAX_LEN);
    pkt->crc = FSK_CRC16(crc_buf, FSK_MSG_MAX_LEN + 1);

    return true;
}

/* ---- Packet validator ----------------------------------------------------- */
bool FSK_ValidatePacket(const FskPacket_t *pkt)
{
    if (!pkt) return false;
    if (pkt->preamble[0] != 0xAA || pkt->preamble[1] != 0xAA) return false;
    if (pkt->sync != 0x2DD4)                                    return false;
    if (pkt->length == 0 || pkt->length > FSK_MSG_MAX_LEN)      return false;

    uint8_t crc_buf[FSK_MSG_MAX_LEN + 1];
    crc_buf[0] = pkt->length;
    memcpy(crc_buf + 1, pkt->payload, FSK_MSG_MAX_LEN);
    uint16_t expected = FSK_CRC16(crc_buf, FSK_MSG_MAX_LEN + 1);

    return (pkt->crc == expected);
}

/* ---- BK4819 register helpers ---------------------------------------------- *
 *
 * REG_58 [15:13] = FSK RX bandwidth  (0=1kHz,1=500Hz,2=300Hz,3=200Hz)
 * REG_58 [12]    = unknown, set 1
 * REG_58 [11:9]  = FSK TX/RX mode    (0=off,1=TX,2=RX,6=TX+RX)
 * REG_58 [8:6]   = preamble length   (recommended: 7 = 7×8 bits)
 * REG_58 [5:4]   = sync length       (recommended: 3)
 * REG_58 [3:2]   = data length type  (0=FSK,1=after-sync byte counts)
 * REG_58 [1:0]   = modulation rate   (0=FSK,1=GFSK,2=AFSK,3=FSK450/700)
 */

/* Modulation-specific FSK config — RX disabled here; caller enables RX bit */
uint16_t FSK_GetConfigReg(FskModulation_t mod)
{
    /* Common fields: preamble=7 groups, sync=3 bytes, length mode=1 */
    uint16_t base = (7 << 6) | (3 << 4) | (1 << 2);

    switch (mod) {
    case FSK_MOD_450:  return base | 0x0003;  /* rate bits = 11 = 450 baud  */
    case FSK_MOD_700:  return base | 0x0002;  /* rate bits = 10 = 700 baud  */
    case FSK_MOD_1200: return base | 0x0001;  /* rate bits = 01 = AFSK 1200 */
    default:           return base | 0x0003;
    }
}

/* TX frequency deviation register (REG_70 bits [6:0] = deviation index).
 * Values calibrated for 25 kHz (wide) and 12.5 kHz (narrow) channel spacing.
 */
uint16_t FSK_GetDeviationReg(FskModulation_t mod, bool wide)
{
    /* REG_70 [15]   = enable TX deviation override
     * REG_70 [14:8] = TX audio gain (leave at reset: 0x28)
     * REG_70 [6:0]  = deviation value
     */
    uint16_t gain = 0x2800;  /* TX audio gain field */

    uint8_t dev;
    if (wide) {
        /* 25 kHz channel */
        switch (mod) {
        case FSK_MOD_450:  dev = 0x2A; break;
        case FSK_MOD_700:  dev = 0x30; break;
        case FSK_MOD_1200: dev = 0x38; break;
        default:           dev = 0x2A; break;
        }
    } else {
        /* 12.5 kHz channel */
        switch (mod) {
        case FSK_MOD_450:  dev = 0x18; break;
        case FSK_MOD_700:  dev = 0x1E; break;
        case FSK_MOD_1200: dev = 0x24; break;
        default:           dev = 0x18; break;
        }
    }

    return 0x8000 | gain | dev;  /* bit 15 = enable override */
}
