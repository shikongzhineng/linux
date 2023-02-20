// SPDX-License-Identifier: GPL-2.0+

#include <drm/drm_debugfs.h>
#include <drm/drm_gem_vram_helper.h>
#include <drm/drm_managed.h>
#include "lsdc_drv.h"

#ifdef CONFIG_DEBUG_FS

static int lsdc_identify(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct lsdc_device *ldev = to_lsdc(node->minor->dev);

	seq_printf(m, "I'm in %s, Running on 0x%x\n",
		   chip_to_str(ldev->descp->chip), loongson_cpu_get_prid());

	return 0;
}

static int lsdc_show_clock(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, ddev) {
		struct lsdc_display_pipe *pipe = crtc_to_display_pipe(crtc);
		struct lsdc_pll *pixpll = &pipe->pixpll;
		const struct lsdc_pixpll_funcs *funcs = pixpll->funcs;
		struct drm_display_mode *mode = &crtc->state->mode;
		struct lsdc_pll_parms parms;
		unsigned int out_khz;

		out_khz = funcs->get_clock_rate(pixpll, &parms);

		seq_printf(m, "Display pipe %u: %dx%d\n",
			   pipe->index, mode->hdisplay, mode->vdisplay);

		seq_printf(m, "Frequency actually output: %u kHz\n", out_khz);
		seq_printf(m, "Pixel clock required: %d kHz\n", mode->clock);
		seq_printf(m, "diff: %d kHz\n", out_khz - mode->clock);

		seq_printf(m, "div_ref=%u, loopc=%u, div_out=%u\n",
			   parms.div_ref, parms.loopc, parms.div_out);

		seq_printf(m, "hsync_start=%d, hsync_end=%d, htotal=%d\n",
			   mode->hsync_start, mode->hsync_end, mode->htotal);
		seq_printf(m, "vsync_start=%d, vsync_end=%d, vtotal=%d\n\n",
			   mode->vsync_start, mode->vsync_end, mode->vtotal);
	}

	return 0;
}

static int lsdc_show_mm(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct drm_printer p = drm_seq_file_printer(m);

	drm_mm_print(&ddev->vma_offset_manager->vm_addr_space_mm, &p);

	return 0;
}

#define REG_DEF(reg) { __stringify_1(LSDC_##reg##_REG), LSDC_##reg##_REG }
static const struct {
	const char *name;
	u32 reg_offset;
} lsdc_regs_array[] = {
	REG_DEF(CURSOR0_CFG),
	REG_DEF(CURSOR0_ADDR_LO),
	REG_DEF(CURSOR0_ADDR_HI),
	REG_DEF(CURSOR0_POSITION),
	REG_DEF(CURSOR0_BG_COLOR),
	REG_DEF(CURSOR0_FG_COLOR),
	REG_DEF(CRTC0_CFG),
	REG_DEF(CRTC0_FB_ORIGIN),
	REG_DEF(CRTC0_HDISPLAY),
	REG_DEF(CRTC0_HSYNC),
	REG_DEF(CRTC0_VDISPLAY),
	REG_DEF(CRTC0_VSYNC),
	REG_DEF(CRTC0_GAMMA_INDEX),
	REG_DEF(CRTC0_GAMMA_DATA),
	REG_DEF(INT),
	REG_DEF(CRTC1_CFG),
	REG_DEF(CRTC1_FB_ORIGIN),
	REG_DEF(CRTC1_HDISPLAY),
	REG_DEF(CRTC1_HSYNC),
	REG_DEF(CRTC1_VDISPLAY),
	REG_DEF(CRTC1_VSYNC),
	REG_DEF(CRTC1_GAMMA_INDEX),
	REG_DEF(CRTC1_GAMMA_DATA),
};

#undef REG_DEF

static int lsdc_show_regs(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	int i;

	for (i = 0; i < ARRAY_SIZE(lsdc_regs_array); i++) {
		u32 offset = lsdc_regs_array[i].reg_offset;
		const char *name = lsdc_regs_array[i].name;

		seq_printf(m, "%s (0x%04x): 0x%08x\n",
			   name, offset, lsdc_rreg32(ldev, offset));
	}

	return 0;
}

static int lsdc_show_vblank_counter(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);

	seq_printf(m, "CRTC-0 vblank counter: %08u\n",
		   lsdc_rreg32(ldev, LSDC_CRTC0_VSYNC_COUNTER_REG));

	seq_printf(m, "CRTC-1 vblank counter: %08u\n",
		   lsdc_rreg32(ldev, LSDC_CRTC1_VSYNC_COUNTER_REG));

	return 0;
}

static int lsdc_show_scan_position(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 p0 = lsdc_rreg32(ldev, LSDC_CRTC0_SCAN_POS_REG);
	u32 p1 = lsdc_rreg32(ldev, LSDC_CRTC1_SCAN_POS_REG);

	seq_printf(m, "CRTC-0: x: %08u, y: %08u\n", p0 >> 16, p0 & 0xFFFF);
	seq_printf(m, "CRTC-1: x: %08u, y: %08u\n", p1 >> 16, p1 & 0xFFFF);

	return 0;
}

static int lsdc_show_fb_addr(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 lo, hi;
	u32 val;

	val = lsdc_rreg32(ldev, LSDC_CRTC0_CFG_REG);
	if (val & CFG_FB_IN_USING) {
		lo = lsdc_rreg32(ldev, LSDC_CRTC0_FB1_LO_ADDR_REG);
		hi = lsdc_rreg32(ldev, LSDC_CRTC0_FB1_HI_ADDR_REG);
		seq_printf(m, "CRTC-0 using fb1: 0x%x:%x\n", hi, lo);
	} else {
		lo = lsdc_rreg32(ldev, LSDC_CRTC0_FB0_LO_ADDR_REG);
		hi = lsdc_rreg32(ldev, LSDC_CRTC0_FB0_HI_ADDR_REG);
		seq_printf(m, "CRTC-0 using fb0: 0x%x:%x\n", hi, lo);
	}

	val = lsdc_rreg32(ldev, LSDC_CRTC1_CFG_REG);
	if (val & CFG_FB_IN_USING) {
		lo = lsdc_rreg32(ldev, LSDC_CRTC1_FB1_LO_ADDR_REG);
		hi = lsdc_rreg32(ldev, LSDC_CRTC1_FB1_HI_ADDR_REG);
		seq_printf(m, "CRTC-1 using fb1: 0x%x:%x\n", hi, lo);
	} else {
		lo = lsdc_rreg32(ldev, LSDC_CRTC1_FB0_LO_ADDR_REG);
		hi = lsdc_rreg32(ldev, LSDC_CRTC1_FB0_HI_ADDR_REG);
		seq_printf(m, "CRTC-1 using fb0: 0x%x:%x\n", hi, lo);
	}

	return 0;
}

static int lsdc_show_stride(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 stride0 = lsdc_rreg32(ldev, LSDC_CRTC0_STRIDE_REG);
	u32 stride1 = lsdc_rreg32(ldev, LSDC_CRTC1_STRIDE_REG);

	seq_printf(m, "PIPE-0 stride: %u\n", stride0);
	seq_printf(m, "PIPE-1 stride: %u\n", stride1);

	return 0;
}

static int lsdc_trigger_flip_fb(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *ddev = node->minor->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 val;

	val = lsdc_rreg32(ldev, LSDC_CRTC0_CFG_REG);
	lsdc_wreg32(ldev, LSDC_CRTC0_CFG_REG, val | CFG_PAGE_FLIP);
	seq_puts(m, "CRTC-0 flip triggered\n");

	val = lsdc_rreg32(ldev, LSDC_CRTC1_CFG_REG);
	lsdc_wreg32(ldev, LSDC_CRTC1_CFG_REG, val | CFG_PAGE_FLIP);
	seq_puts(m, "CRTC-1 flip triggered\n");

	return 0;
}

static struct drm_info_list lsdc_debugfs_list[] = {
	{ "chip",     lsdc_identify, 0, NULL },
	{ "clocks",   lsdc_show_clock, 0 },
	{ "mm",       lsdc_show_mm, 0, NULL },
	{ "regs",     lsdc_show_regs, 0 },
	{ "vblanks",  lsdc_show_vblank_counter, 0, NULL },
	{ "scan_pos", lsdc_show_scan_position, 0, NULL },
	{ "fb_addr",  lsdc_show_fb_addr, 0, NULL },
	{ "stride",   lsdc_show_stride, 0, NULL },
	{ "flip",     lsdc_trigger_flip_fb, 0, NULL },
};

#endif

void lsdc_debugfs_init(struct drm_minor *minor)
{
#ifdef CONFIG_DEBUG_FS
	drm_debugfs_create_files(lsdc_debugfs_list,
				 ARRAY_SIZE(lsdc_debugfs_list),
				 minor->debugfs_root,
				 minor);
#endif
}
