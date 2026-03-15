#ifndef __DSP_EFFECTS_H
#define __DSP_EFFECTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * MODE 0 - GIANT    : 1 octave down, deep demon/giant    (Orange)
 * MODE 1 - CHIPMUNK : pitch x1.8, high squeaky           (Green)
 * MODE 2 - VADER    : pitch x0.75 + slow AM 8Hz          (Red)
 * MODE 3 - ALIEN    : pitch x1.3 + fast vibrato 6Hz      (Blue)
 */

typedef enum
{
    DSP_MODE_GIANT    = 0,
    DSP_MODE_CHIPMUNK = 1,
    DSP_MODE_VADER    = 2,
    DSP_MODE_ALIEN    = 3,
} DSP_Mode_t;

void dsp_init(void);
void dsp_apply(int16_t *buf, uint32_t num_samples, DSP_Mode_t mode);

#ifdef __cplusplus
}
#endif

#endif