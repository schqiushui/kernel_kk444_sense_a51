/* ********************************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 * -- VMware Confidential
 * ********************************************************************/

#ifndef _WORLDSWITCH_H
#define _WORLDSWITCH_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"


#ifndef __ASSEMBLER__
typedef void (*SwitchToMonitor)(void *regSave);
typedef void (*SwitchToUser)(void *regSaveEnd);
#endif

#ifdef __aarch64__
#include "worldswitch64.h"
#else
#include "worldswitch32.h"
#endif

#endif 
