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


#include <linux/module.h>
#include "comm_transp_impl.h"

int (*CommTranspEvProcess)(CommTranspID *id, CommTranspIOEvent event);



void
Mvpkm_CommEvRegisterProcessCB(int (*commProcessFunc)(CommTranspID*,
						     CommTranspIOEvent))
{
	CommTranspEvProcess = commProcessFunc;
}
EXPORT_SYMBOL(Mvpkm_CommEvRegisterProcessCB);


void
Mvpkm_CommEvUnregisterProcessCB(void)
{
	CommTranspEvProcess = NULL;
}
EXPORT_SYMBOL(Mvpkm_CommEvUnregisterProcessCB);


