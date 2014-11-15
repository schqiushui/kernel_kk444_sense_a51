/* Copyright (c) 2013, HTC Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/htc_debug_tools.h>
#include <mach/devices_cmdline.h>


#define RAMLOG_COMPATIBLE_NAME "htc,bldr_log"
#define RAMLOG_LAST_RSE_NAME "bl_old_log"
#define RAMLOG_CUR_RSE_NAME "bl_log"

char *bl_last_log_buf, *bl_cur_log_buf;
unsigned long bl_last_log_buf_size, bl_cur_log_buf_size;
static void bldr_log_parser(const char *bldr_log, char *bldr_log_buf, unsigned long bldr_log_size, unsigned long *bldr_log_buf_size)
{
	int i, j, k;
	int last_index = 0;
	int line_length = 0;
	const char *bldr_log_ptr = bldr_log;
	char *bldr_log_buf_ptr = bldr_log_buf;
	const char *terminal_pattern = "\r\n";
	const char *specific_pattern = "[HBOOT]";
	int terminal_pattern_len = strlen(terminal_pattern);
	int specific_pattern_len = strlen(specific_pattern);

	if (!get_tamper_sf()) {
		memcpy(bldr_log_buf, bldr_log, bldr_log_size);
		*bldr_log_buf_size = bldr_log_size;
		pr_info("bldr_log_parser: size %ld\n", *bldr_log_buf_size);
		return;
	}

	for (i = 0; i < bldr_log_size; i++) {  
		bool terminal_match = true;

		if ((i + terminal_pattern_len) > bldr_log_size)
			break;

		for (j = 0; j < terminal_pattern_len; j++) {  
			if (bldr_log[i + j] != terminal_pattern[j]) {
				terminal_match = false;
				break;
			}
		}

		if (terminal_match) {  
			bool specific_match = true;
			int specific_pattern_start = i - specific_pattern_len;
			line_length = i + terminal_pattern_len - last_index;

			for (k = 0; k < specific_pattern_len; k++) {  
				if (bldr_log[specific_pattern_start + k] != specific_pattern[k]) {
					specific_match = false;
					break;
				}
			}

			if (specific_match) {  
				
				memcpy(bldr_log_buf_ptr, bldr_log_ptr, line_length - terminal_pattern_len - specific_pattern_len);
				bldr_log_buf_ptr += (line_length - terminal_pattern_len - specific_pattern_len);
				memcpy(bldr_log_buf_ptr, terminal_pattern, terminal_pattern_len);
				bldr_log_buf_ptr += terminal_pattern_len;
			}

			bldr_log_ptr += line_length;
			last_index = i + terminal_pattern_len;
		}
	}

	*bldr_log_buf_size = bldr_log_buf_ptr - bldr_log_buf;
	pr_info("bldr_log_parser: size %ld\n", *bldr_log_buf_size);
}

static bool bldr_rst_msg_parser(char *bldr_log_buf, unsigned long bldr_log_buf_size, bool is_last_bldr)
{
	int i, j, k;
	const char *ramdump_pattern_rst = "ramdump_show_rst_msg:";
	const char *ramdump_pattern_vib = "###[ RAMDUMP Mode ]###";
	const char *ramdump_pattern_real = "ramdump_real_rst_msg:";
	int ramdump_pattern_rst_len = strlen(ramdump_pattern_rst);
	int ramdump_pattern_vib_len = strlen(ramdump_pattern_vib);
	int ramdump_pattern_real_len = strlen(ramdump_pattern_real);
	char *bldr_ramdump_pattern_rst_buf_ptr = NULL;
	bool is_ramdump_mode = false;
	bool found_ramdump_pattern_rst = false;

	for (i = 0; i < bldr_log_buf_size; i++) {  
		bool ramdump_pattern_rst_match = true;
		bool ramdump_pattern_vib_match = true;

		if (!found_ramdump_pattern_rst &&
			(i + ramdump_pattern_rst_len) <= bldr_log_buf_size) {
			for (j = 0; j < ramdump_pattern_rst_len; j++) {  
				if (bldr_log_buf[i+j] != ramdump_pattern_rst[j]) {
					ramdump_pattern_rst_match = false;
					break;
				}
			}

			if (ramdump_pattern_rst_match) {  
				if (is_last_bldr)
					bldr_ramdump_pattern_rst_buf_ptr = bldr_log_buf+i;
				else
					memcpy(bldr_log_buf + i, ramdump_pattern_real, ramdump_pattern_real_len);
				found_ramdump_pattern_rst = true;
			}
		}

		if (!is_ramdump_mode &&
			(i + ramdump_pattern_vib_len) <= bldr_log_buf_size &&
			is_last_bldr) {
			for (k = 0; k < ramdump_pattern_vib_len; k++) {  
				if (bldr_log_buf[i + k] != ramdump_pattern_vib[k]) {
					ramdump_pattern_vib_match = false;
					break;
				}
			}

			if (ramdump_pattern_vib_match) 
				is_ramdump_mode = true;
		}

		if (found_ramdump_pattern_rst &&
			is_ramdump_mode &&
			is_last_bldr) {  
			memcpy(bldr_ramdump_pattern_rst_buf_ptr, ramdump_pattern_real, ramdump_pattern_real_len);
			break;
		}
	}

	if (is_last_bldr)
		return is_ramdump_mode;
	else
		return found_ramdump_pattern_rst;
}

ssize_t bldr_log_read(const void *lastk_buf, ssize_t	lastk_size, char __user *userbuf,
						size_t count, loff_t *ppos)
{
	loff_t pos;
	unsigned long total;
	ssize_t len;

	pos = *ppos;
	total = lastk_size + bl_last_log_buf_size + bl_cur_log_buf_size;
	if (pos < bl_last_log_buf_size) {
		len = simple_read_from_buffer(userbuf, count, &pos, bl_last_log_buf, bl_last_log_buf_size);
	} else if (pos < lastk_size + bl_last_log_buf_size) {
		pos -= bl_last_log_buf_size;
		len = simple_read_from_buffer(userbuf, count, &pos, lastk_buf, lastk_size);
	} else {
		pos -= (bl_last_log_buf_size + lastk_size);
		len = simple_read_from_buffer(userbuf, count, &pos, bl_cur_log_buf, bl_cur_log_buf_size);
	}

	if (len > 0)
		*ppos += len;

	return len;
}

int bldr_log_setup(phys_addr_t bldr_phy_addr, size_t bldr_log_size, bool is_last_bldr)
{
	bool is_last_bldr_ramdump_mode = false;
	char *bldr_base;
	int ret = 0;

	if (!bldr_log_size) {
		ret = EINVAL;
		goto _out;
	}

	bldr_base = ioremap(bldr_phy_addr, bldr_log_size);
	if (!bldr_base) {
		pr_warn("%s: failed to map last bootloader log buffer\n", __func__);
		ret = -ENOMEM;
		goto _out;
	}

	if (is_last_bldr) {
		bl_last_log_buf = kmalloc(bldr_log_size, GFP_KERNEL);

		if (!bl_last_log_buf) {
			pr_warn("%s: failed to alloc last bootloader log buffer\n", __func__);
			ret = -ENOMEM;
			goto _unmap;
		} else {
			pr_info("hb_ramlog: allocate buffer for last bootloader log, size: %zu\n", bldr_log_size);
			bldr_log_parser(bldr_base, bl_last_log_buf, bldr_log_size, &bl_last_log_buf_size);
			is_last_bldr_ramdump_mode = bldr_rst_msg_parser(bl_last_log_buf, bl_last_log_buf_size, true);
			if (is_last_bldr_ramdump_mode)
				pr_info("hb_ramlog: Found abnormal rst_msg pattern in last bootloader log\n");
		}
	} else {
		bl_cur_log_buf = kmalloc(bldr_log_size, GFP_KERNEL);

		if (!bl_cur_log_buf) {
			pr_warn("%s: failed to alloc last bootloader log buffer\n", __func__);
			goto _unmap;
		} else {
			pr_info("hb_ramlog: allocate buffer for bootloader log, size: %zu\n", bldr_log_size);
			bldr_log_parser(bldr_base, bl_cur_log_buf, bldr_log_size, &bl_cur_log_buf_size);
			if (!is_last_bldr_ramdump_mode) {
				if (bldr_rst_msg_parser(bl_cur_log_buf, bl_cur_log_buf_size, false))
					pr_info("hb_ramlog: Found abnormal rst_msg pattern in bootloader log\n");
			}
		}
	}


_unmap:
	iounmap(bldr_base);
_out:
	return ret;
}

int bldr_log_init(void)
{
	struct device_node *np;
	struct resource temp_res;
	int num_reg = 0;

	np = of_find_compatible_node(NULL, NULL, RAMLOG_COMPATIBLE_NAME);

	if (np) {
		if (of_can_translate_address(np)) {
			while (of_address_to_resource(np, num_reg, &temp_res) == 0) {
				if (!strcmp(temp_res.name, RAMLOG_LAST_RSE_NAME))
					bldr_log_setup(temp_res.start, resource_size(&temp_res), true);
				else if (!strcmp(temp_res.name, RAMLOG_CUR_RSE_NAME))
					bldr_log_setup(temp_res.start, resource_size(&temp_res), false);
				else
					pr_warn("%s: unknown bldr resource %s\n", __func__, temp_res.name);

				num_reg++;
			}
			if (!num_reg)
				pr_warn("%s: can't find address resource\n", __func__);
		} else
			pr_warn("%s: translate address fail\n", __func__);
	} else
		pr_warn("%s: can't find compatible '%s'\n", __func__, RAMLOG_COMPATIBLE_NAME);

	return num_reg;
}

void bldr_log_release(void)
{
	if (bl_last_log_buf)
		kfree(bl_last_log_buf);

	if (bl_cur_log_buf)
		kfree(bl_cur_log_buf);
}
