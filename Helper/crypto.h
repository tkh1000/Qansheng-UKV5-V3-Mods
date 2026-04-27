/* crypto.h — ChaCha20-256 encryption for UV-K5 V3 Messenger
 * Ported from kamilsss655/uv-k5-firmware-custom
 * Pure C — no MCU-specific dependencies.
 * Reference: RFC 7539
 */
#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

#define CHACHA20_KEY_SIZE    32   /* 256-bit key */
#define CHACHA20_NONCE_SIZE  12   /* 96-bit nonce */

/* Encrypt or decrypt a buffer in-place with ChaCha20.
 * Encryption and decryption are the same operation (XOR stream cipher).
 *
 * @param buf    buffer to encrypt/decrypt in-place
 * @param len    length of buffer in bytes
 * @param key    32-byte secret key
 * @param nonce  12-byte nonce (use a fixed per-session nonce for radio)
 * @param counter  initial block counter (typically 0)
 */
void CRYPTO_ChaCha20(uint8_t       *buf,
                     uint32_t       len,
                     const uint8_t  key[CHACHA20_KEY_SIZE],
                     const uint8_t  nonce[CHACHA20_NONCE_SIZE],
                     uint32_t       counter);

/* Convenience wrappers with a static nonce derived from key[0..3].
 * Use these for the radio messenger — both radios must have the same key. */
void CRYPTO_Encrypt(uint8_t *buf, uint32_t len, const uint8_t key[CHACHA20_KEY_SIZE]);
void CRYPTO_Decrypt(uint8_t *buf, uint32_t len, const uint8_t key[CHACHA20_KEY_SIZE]);

#endif /* CRYPTO_H */
