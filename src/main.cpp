/*
 * main.cpp - M2 FINAL
 * HAL working. Non-blocking loop. Button cycles LED mode.
 *
 * Default  : CCW chase pattern LD3->LD5->LD6->LD4->repeat
 * Button   : each press advances single LED mode
 *            LD3 -> LD4 -> LD5 -> LD6 -> LD3 ...
 * Hold btn : hold 1s to return to chase pattern from any single mode
 */

#include "stm32f4xx_hal.h"
#include "board.h"

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
/*  Types                                                               */
/* ------------------------------------------------------------------ */

typedef enum
{
    MODE_PATTERN = 0,
    MODE_SINGLE  = 1
} AppMode_t;

/* ------------------------------------------------------------------ */
/*  Prototypes                                                          */
/* ------------------------------------------------------------------ */

void SystemClock_Config(void);
void GPIO_Init(void);
void Error_Handler(void);

/* ------------------------------------------------------------------ */
/*  LED helpers                                                         */
/* ------------------------------------------------------------------ */

static void all_off(void)
{
    HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_RESET);
}

static void led_only(uint16_t pin)
{
    all_off();
    HAL_GPIO_WritePin(LED_PORT, pin, GPIO_PIN_SET);
}

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

    /* CCW order on board: LD3 Orange -> LD5 Red -> LD6 Blue -> LD4 Green */
    const uint16_t ccw[4] = {
        LD3_ORANGE_PIN,
        LD5_RED_PIN,
        LD6_BLUE_PIN,
        LD4_GREEN_PIN
    };

    /* Single mode order */
    const uint16_t single[4] = {
        LD3_ORANGE_PIN,
        LD4_GREEN_PIN,
        LD5_RED_PIN,
        LD6_BLUE_PIN
    };

    AppMode_t mode       = MODE_PATTERN;
    uint8_t   pat_idx    = 0U;
    uint8_t   single_idx = 0U;
    uint32_t  pat_last   = 0U;

    /* Button state */
    uint8_t  btn_prev    = 0U;
    uint32_t btn_down_at = 0U;
    uint8_t  btn_fired   = 0U;

    #define PATTERN_MS      300U
    #define DEBOUNCE_MS      50U
    #define HOLD_MS        1000U

    /* Show first pattern step immediately */
    led_only(ccw[0]);
    pat_last = s_tick;

    while (1)
    {
        uint32_t now = s_tick;

        /* ---- Button read ------------------------------------------ */
        uint8_t btn_now = (HAL_GPIO_ReadPin(USER_BTN_PORT, USER_BTN_PIN)
                           == GPIO_PIN_SET) ? 1U : 0U;

        /* Rising edge */
        if (btn_now && !btn_prev)
        {
            btn_down_at = now;
            btn_fired   = 0U;
        }

        /* Held past debounce, not yet fired -> short press action */
        if (btn_now && !btn_fired &&
            (now - btn_down_at) >= DEBOUNCE_MS)
        {
            if (mode == MODE_PATTERN)
            {
                /* First press: enter single mode at LD3 */
                mode       = MODE_SINGLE;
                single_idx = 0U;
            }
            else
            {
                /* Subsequent presses: advance single LED */
                single_idx = (single_idx + 1U) % 4U;
            }
            led_only(single[single_idx]);
            btn_fired = 1U;
        }

        /* Held past 1s -> return to pattern */
        if (btn_now && btn_fired &&
            (now - btn_down_at) >= HOLD_MS &&
            mode == MODE_SINGLE)
        {
            mode      = MODE_PATTERN;
            pat_idx   = 0U;
            pat_last  = now;
            btn_fired = 1U;   /* prevent re-trigger */
        }

        /* Falling edge reset */
        if (!btn_now && btn_prev)
        {
            btn_fired = 0U;
        }

        btn_prev = btn_now;

        /* ---- Pattern tick ----------------------------------------- */
        if (mode == MODE_PATTERN &&
            (now - pat_last) >= PATTERN_MS)
        {
            pat_last = now;
            led_only(ccw[pat_idx]);
            pat_idx = (pat_idx + 1U) % 4U;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  SystemClock_Config - HSI PLL -> 168MHz                             */
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

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
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

    gpio.Pin   = USER_BTN_PIN;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
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