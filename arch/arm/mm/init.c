/*
 *  linux/arch/arm/mm/init.c
 *
 *  Copyright (C) 1995-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/export.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>
#include <linux/of_fdt.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/dma-contiguous.h>
#include <linux/sizes.h>
#include <linux/sort.h>

#include <asm/mach-types.h>
#include <asm/memblock.h>
#include <asm/prom.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/tlb.h>
#include <asm/fixmap.h>
#include <asm/cputype.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#ifdef CONFIG_MSM_KGSL
#include <linux/msm_kgsl.h>
#endif
#ifdef CONFIG_ION
#include <linux/msm_ion.h>
#endif

#include "mm.h"

static unsigned long phys_initrd_start __initdata = 0;
static unsigned long phys_initrd_size __initdata = 0;
int msm_krait_need_wfe_fixup;
EXPORT_SYMBOL(msm_krait_need_wfe_fixup);

static int __init early_initrd(char *p)
{
	unsigned long start, size;
	char *endp;

	start = memparse(p, &endp);
	if (*endp == ',') {
		size = memparse(endp + 1, NULL);

		phys_initrd_start = start;
		phys_initrd_size = size;
	}
	return 0;
}
early_param("initrd", early_initrd);

static int __init parse_tag_initrd(const struct tag *tag)
{
	printk(KERN_WARNING "ATAG_INITRD is deprecated; "
		"please update your bootloader.\n");
	phys_initrd_start = __virt_to_phys(tag->u.initrd.start);
	phys_initrd_size = tag->u.initrd.size;
	return 0;
}

__tagtable(ATAG_INITRD, parse_tag_initrd);

static int __init parse_tag_initrd2(const struct tag *tag)
{
	phys_initrd_start = tag->u.initrd.start;
	phys_initrd_size = tag->u.initrd.size;
	return 0;
}

__tagtable(ATAG_INITRD2, parse_tag_initrd2);

#ifdef CONFIG_OF_FLATTREE
void __init early_init_dt_setup_initrd_arch(u64 start, u64 end)
{
	phys_initrd_start = start;
	phys_initrd_size = end - start;
}
#endif 

void show_mem(unsigned int filter)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, slab = 0;
	struct memblock_region *reg;

#ifdef CONFIG_MSM_KGSL
	unsigned long kgsl_alloc = kgsl_get_alloc_size(true);
#endif
#ifdef CONFIG_ION
	uintptr_t ion_alloc = msm_ion_heap_meminfo(true);
	uintptr_t ion_inuse = msm_ion_heap_meminfo(false);
#endif

	printk("Mem-info:\n");
	show_free_areas(filter);

	if (filter & SHOW_MEM_FILTER_PAGE_COUNT)
		return;

	for_each_memblock(memory, reg) {
		unsigned int pfn1, pfn2;
		struct page *page, *end;

		pfn1 = memblock_region_memory_base_pfn(reg);
		pfn2 = memblock_region_memory_end_pfn(reg);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (PageSlab(page))
				slab++;
			else if (!page_count(page))
				free++;
			else
				shared += page_count(page) - 1;
			page++;
#ifdef CONFIG_SPARSEMEM
			pfn1++;
			if (!(pfn1 % PAGES_PER_SECTION))
				page = pfn_to_page(pfn1);
		} while (pfn1 < pfn2);
#else
		} while (page < end);
#endif
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d slab pages\n", slab);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
#ifdef CONFIG_MSM_KGSL
	printk("KGSL_ALLOC: %8lu kB\n", kgsl_alloc >> 10);
#endif
#ifdef CONFIG_ION
	printk("ION_TOTAL: %8lu kB\n", ion_alloc >> 10);
	printk("ION_INUSE: %8lu kB\n", ion_inuse >> 10);
#endif
}

static void __init find_limits(unsigned long *min, unsigned long *max_low,
			       unsigned long *max_high)
{
	*max_low = PFN_DOWN(memblock_get_current_limit());
	*min = PFN_UP(memblock_start_of_DRAM());
	*max_high = PFN_DOWN(memblock_end_of_DRAM());
}

static void __init arm_bootmem_init(unsigned long start_pfn,
	unsigned long end_pfn)
{
	struct memblock_region *reg;
	unsigned int boot_pages;
	phys_addr_t bitmap;
	pg_data_t *pgdat;

	boot_pages = bootmem_bootmap_pages(end_pfn - start_pfn);
	bitmap = memblock_alloc_base(boot_pages << PAGE_SHIFT, L1_CACHE_BYTES,
				__pfn_to_phys(end_pfn));

	node_set_online(0);
	pgdat = NODE_DATA(0);
	init_bootmem_node(pgdat, __phys_to_pfn(bitmap), start_pfn, end_pfn);

	
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (end >= end_pfn)
			end = end_pfn;
		if (start >= end)
			break;

		free_bootmem(__pfn_to_phys(start), (end - start) << PAGE_SHIFT);
	}

	
	for_each_memblock(reserved, reg) {
		unsigned long start = memblock_region_reserved_base_pfn(reg);
		unsigned long end = memblock_region_reserved_end_pfn(reg);

		if (end >= end_pfn)
			end = end_pfn;
		if (start >= end)
			break;

		reserve_bootmem(__pfn_to_phys(start),
			        (end - start) << PAGE_SHIFT, BOOTMEM_DEFAULT);
	}
}

#ifdef CONFIG_ZONE_DMA

unsigned long arm_dma_zone_size __read_mostly;
EXPORT_SYMBOL(arm_dma_zone_size);

phys_addr_t arm_dma_limit;

static void __init arm_adjust_dma_zone(unsigned long *size, unsigned long *hole,
	unsigned long dma_size)
{
	if (size[0] <= dma_size)
		return;

	size[ZONE_NORMAL] = size[0] - dma_size;
	size[ZONE_DMA] = dma_size;
	hole[ZONE_NORMAL] = hole[0];
	hole[ZONE_DMA] = 0;
}
#endif

void __init setup_dma_zone(struct machine_desc *mdesc)
{
#ifdef CONFIG_ZONE_DMA
	if (mdesc->dma_zone_size) {
		arm_dma_zone_size = mdesc->dma_zone_size;
		arm_dma_limit = PHYS_OFFSET + arm_dma_zone_size - 1;
	} else
		arm_dma_limit = 0xffffffff;
#endif
}

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
static void __init arm_bootmem_free_hmnm(unsigned long max_low,
	unsigned long max_high)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];
	struct memblock_region *reg;

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

	max_zone_pfns[0] = max_low;
#ifdef CONFIG_HIGHMEM
	max_zone_pfns[ZONE_HIGHMEM] = max_high;
#endif
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		memblock_set_node(PFN_PHYS(start), PFN_PHYS(end - start), 0);
	}
	free_area_init_nodes(max_zone_pfns);
}

#else
static void __init arm_bootmem_free(unsigned long min, unsigned long max_low,
	unsigned long max_high)
{
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	struct memblock_region *reg;

	memset(zone_size, 0, sizeof(zone_size));

	zone_size[0] = max_low - min;
#ifdef CONFIG_HIGHMEM
	zone_size[ZONE_HIGHMEM] = max_high - max_low;
#endif

	memcpy(zhole_size, zone_size, sizeof(zhole_size));
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start < max_low) {
			unsigned long low_end = min(end, max_low);
			zhole_size[0] -= low_end - start;
		}
#ifdef CONFIG_HIGHMEM
		if (end > max_low) {
			unsigned long high_start = max(start, max_low);
			zhole_size[ZONE_HIGHMEM] -= end - high_start;
		}
#endif
	}

#ifdef CONFIG_ZONE_DMA
	if (arm_dma_zone_size)
		arm_adjust_dma_zone(zone_size, zhole_size,
			arm_dma_zone_size >> PAGE_SHIFT);
#endif

	free_area_init_node(0, zone_size, min, zhole_size);
}
#endif

#ifdef CONFIG_HAVE_ARCH_PFN_VALID
int pfn_valid(unsigned long pfn)
{
	return memblock_is_memory(__pfn_to_phys(pfn));
}
EXPORT_SYMBOL(pfn_valid);
#endif

#ifndef CONFIG_SPARSEMEM
static void __init arm_memory_present(void)
{
}
#else
static void __init arm_memory_present(void)
{
	struct meminfo *mi = &meminfo;
	int i;
	for_each_bank(i, mi) {
		memory_present(0, bank_pfn_start(&mi->bank[i]),
				bank_pfn_end(&mi->bank[i]));
	}
}
#endif

static bool arm_memblock_steal_permitted = true;

phys_addr_t __init arm_memblock_steal(phys_addr_t size, phys_addr_t align)
{
	phys_addr_t phys;

	BUG_ON(!arm_memblock_steal_permitted);

	phys = memblock_alloc_base(size, align, MEMBLOCK_ALLOC_ANYWHERE);
	memblock_free(phys, size);
	memblock_remove(phys, size);

	return phys;
}

void __init arm_memblock_init(struct machine_desc *mdesc)
{
	
#ifdef CONFIG_XIP_KERNEL
	memblock_reserve(__pa(_sdata), _end - _sdata);
#else
	memblock_reserve(__pa(_stext), _end - _stext);
#endif
#ifdef CONFIG_BLK_DEV_INITRD
	if (phys_initrd_size &&
	    !memblock_is_region_memory(phys_initrd_start, phys_initrd_size)) {
		pr_err("INITRD: 0x%08lx+0x%08lx is not a memory region - disabling initrd\n",
		       phys_initrd_start, phys_initrd_size);
		phys_initrd_start = phys_initrd_size = 0;
	}
	if (phys_initrd_size &&
	    memblock_is_region_reserved(phys_initrd_start, phys_initrd_size)) {
		pr_err("INITRD: 0x%08lx+0x%08lx overlaps in-use memory region - disabling initrd\n",
		       phys_initrd_start, phys_initrd_size);
		phys_initrd_start = phys_initrd_size = 0;
	}
	if (phys_initrd_size) {
		memblock_reserve(phys_initrd_start, phys_initrd_size);

		
		initrd_start = __phys_to_virt(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
#endif

	arm_mm_memblock_reserve();
	arm_dt_memblock_reserve();

	
	if (mdesc->reserve)
		mdesc->reserve();

	dma_contiguous_reserve(min(arm_dma_limit, arm_lowmem_limit));

	arm_memblock_steal_permitted = false;
	memblock_allow_resize();
	memblock_dump_all();
}

#ifdef CONFIG_MEMORY_HOTPLUG_SPARSE
int _early_pfn_valid(unsigned long pfn)
{
	struct meminfo *mi = &meminfo;
	unsigned int left = 0, right = mi->nr_banks;

	do {
		unsigned int mid = (right + left) / 2;
		struct membank *bank = &mi->bank[mid];

		if (pfn < bank_pfn_start(bank))
			right = mid;
		else if (pfn >= bank_pfn_end(bank))
			left = mid + 1;
		else
			return 1;
	} while (left < right);
	return 0;
}
EXPORT_SYMBOL(_early_pfn_valid);
#endif

void __init bootmem_init(void)
{
	unsigned long min, max_low, max_high;

	max_low = max_high = 0;

	find_limits(&min, &max_low, &max_high);

	arm_bootmem_init(min, max_low);

	arm_memory_present();

	sparse_init();

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	arm_bootmem_free_hmnm(max_low, max_high);
#else
	arm_bootmem_free(min, max_low, max_high);
#endif

	max_low_pfn = max_low - PHYS_PFN_OFFSET;
	max_pfn = max_high - PHYS_PFN_OFFSET;
}

static inline void poison_init_mem(void *s, size_t count)
{
	u32 *p = (u32 *)s;
	for (; count != 0; count -= 4)
		*p++ = 0xe7fddef0;
}

static inline void
free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	unsigned long pg, pgend;

	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg = pfn_to_page(end_pfn - 1) + 1;

	pg = (unsigned long)PAGE_ALIGN(__pa(start_pg));
	pgend = (unsigned long)__pa(end_pg) & PAGE_MASK;

	if (pg < pgend)
		free_bootmem(pg, pgend - pg);
}

static void __init free_unused_memmap(void)
{
	unsigned long start, prev_end = 0;
	struct memblock_region *reg;

	for_each_memblock(memory, reg) {
		start = memblock_region_memory_base_pfn(reg);

#ifdef CONFIG_SPARSEMEM
		start = min(start,
				 ALIGN(prev_end, PAGES_PER_SECTION));
#else
		start = round_down(start, MAX_ORDER_NR_PAGES);
#endif
		if (prev_end && prev_end < start)
			free_memmap(prev_end, start);

		prev_end = ALIGN(memblock_region_memory_end_pfn(reg),
				 MAX_ORDER_NR_PAGES);
	}

#ifdef CONFIG_SPARSEMEM
	if (!IS_ALIGNED(prev_end, PAGES_PER_SECTION))
		free_memmap(prev_end,
			    ALIGN(prev_end, PAGES_PER_SECTION));
#endif
}

#ifdef CONFIG_HIGHMEM
static inline void free_area_high(unsigned long pfn, unsigned long end)
{
	for (; pfn < end; pfn++)
		free_highmem_page(pfn_to_page(pfn));
}
#endif

static void __init free_highpages(void)
{
#ifdef CONFIG_HIGHMEM
	unsigned long max_low = max_low_pfn + PHYS_PFN_OFFSET;
	struct memblock_region *mem, *res;

	
	for_each_memblock(memory, mem) {
		unsigned long start = memblock_region_memory_base_pfn(mem);
		unsigned long end = memblock_region_memory_end_pfn(mem);

		
		if (end <= max_low)
			continue;

		
		if (start < max_low)
			start = max_low;

		
		for_each_memblock(reserved, res) {
			unsigned long res_start, res_end;

			res_start = memblock_region_reserved_base_pfn(res);
			res_end = memblock_region_reserved_end_pfn(res);

			if (res_end < start)
				continue;
			if (res_start < start)
				res_start = start;
			if (res_start > end)
				res_start = end;
			if (res_end > end)
				res_end = end;
			if (res_start != start)
				free_area_high(start, res_start);
			start = res_end;
			if (start == end)
				break;
		}

		
		if (start < end)
			free_area_high(start, end);
	}
#endif
}

#define MLK(b, t) b, t, ((t) - (b)) >> 10
#define MLM(b, t) b, t, ((t) - (b)) >> 20
#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

#ifdef CONFIG_ENABLE_VMALLOC_SAVING
static void print_vmalloc_lowmem_info(void)
{
	struct memblock_region *reg, *prev_reg = NULL;

	for_each_memblock_rev(memory, reg) {
		phys_addr_t start_phys = reg->base;
		phys_addr_t end_phys = reg->base + reg->size;

		if (prev_reg == NULL) {
			prev_reg = reg;
			if (end_phys > arm_lowmem_limit) {

				if (start_phys < arm_lowmem_limit) {
					pr_notice(
					"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
					MLM(
					(unsigned long)__va(arm_lowmem_limit),
					VMALLOC_END));

					pr_notice(
					"	   lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n",
					MLM((unsigned long)__va(start_phys),
					(unsigned long)__va(arm_lowmem_limit)));
				} else {
					pr_notice(
					"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
					MLM((unsigned long)__va(start_phys),
					(unsigned long)__va(end_phys)));

				}
			} else {

				pr_notice(
				"	   lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n",
				MLM((unsigned long)__va(start_phys),
				(unsigned long)__va(end_phys)));
			}

			continue;
		}

		start_phys = reg->base + reg->size;
		end_phys = prev_reg->base;

		pr_notice(
		"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
		MLM((unsigned long)__va(start_phys),
		(unsigned long)__va(end_phys)));


		if (end_phys > arm_lowmem_limit) {

			if (start_phys < arm_lowmem_limit) {
				pr_notice(
				"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
				MLM((unsigned long)__va(arm_lowmem_limit),
				VMALLOC_END));

				pr_notice(
				"	   lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n",
				MLM((unsigned long)__va(start_phys),
				(unsigned long)__va(arm_lowmem_limit)));
			} else {
				pr_notice(
				"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
				MLM((unsigned long)__va(start_phys),
				(unsigned long)__va(end_phys)));

			}
		} else {
				start_phys = reg->base;
				end_phys = reg->base + reg->size;
				pr_notice(
				"	   lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n",
				MLM((unsigned long)__va(start_phys),
				(unsigned long)__va(end_phys)));
		}
				prev_reg = reg;
	}

}
#endif

void __init mem_init(void)
{
#ifdef CONFIG_HAVE_TCM
	
	extern u32 dtcm_end;
	extern u32 itcm_end;
#endif

	max_mapnr   = pfn_to_page(max_pfn + PHYS_PFN_OFFSET) - mem_map;

	
	free_unused_memmap();
	free_all_bootmem();

#ifdef CONFIG_SA1111
	
	free_reserved_area(__va(PHYS_OFFSET), swapper_pg_dir, 0, NULL);
#endif

	free_highpages();

	mem_init_print_info(NULL);

	printk(KERN_NOTICE "Virtual kernel memory layout:\n"
			"    vector  : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#ifdef CONFIG_HAVE_TCM
			"    DTCM    : 0x%08lx - 0x%08lx   (%4ld kB)\n"
			"    ITCM    : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#endif
			"    fixmap  : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			MLK(UL(CONFIG_VECTORS_BASE), UL(CONFIG_VECTORS_BASE) +
				(PAGE_SIZE)),
#ifdef CONFIG_HAVE_TCM
			MLK(DTCM_OFFSET, (unsigned long) dtcm_end),
			MLK(ITCM_OFFSET, (unsigned long) itcm_end),
#endif
			MLK(FIXADDR_START, FIXADDR_TOP));
#ifdef CONFIG_ENABLE_VMALLOC_SAVING
	print_vmalloc_lowmem_info();
#else
	printk(KERN_NOTICE
		   "    vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n"
		   "    lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n",
		   MLM(VMALLOC_START, VMALLOC_END),
		   MLM(PAGE_OFFSET, (unsigned long)high_memory));
#endif
	printk(KERN_NOTICE
#ifdef CONFIG_HIGHMEM
		   "    pkmap   : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#endif
#ifdef CONFIG_MODULES
		   "    modules : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#endif
		   "      .text : 0x%p" " - 0x%p" "   (%4d kB)\n"
		   "      .init : 0x%p" " - 0x%p" "   (%4d kB)\n"
		   "      .data : 0x%p" " - 0x%p" "   (%4d kB)\n"
		   "       .bss : 0x%p" " - 0x%p" "   (%4d kB)\n",
#ifdef CONFIG_HIGHMEM
		   MLM(PKMAP_BASE, (PKMAP_BASE) + (LAST_PKMAP) *
				(PAGE_SIZE)),
#endif
#ifdef CONFIG_MODULES
		   MLM(MODULES_VADDR, MODULES_END),
#endif

		   MLK_ROUNDUP(_text, _etext),
		   MLK_ROUNDUP(__init_begin, __init_end),
		   MLK_ROUNDUP(_sdata, _edata),
		   MLK_ROUNDUP(__bss_start, __bss_stop));

#ifdef CONFIG_MMU
	BUILD_BUG_ON(TASK_SIZE				> MODULES_VADDR);
	BUG_ON(TASK_SIZE				> MODULES_VADDR);
#endif

#ifdef CONFIG_HIGHMEM
	BUILD_BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE > PAGE_OFFSET);
	BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE	> PAGE_OFFSET);
#endif

	if (PAGE_SIZE >= 16384 && get_num_physpages() <= 128) {
		extern int sysctl_overcommit_memory;
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

#undef MLK
#undef MLM
#undef MLK_ROUNDUP
void free_initmem(void)
{
	unsigned long reclaimed_initmem;

#ifdef CONFIG_HAVE_TCM
	extern char __tcm_start, __tcm_end;

	poison_init_mem(&__tcm_start, &__tcm_end - &__tcm_start);
	free_reserved_area(&__tcm_start, &__tcm_end, 0, "TCM link");
#endif

#ifdef CONFIG_STRICT_MEMORY_RWX
	poison_init_mem((char *)__arch_info_begin,
		__init_end - (char *)__arch_info_begin);
	reclaimed_initmem = free_reserved_area(
				PAGE_ALIGN((unsigned long)&__arch_info_begin),
				((unsigned long)&__init_end)&PAGE_MASK, 0,
				"unused kernel");
	totalram_pages += reclaimed_initmem;
#else
	poison_init_mem(__init_begin, __init_end - __init_begin);
	if (!machine_is_integrator() && !machine_is_cintegrator()) {
		reclaimed_initmem = free_initmem_default(0);
		totalram_pages += reclaimed_initmem;
	}
#endif
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd;

void free_initrd_mem(unsigned long start, unsigned long end)
{
	unsigned long reclaimed_initrd_mem;

	if (!keep_initrd) {
		poison_init_mem((void *)start, PAGE_ALIGN(end) - start);
		reclaimed_initrd_mem = free_reserved_area(start, end, 0,
				"initrd");
		totalram_pages += reclaimed_initrd_mem;
	}
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif

#ifdef CONFIG_MSM_KRAIT_WFE_FIXUP
static int __init msm_krait_wfe_init(void)
{
	unsigned int val, midr;
	midr = read_cpuid_id() & 0xffffff00;
	if ((midr == 0x511f0400) || (midr == 0x510f0600)) {
		asm volatile("mrc p15, 7, %0, c15, c0, 5" : "=r" (val));
		msm_krait_need_wfe_fixup = (val & 0x10000) ? 1 : 0;
	}
	return 0;
}
pure_initcall(msm_krait_wfe_init);
#endif
