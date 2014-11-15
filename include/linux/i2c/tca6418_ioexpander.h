/* include/linux/i2c/tca6418_ioexpander.h
 *
 * Copyright (C) 2009 HTC Corporation.
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


#ifndef _LINUX_ATMEGA_MICROP_H
#define _LINUX_ATMEGA_MICROP_H

#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>


#define IOEXPANDER_I2C_NAME "tca6418"

#define TCA6418E_Device               0x68
#define TCA6418E_Reg_GPIO_DAT_STAT1  0x14  
#define TCA6418E_Reg_GPIO_DAT_STAT2  0x15  
#define TCA6418E_Reg_GPIO_DAT_STAT3  0x16  
#define TCA6418E_Reg_GPIO_DAT_OUT1    0x17  
#define TCA6418E_Reg_GPIO_DAT_OUT2    0x18  
#define TCA6418E_Reg_GPIO_DAT_OUT3    0x19  
#define TCA6418E_Reg_GPIO_DIR1        0x23  
#define TCA6418E_Reg_GPIO_DIR2        0x24  
#define TCA6418E_Reg_GPIO_DIR3        0x25  
#define TCA6418E_GPIO_NUM             18

struct ioexp_i2c_platform_data {
	struct platform_device *ioexp_devices;
	int			num_devices;
	int			reset_gpio;
	void 			*dev_id;
	void (*setup_gpio)(void);
	void (*reset_chip)(void);
};

struct ioexp_i2c_client_data {
	struct mutex ioexp_i2c_rw_mutex;
	struct mutex ioexp_set_gpio_mutex;
	uint16_t version;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	atomic_t ioexp_is_suspend;
};

struct ioexp_ops {
	int (*init_ioexp_func)(struct i2c_client *);
};

int ioexp_i2c_read(uint8_t addr, uint8_t *data, int length);
int ioexp_i2c_write(uint8_t addr, uint8_t *data, int length);
int ioexp_gpio_set_value(uint8_t gpio, uint8_t value);
int ioexp_gpio_get_value(uint8_t gpio);
int ioexp_gpio_get_direction(uint8_t gpio);
int ioexp_read_gpio_status(uint8_t *data);
void ioexp_print_gpio_status(void);
void ioexp_register_ops(struct ioexp_ops *ops);

#endif 
