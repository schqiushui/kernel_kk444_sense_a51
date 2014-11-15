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

#ifndef __SMB1360_CHARGER_FG_H
#define __SMB1360_CHARGER_FG_H

#include <linux/errno.h>
#include <linux/power_supply.h>
#ifdef CONFIG_HTC_BATT_8960
#include <mach/htc_charger.h>
#endif

#define FALSE       0
#define TRUE        1

#ifdef CONFIG_SMB1360_CHARGER_FG
#ifdef CONFIG_HTC_BATT_8960
int smb1360_is_pwr_src_plugged_in(void);
int smb1360_get_batt_temperature(int *result);
int smb1360_get_batt_voltage(int *result);
int smb1360_get_batt_current(int *result);
int smb1360_get_batt_soc(int *result);
int smb1360_get_batt_cc(int *result);
int smb1360_is_batt_temp_fault_disable_chg(int *result);
int smb1360_is_batt_temperature_fault(int *result);
int smb1360_set_pwrsrc_and_charger_enable(enum htc_power_source_type src,
			bool chg_enable, bool pwrsrc_enable);
int smb1360_charger_enable(bool enable);
int smb1360_get_charge_type(void);
int smb1360_get_chg_usb_iusbmax(void);
int smb1360_get_chg_vinmin(void);
int smb1360_get_input_voltage_regulation(void);
int smb1360_is_charger_ovp(int* result);
int smb1360_dump_all(void);
int smb1360_is_batt_full(int *result);
int smb1360_is_batt_full_eoc_stop(int *result);
int smb1360_limit_input_current(bool enable, int reason);
int smb1360_limit_charge_enable(bool enable);
int smb1360_is_chg_safety_timer_timeout(int *result);
int smb1360_get_batt_id(int *result);
#endif
#else 
#ifdef CONFIG_HTC_BATT_8960
static inline int smb1360_is_pwr_src_plugged_in(void)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_temperature(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_voltage(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_current(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_soc(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_cc(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_present(void)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_status(void)
{
	return -ENXIO;
}
static inline int smb1360_is_batt_temp_fault_disable_chg(int *result)
{
	return -ENXIO;
}
static inline int smb1360_vm_bms_report_eoc(void)
{
	return -ENXIO;
}
static inline int smb1360_is_batt_temperature_fault(int *result)
{
	return -ENXIO;
}
static inline int smb1360_set_pwrsrc_and_charger_enable(enum htc_power_source_type src,
			bool chg_enable, bool pwrsrc_enable)
{
	return -ENXIO;
}
static inline int smb1360_charger_enable(bool enable)
{
	return -ENXIO;
}
static inline int smb1360_is_charger_ovp(int* result)
{
	return -ENXIO;
}
static inline int smb1360_pwrsrc_enable(bool enable)
{
	return -ENXIO;
}
static inline int smb1360_get_battery_status(void)
{
	return -ENXIO;
}
static inline int smb1360_get_charging_source(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_charging_enabled(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_charge_type(void)
{
	return -ENXIO;
}
static inline int smb1360_get_chg_usb_iusbmax(void)
{
	return -ENXIO;
}
static inline int smb1360_get_chg_vinmin(void)
{
	return -ENXIO;
}
static inline int smb1360_get_input_voltage_regulation(void)
{
	return -ENXIO;
}
static inline int smb1360_set_chg_vin_min(int val)
{
	return -ENXIO;
}
static inline int smb1360_set_chg_iusbmax(int val)
{
	return -ENXIO;
}
static inline int smb1360_set_hsml_target_ma(int target_ma)
{
	return -ENXIO;
}
static inline int smb1360_dump_all(void)
{
	return -ENXIO;
}
static inline int smb1360_is_batt_full(int *result)
{
	return -ENXIO;
}
static inline int smb1360_is_batt_full_eoc_stop(int *result)
{
	return -ENXIO;
}
static inline int smb1360_charger_get_attr_text(char *buf, int size)
{
	return -ENXIO;
}
static inline int smb1360_fake_usbin_valid_irq_handler(void)
{
	return -ENXIO;
}
static inline int smb1360_fake_coarse_det_usb_irq_handler(void)
{
	return -ENXIO;
}
static inline int smb1360_is_chg_safety_timer_timeout(int *result)
{
	return -ENXIO;
}
static inline int smb1360_get_usb_temperature(int *result)
{
	return -ENXIO;
}
static inline int smb1360_limit_charge_enable(bool enable)
{
	return -ENXIO;
}
static inline int smb1360_get_batt_id(int *result)
{
	return -ENXIO;
}
static inline int smb1360_limit_input_current(bool enable, int reason);
{
	return -ENXIO;
}
#endif 
#endif 
#endif 

