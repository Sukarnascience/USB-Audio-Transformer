#ifndef __USBD_DESC_H
#define __USBD_DESC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"

/* VID/PID - using STM VID with custom PID */
#define USBD_VID                         0x0483U
#define USBD_PID                         0x5730U
#define USBD_LANGID_STRING               0x0409U  /* English US */
#define USBD_MANUFACTURER_STRING         "STM32_UAC"
#define USBD_PRODUCT_STRING              "BeeVoice Changer Mic"
#define USBD_SERIALNUMBER_STRING         "00000000001A"

extern USBD_DescriptorsTypeDef MIC_Desc;

#ifdef __cplusplus
}
#endif

#endif /* __USBD_DESC_H */