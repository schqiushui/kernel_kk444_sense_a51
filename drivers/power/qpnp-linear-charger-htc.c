/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"[BATT][CHG]: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/bitops.h>
#include <linux/qpnp/qpnp-linear-charger.h>
#include <linux/qpnp/qpnp-vm-bms.h>
#include <mach/cable_detect.h>
#include <mach/devices_cmdline.h>
#include <mach/devices_dtb.h>
#include <mach/htc_gauge.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define CREATE_MASK(NUM_BITS, POS) \
	((unsigned char) (((1 << (NUM_BITS)) - 1) << (POS)))
#define LBC_MASK(MSB_BIT, LSB_BIT) \
	CREATE_MASK(MSB_BIT - LSB_BIT + 1, LSB_BIT)

#define INT_RT_STS_REG				0x10
#define FAST_CHG_ON_IRQ                         BIT(5)
#define OVERTEMP_ON_IRQ				BIT(4)
#define BAT_TEMP_OK_IRQ                         BIT(1)
#define BATT_PRES_IRQ                           BIT(0)

#define USB_PTH_STS_REG				0x09
#define USB_INT_RT_STS				0x10
#define USB_IN_VALID_MASK			LBC_MASK(7, 6)
#define USB_OVP_CTL_REG			0x42
#define USB_SUSP_REG				0x47
#define USB_SUSPEND_BIT				BIT(0)
#define USB_ENUM_TIMER_STOP_REG		0x4E
#define USB_ENUM_TIMER_REG			0x4F

#define CHG_OPTION_REG				0x08
#define CHG_OPTION_MASK				BIT(7)
#define CHG_STATUS_REG				0x09
#define CHG_INT_RT_STS				0x10
#define CHG_VDD_MAX_REG				0x40
#define CHG_VDD_SAFE_REG			0x41
#define CHG_IBAT_MAX_REG			0x44
#define CHG_IBAT_SAFE_REG			0x45
#define CHG_VIN_MIN_REG				0x47
#define CHG_CTRL_REG				0x49
#define CHG_ENABLE				BIT(7)
#define CHG_FORCE_BATT_ON			BIT(0)
#define CHG_EN_MASK				(BIT(7) | BIT(0))
#define CHG_FAILED_REG				0x4A
#define CHG_FAILED_BIT				BIT(7)
#define CHG_VBAT_WEAK_REG			0x52
#define CHG_IBATTERM_EN_REG			0x5B
#define CHG_USB_ENUM_T_STOP_REG			0x4E
#define CHG_TCHG_MAX_EN_REG			0x60
#define CHG_TCHG_MAX_REG			0x61
#define CHG_CHG_WDOG_TIME_REG			0x62
#define CHG_WDOG_EN_REG				0x65
#define CHG_FSM_STATE_REG           0xE7
#define CHG_PERPH_RESET_CTRL3_REG	0xDA
#define CHG_COMP_OVR1				0xEE
#define CHG_VBAT_DET_OVR_MASK			LBC_MASK(1, 0)
#define OVERRIDE_0				0x2
#define OVERRIDE_NONE				0x0

#define BAT_IF_PRES_STATUS_REG			0x08
#define BATT_PRES_MASK				BIT(7)
#define BAT_IF_TEMP_STATUS_REG			0x09
#define BAT_IF_INT_RT_STS_REG			0x10
#define BATT_TEMP_HOT_MASK			BIT(6)
#define BATT_TEMP_COLD_MASK			LBC_MASK(7, 6)
#define BATT_TEMP_OK_MASK			BIT(7)
#define BAT_IF_VREF_BAT_THM_CTRL_REG		0x4A
#define VREF_BATT_THERM_FORCE_ON		LBC_MASK(7, 6)
#define VREF_BAT_THM_ENABLED_FSM		BIT(7)
#define BAT_IF_BPD_CTRL_REG			0x48
#define BATT_BPD_CTRL_SEL_MASK			LBC_MASK(1, 0)
#define BATT_BPD_OFFMODE_EN			BIT(3)
#define BATT_THM_EN				BIT(1)
#define BATT_ID_EN				BIT(0)
#define BAT_IF_BTC_CTRL				0x49
#define BTC_COMP_EN_MASK			BIT(7)
#define BTC_COLD_MASK				BIT(1)
#define BTC_HOT_MASK				BIT(0)

#define MISC_BOOT_DONE_REG                     0x42
#define MISC_BOOT_DONE				BIT(7)

#define PERP_SUBTYPE_REG			0x05
#define SEC_ACCESS                              0xD0

#define LBC_CHGR_SUBTYPE			0x15
#define LBC_BAT_IF_SUBTYPE			0x16
#define LBC_USB_PTH_SUBTYPE			0x17
#define LBC_MISC_SUBTYPE			0x18

#define QPNP_CHG_I_MAX_MIN_90                   90

#define USB_MA_0       (0)
#define USB_MA_2       (2)
#define USB_MA_100     (100)
#define USB_MA_200     (200)
#define USB_MA_300     (300)
#define USB_MA_400     (400)
#define USB_MA_450     (450)
#define USB_MA_500     (500)
#define USB_MA_900     (900)
#define USB_MA_1100    (1100)
#define USB_MA_1500	(1500)
#define USB_MA_1600	(1600)

#define PM8916_CHG_I_MIN_MA         450

#define QPNP_CHARGER_DEV_NAME	"qcom,qpnp-linear-charger"


struct qpnp_lbc_irq {
	int		irq;
	unsigned long	disabled;
};

enum {
	USBIN_VALID = 0,
	USB_OVER_TEMP,
	USB_CHG_GONE,
	BATT_PRES,
	BATT_TEMPOK,
	CHG_DONE,
	CHG_FAILED,
	CHG_FAST_CHG,
	CHG_VBAT_DET_LO,
	COARSE_DET_USB,
	MAX_IRQS,
};

enum {
	USER	= BIT(0),
	THERMAL = BIT(1),
	CURRENT = BIT(2),
	SOC	= BIT(3),
};

enum bpd_type {
	BPD_TYPE_BAT_ID,
	BPD_TYPE_BAT_THM,
	BPD_TYPE_BAT_THM_BAT_ID,
};

static const char * const bpd_label[] = {
	[BPD_TYPE_BAT_ID] = "bpd_id",
	[BPD_TYPE_BAT_THM] = "bpd_thm",
	[BPD_TYPE_BAT_THM_BAT_ID] = "bpd_thm_id",
};

enum btc_type {
	HOT_THD_25_PCT = 25,
	HOT_THD_35_PCT = 35,
	COLD_THD_70_PCT = 70,
	COLD_THD_80_PCT = 80,
};

static u8 btc_value[] = {
	[HOT_THD_25_PCT] = 0x0,
	[HOT_THD_35_PCT] = BIT(0),
	[COLD_THD_70_PCT] = 0x0,
	[COLD_THD_80_PCT] = BIT(1),
};

static inline int get_bpd(const char *name)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(bpd_label); i++) {
		if (strcmp(bpd_label[i], name) == 0)
			return i;
	}
	return -EINVAL;
}

struct lbc_wakeup_source {
	struct wakeup_source	source;
	unsigned long		disabled;
};

struct qpnp_lbc_chip {
	struct device			*dev;
	struct spmi_device		*spmi;
	u16				chgr_base;
	u16				bat_if_base;
	u16				usb_chgpth_base;
	u16				misc_base;
	bool				bat_is_cool;
	bool				bat_is_warm;
	bool				chg_done;
	bool				usb_present;
	bool				batt_present;
	bool				cfg_charging_disabled;
	bool				cfg_btc_disabled;
	bool				cfg_use_fake_battery;
	bool				fastchg_on;
	bool				cfg_use_external_charger;
	unsigned int			cfg_warm_bat_chg_ma;
	unsigned int			cfg_cool_bat_chg_ma;
	unsigned int			cfg_safe_voltage_mv;
	unsigned int			cfg_max_voltage_mv;
	unsigned int			cfg_min_voltage_mv;
	unsigned int			cfg_charger_detect_eoc;
	unsigned int			cfg_batt_weak_voltage_uv;
	unsigned int			cfg_warm_bat_mv;
	unsigned int			cfg_cool_bat_mv;
	unsigned int			cfg_hot_batt_p;
	unsigned int			cfg_cold_batt_p;
	unsigned int			cfg_thermal_levels;
	unsigned int			therm_lvl_sel;
	unsigned int			*thermal_mitigation;
	unsigned int			cfg_safe_current;
	unsigned int			cfg_tchg_mins;
	unsigned int			chg_failed_count;
	unsigned int			cfg_disable_follow_on_reset;
	int				cfg_bpd_detection;
	int				cfg_warm_bat_decidegc;
	int				cfg_cool_bat_decidegc;
	int				fake_battery_soc;
	int				cfg_soc_resume_limit;
	int				cfg_float_charge;
	int				charger_disabled;
	int				prev_max_ma;
	int				usb_psy_ma;
	int				mbat_in_gpio;
	int				is_embeded_batt;
	int				term_current;
	struct qpnp_lbc_irq		irqs[MAX_IRQS];
	struct mutex			jeita_configure_lock;
	struct mutex			chg_enable_lock;
	spinlock_t			ibat_change_lock;
	spinlock_t			hw_access_lock;
	struct power_supply		*bms_psy;
	struct qpnp_adc_tm_btm_param	adc_param;
	struct qpnp_vadc_chip		*vadc_dev;
	struct qpnp_adc_tm_chip		*adc_tm_dev;
	struct lbc_wakeup_source	lbc_chg_wake_source;
	struct delayed_work		eoc_work;
};

static int qpnp_lbc_is_fastchg_on(struct qpnp_lbc_chip *chip);

static struct qpnp_lbc_chip *the_chip;
static unsigned int chg_limit_current; 
enum htc_power_source_type pwr_src;

static int ovp = false;
static int uvp = false;

static bool flag_keep_charge_on;
static bool flag_pa_recharge;
static bool flag_enable_bms_charger_log;
static int test_power_monitor;

static bool is_ac_safety_timeout = false;
static bool is_ac_safety_timeout_twice = false;

static int hsml_target_ma;
static int usb_target_ma;
static int usb_wall_threshold_ma;
static bool is_batt_full = false;
static bool is_batt_full_eoc_stop = false;

extern int board_ftm_mode(void);

#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug(fmt, args...) do { \
		if (flag_enable_bms_charger_log) \
			printk(KERN_WARNING pr_fmt(fmt), ## args); \
	} while (0)

static int __qpnp_lbc_read(struct spmi_device *spmi, u16 base,
			u8 *val, int count)
{
	int rc = 0;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, base, val, count);
	if (rc)
		pr_err("SPMI read failed base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);

	return rc;
}

static int __qpnp_lbc_write(struct spmi_device *spmi, u16 base,
			u8 *val, int count)
{
	int rc;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, base, val,
					count);
	if (rc)
		pr_err("SPMI write failed base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);

	return rc;
}

static int __qpnp_lbc_secure_write(struct spmi_device *spmi, u16 base,
				u16 offset, u8 *val, int count)
{
	int rc;
	u8 reg_val;

	reg_val = 0xA5;
	rc = __qpnp_lbc_write(spmi, base + SEC_ACCESS, &reg_val, 1);
	if (rc) {
		pr_err("SPMI read failed base=0x%02x sid=0x%02x rc=%d\n",
				base + SEC_ACCESS, spmi->sid, rc);
		return rc;
	}

	rc = __qpnp_lbc_write(spmi, base + offset, val, 1);
	if (rc)
		pr_err("SPMI write failed base=0x%02x sid=0x%02x rc=%d\n",
				base + SEC_ACCESS, spmi->sid, rc);

	return rc;
}

static int qpnp_lbc_read(struct qpnp_lbc_chip *chip, u16 base,
			u8 *val, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags;

	if (base == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->hw_access_lock, flags);
	rc = __qpnp_lbc_read(spmi, base, val, count);
	spin_unlock_irqrestore(&chip->hw_access_lock, flags);

	return rc;
}

static int qpnp_lbc_write(struct qpnp_lbc_chip *chip, u16 base,
			u8 *val, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags;

	if (base == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);
		return -EINVAL;
	}
	pr_debug("writing to base=%x val=%x\n", base, *val);
	spin_lock_irqsave(&chip->hw_access_lock, flags);
	rc = __qpnp_lbc_write(spmi, base, val, count);
	spin_unlock_irqrestore(&chip->hw_access_lock, flags);

	return rc;
}

static int qpnp_lbc_masked_write(struct qpnp_lbc_chip *chip, u16 base,
				u8 mask, u8 val)
{
	int rc;
	u8 reg_val;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags;

	spin_lock_irqsave(&chip->hw_access_lock, flags);
	rc = __qpnp_lbc_read(spmi, base, &reg_val, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n", base, rc);
		goto out;
	}
	pr_debug("addr = 0x%x read 0x%x\n", base, reg_val);

	reg_val &= ~mask;
	reg_val |= val & mask;

	pr_debug("writing to base=%x val=%x\n", base, reg_val);

	rc = __qpnp_lbc_write(spmi, base, &reg_val, 1);
	if (rc)
		pr_err("spmi write failed: addr=%03X, rc=%d\n", base, rc);

out:
	spin_unlock_irqrestore(&chip->hw_access_lock, flags);
	return rc;
}

static void lbc_stay_awake(struct lbc_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

#define USB_VALID_BIT	BIT(7)
static void lbc_relax(struct lbc_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static int qpnp_lbc_is_usb_chg_plugged_in(struct qpnp_lbc_chip *chip)
{
	u8 usbin_valid_rt_sts;
	int rc;

	rc = qpnp_lbc_read(chip, chip->usb_chgpth_base + USB_PTH_STS_REG,
				&usbin_valid_rt_sts, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				chip->usb_chgpth_base + USB_PTH_STS_REG, rc);
		return rc;
	}

	pr_debug("usb_path_sts 0x%x\n", usbin_valid_rt_sts);

	return (usbin_valid_rt_sts & USB_VALID_BIT) ? 1 : 0;
}

static int qpnp_lbc_charger_enable(struct qpnp_lbc_chip *chip, int reason,
					int enable)
{
	int disabled = chip->charger_disabled;
	u8 reg_val;
	int rc = 0;

	pr_debug("reason=%d requested_enable=%d disabled_status=%d\n",
					reason, enable, disabled);
	if (enable)
		disabled &= ~reason;
	else
		disabled |= reason;

	if (!!chip->charger_disabled == !!disabled)
		goto skip;

	reg_val = !!disabled ? CHG_FORCE_BATT_ON : CHG_ENABLE;
	rc = qpnp_lbc_masked_write(chip, chip->chgr_base + CHG_CTRL_REG,
				CHG_EN_MASK, reg_val);
	if (rc) {
		pr_err("Failed to %s charger rc=%d\n",
				reg_val ? "enable" : "disable", rc);
		return rc;
	}

skip:
	chip->charger_disabled = disabled;
	return rc;
}

static int qpnp_lbc_is_batt_present(struct qpnp_lbc_chip *chip)
{
	u8 batt_pres_rt_sts;
	int rc;

	rc = qpnp_lbc_read(chip, chip->bat_if_base + INT_RT_STS_REG,
				&batt_pres_rt_sts, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				chip->bat_if_base + INT_RT_STS_REG, rc);
		return rc;
	}

	return (batt_pres_rt_sts & BATT_PRES_IRQ) ? 1 : 0;
}

static int qpnp_lbc_bat_if_configure_btc(struct qpnp_lbc_chip *chip)
{
	u8 btc_cfg = 0, mask = 0, rc;

	
	if (!chip->bat_if_base)
		return 0;

	if ((chip->cfg_hot_batt_p == HOT_THD_25_PCT)
			|| (chip->cfg_hot_batt_p == HOT_THD_35_PCT)) {
		btc_cfg |= btc_value[chip->cfg_hot_batt_p];
		mask |= BTC_HOT_MASK;
	}

	if ((chip->cfg_cold_batt_p == COLD_THD_70_PCT) ||
			(chip->cfg_cold_batt_p == COLD_THD_80_PCT)) {
		btc_cfg |= btc_value[chip->cfg_cold_batt_p];
		mask |= BTC_COLD_MASK;
	}


	
	if (chip->cfg_btc_disabled || flag_keep_charge_on || flag_pa_recharge) {
		
		mask |= BTC_COMP_EN_MASK;
	} else {
		mask |= BTC_COMP_EN_MASK;
		btc_cfg |= BTC_COMP_EN_MASK;
	}

	pr_debug("BTC configuration mask=%x\n", btc_cfg);

	rc = qpnp_lbc_masked_write(chip,
			chip->bat_if_base + BAT_IF_BTC_CTRL,
			mask, btc_cfg);
	if (rc)
		pr_err("Failed to configure BTC rc=%d\n", rc);

	return rc;
}

#define QPNP_LBC_VBATWEAK_MIN_UV        3000000
#define QPNP_LBC_VBATWEAK_MAX_UV        3581250
#define QPNP_LBC_VBATWEAK_STEP_UV       18750
static int qpnp_lbc_vbatweak_set(struct qpnp_lbc_chip *chip, int voltage)
{
	u8 reg_val;
	int rc;

	if (voltage < QPNP_LBC_VBATWEAK_MIN_UV ||
			voltage > QPNP_LBC_VBATWEAK_MAX_UV) {
		rc = -EINVAL;
	} else {
		reg_val = (voltage - QPNP_LBC_VBATWEAK_MIN_UV) /
					QPNP_LBC_VBATWEAK_STEP_UV;
		pr_debug("VBAT_WEAK=%d setting %02x\n",
				chip->cfg_batt_weak_voltage_uv, reg_val);
		rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_VBAT_WEAK_REG,
					&reg_val, 1);
		if (rc)
			pr_err("Failed to set VBAT_WEAK rc=%d\n", rc);
	}

	return rc;
}

#define QPNP_LBC_VBAT_MIN_MV		4000
#define QPNP_LBC_VBAT_MAX_MV		4775
#define QPNP_LBC_VBAT_STEP_MV		25
static int qpnp_lbc_vddsafe_set(struct qpnp_lbc_chip *chip, int voltage)
{
	u8 reg_val;
	int rc;

	if (voltage < QPNP_LBC_VBAT_MIN_MV
			|| voltage > QPNP_LBC_VBAT_MAX_MV) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}
	reg_val = (voltage - QPNP_LBC_VBAT_MIN_MV) / QPNP_LBC_VBAT_STEP_MV;
	pr_debug("voltage=%d setting %02x\n", voltage, reg_val);
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_VDD_SAFE_REG,
				&reg_val, 1);
	if (rc)
		pr_err("Failed to set VDD_SAFE rc=%d\n", rc);

	return rc;
}

static int qpnp_lbc_vddmax_set(struct qpnp_lbc_chip *chip, int voltage)
{
	u8 reg_val;
	int rc;

	if (voltage < QPNP_LBC_VBAT_MIN_MV
			|| voltage > QPNP_LBC_VBAT_MAX_MV) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}
	reg_val = (voltage - QPNP_LBC_VBAT_MIN_MV) / QPNP_LBC_VBAT_STEP_MV;
	pr_debug("voltage=%d setting %02x\n", voltage, reg_val);
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_VDD_MAX_REG,
				&reg_val, 1);
	if (rc)
		pr_err("Failed to set VDD_MAX rc=%d\n", rc);

	return rc;
}

static void qpnp_lbc_set_appropriate_vddmax(struct qpnp_lbc_chip *chip)
{
	int rc;

	if (chip->bat_is_cool)
		rc = qpnp_lbc_vddmax_set(chip, chip->cfg_cool_bat_mv);
	else if (chip->bat_is_warm)
		rc = qpnp_lbc_vddmax_set(chip, chip->cfg_warm_bat_mv);
	else
		rc = qpnp_lbc_vddmax_set(chip, chip->cfg_max_voltage_mv);
	if (rc)
		pr_err("Failed to set appropriate vddmax rc=%d\n", rc);
}

#define QPNP_LBC_VINMIN_MIN_MV		4200
#define QPNP_LBC_VINMIN_MAX_MV		5037
#define QPNP_LBC_VINMIN_STEP_MV		27
static int qpnp_lbc_vinmin_set(struct qpnp_lbc_chip *chip, int voltage)
{
	u8 reg_val;
	int rc;

	if ((voltage < QPNP_LBC_VINMIN_MIN_MV)
			|| (voltage > QPNP_LBC_VINMIN_MAX_MV)) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}

	reg_val = (voltage - QPNP_LBC_VINMIN_MIN_MV) / QPNP_LBC_VINMIN_STEP_MV;
	pr_debug("VIN_MIN=%d setting %02x\n", voltage, reg_val);
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_VIN_MIN_REG,
				&reg_val, 1);
	if (rc)
		pr_err("Failed to set VIN_MIN rc=%d\n", rc);

	return rc;
}

#define QPNP_LBC_IBATSAFE_MIN_MA	90
#define QPNP_LBC_IBATSAFE_MAX_MA	1440
#define QPNP_LBC_I_STEP_MA		90
static int qpnp_lbc_ibatsafe_set(struct qpnp_lbc_chip *chip, int safe_current)
{
	u8 reg_val;
	int rc;

	if (safe_current < QPNP_LBC_IBATSAFE_MIN_MA
			|| safe_current > QPNP_LBC_IBATSAFE_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", safe_current);
		return -EINVAL;
	}

	reg_val = (safe_current - QPNP_LBC_IBATSAFE_MIN_MA)
			/ QPNP_LBC_I_STEP_MA;
	pr_debug("Ibate_safe=%d setting %02x\n", safe_current, reg_val);

	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_IBAT_SAFE_REG,
				&reg_val, 1);
	if (rc)
		pr_err("Failed to set IBAT_SAFE rc=%d\n", rc);

	return rc;
}

#define QPNP_LBC_IBATMAX_MIN	90
#define QPNP_LBC_IBATMAX_MAX	1440
static int qpnp_lbc_ibatmax_set(struct qpnp_lbc_chip *chip, int chg_current)
{
	u8 reg_val;
	int rc;

	if (chg_current > QPNP_LBC_IBATMAX_MAX)
		pr_debug("bad mA=%d clamping current\n", chg_current);

	chg_current = clamp(chg_current, QPNP_LBC_IBATMAX_MIN,
						QPNP_LBC_IBATMAX_MAX);

	if (chg_current == chip->prev_max_ma)
			return 0;

	reg_val = (chg_current - QPNP_LBC_IBATMAX_MIN) / QPNP_LBC_I_STEP_MA;

	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_IBAT_MAX_REG,
				&reg_val, 1);
	if (rc)
		pr_err("Failed to set IBAT_MAX rc=%d\n", rc);
	else
		chip->prev_max_ma = chg_current;

	return rc;
}

#define QPNP_LBC_TCHG_MIN	4
#define QPNP_LBC_TCHG_MAX	512
#define QPNP_LBC_TCHG_STEP	4
static int qpnp_lbc_tchg_max_set(struct qpnp_lbc_chip *chip, int minutes)
{
	u8 reg_val = 0;
	int rc;

	if ((minutes > QPNP_LBC_TCHG_MAX) && (minutes < QPNP_LBC_TCHG_MAX*2))
		minutes = minutes / 2;

	minutes = clamp(minutes, QPNP_LBC_TCHG_MIN, QPNP_LBC_TCHG_MAX);

	
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_TCHG_MAX_EN_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to write tchg_max_en rc=%d\n", rc);
		return rc;
	}

	if (flag_keep_charge_on) {
		pr_info("Disable safety timer due to keep charge flag set.\n");
		return rc;
	}

	reg_val = minutes / QPNP_LBC_TCHG_STEP - 1;

	pr_debug("cfg_tchg_mins=%d, TCHG_MAX=%d mins setting %x\n", chip->cfg_tchg_mins, minutes, reg_val);
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_TCHG_MAX_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to write tchg_max_en rc=%d\n", rc);
		return rc;
	}

	
	reg_val = 0x80;
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_TCHG_MAX_EN_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to write tchg_max_en rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int qpnp_lbc_vbatdet_override(struct qpnp_lbc_chip *chip, int ovr_val)
{
	int rc;
	u8 reg_val;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags;

	spin_lock_irqsave(&chip->hw_access_lock, flags);

	rc = __qpnp_lbc_read(spmi, chip->chgr_base + CHG_COMP_OVR1,
				&reg_val, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
						chip->chgr_base, rc);
		goto out;
	}
	pr_debug("addr = 0x%x read 0x%x\n", chip->chgr_base, reg_val);

	reg_val &= ~CHG_VBAT_DET_OVR_MASK;
	reg_val |= ovr_val & CHG_VBAT_DET_OVR_MASK;

	pr_debug("writing to base=%x val=%x\n", chip->chgr_base, reg_val);

	rc = __qpnp_lbc_secure_write(spmi, chip->chgr_base, CHG_COMP_OVR1,
					&reg_val, 1);
	if (rc)
		pr_err("spmi write failed: addr=%03X, rc=%d\n",
						chip->chgr_base, rc);

out:
	spin_unlock_irqrestore(&chip->hw_access_lock, flags);
	return rc;
}

static int get_prop_battery_voltage_now(struct qpnp_lbc_chip *chip)
{
	int rc = 0;
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(chip->vadc_dev, VBAT_SNS, &results);
	if (rc) {
		pr_err("Unable to read vbat rc=%d\n", rc);
		return 0;
	}

	return results.physical;
}

static int get_prop_batt_present(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	rc = qpnp_lbc_read(chip, chip->bat_if_base + BAT_IF_PRES_STATUS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read battery status read failed rc=%d\n",
				rc);
		return 0;
	}

	return (reg_val & BATT_PRES_MASK) ? 1 : 0;
}

static int get_prop_batt_health(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	rc = qpnp_lbc_read(chip, chip->bat_if_base + BAT_IF_TEMP_STATUS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read battery health rc=%d\n", rc);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (BATT_TEMP_HOT_MASK & reg_val)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (!(BATT_TEMP_COLD_MASK & reg_val))
		return POWER_SUPPLY_HEALTH_COLD;
	if (chip->bat_is_cool)
		return POWER_SUPPLY_HEALTH_COOL;
	if (chip->bat_is_warm)
		return POWER_SUPPLY_HEALTH_WARM;

	return POWER_SUPPLY_HEALTH_GOOD;
}

static int get_prop_charge_type(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 reg_val;

	if (!get_prop_batt_present(chip))
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	rc = qpnp_lbc_read(chip, chip->chgr_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read interrupt sts %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (reg_val & FAST_CHG_ON_IRQ)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int get_prop_batt_status(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 reg_val;

	if (qpnp_lbc_is_usb_chg_plugged_in(chip) && chip->chg_done)
		return POWER_SUPPLY_STATUS_FULL;

	rc = qpnp_lbc_read(chip, chip->chgr_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read interrupt sts rc= %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (reg_val & FAST_CHG_ON_IRQ)
		return POWER_SUPPLY_STATUS_CHARGING;

	return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int get_prop_current_now(struct qpnp_lbc_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
			  POWER_SUPPLY_PROP_CURRENT_NOW, &ret);
		return ret.intval;
	} else {
		pr_debug("No BMS supply registered return 0\n");
	}

	return 0;
}

#define DEFAULT_CAPACITY	50
static int get_prop_capacity(struct qpnp_lbc_chip *chip)
{
	union power_supply_propval ret = {0,};
	int soc, battery_status, charger_in, rc;

	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;

	if (chip->cfg_use_fake_battery || !get_prop_batt_present(chip))
		return DEFAULT_CAPACITY;

	if (chip->bms_psy) {
		mutex_lock(&chip->chg_enable_lock);
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		battery_status = get_prop_batt_status(chip);
		charger_in = qpnp_lbc_is_usb_chg_plugged_in(chip);

		
		if (ret.intval < 100 && chip->chg_done) {
			chip->chg_done = false;
		}
		pr_debug("status:%d, charger_in:%d, charging_disabled:%d, resume_limit:%d, soc:%d\n",
					battery_status, charger_in, chip->cfg_charging_disabled,
					chip->cfg_soc_resume_limit, ret.intval);

		if (battery_status != POWER_SUPPLY_STATUS_CHARGING
				&& charger_in
				&& !chip->cfg_charging_disabled
				&& chip->cfg_soc_resume_limit
				&& ret.intval <= chip->cfg_soc_resume_limit) {
			pr_debug("resuming charging at %d%% soc\n",
					ret.intval);
			qpnp_lbc_vbatdet_override(chip, OVERRIDE_0);
			qpnp_lbc_charger_enable(chip, SOC, 1);
		}
		mutex_unlock(&chip->chg_enable_lock);

		soc = ret.intval;
		if (soc == 0) {
			if (!qpnp_lbc_is_usb_chg_plugged_in(chip))
				pr_warn_ratelimited("Batt 0, CHG absent\n");
		}
		return soc;
	} else {
		rc = pm8916_get_bms_capacity(&soc);
		if (!rc) {
			pr_debug("Call pm8916_get_bms_capacity return %d\n", soc);
			return soc;
		} else {
			pr_debug("No BMS supply registered return %d\n",
							DEFAULT_CAPACITY);
		}
	}

	return DEFAULT_CAPACITY;
}

#define DEFAULT_TEMP		250
static int get_prop_batt_temp(struct qpnp_lbc_chip *chip)
{
	int rc = 0;
	struct qpnp_vadc_result results;

	if (chip->cfg_use_fake_battery || !get_prop_batt_present(chip))
		return DEFAULT_TEMP;

	rc = qpnp_vadc_read(chip->vadc_dev, LR_MUX1_BATT_THERM, &results);
	if (rc) {
		pr_debug("Unable to read batt temperature rc=%d\n", rc);
		return DEFAULT_TEMP;
	}
	pr_debug("get_bat_temp %d, %lld\n", results.adc_code,
							results.physical);

	if ((results.physical >= 680) &&
			(flag_keep_charge_on || flag_pa_recharge))
			results.physical = 650;

	return (int)results.physical;
}

static void qpnp_lbc_set_appropriate_current(struct qpnp_lbc_chip *chip)
{
	unsigned int chg_current = chip->usb_psy_ma;

	if (chip->bat_is_cool && chip->cfg_cool_bat_chg_ma)
		chg_current = min(chg_current, chip->cfg_cool_bat_chg_ma);

	if (chip->bat_is_warm && chip->cfg_warm_bat_chg_ma)
		chg_current = min(chg_current, chip->cfg_warm_bat_chg_ma);

	if (chip->therm_lvl_sel != 0 && chip->thermal_mitigation)
		chg_current = min(chg_current,
			chip->thermal_mitigation[chip->therm_lvl_sel]);

	if (chg_limit_current != 0)
		chg_current = min(chg_current, chg_limit_current);

	pr_debug("setting charger current %d mA\n", chg_current);
	qpnp_lbc_ibatmax_set(chip, chg_current);
}

#if 0
static int qpnp_lbc_system_temp_level_set(struct qpnp_lbc_chip *chip,
								int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;
	unsigned long flags;

	if (!chip->thermal_mitigation) {
		pr_err("Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		pr_err("Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->cfg_thermal_levels) {
		pr_err("Unsupported level selected %d forcing %d\n", lvl_sel,
				chip->cfg_thermal_levels - 1);
		lvl_sel = chip->cfg_thermal_levels - 1;
	}

	if (lvl_sel == chip->therm_lvl_sel)
		return 0;

	spin_lock_irqsave(&chip->ibat_change_lock, flags);
	prev_therm_lvl = chip->therm_lvl_sel;
	chip->therm_lvl_sel = lvl_sel;
	if (chip->therm_lvl_sel == (chip->cfg_thermal_levels - 1)) {
		
		rc = qpnp_lbc_charger_enable(chip, THERMAL, 0);
		if (rc < 0)
			dev_err(chip->dev,
				"Failed to set disable charging rc %d\n", rc);
		goto out;
	}

	qpnp_lbc_set_appropriate_current(chip);

	if (prev_therm_lvl == chip->cfg_thermal_levels - 1) {
		rc = qpnp_lbc_charger_enable(chip, THERMAL, 1);
		if (rc < 0) {
			dev_err(chip->dev,
				"Failed to enable charging rc %d\n", rc);
		}
	}

out:
	spin_unlock_irqrestore(&chip->ibat_change_lock, flags);
	return rc;
}
#endif

#define MIN_COOL_TEMP		-300
#define MAX_WARM_TEMP		1000
#define HYSTERISIS_DECIDEGC	20

#if 0
static int qpnp_lbc_configure_jeita(struct qpnp_lbc_chip *chip,
			enum power_supply_property psp, int temp_degc)
{
	int rc = 0;

	if ((temp_degc < MIN_COOL_TEMP) || (temp_degc > MAX_WARM_TEMP)) {
		pr_err("Bad temperature request %d\n", temp_degc);
		return -EINVAL;
	}

	mutex_lock(&chip->jeita_configure_lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_COOL_TEMP:
		if (temp_degc >=
			(chip->cfg_warm_bat_decidegc - HYSTERISIS_DECIDEGC)) {
			pr_err("Can't set cool %d higher than warm %d - hysterisis %d\n",
					temp_degc,
					chip->cfg_warm_bat_decidegc,
					HYSTERISIS_DECIDEGC);
			rc = -EINVAL;
			goto mutex_unlock;
		}
		if (chip->bat_is_cool)
			chip->adc_param.high_temp =
				temp_degc + HYSTERISIS_DECIDEGC;
		else if (!chip->bat_is_warm)
			chip->adc_param.low_temp = temp_degc;

		chip->cfg_cool_bat_decidegc = temp_degc;
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		if (temp_degc <=
		(chip->cfg_cool_bat_decidegc + HYSTERISIS_DECIDEGC)) {
			pr_err("Can't set warm %d higher than cool %d + hysterisis %d\n",
					temp_degc,
					chip->cfg_warm_bat_decidegc,
					HYSTERISIS_DECIDEGC);
			rc = -EINVAL;
			goto mutex_unlock;
		}
		if (chip->bat_is_warm)
			chip->adc_param.low_temp =
				temp_degc - HYSTERISIS_DECIDEGC;
		else if (!chip->bat_is_cool)
			chip->adc_param.high_temp = temp_degc;

		chip->cfg_warm_bat_decidegc = temp_degc;
		break;
	default:
		rc = -EINVAL;
		goto mutex_unlock;
	}

	if (qpnp_adc_tm_channel_measure(chip->adc_tm_dev, &chip->adc_param))
		pr_err("request ADC error\n");

mutex_unlock:
	mutex_unlock(&chip->jeita_configure_lock);
	return rc;
}

static int qpnp_batt_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_COOL_TEMP:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
	case POWER_SUPPLY_PROP_WARM_TEMP:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		return 1;
	default:
		break;
	}

	return 0;
}
#endif

int pm8916_vm_bms_report_eoc(void)
{
	int rc = 0;

	if (!the_chip) {
			pr_err("called before init\n");
			return -EINVAL;
	}

	if (!the_chip->cfg_float_charge) {
		if (!flag_keep_charge_on) {
			
			rc = qpnp_lbc_vbatdet_override(the_chip, OVERRIDE_NONE);
			if (rc)
				pr_err("Failed to override VBAT_DET rc=%d\n",
						rc);
			
			rc = qpnp_lbc_charger_enable(the_chip, SOC, 0);
			if (rc)
				pr_err("Failed to disable charging rc=%d\n",
						rc);
			else
				the_chip->chg_done = true;
		}
		is_batt_full = true;
		is_batt_full_eoc_stop = true;
		htc_gauge_event_notify(HTC_GAUGE_EVENT_EOC_STOP_CHG);
		lbc_relax(&the_chip->lbc_chg_wake_source);
	}

	return rc;
}

static void qpnp_lbc_jeita_adc_notification(enum qpnp_tm_state state, void *ctx)
{
	struct qpnp_lbc_chip *chip = ctx;
	bool bat_warm = 0, bat_cool = 0;
	int temp;
	unsigned long flags;

	if (state >= ADC_TM_STATE_NUM) {
		pr_err("invalid notification %d\n", state);
		return;
	}

	temp = get_prop_batt_temp(chip);

	pr_debug("temp = %d state = %s\n", temp,
			state == ADC_TM_WARM_STATE ? "warm" : "cool");

	if (state == ADC_TM_WARM_STATE) {
		if (temp >= chip->cfg_warm_bat_decidegc) {
			
			bat_warm = true;
			bat_cool = false;
			chip->adc_param.low_temp =
					chip->cfg_warm_bat_decidegc
					- HYSTERISIS_DECIDEGC;
			chip->adc_param.state_request =
				ADC_TM_COOL_THR_ENABLE;
		} else if (temp >=
			chip->cfg_cool_bat_decidegc + HYSTERISIS_DECIDEGC) {
			
			bat_warm = false;
			bat_cool = false;

			chip->adc_param.low_temp =
					chip->cfg_cool_bat_decidegc;
			chip->adc_param.high_temp =
					chip->cfg_warm_bat_decidegc;
			chip->adc_param.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		}
	} else {
		if (temp <= chip->cfg_cool_bat_decidegc) {
			
			bat_warm = false;
			bat_cool = true;
			chip->adc_param.high_temp =
					chip->cfg_cool_bat_decidegc
					+ HYSTERISIS_DECIDEGC;
			chip->adc_param.state_request =
					ADC_TM_WARM_THR_ENABLE;
		} else if (temp <= (chip->cfg_warm_bat_decidegc -
					HYSTERISIS_DECIDEGC)){
			
			bat_warm = false;
			bat_cool = false;

			chip->adc_param.low_temp =
					chip->cfg_cool_bat_decidegc;
			chip->adc_param.high_temp =
					chip->cfg_warm_bat_decidegc;
			chip->adc_param.state_request =
					ADC_TM_HIGH_LOW_THR_ENABLE;
		}
	}

	if (chip->bat_is_cool ^ bat_cool || chip->bat_is_warm ^ bat_warm) {
		spin_lock_irqsave(&chip->ibat_change_lock, flags);
		chip->bat_is_cool = bat_cool;
		chip->bat_is_warm = bat_warm;
		qpnp_lbc_set_appropriate_vddmax(chip);
		qpnp_lbc_set_appropriate_current(chip);
		spin_unlock_irqrestore(&chip->ibat_change_lock, flags);
		htc_gauge_event_notify(HTC_GAUGE_EVENT_TEMP_ZONE_CHANGE);
	}

	pr_debug("warm %d, cool %d, low = %d deciDegC, high = %d deciDegC\n",
			chip->bat_is_warm, chip->bat_is_cool,
			chip->adc_param.low_temp, chip->adc_param.high_temp);

	if (qpnp_adc_tm_channel_measure(chip->adc_tm_dev, &chip->adc_param))
		pr_err("request ADC error\n");
}

static int get_chgr_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int rc;
	u8 chgr_sts;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	rc = qpnp_lbc_read(the_chip, the_chip->chgr_base + addr,
						&chgr_sts, 1);
	if (rc) {
		pr_err("failed to read chgr_base register sts %d\n", rc);
		return -EAGAIN;
	}
	pr_debug("addr:0x%X, val:0x%X\n", (the_chip->chgr_base + addr), chgr_sts);
	*val = chgr_sts;
	return 0;
}

static int get_bat_if_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int rc;
	u8 bat_if_sts;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	rc = qpnp_lbc_read(the_chip, the_chip->bat_if_base + addr,
						&bat_if_sts, 1);
	if (rc) {
		pr_err("failed to read bat_if_base register sts %d\n", rc);
		return -EAGAIN;
	}
	pr_debug("addr:0x%X, val:0x%X\n", (the_chip->bat_if_base + addr), bat_if_sts);
	*val = bat_if_sts;
	return 0;
}

static int get_usb_chgpth_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int rc;
	u8 chgpth_sts;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	rc = qpnp_lbc_read(the_chip, the_chip->usb_chgpth_base + addr,
						&chgpth_sts, 1);
	if (rc) {
		pr_err("failed to read chgpth_sts register sts %d\n", rc);
		return -EAGAIN;
	}
	pr_debug("addr:0x%X, val:0x%X\n", (the_chip->usb_chgpth_base + addr), chgpth_sts);
	*val = chgpth_sts;
	return 0;
}

static int get_misc_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int rc;
	u8 misc_sts;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	rc = qpnp_lbc_read(the_chip, the_chip->misc_base + addr,
						&misc_sts, 1);
	if (rc) {
		pr_err("failed to read misc_sts register sts %d\n", rc);
		return -EAGAIN;
	}
	pr_debug("addr:0x%X, val:0x%X\n", (the_chip->misc_base + addr), misc_sts);
	*val = misc_sts;
	return 0;
}

static int
qpnp_lbc_usb_suspend_enable(struct qpnp_lbc_chip *chip, int enable)
{
	return qpnp_lbc_masked_write(chip,
			chip->usb_chgpth_base + USB_SUSP_REG,
			USB_SUSPEND_BIT,
			enable ? USB_SUSPEND_BIT : 0);
}

static int
qpnp_lbc_iusbmax_get(struct qpnp_lbc_chip *chip)
{
	int rc, iusbmax_ma;
	u8 iusbmax;

	rc = qpnp_lbc_read(chip, chip->chgr_base + CHG_IBAT_MAX_REG,
						&iusbmax, 1);
	if (rc) {
		pr_err("failed to read IUSB_MAX rc=%d\n", rc);
		return 0;
	}

	iusbmax_ma = iusbmax * QPNP_LBC_I_STEP_MA + QPNP_LBC_IBATMAX_MIN;

	return iusbmax_ma;
}

#define	QPNP_LBC_VINMIN_HIGH_HIGH_VAL	0x3F
static int
qpnp_lbc_vinmin_get(struct qpnp_lbc_chip *chip)
{
	int rc, vin_min_mv;
	u8 vin_min;

	rc = qpnp_lbc_read(chip, chip->chgr_base + CHG_VIN_MIN_REG,
						&vin_min, 1);
	if (rc) {
		pr_err("failed to read VIN_MIN rc=%d\n", rc);
		return 0;
	}

	vin_min_mv = vin_min * QPNP_LBC_VINMIN_STEP_MV + QPNP_LBC_VINMIN_MIN_MV;
	pr_debug("vin_min = 0x%02x, ma = %d\n", vin_min, vin_min_mv);

	return vin_min_mv;
}

int pm8916_is_pwr_src_plugged_in(void)
{
	int usb_in;

	
	if (!the_chip) {
		pr_warn("called before init\n");
		return -EINVAL;
	}

	usb_in = qpnp_lbc_is_usb_chg_plugged_in(the_chip);
	pr_info("usb_in=%d\n", usb_in);
	if (usb_in)
		return TRUE;
	else
		return FALSE;
}

#define EOC_CHECK_PERIOD_MS	60000
#define VBAT_TOLERANCE_MV	70
#define CONSECUTIVE_COUNT	3
static void
qpnp_lbc_eoc_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qpnp_lbc_chip *chip = container_of(dwork,
					struct qpnp_lbc_chip, eoc_work);

	int ibat_ma, vbat_mv;
	static int eoc_count = 0;

	if (!qpnp_lbc_is_usb_chg_plugged_in(chip) ){
		pr_info("no chg connected, stopping\n");
		is_batt_full = false;
		is_batt_full_eoc_stop = false;
		goto stop_eoc;
	}

	if (is_batt_full_eoc_stop)
		goto stop_eoc;

	if (qpnp_lbc_is_fastchg_on(chip)) {
		ibat_ma = get_prop_current_now(chip)/1000;
		vbat_mv = get_prop_battery_voltage_now(chip)/1000;

		pr_info("current:%d, voltage:%d, term_current:%d, eoc_count:%d, max_voltage_mv:%d\n",
					ibat_ma, vbat_mv, chip->term_current, eoc_count, chip->cfg_max_voltage_mv);

		if (vbat_mv > (chip->cfg_max_voltage_mv - VBAT_TOLERANCE_MV)) {
			if ((ibat_ma < 0) && (ibat_ma * (-1) < chip->term_current)) {
				eoc_count++;
				if (eoc_count == CONSECUTIVE_COUNT && !is_batt_full) {
					is_batt_full = true;
					htc_gauge_event_notify(HTC_GAUGE_EVENT_EOC);
					goto stop_eoc;
				}
			}
		} else {
			eoc_count = 0;
		}
	} else {
		goto stop_eoc;
	}

	schedule_delayed_work(&chip->eoc_work,
		msecs_to_jiffies(EOC_CHECK_PERIOD_MS));
	return;

stop_eoc:
	eoc_count = 0;
	pm8916_bms_dump_all();

}

static void handle_usb_present_change(struct qpnp_lbc_chip *chip,
				int usb_present)
{
	int rc;
	u8 reg_val = CHG_FAILED_BIT;

	
	is_ac_safety_timeout = is_ac_safety_timeout_twice = false;
	if (!usb_present) {
		chip->chg_done = false;
		hsml_target_ma = 0;
		usb_target_ma = 0;
		is_batt_full = false;
		is_batt_full_eoc_stop = false;
		
		rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_FAILED_REG,
					&reg_val, 1);
		if (rc)
			pr_err("Failed to write chg_fail clear bit rc=%d\n", rc);

		qpnp_lbc_charger_enable(chip, CURRENT, 0);
		qpnp_lbc_ibatmax_set(chip, QPNP_CHG_I_MAX_MIN_90);
		lbc_relax(&chip->lbc_chg_wake_source);
	} else {
		if (chip->term_current) {
			schedule_delayed_work(&chip->eoc_work,
					msecs_to_jiffies(EOC_CHECK_PERIOD_MS));
		}
	}
}

static u32 htc_fake_charger_for_testing(enum htc_power_source_type src)
{
	
	enum htc_power_source_type new_src = HTC_PWR_SOURCE_TYPE_AC;

	if((src > HTC_PWR_SOURCE_TYPE_9VAC) || (src == HTC_PWR_SOURCE_TYPE_BATT))
		return src;

	pr_info("(%d -> %d)\n", src, new_src);
	return new_src;
}

int pm8916_set_pwrsrc_and_charger_enable(enum htc_power_source_type src,
		bool chg_enable, bool pwrsrc_enable)
{
	int current_ma = 0;
	int rc = 0;
	static int pre_pwr_src;
	unsigned long flags;


	pr_info("src=%d, pre_pwr_src=%d, chg_enable=%d, pwrsrc_enable=%d\n",
				src, pre_pwr_src, chg_enable, pwrsrc_enable);

	if (get_kernel_flag() & KERNEL_FLAG_ENABLE_FAST_CHARGE)
		src = htc_fake_charger_for_testing(src);

	pwr_src = src;

	switch (src) {
	case HTC_PWR_SOURCE_TYPE_BATT:
		current_ma = USB_MA_2;
		break;
	case HTC_PWR_SOURCE_TYPE_WIRELESS:
	case HTC_PWR_SOURCE_TYPE_DETECTING:
	case HTC_PWR_SOURCE_TYPE_UNKNOWN_USB:
	case HTC_PWR_SOURCE_TYPE_USB:
		current_ma = USB_MA_450;
		break;
	case HTC_PWR_SOURCE_TYPE_AC:
	case HTC_PWR_SOURCE_TYPE_9VAC:
	case HTC_PWR_SOURCE_TYPE_MHL_AC:
		current_ma = USB_MA_900;
		break;
	default:
		current_ma = USB_MA_2;
		break;
	}

	pre_pwr_src = src;

	if (!the_chip) {
		pr_warn("called before init\n");
		return -EINVAL;
	}

	pr_debug("qpnp_lbc_ibatmax_set :%dmA\n", current_ma);
	spin_lock_irqsave(&the_chip->ibat_change_lock, flags);
	if (!the_chip->bms_psy)
		the_chip->bms_psy = power_supply_get_by_name("bms");

	if (qpnp_lbc_is_usb_chg_plugged_in(the_chip)) {
		
		if (current_ma <= 2 && !the_chip->cfg_use_fake_battery
				&& get_prop_batt_present(the_chip)) {
			qpnp_lbc_charger_enable(the_chip, CURRENT, 0);
			the_chip->usb_psy_ma = QPNP_CHG_I_MAX_MIN_90;
			qpnp_lbc_set_appropriate_current(the_chip);
			lbc_relax(&the_chip->lbc_chg_wake_source);
		} else {
			lbc_stay_awake(&the_chip->lbc_chg_wake_source);
			the_chip->usb_psy_ma = current_ma;
			qpnp_lbc_set_appropriate_current(the_chip);

			if (!chg_enable || !pwrsrc_enable)
				qpnp_lbc_charger_enable(the_chip, CURRENT, 0);
			else
				qpnp_lbc_charger_enable(the_chip, CURRENT, 1);
		}
	}

	if (HTC_PWR_SOURCE_TYPE_BATT == src)
		handle_usb_present_change(the_chip, 0);
	else
		handle_usb_present_change(the_chip, 1);

	spin_unlock_irqrestore(&the_chip->ibat_change_lock, flags);
	return rc;
}

int pm8916_get_charging_source(int *result)
{
	*result = pwr_src;
	return 0;
}

int pm8916_get_charging_enabled(int *result)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (get_prop_batt_status(the_chip) == POWER_SUPPLY_STATUS_CHARGING)
		return pm8916_get_charging_source(result);
	else
		*result = HTC_PWR_SOURCE_TYPE_BATT;
	return 0;
}

int pm8916_charger_enable(bool enable)
{
	int rc = 0;

	if (!the_chip) {
		pr_err("called before init\n");
		rc = -EINVAL;
	} else {
		enable = !!enable;
		rc = qpnp_lbc_charger_enable(the_chip, CURRENT, enable);
	}

	return rc;
}

#ifdef CONFIG_DUTY_CYCLE_LIMIT
int pm8916_limit_charge_enable(int chg_limit_reason, int chg_limit_timer_sub_mask, int limit_charge_timer_ma)
{
	unsigned long flags;

	pr_info("chg_limit_reason=%d, chg_limit_timer_sub_mask=%d, limit_charge_timer_ma=%d\n",
			chg_limit_reason, chg_limit_timer_sub_mask, limit_charge_timer_ma);

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	
	if (limit_charge_timer_ma != 0 && !!(chg_limit_reason & chg_limit_timer_sub_mask))
		chg_limit_current = limit_charge_timer_ma;
	else {
		if (!!chg_limit_reason)
			chg_limit_current = PM8916_CHG_I_MIN_MA;
		else
			chg_limit_current = 0;
	}

	pr_info("%s:chg_limit_current = %d\n", __func__, chg_limit_current);
	spin_lock_irqsave(&the_chip->ibat_change_lock, flags);
	qpnp_lbc_set_appropriate_current(the_chip);
	spin_unlock_irqrestore(&the_chip->ibat_change_lock, flags);
	return 0;
}
#else
int pm8916_limit_charge_enable(bool enable)
{
	unsigned long flags;

	pr_info("limit_charge=%d\n", enable);
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (enable)
		chg_limit_current = PM8916_CHG_I_MIN_MA;
	else
		chg_limit_current = 0;

	spin_lock_irqsave(&the_chip->ibat_change_lock, flags);
	qpnp_lbc_set_appropriate_current(the_chip);
	spin_unlock_irqrestore(&the_chip->ibat_change_lock, flags);
	return 0;
}
#endif

int pm8916_set_chg_iusbmax(int val)
{
	if (!the_chip) {
		pr_err("%s: called before init\n", __func__);
		return -EINVAL;
	}

	
	return qpnp_lbc_ibatmax_set(the_chip, val);
}

int pm8916_set_chg_vin_min(int val)
{
	if (!the_chip) {
		pr_err("%s: called before init\n", __func__);
		return -EINVAL;
	}
	return qpnp_lbc_vinmin_set(the_chip, val);
}

#define USB_ABOVE_OV_BIT	BIT(6)
static int
get_prop_usb_valid_status(struct qpnp_lbc_chip *chip, int *ov, int *v, int *uv)
{
	int rc;
	u8 usbin_valid_rt_sts;

	rc = qpnp_lbc_read(chip, chip->usb_chgpth_base + USB_PTH_STS_REG,
						&usbin_valid_rt_sts, 1);

	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				chip->usb_chgpth_base + USB_PTH_STS_REG, rc);
		return rc;
	}

	if (usbin_valid_rt_sts & USB_VALID_BIT)
		*v = true;
	else if (usbin_valid_rt_sts & USB_ABOVE_OV_BIT)
		*ov = true;
	else
		*uv = true;
	pr_debug("chgr usb sts 0x%x\n", usbin_valid_rt_sts);
	return 0;
}

static void update_ovp_uvp_state(int ov, int v, int uv)
{
	if ( ov && !v && !uv) {
		if (!ovp) {
			ovp = 1;
			pr_info("OVP: 0 -> 1, USB_Valid: %d\n", v);
			htc_charger_event_notify(HTC_CHARGER_EVENT_OVP);
		}
		if (uvp) {
			uvp = 0;
			pr_debug("UVP: 1 -> 0, USB_Valid: %d\n", v);
		}
	} else if ( !ov && !v && uv) {
		if (ovp) {
			ovp = 0;
			pr_info("OVP: 1 -> 0, USB_Valid: %d\n", v);
			htc_charger_event_notify(HTC_CHARGER_EVENT_OVP_RESOLVE);
		}
		if (!uvp) {
			uvp = 1;
			pr_debug("UVP: 0 -> 1, USB_Valid: %d\n", v);
		}
	} else {
		if (ovp) {
			ovp = 0;
			pr_info("OVP: 1 -> 0, USB_Valid: %d\n", v);
			htc_charger_event_notify(HTC_CHARGER_EVENT_OVP_RESOLVE);
		}
		if (uvp) {
			uvp = 0;
			pr_debug("UVP: 1 -> 0, USB_Valid: %d\n", v);
		}
	}

	pr_debug("ovp=%d, uvp=%d [%d,%d,%d]\n", ovp, uvp, ov, v, uv);
}

int pm8916_is_charger_ovp(int* result)
{
	int ov = false, uv = false, v = false;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	get_prop_usb_valid_status(the_chip, &ov, &v, &uv);

	update_ovp_uvp_state(ov, v, uv);
	*result = ovp;
	return 0;
}

int pm8916_is_batt_temp_fault_disable_chg(int *result)
{
	int batt_temp_status, vbat_mv, is_vbatt_over_vddmax;
	int is_cold = 0, is_hot = 0;
	int is_warm = 0;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	batt_temp_status = get_prop_batt_health(the_chip);

	vbat_mv = get_prop_battery_voltage_now(the_chip) / 1000;

	if (batt_temp_status == POWER_SUPPLY_HEALTH_OVERHEAT)
		is_hot = 1;
	if (batt_temp_status == POWER_SUPPLY_HEALTH_COLD)
		is_cold = 1;

	is_warm = the_chip->bat_is_warm;

	if(vbat_mv >= the_chip->cfg_warm_bat_mv)
		is_vbatt_over_vddmax = true;
	else
		is_vbatt_over_vddmax = false;

	pr_debug("is_cold=%d, is_hot=%d, is_warm=%d, is_vbatt_over_vddmax=%d, warm_bat_mv:%d\n",
			is_cold, is_hot, is_warm, is_vbatt_over_vddmax, the_chip->cfg_warm_bat_mv);
	if ((is_cold || is_hot || (is_warm && is_vbatt_over_vddmax)) &&
			!flag_keep_charge_on && !flag_pa_recharge)
		*result = 1;
	else
		*result = 0;

	return 0;
}

int pm8916_is_chg_safety_timer_timeout(int *result)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	*result = is_ac_safety_timeout;
	return 0;
}

static int is_ac_online(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	return qpnp_lbc_is_usb_chg_plugged_in(the_chip) &&
				(pwr_src == HTC_PWR_SOURCE_TYPE_AC ||
							pwr_src == HTC_PWR_SOURCE_TYPE_9VAC ||
							pwr_src == HTC_PWR_SOURCE_TYPE_MHL_AC);
}
static void __pm8916_charger_vbus_draw(unsigned int mA)
{
	int rc;

	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}

	if (mA >= 0 && mA <= 2) {
		rc = qpnp_lbc_usb_suspend_enable(the_chip, TRUE);
		if (rc)
			pr_err("fail to set suspend bit rc=%d\n", rc);

		rc = qpnp_lbc_ibatmax_set(the_chip, QPNP_CHG_I_MAX_MIN_90);
		if (rc) {
			pr_err("unable to set iusb to %d rc = %d\n", 0, rc);
		}
	} else {
		rc = qpnp_lbc_usb_suspend_enable(the_chip, FALSE);
		if (rc)
			pr_err("fail to reset suspend bit rc=%d\n", rc);
		rc = qpnp_lbc_ibatmax_set(the_chip, mA);
		if (rc) {
			pr_err("unable to set iusb to %d rc = %d\n", 0, rc);
		}
	}
}

static DEFINE_SPINLOCK(vbus_lock);
static void _pm8916_charger_vbus_draw(unsigned int mA)
{
	unsigned long flags;

	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}

	if (usb_target_ma <= USB_MA_2 && mA > usb_wall_threshold_ma
			&& !hsml_target_ma) {
		usb_target_ma = mA;
	}

	spin_lock_irqsave(&vbus_lock, flags);

	if (mA > QPNP_LBC_IBATMAX_MAX) {
		__pm8916_charger_vbus_draw(QPNP_LBC_IBATMAX_MAX);
	} else {
		if (!hsml_target_ma && mA > usb_wall_threshold_ma)
			__pm8916_charger_vbus_draw(usb_wall_threshold_ma);
		else
			__pm8916_charger_vbus_draw(mA);
	}

	spin_unlock_irqrestore(&vbus_lock, flags);
}

int pm8916_set_hsml_target_ma(int target_ma)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	pr_info("target_ma= %d\n", target_ma);
	hsml_target_ma = target_ma;

	if((hsml_target_ma != 0) && (pwr_src == HTC_PWR_SOURCE_TYPE_USB)) {
			_pm8916_charger_vbus_draw(hsml_target_ma);
	}

	return 0;
}

int pm8916_is_batt_full(int *result)
{
	*result = is_batt_full;
	return 0;
}

int pm8916_is_batt_full_eoc_stop(int *result)
{
	*result = is_batt_full_eoc_stop;
	return 0;
}

int pm8916_get_charge_type(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return  get_prop_charge_type(the_chip);
}

int pm8916_get_chg_usb_iusbmax(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	
	return (qpnp_lbc_iusbmax_get(the_chip)*1000);
}

int pm8916_get_chg_vinmin(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return (qpnp_lbc_vinmin_get(the_chip)*1000);
}

int pm8916_get_input_voltage_regulation(void)
{
	
	return 0;
}

int pm8916_get_batt_voltage(int *result)
{
	if (!the_chip) {
		pr_warn("called before init\n");
		return -EINVAL;
	}

	*result = (get_prop_battery_voltage_now(the_chip) / 1000);
	return 0;
}

int pm8916_get_batt_current(int *result)
{
	if (!the_chip) {
		pr_warn("called before init\n");
		return -EINVAL;
	}

	*result = get_prop_current_now(the_chip);
	return 0;
}

int pm8916_get_batt_temperature(int *result)
{
	if (!the_chip) {
		pr_warn("called before init\n");
		return -EINVAL;
	}

	*result = get_prop_batt_temp(the_chip);
	return 0;
}

int pm8916_get_batt_soc(int *result)
{
	if (!the_chip) {
		pr_warn("called before init\n");
		return -EINVAL;
	}

	*result = get_prop_capacity(the_chip);
	return 0;
}

int pm8916_get_batt_cc(int *result)
{
	
	*result = 0;
	return 0;
}

int pm8916_get_batt_present(void)
{
	if (!the_chip) {
		pr_warn("called before init\n");
		return 1;
	}

	return get_prop_batt_present(the_chip);
}

int pm8916_get_batt_status(void)
{
	if (!the_chip) {
		pr_warn("called before init\n");
		return POWER_SUPPLY_STATUS_UNKNOWN;;
	}

	return get_prop_batt_status(the_chip);
}

int pm8916_get_usb_temperature(int *result)
{
	
	*result = 0;
	return 0;
}

int pm8916_is_batt_temperature_fault(int *result)
{
	int is_cold = 0, is_warm = 0;
	int batt_temp_status;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	batt_temp_status = get_prop_batt_health(the_chip);

	if (batt_temp_status == POWER_SUPPLY_HEALTH_COLD || the_chip->bat_is_cool)
		is_cold = 1;

	is_warm = the_chip->bat_is_warm;

	pr_debug("is_cold=%d,is_warm=%d\n", is_cold, is_warm);
	if (is_cold || is_warm)
		*result = 1;
	else
		*result = 0;
	return 0;
}

int pm8916_get_chgr_fsm_state(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 fsm, reg;

	
	rc = qpnp_lbc_masked_write(chip,
			chip->chgr_base + SEC_ACCESS,
			0xFF,
			0xA5);
	if (rc) {
		pr_err("failed to write SEC_ACCESS rc=%d\n", rc);
		return rc;
	}

	reg = 0x01;
	rc = qpnp_lbc_write(chip, chip->chgr_base + 0xE6, &reg, 1);
	if (rc)
		pr_err("failed to unsecure charger fsm rc=%d\n", rc);

	rc = qpnp_lbc_read(chip, chip->chgr_base + CHG_FSM_STATE_REG, &fsm, 1);
	if (rc) {
		pr_err("failed to read charger fsm state %d\n", rc);
		return rc;
	}

	return fsm;
}

int pm8916_charger_get_attr_text(char *buf, int size)
{
	int rc;
	struct qpnp_vadc_result result;
	int len = 0;
	u64 val = 0;
	unsigned long flags;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	local_irq_save(flags);
	len += scnprintf(buf + len, size - len,
			"CHG_DONE_IRQ: %d;\n"
			"CHG_FAIL_IRQ: %d;\n"
			"FAST_CHG_IRQ: %d;\n"
			"VBAT_DET_LO_IRQ: %d;\n"
			"BAT_TEMP_OK_IRQ: %d;\n"
			"BAT_PRES_IRQ: %d;\n"
			"CHG_GONE_IRQ: %d;\n"
			"USBIN_VALID_IRQ: %d;\n"
			"USB_OVER_TEMP_IRQ: %d;\n"
			"COARSE_DET_USB_IRQ: %d;\n",
			irq_read_line(the_chip->irqs[CHG_DONE].irq),
			irq_read_line(the_chip->irqs[CHG_FAILED].irq),
			irq_read_line(the_chip->irqs[CHG_FAST_CHG].irq),
			irq_read_line(the_chip->irqs[CHG_VBAT_DET_LO].irq),
			irq_read_line(the_chip->irqs[BATT_TEMPOK].irq),
			irq_read_line(the_chip->irqs[BATT_PRES].irq),
			irq_read_line(the_chip->irqs[USB_CHG_GONE].irq),
			irq_read_line(the_chip->irqs[USBIN_VALID].irq),
			irq_read_line(the_chip->irqs[USB_OVER_TEMP].irq),
			irq_read_line(the_chip->irqs[COARSE_DET_USB].irq));
	local_irq_restore(flags);

	rc = qpnp_vadc_read(the_chip->vadc_dev, USBIN, &result);
	if (rc) {
		pr_err("error reading USBIN channel = %d, rc = %d\n",
					USBIN, rc);
	}
	len += scnprintf(buf + len, size - len,
			"USBIN(uV): %d;\n", (int)result.physical);

	len += scnprintf(buf + len, size - len,
			"AC_SAFETY_TIMEOUT(bool): %d;\n", (int)is_ac_safety_timeout);

	len += scnprintf(buf + len, size - len,
			"AC_SAFETY_TIMEOUT2(bool): %d;\n", (int)is_ac_safety_timeout_twice);

	len += scnprintf(buf + len, size - len,
			"mitigation_level(int): %d;\n", the_chip->therm_lvl_sel);
	len += scnprintf(buf + len, size - len,
			"BATT_TEMP: %d;\n", get_prop_batt_temp(the_chip));

	
	get_chgr_reg((void *)CHG_VDD_MAX_REG, &val);
	len += scnprintf(buf + len, size - len, "VDD_MAX: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_VDD_SAFE_REG, &val);
	len += scnprintf(buf + len, size - len, "VDD_SAFE: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_IBAT_MAX_REG, &val);
	len += scnprintf(buf + len, size - len, "IBAT_MAX: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_IBAT_SAFE_REG, &val);
	len += scnprintf(buf + len, size - len, "IBAT_SAFE: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_VIN_MIN_REG, &val);
	len += scnprintf(buf + len, size - len, "VIN_MIN: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_CTRL_REG, &val);
	len += scnprintf(buf + len, size - len, "CHG_CTRL: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_IBATTERM_EN_REG, &val);
	len += scnprintf(buf + len, size - len, "IBAT_TERM_CHGR: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_TCHG_MAX_EN_REG, &val);
	len += scnprintf(buf + len, size - len, "TCHG_MAX_EN: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_TCHG_MAX_REG, &val);
	len += scnprintf(buf + len, size - len, "TCHG_MAX: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_CHG_WDOG_TIME_REG, &val);
	len += scnprintf(buf + len, size - len, "WDOG_TIME: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_WDOG_EN_REG, &val);
	len += scnprintf(buf + len, size - len, "WDOG_EN: 0x%llX;\n", val);
	len += scnprintf(buf + len, size - len, "FSM_STATE: %d;\n",
						pm8916_get_chgr_fsm_state(the_chip));
	get_chgr_reg((void *)CHG_STATUS_REG, &val);
	len += scnprintf(buf + len, size - len, "CHG_STATUS: 0x%llX;\n", val);
	get_chgr_reg((void *)CHG_INT_RT_STS, &val);
	len += scnprintf(buf + len, size - len, "CHG_INT_RT_STS: 0x%llX;\n", val);
	
	get_bat_if_reg((void *)BAT_IF_PRES_STATUS_REG, &val);
	len += scnprintf(buf + len, size - len, "PRES_STATUS: 0x%llX;\n", val);
	get_bat_if_reg((void *)BAT_IF_TEMP_STATUS_REG, &val);
	len += scnprintf(buf + len, size - len, "TEMP_STATUS: 0x%llX;\n", val);
	get_bat_if_reg((void *)BAT_IF_INT_RT_STS_REG, &val);
	len += scnprintf(buf + len, size - len, "IF_INT_RT_STS: 0x%llX;\n", val);
	get_bat_if_reg((void *)BAT_IF_BPD_CTRL_REG, &val);
	len += scnprintf(buf + len, size - len, "BPD_CTRL: 0x%llX;\n", val);
	get_bat_if_reg((void *)BAT_IF_BTC_CTRL, &val);
	len += scnprintf(buf + len, size - len, "BTC_CTRL: 0x%llX;\n", val);

	
	get_usb_chgpth_reg((void *)USB_PTH_STS_REG, &val);
	len += scnprintf(buf + len, size - len, "USB_CHG_PTH_STS: 0x%llX;\n", val);
	get_usb_chgpth_reg((void *)USB_INT_RT_STS, &val);
	len += scnprintf(buf + len, size - len, "USB_INT_RT_STS: 0x%llX;\n", val);
	get_usb_chgpth_reg((void *)USB_OVP_CTL_REG, &val);
	len += scnprintf(buf + len, size - len, "USB_OVP_CTL: 0x%llX;\n", val);
	get_usb_chgpth_reg((void *)USB_SUSP_REG, &val);
	len += scnprintf(buf + len, size - len, "USB_SUSP: 0x%llX;\n", val);
	get_usb_chgpth_reg((void *)USB_ENUM_TIMER_STOP_REG, &val);
	len += scnprintf(buf + len, size - len, "ENUM_TIMER_STOP: 0x%llX;\n", val);
	get_usb_chgpth_reg((void *)USB_ENUM_TIMER_REG, &val);
	len += scnprintf(buf + len, size - len, "ENUM_TIMER: 0x%llX;\n", val);

	
	get_misc_reg((void *)MISC_BOOT_DONE_REG, &val);
	len += scnprintf(buf + len, size - len, "BOOT_DONE: 0x%llX", val);

	return len;
}

#define BATT_LOG_BUF_LEN (1024)
static char batt_log_buf[BATT_LOG_BUF_LEN];
static void dump_reg(void)
{
	u64 val;
	unsigned int len =0;

	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}

	memset(batt_log_buf, 0, sizeof(BATT_LOG_BUF_LEN));
	
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "FSM_STATE: %d,",
						pm8916_get_chgr_fsm_state(the_chip));
	get_chgr_reg((void *)CHG_VDD_MAX_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "VDD_MAX=0x%llX,", val);
	get_chgr_reg((void *)CHG_VDD_SAFE_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "VDD_SAFE=0x%llX,", val);
	get_chgr_reg((void *)CHG_IBAT_MAX_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "IBAT_MAX=0x%llX,", val);
	get_chgr_reg((void *)CHG_IBAT_SAFE_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "IBAT_SAFE=0x%llX,", val);
	get_chgr_reg((void *)CHG_VIN_MIN_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "VIN_MIN=0x%llX,", val);
	get_chgr_reg((void *)CHG_CTRL_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "CHG_CTRL=0x%llX,", val);
	get_chgr_reg((void *)CHG_IBATTERM_EN_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "IBAT_TERM_CHGR=0x%llX,", val);
	get_chgr_reg((void *)CHG_TCHG_MAX_EN_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "TCHG_MAX_EN=0x%llX,", val);
	get_chgr_reg((void *)CHG_TCHG_MAX_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "TCHG_MAX=0x%llX,", val);
	get_chgr_reg((void *)CHG_CHG_WDOG_TIME_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "WDOG_TIME=0x%llX,", val);
	get_chgr_reg((void *)CHG_WDOG_EN_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "WDOG_EN=0x%llX,", val);
	get_chgr_reg((void *)CHG_STATUS_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "CHG_STATUS: 0x%llX, ", val);
	get_chgr_reg((void *)CHG_INT_RT_STS, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "CHG_INT_RT_STS: 0x%llX, ", val);

	
	get_bat_if_reg((void *)BAT_IF_PRES_STATUS_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "PRES_STATUS=0x%llX,", val);
	get_bat_if_reg((void *)BAT_IF_TEMP_STATUS_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "TEMP_STATUS=0x%llX,", val);
	get_bat_if_reg((void *)BAT_IF_BPD_CTRL_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "BPD_CTRL=0x%llX,", val);
	get_bat_if_reg((void *)BAT_IF_BTC_CTRL, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "BTC_CTRL=0x%llX,", val);
	get_bat_if_reg((void *)BAT_IF_INT_RT_STS_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "IF_INT_RT_STS: 0x%llX,", val);

	
	get_usb_chgpth_reg((void *)USB_PTH_STS_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "USB_CHG_PTH_STS=0x%llX,", val);
	get_usb_chgpth_reg((void *)USB_OVP_CTL_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "USB_OVP_CTL=0x%llX,", val);
	get_usb_chgpth_reg((void *)USB_SUSP_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "USB_SUSP=0x%llX,", val);
	get_usb_chgpth_reg((void *)USB_ENUM_TIMER_STOP_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "ENUM_TIMER_STOP=0x%llX,", val);
	get_usb_chgpth_reg((void *)USB_ENUM_TIMER_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "ENUM_TIMER=0x%llX,", val);
	get_usb_chgpth_reg((void *)USB_INT_RT_STS, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "USB_INT_RT_STS: 0x%llX,", val);

	
	get_misc_reg((void *)MISC_BOOT_DONE_REG, &val);
	len += scnprintf(batt_log_buf + len, BATT_LOG_BUF_LEN - len, "BOOT_DONE=0x%llX", val);

	if(BATT_LOG_BUF_LEN - len <= 1)
		pr_warn("batt log length maybe out of buffer range!!!");

	pr_info("%s\n", batt_log_buf);
}

static void dump_irq_rt_status(struct qpnp_lbc_chip *chip)
{
		unsigned long flags;

		local_irq_save(flags);
		pr_info("[CHGR_INT] %d%d%d%d [BATIF_INT] %d%d [USB_CHGPTH] %d%d%d [COARSE_DET_USB] %d\n",

		irq_read_line(chip->irqs[CHG_DONE].irq),
		irq_read_line(chip->irqs[CHG_FAILED].irq),
		irq_read_line(chip->irqs[CHG_FAST_CHG].irq),
		irq_read_line(chip->irqs[CHG_VBAT_DET_LO].irq),
		irq_read_line(chip->irqs[BATT_TEMPOK].irq),
		irq_read_line(chip->irqs[BATT_PRES].irq),
		irq_read_line(chip->irqs[USB_CHG_GONE].irq),
		irq_read_line(chip->irqs[USBIN_VALID].irq),
		irq_read_line(chip->irqs[USB_OVER_TEMP].irq),
		irq_read_line(chip->irqs[COARSE_DET_USB].irq));

		local_irq_restore(flags);
}

static void dump_charger_status(struct qpnp_lbc_chip *chip)
{

	int usbin = 0, vchg = 0, temp = 0, rc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(chip->vadc_dev, USBIN, &result);
	if (rc) {
		pr_err("error reading USBIN channel = %d, rc = %d\n",
					USBIN, rc);
	} else {
		usbin = (int)result.physical;
	}

	rc = qpnp_vadc_read(chip->vadc_dev, VCHG_SNS, &result);
	if (rc) {
		pr_err("error reading VCHG_SNS channel = %d, rc = %d\n",
					VCHG_SNS, rc);
	} else {
		vchg = (int)result.physical;
	}

	temp = get_prop_batt_temp(chip);

	pr_info("T=%d,USBIN=%d,VCHG=%d,ovp/uvp=%d/%d,safety_timeout=%d/%d,"
			"charger_disabled=0x%X,is_warm/is_cool=%d/%d,is_full=%d/%d,"
			"therm_lvl_sel=%d\n",
			temp, usbin, vchg, ovp, uvp, is_ac_safety_timeout,
			is_ac_safety_timeout_twice, chip->charger_disabled,
			chip->bat_is_warm, chip->bat_is_cool, is_batt_full,
			is_batt_full_eoc_stop, chip->therm_lvl_sel);

}

static void dump_all(int more)
{
	
	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}

	dump_charger_status(the_chip);
	dump_irq_rt_status(the_chip);
	dump_reg();
	pm8916_bms_dump_all();
}

inline int pm8916_dump_all(void)
{
	dump_all(0);
	return 0;
}

#define IBAT_TERM_EN_MASK		BIT(3)
static int qpnp_lbc_chg_init(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 reg_val;

	qpnp_lbc_vbatweak_set(chip, chip->cfg_batt_weak_voltage_uv);
	rc = qpnp_lbc_vinmin_set(chip, chip->cfg_min_voltage_mv);
	if (rc) {
		pr_err("Failed  to set  vin_min rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_vddsafe_set(chip, chip->cfg_safe_voltage_mv);
	if (rc) {
		pr_err("Failed to set vdd_safe rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_vddmax_set(chip, chip->cfg_max_voltage_mv);
	if (rc) {
		pr_err("Failed to set vdd_safe rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_ibatsafe_set(chip, chip->cfg_safe_current);
	if (rc) {
		pr_err("Failed to set ibat_safe rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_tchg_max_set(chip, chip->cfg_tchg_mins);
	if (rc) {
		pr_err("Failed to set tchg_mins rc=%d\n", rc);
		return rc;
	}

	if (!chip->cfg_charger_detect_eoc || chip->cfg_float_charge) {
		if (chip->cfg_float_charge)
			rc = qpnp_lbc_vbatdet_override(chip, OVERRIDE_0);
			if (rc) {
				pr_err("Failed to override comp rc=%d\n", rc);
				return rc;
			}
		rc = qpnp_lbc_masked_write(chip,
				chip->chgr_base + CHG_IBATTERM_EN_REG,
				IBAT_TERM_EN_MASK, 0);
		if (rc) {
			pr_err("Failed to disable EOC comp rc=%d\n", rc);
			return rc;
		}
	}

	
	reg_val = 0;
	rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_WDOG_EN_REG,
				&reg_val, 1);

	return rc;
}

static int qpnp_lbc_bat_if_init(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	
	switch (chip->cfg_bpd_detection) {
	case BPD_TYPE_BAT_THM:
		reg_val = BATT_THM_EN;
		break;
	case BPD_TYPE_BAT_ID:
		reg_val = BATT_ID_EN;
		break;
	case BPD_TYPE_BAT_THM_BAT_ID:
		reg_val = BATT_THM_EN | BATT_ID_EN;
		break;
	default:
		reg_val = BATT_THM_EN;
		break;
	}

	rc = qpnp_lbc_masked_write(chip,
			chip->bat_if_base + BAT_IF_BPD_CTRL_REG,
			BATT_BPD_CTRL_SEL_MASK, reg_val);
	if (rc) {
		pr_err("Failed to choose BPD rc=%d\n", rc);
		return rc;
	}

	
	reg_val = VREF_BATT_THERM_FORCE_ON;
	rc = qpnp_lbc_write(chip,
			chip->bat_if_base + BAT_IF_VREF_BAT_THM_CTRL_REG,
			&reg_val, 1);
	if (rc) {
		pr_err("Failed to force on VREF_BAT_THM rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int qpnp_lbc_usb_path_init(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 reg_val;

	if (qpnp_lbc_is_usb_chg_plugged_in(chip)) {
		reg_val = 0;
		rc = qpnp_lbc_write(chip,
			chip->usb_chgpth_base + CHG_USB_ENUM_T_STOP_REG,
			&reg_val, 1);
		if (rc) {
			pr_err("Failed to write enum stop rc=%d\n", rc);
			return -ENXIO;
		}
	}

	if (chip->cfg_charging_disabled) {
		rc = qpnp_lbc_charger_enable(chip, USER, 0);
		if (rc)
			pr_err("Failed to disable charging rc=%d\n", rc);
	} else {
		reg_val = CHG_ENABLE;
		rc = qpnp_lbc_masked_write(chip, chip->chgr_base + CHG_CTRL_REG,
					CHG_EN_MASK, reg_val);
		if (rc)
			pr_err("Failed to enable charger rc=%d\n", rc);
	}

	return rc;
}

static int qpnp_lbc_misc_init(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 reg_val;

	pr_debug("Setting BOOT_DONE\n");
	reg_val = MISC_BOOT_DONE;
	rc = qpnp_lbc_write(chip, chip->misc_base + MISC_BOOT_DONE_REG,
				&reg_val, 1);

	return rc;
}

#define OF_PROP_READ(chip, prop, qpnp_dt_property, retval, optional)	\
do {									\
	if (retval)							\
		break;							\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," qpnp_dt_property,	\
					&chip->prop);			\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		pr_err("Error reading " #qpnp_dt_property		\
				" property rc = %d\n", rc);		\
} while (0)

static int qpnp_charger_read_dt_props(struct qpnp_lbc_chip *chip)
{
	int rc = 0;
	const char *bpd;

	OF_PROP_READ(chip, cfg_max_voltage_mv, "vddmax-mv", rc, 0);
	OF_PROP_READ(chip, cfg_safe_voltage_mv, "vddsafe-mv", rc, 0);
	OF_PROP_READ(chip, cfg_min_voltage_mv, "vinmin-mv", rc, 0);
	OF_PROP_READ(chip, cfg_safe_current, "ibatsafe-ma", rc, 0);
	if (rc)
		pr_err("Error reading required property rc=%d\n", rc);

	OF_PROP_READ(chip, cfg_tchg_mins, "tchg-mins", rc, 1);
	OF_PROP_READ(chip, cfg_warm_bat_decidegc, "warm-bat-decidegc", rc, 1);
	OF_PROP_READ(chip, cfg_cool_bat_decidegc, "cool-bat-decidegc", rc, 1);
	OF_PROP_READ(chip, cfg_hot_batt_p, "batt-hot-percentage", rc, 1);
	OF_PROP_READ(chip, cfg_cold_batt_p, "batt-cold-percentage", rc, 1);
	OF_PROP_READ(chip, cfg_batt_weak_voltage_uv, "vbatweak-uv", rc, 1);
	OF_PROP_READ(chip, cfg_soc_resume_limit, "resume-soc", rc, 1);
	OF_PROP_READ(chip, term_current, "ibatterm-ma", rc, true);

	chip->mbat_in_gpio = of_get_named_gpio(chip->spmi->dev.of_node, "htc,mbat-in-gpio", 0);

	if (rc) {
		pr_err("Error reading optional property rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_string(chip->spmi->dev.of_node,
						"qcom,bpd-detection", &bpd);
	if (rc) {

		chip->cfg_bpd_detection = BPD_TYPE_BAT_THM;
		rc = 0;
	} else {
		chip->cfg_bpd_detection = get_bpd(bpd);
		if (chip->cfg_bpd_detection < 0) {
			pr_err("Failed to determine bpd schema rc=%d\n", rc);
			return -EINVAL;
		}
	}

	if (chip->cfg_cool_bat_decidegc || chip->cfg_warm_bat_decidegc) {
		chip->adc_tm_dev = qpnp_get_adc_tm(chip->dev, "chg");
		if (IS_ERR(chip->adc_tm_dev)) {
			rc = PTR_ERR(chip->adc_tm_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("Failed to get adc-tm rc=%d\n", rc);
			return rc;
		}

		OF_PROP_READ(chip, cfg_warm_bat_chg_ma, "ibatmax-warm-ma",
				rc, 1);
		OF_PROP_READ(chip, cfg_cool_bat_chg_ma, "ibatmax-cool-ma",
				rc, 1);
		OF_PROP_READ(chip, cfg_warm_bat_mv, "warm-bat-mv", rc, 1);
		OF_PROP_READ(chip, cfg_cool_bat_mv, "cool-bat-mv", rc, 1);
		if (rc) {
			pr_err("Error reading battery temp prop rc=%d\n", rc);
			return rc;
		}
	}

	
	chip->cfg_btc_disabled = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,btc-disabled");

	
	chip->cfg_charging_disabled =
		of_property_read_bool(chip->spmi->dev.of_node,
					"qcom,charging-disabled");

	
	chip->cfg_use_fake_battery =
			of_property_read_bool(chip->spmi->dev.of_node,
					"qcom,use-default-batt-values");

	
	chip->cfg_disable_follow_on_reset =
			of_property_read_bool(chip->spmi->dev.of_node,
					"qcom,disable-follow-on-reset");

	
	chip->cfg_float_charge =
			of_property_read_bool(chip->spmi->dev.of_node,
					"qcom,float-charge");

	
	chip->cfg_charger_detect_eoc =
			of_property_read_bool(chip->spmi->dev.of_node,
					"qcom,charger-detect-eoc");

	
	if (chip->cfg_use_fake_battery)
		chip->cfg_charging_disabled = true;

	chip->cfg_use_external_charger = of_property_read_bool(
			chip->spmi->dev.of_node, "qcom,use-external-charger");

	if (of_find_property(chip->spmi->dev.of_node,
					"qcom,thermal-mitigation",
					&chip->cfg_thermal_levels)) {
		chip->thermal_mitigation = devm_kzalloc(chip->dev,
			chip->cfg_thermal_levels,
			GFP_KERNEL);

		if (chip->thermal_mitigation == NULL) {
			pr_err("thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->cfg_thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(chip->spmi->dev.of_node,
				"qcom,thermal-mitigation",
				chip->thermal_mitigation,
				chip->cfg_thermal_levels);
		if (rc) {
			pr_err("Failed to read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static irqreturn_t qpnp_lbc_coarse_det_usb_irq_handler(int irq, void *_chip)
{
	int ov = false, uv = false, v = false;
	struct qpnp_lbc_chip *chip = _chip;

	pr_info("[irq]coarse_det_usb triggered: %d\n",
				irq_read_line(chip->irqs[COARSE_DET_USB].irq));

	if (!the_chip) {
		pr_err("called before init\n");
		return IRQ_HANDLED;
	}

	get_prop_usb_valid_status(the_chip, &ov, &v, &uv);
	update_ovp_uvp_state(ov, v, uv);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_lbc_usbin_valid_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	int usb_present, usbin_irq;

	usb_present = qpnp_lbc_is_usb_chg_plugged_in(chip);
	usbin_irq = irq_read_line(chip->irqs[USBIN_VALID].irq);

	pr_info("usbin-valid triggered: %d\n", usb_present);

	if (chip->usb_present ^ usb_present) {
		chip->usb_present = usb_present;
#if 0 
		if (!usb_present) {
			qpnp_lbc_charger_enable(chip, CURRENT, 0);
			qpnp_lbc_ibatmax_set(chip, QPNP_CHG_I_MAX_MIN_90);
		} else {
			qpnp_lbc_charger_enable(chip, CURRENT, 1);
		}
#endif
		cable_detection_vbus_irq_handler();
	} else {
		qpnp_lbc_coarse_det_usb_irq_handler(usbin_irq, chip);
	}

	return IRQ_HANDLED;
}

static int qpnp_lbc_is_batt_temp_ok(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	rc = qpnp_lbc_read(chip, chip->bat_if_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				chip->bat_if_base + INT_RT_STS_REG, rc);
		return rc;
	}

	return (reg_val & BAT_TEMP_OK_IRQ) ? 1 : 0;
}

static irqreturn_t qpnp_lbc_batt_temp_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	int batt_temp_good;

	batt_temp_good = qpnp_lbc_is_batt_temp_ok(chip);
	pr_info("batt-temp triggered: %d\n", batt_temp_good);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_lbc_batt_pres_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	int batt_present;

	batt_present = qpnp_lbc_is_batt_present(chip);
	pr_info("batt-pres triggered: %d\n", batt_present);

	if (chip->batt_present ^ batt_present) {
		chip->batt_present = batt_present;

		if ((chip->cfg_cool_bat_decidegc
					|| chip->cfg_warm_bat_decidegc)
					&& batt_present) {
			pr_debug("enabling vadc notifications\n");
			if (qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						&chip->adc_param))
				pr_err("request ADC error\n");
		} else if ((chip->cfg_cool_bat_decidegc
					|| chip->cfg_warm_bat_decidegc)
					&& !batt_present) {
			qpnp_adc_tm_disable_chan_meas(chip->adc_tm_dev,
					&chip->adc_param);
			pr_debug("disabling vadc notifications\n");
		}
	}

	if (chip->is_embeded_batt) {
		pr_info("Skip it due to embeded battery\n");
		return IRQ_HANDLED;
	}

	if (chip->mbat_in_gpio && gpio_is_valid(chip->mbat_in_gpio)
			&& (gpio_get_value(chip->mbat_in_gpio) == 0)) {
		pr_info("Battery is still existed\n");
		return IRQ_HANDLED;
	}

	pr_info("Battery is not existed CHG_FSM=%d", pm8916_get_chgr_fsm_state(chip));

	htc_gauge_event_notify(HTC_GAUGE_EVENT_BATT_REMOVED);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_lbc_chg_failed_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	int rc;
	u8 reg_val = CHG_FAILED_BIT;

	pr_info("chg_failed triggered count=%u\n", ++chip->chg_failed_count);

	if (!is_ac_online() || flag_keep_charge_on || flag_pa_recharge
		|| (get_kernel_flag() & KERNEL_FLAG_ENABLE_FAST_CHARGE)) {
		pr_debug("write CHG_FAILED_CLEAR bit\n");
		rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_FAILED_REG,
				&reg_val, 1);
		if (rc)
			pr_err("Failed to write chg_fail clear bit rc=%d\n", rc);
	} else {
		if ((chip->cfg_tchg_mins > QPNP_LBC_TCHG_MAX) &&
				!is_ac_safety_timeout_twice) {
			is_ac_safety_timeout_twice = true;
			pr_debug("write CHG_FAILED_CLEAR bit "
					"due to safety time is twice\n");
			rc = qpnp_lbc_write(chip, chip->chgr_base + CHG_FAILED_REG,
						&reg_val, 1);
			if (rc)
				pr_err("Failed to write chg_fail clear bit rc=%d\n", rc);

		} else {
			is_ac_safety_timeout = true;
			pr_err("batt_present=%d, batt_temp_ok=%d\n",
					get_prop_batt_present(chip),
					qpnp_lbc_is_batt_temp_ok(chip));
			htc_charger_event_notify(HTC_CHARGER_EVENT_SAFETY_TIMEOUT);
		}
	}

	return IRQ_HANDLED;
}

static int qpnp_lbc_is_fastchg_on(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	rc = qpnp_lbc_read(chip, chip->chgr_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read interrupt status rc=%d\n", rc);
		return rc;
	}
	pr_debug("charger status %x\n", reg_val);
	return (reg_val & FAST_CHG_ON_IRQ) ? 1 : 0;
}

static irqreturn_t qpnp_lbc_fastchg_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	bool fastchg_on = false;

	fastchg_on = qpnp_lbc_is_fastchg_on(chip);

	pr_info("FAST_CHG IRQ triggered, fastchg_on: %d\n", fastchg_on);

	if (chip->fastchg_on ^ fastchg_on) {
		chip->fastchg_on = fastchg_on;

		if (fastchg_on)
			chip->chg_done = false;
	}

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_lbc_chg_done_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;

	pr_info("charging done triggered\n");

	chip->chg_done = true;

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_lbc_vbatdet_lo_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	int rc;

	pr_info("vbatdet-lo triggered\n");

	is_batt_full_eoc_stop = false;
	rc = qpnp_lbc_charger_enable(chip, SOC, 1);
	if (rc)
		pr_err("Failed to enable charging\n");

	return IRQ_HANDLED;
}

static int qpnp_lbc_is_overtemp(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	rc = qpnp_lbc_read(chip, chip->usb_chgpth_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read interrupt status rc=%d\n", rc);
		return rc;
	}

	pr_debug("OVERTEMP rt status %x\n", reg_val);
	return (reg_val & OVERTEMP_ON_IRQ) ? 1 : 0;
}

static irqreturn_t qpnp_lbc_usb_overtemp_irq_handler(int irq, void *_chip)
{
	struct qpnp_lbc_chip *chip = _chip;
	int overtemp = qpnp_lbc_is_overtemp(chip);

	pr_warn_ratelimited("charger %s temperature limit !!!\n",
					overtemp ? "exceeds" : "within");

	return IRQ_HANDLED;
}

static int qpnp_disable_lbc_charger(struct qpnp_lbc_chip *chip)
{
	int rc;
	u8 reg;

	reg = CHG_FORCE_BATT_ON;
	rc = qpnp_lbc_masked_write(chip, chip->chgr_base + CHG_CTRL_REG,
							CHG_EN_MASK, reg);
	
	rc |= qpnp_lbc_masked_write(chip, chip->bat_if_base + BAT_IF_BTC_CTRL,
							BTC_COMP_EN_MASK, 0);
	
	reg = BATT_ID_EN | BATT_BPD_OFFMODE_EN;
	rc |= qpnp_lbc_write(chip, chip->bat_if_base + BAT_IF_BPD_CTRL_REG,
								&reg, 1);
	return rc;
}

#define SPMI_REQUEST_IRQ(chip, idx, rc, irq_name, threaded, flags, wake)\
do {									\
	if (rc)								\
		break;							\
	if (chip->irqs[idx].irq) {					\
		if (threaded)						\
			rc = devm_request_threaded_irq(chip->dev,	\
				chip->irqs[idx].irq, NULL,		\
				qpnp_lbc_##irq_name##_irq_handler,	\
				flags, #irq_name, chip);		\
		else							\
			rc = devm_request_irq(chip->dev,		\
				chip->irqs[idx].irq,			\
				qpnp_lbc_##irq_name##_irq_handler,	\
				flags, #irq_name, chip);		\
		if (rc < 0) {						\
			pr_err("Unable to request " #irq_name " %d\n",	\
								rc);	\
		} else {						\
			rc = 0;						\
			if (wake)					\
				enable_irq_wake(chip->irqs[idx].irq);	\
		}							\
	}								\
} while (0)

#define SPMI_GET_IRQ_RESOURCE(chip, rc, resource, idx, name)		\
do {									\
	if (rc)								\
		break;							\
									\
	rc = spmi_get_irq_byname(chip->spmi, resource, #name);		\
	if (rc < 0) {							\
		pr_err("Unable to get irq resource " #name "%d\n", rc);	\
	} else {							\
		chip->irqs[idx].irq = rc;				\
		rc = 0;							\
	}								\
} while (0)

static int qpnp_lbc_request_irqs(struct qpnp_lbc_chip *chip)
{
	int rc = 0;

	SPMI_REQUEST_IRQ(chip, CHG_FAILED, rc, chg_failed, 0,
			IRQF_TRIGGER_RISING, 1);

	SPMI_REQUEST_IRQ(chip, CHG_FAST_CHG, rc, fastchg, 0,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 0);

	SPMI_REQUEST_IRQ(chip, CHG_DONE, rc, chg_done, 0,
			IRQF_TRIGGER_RISING, 0);
	if (!test_power_monitor && !board_ftm_mode()
			&& board_mfg_mode() != MFG_MODE_POWER_TEST) {
		SPMI_REQUEST_IRQ(chip, CHG_VBAT_DET_LO, rc, vbatdet_lo, 0,
				IRQF_TRIGGER_FALLING, 1);

		SPMI_REQUEST_IRQ(chip, BATT_PRES, rc, batt_pres, 1,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
				| IRQF_ONESHOT, 1);

		SPMI_REQUEST_IRQ(chip, BATT_TEMPOK, rc, batt_temp, 0,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 1);
	}
	SPMI_REQUEST_IRQ(chip, USBIN_VALID, rc, usbin_valid, 0,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 1);

	SPMI_REQUEST_IRQ(chip, USB_OVER_TEMP, rc, usb_overtemp, 0,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 0);

	SPMI_REQUEST_IRQ(chip, COARSE_DET_USB, rc, coarse_det_usb, 0,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 0);

	return 0;
}

static int qpnp_lbc_get_irqs(struct qpnp_lbc_chip *chip, u8 subtype,
					struct spmi_resource *spmi_resource)
{
	int rc = 0;

	switch (subtype) {
	case LBC_CHGR_SUBTYPE:
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						CHG_FAST_CHG, fast-chg-on);
		if (!test_power_monitor && !board_ftm_mode()
				&& board_mfg_mode() != MFG_MODE_POWER_TEST) {
			SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
							CHG_VBAT_DET_LO, vbat-det-lo);
		}
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						CHG_FAILED, chg-failed);
		if (chip->cfg_charger_detect_eoc)
			SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						CHG_DONE, chg-done);
		break;
	case LBC_BAT_IF_SUBTYPE:
		if (!test_power_monitor && !board_ftm_mode()
				&& board_mfg_mode() != MFG_MODE_POWER_TEST) {
			SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
							BATT_PRES, batt-pres);

			SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
							BATT_TEMPOK, bat-temp-ok);
		}
		break;
	case LBC_USB_PTH_SUBTYPE:
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						USBIN_VALID, usbin-valid);
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						USB_OVER_TEMP, usb-over-temp);
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						COARSE_DET_USB, coarse-det-usb);
		break;
	};

	return 0;
}

static void determine_initial_status(struct qpnp_lbc_chip *chip)
{
	u8 reg_val;
	int rc;

	chip->usb_present = qpnp_lbc_is_usb_chg_plugged_in(chip);
	pr_info("initial usb_present status:%d\n", chip->usb_present);

	 if (chip->cfg_disable_follow_on_reset) {
		reg_val = 0x0;
		rc = __qpnp_lbc_secure_write(chip->spmi, chip->chgr_base,
			CHG_PERPH_RESET_CTRL3_REG, &reg_val, 1);
		if (rc)
			pr_err("Failed to configure PERPH_CTRL3 rc=%d\n", rc);
		else
			pr_warn("Charger is not following PMIC reset\n");
	}
}

int htc_battery_is_support_qc20(void)
{
	
	return 0;
}
EXPORT_SYMBOL(htc_battery_is_support_qc20);

int htc_battery_check_cable_type_from_usb(void)
{
	
	return 0;
}
EXPORT_SYMBOL(htc_battery_check_cable_type_from_usb);

static int qpnp_lbc_probe(struct spmi_device *spmi)
{
	u8 subtype;
	struct qpnp_lbc_chip *chip;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	int rc = 0;

	chip = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_lbc_chip),
				GFP_KERNEL);
	if (!chip) {
		pr_err("memory allocation failed.\n");
		return -ENOMEM;
	}

	chip->dev = &spmi->dev;
	chip->spmi = spmi;
	chip->fake_battery_soc = -EINVAL;
	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);
	mutex_init(&chip->jeita_configure_lock);
	mutex_init(&chip->chg_enable_lock);
	spin_lock_init(&chip->hw_access_lock);
	spin_lock_init(&chip->ibat_change_lock);

	
	rc = qpnp_charger_read_dt_props(chip);
	if (rc) {
		pr_err("Failed to read DT properties rc=%d\n", rc);
		return rc;
	}

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("spmi resource absent\n");
			rc = -ENXIO;
			goto fail_chg_enable;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
							IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
						spmi->dev.of_node->full_name);
			rc = -ENXIO;
			goto fail_chg_enable;
		}

		rc = qpnp_lbc_read(chip, resource->start + PERP_SUBTYPE_REG,
					&subtype, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			goto fail_chg_enable;
		}

		switch (subtype) {
		case LBC_CHGR_SUBTYPE:
			chip->chgr_base = resource->start;

			
			rc = qpnp_lbc_get_irqs(chip, subtype, spmi_resource);
			if (rc) {
				pr_err("Failed to get CHGR irqs rc=%d\n", rc);
				goto fail_chg_enable;
			}
			break;
		case LBC_USB_PTH_SUBTYPE:
			chip->usb_chgpth_base = resource->start;
			rc = qpnp_lbc_get_irqs(chip, subtype, spmi_resource);
			if (rc) {
				pr_err("Failed to get USB_PTH irqs rc=%d\n",
						rc);
				goto fail_chg_enable;
			}
			break;
		case LBC_BAT_IF_SUBTYPE:
			chip->bat_if_base = resource->start;
			chip->vadc_dev = qpnp_get_vadc(chip->dev, "chg");
			if (IS_ERR(chip->vadc_dev)) {
				rc = PTR_ERR(chip->vadc_dev);
				if (rc != -EPROBE_DEFER)
					pr_err("vadc prop missing rc=%d\n",
							rc);
				goto fail_chg_enable;
			}
			
			rc = qpnp_lbc_get_irqs(chip, subtype, spmi_resource);
			if (rc) {
				pr_err("Failed to get BAT_IF irqs rc=%d\n", rc);
				goto fail_chg_enable;
			}
			break;
		case LBC_MISC_SUBTYPE:
			chip->misc_base = resource->start;
			break;
		default:
			pr_err("Invalid peripheral subtype=0x%x\n", subtype);
			rc = -EINVAL;
		}
	}

	if (chip->cfg_use_external_charger) {
		pr_warn("Disabling Linear Charger (e-external-charger = 1)\n");
		rc = qpnp_disable_lbc_charger(chip);
		if (rc)
			pr_err("Unable to disable charger rc=%d\n", rc);
		return -ENODEV;
	}

	
	rc = qpnp_lbc_chg_init(chip);
	if (rc) {
		pr_err("unable to initialize LBC charger rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_bat_if_init(chip);
	if (rc) {
		pr_err("unable to initialize LBC BAT_IF rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_usb_path_init(chip);
	if (rc) {
		pr_err("unable to initialize LBC USB path rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_lbc_misc_init(chip);
	if (rc) {
		pr_err("unable to initialize LBC MISC rc=%d\n", rc);
		return rc;
	}

	usb_wall_threshold_ma = USB_MA_1100;

	if (!test_power_monitor && !board_ftm_mode()
			&& board_mfg_mode() != MFG_MODE_POWER_TEST) {
		if ((chip->cfg_cool_bat_decidegc || chip->cfg_warm_bat_decidegc)
				&& chip->bat_if_base) {
			chip->adc_param.low_temp = chip->cfg_cool_bat_decidegc;
			chip->adc_param.high_temp = chip->cfg_warm_bat_decidegc;
			chip->adc_param.timer_interval = ADC_MEAS1_INTERVAL_1S;
			chip->adc_param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
			chip->adc_param.btm_ctx = chip;
			chip->adc_param.threshold_notification =
				qpnp_lbc_jeita_adc_notification;
			chip->adc_param.channel = LR_MUX1_BATT_THERM;

			if (get_prop_batt_present(chip)) {
				rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						&chip->adc_param);
				if (rc) {
					pr_err("request ADC error rc=%d\n", rc);
				}
			}
		}

		rc = qpnp_lbc_bat_if_configure_btc(chip);
		if (rc) {
			pr_err("Failed to configure btc rc=%d\n", rc);
		}
	}

	
	determine_initial_status(chip);

	if (chip->cfg_charging_disabled && !get_prop_batt_present(chip))
		pr_info("Battery absent and charging disabled !!!\n");

	wakeup_source_init(&chip->lbc_chg_wake_source.source, "lbc_chg_wake");
	chip->lbc_chg_wake_source.disabled = 1;

	INIT_DELAYED_WORK(&chip->eoc_work, qpnp_lbc_eoc_work);

	the_chip = chip;

	rc = qpnp_lbc_request_irqs(chip);
	if (rc) {
		pr_err("unable to initialize LBC MISC rc=%d\n", rc);
	}

	
	cable_detection_vbus_irq_handler();

	htc_charger_event_notify(HTC_CHARGER_EVENT_READY);

	pr_info("Probe chg_dis=%d bpd=%d usb=%d batt_pres=%d batt_volt=%d soc=%d\n",
			chip->cfg_charging_disabled,
			chip->cfg_bpd_detection,
			qpnp_lbc_is_usb_chg_plugged_in(chip),
			get_prop_batt_present(chip),
			get_prop_battery_voltage_now(chip),
			get_prop_capacity(chip));

	return 0;

fail_chg_enable:
	dev_set_drvdata(&spmi->dev, NULL);
	return rc;
}

static int qpnp_lbc_remove(struct spmi_device *spmi)
{
	struct qpnp_lbc_chip *chip = dev_get_drvdata(&spmi->dev);

	cancel_delayed_work_sync(&chip->eoc_work);
	mutex_destroy(&chip->jeita_configure_lock);
	mutex_destroy(&chip->chg_enable_lock);
	dev_set_drvdata(&spmi->dev, NULL);
	return 0;
}


static int qpnp_lbc_resume(struct device *dev)
{
	struct qpnp_lbc_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	if (chip->bat_if_base) {
		rc = qpnp_lbc_masked_write(chip,
			chip->bat_if_base + BAT_IF_VREF_BAT_THM_CTRL_REG,
			VREF_BATT_THERM_FORCE_ON, VREF_BATT_THERM_FORCE_ON);
		if (rc)
			pr_err("Failed to force on VREF_BAT_THM rc=%d\n", rc);
	}

	return rc;
}

static int qpnp_lbc_suspend(struct device *dev)
{
	struct qpnp_lbc_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	if (chip->bat_if_base) {
		rc = qpnp_lbc_masked_write(chip,
			chip->bat_if_base + BAT_IF_VREF_BAT_THM_CTRL_REG,
			VREF_BATT_THERM_FORCE_ON, VREF_BAT_THM_ENABLED_FSM);
		if (rc)
			pr_err("Failed to set FSM VREF_BAT_THM rc=%d\n", rc);
	}

	return rc;
}

static const struct dev_pm_ops qpnp_lbc_pm_ops = {
	.resume		= qpnp_lbc_resume,
	.suspend	= qpnp_lbc_suspend,
};

static struct of_device_id qpnp_lbc_match_table[] = {
	{ .compatible = QPNP_CHARGER_DEV_NAME, },
	{}
};

static struct spmi_driver qpnp_lbc_driver = {
	.probe		= qpnp_lbc_probe,
	.remove		= qpnp_lbc_remove,
	.driver		= {
		.name		= QPNP_CHARGER_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_lbc_match_table,
		.pm		= &qpnp_lbc_pm_ops,
	},
};

static int __init qpnp_lbc_init(void)
{
	flag_keep_charge_on =
		(get_kernel_flag() & KERNEL_FLAG_KEEP_CHARG_ON) ? 1 : 0;
	flag_pa_recharge =
		(get_kernel_flag() & KERNEL_FLAG_PA_RECHARG_TEST) ? 1 : 0;
	flag_enable_bms_charger_log =
               (get_kernel_flag() & KERNEL_FLAG_ENABLE_BMS_CHARGER_LOG) ? 1 : 0;
	test_power_monitor =
		(get_kernel_flag() & KERNEL_FLAG_TEST_PWR_SUPPLY) ? 1 : 0;

	return spmi_driver_register(&qpnp_lbc_driver);
}
late_initcall(qpnp_lbc_init);

static void __exit qpnp_lbc_exit(void)
{
	spmi_driver_unregister(&qpnp_lbc_driver);
}
module_exit(qpnp_lbc_exit);

MODULE_DESCRIPTION("QPNP Linear charger driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_CHARGER_DEV_NAME);
