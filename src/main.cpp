/*
 * main.cpp - M3
 *
 * Boot sequence:
 *   1. All 4 LEDs blink together x2 (heartbeat)
 *   2. Default mode: LD3 Orange ON (passthrough)
 *
 * Button short press cycles:
 *   LD3 Orange  -> passthrough
 *   LD4 Green   -> voice effect 1 (pitch up)
 *   LD5 Red     -> voice effect 2 (pitch down)
 *   LD6 Blue    -> voice effect 3 (vibrato)
 *   (wraps back to Orange)
 */

#include "stm32f4xx_hal.h"
#include "board.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_audio_mic.h"

/* ------------------------------------------------------------------ */
/*  SysTick override                                                    */
/* ------------------------------------------------------------------ */

static volatile uint32_t s_tick = 0U;

extern "C" void SysTick_Handler(void)
{
    s_tick++;
    HAL_IncTick();
}

extern "C" uint32_t HAL_GetTick(void)
{
    return s_tick;
}

/* ------------------------------------------------------------------ */
/*  Voice mode                                                          */
/* ------------------------------------------------------------------ */

typedef enum
{
    MODE_PASSTHROUGH = 0,
    MODE_PITCH_UP    = 1,
    MODE_PITCH_DOWN  = 2,
    MODE_VIBRATO     = 3,
    MODE_COUNT       = 4
} VoiceMode_t;

static const uint16_t MODE_LED[MODE_COUNT] = {
    LD3_ORANGE_PIN,
    LD4_GREEN_PIN,
    LD5_RED_PIN,
    LD6_BLUE_PIN,
};

static VoiceMode_t s_mode = MODE_PASSTHROUGH;

static void set_mode_led(VoiceMode_t mode)
{
    HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, MODE_LED[mode], GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
/*  USB handles                                                         */
/* ------------------------------------------------------------------ */

USBD_HandleTypeDef husbd;

/* ------------------------------------------------------------------ */
/*  Audio interface callbacks                                           */
/* ------------------------------------------------------------------ */

static int8_t audio_init(uint32_t sample_rate)
{
    (void)sample_rate;
    return 0;
}

static int8_t audio_deinit(void)
{
    return 0;
}

static int8_t audio_record(uint8_t *pbuf, uint32_t size)
{
    /* M3: silence. M4: real PDM audio + DSP per s_mode */
    for (uint32_t i = 0U; i < size; i++)
    {
        pbuf[i] = 0U;
    }
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

    /* Heartbeat: all 4 LEDs blink together x2 */
    for (uint8_t i = 0U; i < 2U; i++)
    {
        HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_SET);
        HAL_Delay(200U);
        HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_RESET);
        HAL_Delay(200U);
    }

    /* Default mode: Orange */
    s_mode = MODE_PASSTHROUGH;
    set_mode_led(s_mode);

    /* USB init */
    USBD_Init(&husbd, &MIC_Desc, 0U);
    USBD_RegisterClass(&husbd, USBD_AUDIO_MIC_CLASS);
    USBD_AUDIO_MIC_RegisterInterface(&husbd, &s_audio_fops);
    USBD_Start(&husbd);

    /* Button state */
    uint8_t  btn_prev  = 0U;
    uint32_t btn_down  = 0U;
    uint8_t  btn_fired = 0U;

    while (1)
    {
        uint32_t now     = s_tick;
        uint8_t  btn_now = (HAL_GPIO_ReadPin(USER_BTN_PORT, USER_BTN_PIN)
                            == GPIO_PIN_SET) ? 1U : 0U;

        /* Falling edge: button pressed */
        if (btn_now && !btn_prev)
        {
            btn_down  = now;
            btn_fired = 0U;
        }

        /* Debounced press: fire once after 50ms */
        if (btn_now && !btn_fired && (now - btn_down) >= 50U)
        {
            s_mode = (VoiceMode_t)((s_mode + 1U) % MODE_COUNT);
            set_mode_led(s_mode);
            btn_fired = 1U;
        }

        /* Rising edge: button released */
        if (!btn_now && btn_prev)
        {
            btn_fired = 0U;
        }

        btn_prev = btn_now;
    }
}

/* ------------------------------------------------------------------ */
/*  SystemClock_Config - HSI PLL -> 168 MHz, PLLQ=7 -> 48 MHz USB     */
/* ------------------------------------------------------------------ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

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

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
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