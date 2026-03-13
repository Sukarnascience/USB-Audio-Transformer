#ifndef BOARD_H
#define BOARD_H

/*
 * board.h
 * Hardware pin definitions for STM32F407G-DISC1 (MB997E)
 * All names match the silkscreen labels on the board.
 */

#include "stm32f4xx_hal.h"

/* ------------------------------------------------------------------ */
/*  LEDs                                                                */
/* ------------------------------------------------------------------ */
/* All LEDs are on GPIOD, active HIGH */

#define LED_PORT            GPIOD

#define LD4_GREEN_PIN       GPIO_PIN_12   /* Voice mode V1 - Pitch Up   */
#define LD3_ORANGE_PIN      GPIO_PIN_13   /* Normal mode - passthrough   */
#define LD5_RED_PIN         GPIO_PIN_14   /* Voice mode V2 - Pitch Down  */
#define LD6_BLUE_PIN        GPIO_PIN_15   /* Voice mode V3 - Vibrato     */

#define LED_ALL_PINS        (LD4_GREEN_PIN | LD3_ORANGE_PIN | \
                             LD5_RED_PIN   | LD6_BLUE_PIN)

/* ------------------------------------------------------------------ */
/*  USER Button                                                         */
/* ------------------------------------------------------------------ */
/* PA0 - Active HIGH, external pull-down resistor already on board     */

#define USER_BTN_PORT       GPIOA
#define USER_BTN_PIN        GPIO_PIN_0

/* ------------------------------------------------------------------ */
/*  USB OTG FS (Native USB Port - Mini-B connector)                    */
/* ------------------------------------------------------------------ */
/* PA11 = OTG_FS_DM (D-)                                               */
/* PA12 = OTG_FS_DP (D+)                                               */
/* These are handled by the USB HAL driver, defined here for reference */

#define USB_DM_PORT         GPIOA
#define USB_DM_PIN          GPIO_PIN_11

#define USB_DP_PORT         GPIOA
#define USB_DP_PIN          GPIO_PIN_12

/* ------------------------------------------------------------------ */
/*  MEMS Microphone MP45DT02 (PDM output, connected to I2S2)           */
/* ------------------------------------------------------------------ */
/* PB10 = I2S2_CK  = PDM Clock output from STM32 to MIC               */
/* PC3  = I2S2_SD  = PDM Data  input  from MIC to STM32               */
/* MIC power is tied to 3.3V rail, no enable pin needed                */

#define MIC_CLK_PORT        GPIOB
#define MIC_CLK_PIN         GPIO_PIN_10   /* I2S2_CK  AF5 */

#define MIC_DATA_PORT       GPIOC
#define MIC_DATA_PIN        GPIO_PIN_3    /* I2S2_SD  AF5 */

/* ------------------------------------------------------------------ */
/*  Clock Configuration Summary (for reference)                        */
/* ------------------------------------------------------------------ */
/* HSE  =   8 MHz  (onboard crystal)                                   */
/* PLL  = 168 MHz  (SYSCLK)                                            */
/* AHB  = 168 MHz  (HCLK)                                              */
/* APB1 =  42 MHz  (PCLK1) - I2S2 source                              */
/* APB2 =  84 MHz  (PCLK2)                                             */

#endif /* BOARD_H */