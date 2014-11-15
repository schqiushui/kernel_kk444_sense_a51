/*
 * Copyright (C) 2011 HTC Corporation.
 * Author: Jon Tsai <jon_tsai@htc.com>
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


#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <soc/qcom/scm.h>

#define DEVICE_NAME "simlock"

#define SIMLOCK_GET_SET_CMD	0x2596
#define SIMLOCK_GET_SECURITY_LEVEL 0x2597

#define CODE_SIZE 32
static char code[CODE_SIZE];

static int simlock_major;
static struct class *simlock_class;
static const struct file_operations simlock_fops;

void scm_flush_range(unsigned long start, unsigned long end);

static int simlock_mask;
static int unlock_mask;
static char *simlock_code = "";
static int security_level;

module_param_named(simlock_code, simlock_code, charp, S_IRUGO | S_IWUSR | S_IWGRP);

struct msg_s {
	int size; 	
	unsigned int unlock;
	char code[CODE_SIZE];
};

static int lock_set_func(const char *val, struct kernel_param *kp)
{
	int ret;

	printk(KERN_INFO "%s started(%d)...\n", __func__, strlen(val));
	ret = param_set_int(val, kp);
	printk(KERN_INFO "%s finished(%d): %d...\n", __func__, ret, simlock_mask);

	return ret;
}

static int lock_get_func(char *val, struct kernel_param *kp)
{
	int ret;

	simlock_mask = secure_read_simlock_mask();
	ret = param_get_int(val, kp);
	printk(KERN_INFO "%s: %d, %d(%x)...\n", __func__, ret, simlock_mask, simlock_mask);

	return ret;
}

static int unlock_set_func(const char *val, struct kernel_param *kp)
{
	int ret, ret2;
	static unsigned char scode[17];

	printk(KERN_INFO "%s started(%d)...\n", __func__, strlen(val));
	ret = param_set_int(val, kp);
	ret2 = strlen(simlock_code);
	strncpy(scode, simlock_code, sizeof(scode));
	scode[ret2 - 1] = 0;
	printk(KERN_INFO "%s finished(%d): %d, '%s'...\n", __func__, ret, unlock_mask, scode);
	ret2 = secure_simlock_unlock(unlock_mask, scode);
	printk(KERN_INFO "secure_simlock_unlock ret %d...\n", ret2);

	return ret;
}

static int unlock_get_func(char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_get_int(val, kp);
	printk(KERN_INFO "%s: %d, %d(%x)...\n", __func__, ret, unlock_mask, unlock_mask);

	return ret;
}

static int level_set_func(const char *val, struct kernel_param *kp)
{
	int ret;

	printk(KERN_INFO "%s started(%d)...\n", __func__, strlen(val));
	ret = param_set_int(val, kp);
	printk(KERN_INFO "%s finished(%d): %d...\n", __func__, ret, security_level);

	return ret;
}

static int level_get_func(char *val, struct kernel_param *kp)
{
	int ret;

	security_level = secure_get_security_level();
	ret = param_get_int(val, kp);
	printk(KERN_INFO "%s: %d, %d(%x)...\n", __func__, ret, security_level, security_level);

	return ret;
}

module_param_call(simlock_mask, lock_set_func, lock_get_func, &simlock_mask, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_call(unlock_mask, unlock_set_func, unlock_get_func, &unlock_mask, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_call(security_level, level_set_func, level_get_func, &security_level, S_IRUGO | S_IWUSR | S_IWGRP);

static long simlock_ioctl(struct file *file, unsigned int command, unsigned long arg)
{
	struct msg_s *msg_p = (struct msg_s *)arg;
	int size, ret;
	unsigned int unlock;

	switch (command) {
	case SIMLOCK_GET_SET_CMD:
		if (copy_from_user(&size, (void __user *)&msg_p->size, sizeof(int))) {
			printk(KERN_ERR "simlock_ioctl: copy_from_user error\n");
			return -EFAULT;
		}

		if (size > CODE_SIZE) {
			printk(KERN_ERR "simlock_ioctl: size error\n");
			return -EFAULT;
		}

		if (size > 0) {
			if (copy_from_user(&unlock, (void __user *)&msg_p->unlock, sizeof(unsigned int))) {
				printk(KERN_ERR "simlock_ioctl: copy_from_user error\n");
				return -EFAULT;
			}
			if (copy_from_user(code, (void __user *)&msg_p->code, size)) {
				printk(KERN_ERR "simlock_ioctl: copy_from_user error\n");
				return -EFAULT;
			}
			scm_flush_range((unsigned long)code, (unsigned long)(code + CODE_SIZE));
			ret = secure_simlock_unlock(unlock, code);
		} else {
			ret = secure_read_simlock_mask();
		}
		if (copy_to_user(&msg_p->size, &ret, sizeof(int))) {
			printk(KERN_ERR "simlock_ioctl: copy_to_user error\n");
			return -EFAULT;
		}
		break;
	case SIMLOCK_GET_SECURITY_LEVEL:
		ret = secure_get_security_level();
		if (copy_to_user(&msg_p->size, &ret, sizeof(int))) {
			printk(KERN_ERR "simlock_ioctl: copy_to_user error\n");
			return -EFAULT;
		}
		break;
	default:
		printk(KERN_ERR "simlock_ioctl: command error\n");
		return -EFAULT;
	}
	return ret;
}

static const struct file_operations simlock_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = simlock_ioctl,
};


static int __init simlock_init(void)
{
	int ret;

	ret = register_chrdev(0, DEVICE_NAME, &simlock_fops);
	if (ret < 0) {
		printk(KERN_ERR "simlock_init : register module fail\n");
		return ret;
	}

	simlock_major = ret;
	simlock_class = class_create(THIS_MODULE, "simlock");
	device_create(simlock_class, NULL, MKDEV(simlock_major , 0), NULL, DEVICE_NAME);

	printk(KERN_INFO "simlock_init: register module ok\n");
	return 0;
}


static void  __exit simlock_exit(void)
{
	device_destroy(simlock_class, MKDEV(simlock_major, 0));
	class_unregister(simlock_class);
	class_destroy(simlock_class);
	unregister_chrdev(simlock_major, DEVICE_NAME);
	printk(KERN_INFO "simlock_exit: un-registered module ok\n");
}

module_init(simlock_init);
module_exit(simlock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jon Tsai");

