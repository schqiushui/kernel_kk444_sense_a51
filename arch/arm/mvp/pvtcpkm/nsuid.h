/*
 * Linux 2.6.32 and later Kernel module for VMware MVP PVTCP Server
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


#ifndef _NSUID_H
#define _NSUID_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)

typedef uid_t kuid_t;
typedef gid_t kgid_t;

#define uid_eq(uid1, uid2) ((uid1) == (uid2))
#define gid_eq(gid1, gid2) ((gid1) == (gid2))

#define from_kuid(to_namespace, kuid) (kuid)

#define GLOBAL_ROOT_UID 0
#define INVALID_UID -1
#define INVALID_GID -1
#endif

#endif
