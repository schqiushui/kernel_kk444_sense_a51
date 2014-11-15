/*
 * Copyright (C) 2010 HTC, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/board.h>

#define PROCNAME "driver/hdf"
#define FLAG_LEN 64
static char htc_debug_flag[FLAG_LEN+1];

static int htc_debug_read(struct seq_file *m, void *v)
{

	seq_printf(m, "0X%s\n",htc_debug_flag);

	return 0;
}

static ssize_t htc_debug_write(struct file *file, const char __user *buffer,
		                size_t count, loff_t *ppos)
{
	char buf[FLAG_LEN+3];

	

	if (count != sizeof(buf)){
        
        return -EFAULT;
    }

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

        memset(htc_debug_flag,0,FLAG_LEN+1);
	memcpy(htc_debug_flag,buf+2,FLAG_LEN);
	
	
	return count;
}

static int htc_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, htc_debug_read, NULL);
}

static const struct file_operations htc_debug_fops = {
	.owner		= THIS_MODULE,
	.open		= htc_debug_open,
	.write		= htc_debug_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init sysinfo_proc_init(void)
{
	struct proc_dir_entry *entry = NULL;

	

	
	entry = proc_create_data(PROCNAME, 0660, NULL, &htc_debug_fops, NULL);
	if (entry == NULL) {
		
		return -ENOMEM;
	}

	return 0;
}

module_init(sysinfo_proc_init);
MODULE_AUTHOR("Medad Chang <medad_chang@htc.com>");
MODULE_DESCRIPTION("HTC Debug Interface");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
