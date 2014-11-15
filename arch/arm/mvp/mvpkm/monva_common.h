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


#ifndef _MONVA_COMMON_H_
#define _MONVA_COMMON_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "mmu_defs.h"
#include "mmu_types.h"
#ifdef CONFIG_ARM64
#include "arm64_types.h"
#endif


#define MONITOR_VA_START       ((MVA)0xE8000000)
#define MONITOR_VA_LEN         0x03000000

#ifndef __aarch64__
#define MONITOR_VA_WORLDSWITCH \
	((MVA)(MONITOR_VA_START + 3 * PAGE_SIZE))
#else
#define MONITOR_VA_WORLDSWITCH \
	((MVA)(MONITOR_VA_START + 2 * PAGE_SIZE))
#endif

#ifdef IN_MONITOR
#define MONITOR_VA_UART \
	(MONITOR_VA_WORLDSWITCH + WSP_PAGE_COUNT * PAGE_SIZE)
#endif

typedef enum {
	MEMREGION_MAINMEM = 1,
	MEMREGION_MODULE = 2,
	MEMREGION_WSP = 3,
	MEMREGION_MONITOR_MISC = 4,
	MEMREGION_DEFAULT = 0
} PACKED PhysMem_RegionType;

typedef struct MonVA {		
	MA  l2BaseMA;		
	MVA excVec;		
} MonVA;

typedef enum {
	MVA_MEMORY = 0,
	MVA_DEVICE = 1
} MVAType;

#define MONITOR_TYPE_LPV        0
#define MONITOR_TYPE_VE         1
#define MONITOR_TYPE_LPV64      2
#define MONITOR_TYPE_UNKNOWN  0xf

typedef uint32 MonitorType;

#endif
