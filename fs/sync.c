
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/syscalls.h>
#include <linux/linkage.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/backing-dev.h>
#include "internal.h"

#include <trace/events/mmcio.h>

#define VALID_FLAGS (SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE| \
			SYNC_FILE_RANGE_WAIT_AFTER)

static DEFINE_MUTEX(afsync_lock);
static LIST_HEAD(afsync_list);
static struct workqueue_struct *fsync_workqueue = NULL;
struct fsync_work {
	struct work_struct work;
	char pathname[256];
	struct list_head list;
};

static int __sync_filesystem(struct super_block *sb, int wait)
{
	if (wait)
		sync_inodes_sb(sb);
	else
		writeback_inodes_sb(sb, WB_REASON_SYNC);

	if (sb->s_op->sync_fs)
		sb->s_op->sync_fs(sb, wait);
	return __sync_blockdev(sb->s_bdev, wait);
}

int sync_filesystem(struct super_block *sb)
{
	int ret;

	WARN_ON(!rwsem_is_locked(&sb->s_umount));

	if (sb->s_flags & MS_RDONLY)
		return 0;

	ret = __sync_filesystem(sb, 0);
	if (ret < 0)
		return ret;
	return __sync_filesystem(sb, 1);
}
EXPORT_SYMBOL_GPL(sync_filesystem);

static void sync_inodes_one_sb(struct super_block *sb, void *arg)
{
	if (!(sb->s_flags & MS_RDONLY))
		sync_inodes_sb(sb);
}

static void sync_fs_one_sb(struct super_block *sb, void *arg)
{
	if (!(sb->s_flags & MS_RDONLY) && sb->s_op->sync_fs)
		sb->s_op->sync_fs(sb, *(int *)arg);
}

static void fdatawrite_one_bdev(struct block_device *bdev, void *arg)
{
	filemap_fdatawrite(bdev->bd_inode->i_mapping);
}

static void fdatawait_one_bdev(struct block_device *bdev, void *arg)
{
	filemap_fdatawait(bdev->bd_inode->i_mapping);
}

SYSCALL_DEFINE0(sync)
{
	int nowait = 0, wait = 1;
	trace_sys_sync(0);

	wakeup_flusher_threads(0, WB_REASON_SYNC);
	iterate_supers(sync_inodes_one_sb, NULL);
	iterate_supers(sync_fs_one_sb, &nowait);
	iterate_supers(sync_fs_one_sb, &wait);
	iterate_bdevs(fdatawrite_one_bdev, NULL);
	iterate_bdevs(fdatawait_one_bdev, NULL);
	if (unlikely(laptop_mode))
		laptop_sync_completion();
	trace_sys_sync_done(0);
	return 0;
}

static void do_sync_work(struct work_struct *work)
{
	int nowait = 0;

	iterate_supers(sync_inodes_one_sb, &nowait);
	iterate_supers(sync_fs_one_sb, &nowait);
	iterate_bdevs(fdatawrite_one_bdev, NULL);
	iterate_supers(sync_inodes_one_sb, &nowait);
	iterate_supers(sync_fs_one_sb, &nowait);
	iterate_bdevs(fdatawrite_one_bdev, NULL);
	printk("Emergency Sync complete\n");
	kfree(work);
}

void emergency_sync(void)
{
	struct work_struct *work;

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK(work, do_sync_work);
		schedule_work(work);
	}
}

SYSCALL_DEFINE1(syncfs, int, fd)
{
	struct fd f = fdget(fd);
	struct super_block *sb;
	int ret;

	if (!f.file)
		return -EBADF;
	sb = f.file->f_dentry->d_sb;

	down_read(&sb->s_umount);
	ret = sync_filesystem(sb);
	up_read(&sb->s_umount);

	fdput(f);
	return ret;
}

/**
 * vfs_fsync_range - helper to sync a range of data & metadata to disk
 * @file:		file to sync
 * @start:		offset in bytes of the beginning of data range to sync
 * @end:		offset in bytes of the end of data range (inclusive)
 * @datasync:		perform only datasync
 *
 * Write back data in range @start..@end and metadata for @file to disk.  If
 * @datasync is set only metadata needed to access modified file data is
 * written.
 */
int vfs_fsync_range(struct file *file, loff_t start, loff_t end, int datasync)
{
	int err;
	if (!file->f_op || !file->f_op->fsync)
		return -EINVAL;
	trace_vfs_fsync(file);
	err = file->f_op->fsync(file, start, end, datasync);
	trace_vfs_fsync_done(file);
	return err;
}
EXPORT_SYMBOL(vfs_fsync_range);

/**
 * vfs_fsync - perform a fsync or fdatasync on a file
 * @file:		file to sync
 * @datasync:		only perform a fdatasync operation
 *
 * Write back data and metadata for @file to disk.  If @datasync is
 * set only metadata needed to access modified file data is written.
 */
int vfs_fsync(struct file *file, int datasync)
{
	return vfs_fsync_range(file, 0, LLONG_MAX, datasync);
}
EXPORT_SYMBOL(vfs_fsync);

static int async_fsync(struct file *file)
{
	struct inode *inode = file->f_mapping->host;
	struct super_block *sb = inode->i_sb;
	if ((sb->fsync_flags & FLAG_ASYNC_FSYNC) == 0)
		return 0;

	if (file->f_path.dentry->d_parent &&
		!strcmp(file->f_path.dentry->d_parent->d_name.name, "htclog"))
		return 1;

	return 0;
}

static int do_async_fsync(char *pathname)
{
	struct file *file;
	int ret;
	file = filp_open(pathname, O_RDWR, 0);
	if (IS_ERR(file)) {
		pr_debug("%s: can't open %s\n", __func__, pathname);
		return -EBADF;
	}
	ret = vfs_fsync(file, 0);

	filp_close(file, NULL);
	return ret;
}

static void do_afsync_work(struct work_struct *work)
{
	struct fsync_work *fwork =
		container_of(work, struct fsync_work, work);
	int ret = -EBADF;
	pr_debug("afsync: %s\n", fwork->pathname);
	mutex_lock(&afsync_lock);
	list_del(&fwork->list);
	mutex_unlock(&afsync_lock);
	ret = do_async_fsync(fwork->pathname);
	if (ret != 0 && ret != -EBADF)
		pr_info("afsync return %d\n", ret);
	else
		pr_debug("afsync: %s done\n", fwork->pathname);
	kfree(fwork);
}

static int do_fsync(unsigned int fd, int datasync)
{
	struct fd f = fdget(fd);
	int ret = -EBADF;
	ktime_t fsync_t, fsync_diff;
	struct fsync_work *fwork, *tmp;
	char pathname[256], *path;

	if (f.file) {
		path = d_path(&(f.file->f_path), pathname, sizeof(pathname));
		if (IS_ERR(path))
			path = "(unknown)";
		else if (async_fsync(f.file)) {
			if (!fsync_workqueue)
				fsync_workqueue = create_singlethread_workqueue("fsync");
			if (!fsync_workqueue)
				goto no_async;

			if (IS_ERR(path))
				goto no_async;

			mutex_lock(&afsync_lock);
			list_for_each_entry_safe(fwork, tmp, &afsync_list, list) {
				if (!strcmp(fwork->pathname, path)) {
					if (list_empty(&fwork->work.entry)) {
						pr_debug("fsync(%s): work(%s) not in workqueue\n",
								current->comm, path);
						list_del_init(&fwork->list);
						break;
					}
					
					mutex_unlock(&afsync_lock);
					fdput(f);
					return 0;
				}
			}
			mutex_unlock(&afsync_lock);

			fwork = kmalloc(sizeof(*fwork), GFP_KERNEL);
			if (fwork) {
				strncpy(fwork->pathname, path, sizeof(fwork->pathname) - 1);
				pr_debug("fsync(%s): %s, fsize %llu Bytes\n", current->comm,
					fwork->pathname, f.file->f_path.dentry->d_inode->i_size);
				INIT_WORK(&fwork->work, do_afsync_work);
				mutex_lock(&afsync_lock);
				list_add_tail(&fwork->list, &afsync_list);
				mutex_unlock(&afsync_lock);
				queue_work(fsync_workqueue, &fwork->work);
				fdput(f);
				return 0;
			}
		}
no_async:
		fsync_t = ktime_get();
		ret = vfs_fsync(f.file, datasync);
		fdput(f);

		fsync_diff = ktime_sub(ktime_get(), fsync_t);
		if (ktime_to_ms(fsync_diff) >= 5000) {
			pr_info("VFS: %s pid:%d(%s)(parent:%d/%s) takes %lld ms to fsync %s.\n", __func__,
				current->pid, current->comm, current->parent->pid, current->parent->comm,
				ktime_to_ms(fsync_diff), path);
		}
	}
	return ret;
}

SYSCALL_DEFINE1(fsync, unsigned int, fd)
{
	return do_fsync(fd, 0);
}

SYSCALL_DEFINE1(fdatasync, unsigned int, fd)
{
	return do_fsync(fd, 1);
}

int generic_write_sync(struct file *file, loff_t pos, loff_t count)
{
	if (!(file->f_flags & O_DSYNC) && !IS_SYNC(file->f_mapping->host))
		return 0;
	return vfs_fsync_range(file, pos, pos + count - 1,
			       (file->f_flags & __O_SYNC) ? 0 : 1);
}
EXPORT_SYMBOL(generic_write_sync);

SYSCALL_DEFINE4(sync_file_range, int, fd, loff_t, offset, loff_t, nbytes,
				unsigned int, flags)
{
	int ret;
	struct fd f;
	struct address_space *mapping;
	loff_t endbyte;			
	umode_t i_mode;

	ret = -EINVAL;
	if (flags & ~VALID_FLAGS)
		goto out;

	endbyte = offset + nbytes;

	if ((s64)offset < 0)
		goto out;
	if ((s64)endbyte < 0)
		goto out;
	if (endbyte < offset)
		goto out;

	if (sizeof(pgoff_t) == 4) {
		if (offset >= (0x100000000ULL << PAGE_CACHE_SHIFT)) {
			ret = 0;
			goto out;
		}
		if (endbyte >= (0x100000000ULL << PAGE_CACHE_SHIFT)) {
			nbytes = 0;
		}
	}

	if (nbytes == 0)
		endbyte = LLONG_MAX;
	else
		endbyte--;		

	ret = -EBADF;
	f = fdget(fd);
	if (!f.file)
		goto out;

	i_mode = file_inode(f.file)->i_mode;
	ret = -ESPIPE;
	if (!S_ISREG(i_mode) && !S_ISBLK(i_mode) && !S_ISDIR(i_mode) &&
			!S_ISLNK(i_mode))
		goto out_put;

	mapping = f.file->f_mapping;
	if (!mapping) {
		ret = -EINVAL;
		goto out_put;
	}

	ret = 0;
	if (flags & SYNC_FILE_RANGE_WAIT_BEFORE) {
		ret = filemap_fdatawait_range(mapping, offset, endbyte);
		if (ret < 0)
			goto out_put;
	}

	if (flags & SYNC_FILE_RANGE_WRITE) {
		ret = filemap_fdatawrite_range(mapping, offset, endbyte);
		if (ret < 0)
			goto out_put;
	}

	if (flags & SYNC_FILE_RANGE_WAIT_AFTER)
		ret = filemap_fdatawait_range(mapping, offset, endbyte);

out_put:
	fdput(f);
out:
	return ret;
}

SYSCALL_DEFINE4(sync_file_range2, int, fd, unsigned int, flags,
				 loff_t, offset, loff_t, nbytes)
{
	return sys_sync_file_range(fd, offset, nbytes, flags);
}
