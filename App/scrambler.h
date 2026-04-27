/* scrambler.h / scrambler.c — Frequency-inversion voice scrambler
 * Ported from kamilsss655/uv-k5-firmware-custom
 * Target: armel/uv-k1-k5v3-firmware-custom (PY32F071)
 *
 * Algorithm: mix audio with a local BFO tone to invert the frequency spectrum.
 *   f_out = f_bfo - f_in  (inversion)
 * The BFO frequency sets the inversion point (speech intelligibility is
 * highest at 2700 Hz inversion point for voice audio).
 *
 * PY32F071 vs DP32G030 difference: ADC is 12-bit (0–4095) vs 10-bit (0–1023).
 * All ADC centering math uses 2048 as midpoint instead of 512.
 */
#ifndef SCRAMBLER_H
#define SCRAMBLER_H

#ifdef ENABLE_SCRAMBLER

#include <stdint.h>
#include <stdbool.h>

/* Inversion frequencies available in menu (Hz) */
typedef enum {
    SCRAMBLER_OFF  = 0,
    SCRAMBLER_2000 = 1,   /* 2000 Hz inversion (light scramble) */
    SCRAMBLER_2500 = 2,   /* 2500 Hz inversion                  */
    SCRAMBLER_2700 = 3,   /* 2700 Hz inversion (voice-optimised)*/
    SCRAMBLER_3000 = 4,   /* 3000 Hz inversion (heavy scramble) */
    SCRAMBLER_COUNT
} ScramblerMode_t;

/** Call once from main init after ADC is ready. Pre-computes the BFO LUT. */
void SCRAMBLER_Init(ScramblerMode_t mode);

/** Enable scrambling (call when PTT pressed or in monitor mode). */
void SCRAMBLER_Enable(void);

/** Disable scrambling, restore audio path to normal. */
void SCRAMBLER_Disable(void);

/** Set inversion frequency at runtime (menu change). */
void SCRAMBLER_SetMode(ScramblerMode_t mode);

/** Process one audio sample.
 *  Call from ADC ISR or DMA callback at ~8 kHz sample rate.
 *  @param raw_adc   12-bit ADC value (0–4095) from PY32F071 ADC
 *  @return          processed 12-bit value to write to DAC / BK4819 mic path */
int16_t SCRAMBLER_ProcessSample(uint16_t raw_adc);

#endif /* ENABLE_SCRAMBLER */
#endif /* SCRAMBLER_H */


/* =========================================================================
 * Implementation (scrambler.c content follows — split into .c if preferred)
 * ========================================================================= */
#ifdef SCRAMBLER_IMPL
#ifdef ENABLE_SCRAMBLER

#include "scrambler.h"
#include <math.h>    /* sinf — link with -lm or replace with LUT below */
#include <string.h>

/* Sample rate used by BK4819 audio path */
#define SAMPLE_RATE_HZ   8000

/* LUT size for BFO sine wave — power of 2 for fast modulo */
#define LUT_SIZE         256

static int16_t  s_bfo_lut[LUT_SIZE];   /* Q15 sine LUT for BFO             */
static uint16_t s_bfo_phase;           /* current phase index into LUT      */
static uint16_t s_bfo_step;            /* phase increment per sample        */
static bool     s_enabled = false;

/* Pre-compute integer sine LUT (range ±2047 for 12-bit ADC headroom) */
static void BuildLUT(uint32_t bfo_hz)
{
    /* phase step = LUT_SIZE * bfo_hz / SAMPLE_RATE_HZ */
    s_bfo_step = (uint16_t)((uint32_t)LUT_SIZE * bfo_hz / SAMPLE_RATE_HZ);

    for (int i = 0; i < LUT_SIZE; i++) {
        /* sinf returns -1.0 .. +1.0; scale to ±2047 */
        float angle = 2.0f * 3.14159265f * i / LUT_SIZE;
        s_bfo_lut[i] = (int16_t)(sinf(angle) * 2047.0f);
    }
}

/* Inversion frequencies for each mode */
static const uint32_t MODE_FREQ_HZ[] = {
    0,     /* OFF   */
    2000,  /* 2000 Hz */
    2500,  /* 2500 Hz */
    2700,  /* 2700 Hz */
    3000,  /* 3000 Hz */
};

void SCRAMBLER_Init(ScramblerMode_t mode)
{
    memset(s_bfo_lut, 0, sizeof(s_bfo_lut));
    s_bfo_phase = 0;
    s_bfo_step  = 0;
    s_enabled   = false;

    if (mode != SCRAMBLER_OFF)
        BuildLUT(MODE_FREQ_HZ[mode]);
}

void SCRAMBLER_SetMode(ScramblerMode_t mode)
{
    s_bfo_phase = 0;
    if (mode == SCRAMBLER_OFF) {
        s_bfo_step = 0;
    } else {
        BuildLUT(MODE_FREQ_HZ[mode]);
    }
}

void SCRAMBLER_Enable(void)  { s_enabled = true;  s_bfo_phase = 0; }
void SCRAMBLER_Disable(void) { s_enabled = false; }

int16_t SCRAMBLER_ProcessSample(uint16_t raw_adc)
{
    if (!s_enabled || s_bfo_step == 0) {
        /* Pass-through: re-center and return */
        return (int16_t)raw_adc - 2048;
    }

    /* Center the 12-bit sample around zero (-2048 .. +2047) */
    int32_t sample = (int32_t)raw_adc - 2048;

    /* Multiply by BFO sine — frequency inversion via heterodyne mixing.
     * result = sample * sin(2π * f_bfo * t)
     * Q15 sine * signed sample → divide by 2048 to keep 12-bit range.
     */
    int32_t bfo    = s_bfo_lut[s_bfo_phase & (LUT_SIZE - 1)];
    int32_t mixed  = (sample * bfo) >> 11;  /* >>11 ≈ /2048 */

    /* Advance BFO phase */
    s_bfo_phase = (s_bfo_phase + s_bfo_step) & (LUT_SIZE - 1);

    /* Clamp to 12-bit signed range */
    if (mixed >  2047) mixed =  2047;
    if (mixed < -2048) mixed = -2048;

    return (int16_t)mixed;
}

#endif /* ENABLE_SCRAMBLER */
#endif /* SCRAMBLER_IMPL */
