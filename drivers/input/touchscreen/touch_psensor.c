/* drivers/input/touchscreen/touch_psensor.c - touch psensor driver
 * It will used to send out events to the input system
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
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <mach/board.h>
#include <linux/of.h>
#include <mach/devices_cmdline.h>
#include <linux/async.h>
#include <linux/touch_psensor.h>

#define D(x...) pr_info(x)

static void report_near_do_work(struct work_struct *w);
static DECLARE_DELAYED_WORK(report_near_work, report_near_do_work);
static int is_probe_success;
static int p_status = 1;
static struct touch_psensor_info *lp_info;
static int ps_hal_enable, ps_drv_enable;
static int phone_status;
static int psensor_enable_by_touch = 0;

extern int proximity_enable_from_ps(int on);

module_param(p_status, int, 0444);

struct touch_psensor_info {
	struct class *touch_psensor_class;
	struct device *ls_dev;
	struct device *ps_dev;

	struct input_dev *ls_input_dev;
	struct input_dev *ps_input_dev;
	struct workqueue_struct *lp_wq;
	struct wake_lock ps_wake_lock;
	int model;
	int intr_pin;
	int ps_enable;
	int ps_irq_flag;
	int psensor_opened;
	uint16_t ps_delay_time;
};

static int psensor_enable(struct touch_psensor_info *lpi)
{
	lpi->ps_enable = 1;
	D("[PS][TP] %s %d\n", __func__, lpi->ps_enable);
	return 0;
}

static int psensor_disable(struct touch_psensor_info *lpi)
{
	lpi->ps_enable = 0;
	D("[PS][TP] %s %d\n", __func__, lpi->ps_enable);
	return 0;
}

static void report_near_do_work(struct work_struct *w)
{
	struct touch_psensor_info *lpi = lp_info;

	D("[PS][TP] %s: delay %dms, report proximity NEAR\n", __func__, lpi->ps_delay_time);

	input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 0);
	input_sync(lpi->ps_input_dev);
}

int psensor_enable_by_touch_driver(int on)
{
	struct touch_psensor_info *lpi = lp_info;

	if (!is_probe_success) {
		D("[PS][TP] %s return by touch_psensor probe fail\n", __func__);
		return 0;
	}
	psensor_enable_by_touch = 1;

	D("[PS][TP] %s on:%d\n", __func__, on);
	if (on) {
		psensor_enable(lpi);
	} else {
		psensor_disable(lpi);
	}

	psensor_enable_by_touch = 0;
	return 0;
}
EXPORT_SYMBOL_GPL(psensor_enable_by_touch_driver);


void touch_report_psensor_input_event(int status)
{
	struct touch_psensor_info *lpi = lp_info;
	int ps_status;

	ps_status = status;
	D("[PS][TP] %s ps_status=%d\n",__func__,ps_status);
	input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, ps_status);
	input_sync(lpi->ps_input_dev);
}

EXPORT_SYMBOL_GPL(touch_report_psensor_input_event);


static ssize_t touch_proximity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int exist = 1;
	return snprintf(buf, sizeof(int), "%d", exist);
}

static DEVICE_ATTR(touch_proximity, S_IRUGO,
	touch_proximity_show, NULL);

static ssize_t phone_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "phone_status = %d\n", phone_status);
}

static ssize_t phone_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int phone_status1 = 0;
	sscanf(buf, "%d" , &phone_status1);

	phone_status = phone_status1;

	D("[PS][TP] %s: phone_status = %d\n", __func__, phone_status);

	if ((phone_status == 1)||(phone_status == 2)) 
	{
		D("[PS][TP] %s proximity on\n",__func__);
		proximity_enable_from_ps(1);
	}
	if (phone_status == 0) 
	{
		D("[PS][TP] %s proximity off\n",__func__);
		proximity_enable_from_ps(0);
	}

	return count;
}
static DEVICE_ATTR(PhoneApp_status, 0666, phone_status_show, phone_status_store);

static int psensor_open(struct inode *inode, struct file *file)
{
	struct touch_psensor_info *lpi = lp_info;

	if (lpi->psensor_opened)
		return -EBUSY;

	lpi->psensor_opened = 1;
	return 0;
}

static int psensor_release(struct inode *inode, struct file *file)
{
	struct touch_psensor_info *lpi = lp_info;

	D("[PS][TP] %s\n", __func__);
	phone_status = 0;
	lpi->psensor_opened = 0;
	return 0;
}

static long psensor_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int val;
	struct touch_psensor_info *lpi = lp_info;

	D("[PS][TP] %s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case TOUCH_PSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg))
			return -EFAULT;
		if (val) {
				ps_hal_enable = 1;
			return 0;
		} else {
				ps_hal_enable = 0;
			return 0;
		}
		break;
	case TOUCH_PSENSOR_IOCTL_GET_ENABLED:
		return put_user(lpi->ps_enable, (unsigned long __user *)arg);
		break;
	default:
		pr_err("[PS][TP][ERR]%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		return -EINVAL;
	}
}

static const struct file_operations psensor_fops = {
	.owner = THIS_MODULE,
	.open = psensor_open,
	.release = psensor_release,
	.unlocked_ioctl = psensor_ioctl
};

static struct miscdevice psensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cm3602",
	.fops = &psensor_fops
};

static int psensor_setup(struct touch_psensor_info *lpi)
{
	int ret;

	lpi->ps_input_dev = input_allocate_device();
	if (!lpi->ps_input_dev) {
		pr_err(
			"[PS][TP][ERR]%s: could not allocate ps input device\n",
			__func__);
		return -ENOMEM;
	}
	lpi->ps_input_dev->name = "proximity";
	set_bit(EV_ABS, lpi->ps_input_dev->evbit);
	input_set_abs_params(lpi->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(lpi->ps_input_dev);
	if (ret < 0) {
		pr_err(
			"[PS][TP][ERR]%s: could not register ps input device\n",
			__func__);
		goto err_free_ps_input_device;
	}

	ret = misc_register(&psensor_misc);
	if (ret < 0) {
		pr_err(
			"[PS][TP][ERR]%s: could not register ps misc device\n",
			__func__);
		goto err_unregister_ps_input_device;
	}

	return ret;

err_unregister_ps_input_device:
	input_unregister_device(lpi->ps_input_dev);
err_free_ps_input_device:
	input_free_device(lpi->ps_input_dev);
	return ret;
}

static int touch_psensor_parse_dt(struct device *dev, struct psensor_platform_data *pdata)
{
	D("[PS][TP] %s: +\n", __func__);

	return 0;
}

static int  proximity_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct touch_psensor_info *lpi;
	struct psensor_platform_data *pdata;

	D("[PS][TP] %s\n", __func__);


	lpi = kzalloc(sizeof(struct touch_psensor_info), GFP_KERNEL);
	if (!lpi)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
		{
			D("platform_data alloc memory fail");
			goto err_alloc_mem_failed;
		}
		ret = touch_psensor_parse_dt(&pdev->dev, pdata);
		if (ret < 0) {
			ret = -ENOMEM;
			goto err_alloc_pdata_mem_failed;
		}
	} else {
		pdata = pdev->dev.platform_data;
	}

	lp_info = lpi;
	ps_hal_enable = ps_drv_enable = 0;

	ret = psensor_setup(lpi);
	if (ret < 0) {
		pr_err("[PS][TP]%s: psensor_setup error!!\n",
			__func__);
		goto err_psensor_setup;
	}

	lpi->lp_wq = create_singlethread_workqueue("touch_psensor_wq");
	if (!lpi->lp_wq) {
		pr_err("[PS][TP]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}

	wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");

	lpi->touch_psensor_class = class_create(THIS_MODULE, "optical_sensors");
	if (IS_ERR(lpi->touch_psensor_class)) {
		ret = PTR_ERR(lpi->touch_psensor_class);
		lpi->touch_psensor_class = NULL;
		goto err_create_class;
	}


	lpi->ps_dev = device_create(lpi->touch_psensor_class,
				NULL, 0, "%s", "proximity");
	if (unlikely(IS_ERR(lpi->ps_dev))) {
		ret = PTR_ERR(lpi->ps_dev);
		lpi->ps_dev = NULL;
		goto err_create_ls_device_file;
	}

	ret = device_create_file(lpi->ps_dev, &dev_attr_touch_proximity);
	if (ret)
		goto err_create_ps_device;

	ret = device_create_file(lpi->ps_dev, &dev_attr_PhoneApp_status);
	if (ret)
		goto err_create_ps_device;

	D("[PS][TP] %s: Probe success!\n", __func__);
	is_probe_success = 1;
	return ret;

err_create_ps_device:
	device_unregister(lpi->ps_dev);
err_create_ls_device_file:
	device_unregister(lpi->ls_dev);
err_create_class:
	destroy_workqueue(lpi->lp_wq);
	wake_lock_destroy(&(lpi->ps_wake_lock));
	input_unregister_device(lpi->ps_input_dev);
	input_free_device(lpi->ps_input_dev);
err_create_singlethread_workqueue:
	misc_deregister(&psensor_misc);
err_alloc_pdata_mem_failed:
	if (pdev->dev.of_node)
		kfree(pdata);
err_psensor_setup:
err_alloc_mem_failed:
	kfree(lpi);
	return ret;

}

static int  proximity_sensor_remove(struct platform_device *pdev)
{
	struct touch_psensor_info *lpi = platform_get_drvdata(pdev);

	device_remove_file(lpi->ps_dev, &dev_attr_touch_proximity);
	device_remove_file(lpi->ps_dev, &dev_attr_PhoneApp_status);

	if (lpi->ps_input_dev) {
		input_unregister_device(lpi->ps_input_dev);
		input_free_device(lpi->ps_input_dev);
	}
	wake_lock_destroy(&lpi->ps_wake_lock);
	kfree(lpi);
	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id proximity_sensor_mttable[] = {
	{ .compatible = "proximity_sensor,touch"},
	{},
};
#else
#define proximity_sensor_mttable NULL
#endif

static struct platform_driver proximity_sensor_driver = {
	.probe  = proximity_sensor_probe,
	.remove = proximity_sensor_remove,
	.driver = {
		.name = "PROXIMITY_SENSOR",
		.owner = THIS_MODULE,
		.of_match_table = proximity_sensor_mttable,
	},
};

static void __init proximity_sensor_init_async(void *unused, async_cookie_t cookie)
{
	platform_driver_register(&proximity_sensor_driver);
}

static int __init proximity_sensor_init(void)
{
	async_schedule(proximity_sensor_init_async, NULL);
	return 0;
}

static void __exit proximity_sensor_exit(void)
{
	platform_driver_unregister(&proximity_sensor_driver);
}
module_init(proximity_sensor_init);
module_exit(proximity_sensor_exit);


MODULE_DESCRIPTION("Touch Proximity Driver");
MODULE_LICENSE("GPL");
