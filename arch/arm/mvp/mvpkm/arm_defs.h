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


#ifndef _ARM_DEFS_H_
#define _ARM_DEFS_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define ARM_V4 4
#define ARM_V5 5
#define ARM_V6 6
#define ARM_V7 7
#define ARM_V8 8

#include "coproc_defs.h"
#include "system_register_defs.h"
#include "exc_defs.h"
#include "instr_defs.h"
#include "mmu_defs.h"
#include "lpae_defs.h"

#ifdef CONFIG_ARM64
#include "arm64_defs.h"
#endif
#include "ve_defs.h"
#include "psr_defs.h"

#endif 
