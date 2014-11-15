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



#ifndef _MVPKMPRIVATE_H
#define _MVPKMPRIVATE_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include <linux/ioctl.h>
#define MVP_IOCTL_LETTER '9'
#define MVPKM_DISABLE_FAULT  _IO(MVP_IOCTL_LETTER, 0xa0)
#define MVPKM_LOCK_MPN       _IOW(MVP_IOCTL_LETTER, 0xa1, MvpkmLockMPN)
#define MVPKM_UNLOCK_MPN     _IOW(MVP_IOCTL_LETTER, 0xa2, MvpkmLockMPN)
#define MVPKM_RUN_MONITOR    _IO(MVP_IOCTL_LETTER, 0xa3)
#define MVPKM_CPU_INFO       _IOR(MVP_IOCTL_LETTER, 0xa4, MvpkmCpuInfo)
#define MVPKM_ABORT_MONITOR  _IO(MVP_IOCTL_LETTER, 0xa5)
#define MVPKM_CPU_INFO64     _IOR(MVP_IOCTL_LETTER, 0xa6, MvpkmCpuInfo64)
#define MVPKM_MAP_WSPHKVA    _IOW(MVP_IOCTL_LETTER, 0xa7, MvpkmMapHKVA)

#include "mksck.h"
#include "monva_common.h"
#include "mvpkm_types.h"

typedef struct MvpkmLockMPN {
	uint32  order;  
	PhysMem_RegionType forRegion;  
	uint32  mpn;    
} MvpkmLockMPN;

typedef struct MvpkmMapHKVA {
	uint64 nrPages;  
	HkvaMapInfo *mapInfo;  
#ifndef __aarch64__ 
	uint32 padding0; 
#endif
	PhysMem_RegionType forRegion;  
#ifndef __aarch64__ 
	uint32 padding1; 
#endif
	uint64 hkva;    
} MvpkmMapHKVA;

typedef struct MvpkmCpuInfo {
	ARM_L2D attribL2D;           
	ARM_MemAttrNormal attribMAN; 
	_Bool mpExt;                 
} MvpkmCpuInfo;

#ifdef CONFIG_ARM64
typedef struct MvpkmCpuInfo64 {
	ARM_ARM64_L3D attribL3D;           
	ARM_MemAttrNormal attribMAN; 
	_Bool mpExt;                 
} MvpkmCpuInfo64;
#endif

#define MVPKM_STUBPAGE_BEG 0x78d10c67
#define MVPKM_STUBPAGE_END 0x8378f3dd
#endif
