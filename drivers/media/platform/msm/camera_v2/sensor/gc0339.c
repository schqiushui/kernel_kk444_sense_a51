/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <mach/gpiomux.h>
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"


#define GC0339_SENSOR_NAME "gc0339"
DEFINE_MSM_MUTEX(gc0339_mut);

#define CONFIG_MSMB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


struct msm_sensor_ctrl_t gc0339_s_ctrl;

int32_t gc0339_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
    pr_info("%s:E", __func__);
	if (!s_ctrl) {
		pr_err("%s:%d failed: %p\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client,
		0xfc,
		0x10,
		MSM_CAMERA_I2C_BYTE_DATA);

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_BYTE_DATA);

	CDBG("%s: read id: 0x%x\n", __func__, chipid);

	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}


struct msm_sensor_fn_t gc0339_sensor_fn_t = {
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = gc0339_match_id,
	.sensor_config = msm_sensor_config,
};


struct msm_sensor_ctrl_t gc0339_s_ctrl = {
	.func_tbl = &gc0339_sensor_fn_t,
};



static const struct of_device_id gc0339_dt_match[] = {
	{.compatible = "htc,gc0339", .data = &gc0339_s_ctrl},
	{}
};


MODULE_DEVICE_TABLE(of, gc0339_dt_match);

MODULE_DESCRIPTION("gc0339");
MODULE_LICENSE("GPL v2");
