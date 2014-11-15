/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/of_gpio.h>
#include <linux/clk/msm-clk.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/smsm.h>
#if defined(CONFIG_HTC_FEATURES_RIL_PCN0001_REBOOT_WITH_ERASE_EFS)
#include <soc/qcom/restart.h>
#include <asm/system_misc.h>
#include "smd_private.h"
#endif

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "pil-msa.h"
#if defined(CONFIG_HTC_FEATURES_SSR)
#include <mach/devices_dtb.h>
#include <mach/devices_cmdline.h>
#endif

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0006_HTC_DUMP_BAM_DMUX_LOG
extern void msm_bam_dmux_dumplog(void);
#endif
#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG
extern void msm_smsm_dumplog(void);
#endif

#define MAX_VDD_MSS_UV		1150000
#define PROXY_TIMEOUT_MS	10000
#define MAX_SSR_REASON_LEN	81U
#define STOP_ACK_TIMEOUT_MS	1000

#define subsys_to_drv(d) container_of(d, struct modem_data, subsys_desc)

#if defined(CONFIG_HTC_DEBUG_SSR)
static void log_modem_sfr(struct subsys_device *dev)
#else
static void log_modem_sfr(void)
#endif
{
	u32 size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];

	smem_reason = smem_get_entry_no_rlock(SMEM_SSR_REASON_MSS0, &size, 0,
							SMEM_ANY_HOST_FLAG);
	if (!smem_reason || !size) {
		pr_err("modem subsystem failure reason: (unknown, smem_get_entry_no_rlock failed).\n");
		return;
	}
	if (!smem_reason[0]) {
		pr_err("modem subsystem failure reason: (unknown, empty string found).\n");
		return;
	}

	strlcpy(reason, smem_reason, min(size, MAX_SSR_REASON_LEN));
	pr_err("modem subsystem failure reason: %s.\n", reason);
#if defined(CONFIG_HTC_DEBUG_SSR)
	subsys_set_restart_reason(dev, reason);
#endif

	smem_reason[0] = '\0';
	wmb();
}

static void restart_modem(struct modem_data *drv)
{
#if defined(CONFIG_HTC_DEBUG_SSR)
	log_modem_sfr(drv->subsys);
#else
	log_modem_sfr();
#endif
	drv->ignore_errors = true;
	subsystem_restart_dev(drv->subsys);
}

#if defined(CONFIG_HTC_DEBUG_RIL_PCN0002_DUMP_STACK)
static void dump_dbg_info(void)
{

	printk(KERN_INFO "=== Show rmt_storage ===");
	show_thread_group_state_filter("rmt_storage", 0);
	printk(KERN_INFO "\n");

	pr_info("### Show Blocked State in ###\n");
	show_state_filter(TASK_UNINTERRUPTIBLE);
}
#endif

static irqreturn_t modem_err_fatal_intr_handler(int irq, void *dev_id)
{
	struct modem_data *drv = subsys_to_drv(dev_id);

	
	if (drv->crash_shutdown)
		return IRQ_HANDLED;

#if defined(CONFIG_HTC_DEBUG_RIL_PCN0002_DUMP_STACK)
	dump_dbg_info();
#endif

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG
	msm_smsm_dumplog();
#endif

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0006_HTC_DUMP_BAM_DMUX_LOG
	msm_bam_dmux_dumplog();
#endif

	pr_err("Fatal error on the modem.\n");
	subsys_set_crash_status(drv->subsys, true);
#if defined(CONFIG_HTC_FEATURES_RIL_PCN0001_REBOOT_WITH_ERASE_EFS)
	if (smd_smsm_erase_efs()) {
		pr_err("Unrecoverable efs, need to reboot and erase"
				"modem_st1/st2 partitions...\n");
		if ( arm_pm_restart ) {
			arm_pm_restart(REBOOT_HARD, "erase-efs");
		}
		else {
			pr_err("arm_pm_restart is null\n");
			restart_modem(drv);
		}
	} else {
		restart_modem(drv);
	}
#else
	restart_modem(drv);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t modem_stop_ack_intr_handler(int irq, void *dev_id)
{
	struct modem_data *drv = subsys_to_drv(dev_id);
	pr_info("Received stop ack interrupt from modem\n");
	complete(&drv->stop_ack);
	return IRQ_HANDLED;
}

static int modem_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	unsigned long ret;

	if (subsys->is_not_loadable)
		return 0;

	if (!subsys_get_crash_status(drv->subsys) && force_stop) {
		gpio_set_value(subsys->force_stop_gpio, 1);
		ret = wait_for_completion_timeout(&drv->stop_ack,
				msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));
		if (!ret)
			pr_warn("Timed out on stop ack from modem.\n");
		gpio_set_value(subsys->force_stop_gpio, 0);
	}

	pil_shutdown(&drv->q6->desc);

	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);

	if (subsys->is_not_loadable)
		return 0;
	INIT_COMPLETION(drv->stop_ack);
	drv->ignore_errors = false;
	return pil_boot(&drv->q6->desc);
}

static void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	drv->crash_shutdown = true;
	if (!subsys_get_crash_status(drv->subsys)) {
		gpio_set_value(subsys->force_stop_gpio, 1);
		mdelay(STOP_ACK_TIMEOUT_MS);
	}
}

static int modem_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	int ret;

	if (!enable)
		return 0;

	ret = pil_mss_make_proxy_votes(&drv->q6->desc);
	if (ret)
		return ret;

	ret = pil_mss_reset_load_mba(&drv->q6->desc);
	if (ret)
		return ret;

	ret = pil_do_ramdump(&drv->q6->desc, drv->ramdump_dev);
	if (ret < 0)
		pr_err("Unable to dump modem fw memory (rc = %d).\n", ret);

	dma_free_coherent(&drv->mba_mem_dev, drv->q6->mba_size,
				drv->q6->mba_virt, drv->q6->mba_phys);

	pil_mss_shutdown(&drv->q6->desc);
	pil_mss_remove_proxy_votes(&drv->q6->desc);
	return ret;
}

static irqreturn_t modem_wdog_bite_intr_handler(int irq, void *dev_id)
{
	struct modem_data *drv = subsys_to_drv(dev_id);
	if (drv->ignore_errors)
		return IRQ_HANDLED;
	pr_err("Watchdog bite received from modem software!\n");
#if defined(CONFIG_HTC_DEBUG_SSR)
	subsys_set_restart_reason(drv->subsys, "Watchdog bite received from modem software!");
#endif

	subsys_set_crash_status(drv->subsys, true);
	restart_modem(drv);
	return IRQ_HANDLED;
}

static int pil_subsys_init(struct modem_data *drv,
					struct platform_device *pdev)
{
	int ret;

	drv->subsys_desc.name = "modem";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = modem_shutdown;
	drv->subsys_desc.powerup = modem_powerup;
	drv->subsys_desc.ramdump = modem_ramdump;
	drv->subsys_desc.crash_shutdown = modem_crash_shutdown;
	drv->subsys_desc.err_fatal_handler = modem_err_fatal_intr_handler;
	drv->subsys_desc.stop_ack_handler = modem_stop_ack_intr_handler;
	drv->subsys_desc.wdog_bite_handler = modem_wdog_bite_intr_handler;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

#if defined(CONFIG_HTC_FEATURES_SSR)
#if defined(CONFIG_HTC_FEATURES_SSR_MODEM_ENABLE)
	if (get_kernel_flag() & (KERNEL_FLAG_ENABLE_SSR_MODEM)) {
		subsys_set_restart_level(drv->subsys, RESET_SOC);
		subsys_set_enable_ramdump(drv->subsys, DISABLE_RAMDUMP);
	} else {
		subsys_set_restart_level(drv->subsys, RESET_SUBSYS_COUPLED);
		if (get_radio_flag() & BIT(3))
			subsys_set_enable_ramdump(drv->subsys, ENABLE_RAMDUMP);
		else
			subsys_set_enable_ramdump(drv->subsys, DISABLE_RAMDUMP);
	}
#else
	if (get_kernel_flag() & (KERNEL_FLAG_ENABLE_SSR_MODEM)) {
		subsys_set_restart_level(drv->subsys, RESET_SUBSYS_COUPLED);
		if (get_radio_flag() & BIT(3))
			subsys_set_enable_ramdump(drv->subsys, ENABLE_RAMDUMP);
		else
			subsys_set_enable_ramdump(drv->subsys, DISABLE_RAMDUMP);
	} else {
		subsys_set_restart_level(drv->subsys, RESET_SOC);
		subsys_set_enable_ramdump(drv->subsys, DISABLE_RAMDUMP);
	}
#endif

	if (board_mfg_mode() != 0) {
		subsys_set_restart_level(drv->subsys, RESET_SOC);
		subsys_set_enable_ramdump(drv->subsys, DISABLE_RAMDUMP);
	}
#endif

	drv->ramdump_dev = create_ramdump_device("modem", &pdev->dev);
	if (!drv->ramdump_dev) {
		pr_err("%s: Unable to create a modem ramdump device.\n",
			__func__);
		ret = -ENOMEM;
		goto err_ramdump;
	}

	return 0;

err_ramdump:
	subsys_unregister(drv->subsys);
err_subsys:
	return ret;
}

static int pil_mss_loadable_init(struct modem_data *drv,
					struct platform_device *pdev)
{
	struct q6v5_data *q6;
	struct pil_desc *q6_desc;
	struct resource *res;
	struct property *prop;
	int ret;

	q6 = pil_q6v5_init(pdev);
	if (IS_ERR(q6))
		return PTR_ERR(q6);
	drv->q6 = q6;
	drv->xo = q6->xo;

	q6_desc = &q6->desc;
	q6_desc->owner = THIS_MODULE;
	q6_desc->proxy_timeout = PROXY_TIMEOUT_MS;

	q6_desc->ops = &pil_msa_mss_ops;

	q6->self_auth = of_property_read_bool(pdev->dev.of_node,
							"qcom,pil-self-auth");
	if (q6->self_auth) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						    "rmb_base");
		q6->rmb_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!q6->rmb_base)
			return -ENOMEM;
		drv->rmb_base = q6->rmb_base;
		q6_desc->ops = &pil_msa_mss_ops_selfauth;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "restart_reg");
	if (!res) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"restart_reg_sec");
		q6->restart_reg_sec = true;
	}

	q6->restart_reg = devm_request_and_ioremap(&pdev->dev, res);
	if (!q6->restart_reg)
		return -ENOMEM;

	q6->vreg = NULL;

	prop = of_find_property(pdev->dev.of_node, "vdd_mss-supply", NULL);
	if (prop) {
		q6->vreg = devm_regulator_get(&pdev->dev, "vdd_mss");
		if (IS_ERR(q6->vreg))
			return PTR_ERR(q6->vreg);

		ret = regulator_set_voltage(q6->vreg, VDD_MSS_UV,
						MAX_VDD_MSS_UV);
		if (ret)
			dev_err(&pdev->dev, "Failed to set vreg voltage.\n");

		ret = regulator_set_optimum_mode(q6->vreg, 100000);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set vreg mode.\n");
			return ret;
		}
	}

	q6->vreg_mx = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(q6->vreg_mx))
		return PTR_ERR(q6->vreg_mx);
	prop = of_find_property(pdev->dev.of_node, "vdd_mx-uV", NULL);
	if (!prop) {
		dev_err(&pdev->dev, "Missing vdd_mx-uV property\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"cxrail_bhs_reg");
	if (res)
		q6->cxrail_bhs = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));

	q6->ahb_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(q6->ahb_clk))
		return PTR_ERR(q6->ahb_clk);

	q6->axi_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(q6->axi_clk))
		return PTR_ERR(q6->axi_clk);

	q6->rom_clk = devm_clk_get(&pdev->dev, "mem_clk");
	if (IS_ERR(q6->rom_clk))
		return PTR_ERR(q6->rom_clk);

	ret = pil_desc_init(q6_desc);

	return ret;
}

static int pil_mss_driver_probe(struct platform_device *pdev)
{
	struct modem_data *drv;
	int ret, is_not_loadable;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	is_not_loadable = of_property_read_bool(pdev->dev.of_node,
							"qcom,is-not-loadable");
	if (is_not_loadable) {
		drv->subsys_desc.is_not_loadable = 1;
	} else {
		ret = pil_mss_loadable_init(drv, pdev);
		if (ret)
			return ret;
	}
	init_completion(&drv->stop_ack);

	return pil_subsys_init(drv, pdev);
}

static int pil_mss_driver_exit(struct platform_device *pdev)
{
	struct modem_data *drv = platform_get_drvdata(pdev);

	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->q6->desc);
	return 0;
}

static struct of_device_id mss_match_table[] = {
	{ .compatible = "qcom,pil-q6v5-mss" },
	{ .compatible = "qcom,pil-q6v55-mss" },
	{ .compatible = "qcom,pil-q6v56-mss" },
	{}
};

static struct platform_driver pil_mss_driver = {
	.probe = pil_mss_driver_probe,
	.remove = pil_mss_driver_exit,
	.driver = {
		.name = "pil-q6v5-mss",
		.of_match_table = mss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_mss_init(void)
{
	return platform_driver_register(&pil_mss_driver);
}
module_init(pil_mss_init);

static void __exit pil_mss_exit(void)
{
	platform_driver_unregister(&pil_mss_driver);
}
module_exit(pil_mss_exit);

MODULE_DESCRIPTION("Support for booting modem subsystems with QDSP6v5 Hexagon processors");
MODULE_LICENSE("GPL v2");
