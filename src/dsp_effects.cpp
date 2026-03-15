/*
 * dsp_effects.cpp - 4 distinct speech-intelligible voice effects.
 *
 * MODE 0 - GIANT    : pitch x0.5  (1 octave down, deep demon/giant)
 * MODE 1 - CHIPMUNK : pitch x1.8  (chipmunk, high but intelligible)
 * MODE 2 - VADER    : pitch x0.75 + slow AM 8Hz (Darth Vader style)
 * MODE 3 - ALIEN    : pitch x1.3  + fast vibrato 6Hz (ET/alien)
 *
 * No echo in any mode. All effects preserve speech intelligibility.
 */

#include "dsp_effects.h"
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Hann window 64pt Q15                                               */
/* ------------------------------------------------------------------ */

static const int16_t s_hann[64] = {
       0,   20,   80,  180,  320,  499,  716,  971,
    1261, 1585, 1942, 2328, 2742, 3181, 3642, 4122,
    4619, 5129, 5649, 6177, 6709, 7242, 7773, 8298,
    8813, 9315, 9801,10267,10709,11125,11511,11864,
   12182,12461,12699,12894,13044,13147,13201,13206,
   13161,13066,12920,12726,12483,12192,11854,11472,
   11046,10580,10075, 9534, 8959, 8352, 7718, 7058,
    6376, 5674, 4956, 4225, 3484, 2736, 1984, 1232,
};

/* ------------------------------------------------------------------ */
/*  OLA                                                                 */
/* ------------------------------------------------------------------ */

#define HIST_SIZE  512U
#define HIST_MASK  (HIST_SIZE - 1U)
#define WIN_SIZE   64U
#define FRAME      16U

typedef struct
{
    int16_t  hist[HIST_SIZE];
    uint32_t hist_wr;
    int32_t  accum[WIN_SIZE];
    uint32_t phase_q8;
} OLA_t;

static OLA_t s_ola_a;   /* giant    hop_in=8  */
static OLA_t s_ola_b;   /* chipmunk hop_in=29 */
static OLA_t s_ola_c;   /* vader    hop_in=12 */
static OLA_t s_ola_d;   /* alien    hop_in=21 */

static void ola_feed(OLA_t *st, const int16_t *in, uint32_t n)
{
    for (uint32_t i = 0U; i < n; i++)
    {
        st->hist[st->hist_wr & HIST_MASK] = in[i];
        st->hist_wr++;
    }
}

static void ola_run(OLA_t *st, int16_t *out, uint32_t hop_in_q8)
{
    for (uint32_t i = 0U; i < WIN_SIZE; i++)
    {
        uint32_t rp = (st->hist_wr - WIN_SIZE +
                      (st->phase_q8 >> 8U) + i) & HIST_MASK;
        st->accum[i] += ((int32_t)st->hist[rp] * (int32_t)s_hann[i]) >> 15;
    }
    st->phase_q8 += hop_in_q8;

    for (uint32_t i = 0U; i < FRAME; i++)
    {
        int32_t v = st->accum[i];
        if      (v >  32767) { v =  32767; }
        else if (v < -32768) { v = -32768; }
        out[i] = (int16_t)v;
    }
    memmove(&st->accum[0], &st->accum[FRAME],
            (WIN_SIZE - FRAME) * sizeof(int32_t));
    memset(&st->accum[WIN_SIZE - FRAME], 0, FRAME * sizeof(int32_t));
}

/* ------------------------------------------------------------------ */
/*  Vader AM state: 8Hz, period = 2000 samples                        */
/* ------------------------------------------------------------------ */
#define VADER_AM_PERIOD  2000U
static uint32_t s_vader_am = 0U;

/* ------------------------------------------------------------------ */
/*  Alien vibrato state: 6Hz, period = 2667 samples                   */
/* ------------------------------------------------------------------ */
#define ALIEN_VIB_PERIOD  2667U
#define ALIEN_DELAY_SIZE  16U
#define ALIEN_DELAY_MASK  (ALIEN_DELAY_SIZE - 1U)
static int16_t  s_alien_delay[ALIEN_DELAY_SIZE];
static uint32_t s_alien_wr    = 0U;
static uint32_t s_alien_phase = 0U;

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void dsp_init(void)
{
    memset(&s_ola_a, 0, sizeof(s_ola_a));
    memset(&s_ola_b, 0, sizeof(s_ola_b));
    memset(&s_ola_c, 0, sizeof(s_ola_c));
    memset(&s_ola_d, 0, sizeof(s_ola_d));
    memset(s_alien_delay, 0, sizeof(s_alien_delay));
    s_vader_am    = 0U;
    s_alien_wr    = 0U;
    s_alien_phase = 0U;
}

void dsp_apply(int16_t *buf, uint32_t n, DSP_Mode_t mode)
{
    switch (mode)
    {
        /* ---------------------------------------------------------- */
        /*  GIANT: 1 full octave down                                  */
        /*  hop_in=8 -> reads input at half speed -> pitch halved     */
        /* ---------------------------------------------------------- */
        case DSP_MODE_GIANT:
            ola_feed(&s_ola_a, buf, n);
            ola_run(&s_ola_a, buf, 8U << 8U);
            break;

        /* ---------------------------------------------------------- */
        /*  CHIPMUNK: pitch x1.8                                       */
        /*  hop_in=29 -> reads 29 input per 16 output                 */
        /* ---------------------------------------------------------- */
        case DSP_MODE_CHIPMUNK:
            ola_feed(&s_ola_b, buf, n);
            ola_run(&s_ola_b, buf, 29U << 8U);
            break;

        /* ---------------------------------------------------------- */
        /*  VADER: pitch x0.75 + slow 8Hz AM                          */
        /*  Drops voice 4 semitones. Slow AM creates the heavy        */
        /*  breathing, ominous Darth Vader character.                 */
        /*  AM range 50%..100% so voice never fully disappears.       */
        /* ---------------------------------------------------------- */
        case DSP_MODE_VADER:
        {
            ola_feed(&s_ola_c, buf, n);
            ola_run(&s_ola_c, buf, 12U << 8U);

            for (uint32_t i = 0U; i < n; i++)
            {
                uint32_t half    = VADER_AM_PERIOD / 2U;
                uint32_t tri     = (s_vader_am < half)
                    ? (s_vader_am * 256U / half)
                    : ((VADER_AM_PERIOD - s_vader_am) * 256U / half);
                /* Remap 0..256 -> 128..256 (50%..100%) */
                uint32_t gain_q8 = 128U + (tri >> 1U);

                int32_t s = ((int32_t)buf[i] * (int32_t)gain_q8) >> 8;
                if      (s >  32767) { s =  32767; }
                else if (s < -32768) { s = -32768; }
                buf[i] = (int16_t)s;

                s_vader_am++;
                if (s_vader_am >= VADER_AM_PERIOD) { s_vader_am = 0U; }
            }
            break;
        }

        /* ---------------------------------------------------------- */
        /*  ALIEN: pitch x1.3 + fast 6Hz vibrato                      */
        /*  Raises pitch 3 semitones. Fast vibrato wobble makes it   */
        /*  sound non-human — like ET or an alien creature speaking.  */
        /* ---------------------------------------------------------- */
        case DSP_MODE_ALIEN:
        {
            ola_feed(&s_ola_d, buf, n);
            ola_run(&s_ola_d, buf, 21U << 8U);

            for (uint32_t i = 0U; i < n; i++)
            {
                s_alien_delay[s_alien_wr & ALIEN_DELAY_MASK] = buf[i];
                s_alien_wr++;

                uint32_t half  = ALIEN_VIB_PERIOD / 2U;
                uint32_t tri   = (s_alien_phase < half)
                    ? s_alien_phase
                    : (ALIEN_VIB_PERIOD - s_alien_phase);
                /* Delay 1..5 samples */
                uint32_t delay = 1U + (tri * 4U / half);

                uint32_t rd = (s_alien_wr - delay) & ALIEN_DELAY_MASK;
                buf[i] = s_alien_delay[rd];

                s_alien_phase++;
                if (s_alien_phase >= ALIEN_VIB_PERIOD) { s_alien_phase = 0U; }
            }
            break;
        }

        default:
            break;
    }
}