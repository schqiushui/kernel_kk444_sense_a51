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


#ifndef _CPUFREQ_KERNEL_H
#define _CPUFREQ_KERNEL_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

struct TscToRate64Cb {
	uint32 mult;
	uint32 shift;
};

void CpuFreq_Init(void);
void CpuFreq_Exit(void);
int CpuFreqUpdate(unsigned int *freq, struct TscToRate64Cb *ttr);

#endif
