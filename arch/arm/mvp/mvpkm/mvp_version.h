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


#ifndef _MVP_VERSION_H_
#define _MVP_VERSION_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_HOSTUSER
#include "include_check.h"
#include "utils.h"


#define MVP_VERSION_CODE 16800042
#define MVP_VERSION_CODE_FORMATSTR       "%s_%d"
#define MVP_VERSION_CODE_FORMATARGSV(V_) MVP_STRINGIFY(e.x.p), (V_)
#define MVP_VERSION_CODE_FORMATARGS	\
	MVP_VERSION_CODE_FORMATARGSV(MVP_VERSION_CODE)

#define MVP_VERSION_FORMATSTR				\
	MVP_VERSION_CODE_FORMATSTR			\
	" compiled at %s based on revision %s by user %s."

#define MVP_VERSION_FORMATARGS		\
	MVP_VERSION_CODE_FORMATARGS,	\
	__DATE__,			\
	MVP_STRINGIFY(mvp_stage_armv8-2419c99),	\
	MVP_STRINGIFY(rbenis)

#define MvpVersion_Map(map_, version_) ({		\
	uint32 ii_;					\
	uint32 versionApi_ = 0;				\
							\
	for (ii_ = 0; ii_ < NELEM(map_); ii_++) {	\
		if (map_[ii_] <= version_)		\
			versionApi_ = map_[ii_];	\
	}						\
	versionApi_;					\
})

#define MVP_AARCH32_VERSION_STR "-aarch32"
#define MVP_AARCH64_VERSION_STR "-aarch64"

#define MVP_LPAE_VERSION_STR "-lpae"

#define VVP_VERSION_CODE_MIN 16800015
#define OEK_VERSION_CODE_MIN 0x01000001

#define BALLOON_WATCHDOG     16800010
#define GUEST_SAME_MINFREE   16800033
#define BALLOON_WATCHDOG_RO  16800039

#endif 
