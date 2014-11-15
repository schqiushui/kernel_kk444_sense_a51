#ifndef _KERNEL_WORKQUEUE_INTERNAL_H
#define _KERNEL_WORKQUEUE_INTERNAL_H

#include <linux/workqueue.h>
#include <linux/kthread.h>

struct worker_pool;

struct worker {
	
	union {
		struct list_head	entry;	
		struct hlist_node	hentry;	
	};

	struct work_struct	*current_work;	
	work_func_t		current_func;	
	struct pool_workqueue	*current_pwq; 
	bool			desc_valid;	
	struct list_head	scheduled;	

	

	struct task_struct	*task;		
	struct worker_pool	*pool;		
						

	unsigned long		last_active;	
	unsigned int		flags;		
	int			id;		

	char			desc[WORKER_DESC_LEN];

	
	struct workqueue_struct	*rescue_wq;	
	struct work_struct 	*previous_work;	
};

static inline struct worker *current_wq_worker(void)
{
	if (current->flags & PF_WQ_WORKER)
		return kthread_data(current);
	return NULL;
}

void wq_worker_waking_up(struct task_struct *task, int cpu);
struct task_struct *wq_worker_sleeping(struct task_struct *task, int cpu);

#endif 
