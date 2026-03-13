#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USB stack configuration */
#define USBD_MAX_NUM_INTERFACES          3U   /* AudioControl + AudioStreaming + margin */
#define USBD_MAX_NUM_CONFIGURATION       1U
#define USBD_MAX_STR_DESC_SIZ            512U
#define USBD_SELF_POWERED                0U   /* Bus powered */
#define USBD_DEBUG_LEVEL                 0U   /* 0=off, saves flash */

/* Audio class config */
#define USBD_AUDIO_FREQ                  16000U
#define AUDIO_FS_BINTERVAL               0x01U

/* Memory management - static allocation, no heap needed */
#define USBD_malloc                      (void *)USBD_static_malloc
#define USBD_free                        USBD_static_free
#define USBD_memset                      memset
#define USBD_memcpy                      memcpy
#define USBD_Delay                       HAL_Delay

/* Debug macros - all silent at level 0 */
#define USBD_UsrLog(...)   do {} while (0)
#define USBD_ErrLog(...)   do {} while (0)
#define USBD_DbgLog(...)   do {} while (0)

/* Exported functions */
void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_H */