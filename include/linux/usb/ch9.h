#ifndef __LINUX_USB_CH9_H
#define __LINUX_USB_CH9_H

#include <uapi/linux/usb/ch9.h>


#define USB_REQ_HTC_FUNCTION		0x01

#define USB_WVAL_ADB				0x01

extern const char *usb_speed_string(enum usb_device_speed speed);


extern const char *usb_state_string(enum usb_device_state state);

#endif 
