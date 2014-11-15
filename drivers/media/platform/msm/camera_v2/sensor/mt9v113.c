/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include "mt9v113.h"
#define mt9v113_SENSOR_NAME "mt9v113"
#define PLATFORM_DRIVER_NAME "msm_camera_mt9v113"
#define mt9v113_obj mt9v113_##obj


#define CONFIG_MSMB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

DEFINE_MSM_MUTEX(mt9v113_mut);

static struct msm_sensor_ctrl_t mt9v113_s_ctrl;


static struct msm_sensor_power_setting mt9v113_power_setting[] = {
    {
        .seq_type = SENSOR_VREG,
        .seq_val = CAM_VDIG,
        .config_val = 1,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_VREG,
        .seq_val = CAM_VIO,
        .config_val = 1,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_GPIO,
        .seq_val = SENSOR_GPIO_VANA,
        .config_val = 1,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_GPIO,
        .seq_val = SENSOR_GPIO_RESET,
        .config_val = GPIO_OUT_LOW,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_GPIO,
        .seq_val = SENSOR_GPIO_RESET,
        .config_val = GPIO_OUT_HIGH,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_CLK,
        .seq_val = SENSOR_CAM_MCLK,
        .config_val = 23880000,
        .delay = 5,
    },
    {
        .seq_type = SENSOR_I2C_MUX,
        .seq_val = 0,
        .config_val = 0,
        .delay = 0,
    },

};

static struct v4l2_subdev_info mt9v113_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id mt9v113_i2c_id[] = {
	{mt9v113_SENSOR_NAME, (kernel_ulong_t)&mt9v113_s_ctrl},
	{ }
};

static int32_t msm_mt9v113_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &mt9v113_s_ctrl);
}

static struct i2c_driver mt9v113_i2c_driver = {
	.id_table = mt9v113_i2c_id,
	.probe  = msm_mt9v113_i2c_probe,
	.driver = {
		.name = mt9v113_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9v113_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id mt9v113_dt_match[] = {
	{
		.compatible = "ovti,mt9v113",
		.data = &mt9v113_s_ctrl
	},
	{}
};

MODULE_DEVICE_TABLE(of, mt9v113_dt_match);

static int32_t mt9v113_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(mt9v113_dt_match, &pdev->dev);
	
	if (!match) {
		pr_err("%s:%d match is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}


static struct platform_driver mt9v113_platform_driver = {
	.driver = {
		.name = "ovti,mt9v113",
		.owner = THIS_MODULE,
		.of_match_table = mt9v113_dt_match,
	},
	.probe = mt9v113_platform_probe,
};

static const char *mt9v113Vendor = "Micron";
static const char *mt9v113NAME = "mt9v113";
static const char *mt9v113Size = "VGA CMOS";

static ssize_t sensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", mt9v113Vendor, mt9v113NAME, mt9v113Size);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);

static struct kobject *android_mt9v113;

static int mt9v113_sysfs_init(void)
{
	int ret ;
	pr_info("mt9v113:kobject creat and add\n");
	android_mt9v113 = kobject_create_and_add("android_camera2", NULL);
	if (android_mt9v113 == NULL) {
		pr_info("mt9v113_sysfs_init: subsystem_register " \
		"failed\n");
		ret = -ENOMEM;
		return ret ;
	}
	pr_info("mt9v113:sysfs_create_file\n");
	ret = sysfs_create_file(android_mt9v113, &dev_attr_sensor.attr);
	if (ret) {
		pr_info("mt9v113_sysfs_init: sysfs_create_file " \
		"failed\n");
		kobject_del(android_mt9v113);
	}

	return 0 ;
}


static int __init mt9v113_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
    rc = platform_driver_register(&mt9v113_platform_driver);
	if (!rc) {
		mt9v113_sysfs_init();
		return rc;
	}
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&mt9v113_i2c_driver);
}

static void __exit mt9v113_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (mt9v113_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&mt9v113_s_ctrl);
		platform_driver_unregister(&mt9v113_platform_driver);
	} else
		i2c_del_driver(&mt9v113_i2c_driver);
	return;
}
static int suspend_fail_retry_count_2;
#define SUSPEND_FAIL_RETRY_MAX_2 0
int g_csi_if = 1;
static int op_mode = 0;
static int just_power_on = 1;

#define CHECK_STATE_TIME 100

static struct i2c_client mt9v113_client_t; 
static struct i2c_client *mt9v113_client = &mt9v113_client_t;

static int mt9v113_i2c_write(unsigned short saddr,
				 unsigned short waddr, unsigned short wdata,
				 enum mt9v113_width width)
{
	int rc = -EIO;

	switch (width) {
	case WORD_LEN:{
			


            rc =mt9v113_sensor_i2c_client.i2c_func_tbl->i2c_write(&mt9v113_sensor_i2c_client,waddr, wdata ,MSM_CAMERA_I2C_WORD_DATA);
		}
		break;

	case BYTE_LEN:{
			

            rc =mt9v113_sensor_i2c_client.i2c_func_tbl->i2c_write(&mt9v113_sensor_i2c_client,waddr, wdata ,MSM_CAMERA_I2C_BYTE_DATA);
		}
		break;

	default:
		break;
	}

	if (rc < 0)
		pr_info("i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		     waddr, wdata);

	return rc;
}

static int mt9v113_i2c_write_table(struct mt9v113_i2c_reg_conf
				       *reg_conf_tbl, int num_of_items_in_table)
{
	int i;
	int rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9v113_i2c_write(mt9v113_client->addr,
				       reg_conf_tbl->waddr, reg_conf_tbl->wdata,
				       reg_conf_tbl->width);
		if (rc < 0) {
		pr_err("%s: num_of_items_in_table=%d\n", __func__,
			num_of_items_in_table);
			break;
		}
		if (reg_conf_tbl->mdelay_time != 0)
			mdelay(reg_conf_tbl->mdelay_time);
		reg_conf_tbl++;
	}

	return rc;
}

static int32_t mt9v113_i2c_read_w(unsigned short saddr, unsigned short raddr,
	unsigned short *rdata)
{
	int32_t rc = 0;

	if (!rdata)
		return -EIO;


	rc =mt9v113_sensor_i2c_client.i2c_func_tbl->i2c_read(&mt9v113_sensor_i2c_client,raddr, rdata ,MSM_CAMERA_I2C_WORD_DATA);

	if (rc < 0)
		pr_err("mt9v113_i2c_read_w failed!\n");

	return rc;
}


static int mt9v113_i2c_write_bit(unsigned short saddr, unsigned short raddr,
unsigned short bit, unsigned short state)
{
	int rc;
	unsigned short check_value;
	unsigned short check_bit;

	if (state)
		check_bit = 0x0001 << bit;
	else
		check_bit = 0xFFFF & (~(0x0001 << bit));
	pr_debug("mt9v113_i2c_write_bit check_bit:0x%4x", check_bit);
	rc = mt9v113_i2c_read_w(saddr, raddr, &check_value);
	if (rc < 0)
	  return rc;

	pr_debug("%s: mt9v113: 0x%4x reg value = 0x%4x\n", __func__,
		raddr, check_value);
	if (state)
		check_value = (check_value | check_bit);
	else
		check_value = (check_value & check_bit);

	pr_debug("%s: mt9v113: Set to 0x%4x reg value = 0x%4x\n", __func__,
		raddr, check_value);

	rc = mt9v113_i2c_write(saddr, raddr, check_value,
		WORD_LEN);
	return rc;
}

static int mt9v113_i2c_check_bit(unsigned short saddr, unsigned short raddr,
unsigned short bit, int check_state)
{
	int k;
	unsigned short check_value;
	unsigned short check_bit;
	check_bit = 0x0001 << bit;
	for (k = 0; k < CHECK_STATE_TIME; k++) {
		mt9v113_i2c_read_w(mt9v113_client->addr,
			      raddr, &check_value);
		if (check_state) {
			if ((check_value & check_bit))
			break;
		} else {
			if (!(check_value & check_bit))
			break;
		}
		msleep(1);
	}
	if (k == CHECK_STATE_TIME) {
		pr_err("%s failed addr:0x%2x data check_bit:0x%2x",
			__func__, raddr, check_bit);
		return -1;
	}
	return 1;
}

static inline int resume(void)
{
	int k = 0, rc = 0;
	unsigned short check_value;

	
	
	rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0016, &check_value);
	if (rc < 0)
	  return rc;

	pr_info("%s: mt9v113: 0x0016 reg value = 0x%x\n", __func__,
		check_value);

	check_value = (check_value|0x0020);

	pr_info("%s: mt9v113: Set to 0x0016 reg value = 0x%x\n", __func__,
		check_value);

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0016, check_value,
		WORD_LEN);
	if (rc < 0) {
		pr_err("%s: Enter Active mode fail\n", __func__);
		return rc;
	}

	
	pr_info("resume, check_value=0x%x", check_value);
	rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0018, &check_value);
	if (rc < 0)
	  return rc;

	pr_info("%s: mt9v113: 0x0018 reg value = 0x%x\n", __func__,
		check_value);

	check_value = (check_value & 0xFFFE);

	pr_info("%s: mt9v113: Set to 0x0018 reg value = 0x%x\n", __func__,
		check_value);

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0018, check_value,
		WORD_LEN);
	if (rc < 0) {
		pr_err("%s: Enter Active mode fail\n", __func__);
		return rc;
	}

	
	for (k = 0; k < CHECK_STATE_TIME; k++) {
		mt9v113_i2c_read_w(mt9v113_client->addr,
			  0x0018, &check_value);

		pr_info("%s: mt9v113: 0x0018 reg value = 0x%x\n", __func__,
			check_value);

		if (!(check_value & 0x4000)) {
			pr_info("%s: (check 0x0018[14] is 0) k=%d\n",
				__func__, k);
			break;
		}
		msleep(1);	
	}
	if (k == CHECK_STATE_TIME) {
		pr_err("%s: check status time out (check 0x0018[14] is 0)\n",
			__func__);
		return -EIO;
	}

	
	for (k = 0; k < CHECK_STATE_TIME; k++) {
		mt9v113_i2c_read_w(mt9v113_client->addr,
			  0x301A, &check_value);
		if (check_value & 0x0004) {
			pr_info("%s: (check 0x301A[2] is 1) k=%d\n",
				__func__, k);
			break;
		}
		msleep(1);	
	}
	if (k == CHECK_STATE_TIME) {
		pr_err("%s: check status time out (check 0x301A[2] is 1)\n",
			__func__);
		return -EIO;
	}

	
	for (k = 0; k < CHECK_STATE_TIME; k++) {
		rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x31E0,
			&check_value);
		if (check_value == 0x0003) { 
			pr_info("%s: (check 0x31E0 is 0x003 ) k=%d\n",
				__func__, k);
			break;
		}
		msleep(1);	
	}
	if (k == CHECK_STATE_TIME) {
		pr_err("%s: check status time out (check 0x31E0 is 0x003 )\n",
			__func__);
		return -EIO;
	}

	
	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x31E0, 0x0001,
	WORD_LEN);
	if (rc < 0) {
		pr_err("%s: Enter Active mode fail\n", __func__);
		return rc;
	}

    msleep(2);

	return rc;
}

static inline int suspend(void)
{
	int k = 0, rc = 0;
	unsigned short check_value;

	
	
	rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0018, &check_value);
	if (rc < 0)
	  return rc;

	check_value = (check_value|0x0008);

	pr_info("suspend, check_value=0x%x", check_value);

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0018, check_value,
		WORD_LEN);
	if (rc < 0) {
		pr_err("%s: Enter standy mode fail\n", __func__);
		return rc;
	}
	
	rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0018, &check_value);
	if (rc < 0)
	  return rc;

	check_value = (check_value|0x0001);

	pr_info("%s: mt9v113: Set to 0x0018 reg value = 0x%x\n", __func__,
		check_value);

	pr_info("suspend, 2,check_value=0x%x", check_value);

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0018, check_value,
		WORD_LEN);
	if (rc < 0) {
		pr_err("%s: Enter standy mode fail\n", __func__);
		return rc;
	}

	
	for (k = 0; k < CHECK_STATE_TIME; k++) {
		mt9v113_i2c_read_w(mt9v113_client->addr,
			  0x0018, &check_value);
		if ((check_value & 0x4000)) { 
			pr_info("%s: ( check 0x0018[14] is 1 ) k=%d\n",
				__func__, k);
			break;
		}
		msleep(1);	
	}
	if (k == CHECK_STATE_TIME) {
		pr_err("%s: check status time out\n", __func__);
		return -EIO;
	}
    msleep(2);
	return rc;
}

static int mt9v113_reg_init(void)
{
	int rc = 0, k = 0;
	unsigned short check_value;

    
	pr_info("%s: Power Up Start\n", __func__);

	rc = mt9v113_i2c_write(mt9v113_client->addr,
					0x0018, 0x4028, WORD_LEN);
	if (rc < 0)
		goto reg_init_fail;

	rc = mt9v113_i2c_check_bit(mt9v113_client->addr, 0x0018, 14, 0);
	if (rc < 0)
		goto reg_init_fail;

	
	rc = mt9v113_i2c_check_bit(mt9v113_client->addr, 0x301A, 2, 1);
	if (rc < 0)
		goto reg_init_fail;

	rc = mt9v113_i2c_write_table(&mt9v113_regs.power_up_tbl[0],
				     mt9v113_regs.power_up_tbl_size);
	if (rc < 0) {
		pr_err("%s: Power Up fail\n", __func__);
		goto reg_init_fail;
	}
	
    if(suspend_fail_retry_count_2 != SUSPEND_FAIL_RETRY_MAX_2) {
        pr_info("%s: added additional delay count=%d\n", __func__, suspend_fail_retry_count_2);
        mdelay(20);
    }
    
	
	pr_info("%s: RESET and MISC Control\n", __func__);

	rc = mt9v113_i2c_write(mt9v113_client->addr,
					0x0018, 0x4028, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_check_bit(mt9v113_client->addr, 0x0018, 14, 0);
	if (rc < 0)
		goto reg_init_fail;

	
	rc = mt9v113_i2c_check_bit(mt9v113_client->addr, 0x301A, 2, 1);
	if (rc < 0)
		goto reg_init_fail;

	
	rc = mt9v113_i2c_write_bit(mt9v113_client->addr, 0x31E0, 1, 0);
	if (rc < 0)
		goto reg_init_fail;

	if (g_csi_if) {
	    
	    rc = mt9v113_i2c_write_bit(mt9v113_client->addr, 0x001A, 9, 0);
	    if (rc < 0)
	      goto reg_init_fail;

	    
	    
	    
	    

	    rc = mt9v113_i2c_write_bit(mt9v113_client->addr, 0x3400, 4, 1);
	    if (rc < 0)
	      goto reg_init_fail;

		
		for (k = 0; k < CHECK_STATE_TIME; k++) {
			rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x3400,
				&check_value);
			pr_info("%s: mt9v113: 0x3400 reg value = 0x%4x\n", __func__, check_value);
			if (check_value & 0x0010) { 
				pr_info("%s: (check 0x3400[4] is 1 ) k=%d\n",
				__func__, k);
			break;
		} else {
			check_value = (check_value | 0x0010);
			pr_info("%s: mt9v113: Set to 0x3400 reg value = 0x%4x\n", __func__, check_value);
				rc = mt9v113_i2c_write(mt9v113_client->addr, 0x3400,
				check_value, WORD_LEN);
			if (rc < 0)
				goto reg_init_fail;
		}
			msleep(1);	
		}
		if (k == CHECK_STATE_TIME) {
			pr_err("%s: check status time out (check 0x3400[4] is 1 )\n",
				__func__);
			goto reg_init_fail;
		}

		mdelay(10);
	    
	    rc = mt9v113_i2c_write_bit(mt9v113_client->addr, 0x3400, 9, 1);
	    if (rc < 0)
	      goto reg_init_fail;

		
		for (k = 0; k < CHECK_STATE_TIME; k++) {
			rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x3400,
				&check_value);
			pr_info("%s: mt9v113: 0x3400 reg value = 0x%4x\n", __func__, check_value);
			if (check_value & 0x0200) { 
				pr_info("%s: (check 0x3400[9] is 1 ) k=%d\n",
					__func__, k);
				break;
			} else {
				check_value = (check_value | 0x0200);
				pr_info("%s: mt9v113: Set to 0x3400 reg value = 0x%4x\n", __func__, check_value);
				rc = mt9v113_i2c_write(mt9v113_client->addr, 0x3400,
					check_value, WORD_LEN);
				if (rc < 0)
					goto reg_init_fail;
			}
			msleep(1);	
		}
		if (k == CHECK_STATE_TIME) {
			pr_err("%s: check status time out (check 0x3400[9] is 1 )\n",
				__func__);
			goto reg_init_fail;
		}

	    
	    rc = mt9v113_i2c_write_bit(mt9v113_client->addr, 0x321C, 7, 0);
	    if (rc < 0)
	      goto reg_init_fail;
	} else {
	    rc = mt9v113_i2c_write(mt9v113_client->addr, 0x001A, 0x0210, WORD_LEN);
	    if (rc < 0)
	      goto reg_init_fail;
	}

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x001E, 0x0777, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;


	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0016, 0x42DF, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;


	
	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0014, 0xB04B, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0014, 0xB049, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0010, 0x021C, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0012, 0x0000, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0014, 0x244B, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	msleep(30);

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0014, 0x304B, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_check_bit(mt9v113_client->addr, 0x0014, 15, 1);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0014, 0xB04A, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	
	rc = mt9v113_i2c_write_table(&mt9v113_regs.register_init_1[0],
			mt9v113_regs.register_init_size_1);
	if (rc < 0)
	  goto reg_init_fail;

	
	rc = mt9v113_i2c_write_bit(mt9v113_client->addr, 0x3210, 3, 1);
	if (rc < 0)
	  goto reg_init_fail;

	
	rc = mt9v113_i2c_write_table(&mt9v113_regs.register_init_2[0],
			mt9v113_regs.register_init_size_2);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0006, WORD_LEN);
	for (k = 0; k < CHECK_STATE_TIME; k++) {  
		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103,
			WORD_LEN);
		rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0990,
			&check_value);
		if (check_value == 0x0000) 
			break;
		msleep(1);
	}
	if (k == CHECK_STATE_TIME)
		goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0005, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	for (k = 0; k < CHECK_STATE_TIME; k++) {  
		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103,
			WORD_LEN);
		rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0990,
			&check_value);
		if (check_value == 0x0000) 
			break;
		msleep(1);
	}
	if (k == CHECK_STATE_TIME)
		goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA102, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x000F, WORD_LEN);
	if (rc < 0)
	  goto reg_init_fail;

	return rc;
reg_init_fail:
	pr_err("mt9v113 register initial fail\n");
	return rc;
}






static int mt9v113_set_sensor_mode(int mode)
{
	int rc = 0 , k;
	uint16_t check_value = 0;
	pr_info("%s: E\n", __func__);
	pr_info("sinfo->csi_if = %d, mode = %d", g_csi_if, mode);

		if (g_csi_if) {

			rc = resume();

			if (rc < 0)
				pr_err("%s: resume fail\n", __func__);
		}


	switch (mode) {
	case 0: 
		op_mode = 0; 
		pr_info("mt9v113:sensor set mode: preview\n");

		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103,
			WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0002,
		WORD_LEN);
		if (rc < 0)
			return rc;

		for (k = 0; k < CHECK_STATE_TIME; k++) {  
			rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C,
				0xA104,	WORD_LEN);
			rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0990,
				&check_value);
			pr_info("check_value=%d", check_value);
			if (check_value == 0x0003) 
				break;
			msleep(1);
		}
		if (k == CHECK_STATE_TIME) {
			pr_err("%s: Preview fail\n", __func__);
			return -EIO;
		}

		
		msleep(150);

		break;
	case 1: 
		op_mode = 1; 
		
		pr_info("mt9v113:sensor set mode: snapshot\n");

		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C, 0xA103,
			WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0990, 0x0001,
		WORD_LEN);
		if (rc < 0)
			return rc;

		for (k = 0; k < CHECK_STATE_TIME; k++) {
			rc = mt9v113_i2c_write(mt9v113_client->addr, 0x098C,
				0xA104, WORD_LEN);
			rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0990,
				&check_value);
			if (check_value == 0x0003)
				break;
			msleep(1);
		}
		if (k == CHECK_STATE_TIME) {
			pr_err("%s: Snapshot fail\n", __func__);
			return -EIO;
		}
		break;

	default:
		return -EINVAL;
	}

	pr_info("%s: X\n", __func__);
	return rc;
}

int mt9v113_sensor_open_init(void)
{
	int rc = 0;
	uint16_t check_value = 0;

	pr_info("%s\n", __func__);


	suspend_fail_retry_count_2 = SUSPEND_FAIL_RETRY_MAX_2;


probe_suspend_fail_retry_2:
		pr_info("%s suspend_fail_retry_count_2=%d\n", __func__, suspend_fail_retry_count_2);

		mdelay(5);

		
		rc = mt9v113_reg_init();
		if (rc < 0) {
			pr_err("%s: mt9v113_reg_init fail\n", __func__);

			if (suspend_fail_retry_count_2 > 0) {
				suspend_fail_retry_count_2--;
				pr_info("%s: mt9v113 reg_init fail start retry mechanism !!!\n", __func__);
				goto probe_suspend_fail_retry_2;
			}

			goto init_fail;
		}

		
		
		rc = mt9v113_i2c_read_w(mt9v113_client->addr, 0x0016, &check_value);
		if (rc < 0)
		  return rc;

		pr_info("%s: mt9v113: 0x0016 reg value = 0x%x\n",
			__func__, check_value);

		check_value = (check_value&0xFFDF);

		pr_info("%s: mt9v113: Set to 0x0016 reg value = 0x%x\n",
			__func__, check_value);

		rc = mt9v113_i2c_write(mt9v113_client->addr, 0x0016,
			check_value, WORD_LEN);
		if (rc < 0) {
			pr_err("%s: Enter Standby mode fail\n", __func__);
			return rc;
		}
	 

	if (!g_csi_if) {
		
		rc = resume();
		if (rc < 0) {
			pr_err("%s: Enter Active mode fail\n", __func__);
			goto init_fail;
		}
	}

	just_power_on = 0;
	goto init_done;

init_fail:
	pr_info("%s init_fail\n", __func__);
	
	
	return rc;
init_done:
	pr_info("%s init_done\n", __func__);
	return rc;

}

int32_t mt9v113_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		break;
	case CFG_SET_INIT_SETTING:
		
		
#if 0 
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, mt9m114_recommend_settings,
			ARRAY_SIZE(mt9m114_recommend_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			mt9m114_config_change_settings,
			ARRAY_SIZE(mt9m114_config_change_settings),
			MSM_CAMERA_I2C_WORD_DATA);
#endif 
		break;
	case CFG_SET_RESOLUTION:
		if (copy_from_user(&op_mode,
			(void *)cdata->cfg.setting, sizeof(int))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_info("%s: CFG_SET_RESOLUTION %d\n", __func__, op_mode);
		break;
	case CFG_SET_STOP_STREAM:
		if (just_power_on)
			mt9v113_sensor_open_init();
		rc = suspend();  
		if (rc < 0)
			pr_err("%s: suspend fail\n", __func__);
		break;
	case CFG_SET_START_STREAM:
      rc = mt9v113_set_sensor_mode(op_mode);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
#if 0 
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_sensor_power_setting_array *power_setting_array;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
		    (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		
		s_ctrl->power_setting_array =
			sensor_slave_info.power_setting_array;
		power_setting_array = &s_ctrl->power_setting_array;
		power_setting_array->power_setting = kzalloc(
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
		if (!power_setting_array->power_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(power_setting_array->power_setting,
		    (void *)sensor_slave_info.power_setting_array.power_setting,
		    power_setting_array->size *
		    sizeof(struct msm_sensor_power_setting))) {
			kfree(power_setting_array->power_setting);
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->free_power_setting = true;
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			power_setting_array->size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				power_setting_array->power_setting[slave_index].
				seq_type,
				power_setting_array->power_setting[slave_index].
				seq_val,
				power_setting_array->power_setting[slave_index].
				config_val,
				power_setting_array->power_setting[slave_index].
				delay);
		}
		kfree(power_setting_array->power_setting);
#endif 
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		just_power_on = 1;
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
		    sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
		}
		case CFG_SET_SATURATION: {
			int32_t sat_lev;
			if (copy_from_user(&sat_lev, (void *)cdata->cfg.setting,
				sizeof(int32_t))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
		pr_debug("%s: Saturation Value is %d", __func__, sat_lev);
		break;
		}
		case CFG_SET_CONTRAST: {
			int32_t con_lev;
			if (copy_from_user(&con_lev, (void *)cdata->cfg.setting,
				sizeof(int32_t))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
		pr_debug("%s: Contrast Value is %d", __func__, con_lev);
		break;
		}
		case CFG_SET_SHARPNESS: {
			int32_t shp_lev;
			if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
				sizeof(int32_t))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
		pr_debug("%s: Sharpness Value is %d", __func__, shp_lev);
		break;
		}
		case CFG_SET_AUTOFOCUS: {
		
		pr_debug("%s: Setting Auto Focus", __func__);
		break;
		}
		case CFG_CANCEL_AUTOFOCUS: {
		
		pr_debug("%s: Cancelling Auto Focus", __func__);
		break;
		}
		default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static struct msm_sensor_fn_t sensor_func_tbl = {
	.sensor_config = mt9v113_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t mt9v113_s_ctrl = {
	.sensor_i2c_client = &mt9v113_sensor_i2c_client,
	.power_setting_array.power_setting = mt9v113_power_setting,
	.power_setting_array.size = ARRAY_SIZE(mt9v113_power_setting),
	.msm_sensor_mutex = &mt9v113_mut,
	.sensor_v4l2_subdev_info = mt9v113_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9v113_subdev_info),
	.func_tbl = &sensor_func_tbl,
};

module_init(mt9v113_init_module);
module_exit(mt9v113_exit_module);
MODULE_DESCRIPTION("mt9v113");
MODULE_LICENSE("GPL v2");
