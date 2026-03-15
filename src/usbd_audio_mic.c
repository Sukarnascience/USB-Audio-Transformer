/*
 * usbd_audio_mic.c - UAC 1.0 Microphone, SOF + IsoINIncomplete driven.
 */

#include "usbd_audio_mic.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"

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

#define AUDIO_MIC_CFG_DESC_SIZE   109U

static uint8_t s_cfg_desc[AUDIO_MIC_CFG_DESC_SIZE] =
{
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(AUDIO_MIC_CFG_DESC_SIZE), HIBYTE(AUDIO_MIC_CFG_DESC_SIZE),
    0x02, 0x01, 0x00, 0x80, 0x32,

    0x09, USB_DESC_TYPE_INTERFACE,
    AUDIO_MIC_AC_INTERFACE, 0x00, 0x00,
    USB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_PROTO_NONE, 0x00,

    0x09, CS_INTERFACE, AC_HEADER,
    0x00, 0x01, 0x27, 0x00, 0x01, 0x01,

    0x0C, CS_INTERFACE, AC_INPUT_TERMINAL,
    AUDIO_MIC_INPUT_TERMINAL_ID, 0x01, 0x02,
    0x00, AUDIO_MIC_CHANNELS, 0x00, 0x00, 0x00, 0x00,

    0x09, CS_INTERFACE, AC_FEATURE_UNIT,
    AUDIO_MIC_FEATURE_UNIT_ID, AUDIO_MIC_INPUT_TERMINAL_ID,
    0x01, 0x01, 0x00, 0x00,

    0x09, CS_INTERFACE, AC_OUTPUT_TERMINAL,
    AUDIO_MIC_OUTPUT_TERMINAL_ID, 0x01, 0x01,
    0x00, AUDIO_MIC_FEATURE_UNIT_ID, 0x00,

    0x09, USB_DESC_TYPE_INTERFACE,
    AUDIO_MIC_AS_INTERFACE, 0x00, 0x00,
    USB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAM, AUDIO_PROTO_NONE, 0x00,

    0x09, USB_DESC_TYPE_INTERFACE,
    AUDIO_MIC_AS_INTERFACE, 0x01, 0x01,
    USB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAM, AUDIO_PROTO_NONE, 0x00,

    0x07, CS_INTERFACE, AS_GENERAL,
    AUDIO_MIC_OUTPUT_TERMINAL_ID, 0x01, 0x01, 0x00,

    0x0B, CS_INTERFACE, AS_FORMAT_TYPE,
    FORMAT_TYPE_I, AUDIO_MIC_CHANNELS,
    AUDIO_MIC_SUBFRAME_SIZE, AUDIO_MIC_BIT_RESOLUTION,
    0x01, 0x80, 0x3E, 0x00,

    0x09, USB_DESC_TYPE_ENDPOINT,
    AUDIO_MIC_IN_EP, 0x01,
    LOBYTE(AUDIO_MIC_PACKET_SZ), HIBYTE(AUDIO_MIC_PACKET_SZ),
    0x01, 0x00, 0x00,

    0x07, CS_ENDPOINT, EP_GENERAL,
    0x00, 0x00, 0x00, 0x00,
};

typedef char assert_desc[(sizeof(s_cfg_desc) == AUDIO_MIC_CFG_DESC_SIZE) ? 1 : -1];

/* ------------------------------------------------------------------ */

static USBD_AUDIO_MIC_HandleTypeDef  s_handle;
static USBD_AUDIO_MIC_ItfTypeDef    *s_fops = NULL;

/* Double packet buffer - one being transmitted, one being filled */
static uint8_t s_pkt_a[AUDIO_MIC_PACKET_SZ];
static uint8_t s_pkt_b[AUDIO_MIC_PACKET_SZ];
static uint8_t s_active_buf = 0U;  /* which buffer is in flight */

/* Diagnostic counters */
volatile uint32_t g_set_interface_count = 0U;
volatile uint8_t  g_stream_active       = 0U;
volatile uint32_t g_record_count        = 0U;

/* ------------------------------------------------------------------ */

static void fill_packet(uint8_t *buf)
{
    if (s_fops && s_fops->Record)
    {
        s_fops->Record(buf, AUDIO_MIC_PACKET_SZ);
        g_record_count++;
    }
    else
    {
        for (uint32_t i = 0U; i < AUDIO_MIC_PACKET_SZ; i++) { buf[i] = 0U; }
    }
}

static void transmit_next(USBD_HandleTypeDef *pdev)
{
    /* Fill the idle buffer while the other is in flight */
    uint8_t *tx_buf  = (s_active_buf == 0U) ? s_pkt_a : s_pkt_b;
    uint8_t *fill_buf = (s_active_buf == 0U) ? s_pkt_b : s_pkt_a;

    fill_packet(fill_buf);
    USBD_LL_Transmit(pdev, AUDIO_MIC_IN_EP, tx_buf, AUDIO_MIC_PACKET_SZ);
    s_active_buf ^= 1U;
}

/* ------------------------------------------------------------------ */

static uint8_t mic_init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    s_handle.alt_setting = 0U;
    USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP, USBD_EP_TYPE_ISOC, 0U);
    pdev->ep_in[AUDIO_MIC_IN_EP & 0xFU].is_used = 1U;
    pdev->pClassData = &s_handle;
    if (s_fops && s_fops->Init) { s_fops->Init(AUDIO_MIC_SAMPLE_RATE); }
    return USBD_OK;
}

static uint8_t mic_deinit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    s_handle.alt_setting = 0U;
    g_stream_active = 0U;
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
        case USB_REQ_TYPE_CLASS:
            if (req->wLength > 0U)
            {
                if ((req->bmRequest & 0x80U) != 0U)
                {
                    static uint8_t dummy[2] = {0x00, 0x00};
                    uint16_t len = (req->wLength < 2U) ? req->wLength : 2U;
                    USBD_CtlSendData(pdev, dummy, len);
                }
                else
                {
                    static uint8_t scratch[64];
                    uint16_t len = (req->wLength < 64U) ? req->wLength : 64U;
                    USBD_CtlPrepareRx(pdev, scratch, len);
                }
            }
            break;

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
                            g_set_interface_count++;
                            g_stream_active = 1U;
                            USBD_LL_CloseEP(pdev, AUDIO_MIC_IN_EP);
                            USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP,
                                           USBD_EP_TYPE_ISOC,
                                           AUDIO_MIC_PACKET_SZ);
                            s_handle.alt_setting = 1U;
                            s_active_buf = 0U;
                            /* Pre-fill both buffers */
                            fill_packet(s_pkt_a);
                            fill_packet(s_pkt_b);
                            /* Kick off first transmission */
                            USBD_LL_Transmit(pdev, AUDIO_MIC_IN_EP,
                                             s_pkt_a, AUDIO_MIC_PACKET_SZ);
                            s_active_buf = 1U;
                        }
                        else if (new_alt == 0U && s_handle.alt_setting == 1U)
                        {
                            g_stream_active = 0U;
                            USBD_LL_CloseEP(pdev, AUDIO_MIC_IN_EP);
                            USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP,
                                           USBD_EP_TYPE_ISOC, 0U);
                            s_handle.alt_setting = 0U;
                        }
                    }
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

/* DataIn: previous packet delivered, send next one immediately */
static uint8_t mic_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if (epnum == (AUDIO_MIC_IN_EP & 0x7FU) && s_handle.alt_setting == 1U)
    {
        transmit_next(pdev);
    }
    return USBD_OK;
}

/* SOF: backup pump in case DataIn missed (e.g. bus glitch) */
static uint8_t mic_sof(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    return USBD_OK;
}

/* IsoINIncomplete: USB frame had no data - re-arm the endpoint */
static uint8_t mic_iso_in_incomplete(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    (void)epnum;
    if (s_handle.alt_setting == 1U)
    {
        /* Re-open EP and retransmit */
        USBD_LL_CloseEP(pdev, AUDIO_MIC_IN_EP);
        USBD_LL_OpenEP(pdev, AUDIO_MIC_IN_EP,
                       USBD_EP_TYPE_ISOC, AUDIO_MIC_PACKET_SZ);
        transmit_next(pdev);
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

USBD_ClassTypeDef USBD_AUDIO_MIC =
{
    mic_init,
    mic_deinit,
    mic_setup,
    NULL,
    NULL,
    mic_data_in,
    NULL,
    mic_sof,
    mic_iso_in_incomplete,  /* IsoINIncomplete */
    NULL,
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

void USBD_AUDIO_MIC_PushPacket(USBD_HandleTypeDef *pdev)
{
    if (s_handle.alt_setting == 1U)
    {
        transmit_next(pdev);
    }
}