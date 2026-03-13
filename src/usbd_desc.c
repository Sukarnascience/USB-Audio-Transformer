/*
 * usbd_desc.c
 * Device descriptor - pure audio function, no IAD, no composite class.
 * bDeviceClass=0x00 tells Windows to look at interface descriptors.
 */

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

static uint8_t s_dev_desc[USB_LEN_DEV_DESC] =
{
    USB_LEN_DEV_DESC,         /* bLength              */
    USB_DESC_TYPE_DEVICE,     /* bDescriptorType      */
    0x00, 0x02,               /* bcdUSB = 2.00        */
    0x00,                     /* bDeviceClass: 0 = per-interface */
    0x00,                     /* bDeviceSubClass      */
    0x00,                     /* bDeviceProtocol      */
    USB_MAX_EP0_SIZE,         /* bMaxPacketSize       */
    LOBYTE(USBD_VID),
    HIBYTE(USBD_VID),
    LOBYTE(USBD_PID),
    HIBYTE(USBD_PID),
    0x00, 0x02,               /* bcdDevice = 2.00     */
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION
};

static uint8_t s_str_buf[USBD_MAX_STR_DESC_SIZ];

static uint8_t *get_string(uint8_t *desc, uint8_t *buf, uint16_t *length)
{
    uint8_t idx  = 0U;
    uint8_t *src = desc;

    buf[idx++] = 0U;              /* length placeholder */
    buf[idx++] = USB_DESC_TYPE_STRING;

    while (*src != '\0')
    {
        buf[idx++] = *src++;
        buf[idx++] = 0x00U;
    }

    buf[0]   = idx;
    *length  = (uint16_t)idx;
    return buf;
}

static uint8_t *dev_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(s_dev_desc);
    return s_dev_desc;
}

static uint8_t *lang_id_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    static uint8_t buf[4] = {
        4U, USB_DESC_TYPE_STRING,
        LOBYTE(USBD_LANGID_STRING), HIBYTE(USBD_LANGID_STRING)
    };
    *length = 4U;
    return buf;
}

static uint8_t *manufacturer_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return get_string((uint8_t *)USBD_MANUFACTURER_STRING, s_str_buf, length);
}

static uint8_t *product_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return get_string((uint8_t *)USBD_PRODUCT_STRING, s_str_buf, length);
}

static uint8_t *serial_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    return get_string((uint8_t *)USBD_SERIALNUMBER_STRING, s_str_buf, length);
}

static uint8_t *config_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed; (void)length;
    return NULL;
}

static uint8_t *interface_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed; (void)length;
    return NULL;
}

USBD_DescriptorsTypeDef MIC_Desc = {
    dev_desc,
    lang_id_desc,
    manufacturer_desc,
    product_desc,
    serial_desc,
    config_desc,
    interface_desc,
};