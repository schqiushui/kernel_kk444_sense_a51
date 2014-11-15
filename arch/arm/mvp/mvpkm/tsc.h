/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#line 5


#ifndef _TSC_H_
#define _TSC_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "arm_inline.h"

#define ARM_PMNC_E          (1 << 0)
#define ARM_PMNC_D          (1 << 3)

#define ARM_PMCNT_C          (1 << 31)

#define ARM_PMNC_INVALID_EVENT -1

#if defined(__aarch64__)
#define TSC_READ(_reg)  ARM_READ_SYSTEM_REG(PMCCNTR_EL0, (_reg))
#define TSC_WRITE(_reg) ARM_WRITE_SYSTEM_REG(PMCCNTR_EL0, (_reg))
#else
#define TSC_READ(_reg)  ARM_MRC_CP15(CYCLE_COUNT, (_reg))
#define TSC_WRITE(_reg) ARM_MCR_CP15(CYCLE_COUNT, (_reg))
#endif

#endif 
