/*
 * usbd_audio_mic.c
 * UAC 1.0 Microphone - no IAD, pure audio function.
 *
 * Key points:
 *  - bDeviceClass=0x00 in device descriptor (Windows reads interfaces)
 *  - No IAD needed for single-function audio device
 *  - ISO endpoint bmAttributes=0x01 (isochronous, no sync) for IN mic
 *  - SET_INTERFACE handler opens/closes EP and primes first packet
 */

#include "usbd_audio_mic.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"

/* UAC 1.0 raw constants */
#define CS_INTERFACE            0x24U
#define CS_ENDPOINT             0x25U
#define AC_HEADER               0x01U
#define AC_INPUT_TERMINAL       0x02U
#define AC_OUTPUT_TERMINAL      0x03U
#define AC_FEATURE_UNIT         0x06U
#define AS_GENERAL              0x01U
#define AS_FORMAT_TYPE          0x02U
#define FORMAT_TYPE_I           0x01U
#define EP_GENERAL              0x01U
#define USB_CLASS_AUDIO         0x01U
#define AUDIO_SUBCLASS_CONTROL  0x01U
#define AUDIO_SUBCLASS_STREAM   0x02U
#define AUDIO_PROTO_NONE        0x00U

/* ------------------------------------------------------------------ */
/*  Configuration Descriptor  (109 bytes, no IAD)                     */
/* ------------------------------------------------------------------ */

#define AUDIO_MIC_CFG_DESC_SIZE   109U

static uint8_t s_cfg_desc[AUDIO_MIC_CFG_DESC_SIZE] =
{
    /* Configuration */
    0x09,
    USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(AUDIO_MIC_CFG_DESC_SIZE),
    HIBYTE(AUDIO_MIC_CFG_DESC_SIZE),
    0x02,                   /* bNumInterfaces: 2 (AC + AS)    */
    0x01,                   /* bConfigurationValue            */
    0x00,                   /* iConfiguration                 */
    0x80,                   /* bmAttributes: bus powered      */
    0x32,                   /* bMaxPower: 100mA               */

    /* Standard AC Interface (IF 0, Alt 0) */
    0x09,
    USB_DESC_TYPE_INTERFACE,
    AUDIO_MIC_AC_INTERFACE, /* 0                              */
    0x00,
    0x00,                   /* bNumEndpoints: 0               */
    USB_CLASS_AUDIO,        /* 0x01                           */
    AUDIO_SUBCLASS_CONTROL, /* 0x01                           */
    AUDIO_PROTO_NONE,
    0x00,

    /* CS AC Header
       wTotalLength = 9+12+9+9 = 39 */
    0x09,
    CS_INTERFACE,
    AC_HEADER,
    0x00, 0x01,             /* bcdADC 1.00                    */
    0x27, 0x00,             /* wTotalLength 39                */
    0x01,                   /* bInCollection                  */
    0x01,                   /* baInterfaceNr: AS interface=1  */

    /* Input Terminal - Microphone */
    0x0C,
    CS_INTERFACE,
    AC_INPUT_TERMINAL,
    AUDIO_MIC_INPUT_TERMINAL_ID,    /* 1                      */
    0x01, 0x02,             /* wTerminalType 0x0201 Microphone */
    0x00,
    AUDIO_MIC_CHANNELS,     /* 1                              */
    0x00, 0x00,             /* wChannelConfig mono            */
    0x00,
    0x00,

    /* Feature Unit */
    0x09,
    CS_INTERFACE,
    AC_FEATURE_UNIT,
    AUDIO_MIC_FEATURE_UNIT_ID,      /* 2                      */
    AUDIO_MIC_INPUT_TERMINAL_ID,    /* bSourceID 1            */
    0x01,                   /* bControlSize                   */
    0x01,                   /* bmaControls[0]: mute           */
    0x00,
    0x00,

    /* Output Terminal - USB streaming */
    0x09,
    CS_INTERFACE,
    AC_OUTPUT_TERMINAL,
    AUDIO_MIC_OUTPUT_TERMINAL_ID,   /* 3                      */
    0x01, 0x01,             /* wTerminalType 0x0101 USB stream */
    0x00,
    AUDIO_MIC_FEATURE_UNIT_ID,      /* bSourceID 2            */
    0x00,

    /* Standard AS Interface Alt 0 - zero bandwidth */
    0x09,
    USB_DESC_TYPE_INTERFACE,
    AUDIO_MIC_AS_INTERFACE, /* 1                              */
    0x00,                   /* bAlternateSetting 0            */
    0x00,                   /* bNumEndpoints 0                */
    USB_CLASS_AUDIO,
    AUDIO_SUBCLASS_STREAM,
    AUDIO_PROTO_NONE,
    0x00,

    /* Standard AS Interface Alt 1 - active */
    0x09,
    USB_DESC_TYPE_INTERFACE,
    AUDIO_MIC_AS_INTERFACE, /* 1                              */
    0x01,                   /* bAlternateSetting 1            */
    0x01,                   /* bNumEndpoints 1                */
    USB_CLASS_AUDIO,
    AUDIO_SUBCLASS_STREAM,
    AUDIO_PROTO_NONE,
    0x00,

    /* CS AS General */
    0x07,
    CS_INTERFACE,
    AS_GENERAL,
    AUDIO_MIC_OUTPUT_TERMINAL_ID,   /* bTerminalLink 3        */
    0x01,                   /* bDelay                         */
    0x01, 0x00,             /* wFormatTag PCM                 */

    /* Type I Format */
    0x0B,
    CS_INTERFACE,
    AS_FORMAT_TYPE,
    FORMAT_TYPE_I,
    AUDIO_MIC_CHANNELS,     /* 1                              */
    AUDIO_MIC_SUBFRAME_SIZE,/* 2                              */
    AUDIO_MIC_BIT_RESOLUTION,/* 16                            */
    0x01,                   /* bSamFreqType: 1 discrete freq  */
    0x80, 0x3E, 0x00,       /* tSamFreq 16000Hz = 0x003E80   */

    /* Standard Isochronous Endpoint
       bmAttributes 0x01 = isochronous, no sync (correct for IN mic) */
    0x09,
    USB_DESC_TYPE_ENDPOINT,
    AUDIO_MIC_IN_EP,        /* 0x81                           */
    0x01,                   /* bmAttributes: isochronous      */
    LOBYTE(AUDIO_MIC_PACKET_SZ),
    HIBYTE(AUDIO_MIC_PACKET_SZ),
    0x01,                   /* bInterval: 1ms                 */
    0x00,
    0x00,

    /* CS Endpoint */
    0x07,
    CS_ENDPOINT,
    EP_GENERAL,
    0x00,
    0x00,
    0x00, 0x00,
};

/* ------------------------------------------------------------------ */
/*  Static assert - catch size mismatch at compile time                */
/* ------------------------------------------------------------------ */

typedef char assert_cfg_desc_size[
    (sizeof(s_cfg_desc) == AUDIO_MIC_CFG_DESC_SIZE) ? 1 : -1
];

/* ------------------------------------------------------------------ */
/*  Module state                                                        */
/* ------------------------------------------------------------------ */

static USBD_AUDIO_MIC_HandleTypeDef  s_handle;
static USBD_AUDIO_MIC_ItfTypeDef    *s_fops = NULL;
static uint8_t s_packet[AUDIO_MIC_PACKET_SZ];

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

static void prime_next_packet(USBD_HandleTypeDef *pdev)
{
    if (s_fops && s_fops->Record)
    {
        s_fops->Record(s_packet, AUDIO_MIC_PACKET_SZ);
    }
    else
    {
        for (uint32_t i = 0U; i < AUDIO_MIC_PACKET_SZ; i++) { s_packet[i] = 0U; }
    }
    USBD_LL_Transmit(pdev, AUDIO_MIC_IN_EP, s_packet, AUDIO_MIC_PACKET_SZ);
}

/* ------------------------------------------------------------------ */
/*  Class callbacks                                                     */
/* ------------------------------------------------------------------ */

static uint8_t mic_init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    s_handle.alt_setting = 0U;

    /* Open EP at alt 0 size (0) then re-open when alt 1 selected */
    USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP, USBD_EP_TYPE_ISOC, 0U);
    pdev->ep_in[AUDIO_MIC_IN_EP & 0xFU].is_used = 1U;
    pdev->pClassData = &s_handle;

    if (s_fops && s_fops->Init)
    {
        s_fops->Init(AUDIO_MIC_SAMPLE_RATE);
    }
    return USBD_OK;
}

static uint8_t mic_deinit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    s_handle.alt_setting = 0U;
    USBD_LL_CloseEP(pdev, AUDIO_MIC_IN_EP);
    pdev->ep_in[AUDIO_MIC_IN_EP & 0xFU].is_used = 0U;

    if (s_fops && s_fops->DeInit) { s_fops->DeInit(); }
    return USBD_OK;
}

static uint8_t mic_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint8_t ret = USBD_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
        /* ---- Class requests (volume, mute GET/SET) --------------- */
        case USB_REQ_TYPE_CLASS:
            if (req->wLength > 0U)
            {
                if ((req->bmRequest & 0x80U) != 0U)
                {
                    /* GET - return two zero bytes */
                    static uint8_t dummy[2] = {0x00, 0x00};
                    uint16_t len = (req->wLength < 2U) ? req->wLength : 2U;
                    USBD_CtlSendData(pdev, dummy, len);
                }
                else
                {
                    /* SET - receive and discard */
                    static uint8_t scratch[64];
                    uint16_t len = (req->wLength < 64U) ? req->wLength : 64U;
                    USBD_CtlPrepareRx(pdev, scratch, len);
                }
            }
            break;

        /* ---- Standard interface requests ------------------------- */
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest)
            {
                case USB_REQ_GET_INTERFACE:
                {
                    uint8_t alt = (uint8_t)s_handle.alt_setting;
                    USBD_CtlSendData(pdev, &alt, 1U);
                    break;
                }

                case USB_REQ_SET_INTERFACE:
                {
                    uint8_t new_alt = (uint8_t)(req->wValue & 0xFFU);
                    uint8_t if_num  = (uint8_t)(req->wIndex & 0xFFU);

                    if (if_num == AUDIO_MIC_AS_INTERFACE)
                    {
                        if (new_alt == 1U && s_handle.alt_setting == 0U)
                        {
                            /* Host activating stream */
                            USBD_LL_CloseEP(pdev, AUDIO_MIC_IN_EP);
                            USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP,
                                           USBD_EP_TYPE_ISOC,
                                           AUDIO_MIC_PACKET_SZ);
                            s_handle.alt_setting = 1U;
                            prime_next_packet(pdev);
                        }
                        else if (new_alt == 0U && s_handle.alt_setting == 1U)
                        {
                            /* Host closing stream */
                            USBD_LL_CloseEP(pdev, AUDIO_MIC_IN_EP);
                            USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP,
                                           USBD_EP_TYPE_ISOC, 0U);
                            s_handle.alt_setting = 0U;
                        }
                    }
                    /* AC interface SET_INTERFACE(0) - silently accept */
                    break;
                }

                default:
                    USBD_CtlError(pdev, req);
                    ret = USBD_FAIL;
                    break;
            }
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
    }
    return ret;
}

static uint8_t mic_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if ((epnum == (AUDIO_MIC_IN_EP & 0x7FU)) && (s_handle.alt_setting == 1U))
    {
        prime_next_packet(pdev);
    }
    return USBD_OK;
}

static uint8_t *mic_get_cfg_desc(uint16_t *length)
{
    *length = (uint16_t)sizeof(s_cfg_desc);
    return s_cfg_desc;
}

static uint8_t *mic_get_device_qualifier_desc(uint16_t *length)
{
    (void)length;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Class object                                                        */
/* ------------------------------------------------------------------ */

USBD_ClassTypeDef USBD_AUDIO_MIC =
{
    mic_init,
    mic_deinit,
    mic_setup,
    NULL,           /* EP0_TxSent       */
    NULL,           /* EP0_RxReady      */
    mic_data_in,
    NULL,           /* DataOut          */
    NULL,           /* SOF              */
    NULL,           /* IsoINIncomplete  */
    NULL,           /* IsoOUTIncomplete */
    mic_get_cfg_desc,
    mic_get_cfg_desc,
    mic_get_cfg_desc,
    mic_get_device_qualifier_desc,
};

uint8_t USBD_AUDIO_MIC_RegisterInterface(USBD_HandleTypeDef *pdev,
                                          USBD_AUDIO_MIC_ItfTypeDef *fops)
{
    (void)pdev;
    s_fops = fops;
    return USBD_OK;
}