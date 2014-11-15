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


#ifndef _MKSCK_SOCKADDR_H_
#define _MKSCK_SOCKADDR_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "mksck.h"

#define AF_MKSCK  AF_DECnet
#define PF_MKSCK  PF_DECnet

struct sockaddr_mk {
	sa_family_t   mk_family;
	Mksck_Address mk_addr;
};

#endif
