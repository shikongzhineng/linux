// SPDX-License-Identifier: GPL-2.0

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_vblank.h>
#include "lsdc_drv.h"
#include "lsdc_regs.h"

static u32 lsdc_crtc_get_vblank_counter(struct drm_crtc *crtc)
{
	struct lsdc_device *ldev = to_lsdc(crtc->dev);

	return lsdc_crtc_rreg32(ldev, LSDC_CRTC0_VSYNC_COUNTER_REG, drm_crtc_index(crtc));
}

static int lsdc_enable_vblank_pipe_0(struct drm_crtc *crtc)
{
	struct lsdc_device *ldev = to_lsdc(crtc->dev);

	lsdc_ureg32_set(ldev, LSDC_INT_REG, INT_CRTC0_VSYNC_EN);

	return 0;
}

static void lsdc_disable_vblank_pipe_0(struct drm_crtc *crtc)
{
	struct lsdc_device *ldev = to_lsdc(crtc->dev);

	lsdc_ureg32_clr(ldev, LSDC_INT_REG, INT_CRTC0_VSYNC_EN);
}

static int lsdc_enable_vblank_pipe_1(struct drm_crtc *crtc)
{
	struct lsdc_device *ldev = to_lsdc(crtc->dev);

	lsdc_ureg32_set(ldev, LSDC_INT_REG, INT_CRTC1_VSYNC_EN);

	return 0;
}

static void lsdc_disable_vblank_pipe_1(struct drm_crtc *crtc)
{
	struct lsdc_device *ldev = to_lsdc(crtc->dev);

	lsdc_ureg32_clr(ldev, LSDC_INT_REG, INT_CRTC1_VSYNC_EN);
}

static void lsdc_crtc_reset(struct drm_crtc *crtc)
{
	struct drm_device *ddev = crtc->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct lsdc_crtc_state *priv_crtc_state;

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_CFG_REG, drm_crtc_index(crtc),
			 CFG_RESET_N | LSDC_PF_XRGB8888 | LSDC_DMA_STEP_64_BYTES);

	if (crtc->state) {
		priv_crtc_state = to_lsdc_crtc_state(crtc->state);
		__drm_atomic_helper_crtc_destroy_state(&priv_crtc_state->base);
		kfree(priv_crtc_state);
	}

	priv_crtc_state = kzalloc(sizeof(*priv_crtc_state), GFP_KERNEL);
	if (!priv_crtc_state)
		return;

	__drm_atomic_helper_crtc_reset(crtc, &priv_crtc_state->base);
}

static void lsdc_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					   struct drm_crtc_state *state)
{
	struct lsdc_crtc_state *priv_state = to_lsdc_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(&priv_state->base);

	kfree(priv_state);
}

static struct drm_crtc_state *
lsdc_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct lsdc_crtc_state *new_priv_state;
	struct lsdc_crtc_state *old_priv_state;

	new_priv_state = kmalloc(sizeof(*new_priv_state), GFP_KERNEL);
	if (!new_priv_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &new_priv_state->base);

	old_priv_state = to_lsdc_crtc_state(crtc->state);

	memcpy(&new_priv_state->pparms, &old_priv_state->pparms,
	       sizeof(new_priv_state->pparms));

	return &new_priv_state->base;
}

#define lsdc_crtc_funcs_common                                               \
	.reset = lsdc_crtc_reset,                                            \
	.destroy = drm_crtc_cleanup,                                         \
	.set_config = drm_atomic_helper_set_config,                          \
	.page_flip = drm_atomic_helper_page_flip,                            \
	.atomic_duplicate_state = lsdc_crtc_atomic_duplicate_state,          \
	.atomic_destroy_state = lsdc_crtc_atomic_destroy_state,              \
	.get_vblank_timestamp = drm_crtc_vblank_helper_get_vblank_timestamp

static const struct drm_crtc_funcs lsdc_crtc_funcs_array[2][LSDC_NUM_CRTC] = {
	{
		{
			lsdc_crtc_funcs_common,
			.enable_vblank = lsdc_enable_vblank_pipe_0,
			.disable_vblank = lsdc_disable_vblank_pipe_0,
		},
		{
			lsdc_crtc_funcs_common,
			.enable_vblank = lsdc_enable_vblank_pipe_1,
			.disable_vblank = lsdc_disable_vblank_pipe_1,
		},
	},
	{
		{
			lsdc_crtc_funcs_common,
			.enable_vblank = lsdc_enable_vblank_pipe_0,
			.disable_vblank = lsdc_disable_vblank_pipe_0,
			.get_vblank_counter = lsdc_crtc_get_vblank_counter,
		},
		{
			lsdc_crtc_funcs_common,
			.enable_vblank = lsdc_enable_vblank_pipe_1,
			.disable_vblank = lsdc_disable_vblank_pipe_1,
			.get_vblank_counter = lsdc_crtc_get_vblank_counter,
		}
	}
};

#undef lsdc_crtc_funcs_common

static enum drm_mode_status
lsdc_crtc_mode_valid(struct drm_crtc *crtc,
		     const struct drm_display_mode *mode)
{
	struct drm_device *ddev = crtc->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	const struct lsdc_desc *descp = ldev->descp;

	if (mode->hdisplay > descp->max_width)
		return MODE_BAD_HVALUE;
	if (mode->vdisplay > descp->max_height)
		return MODE_BAD_VVALUE;

	if (mode->clock > descp->max_pixel_clk) {
		drm_dbg(ddev, "mode %dx%d, pixel clock=%d is too high\n",
			mode->hdisplay, mode->vdisplay, mode->clock);
		return MODE_CLOCK_HIGH;
	}

	if ((mode->hdisplay * 4) % descp->pitch_align) {
		drm_dbg(ddev, "stride align to %u bytes is required\n",
			descp->pitch_align);
		return MODE_BAD;
	}

	return MODE_OK;
}

static int lsdc_pixpll_atomic_check(struct drm_crtc *crtc,
				    struct drm_crtc_state *state)
{
	struct lsdc_display_pipe *dispipe = crtc_to_display_pipe(crtc);
	struct lsdc_pll *pixpll = &dispipe->pixpll;
	const struct lsdc_pixpll_funcs *pfuncs = pixpll->funcs;
	struct lsdc_crtc_state *priv_state = to_lsdc_crtc_state(state);
	struct lsdc_pll_parms *pout = &priv_state->pparms;
	unsigned int clock = state->mode.clock;
	bool ret;

	ret = pfuncs->compute(pixpll, clock, pout);
	if (ret)
		return 0;

	drm_warn(crtc->dev, "Find PLL parameters for %u failed\n", clock);

	return -EINVAL;
}

static int lsdc_crtc_helper_atomic_check(struct drm_crtc *crtc,
					 struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

	if (!crtc_state->enable)
		return 0;

	return lsdc_pixpll_atomic_check(crtc, crtc_state);
}

static void lsdc_crtc_enable(struct drm_crtc *crtc,
			     struct drm_atomic_state *state)
{
	struct drm_device *ddev = crtc->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	const struct lsdc_desc *descp = ldev->descp;
	struct lsdc_display_pipe *dispipe = crtc_to_display_pipe(crtc);
	struct lsdc_pll *pixpll = &dispipe->pixpll;
	const struct lsdc_pixpll_funcs *clk_func = pixpll->funcs;
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
	struct lsdc_crtc_state *priv_state = to_lsdc_crtc_state(crtc_state);
	struct drm_display_mode *mode = &crtc_state->mode;
	unsigned int index = drm_crtc_index(crtc);
	unsigned int width_in_bytes = mode->crtc_hdisplay * 4;
	u32 val;

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_HDISPLAY_REG, index,
			 (mode->crtc_htotal << 16) | mode->crtc_hdisplay);

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_VDISPLAY_REG, index,
			 (mode->crtc_vtotal << 16) | mode->crtc_vdisplay);

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_HSYNC_REG, index,
			 (mode->crtc_hsync_end << 16) |
			 mode->crtc_hsync_start |
			 CFG_HSYNC_EN);

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_VSYNC_REG, index,
			 (mode->crtc_vsync_end << 16) |
			 mode->crtc_vsync_start |
			 CFG_VSYNC_EN);

	val = lsdc_crtc_rreg32(ldev, LSDC_CRTC0_CFG_REG, index);
	/* clear old dma step settings */
	val &= ~CFG_DMA_STEP_MASK;

	if (descp->chip == CHIP_LS7A2000 || descp->chip == CHIP_LS2K2000) {
		/*
		 * Using large dma step as much as possible,
		 * for improve hardware DMA efficiency.
		 */
		if (width_in_bytes % 256 == 0)
			val |= LSDC_DMA_STEP_256_BYTES;
		else if (width_in_bytes % 128 == 0)
			val |= LSDC_DMA_STEP_128_BYTES;
		else if (width_in_bytes % 64 == 0)
			val |= LSDC_DMA_STEP_64_BYTES;
		else  /* width_in_bytes % 32 == 0 */
			val |= LSDC_DMA_STEP_32_BYTES;
	}

	lsdc_crtc_wreg32(ldev, LSDC_CRTC0_CFG_REG, index, val);

	clk_func->update(pixpll, &priv_state->pparms);

	drm_crtc_vblank_on(crtc);

	lsdc_crtc_ureg32_set(ldev, LSDC_CRTC0_CFG_REG, index, CFG_OUTPUT_EN);

	drm_dbg(ddev, "CRTC-%u enabled: %ux%u\n",
		index, mode->hdisplay, mode->vdisplay);
}

static void lsdc_crtc_disable(struct drm_crtc *crtc,
			      struct drm_atomic_state *state)
{
	struct drm_device *ddev = crtc->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	unsigned int i = drm_crtc_index(crtc);

	drm_crtc_wait_one_vblank(crtc);
	lsdc_crtc_ureg32_clr(ldev, LSDC_CRTC0_CFG_REG, i, CFG_OUTPUT_EN);
	drm_crtc_vblank_off(crtc);

	drm_dbg(ddev, "CRTC-%u disabled\n", i);
}

static void lsdc_crtc_atomic_flush(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, crtc->state->event);
		else
			drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);
}

static bool lsdc_crtc_get_scanout_position(struct drm_crtc *crtc,
					   bool in_vblank_irq,
					   int *vpos,
					   int *hpos,
					   ktime_t *stime,
					   ktime_t *etime,
					   const struct drm_display_mode *mode)
{
	struct drm_device *ddev = crtc->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	unsigned int i = drm_crtc_index(crtc);
	int line, vsw, vbp, vactive_start, vactive_end, vfp_end;
	u32 val = 0;

	vsw = mode->crtc_vsync_end - mode->crtc_vsync_start;
	vbp = mode->crtc_vtotal - mode->crtc_vsync_end;

	vactive_start = vsw + vbp + 1;
	vactive_end = vactive_start + mode->crtc_vdisplay;

	/* last scan line before VSYNC */
	vfp_end = mode->crtc_vtotal;

	if (stime)
		*stime = ktime_get();

	val = lsdc_crtc_rreg32(ldev, LSDC_CRTC0_SCAN_POS_REG, i);

	line = (val & 0xffff);

	if (line < vactive_start)
		line -= vactive_start;
	else if (line > vactive_end)
		line = line - vfp_end - vactive_start;
	else
		line -= vactive_start;

	*vpos = line;
	*hpos = val >> 16;

	if (etime)
		*etime = ktime_get();

	return true;
}

static const struct drm_crtc_helper_funcs lsdc_crtc_helper_funcs = {
	.mode_valid = lsdc_crtc_mode_valid,
	.atomic_enable = lsdc_crtc_enable,
	.atomic_disable = lsdc_crtc_disable,
	.atomic_check = lsdc_crtc_helper_atomic_check,
	.atomic_flush = lsdc_crtc_atomic_flush,
	.get_scanout_position = lsdc_crtc_get_scanout_position,
};

int lsdc_crtc_init(struct drm_device *ddev,
		   struct drm_crtc *crtc,
		   unsigned int index,
		   struct drm_plane *primary,
		   struct drm_plane *cursor)
{
	struct lsdc_device *ldev = to_lsdc(ddev);
	const struct lsdc_desc *descp = ldev->descp;
	unsigned int h = descp->has_vblank_counter;
	int ret;

	ret = drm_crtc_init_with_planes(ddev, crtc, primary, cursor,
					&lsdc_crtc_funcs_array[h][index],
					"CRTC-%d", index);
	if (ret) {
		drm_err(ddev, "crtc init with planes failed: %d\n", ret);
		return ret;
	}

	drm_crtc_helper_add(crtc, &lsdc_crtc_helper_funcs);

	drm_info(ddev, "%s initialized %s vblank counter support\n",
		 crtc->name, h ? "with" : "without");

	ret = drm_mode_crtc_set_gamma_size(crtc, 256);
	if (ret)
		drm_warn(ddev, "set the gamma table size failed\n");

	drm_crtc_enable_color_mgmt(crtc, 0, false, 256);

	return 0;
}
