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


#ifndef _ATOMIC_ARM64_H
#define _ATOMIC_ARM64_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#include "include_check.h"

#include "mvp_assert.h"

#define ATOMIC_ADDO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, add)

#define ATOMIC_ADDV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, add)

#define ATOMIC_ANDO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, and)

#define ATOMIC_ANDV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, and)

#define ATOMIC_GETO(atm) ({							\
	typeof((atm).atm_Normal) _oldval;					\
										\
	switch (sizeof(_oldval)) {						\
	case 4:									\
		_oldval = ATOMIC_SINGLE_COPY_READ32(&((atm).atm_Volatl));	\
		break;								\
	case 8:									\
		_oldval = ATOMIC_SINGLE_COPY_READ64(&((atm).atm_Volatl));	\
		break;								\
	default:								\
		FATAL();							\
	}									\
	_oldval;								\
})

#define ATOMIC_ORO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, orr)

#define ATOMIC_ORV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, orr)

#define ATOMIC_SETIF(atm, newval, oldval) ({				\
	int _failed;							\
	typeof((atm).atm_Normal) _newval = newval;			\
	typeof((atm).atm_Normal) _oldval = oldval;			\
									\
	switch (sizeof(_newval)) {					\
	case 4:								\
		asm volatile (" // atomic 32 conditional set\n"		\
"1:	ldxr    %w0, [%1]\n"						\
"	cmp	%w0, %w2\n"						\
"	mov	%w0, #1\n"						\
"	B.ne	2f\n"							\
"	stxr	%w0, %w3, [%1]\n"					\
"	cbnz	%w0, 1b\n"						\
"2:"									\
	: "=&r" (_failed)						\
	: "r"   (&((atm).atm_Volatl)), "r" (_oldval), "r" (_newval)	\
	: "cc", "memory");						\
		break;							\
	case 8:								\
		asm volatile (" // atomic 64 conditional set\n"		\
"1:	ldxr    %0, [%1]\n"						\
"	cmp	%0, %2\n"						\
"	mov	%w0, #1\n"						\
"	B.ne	2f\n"							\
"	stxr	%w0, %3, [%1]\n"					\
"	cbnz	%w0, 1b\n"						\
"2:"									\
	: "=&r" (_failed)						\
	: "r"   (&((atm).atm_Volatl)), "r" (_oldval), "r" (_newval)	\
	: "cc", "memory");						\
		break;							\
	default:							\
		FATAL();						\
	}								\
	!_failed;							\
})


#define ATOMIC_SETO(atm, newval) ({				\
	int _tmp;						\
	typeof((atm).atm_Normal) _oldval;			\
	typeof((atm).atm_Normal) _newval = newval;		\
								\
	switch (sizeof(_newval)) {				\
	case 4:							\
		asm volatile (" // atomic 32 set\n"		\
"1:	ldxr    %w0, [%2]\n"					\
"	stxr	%w1, %w3, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_tmp)				\
	: "r"   (&((atm).atm_Volatl)), "r" (_newval)		\
	: "memory");						\
		break;						\
	case 8:							\
		asm volatile (" // atomic 64 set\n"		\
"1:	ldxr    %0, [%2]\n"					\
"	stxr	%w1, %3, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_tmp)				\
	: "r"   (&((atm).atm_Volatl)), "r" (_newval)		\
	: "memory");						\
		break;						\
	default:						\
		FATAL();					\
	}							\
	_oldval;						\
})

#define ATOMIC_SETV(atm, newval) ATOMIC_SETO((atm), (newval))

#define ATOMIC_SUBO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, sub)

#define ATOMIC_SUBV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, sub)

#define ATOMIC_OPO_PRIVATE(atm, modval, op) ({			\
	int _tmp;						\
	typeof((atm).atm_Normal) _modval = modval;		\
	typeof((atm).atm_Normal) _oldval;			\
	typeof((atm).atm_Normal) _newval;			\
								\
	switch (sizeof(_newval)) {				\
	case 4:							\
		asm volatile (" // atomic 32 op output\n"	\
"1:	ldxr    %w0, [%3]\n"					\
	#op "	%w1, %w0, %w4\n"				\
"	stxr	%w2, %w1, [%3]\n"				\
"	cbnz	%w2, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_newval), "=&r" (_tmp)	\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	case 8:							\
		asm volatile (" // atomic 64 op output\n"	\
"1:	ldxr    %0, [%3]\n"					\
	#op "	%1, %0, %4\n"					\
"	stxr	%w2, %1, [%3]\n"				\
"	cbnz	%w2, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_newval), "=&r" (_tmp)	\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	default:						\
		FATAL();					\
	}							\
	_oldval;						\
})

#define ATOMIC_OPV_PRIVATE(atm, modval, op) do {		\
	int _failed;						\
	typeof((atm).atm_Normal) _modval = modval;		\
	typeof((atm).atm_Normal) _sample;			\
								\
	switch (sizeof(_modval)) {				\
	case 4:							\
		asm volatile (" // atomic 32 op void\n"		\
"1:	ldxr    %w0, [%2]\n"					\
	#op "	%w0, %w0, %w3\n"				\
"	stxr	%w1, %w0, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_sample), "=&r" (_failed)			\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	case 8:							\
		asm volatile (" // atomic 64 op void\n"		\
"1:	ldxr    %0, [%2]\n"					\
	#op "	%0, %0, %3\n"					\
"	stxr	%w1, %0, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_sample), "=&r" (_failed)			\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	default:						\
		FATAL();					\
	}							\
} while (0)



#define ATOMIC_SINGLE_COPY_WRITE32(p, val) do {	\
	ASSERT(sizeof(val) == 4);		\
	ASSERT((MVA)(p) % sizeof(val) == 0);	\
	*(uint32*)(p) = (val);			\
} while (0)

#define ATOMIC_SINGLE_COPY_READ32(p) ({		\
	ASSERT((MVA)(p) % sizeof(uint32) == 0);	\
	(uint32) *(p);				\
})

#define ATOMIC_SINGLE_COPY_WRITE64(p, val) do {	\
	ASSERT(sizeof(val) == 8);		\
	ASSERT((MVA)(p) % sizeof(val) == 0);	\
	*(uint64 *)(p) = (val);			\
} while (0)


#define ATOMIC_SINGLE_COPY_READ64(p) ({		\
	ASSERT((MVA)(p) % sizeof(uint64) == 0);	\
	(uint64) *(p);				\
})
#endif
