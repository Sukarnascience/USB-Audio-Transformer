#ifndef MAIN_H
#define MAIN_H

/*
 * main.h
 * Application-level types, enums, and function prototypes.
 * Grows with each milestone.
 */

#include "stm32f4xx_hal.h"
#include "board.h"

/* ------------------------------------------------------------------ */
/*  Voice Mode                                                          */
/* ------------------------------------------------------------------ */

typedef enum
{
    MODE_NORMAL     = 0,   /* LD3 Orange - raw passthrough              */
    MODE_PITCH_UP   = 1,   /* LD4 Green  - pitch up (boy -> girl)       */
    MODE_PITCH_DOWN = 2,   /* LD5 Red    - pitch down (deep/monster)    */
    MODE_VIBRATO    = 3,   /* LD6 Blue   - vibrato/chorus warble        */
    MODE_COUNT      = 4
} VoiceMode_t;

/* ------------------------------------------------------------------ */
/*  Button Debounce State                                               */
/* ------------------------------------------------------------------ */

typedef enum
{
    BTN_IDLE    = 0,
    BTN_PRESSED = 1
} ButtonState_t;

/* ------------------------------------------------------------------ */
/*  Function Prototypes                                                 */
/* ------------------------------------------------------------------ */

/* System */
void SystemClock_Config(void);
void Error_Handler(void);

/* GPIO */
void GPIO_Init(void);

/* Mode */
void Mode_Set(VoiceMode_t mode);
void Mode_Cycle(void);
VoiceMode_t Mode_Get(void);

#endif /* MAIN_H */