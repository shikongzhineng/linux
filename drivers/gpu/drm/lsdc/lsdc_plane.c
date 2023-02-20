// SPDX-License-Identifier: GPL-2.0

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane_helper.h>
#include "lsdc_drv.h"
#include "lsdc_regs.h"
#include "lsdc_ttm.h"

static const u32 lsdc_primary_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
};

static const u32 lsdc_cursor_formats[] = {
	DRM_FORMAT_ARGB8888,
};

static const u64 lsdc_fb_format_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static unsigned int lsdc_get_fb_offset(struct drm_framebuffer *fb,
				       struct drm_plane_state *state,
				       unsigned int plane)
{
	unsigned int offset = fb->offsets[plane];

	offset += fb->format->cpp[plane] * (state->src_x >> 16);
	offset += fb->pitches[plane] * (state->src_y >> 16);

	return offset;
}

static void lsdc_primary_update_impl(struct lsdc_device *ldev,
				     struct drm_framebuffer *fb,
				     unsigned int fb_offset,
				     unsigned int pipe)
{
	struct drm_device *ddev = &ldev->base;
	struct ttm_buffer_object *tbo = to_ttm_bo(fb->obj[0]);
	u64 bo_offset = lsdc_bo_gpu_offset(tbo);
	u64 fb_addr = ldev->vram_base + bo_offset + fb_offset;
	u32 stride = fb->pitches[0];
	u32 cfg;
	u32 lo, hi;

	if (IS_ERR((void *)bo_offset)) {
		drm_warn(ddev, "bo not pinned, should not happen\n");
		return;
	}

	/* 40-bit width physical address bus */
	lo = fb_addr & 0xFFFFFFFF;
	hi = (fb_addr >> 32) & 0xFF;

	cfg = lsdc_crtc_rreg32(ldev, LSDC_CRTC0_CFG_REG, pipe);
	if (cfg & CFG_FB_IN_USING) {
		drm_dbg(ddev, "CRTC-%u(FB1) is in using\n", pipe);
		lsdc_crtc_wreg32(ldev, LSDC_CRTC0_FB1_LO_ADDR_REG, pipe, lo);
		lsdc_crtc_wreg32(ldev, LSDC_CRTC0_FB1_HI_ADDR_REG, pipe, hi);
	} else {
		drm_dbg(ddev, "CRTC-%u(FB0) is in using\n", pipe);
		lsdc_crtc_wreg32(ldev, LSDC_CRTC0_FB0_LO_ADDR_REG, pipe, lo);
		lsdc_crtc_wreg32(ldev, LSDC_CRTC0_FB0_HI_ADDR_REG, pipe, hi);
	}

	drm_dbg(ddev, "CRTC-%u scanout from 0x%llx\n", pipe, fb_addr);

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_STRIDE_REG, pipe, stride);

	/* clean old fb format settings */
	cfg &= ~CFG_PIX_FMT_MASK;
	/* TODO: add RGB565 support */
	cfg |= LSDC_PF_XRGB8888;

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_CFG_REG, pipe, cfg);
}

static int lsdc_plane_atomic_check(struct drm_plane *plane,
				   struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct drm_crtc_state *new_crtc_state;
	bool can_position;

	if (!crtc)
		return 0;

	new_crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

	can_position = (plane->type == DRM_PLANE_TYPE_CURSOR) ? true : false;

	return drm_atomic_helper_check_plane_state(new_plane_state,
						   new_crtc_state,
						   DRM_PLANE_NO_SCALING,
						   DRM_PLANE_NO_SCALING,
						   can_position,
						   true);
}

static void lsdc_update_primary_plane(struct drm_plane *plane,
				      struct drm_atomic_state *state)
{
	struct lsdc_device *ldev = to_lsdc(plane->dev);
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct drm_framebuffer *fb = new_plane_state->fb;
	unsigned int pipe = drm_crtc_index(crtc);
	unsigned int fb_offset = lsdc_get_fb_offset(fb, new_plane_state, 0);

	lsdc_primary_update_impl(ldev, fb, fb_offset, pipe);
}

static void lsdc_disable_primary_plane(struct drm_plane *plane,
				       struct drm_atomic_state *state)
{
	/* do nothing, just prevent call into atomic_update() */
	drm_dbg(plane->dev, "%s disabled\n", plane->name);
}

static void lsdc_ttm_cleanup_fb(struct drm_plane *plane,
				struct drm_plane_state *state,
				unsigned int np)
{
	struct drm_gem_object *obj;
	struct drm_framebuffer *fb = state->fb;

	while (np) {
		--np;
		obj = fb->obj[np];
		if (!obj) {
			drm_err(plane->dev, "%s: no obj\n", plane->name);
			continue;
		}
		lsdc_bo_unpin(obj);
	}
}

static int lsdc_plane_prepare_fb(struct drm_plane *plane,
				 struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct ttm_buffer_object *tbo;
	struct drm_gem_object *obj;
	unsigned int i;
	int ret;

	if (!fb)
		return 0;

	for (i = 0; i < fb->format->num_planes; ++i) {
		obj = fb->obj[i];
		if (!obj) {
			ret = -EINVAL;
			goto err_ret;
		}
		tbo = to_ttm_bo(obj);

		lsdc_bo_set_placement(tbo, LSDC_GEM_DOMAIN_VRAM, TTM_PL_FLAG_CONTIGUOUS);

		ret = lsdc_bo_pin(obj);
		if (ret)
			goto err_ret;
	}

	ret = drm_gem_plane_helper_prepare_fb(plane, new_state);
	if (ret)
		goto err_ret;

	return 0;

err_ret:
	drm_err(plane->dev, "%s: %d\n", __func__, ret);
	lsdc_ttm_cleanup_fb(plane, new_state, i);
	return ret;
}

static void lsdc_plane_cleanup_fb(struct drm_plane *plane,
				  struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state->fb;

	if (!fb)
		return;

	lsdc_ttm_cleanup_fb(plane, old_state, fb->format->num_planes);
}

static const struct drm_plane_helper_funcs lsdc_primary_plane_helpers = {
	.prepare_fb = lsdc_plane_prepare_fb,
	.cleanup_fb = lsdc_plane_cleanup_fb,
	.atomic_check = lsdc_plane_atomic_check,
	.atomic_update = lsdc_update_primary_plane,
	.atomic_disable = lsdc_disable_primary_plane,
};

/* update the format, size and location of the cursor */
static void lsdc_cursor_atomic_update(struct drm_plane *plane,
				      struct drm_atomic_state *state)
{
	struct lsdc_cursor *cursor = to_lsdc_cursor(plane);
	const struct lsdc_cursor_lowing_funcs *cfuncs = cursor->funcs;
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_framebuffer *cursor_fb = new_plane_state->fb;
	struct ttm_buffer_object *tbo = to_ttm_bo(cursor_fb->obj[0]);
	u64 cursor_bo_offset = lsdc_bo_gpu_offset(tbo);

	cfuncs->update_position(cursor, new_plane_state->crtc_x, new_plane_state->crtc_y);
	cfuncs->update_offset(cursor, cursor_bo_offset);
	cfuncs->update_config(cursor, CURSOR_FORMAT_ARGB8888 | CURSOR_SIZE_64X64);
}

static void lsdc_cursor_atomic_disable(struct drm_plane *plane,
				       struct drm_atomic_state *state)
{
	struct drm_device *ddev = plane->dev;
	struct lsdc_cursor *cursor = to_lsdc_cursor(plane);
	const struct lsdc_cursor_lowing_funcs *cfuncs = cursor->funcs;

	cfuncs->update_config(cursor, 0);

	drm_dbg(ddev, "%s disabled\n", plane->name);
}

static const struct drm_plane_helper_funcs lsdc_cursor_plane_helpers = {
	.prepare_fb = lsdc_plane_prepare_fb,
	.cleanup_fb = lsdc_plane_cleanup_fb,
	.atomic_check = lsdc_plane_atomic_check,
	.atomic_update = lsdc_cursor_atomic_update,
	.atomic_disable = lsdc_cursor_atomic_disable,
};

static const struct drm_plane_funcs lsdc_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static void lsdc_update_cursor0_position(struct lsdc_cursor * const this,
					 int x,
					 int y)
{
	struct lsdc_device *ldev = this->ldev;

	if (x < 0)
		x = 0;

	if (y < 0)
		y = 0;

	lsdc_wreg32(ldev, LSDC_CURSOR0_POSITION_REG, (y << 16) | x);
}

static void lsdc_update_cursor1_position(struct lsdc_cursor * const this,
					 int x,
					 int y)
{
	struct lsdc_device *ldev = this->ldev;

	if (x < 0)
		x = 0;

	if (y < 0)
		y = 0;

	lsdc_wreg32(ldev, LSDC_CURSOR1_POSITION_REG, (y << 16) | x);
}

static void lsdc_update_cursor0_config(struct lsdc_cursor * const this,
				       u32 cfg)
{
	struct lsdc_device *ldev = this->ldev;

	if (this->cfg != cfg) {
		this->cfg = cfg;
		lsdc_wreg32(ldev, LSDC_CURSOR0_CFG_REG, cfg & ~CURSOR_LOCATION);
	}
}

/*
 * Update location, format, enable and disable of the cursor,
 * For those who have two hardware cursor, cursor 0 is attach it to CRTC-0,
 * cursor 1 is attached to CRTC-1. Compositing the primary and cursor plane
 * is automatically done by hardware, the cursor is alway on the top of the
 * primary, there is no depth property can be set. pertty convenient.
 */
static void lsdc_update_cursor1_config(struct lsdc_cursor * const this,
				       u32 cfg)
{
	struct lsdc_device *ldev = this->ldev;

	if (this->cfg != cfg) {
		this->cfg = cfg;
		lsdc_wreg32(ldev, LSDC_CURSOR1_CFG_REG, cfg | CURSOR_LOCATION);
	}
}

static void lsdc_update_cursor0_offset(struct lsdc_cursor * const this,
				       u64 offset)
{
	struct lsdc_device *ldev = this->ldev;
	u64 addr;

	if (this->offset != offset) {
		this->offset = offset;
		addr = ldev->vram_base + offset;
		lsdc_wreg32(ldev, LSDC_CURSOR0_ADDR_HI_REG, (addr >> 32) & 0xFF);
		lsdc_wreg32(ldev, LSDC_CURSOR0_ADDR_LO_REG, addr);
	}
}

static void lsdc_update_cursor1_offset(struct lsdc_cursor * const this,
				       u64 offset)
{
	struct lsdc_device *ldev = this->ldev;
	u64 addr;

	if (this->offset != offset) {
		this->offset = offset;
		addr = ldev->vram_base + offset;
		lsdc_wreg32(ldev, LSDC_CURSOR1_ADDR_HI_REG, (addr >> 32) & 0xFF);
		lsdc_wreg32(ldev, LSDC_CURSOR1_ADDR_LO_REG, addr);
	}
}

static const struct lsdc_cursor_lowing_funcs cursor_lowing_funcs_pipe0 = {
	.update_position = lsdc_update_cursor0_position,
	.update_config = lsdc_update_cursor0_config,
	.update_offset = lsdc_update_cursor0_offset,
};

static const struct lsdc_cursor_lowing_funcs cursor_lowing_funcs_pipe1 = {
	.update_position = lsdc_update_cursor1_position,
	.update_config = lsdc_update_cursor1_config,
	.update_offset = lsdc_update_cursor1_offset,
};

static void lsdc_update_cursor0_config_quirk(struct lsdc_cursor * const this,
					     u32 cfg)
{
	struct lsdc_device *ldev = this->ldev;

	/*
	 * If bit 4 of LSDC_CURSOR0_CFG_REG is 1, cursor will be locate at CRTC-1,
	 * if bit 4 of LSDC_CURSOR0_CFG_REG is 0, cursor will be locate at CRTC-0.
	 * We made it shared by the two CRTC on extend screen usage case.
	 */
	lsdc_wreg32(ldev, LSDC_CURSOR0_CFG_REG, cfg | CURSOR_LOCATION);
}

static const struct lsdc_cursor_lowing_funcs cursor_lowing_funcs_pipe1_quirk = {
	.update_position = lsdc_update_cursor0_position,
	.update_config = lsdc_update_cursor0_config_quirk,
	.update_offset = lsdc_update_cursor0_offset,
};

static void lsdc_cursor_plane_preinit(struct drm_plane *plane,
				      struct lsdc_device *ldev,
				      unsigned int index)
{
	struct lsdc_cursor *cursor = to_lsdc_cursor(plane);
	const struct lsdc_desc *descp = ldev->descp;

	cursor->ldev = ldev;

	if (index == 0) {
		cursor->funcs = &cursor_lowing_funcs_pipe0;
		return;
	}

	/* index == 1 case */
	if (descp->chip == CHIP_LS7A2000 || descp->chip == CHIP_LS2K2000) {
		cursor->funcs = &cursor_lowing_funcs_pipe1;
		return;
	}

	/* only one hardware cursor in ls7a1000, ls2k1000 */
	cursor->funcs = &cursor_lowing_funcs_pipe1_quirk;
}

int lsdc_plane_init(struct lsdc_device *ldev,
		    struct drm_plane *plane,
		    enum drm_plane_type type,
		    unsigned int index)
{
	struct drm_device *ddev = &ldev->base;
	unsigned int format_count;
	const u32 *formats;
	const char *name;
	int ret;

	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY:
		formats = lsdc_primary_formats;
		format_count = ARRAY_SIZE(lsdc_primary_formats);
		name = "primary-%u";
		break;
	case DRM_PLANE_TYPE_CURSOR:
		formats = lsdc_cursor_formats;
		format_count = ARRAY_SIZE(lsdc_cursor_formats);
		name = "cursor-%u";
		break;
	case DRM_PLANE_TYPE_OVERLAY:
		drm_err(ddev, "overlay plane is not supported\n");
		break;
	}

	ret = drm_universal_plane_init(ddev, plane, 1 << index,
				       &lsdc_plane_funcs,
				       formats, format_count,
				       lsdc_fb_format_modifiers,
				       type, name, index);
	if (ret) {
		drm_err(ddev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY:
		drm_plane_helper_add(plane, &lsdc_primary_plane_helpers);
		break;
	case DRM_PLANE_TYPE_CURSOR:
		drm_plane_helper_add(plane, &lsdc_cursor_plane_helpers);
		lsdc_cursor_plane_preinit(plane, ldev, index);
		break;
	case DRM_PLANE_TYPE_OVERLAY:
		drm_err(ddev, "overlay plane is not supported\n");
		break;
	}

	return 0;
}
