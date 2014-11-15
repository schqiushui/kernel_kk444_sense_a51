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


#ifndef _EXC_TYPES_H_
#define _EXC_TYPES_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

enum ARM_Exception {
	EXC_NONE,
	EXC_RESET,
	EXC_UNDEFINED,
	EXC_SWI,
	EXC_PREFETCH_ABORT,
	EXC_DATA_ABORT,
	EXC_IRQ,
	EXC_FIQ
};
typedef enum ARM_Exception ARM_Exception;

#endif 
