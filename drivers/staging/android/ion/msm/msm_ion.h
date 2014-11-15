#ifndef _MSM_MSM_ION_H
#define _MSM_MSM_ION_H

#include "../ion.h"
#include "../../uapi/linux/msm_ion.h"

enum ion_permission_type {
	IPT_TYPE_MM_CARVEOUT = 0,
	IPT_TYPE_MFC_SHAREDMEM = 1,
	IPT_TYPE_MDP_WRITEBACK = 2,
};

#define ION_IOMMU_UNMAP_DELAYED 1

#define ION_UNSECURE_DELAYED	1

struct ion_cp_heap_pdata {
	enum ion_permission_type permission_type;
	unsigned int align;
	ion_phys_addr_t secure_base; 
	size_t secure_size; 
	int is_cma;
	enum ion_fixed_position fixed_position;
	int iommu_map_all;
	int iommu_2x_map_domain;
	int (*request_ion_region)(void *);
	int (*release_ion_region)(void *);
	void *(*setup_ion_region)(void);
	int allow_nonsecure_alloc;
};

struct ion_co_heap_pdata {
	int adjacent_mem_id;
	unsigned int align;
	enum ion_fixed_position fixed_position;
	int (*request_ion_region)(void *);
	int (*release_ion_region)(void *);
	void *(*setup_ion_region)(void);
};

struct ion_cma_pdata {
	unsigned long default_prefetch_size;
};

#ifdef CONFIG_ION

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name);

int ion_handle_get_flags(struct ion_client *client, struct ion_handle *handle,
				unsigned long *flags);


int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags);



int ion_handle_get_size(struct ion_client *client, struct ion_handle *handle,
			unsigned long *size);

void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num);

int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd);

int msm_ion_secure_buffer(struct ion_client *client, struct ion_handle *handle,
				enum cp_mem_usage usage, int flags);

int msm_ion_unsecure_buffer(struct ion_client *client,
				struct ion_handle *handle);

uintptr_t msm_ion_heap_meminfo(const bool is_total);
#else
static inline struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline int ion_map_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags,
			unsigned long iommu_flags)
{
	return -ENODEV;
}

static inline int ion_handle_get_size(struct ion_client *client,
				struct ion_handle *handle, unsigned long *size)
{
	return -ENODEV;
}

static inline void ion_unmap_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num)
{
	return;
}

static inline int msm_ion_do_cache_op(struct ion_client *client,
			struct ion_handle *handle, void *vaddr,
			unsigned long len, unsigned int cmd)
{
	return -ENODEV;
}

static inline int msm_ion_secure_buffer(struct ion_client *client,
					struct ion_handle *handle,
					enum cp_mem_usage usage,
					int flags)
{
	return -ENODEV;
}

static inline int msm_ion_unsecure_buffer(struct ion_client *client,
					struct ion_handle *handle)
{
	return -ENODEV;
}

static inline uintptr_t msm_ion_heap_meminfo(const bool is_total)
{
	return 0;
}
#endif 

#endif
