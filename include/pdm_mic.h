#ifndef __PDM_MIC_H
#define __PDM_MIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
 * PDM Microphone driver - MP45DT02 on STM32F407G-DISC1
 *
 * Hardware:
 *   PB10 = I2S2_CK  (PDM clock output, 1.024 MHz)
 *   PC3  = I2S2_SD  (PDM data input)
 *
 * Pipeline:
 *   I2S2 DMA (16-bit words at 64kHz) -> CIC decimation (64x) -> 16kHz PCM
 *
 * Output:
 *   16kHz, 16-bit signed PCM, mono
 *   pdm_mic_read() fills caller's buffer from a lock-free ring buffer
 */

/* PCM output parameters */
#define PDM_MIC_SAMPLE_RATE     16000U
#define PDM_MIC_PACKET_SAMPLES  16U
#define PDM_MIC_PACKET_BYTES    (PDM_MIC_PACKET_SAMPLES * 2U)

#define PDM_DECIMATION          64U
#define PDM_I2S_WORD_RATE       64000U

/* DMA: 128 words per half = 32 PCM samples per IRQ = 2ms production rate */
#define PDM_DMA_HALF_WORDS      128U
#define PDM_DMA_TOTAL_WORDS     (PDM_DMA_HALF_WORDS * 2U)

void     pdm_mic_init(void);
void     pdm_mic_start(void);
void     pdm_mic_stop(void);
uint32_t pdm_mic_read(int16_t *dst, uint32_t num_samples);

/* Called from DMA ISR - do not call directly */
void pdm_mic_dma_half_cb(void);
void pdm_mic_dma_full_cb(void);

#ifdef __cplusplus
}
#endif

#endif /* __PDM_MIC_H */