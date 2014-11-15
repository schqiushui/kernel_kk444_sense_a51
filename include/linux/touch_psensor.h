/* include/linux/touch_psensor.h
 *
 * Copyright (C) 2010 HTC, Inc.
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

#ifndef __LINUX_PROXIMITY_H
#define __LINUX_PROXIMITY_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define TOUCH_PSENSOR_IOCTL_MAGIC 'c'
#define TOUCH_PSENSOR_IOCTL_GET_ENABLED \
		_IOR(TOUCH_PSENSOR_IOCTL_MAGIC, 1, int *)
#define TOUCH_PSENSOR_IOCTL_ENABLE \
		_IOW(TOUCH_PSENSOR_IOCTL_MAGIC, 2, int *)

#ifdef __KERNEL__
struct psensor_platform_data {
	int intr;
	uint32_t irq_gpio_flags;
};

extern void touch_report_psensor_input_event(int status);
extern int psensor_enable_by_touch_driver(int on);
#endif

#endif
