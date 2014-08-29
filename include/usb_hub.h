#ifndef _USB_HUB_H_
#define _USB_HUB_H_

/* CONFIG */
#define USB_HUB_NO_DEVICES      32
#define USB_HUB_MAX_HUBS        4

/* Structures */
struct hub_port {
    struct usb_device* dev;
    uint16_t status;
    uint16_t change;
};

struct hub_ctx {
    unsigned no_ports;
    struct hub_port ports[USB_HUB_NO_DEVICES];
    void* ext;
};

/* Descriptor types */
#define DESC_HUB                0x29

/* Requests */
#define GET_STAT                0x00

/* Port feature definitions */
#define PORT_ENABLE             1
#define PORT_RESET              4
#define PORT_POWER              8
#define C_PORT_CONN             16
#define C_PORT_RESET            20

/* Port mask definitions */
#define PORT_ENABLE_MASK        0x0002
#define PORT_POWER_MASK         0x100
#define PORT_EMPTY              (0 | PORT_POWER)
#define PORT_FULL               0x0103
#define C_PORT_CONN_MASK        0x0001
#define C_PORT_RESET_MASK       0x0010

/* Functions */
void usb_hub_make_device(struct usb_device* dev);
void usb_hub_add_device(struct usb_device*, struct usb_device*);

/* Internal functions */
void usb_port_class_request(struct usb_device* ctx, uint8_t req,
                            uint16_t val, uint16_t idx, uint16_t len,
                            void** buffer, uint16_t* ret_len);
#endif /* USB_HUB_H_ */
