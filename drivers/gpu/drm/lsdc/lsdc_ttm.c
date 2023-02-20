// SPDX-License-Identifier: GPL-2.0+

#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_managed.h>
#include <drm/ttm/ttm_range_manager.h>
#include <drm/ttm/ttm_tt.h>
#include "lsdc_drv.h"
#include "lsdc_ttm.h"

static void lsdc_ttm_tt_destroy(struct ttm_device *bdev, struct ttm_tt *tt)
{
	ttm_tt_fini(tt);
	kfree(tt);
}

static struct ttm_tt *
lsdc_ttm_tt_create(struct ttm_buffer_object *bo, uint32_t page_flags)
{
	struct ttm_tt *tt;
	int ret;

	tt = kzalloc(sizeof(*tt), GFP_KERNEL);
	if (!tt)
		return NULL;

	ret = ttm_tt_init(tt, bo, page_flags, ttm_cached, 0);
	if (ret < 0) {
		kfree(tt);
		return NULL;
	}

	return tt;
}

void lsdc_bo_set_placement(struct ttm_buffer_object *tbo, u32 domain, u32 flags)
{
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);
	unsigned int i;
	unsigned int c = 0;

	lbo->placement.placement = lbo->placements;
	lbo->placement.busy_placement = lbo->placements;

	if (domain & LSDC_GEM_DOMAIN_VRAM) {
		lbo->placements[c].mem_type = TTM_PL_VRAM;
		lbo->placements[c++].flags = flags;
	}

	if (domain & LSDC_GEM_DOMAIN_SYSTEM || !c) {
		lbo->placements[c].mem_type = TTM_PL_SYSTEM;
		lbo->placements[c++].flags = flags;
	}

	lbo->placement.num_placement = c;
	lbo->placement.num_busy_placement = c;

	for (i = 0; i < c; ++i) {
		lbo->placements[i].fpfn = 0;
		lbo->placements[i].lpfn = 0;
	}
}

static void lsdc_bo_evict_flags(struct ttm_buffer_object *tbo,
				struct ttm_placement *placement)
{
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);

	lsdc_bo_set_placement(tbo, LSDC_GEM_DOMAIN_SYSTEM, 0);

	*placement = lbo->placement;
}

static int lsdc_bo_move(struct ttm_buffer_object *tbo,
			bool evict,
			struct ttm_operation_ctx *ctx,
			struct ttm_resource *new_mem,
			struct ttm_place *hop)
{
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);
	struct drm_device *ddev = tbo->base.dev;

	if (drm_WARN_ON_ONCE(ddev, lbo->vmap_use_count))
		goto just_move_it;

	ttm_bo_vunmap(tbo, &lbo->map);
	/* explicitly clear mapping for next vmap call */
	iosys_map_clear(&lbo->map);

	drm_dbg(ddev, "%s: evict: %s\n", __func__, evict ? "Yes" : "No");

just_move_it:
	return ttm_bo_move_memcpy(tbo, ctx, new_mem);
}

static void lsdc_bo_delete_mem_notify(struct ttm_buffer_object *tbo)
{
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);
	struct drm_device *ddev = tbo->base.dev;

	if (drm_WARN_ON_ONCE(ddev, lbo->vmap_use_count))
		return;

	ttm_bo_vunmap(tbo, &lbo->map);
	iosys_map_clear(&lbo->map);
}

static int lsdc_bo_reserve_io_mem(struct ttm_device *bdev,
				  struct ttm_resource *mem)
{
	struct lsdc_device *ldev = bdev_to_lsdc(bdev);
	const struct lsdc_desc *descp = ldev->descp;

	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:
		/* nothing to do */
		break;
	case TTM_PL_VRAM:
		mem->bus.offset = (mem->start << PAGE_SHIFT) + ldev->vram_base;
		mem->bus.is_iomem = true;
		if (descp->is_soc)
			mem->bus.caching = ttm_cached;
		else
			mem->bus.caching = ttm_write_combined;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct ttm_device_funcs lsdc_bo_driver = {
	.ttm_tt_create = lsdc_ttm_tt_create,
	.ttm_tt_destroy = lsdc_ttm_tt_destroy,
	.eviction_valuable = ttm_bo_eviction_valuable,
	.evict_flags = lsdc_bo_evict_flags,
	.move = lsdc_bo_move,
	.delete_mem_notify = lsdc_bo_delete_mem_notify,
	.io_mem_reserve = lsdc_bo_reserve_io_mem,
};

static void lsdc_bo_free(struct drm_gem_object *gem)
{
	struct ttm_buffer_object *tbo = to_ttm_bo(gem);

	ttm_bo_put(tbo);
}

int lsdc_bo_pin(struct drm_gem_object *gem)
{
	struct ttm_buffer_object *tbo = to_ttm_bo(gem);
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);
	int ret;

	ret = ttm_bo_reserve(tbo, true, false, NULL);
	if (ret) {
		drm_err(gem->dev, "%s: %d\n", __func__, ret);
		return ret;
	}

	if (tbo->pin_count == 0) {
		struct ttm_operation_ctx ctx = { false, false };

		ret = ttm_bo_validate(tbo, &lbo->placement, &ctx);
		if (ret < 0) {
			ttm_bo_unreserve(tbo);
			drm_err(gem->dev, "%s: %d\n", __func__, ret);
			return ret;
		}
	}

	ttm_bo_pin(tbo);

	ttm_bo_unreserve(tbo);

	return ret;
}

void lsdc_bo_unpin(struct drm_gem_object *gem)
{
	struct ttm_buffer_object *tbo = to_ttm_bo(gem);
	int ret;

	ret = ttm_bo_reserve(tbo, true, false, NULL);
	if (ret) {
		drm_err(gem->dev, "%s: bo reserve failed\n", __func__);
		return;
	}

	ttm_bo_unpin(tbo);
	ttm_bo_unreserve(tbo);
}

static int lsdc_bo_vmap(struct drm_gem_object *gem, struct iosys_map *map)
{
	struct drm_device *ddev = gem->dev;
	struct ttm_buffer_object *tbo = to_ttm_bo(gem);
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);
	int ret;

	dma_resv_assert_held(gem->resv);

	if (tbo->pin_count == 0) {
		struct ttm_operation_ctx ctx = { false, false };

		ret = ttm_bo_validate(tbo, &lbo->placement, &ctx);
		if (ret < 0)
			return ret;
	}

	ttm_bo_pin(tbo);

	if (lbo->vmap_use_count > 0) {
		drm_dbg(ddev, "%s: already mapped\n", __func__);
		goto finish;
	}

	/* Only vmap if the there's no mapping present */
	if (iosys_map_is_null(&lbo->map)) {
		ret = ttm_bo_vmap(tbo, &lbo->map);
		if (ret) {
			ttm_bo_unpin(tbo);
			return ret;
		}
	}

finish:
	++lbo->vmap_use_count;
	*map = lbo->map;

	return 0;
}

static void lsdc_bo_vunmap(struct drm_gem_object *gem, struct iosys_map *map)
{
	struct drm_device *ddev = gem->dev;
	struct ttm_buffer_object *tbo = to_ttm_bo(gem);
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);

	dma_resv_assert_held(gem->resv);

	if (drm_WARN_ON_ONCE(ddev, !lbo->vmap_use_count))
		return;

	if (drm_WARN_ON_ONCE(ddev, !iosys_map_is_equal(&lbo->map, map)))
		return; /* BUG: map not mapped from this BO */

	if (--lbo->vmap_use_count > 0)
		return;

	/* We delay the actual unmap operation until the BO gets evicted */
	ttm_bo_unpin(tbo);
}

static int lsdc_bo_mmap(struct drm_gem_object *gem,
			struct vm_area_struct *vma)
{
	struct ttm_buffer_object *tbo = to_ttm_bo(gem);
	int ret;

	ret = ttm_bo_mmap_obj(vma, tbo);
	if (ret < 0)
		return ret;

	/*
	 * ttm has its own object refcounting, so drop gem reference
	 * to avoid double accounting counting.
	 */
	drm_gem_object_put(gem);

	return 0;
}

static const struct drm_gem_object_funcs lsdc_gem_object_funcs = {
	.free   = lsdc_bo_free,
	.pin    = lsdc_bo_pin,
	.unpin  = lsdc_bo_unpin,
	.vmap   = lsdc_bo_vmap,
	.vunmap = lsdc_bo_vunmap,
	.mmap   = lsdc_bo_mmap,
};

static void lsdc_bo_destroy(struct ttm_buffer_object *tbo)
{
	struct lsdc_bo *lbo = to_lsdc_bo(tbo);

	WARN_ON(lbo->vmap_use_count);
	WARN_ON(iosys_map_is_set(&lbo->map));

	drm_gem_object_release(&tbo->base);

	kfree(lbo);
}

static struct lsdc_bo *lsdc_bo_create(struct drm_device *ddev, size_t size)
{
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct ttm_device *bdev = &ldev->bdev;
	struct lsdc_bo *lbo;
	struct ttm_buffer_object *tbo;
	struct drm_gem_object *gem;
	int ret;

	lbo = kzalloc(sizeof(*lbo), GFP_KERNEL);
	if (!lbo)
		return ERR_PTR(-ENOMEM);

	tbo = &lbo->bo;
	gem = &tbo->base;
	gem->funcs = &lsdc_gem_object_funcs;

	ret = drm_gem_object_init(ddev, gem, size);
	if (ret) {
		kfree(lbo);
		return ERR_PTR(ret);
	}

	tbo->bdev = bdev;
	lsdc_bo_set_placement(tbo, LSDC_GEM_DOMAIN_SYSTEM, 0);

	ret = ttm_bo_init_validate(bdev,
				   tbo,
				   ttm_bo_type_device,
				   &lbo->placement,
				   0,
				   false, NULL, NULL,
				   lsdc_bo_destroy);
	if (ret)
		return ERR_PTR(ret);

	return lbo;
}

u64 lsdc_bo_gpu_offset(struct ttm_buffer_object *tbo)
{
	struct ttm_resource *resource = tbo->resource;

	if (WARN_ON_ONCE(!tbo->pin_count))
		return -ENODEV;

	/* Keep TTM behavior for now, remove when drivers are audited */
	if (WARN_ON_ONCE(!resource))
		return 0;

	if (WARN_ON_ONCE(resource->mem_type == TTM_PL_SYSTEM))
		return 0;

	return resource->start << PAGE_SHIFT;
}

int lsdc_dumb_create(struct drm_file *file,
		     struct drm_device *ddev,
		     struct drm_mode_create_dumb *args)
{
	struct lsdc_device *ldev = to_lsdc(ddev);
	const struct lsdc_desc *descp = ldev->descp;
	size_t pitch, size;
	struct lsdc_bo *lbo;
	struct ttm_buffer_object *tbo;
	u32 handle;
	int ret;

	pitch = args->width * DIV_ROUND_UP(args->bpp, 8);
	pitch = ALIGN(pitch, descp->pitch_align);
	size = pitch * args->height;
	size = roundup(size, PAGE_SIZE);
	if (!size)
		return -EINVAL;

	lbo = lsdc_bo_create(ddev, size);
	if (IS_ERR(lbo))
		return PTR_ERR(lbo);

	tbo = &lbo->bo;

	ret = drm_gem_handle_create(file, &tbo->base, &handle);
	if (ret)
		goto err_drm_gem_object_put;

	drm_gem_object_put(&tbo->base);

	drm_dbg(ddev, "stride: %lu, height: %u\n", pitch, args->height);

	args->pitch = pitch;
	args->size = size;
	args->handle = handle;

	return 0;

err_drm_gem_object_put:
	drm_gem_object_put(&tbo->base);
	return ret;
}

int lsdc_dumb_map_offset(struct drm_file *file,
			 struct drm_device *ddev,
			 u32 handle,
			 uint64_t *offset)
{
	struct drm_gem_object *gem;

	gem = drm_gem_object_lookup(file, handle);
	if (!gem)
		return -ENOENT;

	*offset = drm_vma_node_offset_addr(&gem->vma_node);

	drm_gem_object_put(gem);

	return 0;
}

static void lsdc_ttm_fini(struct drm_device *ddev, void *data)
{
	struct lsdc_device *ldev = (struct lsdc_device *)data;

	ttm_range_man_fini(&ldev->bdev, TTM_PL_VRAM);
	ttm_device_fini(&ldev->bdev);
}

int lsdc_ttm_init(struct lsdc_device *ldev)
{
	struct drm_device *ddev = &ldev->base;
	unsigned long num_pages;
	int ret;

	ret = ttm_device_init(&ldev->bdev,
			      &lsdc_bo_driver,
			      ddev->dev,
			      ddev->anon_inode->i_mapping,
			      ddev->vma_offset_manager,
			      false,
			      true);
	if (ret)
		return ret;

	num_pages = ldev->vram_size >> PAGE_SHIFT;

	ret = ttm_range_man_init(&ldev->bdev,
				 TTM_PL_VRAM,
				 false,
				 num_pages);
	if (ret)
		return ret;

	drm_info(ddev, "number of pages: %lu\n", num_pages);

	return drmm_add_action_or_reset(ddev, lsdc_ttm_fini, ldev);
}
