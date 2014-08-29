#ifndef _USB_DEVICE_H_
#define _USB_DEVICE_H_

#include <stdint.h>
#include <constants.h>
#include <avr/pgmspace.h>

#define CONST_STORAGE   PROGMEM
#define CONST_PTR       0x8000

/* Config */
#define USB_MAX_DEVICES 16

/* Descriptor types */
#define DESC_DEV        0x01
#define DESC_CONF       0x02

/* Recipients */
#define REC_DEVICE      0x00
#define REC_INTERFACE   0x01
#define REC_ENDPOINT    0x02
#define REC_OTHER       0x03

/* Standard requests */
#define CLR_FEAT        0x01
#define SET_FEAT        0x03
#define SET_ADDR        0x05
#define GET_DESC        0x06
#define GET_CONF        0x08
#define SET_CONF        0x09

/* Request types */
#define TYPE_STD        0x00
#define TYPE_CLS        0x01

/* USB device features */
#define FEAT_EARLY_EXIT 0x01
#define FEAT_HAS_ADDR   0x02
#define FEAT_HAS_RESET  0x04
#define FEAT_BROKEN     0x08
#define FEAT_CORRUPT    0x10

#define FEAT_RETAIN_FLAGS   (FEAT_EARLY_EXIT | FEAT_BROKEN | FEAT_CORRUPT)

/* Structures */
struct usb_device {
    uint8_t addr;
    struct usb_device* parent;
    void(*init)(struct usb_device* ctx);
    void(*handle_configuration)(    struct usb_device* ctx,
                                    uint8_t type,
                                    uint8_t req,
                                    uint16_t val,
                                    uint16_t idx,
                                    uint16_t len,
                                    void** buffer,
                                    uint16_t* ret_len);
    void(*handle_endpoint)(struct usb_device* ctx);
    void(*reset)(struct usb_device* ctx);
    void* data;
    uint8_t feature;
};

/* Functions */
void usb_set_addr(uint8_t val);
void usb_set_device(struct usb_device*, int);
void usb_init(void);
void usb_do(void);
void usb_setup_hardware(void);
void usb_update_led(void);

#endif /* _USB_DEVICE_H_ */
