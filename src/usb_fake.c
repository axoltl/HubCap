#include <usb_device.h>
#include <stdint.h>
#include <uart.h>
#include <string.h>

static uint8_t conf_descriptor[] = {
    0x09,           // Length
    0x02,           // Type
    0x12, 0x00,     // TotalLength
    0x01,           // numInterfaces
    0x01,           // confValue
    0x00,           // String
    0x60,           // Attrs
    0x00,           // maxPower

    /* Interface Desc */
    0x09,           // Length
    0x04,           // Type
    0x00,           // Num
    0x00,           // Alt
    0x00,           // No EPs
    0xFF,           // Vendor specific
    0xFF,           // Subclass
    0xFF,           // Protocol
    0x00,           // String
};
    

CONST_STORAGE static const uint8_t dev_descriptor[] = {
    0x12,
    0x01,
    0x00, 0x02,
    0xFF,
    0xFF,
    0xFF,
    8,
    0xDE, 0xAD,
    0xBE, 0xEF,
    0x00, 0x00,
    0x00,
    0x00,
    0x00,
    0x01
};

void usb_fake_init(struct usb_device*);
void usb_fake_handle_config( struct usb_device*, uint8_t, uint8_t,
                            uint16_t, uint16_t, uint16_t,
                            void**, uint16_t*);
void usb_fake_handle_ep(struct usb_device*);
void usb_fake_reset(struct usb_device*);

const struct usb_device usb_fake = {
    0,
    NULL,
    usb_fake_init,
    usb_fake_handle_config,
    usb_fake_handle_ep,
    usb_fake_reset,
    NULL,
    FEAT_EARLY_EXIT
};

void usb_fake_make_device(struct usb_device* dev){
    memcpy(dev, &usb_fake, sizeof(usb_fake));
}

void usb_fake_init(struct usb_device* ctx){
}

#define min(n, m)       ((n < m) ? n : m)
void usb_fake_get_descriptor(uint8_t type, uint8_t idx,
                            uint16_t lang, uint16_t len,
                            void** buffer, uint16_t* retlen){
    switch(type){
        case DESC_DEV:
            *retlen = min(len, sizeof(dev_descriptor));
            *buffer = (void*)dev_descriptor;
            uart_print("Sending dev descriptor (%d)\n", *retlen);
            *retlen |= CONST_PTR;
            break;
        case DESC_CONF:
            *retlen = min(len, sizeof(conf_descriptor));
            *buffer = (void*)conf_descriptor;
            uart_print("Sending conf descriptor (%d)\n", *retlen);
            break;
    }
}

void usb_fake_std_request(   struct usb_device* ctx, uint8_t req, uint16_t val,
                            uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len){
    static uint8_t ret = 0x01;
    switch(req){
        case GET_DESC:
            // This is hacky, but we're dropping a packet in the hub
            // so this is the only place to do it...
            ctx->feature |= FEAT_HAS_RESET;
            usb_fake_get_descriptor(val >> 8, val, idx, len, buffer, ret_len);
            if(ctx->addr == 0 && ctx->parent != NULL) {
                // We have to hand off to the parent to handle the port reset
                usb_set_device(ctx->parent, 0);
            }
            break;
        case SET_ADDR:
            ctx->addr = val;
            usb_set_addr(val);
            *buffer = (void*)1;
            *ret_len = 0;
            break;
        case GET_CONF:
            *buffer = (void*)&ret;
            *ret_len = 1;
            break;
        case SET_FEAT:
        case SET_CONF:
            // This is going to kill this device, but that's fine. We don't need it.
            *buffer = (void*)1;
            *ret_len = 0xFFFF;
            if(ctx->parent != NULL) {
                // Hand off to the parent, we're done here.
                usb_set_device(ctx->parent, 1);
            }
            break;
    }
}

void usb_fake_handle_config(struct usb_device* ctx,
                            uint8_t type,
                            uint8_t req,
                            uint16_t val,
                            uint16_t idx,
                            uint16_t len,
                            void** buffer,
                            uint16_t* ret_len){

    uint8_t dir = (type >> 7) & 0x1;
    uint8_t rec = (type >> 0) & 0xF;
    type = (type >> 5) & 0x3;

    switch(rec){
        case REC_DEVICE:
            switch(type){
                case TYPE_STD:
                    usb_fake_std_request(ctx, req, val, idx, len, buffer, ret_len);
                    break;
            }
            break;
        case REC_INTERFACE:
        case REC_ENDPOINT:
        case REC_OTHER:
            break;
    }
}

void usb_fake_handle_ep(struct usb_device* ctx){

}

void usb_fake_reset(struct usb_device* ctx){
    ctx->addr = 0;
    // Preserve the retain features.
    ctx->feature &= FEAT_RETAIN_FLAGS;

    // This obviously ruins the entire thing...
    if(ctx->feature & FEAT_CORRUPT) {
        conf_descriptor[1] = 0xFF;
    }
}

