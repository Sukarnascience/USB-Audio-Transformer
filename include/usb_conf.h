#ifndef USB_CONF_H
#define USB_CONF_H

/*
 * usb_conf.h
 * USB UAC configuration constants.
 * Placeholder for M1 - fully populated in M3.
 *
 * UAC = USB Audio Class 1.0
 * The device will enumerate as a single mono microphone.
 *
 * Audio parameters:
 *   Sample rate  : 16000 Hz (16 kHz - adequate for voice, low USB bandwidth)
 *   Bit depth    : 16-bit PCM signed
 *   Channels     : 1 (mono)
 *   Packet size  : 32 bytes per 1ms USB frame (16 samples * 2 bytes)
 */

#define USBD_VID                    0x0483   /* STMicroelectronics VID     */
#define USBD_PID                    0x5730   /* Custom PID for this device */
#define USBD_MANUFACTURER_STRING    "STM32_UAC"
#define USBD_PRODUCT_STRING         "VoiceMod Microphone"
#define USBD_SERIALNUMBER_STRING    "00000000001A"

#define AUDIO_SAMPLE_RATE           16000U   /* Hz                         */
#define AUDIO_BIT_DEPTH             16U      /* bits per sample            */
#define AUDIO_CHANNELS              1U       /* mono                       */

/* Bytes per 1ms isochronous USB audio frame */
#define AUDIO_PACKET_SZ             ((AUDIO_SAMPLE_RATE / 1000U) * \
                                     (AUDIO_BIT_DEPTH  / 8U)    * \
                                     AUDIO_CHANNELS)             /* = 32 */

#endif /* USB_CONF_H */