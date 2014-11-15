/*
 * Platform data for Android USB
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef	__LINUX_USB_ANDROID_H
#define	__LINUX_USB_ANDROID_H

#include <linux/usb/composite.h>

#define MAX_STREAMING_FUNCS 3
#define FUNC_NAME_LEN 10

enum android_pm_qos_state {
	WFI,
	IDLE_PC,
	IDLE_PC_RPM,
	NO_USB_VOTE,
	MAX_VOTES = NO_USB_VOTE,
};

struct android_usb_platform_data {
	int (*update_pid_and_serial_num)(uint32_t, const char *);
	u32 pm_qos_latency[MAX_VOTES];
	u8 usb_core_id;
	char streaming_func[MAX_STREAMING_FUNCS][FUNC_NAME_LEN];
	int  streaming_func_count;


	
	__u16 vendor_id;
	
	__u16 product_id;

	char *product_name;
	char *manufacturer_name;
	char *serial_number;

	const char *fserial_init_string;

	const char *usb_rmnet_interface;
	const char *usb_diag_interface;

	unsigned char diag_init:1;
	unsigned char modem_init:1;
	unsigned char rmnet_init:1;
	unsigned char reserved:5;

	int (*match)(int product_id, int intrsharing);

	

	
	int nluns;
	int cdrom_lun;
	bool internal_ums;
	int vzw_unmount_cdrom;

	u8 uicc_nluns;
	bool cdrom;
};

#ifndef CONFIG_TARGET_CORE
static inline int f_tcm_init(int (*connect_cb)(bool connect))
{
	return 0;
}
static inline void f_tcm_exit(void)
{
}
static inline int tcm_bind_config(struct usb_configuration *c)
{
	return -ENODEV;
}
#endif

extern int gport_setup(struct usb_configuration *c);
extern void gport_cleanup(void);
extern int gserial_init_port(int port_num, const char *name,
					const char *port_name);

int acm_port_setup(struct usb_configuration *c);
void acm_port_cleanup(void);
int acm_init_port(int port_num, const char *name);


enum fserial_func_type {
	USB_FSER_FUNC_NONE,
	USB_FSER_FUNC_SERIAL,
	USB_FSER_FUNC_MODEM,
	USB_FSER_FUNC_MODEM_MDM,
	USB_FSER_FUNC_ACM,
	USB_FSER_FUNC_AUTOBOT,
};

#endif	
