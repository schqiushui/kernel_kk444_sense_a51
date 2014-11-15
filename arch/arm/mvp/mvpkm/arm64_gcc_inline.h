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


#ifndef _ARM64_GCC_INLINE_H_
#define _ARM64_GCC_INLINE_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define REGISTER_TO_STR(_reg) \
	#_reg
#define ARM_READ_SYSTEM_REG(_reg, _var) do {			\
	asm volatile ("mrs %0, " REGISTER_TO_STR(_reg) "\n"	\
		      : "=r" (_var));				\
} while (0)

#define ARM_READO_SYSTEM_REG_32(_reg) ({	\
	uint32 val;				\
	ARM_READ_SYSTEM_REG(_reg, val);		\
	val;					\
})
#define ARM_WRITE_SYSTEM_REG(_reg, _val) do {			\
	asm volatile ("msr "  REGISTER_TO_STR(_reg) ", %0\n"	\
		      :						\
		      : "r" (_val)				\
		      : "memory");				\
} while (0)

#define DMB() { asm volatile ("dmb sy" : : : "memory"); }
#define DSB() { asm volatile ("dsb sy" : : : "memory"); }
#define ISB() { asm volatile ("isb" : : : "memory"); }

#define ARM_TLB_OP_NOARG(_op) do { \
   asm volatile ( _op "\n" ::: "memory"); \
} while (0)

#define ARM_TLB_OP(_op, _reg) do { \
   asm volatile ( _op ", %0\n" :: "r" (_reg) : "memory"); \
} while (0)

#define ARM_CACHE_OP_NOARG(_op) do { \
   asm volatile ( _op "\n" ::: "memory"); \
} while (0)

#define ARM_CACHE_OP(_op, _reg) do { \
   asm volatile ( _op ", %0\n" :: "r" (_reg) : "memory"); \
} while (0)

#define VMM_ARM_CACHE_OP(_op, _val) \
   ({ \
    register ARM_Exception _ret; \
    asm volatile (" mov x0, %2 \n\t" \
                   _op ", %1 \n\t" \
                  "mov %0, x0 \n\t" \
                  : "=r" (_ret) \
                  : "r" (_val), "i" (EXC_NONE) \
                  : "x0", "memory"); \
    _ret;\
    })
static inline uint32
ARM_DisableInterrupts(void)
{
	register uint32 status;

	asm volatile ( "mrs %0, daif\n\t" : "=r" (status) );
	asm volatile ( "msr daifset, %0\n\t" : :"i" (2) );

	return status;
}

static inline void
ARM_RestoreInterrupts(uint32 status)
{
	asm volatile ("msr daif, %0 \n\t" : : "r" (status) : "memory");
}

static inline uint32
ARM_ReadCPSR(void)
{
	uint32 status=0;

	
	ASSERT(0);

	return status;
}

static inline uint32
ARM_ReadDAIF(void)
{
	uint32 status=0;
	asm volatile ( "mrs %0, daif\n\t" : "=r" (status) );
	return status;
}

static inline uint32
ARM_ReadSP(void)
{
	uint32 sp=0;

	asm volatile ("mov %0, sp\n\t" : "=r" (sp));

	return sp;
}
#endif 
