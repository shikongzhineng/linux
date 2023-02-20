/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LSDC_TTM_H__
#define __LSDC_TTM_H__

#include <linux/container_of.h>
#include <linux/iosys-map.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_placement.h>

#define LSDC_GEM_DOMAIN_SYSTEM          0x1
#define LSDC_GEM_DOMAIN_GTT             0x2
#define LSDC_GEM_DOMAIN_VRAM            0x4

struct lsdc_bo {
	struct ttm_buffer_object bo;
	struct iosys_map map;

	unsigned int vmap_use_count;

	struct ttm_placement placement;
	struct ttm_place placements[2];
};

static inline struct lsdc_bo *
to_lsdc_bo(struct ttm_buffer_object *tbo)
{
	return container_of(tbo, struct lsdc_bo, bo);
}

static inline struct lsdc_bo *
gem_to_lsdc_bo(struct drm_gem_object *gem)
{
	return container_of(gem, struct lsdc_bo, bo.base);
}

static inline struct ttm_buffer_object *
to_ttm_bo(struct drm_gem_object *gem)
{
	return container_of(gem, struct ttm_buffer_object, base);
}

u64 lsdc_bo_gpu_offset(struct ttm_buffer_object *tbo);

void lsdc_bo_set_placement(struct ttm_buffer_object *tbo, u32 domain, u32 flags);
int lsdc_bo_pin(struct drm_gem_object *gem);
void lsdc_bo_unpin(struct drm_gem_object *gem);

int lsdc_dumb_map_offset(struct drm_file *file,
			 struct drm_device *dev,
			 u32 handle,
			 uint64_t *offset);

int lsdc_dumb_create(struct drm_file *file,
		     struct drm_device *ddev,
		     struct drm_mode_create_dumb *args);

int lsdc_ttm_init(struct lsdc_device *ldev);

#endif
