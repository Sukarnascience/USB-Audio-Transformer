/*
 * main.cpp - M4 + M5 FINAL
 * PDM mic + DSP voice effects over USB Audio.
 *
 * Boot: all 4 LEDs blink x2, then LD3 Orange (passthrough)
 * Button cycles:
 *   LD3 Orange = passthrough (raw mic)
 *   LD4 Green  = pitch up   (girl voice)
 *   LD5 Red    = pitch down (monster voice)
 *   LD6 Blue   = vibrato
 */

#include "stm32f4xx_hal.h"
#include "board.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_audio_mic.h"
#include "pdm_mic.h"
#include "dsp_effects.h"

/* ------------------------------------------------------------------ */
/*  SysTick                                                             */
/* ------------------------------------------------------------------ */

static volatile uint32_t s_tick = 0U;
extern "C" void SysTick_Handler(void) { s_tick++; HAL_IncTick(); }
extern "C" uint32_t HAL_GetTick(void) { return s_tick; }

/* ------------------------------------------------------------------ */
/*  USB handle                                                          */
/* ------------------------------------------------------------------ */

USBD_HandleTypeDef husbd;

/* Diagnostic counters defined in usbd_audio_mic.c */
extern volatile uint32_t g_set_interface_count;
extern volatile uint8_t  g_stream_active;
extern volatile uint32_t g_record_count;

/* ------------------------------------------------------------------ */
/*  Voice mode                                                          */
/* ------------------------------------------------------------------ */

typedef enum
{
    MODE_GIANT  = 0,   /* Orange - 1 octave down, giant/demon   */
    MODE_CHIPMUNK = 1,   /* Green  - pitch x1.8, chipmunk       */
    MODE_VADER    = 2,   /* Red    - pitch x0.75 + slow AM, Vader     */
    MODE_ALIEN     = 3,   /* Blue   - pitch x1.3 + vibrato, alien   */
    MODE_COUNT    = 4
} VoiceMode_t;

static const uint16_t MODE_LED[MODE_COUNT] = {
    LD3_ORANGE_PIN,
    LD4_GREEN_PIN,
    LD5_RED_PIN,
    LD6_BLUE_PIN,
};

static volatile VoiceMode_t s_mode = MODE_GIANT;

static void set_mode_led(VoiceMode_t mode)
{
    HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, MODE_LED[mode], GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
/*  Audio callbacks                                                     */
/* ------------------------------------------------------------------ */

static int8_t audio_init(uint32_t sr)
{
    (void)sr;
    g_stream_active = 1U;
    return 0;
}

static int8_t audio_deinit(void)
{
    g_stream_active = 0U;
    return 0;
}

static int8_t audio_record(uint8_t *pbuf, uint32_t size)
{
    g_record_count++;
    int16_t *samples = (int16_t *)pbuf;
    uint32_t n       = size / 2U;

    pdm_mic_read(samples, n);
    dsp_apply(samples, n, (DSP_Mode_t)s_mode);
    return 0;
}

static USBD_AUDIO_MIC_ItfTypeDef s_audio_fops = {
    audio_init,
    audio_deinit,
    audio_record,
};

/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */

void SystemClock_Config(void);
void GPIO_Init(void);
void Error_Handler(void);

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    SysTick_Config(16000U);
    NVIC_SetPriority(SysTick_IRQn, 0U);
    HAL_Init();
    SystemClock_Config();
    SysTick_Config(168000U);
    GPIO_Init();

    /* Heartbeat x2 */
    for (uint8_t i = 0U; i < 2U; i++)
    {
        HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_SET);
        HAL_Delay(200U);
        HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_RESET);
        HAL_Delay(200U);
    }

    s_mode = MODE_GIANT;
    set_mode_led(s_mode);

    /* Init PDM mic and DSP before USB starts */
    pdm_mic_init();
    dsp_init();

    /* USB init */
    USBD_Init(&husbd, &MIC_Desc, 0U);
    USBD_RegisterClass(&husbd, USBD_AUDIO_MIC_CLASS);
    USBD_AUDIO_MIC_RegisterInterface(&husbd, &s_audio_fops);
    USBD_Start(&husbd);

    /* Start PDM DMA - mic feeds ring buffer continuously */
    pdm_mic_start();

    /* Button state */
    uint8_t  btn_prev  = 0U;
    uint32_t btn_down  = 0U;
    uint8_t  btn_fired = 0U;

    while (1)
    {
        uint32_t now     = s_tick;
        uint8_t  btn_now = (HAL_GPIO_ReadPin(USER_BTN_PORT, USER_BTN_PIN)
                            == GPIO_PIN_SET) ? 1U : 0U;

        if (btn_now && !btn_prev)   { btn_down = now; btn_fired = 0U; }

        if (btn_now && !btn_fired && (now - btn_down) >= 50U)
        {
            s_mode = (VoiceMode_t)((uint32_t)(s_mode + 1U) % (uint32_t)MODE_COUNT);
            set_mode_led(s_mode);
            btn_fired = 1U;
        }

        if (!btn_now && btn_prev) { btn_fired = 0U; }
        btn_prev = btn_now;
    }
}

/* ------------------------------------------------------------------ */
/*  SystemClock_Config                                                  */
/*  HSI PLL -> 168MHz, PLLQ=7 -> 48MHz USB                           */
/*  PLLI2S N=192 R=5 -> 38.4MHz I2S clock for PDM mic                */
/* ------------------------------------------------------------------ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       osc  = {0};
    RCC_ClkInitTypeDef       clk  = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 16U;
    osc.PLL.PLLN            = 336U;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 7U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }

    pclk.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    pclk.PLLI2S.PLLI2SN       = 192U;
    pclk.PLLI2S.PLLI2SR       = 5U;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) { Error_Handler(); }
}

/* ------------------------------------------------------------------ */
/*  GPIO_Init                                                           */
/* ------------------------------------------------------------------ */

void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_RESET);
    gpio.Pin   = LED_ALL_PINS;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);

    gpio.Pin  = USER_BTN_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_BTN_PORT, &gpio);
}

/* ------------------------------------------------------------------ */
/*  Error_Handler                                                       */
/* ------------------------------------------------------------------ */

void Error_Handler(void)
{
    __disable_irq();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    while (1)
    {
        HAL_GPIO_TogglePin(LED_PORT, LED_ALL_PINS);
        for (volatile uint32_t d = 0U; d < 500000U; d++) {}
    }
}