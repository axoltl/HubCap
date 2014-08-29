#ifndef _USB_PAYLOAD_H_
#define _USB_PAYLOAD_H_

/* Functions */
void usb_payload_make_device(struct usb_device* dev);
void usb_payload_delayed_parent(struct usb_device*, struct usb_device*);

#endif /* USB_PAYLOAD_H_ */
