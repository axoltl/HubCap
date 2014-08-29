#include <usb_device.h>
#include <usb_hub.h>
#include <stdint.h>
#include <uart.h>
#include <string.h>

#define USB_NO_VAR_BYTES    ((USB_HUB_NO_DEVICES + 7) / 8)

#if USB_HUB_NO_DEVICES > 40
#error "Only support up to 40 devices"
#endif


/*  Ok, so, this is a shared response buffer, between all hub instances.
    The contents are only live for a single response (it gets filled, sent to
    the host, and then the contents are invalid). So we can safely share it,
    because we'll only process one transaction at a time. */

uint8_t hub_buf[4] = {};

struct hub_ctx hubs[USB_HUB_MAX_HUBS];
unsigned no_hubs = 0;

CONST_STORAGE static const uint8_t conf_descriptor[] = {
    0x09,           // Length
    0x02,           // Type
    0x19, 0x00,     // TotalLength
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
    0x01,           // No EPs
    0x09,           // HUB
    0x00,           // Subclass
    0x00,           // Protocol
    0x00,           // String
    
    /* Endpoint desc */
    0x07,           // Length
    0x05,           // Type
    0x81,           // Addr
    0x03,           // Attr (intr)
    0x40, 0x00,     // MaxPacket
    0xFF            // Interval
};
    

CONST_STORAGE static const uint8_t dev_descriptor[] = {
    0x12,
    0x01,
    0x00, 0x02,
    0x09,
    0x00,
    0x00,
    8,
    0xDE, 0xAD,
    0xBE, 0xEF,
    0x00, 0x00,
    0x00,
    0x00,
    0x00,
    0x01
};

#define HUB_DESC_LENGTH     (0x06 + USB_NO_VAR_BYTES * 2)
#define HUB_DESC_TYPE       (0x29)

CONST_STORAGE static const uint8_t hub_descriptor[] = {
    HUB_DESC_LENGTH,
    HUB_DESC_TYPE,
    USB_HUB_NO_DEVICES,
    0x00,
    0x01,
    0x01,
#if USB_NO_VAR_BYTES > 1
    0x00, 0x00,
#endif
#if USB_NO_VAR_BYTES > 2
    0x00, 0x00,
#endif
#if USB_NO_VAR_BYTES > 3
    0x00, 0x00,
#endif
#if USB_NO_VAR_BYTES > 4
    0x00, 0x00,
#endif
    0x00,
    0x00
};

void usb_hub_init(struct usb_device*);
void usb_hub_handle_config( struct usb_device*, uint8_t, uint8_t,
                            uint16_t, uint16_t, uint16_t,
                            void**, uint16_t*);
void usb_hub_handle_ep(struct usb_device*);
void usb_hub_reset(struct usb_device*);

const struct usb_device usb_hub = {
    0,
    NULL,
    usb_hub_init,
    usb_hub_handle_config,
    usb_hub_handle_ep,
    usb_hub_reset,
    NULL,
    0
};

void usb_hub_make_device(struct usb_device* dev){
    memcpy(dev, &usb_hub, sizeof(usb_hub));
}

void usb_hub_add_device(struct usb_device* ctx, struct usb_device* dev) {
    struct hub_ctx* hctx = (struct hub_ctx*)ctx->data;
    unsigned no_ports = hctx->no_ports;
    struct hub_port* ports = hctx->ports;

    if(no_ports >= USB_HUB_NO_DEVICES)
        return;

    dev->parent = ctx;

    ports[no_ports].dev = dev;
    ports[no_ports].status = PORT_FULL;
    ports[no_ports].change = C_PORT_CONN_MASK;
    hctx->no_ports++;
}

void usb_hub_init(struct usb_device* ctx){
    if(no_hubs >= USB_HUB_MAX_HUBS)
        return;

    if(ctx->data)
        return;

    struct hub_ctx* hub = &hubs[no_hubs++];
    ctx->data = hub;

    struct hub_port* ports = hub->ports;
    hub->no_ports = 0;
    for(int i = 0; i < USB_HUB_NO_DEVICES; i++){
        ports[i].dev = NULL;
        ports[i].status = PORT_EMPTY;
        ports[i].change = 0;
    } 
}

#define min(n, m)       ((n < m) ? n : m)
void usb_hub_get_descriptor(uint8_t type, uint8_t idx,
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
            *retlen |= CONST_PTR;
            break;
        case DESC_HUB:
            *retlen = min(len, sizeof(hub_descriptor));
            *buffer = (void*)hub_descriptor;
            uart_print("Sending hub descriptor (%d)\n", *retlen);
            *retlen |= CONST_PTR;
            break;
    }
}

void usb_hub_std_request(   struct usb_device* ctx, uint8_t req, uint16_t val,
                            uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len){
    switch(req){
        case GET_DESC:
            usb_hub_get_descriptor(val >> 8, val, idx, len, buffer, ret_len);
            if(ctx->addr == 0 && ctx->parent != NULL && !(ctx->feature & FEAT_HAS_ADDR)) {
                // We have to hand off to the parent to handle the port reset
                usb_set_device(ctx->parent, 0);
                ctx->feature |= FEAT_HAS_ADDR;
            }
            break;
        case SET_ADDR:
            ctx->addr = val;
            usb_set_addr(val);
            *buffer = hub_buf;
            *ret_len = 0;
            break;
        case GET_CONF:
            hub_buf[0] = 0x01;
            *buffer = hub_buf;
            *ret_len = 1;
            break;
        case SET_FEAT:
        case SET_CONF:
            *buffer = hub_buf;
            *ret_len = 0;
            break;
    }
}

static inline void make_status(struct hub_port* p, uint8_t* buf) {
    buf[0] = (p->status >> 0) & 0xFF;
    buf[1] = (p->status >> 8) & 0xFF;
    buf[2] = (p->change >> 0) & 0xFF;
    buf[3] = (p->change >> 8) & 0xFF;
}   
 
void usb_hub_class_request( struct usb_device* ctx, uint8_t req, 
                            uint16_t val, uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len) {
    
    switch(req){
        case GET_STAT:
            memset(hub_buf, 0, 4);
            *buffer = hub_buf;
            *ret_len = 4;
            break;
        case GET_DESC:
            usb_hub_get_descriptor(val >> 8, val, idx, len, buffer, ret_len);
            break;
    }
}

void usb_port_class_request(struct usb_device* ctx, uint8_t req,
                            uint16_t val, uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len) {
    struct hub_ctx* hctx = (struct hub_ctx*)ctx->data;
    struct hub_port* port = &hctx->ports[idx - 1];

    if(idx == 0 || idx > USB_HUB_NO_DEVICES)
        return;

    switch(req) {
        case GET_STAT:
            uart_print("Sending status for port %d\n", idx);
            if( ((port->dev->feature & FEAT_EARLY_EXIT) &&
                (port->dev->feature & FEAT_HAS_RESET)) || 
                (port->dev->feature & FEAT_BROKEN)) {
                port->change |= C_PORT_CONN_MASK;
            }
            make_status(port, hub_buf);
            *buffer = hub_buf;
            *ret_len = 4;
            break;
        case CLR_FEAT:
            switch(val) {
                case PORT_ENABLE:
                    port->status &= ~PORT_ENABLE_MASK;
                    *buffer = hub_buf;
                    *ret_len = 0;
                    break;
                case C_PORT_CONN:
                    port->change &= ~C_PORT_CONN_MASK;
                    *buffer = hub_buf;
                    *ret_len = 0;
                    break;
                case C_PORT_RESET:
                    *buffer = hub_buf;
                    // Hack, quiesce any response. This is a race that
                    // we need to win.
                    *ret_len = 0xFFFF;
                    // Forcibly change the address.
                    usb_set_device(port->dev, 1);
                    uart_print("%d\n", port->dev->addr);
                    usb_update_led();
                    break;
            }
        case SET_FEAT:
            switch(val) {
                case PORT_RESET:
                    port->change |= C_PORT_RESET_MASK;
                    port->dev->reset(port->dev);
                    *buffer = hub_buf;
                    *ret_len = 0;
                    break;
                case PORT_POWER:
                    port->status |= PORT_POWER_MASK;
                    *buffer = hub_buf;
                    *ret_len = 0;
                    break;
            }
    }
}

void usb_hub_handle_config( struct usb_device* ctx,
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
                    usb_hub_std_request(ctx, req, val, idx, len, buffer, ret_len);
                    break;
                case TYPE_CLS:
                    usb_hub_class_request(ctx, req, val, idx, len, buffer, ret_len);
                    break;
            }
            break;
        case REC_INTERFACE:
        case REC_ENDPOINT:
        case REC_OTHER:
            switch(type) {
                case TYPE_CLS:
                    usb_port_class_request(ctx, req, val, idx, len, buffer, ret_len);
                    break;
            }
            break;
    }
}

void usb_hub_handle_ep(struct usb_device* ctx){

}

void usb_hub_reset(struct usb_device* ctx){
    ctx->addr = 0;
    ctx->feature = 0;
}

