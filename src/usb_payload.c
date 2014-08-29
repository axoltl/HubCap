#include <usb_device.h>
#include <usb_hub.h>
#include <constants.h>

#include <stdint.h>
#include <uart.h>
#include <string.h>

static uint8_t payload_buf[4] = {};

static struct hub_ctx hub[2] = {};
static int no_hub = 0;

static const uint8_t conf_descriptor[] = {
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
    

static const uint8_t dev_descriptor[] = {
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

#define HUB_DESC_TYPE       (0x29)

CONST_STORAGE static const uint8_t payload_descriptor_1[] = {
    PAYLOAD_SIZE,
    HUB_DESC_TYPE,
    0x02,       // No devices
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,       // End of regular descriptor

/* padding */
    PAYLOAD_PAD,
/* base_ptr */
    HUB_BASE_PTR,     // Pointer into the stack, known to have zeroes.
/* hub_array */
    HUB_NEW_ARRAY_PTR
};

CONST_STORAGE static const uint8_t payload_descriptor_2[] = {
    0xFF,
    HUB_DESC_TYPE,
    0x01,       // No devices
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,       // End of regular descriptor

    // Padding
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    // The actual payload that's going on the stack.
    #include <payload.h>
};

static const uint8_t* payload_descriptor[] = {
    payload_descriptor_1,
    payload_descriptor_2
};

static const unsigned payload_size[] = {
    sizeof(payload_descriptor_1),
    sizeof(payload_descriptor_2)
}; 

void usb_payload_init(struct usb_device*);
void usb_payload_handle_config( struct usb_device*, uint8_t, uint8_t,
                            uint16_t, uint16_t, uint16_t,
                            void**, uint16_t*);
void usb_payload_handle_ep(struct usb_device*);
void usb_payload_reset(struct usb_device*);

const struct usb_device usb_payload = {
    0,
    NULL,
    usb_payload_init,
    usb_payload_handle_config,
    usb_payload_handle_ep,
    usb_payload_reset,
    NULL,
    0
};

void usb_payload_make_device(struct usb_device* dev){
    memcpy(dev, &usb_payload, sizeof(usb_payload));
}

void usb_payload_init(struct usb_device* ctx){
    // We've already initted.
    if(ctx->data)
        return;

    struct hub_ctx* hctx = &hub[no_hub++];
    struct hub_port* ports = hctx->ports;
    hctx->no_ports = 0;
    for(int i = 0; i < USB_HUB_NO_DEVICES; i++) {
        ports[i].dev = NULL;    
        ports[i].status = PORT_EMPTY;
        ports[i].change = 0;
    }
    ctx->data = hctx;
}

void usb_payload_delayed_parent(struct usb_device* ctx, struct usb_device* parent) {
    struct hub_ctx* hctx = (struct hub_ctx*)ctx->data;
    hctx->ext = parent;
}

/* Replug all the devices */
void usb_payload_reinit(struct usb_device* ctx){
    struct hub_ctx* hctx = (struct hub_ctx*)ctx->data;
    struct hub_port* ports = hctx->ports;
    for(int i = 0; i < hctx->no_ports; i++){
        ports[i].status = PORT_FULL;
        ports[i].change = C_PORT_CONN_MASK;
    }        
}

unsigned get_hub_idx(struct hub_ctx* hctx) {
    for(int i = 0; i < (sizeof(hub) / sizeof(hub[0])); i++) {
        if(hctx == &hub[i])
            return i;
    }
    // Eeeh, we dunno.
    return 0;
}

#define min(n, m)       ((n < m) ? n : m)
void usb_payload_get_descriptor(struct usb_device* ctx, uint8_t type, uint8_t idx,
                            uint16_t lang, uint16_t len,
                            void** buffer, uint16_t* retlen){
    struct hub_ctx* hctx = (struct hub_ctx*) ctx->data;

    // Yes, this isn't the best solution to this problem. We're using the
    // hub index to select which payload to send.
    unsigned hidx = get_hub_idx(hctx);
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
        case DESC_HUB:
            *retlen = min(len, payload_size[hidx]);
            *buffer = (void*)payload_descriptor[hidx];
            uart_print("Sending hub descriptor (%d) (%d)\n", *retlen, hidx);

            *retlen |= CONST_PTR;
            break;
    }
}

void usb_payload_std_request(   struct usb_device* ctx, uint8_t req, uint16_t val,
                            uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len){
    struct hub_ctx* hctx = (struct hub_ctx*)ctx->data;
    switch(req){
        case GET_DESC:
            usb_payload_get_descriptor(ctx, val >> 8, val, idx, len, buffer, ret_len);
            if(ctx->addr == 0 && ctx->parent != NULL && !(ctx->feature & FEAT_HAS_ADDR)) {
                // We have to hand off to the parent to handle the port reset
                usb_set_device(ctx->parent, 0);
                ctx->feature |= FEAT_HAS_ADDR;
            }
            break;
        case SET_ADDR:
            if(hctx->ext)
                ctx->parent = (struct usb_device*)hctx->ext;

            ctx->addr = val;
            usb_set_addr(val);
            *buffer = payload_buf;
            *ret_len = 0;
            break;
        case GET_CONF:
            payload_buf[0] = 0x01;
            *buffer = payload_buf;
            *ret_len = 1;
            break;
        case SET_FEAT:
        case SET_CONF:
            *buffer = payload_buf;
            *ret_len = 0;
            break;
    }
}

void usb_payload_class_request( struct usb_device* ctx, uint8_t req,
                            uint16_t val, uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len) {

    switch(req){
        case GET_STAT:
            memset(payload_buf, 0, 4);
            *buffer = payload_buf;
            *ret_len = 4;
            break;
        case GET_DESC:
            usb_payload_get_descriptor(ctx, val >> 8, val, idx, len, buffer, ret_len);
            break;
    }
}

void usb_payload_handle_config( struct usb_device* ctx,
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
                    usb_payload_std_request(ctx, req, val, idx, len, buffer, ret_len);
                    break;
                case TYPE_CLS:
                    usb_payload_class_request(ctx, req, val, idx, len, buffer, ret_len);
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

void usb_payload_handle_ep(struct usb_device* ctx){

}

extern void usb_mem_write_dev_index(uint32_t);

void usb_payload_reset(struct usb_device* ctx){
    ctx->addr = 0;
    ctx->feature = 0;
    usb_mem_write_dev_index(29);
    usb_payload_reinit(ctx);
}

