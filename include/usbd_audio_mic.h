#ifndef __USBD_AUDIO_MIC_H
#define __USBD_AUDIO_MIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"

/*
 * UAC 1.0 Microphone class driver.
 *
 * Topology:
 *   Input Terminal (Microphone) --> Feature Unit --> Output Terminal (USB)
 *   Output Terminal sends audio IN on EP 0x81 (isochronous)
 *
 * Audio format:
 *   16 kHz, 16-bit PCM, mono
 *   Packet size = 16000/1000 * 2 bytes = 32 bytes per 1ms frame
 */

/* Isochronous IN endpoint address */
#define AUDIO_MIC_IN_EP                  0x81U

/* Audio parameters */
#define AUDIO_MIC_SAMPLE_RATE            16000U
#define AUDIO_MIC_CHANNELS               1U
#define AUDIO_MIC_SUBFRAME_SIZE          2U       /* bytes per sample */
#define AUDIO_MIC_BIT_RESOLUTION         16U
#define AUDIO_MIC_PACKET_SZ              ((AUDIO_MIC_SAMPLE_RATE / 1000U) * \
                                           AUDIO_MIC_SUBFRAME_SIZE * \
                                           AUDIO_MIC_CHANNELS)   /* = 32 */

/* Interface numbers */
#define AUDIO_MIC_AC_INTERFACE           0x00U    /* AudioControl */
#define AUDIO_MIC_AS_INTERFACE           0x01U    /* AudioStreaming */

/* Terminal / Unit IDs */
#define AUDIO_MIC_INPUT_TERMINAL_ID      0x01U    /* Microphone input */
#define AUDIO_MIC_FEATURE_UNIT_ID        0x02U    /* Feature unit (mute/vol) */
#define AUDIO_MIC_OUTPUT_TERMINAL_ID     0x03U    /* USB streaming output */

/* Class handle */
typedef struct
{
    uint32_t alt_setting;   /* 0 = zero bandwidth, 1 = active streaming */
} USBD_AUDIO_MIC_HandleTypeDef;

/* Interface callback - application fills audio buffer */
typedef struct
{
    int8_t (*Init)(uint32_t sample_rate);
    int8_t (*DeInit)(void);
    int8_t (*Record)(uint8_t *pbuf, uint32_t size);  /* fill pbuf with PCM */
} USBD_AUDIO_MIC_ItfTypeDef;

/* Class object exported to usbd_core */
extern USBD_ClassTypeDef USBD_AUDIO_MIC;
#define USBD_AUDIO_MIC_CLASS &USBD_AUDIO_MIC

/* Register application callbacks */
uint8_t USBD_AUDIO_MIC_RegisterInterface(USBD_HandleTypeDef *pdev,
                                          USBD_AUDIO_MIC_ItfTypeDef *fops);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_AUDIO_MIC_H */