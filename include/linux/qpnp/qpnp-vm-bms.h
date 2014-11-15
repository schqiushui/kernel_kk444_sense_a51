/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __QPNP_VM_BMS_H
#define __QPNP_VM_BMS_H

#ifdef CONFIG_QPNP_VM_BMS
#ifdef CONFIG_HTC_BATT_8960
int pm8916_bms_dump_all(void);
int pm8916_get_bms_capacity(int *result);
int pm8916_get_batt_id(int *result);
int pm8916_gauge_get_attr_text(char *buf, int size);
int pm8916_batt_lower_alarm_threshold_set(int threshold_mV);
int pm8916_bms_store_battery_data_emmc(void);
int pm8916_bms_store_battery_ui_soc(int soc_ui);
int pm8916_bms_get_battery_ui_soc(void);
#endif 
#else 
#ifdef CONFIG_HTC_BATT_8960
static inline int pm8916_bms_dump_all(void)
{
	return -ENXIO;
}
static inline int pm8916_get_bms_capacity(int *result)
{
	return -ENXIO;
}
static inline int pm8916_get_batt_id(int *result)
{
	return -ENXIO;
}
static inline int pm8916_gauge_get_attr_text(char *buf, int size)
{
	return -ENXIO;
}
static int pm8916_batt_lower_alarm_threshold_set(int threshold_mV)
{
	return -ENXIO;
}
static int pm8916_bms_store_battery_data_emmc(void)
{
	return -ENXIO;
}
static int pm8916_bms_get_battery_ui_soc(void)
{
	return -ENXIO;
}
static int pm8916_bms_store_battery_ui_soc(int soc_ui)
{
	return -ENXIO;
}
#endif 
#endif 

#endif 
