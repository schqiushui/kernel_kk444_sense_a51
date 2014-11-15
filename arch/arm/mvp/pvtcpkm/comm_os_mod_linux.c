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


#include "comm_os.h"
#include "comm_os_mod_ver.h"

#include <linux/moduleparam.h>



static char modParams[256];
module_param_string(COMM_OS_MOD_SHORT_NAME, modParams, sizeof(modParams), 0644);



static int __init
ModInit(void)
{
	int rc;

	if (!commOSModInit) {
		commos_info("%s: Can't find \'init\' function for module \'"
			    COMM_OS_MOD_SHORT_NAME_STRING "\'.\n", __func__);
		return -1;
	}

	commos_debug("%s: Module parameters: [%s].\n", __func__, modParams);

	rc = (*commOSModInit)(modParams);
	if (rc == 0)
		commos_info("%s: Module \'" COMM_OS_MOD_SHORT_NAME_STRING
			    "\' has been successfully initialized.\n",
			    __func__);
	else
		commos_info("%s: Module \'" COMM_OS_MOD_SHORT_NAME_STRING
			    "\' could not be initialized [%d].\n",
			    __func__, rc);

	return rc > 0 ? -rc : rc;
}



static void __exit
ModExit(void)
{
	if (!commOSModExit) {
		commos_info("%s: Can't find \'fini\' function for module \'"
			    COMM_OS_MOD_SHORT_NAME_STRING "\'.\n", __func__);
		return;
	}

	(*commOSModExit)();
	commos_info("%s: Module \'" COMM_OS_MOD_SHORT_NAME_STRING
		    "\' has been stopped.\n", __func__);
}


module_init(ModInit);
module_exit(ModExit);

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION(COMM_OS_MOD_NAME_STRING);
MODULE_VERSION(COMM_OS_MOD_VERSION_STRING);
MODULE_LICENSE("GPL v2");
MODULE_INFO(supported, "external");

