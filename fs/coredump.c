#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/swap.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/perf_event.h>
#include <linux/highmem.h>
#include <linux/spinlock.h>
#include <linux/key.h>
#include <linux/personality.h>
#include <linux/binfmts.h>
#include <linux/coredump.h>
#include <linux/utsname.h>
#include <linux/pid_namespace.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/tsacct_kern.h>
#include <linux/cn_proc.h>
#include <linux/audit.h>
#include <linux/tracehook.h>
#include <linux/kmod.h>
#include <linux/fsnotify.h>
#include <linux/fs_struct.h>
#include <linux/pipe_fs_i.h>
#include <linux/oom.h>
#include <linux/compat.h>

#include <asm/uaccess.h>
#include <asm/mmu_context.h>
#include <asm/tlb.h>
#include <asm/exec.h>

#include <trace/events/task.h>
#include "internal.h"
#include "coredump.h"

#include <trace/events/sched.h>

int core_uses_pid;
char core_pattern[CORENAME_MAX_SIZE] = "core";
unsigned int core_pipe_limit;

struct core_name {
	char *corename;
	int used, size;
};
static atomic_t call_count = ATOMIC_INIT(1);


static int expand_corename(struct core_name *cn)
{
	char *old_corename = cn->corename;

	cn->size = CORENAME_MAX_SIZE * atomic_inc_return(&call_count);
	cn->corename = krealloc(old_corename, cn->size, GFP_KERNEL);

	if (!cn->corename) {
		kfree(old_corename);
		return -ENOMEM;
	}

	return 0;
}

static int cn_printf(struct core_name *cn, const char *fmt, ...)
{
	char *cur;
	int need;
	int ret;
	va_list arg;

	va_start(arg, fmt);
	need = vsnprintf(NULL, 0, fmt, arg);
	va_end(arg);

	if (likely(need < cn->size - cn->used - 1))
		goto out_printf;

	ret = expand_corename(cn);
	if (ret)
		goto expand_fail;

out_printf:
	cur = cn->corename + cn->used;
	va_start(arg, fmt);
	vsnprintf(cur, need + 1, fmt, arg);
	va_end(arg);
	cn->used += need;
	return 0;

expand_fail:
	return ret;
}

static void cn_escape(char *str)
{
	for (; *str; str++)
		if (*str == '/')
			*str = '!';
}

static int cn_print_exe_file(struct core_name *cn)
{
	struct file *exe_file;
	char *pathbuf, *path;
	int ret;

	exe_file = get_mm_exe_file(current->mm);
	if (!exe_file) {
		char *commstart = cn->corename + cn->used;
		ret = cn_printf(cn, "%s (path unknown)", current->comm);
		cn_escape(commstart);
		return ret;
	}

	pathbuf = kmalloc(PATH_MAX, GFP_TEMPORARY);
	if (!pathbuf) {
		ret = -ENOMEM;
		goto put_exe_file;
	}

	path = d_path(&exe_file->f_path, pathbuf, PATH_MAX);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		goto free_buf;
	}

	cn_escape(path);

	ret = cn_printf(cn, "%s", path);

free_buf:
	kfree(pathbuf);
put_exe_file:
	fput(exe_file);
	return ret;
}

static int format_corename(struct core_name *cn, struct coredump_params *cprm)
{
	const struct cred *cred = current_cred();
	const char *pat_ptr = core_pattern;
	int ispipe = (*pat_ptr == '|');
	int pid_in_pattern = 0;
	int err = 0;

#ifdef CONFIG_HTC_INIT_COREDUMP
        char init_core_pattern[CORENAME_MAX_SIZE] = "/data/core/%e.%p";
        if((task_tgid_vnr(current) == 1) || (current->comm?(!strcmp(current->comm, "ueventd")):0) ){
                ispipe = 0;
                pat_ptr = init_core_pattern;
        }
#endif

	cn->size = CORENAME_MAX_SIZE * atomic_read(&call_count);
	cn->corename = kmalloc(cn->size, GFP_KERNEL);
	cn->used = 0;

	if (!cn->corename)
		return -ENOMEM;

	while (*pat_ptr) {
		if (*pat_ptr != '%') {
			if (*pat_ptr == 0)
				goto out;
			err = cn_printf(cn, "%c", *pat_ptr++);
		} else {
			switch (*++pat_ptr) {
			
			case 0:
				goto out;
			
			case '%':
				err = cn_printf(cn, "%c", '%');
				break;
			
			case 'p':
				pid_in_pattern = 1;
				err = cn_printf(cn, "%d",
					      task_tgid_vnr(current));
				break;
			
			case 'u':
				err = cn_printf(cn, "%d", cred->uid);
				break;
			
			case 'g':
				err = cn_printf(cn, "%d", cred->gid);
				break;
			case 'd':
				err = cn_printf(cn, "%d",
					__get_dumpable(cprm->mm_flags));
				break;
			
			case 's':
				err = cn_printf(cn, "%ld", cprm->siginfo->si_signo);
				break;
			
			case 't': {
				struct timeval tv;
				do_gettimeofday(&tv);
				err = cn_printf(cn, "%lu", tv.tv_sec);
				break;
			}
			
			case 'h': {
				char *namestart = cn->corename + cn->used;
				down_read(&uts_sem);
				err = cn_printf(cn, "%s",
					      utsname()->nodename);
				up_read(&uts_sem);
				cn_escape(namestart);
				break;
			}
			
			case 'e': {
				char *commstart = cn->corename + cn->used;
				err = cn_printf(cn, "%s", current->comm);
				cn_escape(commstart);
				break;
			}
			case 'E':
				err = cn_print_exe_file(cn);
				break;
			
			case 'c':
				err = cn_printf(cn, "%lu",
					      rlimit(RLIMIT_CORE));
				break;
			default:
				break;
			}
			++pat_ptr;
		}

		if (err)
			return err;
	}

	if (!ispipe && !pid_in_pattern && core_uses_pid) {
		err = cn_printf(cn, ".%d", task_tgid_vnr(current));
		if (err)
			return err;
	}
out:
	return ispipe;
}

static int zap_process(struct task_struct *start, int exit_code)
{
	struct task_struct *t;
	int nr = 0;

	start->signal->group_exit_code = exit_code;
	start->signal->group_stop_count = 0;

	t = start;
	do {
		task_clear_jobctl_pending(t, JOBCTL_PENDING_MASK);
		if (t != current && t->mm) {
			sigaddset(&t->pending.signal, SIGKILL);
			signal_wake_up(t, 1);
			nr++;
		}
	} while_each_thread(start, t);

	return nr;
}

static int zap_threads(struct task_struct *tsk, struct mm_struct *mm,
			struct core_state *core_state, int exit_code)
{
	struct task_struct *g, *p;
	unsigned long flags;
	int nr = -EAGAIN;

	spin_lock_irq(&tsk->sighand->siglock);
	if (!signal_group_exit(tsk->signal)) {
		mm->core_state = core_state;
		nr = zap_process(tsk, exit_code);
		tsk->signal->group_exit_task = tsk;
		
		tsk->signal->flags = SIGNAL_GROUP_COREDUMP;
		clear_tsk_thread_flag(tsk, TIF_SIGPENDING);
	}
	spin_unlock_irq(&tsk->sighand->siglock);
	if (unlikely(nr < 0))
		return nr;

	tsk->flags = PF_DUMPCORE;
	if (atomic_read(&mm->mm_users) == nr + 1)
		goto done;
	rcu_read_lock();
	for_each_process(g) {
		if (g == tsk->group_leader)
			continue;
		if (g->flags & PF_KTHREAD)
			continue;
		p = g;
		do {
			if (p->mm) {
				if (unlikely(p->mm == mm)) {
					lock_task_sighand(p, &flags);
					nr += zap_process(p, exit_code);
					p->signal->flags = SIGNAL_GROUP_EXIT;
					unlock_task_sighand(p, &flags);
				}
				break;
			}
		} while_each_thread(g, p);
	}
	rcu_read_unlock();
done:
	atomic_set(&core_state->nr_threads, nr);
	return nr;
}

static int coredump_wait(int exit_code, struct core_state *core_state)
{
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->mm;
	int core_waiters = -EBUSY;

	init_completion(&core_state->startup);
	core_state->dumper.task = tsk;
	core_state->dumper.next = NULL;

	down_write(&mm->mmap_sem);
	if (!mm->core_state)
		core_waiters = zap_threads(tsk, mm, core_state, exit_code);
	up_write(&mm->mmap_sem);

	if (core_waiters > 0) {
		struct core_thread *ptr;

		wait_for_completion(&core_state->startup);
		ptr = core_state->dumper.next;
		while (ptr != NULL) {
			wait_task_inactive(ptr->task, 0);
			ptr = ptr->next;
		}
	}

	return core_waiters;
}

static void coredump_finish(struct mm_struct *mm, bool core_dumped)
{
	struct core_thread *curr, *next;
	struct task_struct *task;

	spin_lock_irq(&current->sighand->siglock);
	if (core_dumped && !__fatal_signal_pending(current))
		current->signal->group_exit_code |= 0x80;
	current->signal->group_exit_task = NULL;
	current->signal->flags = SIGNAL_GROUP_EXIT;
	spin_unlock_irq(&current->sighand->siglock);

	next = mm->core_state->dumper.next;
	while ((curr = next) != NULL) {
		next = curr->next;
		task = curr->task;
		smp_mb();
		curr->task = NULL;
		wake_up_process(task);
	}

	mm->core_state = NULL;
}

static bool dump_interrupted(void)
{
	return signal_pending(current);
}

static void wait_for_dump_helpers(struct file *file)
{
	struct pipe_inode_info *pipe = file->private_data;

	pipe_lock(pipe);
	pipe->readers++;
	pipe->writers--;
	wake_up_interruptible_sync(&pipe->wait);
	kill_fasync(&pipe->fasync_readers, SIGIO, POLL_IN);
	pipe_unlock(pipe);

	wait_event_interruptible(pipe->wait, pipe->readers == 1);

	pipe_lock(pipe);
	pipe->readers--;
	pipe->writers++;
	pipe_unlock(pipe);
}

static int umh_pipe_setup(struct subprocess_info *info, struct cred *new)
{
	struct file *files[2];
	struct coredump_params *cp = (struct coredump_params *)info->data;
	int err = create_pipe_files(files, 0);
	if (err)
		return err;

	cp->file = files[1];

	err = replace_fd(0, files[0], 0);
	fput(files[0]);
	
	current->signal->rlim[RLIMIT_CORE] = (struct rlimit){1, 1};

	return err;
}

void do_coredump(siginfo_t *siginfo)
{
	struct core_state core_state;
	struct core_name cn;
	struct mm_struct *mm = current->mm;
	struct linux_binfmt * binfmt;
	const struct cred *old_cred;
	struct cred *cred;
	int retval = 0;
	int flag = 0;
	int ispipe;
	struct files_struct *displaced;
	bool need_nonrelative = false;
	bool core_dumped = false;
	static atomic_t core_dump_count = ATOMIC_INIT(0);
	struct coredump_params cprm = {
		.siginfo = siginfo,
		.regs = signal_pt_regs(),
		.limit = rlimit(RLIMIT_CORE),
		.mm_flags = mm->flags,
	};

	audit_core_dumps(siginfo->si_signo);

	binfmt = mm->binfmt;
	if (!binfmt || !binfmt->core_dump)
		goto fail;
	if (!__get_dumpable(cprm.mm_flags))
		goto fail;

	cred = prepare_creds();
	if (!cred)
		goto fail;
	if (__get_dumpable(cprm.mm_flags) == SUID_DUMP_ROOT) {
		
		flag = O_EXCL;		
		cred->fsuid = GLOBAL_ROOT_UID;	
		need_nonrelative = true;
	}

	retval = coredump_wait(siginfo->si_signo, &core_state);
	if (retval < 0)
		goto fail_creds;

	old_cred = override_creds(cred);

	ispipe = format_corename(&cn, &cprm);

	if (ispipe) {
		int dump_count;
		char **helper_argv;
		struct subprocess_info *sub_info;

		if (ispipe < 0) {
			printk(KERN_WARNING "format_corename failed\n");
			printk(KERN_WARNING "Aborting core\n");
			goto fail_corename;
		}

		if (cprm.limit == 1) {
			printk(KERN_WARNING
				"Process %d(%s) has RLIMIT_CORE set to 1\n",
				task_tgid_vnr(current), current->comm);
			printk(KERN_WARNING "Aborting core\n");
			goto fail_unlock;
		}
		cprm.limit = RLIM_INFINITY;

		dump_count = atomic_inc_return(&core_dump_count);
		if (core_pipe_limit && (core_pipe_limit < dump_count)) {
			printk(KERN_WARNING "Pid %d(%s) over core_pipe_limit\n",
			       task_tgid_vnr(current), current->comm);
			printk(KERN_WARNING "Skipping core dump\n");
			goto fail_dropcount;
		}

		helper_argv = argv_split(GFP_KERNEL, cn.corename+1, NULL);
		if (!helper_argv) {
			printk(KERN_WARNING "%s failed to allocate memory\n",
			       __func__);
			goto fail_dropcount;
		}

		retval = -ENOMEM;
		sub_info = call_usermodehelper_setup(helper_argv[0],
						helper_argv, NULL, GFP_KERNEL,
						umh_pipe_setup, NULL, &cprm);
		if (sub_info)
			retval = call_usermodehelper_exec(sub_info,
							  UMH_WAIT_EXEC);

		argv_free(helper_argv);
		if (retval) {
			printk(KERN_INFO "Core dump to %s pipe failed\n",
			       cn.corename);
			goto close_fail;
		}
	} else {
		struct inode *inode;

		if (cprm.limit < binfmt->min_coredump)
			goto fail_unlock;

		if (need_nonrelative && cn.corename[0] != '/') {
			printk(KERN_WARNING "Pid %d(%s) can only dump core "\
				"to fully qualified path!\n",
				task_tgid_vnr(current), current->comm);
			printk(KERN_WARNING "Skipping core dump\n");
			goto fail_unlock;
		}

		cprm.file = filp_open(cn.corename,
				 O_CREAT | 2 | O_NOFOLLOW | O_LARGEFILE | flag,
				 0600);
		if (IS_ERR(cprm.file))
			goto fail_unlock;

		inode = file_inode(cprm.file);
		if (inode->i_nlink > 1)
			goto close_fail;
		if (d_unhashed(cprm.file->f_path.dentry))
			goto close_fail;
		if (!S_ISREG(inode->i_mode))
			goto close_fail;
		if (!uid_eq(inode->i_uid, current_fsuid()))
			goto close_fail;
		if (!cprm.file->f_op || !cprm.file->f_op->write)
			goto close_fail;
		if (do_truncate(cprm.file->f_path.dentry, 0, 0, cprm.file))
			goto close_fail;
	}

	
	retval = unshare_files(&displaced);
	if (retval)
		goto close_fail;
	if (displaced)
		put_files_struct(displaced);
	if (!dump_interrupted()) {
		file_start_write(cprm.file);
		core_dumped = binfmt->core_dump(&cprm);
		file_end_write(cprm.file);
	}
	if (ispipe && core_pipe_limit)
		wait_for_dump_helpers(cprm.file);
close_fail:
	if (cprm.file){
#ifdef CONFIG_HTC_INIT_COREDUMP
                if(task_tgid_vnr(current) == 1)
                        vfs_fsync(cprm.file, 1);
#endif
		filp_close(cprm.file, NULL);
	}
fail_dropcount:
	if (ispipe)
		atomic_dec(&core_dump_count);
fail_unlock:
	kfree(cn.corename);
fail_corename:
	coredump_finish(mm, core_dumped);
	revert_creds(old_cred);
fail_creds:
	put_cred(cred);
fail:
	return;
}

int dump_write(struct file *file, const void *addr, int nr)
{
	return !dump_interrupted() &&
		access_ok(VERIFY_READ, addr, nr) &&
		file->f_op->write(file, addr, nr, &file->f_pos) == nr;
}
EXPORT_SYMBOL(dump_write);

int dump_seek(struct file *file, loff_t off)
{
	int ret = 1;

	if (file->f_op->llseek && file->f_op->llseek != no_llseek) {
		if (dump_interrupted() ||
		    file->f_op->llseek(file, off, SEEK_CUR) < 0)
			return 0;
	} else {
		char *buf = (char *)get_zeroed_page(GFP_KERNEL);

		if (!buf)
			return 0;
		while (off > 0) {
			unsigned long n = off;

			if (n > PAGE_SIZE)
				n = PAGE_SIZE;
			if (!dump_write(file, buf, n)) {
				ret = 0;
				break;
			}
			off -= n;
		}
		free_page((unsigned long)buf);
	}
	return ret;
}
EXPORT_SYMBOL(dump_seek);
