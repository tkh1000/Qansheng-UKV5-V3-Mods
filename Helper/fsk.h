/* fsk.h — FSK packet framing for UV-K5 V3 Messenger port
 * Ported from kamilsss655/uv-k5-firmware-custom to armel/uv-k1-k5v3-firmware-custom
 * Target MCU: PY32F071 (UV-K5 V3 / UV-K1)
 * Pure C, no MCU-specific dependencies.
 */
#ifndef FSK_H
#define FSK_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum message payload length (bytes, excluding framing) */
#define FSK_MSG_MAX_LEN   80

/* Total packet size:
 *   2 B preamble  +  2 B sync  +  1 B length  +  FSK_MSG_MAX_LEN B data  +  2 B CRC
 */
#define FSK_PACKET_SIZE   (FSK_MSG_MAX_LEN + 7)

/* BK4819 FSK FIFO depth is 64 bytes; we send in 64-byte chunks */
#define FSK_FIFO_SIZE     64

/* Modulation presets -------------------------------------------------------- */
typedef enum {
    FSK_MOD_450  = 0,  /* 450  baud — best for poor conditions   */
    FSK_MOD_700  = 1,  /* 700  baud — medium conditions           */
    FSK_MOD_1200 = 2,  /* 1200 baud AFSK — good conditions        */
} FskModulation_t;

/* Packet structure ---------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[2];   /* 0xAA 0xAA                             */
    uint16_t sync;          /* 0x2DD4 sync word                      */
    uint8_t  length;        /* payload length (≤ FSK_MSG_MAX_LEN)    */
    uint8_t  payload[FSK_MSG_MAX_LEN];
    uint16_t crc;           /* CRC-16/CCITT-FALSE over length+payload */
} FskPacket_t;

/* API ----------------------------------------------------------------------- */

/** Build a packet from a plaintext/encrypted buffer.
 *  @param pkt      output packet (caller-allocated)
 *  @param data     payload bytes
 *  @param len      payload length (must be ≤ FSK_MSG_MAX_LEN)
 *  @return true on success */
bool     FSK_BuildPacket(FskPacket_t *pkt, const uint8_t *data, uint8_t len);

/** Validate a received packet (CRC check).
 *  @return true if CRC matches */
bool     FSK_ValidatePacket(const FskPacket_t *pkt);

/** Compute CRC-16/CCITT-FALSE over a buffer. */
uint16_t FSK_CRC16(const uint8_t *buf, uint16_t len);

/** Return the BK4819 register value for REG_70 (TX deviation) for a given
 *  modulation preset and bandwidth (25 kHz = true, 12.5 kHz = false). */
uint16_t FSK_GetDeviationReg(FskModulation_t mod, bool wide);

/** Return the BK4819 register value for REG_58 (FSK config) for a given
 *  modulation preset. */
uint16_t FSK_GetConfigReg(FskModulation_t mod);

#endif /* FSK_H */
