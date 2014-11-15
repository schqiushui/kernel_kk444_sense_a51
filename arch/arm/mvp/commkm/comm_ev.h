/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Guest Communications
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


#ifndef _COMM_EV_H
#define _COMM_EV_H

#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_MODULE
#include "include_check.h"

#define MVP_COMM_EV_SIGNATURE 0x4d4d4f43                   
#define MVP_COMM_EV_SIGNAL    (MVP_OBJECT_CUSTOM_BASE + 0) 
#define MVP_COMM_EV_READ_EVENT_DATA \
	(MVP_OBJECT_CUSTOM_BASE + 1) 
#define MVP_COMM_EV_LAST (MVP_OBJECT_CUSTOM_BASE + 2) 

typedef struct CommEvent {
	CommTranspID id;
	CommTranspIOEvent event;
} CommEvent;

#endif
