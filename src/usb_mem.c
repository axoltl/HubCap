#include <usb_device.h>
#include <stdint.h>
#include <uart.h>
#include <string.h>

#define DUMMY_IF(n)     0x03, 0x04, (n)
#define TOT_LEN(n, m)   (9 + (n * 3) + m)

static uint8_t conf_descriptor[TOT_LEN(8,0x50)] = {
    0x09,           // Length
    0x02,           // Type
    TOT_LEN(8,0x50), 0x00,     // TotalLength
    0x00,           // numInterfaces
    0x01,           // confValue
    0x00,           // String
    0x60,           // Attrs
    0x00,           // maxPower

    DUMMY_IF(0),
    DUMMY_IF(1),
    DUMMY_IF(2),
    DUMMY_IF(3),
    DUMMY_IF(4),
    DUMMY_IF(5),
    DUMMY_IF(6),
    DUMMY_IF(7),
    0x50,
    0x04,
    8,
    0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0x00,0x00,0x00,0x00,
// hub_index overwrite
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
// dev_index overwrite  (-1)
    0xFF,0xFF,0xFF,0xFF

};

#define HUB_INDEX_OFF   (sizeof(conf_descriptor) - 16)
#define DEV_INDEX_OFF   (sizeof(conf_descriptor) - 4)

void usb_mem_write_hub_index(uint32_t val){
    conf_descriptor[HUB_INDEX_OFF + 0] = (val >> 0) & 0xFF; 
    conf_descriptor[HUB_INDEX_OFF + 1] = (val >> 8) & 0xFF; 
    conf_descriptor[HUB_INDEX_OFF + 2] = (val >> 16) & 0xFF; 
    conf_descriptor[HUB_INDEX_OFF + 3] = (val >> 24) & 0xFF; 
}

void usb_mem_write_dev_index(uint32_t val){
    conf_descriptor[DEV_INDEX_OFF + 0] = (val >> 0) & 0xFF; 
    conf_descriptor[DEV_INDEX_OFF + 1] = (val >> 8) & 0xFF; 
    conf_descriptor[DEV_INDEX_OFF + 2] = (val >> 16) & 0xFF; 
    conf_descriptor[DEV_INDEX_OFF + 3] = (val >> 24) & 0xFF; 
}

void usb_mem_write_to(uint32_t addr) {
    uart_print("I'll be writing to: 0x%08lX ", addr);

    addr = -((HUB_ARRAY_ADDR - addr) >> 4);
    usb_mem_write_hub_index(addr); 
    uart_print("(offset is %08lX)\n", addr); 
}

static const uint8_t dev_descriptor[] = {
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

void usb_mem_init(struct usb_device*);
void usb_mem_handle_config( struct usb_device*, uint8_t, uint8_t,
                            uint16_t, uint16_t, uint16_t,
                            void**, uint16_t*);
void usb_mem_handle_ep(struct usb_device*);
void usb_mem_reset(struct usb_device*);

const struct usb_device usb_mem = {
    0,
    NULL,
    usb_mem_init,
    usb_mem_handle_config,
    usb_mem_handle_ep,
    usb_mem_reset,
    NULL,
    0
};

void usb_mem_make_device(struct usb_device* dev){
    memcpy(dev, &usb_mem, sizeof(usb_mem));
}

void usb_mem_init(struct usb_device* ctx){
}

#define min(n, m)       ((n < m) ? n : m)
void usb_mem_get_descriptor(uint8_t type, uint8_t idx,
                            uint16_t lang, uint16_t len,
                            void** buffer, uint16_t* retlen){
    switch(type){
        case DESC_DEV:
            *retlen = min(len, sizeof(dev_descriptor));
            *buffer = (void*)dev_descriptor;
            uart_print("Sending dev descriptor (%d)\n", *retlen);
            break;
        case DESC_CONF:
            *retlen = min(len, sizeof(conf_descriptor));
            *buffer = (void*)conf_descriptor;
            uart_print("Sending conf descriptor (%d)\n", *retlen);
            break;
    }
}

void usb_mem_std_request(   struct usb_device* ctx, uint8_t req, uint16_t val,
                            uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len){
    static uint8_t ret = 0x01;
    switch(req){
        case GET_DESC:
            usb_mem_get_descriptor(val >> 8, val, idx, len, buffer, ret_len);
            if(ctx->addr == 0 && ctx->parent != NULL && !(ctx->feature & FEAT_HAS_ADDR)) {
                // We have to hand off to the parent to handle the port reset
                usb_set_device(ctx->parent, 0);
                ctx->feature |= FEAT_HAS_ADDR;
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
            // This is going to kill this device, but the damage is already done.
            *buffer = (void*)1;
            *ret_len = 0xFFFF;
            if(ctx->parent != NULL) {
                // Hand off to the parent, we're done here.
                usb_set_device(ctx->parent, 1);
            }
            break;
    }
}

void usb_mem_handle_config(struct usb_device* ctx,
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
                    usb_mem_std_request(ctx, req, val, idx, len, buffer, ret_len);
                    break;
            }
            break;
        case REC_INTERFACE:
        case REC_ENDPOINT:
        case REC_OTHER:
            break;
    }
}

void usb_mem_handle_ep(struct usb_device* ctx){

}

void usb_mem_reset(struct usb_device* ctx){
    ctx->addr = 0;
    ctx->feature = 0;
}

