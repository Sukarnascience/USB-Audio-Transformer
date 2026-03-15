/*
 * pdm_mic.cpp - MP45DT02 PDM mic, STM32F407G-DISC1
 *
 * Pipeline:
 *   I2S2 DMA -> CIC decimation (64x) -> Pre-emphasis FIR -> PCM ring
 *
 * Pre-emphasis: y[n] = x[n] - 0.97*x[n-1]
 * Compensates CIC high-frequency droop so speech sounds natural.
 */

#include "pdm_mic.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static I2S_HandleTypeDef hi2s2;
static DMA_HandleTypeDef hdma_i2s2_rx;

#define DMA_HALF    128U
#define DMA_TOTAL   (DMA_HALF * 2U)
static uint16_t s_dma_buf[DMA_TOTAL];

#define RING_SIZE   256U
#define RING_MASK   (RING_SIZE - 1U)
static int16_t           s_ring[RING_SIZE];
static volatile uint32_t s_wr = 0U;
static volatile uint32_t s_rd = 0U;

/* CIC state */
typedef struct { int32_t i1,i2,i3,c1p,c2p,c3p; } CIC_t;
static CIC_t s_cic;

/* Pre-emphasis state: y[n] = x[n] - 0.97*x[n-1]
   0.97 in Q8 = 248 */
static int32_t s_pre_prev = 0;
#define PRE_ALPHA_Q8  248   /* 0.97 * 256 */

/* ------------------------------------------------------------------ */

#define BITS_PER_SAMPLE  64U
#define PCM_PER_HALF     32U   /* 128 words * 16 bits / 64 = 32 */

static void process_half(const uint16_t *words)
{
    uint32_t bit_count = 0U;
    uint32_t out_idx   = 0U;
    int16_t  tmp[PCM_PER_HALF];

    for (uint32_t w = 0U; w < DMA_HALF && out_idx < PCM_PER_HALF; w++)
    {
        uint16_t word = words[w];
        for (int b = 15; b >= 0; b--)
        {
            int32_t bit = ((word >> (uint32_t)b) & 1U) ? 1 : -1;

            s_cic.i1 += bit;
            s_cic.i2 += s_cic.i1;
            s_cic.i3 += s_cic.i2;

            if (++bit_count >= BITS_PER_SAMPLE)
            {
                bit_count = 0U;

                int32_t d1 = s_cic.i3  - s_cic.c1p; s_cic.c1p = s_cic.i3;
                int32_t d2 = d1 - s_cic.c2p;         s_cic.c2p = d1;
                int32_t d3 = d2 - s_cic.c3p;         s_cic.c3p = d2;

                /* Gain shift: 2^18 -> 2^3 = shift 15 */
                int32_t pcm = d3 >> 3;

                /* Pre-emphasis: y = x - 0.97*prev */
                int32_t pre = pcm - ((s_pre_prev * PRE_ALPHA_Q8) >> 8);
                s_pre_prev  = pcm;

                /* Clamp */
                if      (pre >  32767) { pre =  32767; }
                else if (pre < -32768) { pre = -32768; }

                tmp[out_idx++] = (int16_t)pre;
            }
        }
    }

    for (uint32_t i = 0U; i < out_idx; i++)
    {
        s_ring[s_wr & RING_MASK] = tmp[i];
        s_wr++;
    }
}

extern "C" void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI2) { process_half(&s_dma_buf[0]); }
}

extern "C" void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI2) { process_half(&s_dma_buf[DMA_HALF]); }
}

extern "C" void HAL_I2S_MspInit(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance != SPI2) { return; }
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_10; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_3; HAL_GPIO_Init(GPIOC, &gpio);

    hdma_i2s2_rx.Instance                 = DMA1_Stream3;
    hdma_i2s2_rx.Init.Channel             = DMA_CHANNEL_0;
    hdma_i2s2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_i2s2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_i2s2_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_i2s2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_i2s2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_i2s2_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_i2s2_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_i2s2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_i2s2_rx);
    __HAL_LINKDMA(hi2s, hdmarx, hdma_i2s2_rx);

    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
}

extern "C" void HAL_I2S_MspDeInit(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance != SPI2) { return; }
    HAL_DMA_DeInit(&hdma_i2s2_rx);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_3);
}

extern "C" void DMA1_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_i2s2_rx);
}

void pdm_mic_init(void)
{
    memset(&s_cic, 0, sizeof(s_cic));
    memset(s_ring,    0, sizeof(s_ring));
    memset(s_dma_buf, 0, sizeof(s_dma_buf));
    s_wr = 0U; s_rd = 0U; s_pre_prev = 0;

    hi2s2.Instance            = SPI2;
    hi2s2.Init.Mode           = I2S_MODE_MASTER_RX;
    hi2s2.Init.Standard       = I2S_STANDARD_PHILIPS;
    hi2s2.Init.DataFormat     = I2S_DATAFORMAT_16B;
    hi2s2.Init.MCLKOutput     = I2S_MCLKOUTPUT_DISABLE;
    hi2s2.Init.AudioFreq      = I2S_AUDIOFREQ_16K;
    hi2s2.Init.CPOL           = I2S_CPOL_HIGH;
    hi2s2.Init.ClockSource    = I2S_CLOCK_PLL;
    hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
    HAL_I2S_Init(&hi2s2);
}

void pdm_mic_start(void)
{
    HAL_I2S_Receive_DMA(&hi2s2, s_dma_buf, DMA_TOTAL);
}

void pdm_mic_stop(void)
{
    HAL_I2S_DMAStop(&hi2s2);
}

uint32_t pdm_mic_read(int16_t *dst, uint32_t num_samples)
{
    uint32_t available = s_wr - s_rd;
    uint32_t count     = (available < num_samples) ? available : num_samples;
    for (uint32_t i = 0U; i < count; i++)
    {
        dst[i] = s_ring[s_rd & RING_MASK];
        s_rd++;
    }
    for (uint32_t i = count; i < num_samples; i++) { dst[i] = 0; }
    return count;
}