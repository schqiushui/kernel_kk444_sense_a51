/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_IOMAP_8916_H
#define __ASM_ARCH_MSM_IOMAP_8916_H


#define MSM8916_APCS_GCC_PHYS	0xB011000
#define MSM8916_APCS_GCC_SIZE	SZ_4K

#ifdef CONFIG_HTC_FEATURES_RIL_PCN0004_HTC_GARBAGE_FILTER
#define MSM_SHARED_RAM_BASE    IOMEM(0x86300000)       
#define MSM8916_MSM_SHARED_RAM_PHYS	0x86300000
#endif

#ifdef CONFIG_DEBUG_MSM8916_UART
#define MSM_DEBUG_UART_BASE	IOMEM(0xFA0B0000)
#define MSM_DEBUG_UART_PHYS	0x78B0000
#endif

#ifdef CONFIG_HTC_DEBUG_FOOTPRINT
#define HTC_DEBUG_INFO_BASE	0x8CBD0000
#define HTC_DEBUG_INFO_SIZE	SZ_128K
#define HTC_DEBUG_FOOTPRINT_PHYS	HTC_DEBUG_INFO_BASE + (HTC_DEBUG_INFO_SIZE - SZ_64K) 
#define HTC_DEBUG_FOOTPRINT_BASE	IOMEM(0xFE000000)
#define HTC_DEBUG_FOOTPRINT_SIZE	SZ_8K
#endif

#define MSM_MPM_SLEEPTICK_BASE	IOMEM(0xFE010000)
#define MSM8916_MPM_SLEEPTICK_PHYS	0x004A3000
#define MSM8916_MPM_SLEEPTICK_SIZE	SZ_4K

#endif
