/*
 * ALSA SoC Texas Instruments TPA6130A2 headset stereo amplifier driver
 *
 * Copyright (C) Nokia Corporation
 *
 * Author: Peter Ujfalusi <peter.ujfalusi@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <sound/tpa6130a2-plat.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/of_gpio.h>
#include "mach/tpa6130a2.h"
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/of_gpio.h>

#include <mach/htc_acoustic_alsa.h>
#ifdef CONFIG_TI_TCA6418
#include <linux/i2c/tca6418_ioexpander.h>
#endif

#undef pr_info
#undef pr_err
#define pr_info(fmt, ...) pr_aud_info(fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) pr_aud_err(fmt, ##__VA_ARGS__)

static struct i2c_client *tpa6130a2_client;
struct delayed_work powerup_work;
static struct workqueue_struct *poweron_wq;
struct work_struct ramp_work;
static struct workqueue_struct *ramp_wq;
static atomic_t modeID = ATOMIC_INIT(-1);


#define TPA6130A2_SD_GPIO_NUMBER 2 

struct tpa6130a2_data {
	unsigned char initRegs[TPA6130A2_CACHEREGNUM];
	struct regulator *supply;
	int power_gpio;
	unsigned char power_state;
};

struct amp_config_data tpa6130_config_data;

struct tpa6130a2_reg_data {
	unsigned char addr;
	unsigned char val;
};

static unsigned char tpa6130_curmode_value = 0x3f;
static int tpa6130a2_opened;
static struct mutex hp_amp_lock;

static int tpa6130a2_i2c_write(u8 reg, u8 value)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
	struct tpa6130a2_data *chipdata = NULL;
	chipdata = i2c_get_clientdata(tpa6130a2_client);

	msg->addr = tpa6130a2_client->addr;
	msg->flags = 0; 
	msg->len = 2; 
	msg->buf = data;
	data[0] = reg;
	data[1] = value;

	pr_info("%s: write reg 0x%x val 0x%x\n",__func__,data[0],data[1]);

	if (chipdata->power_state == 0) { 
		pr_info("%s: write reg need power on first. Exit!\n",__func__);
		return 0;
	}
	err = i2c_transfer(tpa6130a2_client->adapter, msg, 1);

	if (err >= 0)
		return 0;
        else {

            pr_info("%s: write error error %d\n",__func__,err);
            return err;
        }

}

static int tpa6130a2_read(unsigned char *rxData, unsigned char addr)
{
	int rc;
	struct i2c_msg msgs[] = {
		{
		 .addr = tpa6130a2_client->addr,
		 .flags = 0, 
		 .len = 1,
		 .buf = rxData,
		},
		{
		 .addr = tpa6130a2_client->addr,
		 .flags = I2C_M_RD, 
		 .len = 1,
		 .buf = rxData,
		},
	};

	if(!rxData)
		return -1;

	*rxData = addr;

	rc = i2c_transfer(tpa6130a2_client->adapter, msgs, 2);
	if (rc < 0) {
		pr_err("%s:[1] transfer error %d\n", __func__, rc);
		return rc;
	}

	pr_info("%s:i2c_read addr 0x%x value = 0x%x\n", __func__, addr, *rxData);
	return 0;
}


static int tpa6130a2_initialize(void)
{
	int ret = 0;

	pr_info("%s :\n", __func__);

       
	ret = tpa6130a2_i2c_write(TPA6130A2_REG_CONTROL, 0);
	ret = tpa6130a2_i2c_write(TPA6130A2_REG_VOL_MUTE, TPA6130A2_MUTE_R \
		                                    |TPA6130A2_MUTE_L | tpa6130_curmode_value);

	return ret;
}

static void tpa6130a2_channel_enable(int channel, int enable)
{
	unsigned char val;
	int ret=0;
	pr_info("%s : enable=%d\n", __func__, enable);

	if (enable) {
		
		
		ret = tpa6130a2_read(&val,TPA6130A2_REG_CONTROL);
		val |= channel;
		val &= ~TPA6130A2_SWS; 
		tpa6130a2_i2c_write(TPA6130A2_REG_CONTROL, val);

		
		ret = tpa6130a2_read(&val,TPA6130A2_REG_VOL_MUTE);
		val &= ~channel;
		tpa6130a2_i2c_write(TPA6130A2_REG_VOL_MUTE, val);
	} else {
		
		
		ret = tpa6130a2_read(&val,TPA6130A2_REG_VOL_MUTE);
		val |= channel;
		tpa6130a2_i2c_write(TPA6130A2_REG_VOL_MUTE, val);

		
		ret = tpa6130a2_read(&val,TPA6130A2_REG_CONTROL);
		val &= ~channel;
		tpa6130a2_i2c_write(TPA6130A2_REG_CONTROL, val);
	}
}

static void tpa6130a2_poweron(struct work_struct *work)
{
	struct tpa6130a2_data *data = NULL;
	int ret = 0;

	pr_info("%s ++:\n", __func__);
	data = i2c_get_clientdata(tpa6130a2_client);

	if (data->power_state == 0) { 
		pr_info("%s : state=%d, start to poweron\n", __func__,data->power_state);

		
		ret = regulator_enable(data->supply);
		if (ret != 0) {
			pr_err("%s : Failed to enable supply: %d\n", __func__, ret);
			data->power_state = 0;
			return;
		}
		msleep(10);

		
		ioexp_gpio_set_value(TPA6130A2_SD_GPIO_NUMBER, 1);
		msleep(10);

		data->power_state = 1;
		
		tpa6130a2_initialize();
		tpa6130a2_channel_enable(TPA6130A2_HP_EN_R | TPA6130A2_HP_EN_L,1);
	} else {
		pr_info("%s : on state. no need to poweron again", __func__);
	}
	pr_info("%s --:\n", __func__);
}

static void tpa6130a2_volume_ramp(struct work_struct *work)
{
	int ret = 0, i = 0, j = 0;
	int val_new = 0, val_current = 0, modeid = atomic_read(&modeID);
	const int sleepMs = 1;
	const int step = 2;
	struct tpa6130a2_data *chipdata = NULL;
	chipdata = i2c_get_clientdata(tpa6130a2_client);

	for (i = 0; i < tpa6130_config_data.cmd_data[modeid].config.reg_len; i++) {
		val_new = tpa6130_config_data.cmd_data[modeid].config.reg[i].val & 0x3f;
		val_current = tpa6130_curmode_value;
		tpa6130_curmode_value = tpa6130_config_data.cmd_data[modeid].config.reg[i].val & 0x3f;

		if (chipdata->power_state != 0) {
			if (val_new != val_current) {
				pr_info("%s: tap6130 vol ramping: %#x -> %#x, modeid: %d, modeID: %d\n",
						__func__, val_current, val_new, modeid, atomic_read(&modeID));
				if (val_new > val_current) {
					for (j = val_current+step; j <= val_new; j+=step) {
						ret = tpa6130a2_i2c_write(tpa6130_config_data.cmd_data[modeid].config.reg[i].addr, j);
						if (ret < 0) {
							pr_err("%s: tpa6130 set mode to write failed.\n", __func__);
							return;
						}
						if (j != val_new)
							msleep(sleepMs);

						if ((j != val_new) && ((j+step) > val_new)) {
							ret = tpa6130a2_i2c_write(tpa6130_config_data.cmd_data[modeid].config.reg[i].addr, val_new);
							if (ret < 0) {
								pr_err("%s: tpa6130 set mode to write failed, ret: %d\n", __func__, ret);
								return;
							}
						}
					}
				} else {
					for (j = val_current-step; j >= val_new; j-=step) {
						ret = tpa6130a2_i2c_write(tpa6130_config_data.cmd_data[modeid].config.reg[i].addr, j);
						if (ret < 0) {
							pr_err("%s: tpa6130 set mode to write failed.\n", __func__);
							return;
						}
						if (j != val_new)
							msleep(sleepMs);

						if ((j != val_new) && ((j-step) < val_new)) {
							ret = tpa6130a2_i2c_write(tpa6130_config_data.cmd_data[modeid].config.reg[i].addr, val_new);
							if (ret < 0) {
								pr_err("%s: tpa6130 set mode to write failed, ret: %d\n", __func__, ret);
								return;
							}
						}
					}
				}
			}
		} else {
			pr_info("%s: tap6130 set mode %d with value %#x, but power is off!\n", __func__,
					modeid, val_new);
		}
	}
}

void tpa6130a2_HSMute(int mute)
{
	struct tpa6130a2_data *data = NULL;
	data = i2c_get_clientdata(tpa6130a2_client);

	if (data == NULL) {
		pr_info("%s : tpa6130a2_client does not exist", __func__);
		return;
	}

	pr_info("%s : mute HS AMP %d (pw = %d)", __func__, mute, data->power_state);
	if (data->power_state == 1 && (mute == 0 || mute == 1)) {
		tpa6130a2_channel_enable(TPA6130A2_HP_EN_R | TPA6130A2_HP_EN_L, !mute);
	}
}


static int tpa6130a2_power(int power)
{
	struct	tpa6130a2_data *data = NULL;
	int	ret = 0;

	pr_info("%s : %d", __func__, power);

	data = i2c_get_clientdata(tpa6130a2_client);

	if (power == data->power_state)
		goto exit;

	if (power) {
		
		ret = regulator_enable(data->supply);
		if (ret != 0) {
			pr_err("%s : Failed to enable supply: %d\n", __func__, ret);
			goto exit;
		}
		msleep(10);

		
		ioexp_gpio_set_value(TPA6130A2_SD_GPIO_NUMBER, 1);
		msleep(10);

		data->power_state = 1;
		
		ret = tpa6130a2_initialize();

		if (ret < 0) {
			pr_err("%s : Failed to initialize chip\n", __func__);
			regulator_disable(data->supply);
			data->power_state = 0;
			goto exit;
		}
	} else {
		
		ioexp_gpio_set_value(TPA6130A2_SD_GPIO_NUMBER, 0);
		msleep(10);

		
		ret = regulator_disable(data->supply);
		if (ret != 0) {
			pr_err("%s: Failed to disable supply: %d\n", __func__, ret);
			goto exit;
		}
		data->power_state = 0;
	}

exit:
	return ret;
}

int tpa6130a2_stereo_enable(int enable)
{
	int ret = 0;
	struct tpa6130a2_data *data = NULL;
	data = i2c_get_clientdata(tpa6130a2_client);

	pr_info("%s ++: enable=%d, power_state=%d\n", __func__, enable, data->power_state);
	mutex_lock(&hp_amp_lock);

	if (enable) {
		flush_delayed_work(&powerup_work);

		if (data->power_state == 0) {
#if 0
			ret = tpa6130a2_power(1);
			tpa6130a2_channel_enable(TPA6130A2_HP_EN_R | TPA6130A2_HP_EN_L, 1);
#else
			queue_delayed_work(poweron_wq, &powerup_work, msecs_to_jiffies(50));
#endif
		} else {
			pr_info("already enabled! no need to enable again");
		}
	} else {
		cancel_delayed_work_sync(&powerup_work);

		if (data->power_state == 1) {
			tpa6130a2_channel_enable(TPA6130A2_HP_EN_R | TPA6130A2_HP_EN_L, 0);
			ret = tpa6130a2_power(0);
		} else {
			pr_info("already disabled! no need to disable again");
		}
	}
	mutex_unlock(&hp_amp_lock);

	pr_info("%s --: %d\n", __func__, enable);
	return ret;
}


static int set_tpa6130a2_amp(int on, int dsp)
{
	pr_info("%s: %d", __func__, on);

	if(on) {
		tpa6130a2_stereo_enable(1);
	}
	else {
		tpa6130a2_stereo_enable(0);
	}

	return 0;
}
static int tpa6130a2_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	mutex_lock(&hp_amp_lock);

	if (tpa6130a2_opened) {
		pr_err("%s: busy\n", __func__);
		rc = -EBUSY;
		goto done;
	}
	tpa6130a2_opened = 1;
done:
	mutex_unlock(&hp_amp_lock);
	return rc;
}

static int tpa6130a2_release(struct inode *inode, struct file *file)
{
	mutex_lock(&hp_amp_lock);
	tpa6130a2_opened = 0;
	mutex_unlock(&hp_amp_lock);

	return 0;
}

int dump_amp_acoustic_table(struct amp_config_data *amp_data)
{
    int i, j;

    pr_info("=========== acoustic table dump =============");
    for (i=0; i < amp_data->mode_num; i++)
    {
        pr_info("== mode: %d", i);
        pr_info("   out_mode %d ", amp_data->cmd_data[i].out_mode);
        for(j=0;j< amp_data->cmd_data[i].config.reg_len; j++)
            pr_info("    reg: %x val: %x", amp_data->cmd_data[i].config.reg[j].addr,
                amp_data->cmd_data[i].config.reg[j].val);
    }

    return 0;
}

static long tpa6130a2_ioctl(struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int rc = 0, modeid = 0;
	pr_info("%s: tpa6130a2_ioctl: cmd:%d", __func__, cmd);
	switch (cmd) {
	case TPA6130_SET_MODE:
		if (copy_from_user(&modeid, argp, sizeof(modeid))) {
			pr_err("%s: copy from user failed.\n", __func__);
			return -EFAULT;
		}
		pr_info("%s: update tpa6130 TPA6130_SET_MODE commands mode:%d start.\n",
				__func__, modeid);
		if (modeid >= tpa6130_config_data.mode_num || modeid < 0) {
			pr_err("%s: unsupported tpa6130 mode %d\n", __func__, modeid);
			return -EINVAL;
		} else {
			atomic_set(&modeID, modeid);
			cancel_work_sync(&ramp_work);
			queue_work(ramp_wq, &ramp_work);
		}
		pr_info("%s: update tpa6130 TPA6130_SET_MODE commands mode:%d success.\n",
				__func__, modeid);
		break;

	case TPA6130_SET_MUTE:
		pr_info("%s: update tpa6130 TPA6130_SET_MUTE commands mode:%d start.\n", __func__, modeid);
		break;

	case TPA6130_SET_PARAM:
		if (copy_from_user(&tpa6130_config_data.mode_num, argp, sizeof(unsigned int))) {
			pr_err("%s: copy from user failed.\n", __func__);
			return -EFAULT;
		}

		if (tpa6130_config_data.mode_num <= 0) {
			pr_err("%s: invalid mode number %d\n",
					__func__, tpa6130_config_data.mode_num);
			return -EINVAL;
		}
		if (tpa6130_config_data.cmd_data == NULL)
			tpa6130_config_data.cmd_data = kzalloc(sizeof(struct amp_comm_data)*tpa6130_config_data.mode_num, GFP_KERNEL);

		if (!tpa6130_config_data.cmd_data) {
			pr_err("%s: out of memory\n", __func__);
			return -ENOMEM;
		}

		if (copy_from_user(tpa6130_config_data.cmd_data,((struct amp_config_data*)argp)->cmd_data \
                        ,sizeof(struct amp_comm_data)*tpa6130_config_data.mode_num)) {
                    pr_err("%s: copy data from user failed.\n", __func__);
                    kfree(tpa6130_config_data.cmd_data);
                    tpa6130_config_data.cmd_data = NULL;
                    return -EFAULT;
		}

		pr_info("%s: update tpa6130 i2c commands #%d success.\n",
				__func__, tpa6130_config_data.mode_num);
		
		
                
		
		
		
		
                
		rc = 0;
		break;

	default:
		pr_err("%s: Invalid command\n", __func__);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static struct file_operations tpa6130a2_fops = {
	.owner = THIS_MODULE,
	.open = tpa6130a2_open,
	.release = tpa6130a2_release,
	.unlocked_ioctl = tpa6130a2_ioctl,
};

static struct miscdevice tpa6130a2_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tpa6130a2",
	.fops = &tpa6130a2_fops,
};


static int tpa6130a2_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct device *dev;
	struct tpa6130a2_data *data = NULL;
	const char *regulator;
	int ret;


	dev = &client->dev;

	pr_info("tpa6130a2_probe ++");

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s : Can not allocate memory\n", __func__);
		return -ENOMEM;
	}

	tpa6130a2_client = client;

	i2c_set_clientdata(tpa6130a2_client, data);

	ret = misc_register(&tpa6130a2_device);
	htc_acoustic_register_hs_amp(set_tpa6130a2_amp, &tpa6130a2_fops);

	mutex_init(&hp_amp_lock);

	data->power_state = 0;
	regulator = "Vdd";
	data->supply = regulator_get(dev, regulator);

	if (IS_ERR(data->supply)) {
		pr_err("%s : Failed to request supply: %d\n", __func__, ret);
		if (data)
			kfree(data);
		return -ENOMEM;
	}

	poweron_wq = create_workqueue("tpa_power_on");
	INIT_DELAYED_WORK(&powerup_work, tpa6130a2_poweron);
	ramp_wq = create_workqueue("tpa6130a2_volume_ramp");
	INIT_WORK(&ramp_work, tpa6130a2_volume_ramp);
	pr_info("tpa6130a2_probe --");

	return 0;
}

static int tpa6130a2_remove(struct i2c_client *client)
{
	struct tpa6130a2_data *data = NULL;
	data = i2c_get_clientdata(tpa6130a2_client);
	if (data)
		kfree(data);

	tpa6130a2_power(0);
	tpa6130a2_client = NULL;

	return 0;
}
static int tpa6130a2_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int tpa6130a2_resume(struct i2c_client *client)
{
	return 0;
}

static struct of_device_id tpa6130a2_match_table[] = {
        { .compatible = "tpa6130a2",},
        { },
};

static const struct i2c_device_id tpa6130a2_id[] = {
	{ TPA6130A2_I2C_NAME, 0 },
	{ }
};


static struct i2c_driver tpa6130a2_i2c_driver = {
	.probe = tpa6130a2_probe,
	.remove = tpa6130a2_remove,
	.suspend = tpa6130a2_suspend,
	.resume = tpa6130a2_resume,
	.id_table = tpa6130a2_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = TPA6130A2_I2C_NAME,
		.of_match_table = tpa6130a2_match_table,
	},
};

static int __init tpa6130a2_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&tpa6130a2_i2c_driver);
}

static void __exit tpa6130a2_exit(void)
{
	i2c_del_driver(&tpa6130a2_i2c_driver);
}

module_init(tpa6130a2_init);
module_exit(tpa6130a2_exit);

MODULE_DESCRIPTION("TPA6130A2 Headphone amplifier driver");
MODULE_LICENSE("GPL");
