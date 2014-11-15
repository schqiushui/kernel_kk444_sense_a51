/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include "msm_sensor_driver.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"

#define s5k5e_SENSOR_NAME "s5k5e"
#define PLATFORM_DRIVER_NAME "msm_camera_s5k5e"
#define s5k5e_obj s5k5e_##obj

#define CONFIG_MSMB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err("[CAM]"fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


DEFINE_MSM_MUTEX(s5k5e_mut);

struct msm_sensor_ctrl_t s5k5e_s_ctrl;

struct msm_sensor_fn_t s5k5e_sensor_func_tbl = {
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_config = msm_sensor_config,
};

struct msm_sensor_ctrl_t s5k5e_s_ctrl = {
	.func_tbl = &s5k5e_sensor_func_tbl
};

MODULE_DEVICE_TABLE(of, s5k5e_dt_match);

MODULE_DESCRIPTION("s5k5e");
MODULE_LICENSE("GPL v2");
