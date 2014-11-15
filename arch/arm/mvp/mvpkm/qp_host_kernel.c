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
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/slab.h>

#include "mvp.h"
#include "mvpkm_kernel.h"
#include "qp.h"
#include "qp_host_kernel.h"

static QPHandle   queuePairs[QP_MAX_QUEUE_PAIRS];
static QPListener listeners[QP_MAX_LISTENERS];

static DEFINE_MUTEX(qpLock);

#define QPLock()    mutex_lock(&qpLock)
#define QPUnlock()  mutex_unlock(&qpLock)


static int32
MapPages(struct MvpkmVM *vm,
	 MPN base,
	 uint32 nrPages,
	 QPHandle *qp,
	 HKVA *hkva)
{
	HKVA *va;
	uint32 i;
	uint32 rc;
	struct page *basepfn = pfn_to_page(base);
	struct page **pages;

	BUG_ON(!vm); 

	if (!hkva)
		return QP_ERROR_INVALID_ARGS;

	pages = kmalloc(nrPages * sizeof(MPN), GFP_KERNEL);
	if (!pages)
		return QP_ERROR_NO_MEM;

	down_write(&vm->lockedSem);
	va = kmap(basepfn);
	if (!va) {
		rc = QP_ERROR_INVALID_ARGS;
		kfree(pages);
		qp->pages = NULL;
		goto out;
	}

	for (i = 0; i < nrPages; i++) {
		pages[i] = pfn_to_page(((MPN *)va)[i]);
		get_page(pages[i]);
	}

	kunmap(basepfn);
	va = vmap(pages, nrPages, VM_MAP, PAGE_KERNEL);
	if (!va) {
		rc = QP_ERROR_NO_MEM;
		for (i = 0; i < nrPages; i++)
			put_page(pages[i]);
		kfree(pages);
		qp->pages = NULL;
		goto out;
	} else {
		*hkva = (HKVA)va;
		qp->pages = pages;
	}

	memset(va, 0x0, nrPages * PAGE_SIZE);

	rc = QP_SUCCESS;

out:
	up_write(&vm->lockedSem);
	return rc;
}


void
QP_HostInit(void)
{
	uint32 i;

	for (i = 0; i < QP_MAX_QUEUE_PAIRS; i++)
		QP_MakeInvalidQPHandle(&queuePairs[i]);

	for (i = 0; i < QP_MAX_LISTENERS; i++)
		listeners[i] = NULL;
}



int32
QP_GuestDetachRequest(QPId id)
{
	QPHandle *qp;
	uint32 i;

	if (id.resource >= QP_MAX_ID && id.resource != QP_INVALID_ID)
		return QP_ERROR_INVALID_ARGS;

	QPLock();

	if (id.resource == QP_INVALID_ID) {
		for (i = 0; i < QP_MAX_QUEUE_PAIRS; i++) {
			qp = &queuePairs[i];
			if (qp->id.context == id.context && qp->peerDetachCB)
				qp->peerDetachCB(qp->detachData);
		}
	} else {
		qp = &queuePairs[id.resource];
		if (qp->peerDetachCB)
			qp->peerDetachCB(qp->detachData);
	}

	QPUnlock();

	return QP_SUCCESS;
}



int32
QP_GuestAttachRequest(struct MvpkmVM *vm,
		      QPInitArgs *args,
		      MPN base,
		      uint32 nrPages)
{
	int32 rc;
	HKVA hkva = 0;
	QPHandle *qp;
	uint32 i;

	if ((!QP_CheckArgs(args))                              ||
	    (vm->wsp->guestId != (Mksck_VmId)args->id.context) ||
	    (args->capacity != (nrPages * PAGE_SIZE)))
		return QP_ERROR_INVALID_ARGS;

	pr_debug("%s: Guest requested attach to [%u:%u] capacity: %u type: %x base: %"LONG_FORMAT" nrPages: %u\n",
		 __func__, args->id.context, args->id.resource, args->capacity,
		 args->type, base, nrPages);

	QPLock();

	if (args->id.resource == QP_INVALID_ID) {
		for (i = 0; i < QP_MAX_QUEUE_PAIRS; i++)
			if (queuePairs[i].state == QP_STATE_FREE) {
				args->id.resource = i;
				pr_debug("%s: Guest requested anonymous region, assigning resource id %u\n",
					 __func__, args->id.resource);
				goto found;
			}

		rc = QP_ERROR_NO_MEM;
		goto out;
	}

found:
	qp = queuePairs + args->id.resource;

	if (qp->state != QP_STATE_FREE) {
		rc = QP_ERROR_ALREADY_ATTACHED;
		goto out;
	}

	rc = MapPages(vm, base, nrPages, qp, &hkva);
	if (rc != QP_SUCCESS)
		goto out;

	
	qp->id        = args->id;
	qp->capacity  = args->capacity;
	qp->produceQ  = (QHandle *)hkva;
	qp->consumeQ  = (QHandle *)(hkva + args->capacity/2);
	qp->queueSize = args->capacity/2 - sizeof(QHandle);
	qp->type      = args->type;
	qp->state     = QP_STATE_GUEST_ATTACHED;

	pr_debug("%s: Guest attached to region [%u:%u] capacity: %u HKVA: %x\n",
		 __func__, args->id.context, args->id.resource,
		 args->capacity, (uint32)hkva);
	rc = QP_SUCCESS;

out:
	QPUnlock();
	if (rc != QP_SUCCESS)
		pr_debug("%s: Failed to attach: %u\n", __func__, rc);
	return rc;
}



int32
QP_Attach(QPInitArgs *args,
	  QPHandle **qp)
{
	uint32 rc;

	if (!qp || !QP_CheckArgs(args))
		return QP_ERROR_INVALID_ARGS;

	pr_debug("%s: Attaching to id: [%u:%u] capacity: %u\n",
		 __func__, args->id.context, args->id.resource, args->capacity);

	QPLock();
	*qp = queuePairs + args->id.resource;

	if (!QP_CheckHandle(*qp)) {
		*qp = NULL;
		rc = QP_ERROR_INVALID_HANDLE;
		goto out;
	}

	if ((*qp)->state == QP_STATE_CONNECTED) {
		rc = QP_ERROR_ALREADY_ATTACHED;
		goto out;
	}

	if ((*qp)->state != QP_STATE_GUEST_ATTACHED) {
		rc = QP_ERROR_INVALID_HANDLE;
		goto out;
	}

	(*qp)->state = QP_STATE_CONNECTED;

	pr_debug("%s: Attached!\n", __func__);
	rc = QP_SUCCESS;

out:
	QPUnlock();
	return rc;
}
EXPORT_SYMBOL(QP_Attach);


int32
QP_Detach(QPHandle *qp)
{
	uint32 rc;
	uint32 i;

	QPLock();
	if (!QP_CheckHandle(qp)) {
		rc = QP_ERROR_INVALID_HANDLE;
		goto out;
	}

	pr_debug("%s: Freeing queue pair [%u:%u]\n",
		 __func__, qp->id.context, qp->id.resource);

	BUG_ON(!qp->produceQ);
	BUG_ON(!qp->pages);
	BUG_ON((qp->state != QP_STATE_CONNECTED) &&
	       (qp->state != QP_STATE_GUEST_ATTACHED));

	vunmap(qp->produceQ);

	for (i = 0; i < qp->capacity/PAGE_SIZE; i++)
		put_page(qp->pages[i]);
	kfree(qp->pages);

	pr_debug("%s: Host detached from [%u:%u]\n",
		 __func__, qp->id.context, qp->id.resource);

	QP_MakeInvalidQPHandle(qp);
	rc = QP_SUCCESS;

out:
	QPUnlock();
	return rc;
}



void QP_DetachAll(Mksck_VmId vmID)
{
	QPId id = {
		.context = (uint32)vmID,
		.resource = QP_INVALID_ID
	};

	pr_debug("%s: Detaching all queue pairs from vmId context %u\n",
		 __func__, vmID);
	QP_GuestDetachRequest(id);
}


int32
QP_RegisterListener(const QPListener listener)
{
	uint32 i;
	int32 rc = QP_ERROR_NO_MEM;

	QPLock();
	for (i = 0; i < QP_MAX_LISTENERS; i++)
		if (!listeners[i]) {
			listeners[i] = listener;
			pr_debug("%s: Registered listener\n", __func__);
			rc = QP_SUCCESS;
			break;
		}
	QPUnlock();

	return rc;
}
EXPORT_SYMBOL(QP_RegisterListener);



int32
QP_UnregisterListener(const QPListener listener)
{
	uint32 i;
	int32 rc = QP_ERROR_INVALID_HANDLE;

	QPLock();
	for (i = 0; i < QP_MAX_LISTENERS; i++)
		if (listeners[i] == listener) {
			listeners[i] = NULL;
			pr_debug("%s: Unregistered listener\n", __func__);
			rc = QP_SUCCESS;
			break;
		}
	QPUnlock();
	return rc;
}
EXPORT_SYMBOL(QP_UnregisterListener);



int32
QP_RegisterDetachCB(QPHandle *qp,
		    void (*callback)(void *),
		    void *data)
{
	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	if (!callback)
		return QP_ERROR_INVALID_ARGS;

	qp->peerDetachCB   = callback;
	qp->detachData = data;
	pr_debug("%s: Registered detach callback\n", __func__);
	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_RegisterDetachCB);




int32 QP_Notify(QPInitArgs *args)
{
	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_Notify);



int32 QP_NotifyListener(QPInitArgs *args)
{
	int32 i;
	QPHandle *qp = NULL;

	if (!QP_CheckArgs(args))
		return QP_ERROR_INVALID_ARGS;

	QPLock();
	for (i = 0; i < QP_MAX_LISTENERS; i++)
		if (listeners[i]) {
			pr_debug("Delivering attach event to listener...\n");
			if (listeners[i](args) == QP_SUCCESS)
				break;
		}

	if (i == QP_MAX_LISTENERS) {

		qp = &queuePairs[args->id.resource];
	}

	QPUnlock();

	if (qp)
		QP_Detach(qp);

	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_Detach);

