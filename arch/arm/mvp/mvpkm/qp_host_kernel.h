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



#ifndef _QP_HOST_KERNEL_H
#define _QP_HOST_KERNEL_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

void  QP_HostInit(void);
int32
QP_GuestAttachRequest(struct MvpkmVM *vm,
		      QPInitArgs *args,
		      MPN base,
		      uint32 nr_pages);
int32 QP_GuestDetachRequest(QPId id);
void  QP_DetachAll(Mksck_VmId vmID);
int32 QP_NotifyListener(QPInitArgs *args);

#endif
