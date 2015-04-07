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
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <asm/mach/arch.h>
#include <soc/qcom/socinfo.h>
#include <mach/board.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_iomap.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include "board-dt.h"
#include "platsmp.h"
#include <linux/gpio.h>
#include <mach/cable_detect.h>
#include <linux/usb/android.h>
#include <mach/devices_cmdline.h>
#include <mach/devices_dtb.h>
#ifdef CONFIG_HTC_POWER_DEBUG
#include <soc/qcom/htc_util.h>
#include <mach/devices_dtb.h>
#endif

#ifdef CONFIG_BT
#include <mach/htc_bdaddress.h>
#endif
#include <linux/pstore_ram.h>
#include <linux/memblock.h>

#ifdef CONFIG_HTC_BATT_8960
#include "mach/htc_battery_8960.h"
#include "mach/htc_battery_cell.h"
#ifdef CONFIG_SMB1360_CHARGER_FG
#include <linux/qpnp/smb1360-charger-fg.h>
#else
#include <linux/qpnp/qpnp-linear-charger.h>
#include <linux/qpnp/qpnp-vm-bms.h>
#endif 
#endif 

#ifdef CONFIG_HTC_DEBUG_FOOTPRINT
#include <mach/htc_mnemosyne.h>
#endif

#ifdef CONFIG_HTC_PNPMGR
#include <linux/htc_pnpmgr.h>
#endif

extern int __init htc_8939_dsi_panel_power_register(void);

#define HTC_8939_A51_UL_PROJECT_ID 342
#define HTC_8939_A51_DUGL_PROJECT_ID 343
#define HTC_8939_A51_DTUL_PROJECT_ID 344
#define HTC_8939_A51_DTGL_PROJECT_ID 345
#define HTC_8939_USB1_HS_ID_GPIO 110 + 902

static int htc_get_usbid(void)
{
	int usbid_gpio;

	usbid_gpio = HTC_8939_USB1_HS_ID_GPIO;
	pr_debug("%s: pcbid=%d, usbid_gpio=%d\n", __func__, of_machine_pcbid(), usbid_gpio);

	return usbid_gpio;
}

static int64_t htc_8x26_get_usbid_adc(void)
{
	return htc_qpnp_adc_get_usbid_adc();
}

static void htc_8x26_config_usb_id_gpios(bool output)
{
	if (output) {
		if (gpio_direction_output(htc_get_usbid(),1)) {
			printk(KERN_ERR "[CABLE] fail to config usb id, output = %d\n",output);
			return;
		}
		pr_info("[CABLE] %s: %d output high\n",  __func__, htc_get_usbid());
	} else {
		if (gpio_direction_input(htc_get_usbid())) {
			printk(KERN_ERR "[CABLE] fail to config usb id, output = %d\n",output);
			return;
		}
		pr_info("[CABLE] %s: %d intput nopull\n",  __func__, htc_get_usbid());
	}
}

static struct cable_detect_platform_data cable_detect_pdata = {
	.detect_type            = CABLE_TYPE_PMIC_ADC,
	.usb_id_pin_type        = CABLE_TYPE_APP_GPIO,
	.usb_id_pin_gpio        = htc_get_usbid,
	.get_adc_cb             = htc_8x26_get_usbid_adc,
	.config_usb_id_gpios    = htc_8x26_config_usb_id_gpios,
#ifdef CONFIG_FB_MSM_HDMI_MHL
	.mhl_1v2_power = mhl_sii9234_1v2_power,
	.usb_dpdn_switch        = m7_usb_dpdn_switch,
#endif
#ifdef CONFIG_HTC_BATT_8960
#ifdef CONFIG_SMB1360_CHARGER_FG
	.is_pwr_src_plugged_in	= smb1360_is_pwr_src_plugged_in,
#else
	.is_pwr_src_plugged_in	= pm8916_is_pwr_src_plugged_in,
#endif 
#endif 
	.vbus_debounce_retry = 1,
};

static struct platform_device cable_detect_device = {
	.name   = "cable_detect",
	.id     = -1,
	.dev    = {
		.platform_data = &cable_detect_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id      = 0x0bb4,
	.product_id     = 0x0dff, 
	.product_name		= "Android Phone",
	.manufacturer_name	= "HTC",
	.serial_number = "123456789012",
	.usb_core_id = 0,
	.usb_rmnet_interface = "smd,bam",
	.usb_diag_interface = "diag",
	.fserial_init_string = "smd:modem,tty,tty:autobot,tty:serial,tty:autobot,tty:acm",
#ifdef CONFIG_MACH_MEM_WL
	.match = memwl_usb_product_id_match,
#endif
	.nluns = 1,
	.cdrom_lun = 0x1,
	.vzw_unmount_cdrom = 0,
};

#define QCT_ANDROID_USB_REGS 0x086000c8
#define QCT_ANDROID_USB_SIZE 0xc8
static struct resource resources_android_usb[] = {
	{
		.start  = QCT_ANDROID_USB_REGS,
		.end    = QCT_ANDROID_USB_REGS + QCT_ANDROID_USB_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device android_usb_device = {
	.name   = "android_usb",
	.id     = -1,
	.num_resources  = ARRAY_SIZE(resources_android_usb),
	.resource       = resources_android_usb,
	.dev    = {
		.platform_data = &android_usb_pdata,
	},
};

static void msm8916_add_usb_devices(void)
{
	int mode = board_mfg_mode();
	int project_id = of_machine_pid();
	android_usb_pdata.serial_number = board_serialno();

	if (mode != 0) {
		android_usb_pdata.nluns = 1;
		android_usb_pdata.cdrom_lun = 0x0;
	}

	if ((!(get_radio_flag() & BIT(17))) && (mode == MFG_MODE_MFGKERNEL || mode == MFG_MODE_MFGKERNEL_DIAG58)) {
		android_usb_pdata.fserial_init_string = "tty,tty:autobot,tty:serial,tty:autobot,tty:acm";
	}

	if (project_id == HTC_8939_A51_UL_PROJECT_ID)
		android_usb_pdata.product_id = 0x658;
	else if (project_id == HTC_8939_A51_DUGL_PROJECT_ID)
		android_usb_pdata.product_id = 0x657;
	else if (project_id == HTC_8939_A51_DTUL_PROJECT_ID)
		android_usb_pdata.product_id = 0x64d;
	else if (project_id == HTC_8939_A51_DTGL_PROJECT_ID)
		android_usb_pdata.product_id = 0x656;

	platform_device_register(&android_usb_device);
}

#ifdef CONFIG_HTC_POWER_DEBUG
static struct platform_device cpu_usage_stats_device = {
       .name = "cpu_usage_stats",
       .id = -1,
};

int __init htc_cpu_usage_register(void)
{
       platform_device_register(&cpu_usage_stats_device);
       return 0;
}
#endif

static void msm8916_cable_detect_register(void)
{
	platform_device_register(&cable_detect_device);
}

#ifdef CONFIG_HTC_PNPMGR
#if (CONFIG_NR_CPUS == 4)
unsigned msm8916_bc_perf_table[] = {
	533330,  
	800000,  
	998400,  
	1094400, 
	1190400, 
};
static struct user_perf_data msm8916_perf_data = {
	.bc_perf_table = msm8916_bc_perf_table,
	.lc_perf_table = NULL,
};
#else
unsigned msm8916_bc_perf_table[] = {
	533330,  
	652800,  
	800000,  
	1113600, 
	1344000, 
};
unsigned msm8916_lc_perf_table[] = {
	499200,  
	800000,  
	998400, 
	998400, 
	998400, 
};
static struct user_perf_data msm8916_perf_data = {
	.bc_perf_table = msm8916_bc_perf_table,
	.lc_perf_table = msm8916_lc_perf_table,
};
#endif
#endif

#define RAMOOPS_MEM_PHY 0x8C800000
#define RAMOOPS_MEM_SIZE SZ_1M

static struct ramoops_platform_data ramoops_data = {
	.mem_size		= RAMOOPS_MEM_SIZE,
	.mem_address	= RAMOOPS_MEM_PHY,
	.console_size	= RAMOOPS_MEM_SIZE,
	.dump_oops		= 1,
};

static struct platform_device ramoops_dev = {
	.name = "ramoops",
	.dev = {
	.platform_data = &ramoops_data,
	},
};

static void __init htc_8916_dt_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
	memblock_reserve(ramoops_data.mem_address, ramoops_data.mem_size);
}

static void __init htc_8916_map_io(void)
{
	msm_map_msm8916_io();
}

static struct of_dev_auxdata htc_8916_auxdata_lookup[] __initdata = {
	{}
};

#if defined(CONFIG_HTC_BATT_8960)
#ifdef CONFIG_HTC_PNPMGR
extern int pnpmgr_battery_charging_enabled(int charging_enabled);
#endif 
static int critical_alarm_voltage_mv[] = {3000, 3200, 3400};

static struct htc_battery_platform_data htc_battery_pdev_data = {
	.guage_driver = 0,
#ifdef CONFIG_SMB1360_CHARGER_FG
	.ibat_limit_active_mask = HTC_BATT_CHG_LIMIT_BIT_TALK |
								HTC_BATT_CHG_LIMIT_BIT_NAVI,
	.iusb_limit_active_mask = 0,
#else
	.ibat_limit_active_mask = HTC_BATT_CHG_LIMIT_BIT_TALK |
								HTC_BATT_CHG_LIMIT_BIT_NAVI |
								HTC_BATT_CHG_LIMIT_BIT_THRML,
	.iusb_limit_active_mask = 0,
#endif
	.critical_low_voltage_mv = 3200,
	.critical_alarm_vol_ptr = critical_alarm_voltage_mv,
	.critical_alarm_vol_cols = sizeof(critical_alarm_voltage_mv) / sizeof(int),
	.overload_vol_thr_mv = 4000,
	.overload_curr_thr_ma = 0,
	.smooth_chg_full_delay_min = 3,
	.decreased_batt_level_check = 1,
#ifdef CONFIG_SMB1360_CHARGER_FG
	.force_shutdown_batt_vol = 3000,
#endif

#ifdef CONFIG_QPNP_VM_BMS
	.icharger.name = "pm8916",
		.icharger.get_charging_source = pm8916_get_charging_source,
	.icharger.get_charging_enabled = pm8916_get_charging_enabled,
	.icharger.set_charger_enable = pm8916_charger_enable,
	
	.icharger.set_pwrsrc_enable = pm8916_charger_enable,
	.icharger.set_pwrsrc_and_charger_enable =
						pm8916_set_pwrsrc_and_charger_enable,
	.icharger.set_limit_charge_enable = pm8916_limit_charge_enable,
	.icharger.set_chg_iusbmax = pm8916_set_chg_iusbmax,
	.icharger.set_chg_vin_min = pm8916_set_chg_vin_min,
	.icharger.is_ovp = pm8916_is_charger_ovp,
	.icharger.is_batt_temp_fault_disable_chg =
						pm8916_is_batt_temp_fault_disable_chg,
	.icharger.charger_change_notifier_register =
						cable_detect_register_notifier,
	.icharger.is_safty_timer_timeout = pm8916_is_chg_safety_timer_timeout,
	.icharger.get_attr_text = pm8916_charger_get_attr_text,
	.icharger.max_input_current = pm8916_set_hsml_target_ma,
	.icharger.is_battery_full_eoc_stop = pm8916_is_batt_full_eoc_stop,
	.icharger.get_charge_type = pm8916_get_charge_type,
	.icharger.get_chg_usb_iusbmax = pm8916_get_chg_usb_iusbmax,
	.icharger.get_chg_vinmin = pm8916_get_chg_vinmin,
	.icharger.get_input_voltage_regulation =
						pm8916_get_input_voltage_regulation,
	.icharger.dump_all = pm8916_dump_all,
#endif 

#ifdef CONFIG_QPNP_VM_BMS
	.igauge.name = "pm8916",
	.igauge.get_battery_voltage = pm8916_get_batt_voltage,
	.igauge.get_battery_current = pm8916_get_batt_current,
	.igauge.get_battery_temperature = pm8916_get_batt_temperature,
	.igauge.get_battery_id = pm8916_get_batt_id,
	.igauge.get_battery_soc = pm8916_get_batt_soc,
	.igauge.get_battery_cc = pm8916_get_batt_cc,
	.igauge.is_battery_full = pm8916_is_batt_full,
	.igauge.is_battery_temp_fault = pm8916_is_batt_temperature_fault,
	.igauge.get_attr_text = pm8916_gauge_get_attr_text,
	.igauge.get_usb_temperature = pm8916_get_usb_temperature,
	.igauge.set_lower_voltage_alarm_threshold =
						pm8916_batt_lower_alarm_threshold_set,
	.igauge.store_battery_data = pm8916_bms_store_battery_data_emmc,
	.igauge.store_battery_ui_soc = pm8916_bms_store_battery_ui_soc,
	.igauge.get_battery_ui_soc = pm8916_bms_get_battery_ui_soc,
#endif 

#ifdef CONFIG_SMB1360_CHARGER_FG
	.icharger.name = "smb1360",
	.icharger.set_charger_enable = smb1360_charger_enable,
	
	.icharger.set_pwrsrc_and_charger_enable =
						smb1360_set_pwrsrc_and_charger_enable,
	.icharger.set_limit_charge_enable = smb1360_limit_charge_enable,
	.icharger.is_ovp = smb1360_is_charger_ovp,
	.icharger.is_batt_temp_fault_disable_chg =
						smb1360_is_batt_temp_fault_disable_chg,
	.icharger.charger_change_notifier_register =
						cable_detect_register_notifier,
	.icharger.is_safty_timer_timeout = smb1360_is_chg_safety_timer_timeout,
	.icharger.is_battery_full_eoc_stop = smb1360_is_batt_full_eoc_stop,
	.icharger.get_charge_type = smb1360_get_charge_type,
	.icharger.get_chg_usb_iusbmax = smb1360_get_chg_usb_iusbmax,
	.icharger.get_chg_vinmin = smb1360_get_chg_vinmin,
	.icharger.get_input_voltage_regulation =
						smb1360_get_input_voltage_regulation,
	.icharger.dump_all = smb1360_dump_all,
	.icharger.set_limit_input_current = smb1360_limit_input_current,

	.igauge.name = "smb1360",
	.igauge.get_battery_voltage = smb1360_get_batt_voltage,
	.igauge.get_battery_current = smb1360_get_batt_current,
	.igauge.get_battery_temperature = smb1360_get_batt_temperature,
	.igauge.get_battery_id = smb1360_get_batt_id,
	.igauge.get_battery_soc = smb1360_get_batt_soc,
	.igauge.get_battery_cc = smb1360_get_batt_cc,
	.igauge.is_battery_full = smb1360_is_batt_full,
	.igauge.is_battery_temp_fault = smb1360_is_batt_temperature_fault,
#endif 
			
#ifdef CONFIG_HTC_PNPMGR
	.notify_pnpmgr_charging_enabled = pnpmgr_battery_charging_enabled,
#endif 

};
static struct platform_device htc_battery_pdev = {
	.name = "htc_battery",
	.id = -1,
	.dev    = {
		.platform_data = &htc_battery_pdev_data,
	},
};

static void msm8x16_add_batt_devices(void)
{
	platform_device_register(&htc_battery_pdev);
}

static struct platform_device htc_battery_cell_pdev = {
	.name = "htc_battery_cell",
	.id = -1,
};

int __init htc_batt_cell_register(void)
{
	platform_device_register(&htc_battery_cell_pdev);
	return 0;
}
#endif 

void __init htc_8916_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	htc_8939_dsi_panel_power_register();
	msm8916_cable_detect_register();
	msm8916_add_usb_devices();
	platform_device_register(&ramoops_dev);
#ifdef CONFIG_HTC_POWER_DEBUG
        htc_cpu_usage_register();
#endif
#if defined(CONFIG_HTC_BATT_8960)
	htc_batt_cell_register();
	msm8x16_add_batt_devices();
#endif 
}

void __init htc_8916_init_early(void)
{
#ifdef CONFIG_HTC_DEBUG_FOOTPRINT
	mnemosyne_early_init((unsigned int)HTC_DEBUG_FOOTPRINT_PHYS, (unsigned int)HTC_DEBUG_FOOTPRINT_BASE);
#endif
}

static void __init htc_8916_init(void)
{
	struct of_dev_auxdata *adata = htc_8916_auxdata_lookup;

	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

       pr_info("%s: pid=%d, pcbid=0x%X, subtype=0x%X, socver=0x%X\n", __func__
               , of_machine_pid(), of_machine_pcbid(), of_machine_subtype(), of_machine_socver());

	htc_8916_add_drivers();

#ifdef CONFIG_HTC_POWER_DEBUG
        htc_monitor_init();
#endif

#ifdef CONFIG_BT
	bt_export_bd_address();
#endif
#ifdef CONFIG_HTC_PNPMGR
	pnpmgr_init_perf_table(&msm8916_perf_data);
#endif
}

static const char *htc_8916_dt_match[] __initconst = {
	"htc,msm8916",
	NULL
};

static const char *htc_8936_dt_match[] __initconst = {
	"htc,msm8936",
	NULL
};

static const char *htc_8939_dt_match[] __initconst = {
	"htc,msm8939",
	NULL
};

DT_MACHINE_START(MSM8916_DT, "UNKNOWN")
	.map_io = htc_8916_map_io,
	.init_early = htc_8916_init_early,
	.init_machine = htc_8916_init,
	.dt_compat = htc_8916_dt_match,
	.reserve = htc_8916_dt_reserve,
	.smp = &msm8916_smp_ops,
MACHINE_END

DT_MACHINE_START(MSM8936_DT, "UNKNOWN")
	.map_io = htc_8916_map_io,
	.init_early = htc_8916_init_early,
	.init_machine = htc_8916_init,
	.dt_compat = htc_8936_dt_match,
	.reserve = htc_8916_dt_reserve,
	.smp = &msm8936_smp_ops,
MACHINE_END


DT_MACHINE_START(MSM8939_DT, "UNKNOWN")
	.map_io = htc_8916_map_io,
	.init_early = htc_8916_init_early,
	.init_machine = htc_8916_init,
	.dt_compat = htc_8939_dt_match,
	.reserve = htc_8916_dt_reserve,
	.smp = &msm8936_smp_ops,
MACHINE_END
