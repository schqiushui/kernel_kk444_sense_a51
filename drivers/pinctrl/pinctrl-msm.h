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
#ifndef __PINCTRL_MSM_H__
#define __PINCTRL_MSM_H__

#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/machine.h>
#include <linux/platform_device.h>

struct msm_pin_grps {
	const char *name;
	unsigned int *pins;
	unsigned num_pins;
	u32 func;
};

struct msm_pmx_funcs {
	const char *name;
	const char **gps;
	unsigned num_grps;
};

struct msm_tlmm_irq_chip {
	int irq;
	void *__iomem chip_base;
	unsigned int num_irqs;
	unsigned int apps_id;
	unsigned long *enabled_irqs;
	unsigned long *dual_edge_irqs;
	unsigned long *wake_irqs;
	spinlock_t irq_lock;
	struct irq_domain *domain;
	const struct irq_domain_ops *domain_ops;
	struct irq_chip chip;
	struct irq_chip *irq_chip_extn;
	struct device *dev;
	struct device_node *node;
	void *pinfo;
	irqreturn_t (*handler)(int irq, struct msm_tlmm_irq_chip *ic);
};

struct msm_pintype_info {
	int (*prg_cfg)(uint pin_no, unsigned long *config, void *reg_data,
								bool rw);
	void (*prg_func)(uint pin_no, u32 func, void *reg_data, bool enable);
	void (*set_reg_base)(void __iomem **ptype_base,
						void __iomem *tlmm_base);
	int (*init_irq)(int irq, struct msm_pintype_info *pinfo,
						struct device *tlmm_dev);
	void __iomem *reg_base;
	const char *prop_name;
	const char *name;
	u32 num_pins;
	int pin_start;
	int pin_end;
	struct gpio_chip gc;
	struct msm_tlmm_irq_chip *irq_chip;
	bool supports_gpio;
	struct pinctrl_gpio_range grange;
	struct device_node *node;
};

struct msm_tlmm_pintype {
	const uint num_entries;
	struct msm_pintype_info *pintype_info;
};

struct msm_pindesc {
	struct msm_pintype_info *pin_info;
	char name[20];
};

struct msm_tlmm_desc {
	void __iomem *base;
	int irq;
	unsigned int num_pintypes;
	struct msm_pintype_info *pintypes;
};

int msm_pinctrl_probe(struct platform_device *pdev,
					struct msm_tlmm_desc *tlmm_info);
#ifdef CONFIG_USE_PINCTRL_IRQ
#ifdef CONFIG_PINCTRL_MSM_TLMM_V3
extern int msm_tlmm_v3_of_irq_init(struct device_node *np, struct irq_chip *ic);
#else
static inline int msm_tlmm_v3_of_irq_init(struct device_node *np,
							struct irq_chip *ic)
{
	return -EIO;
}
#endif
#ifdef CONFIG_PINCTRL_MSM_TLMM_V4
#ifdef CONFIG_HTC_POWER_DEBUG
struct  msm_gpio_dump_info {
        unsigned int dir;
        unsigned int pull;
        unsigned int drv;
        unsigned int value;
        unsigned int func_sel;
        unsigned int int_en;
        unsigned int int_owner;
};
void __msm_gpio_get_dump_info(struct gpio_chip *gc, unsigned gpio, struct msm_gpio_dump_info *data);
#endif
extern int msm_tlmm_v4_of_irq_init(struct device_node *np, struct irq_chip *ic);
#else
static inline int msm_tlmm_v4_of_irq_init(struct device_node *np,
							struct irq_chip *ic)
{
	return -EIO;
}
#endif
#endif
#endif
