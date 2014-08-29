#ifndef _USB_MEM_H_
#define _USB_MEM_H_

/* Functions */
void usb_mem_make_device(struct usb_device* dev);
void usb_mem_write_to(uint32_t addr);

#endif /* USB_MEM_H_ */
