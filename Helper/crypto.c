/* crypto.c — ChaCha20-256 stream cipher (RFC 7539)
 * Ported from kamilsss655/uv-k5-firmware-custom
 * Pure C, no MCU-specific code.
 */
#include "crypto.h"
#include <string.h>

/* ---- Internal helpers ----------------------------------------------------- */

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define QR(a, b, c, d) \
    a += b; d ^= a; d = ROTL32(d, 16); \
    c += d; b ^= c; b = ROTL32(b, 12); \
    a += b; d ^= a; d = ROTL32(d,  8); \
    c += d; b ^= c; b = ROTL32(b,  7)

static void chacha20_block(uint32_t out[16], const uint32_t in[16])
{
    uint32_t x[16];
    memcpy(x, in, 64);

    for (int i = 0; i < 10; i++) {
        /* Column rounds */
        QR(x[0], x[4], x[ 8], x[12]);
        QR(x[1], x[5], x[ 9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        /* Diagonal rounds */
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[ 8], x[13]);
        QR(x[3], x[4], x[ 9], x[14]);
    }

    for (int i = 0; i < 16; i++)
        out[i] = x[i] + in[i];
}

/* Load a little-endian 32-bit word from 4 bytes */
static inline uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* ---- Public API ----------------------------------------------------------- */

void CRYPTO_ChaCha20(uint8_t       *buf,
                     uint32_t       len,
                     const uint8_t  key[32],
                     const uint8_t  nonce[12],
                     uint32_t       counter)
{
    /* ChaCha20 state constants ("expand 32-byte k") */
    static const uint32_t SIGMA[4] = {
        0x61707865, 0x3320646E, 0x79622D32, 0x6B206574
    };

    uint32_t state[16] = {
        SIGMA[0],   SIGMA[1],   SIGMA[2],   SIGMA[3],
        le32(key+0), le32(key+4), le32(key+8),  le32(key+12),
        le32(key+16),le32(key+20),le32(key+24), le32(key+28),
        counter,
        le32(nonce+0), le32(nonce+4), le32(nonce+8)
    };

    uint32_t block[16];
    uint8_t  keystream[64];

    while (len > 0) {
        chacha20_block(block, state);

        /* Convert block to little-endian byte stream */
        for (int i = 0; i < 16; i++) {
            keystream[i*4+0] = (uint8_t)(block[i]      );
            keystream[i*4+1] = (uint8_t)(block[i] >>  8);
            keystream[i*4+2] = (uint8_t)(block[i] >> 16);
            keystream[i*4+3] = (uint8_t)(block[i] >> 24);
        }

        uint32_t chunk = (len < 64) ? len : 64;
        for (uint32_t i = 0; i < chunk; i++)
            buf[i] ^= keystream[i];

        buf     += chunk;
        len     -= chunk;
        state[12]++;   /* increment block counter */
    }
}

void CRYPTO_Encrypt(uint8_t *buf, uint32_t len, const uint8_t key[32])
{
    /* Derive a deterministic nonce from the key to keep it simple on-radio.
     * Both radios share the key, so the nonce is implicit. */
    uint8_t nonce[12];
    for (int i = 0; i < 12; i++)
        nonce[i] = key[i % 32] ^ (uint8_t)(i * 0x5A);

    CRYPTO_ChaCha20(buf, len, key, nonce, 1);
}

void CRYPTO_Decrypt(uint8_t *buf, uint32_t len, const uint8_t key[32])
{
    /* Same operation as encrypt (XOR stream cipher) */
    CRYPTO_Encrypt(buf, len, key);
}
