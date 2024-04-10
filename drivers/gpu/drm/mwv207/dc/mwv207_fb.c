/*
* SPDX-License-Identifier: GPL
*
* Copyright (c) 2020 ChangSha JingJiaMicro Electronics Co., Ltd.
* All rights reserved.
*
* Author:
*      shanjinkui <shanjinkui@jingjiamicro.com>
*
* The software and information contained herein is proprietary and
* confidential to JingJiaMicro Electronics. This software can only be
* used by JingJiaMicro Electronics Corporation. Any use, reproduction,
* or disclosure without the written permission of JingJiaMicro
* Electronics Corporation is strictly prohibited.
*/
#include <linux/version.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_print.h>
#include "mwv207.h"
#include "mwv207_bo.h"
#include "mwv207_drm.h"
#include "mwv207_gem.h"
#include "mwv207_kms.h"

static struct fb_ops mwv207_fb_ops = {
	.owner          = THIS_MODULE,
	DRM_FB_HELPER_DEFAULT_OPS,
	.fb_fillrect    = drm_fb_helper_cfb_fillrect,
	.fb_copyarea    = drm_fb_helper_cfb_copyarea,
	.fb_imageblit   = drm_fb_helper_cfb_imageblit,
};

static int mwv207_drm_fb_helper_init(struct drm_device *dev,
		struct drm_fb_helper *fb_helper,
		unsigned int crtc_count,
		unsigned int max_conn_count)
{
	return drm_fb_helper_init(dev, fb_helper);
}

static int mwv207_fb_create(struct drm_fb_helper *fb_helper,
		struct drm_fb_helper_surface_size *sizes)
{
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_framebuffer *fb;
	struct mwv207_device *jdev;
	struct mwv207_bo *jbo;
	struct fb_info *info;
	u32 bytes;
	void *logical;
	int ret;

	jdev = fb_helper->dev->dev_private;
	memset(&mode_cmd, 0, sizeof(mode_cmd));
	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	if (sizes->surface_bpp == 24)
		sizes->surface_bpp = 32;

	mode_cmd.pitches[0] = mode_cmd.width * ((sizes->surface_bpp + 7)/8);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
			sizes->surface_depth);

	info = drm_fb_helper_alloc_fbi(fb_helper);
	if (IS_ERR(info))
		return  PTR_ERR(info);

	bytes  = mode_cmd.pitches[0] * mode_cmd.height;
	bytes = ALIGN(bytes, PAGE_SIZE);
	ret = mwv207_bo_create(jdev, bytes, 0x10000, ttm_bo_type_kernel,
			0x2, (1<<0), &jbo);
	if (ret)
		return ret;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (fb == NULL) {
		ret = -ENOMEM;
		goto free_bo;
	}

	ret = mwv207_framebuffer_init(jdev, fb, &mode_cmd, mwv207_gem_from_bo(jbo));
	if (ret)
		goto free_fb;

	ret = mwv207_bo_reserve(jbo, true);
	if (ret)
		goto free_fb;
	ret = mwv207_bo_pin_reserved(jbo, 0x2);
	if (ret)
		goto unreserve_bo;

	ret = mwv207_bo_kmap_reserved(jbo, &logical);
	if (ret)
		goto unpin_bo;

	memset(logical, 0x0, bytes);

	fb_helper->fb = fb;
	info->skip_vt_switch = true;
	info->fbops = &mwv207_fb_ops;
	info->screen_size = fb->height * fb->pitches[0];
	info->fix.smem_len = info->screen_size;
	info->screen_base = logical;

	drm_fb_helper_fill_info(info, fb_helper, sizes);

	mwv207_bo_unreserve(jbo);
	return 0;

unpin_bo:
	mwv207_bo_unpin(jbo);
unreserve_bo:
	mwv207_bo_unreserve(jbo);
free_fb:
	kfree(fb);
free_bo:
	mwv207_bo_unref(jbo);
	return ret;
}

static const struct drm_fb_helper_funcs mwv207_fb_helper_funcs = {
	.fb_probe = mwv207_fb_create,
};

int mwv207_fbdev_init(struct mwv207_device *jdev)
{
	struct drm_fb_helper *fb_helper;
	int preferred_bpp = 32;
	int ret;

	fb_helper = devm_kzalloc(jdev->dev, sizeof(*fb_helper), GFP_KERNEL);
	if (!fb_helper)
		return -ENOMEM;

	drm_fb_helper_prepare(&jdev->base, fb_helper, &mwv207_fb_helper_funcs);
	ret = mwv207_drm_fb_helper_init(&jdev->base, fb_helper,
			jdev->base.num_crtcs, jdev->base.num_crtcs);
	if (ret) {
		DRM_ERROR("Failed to initialize fbdev helper\n");
		return ret;
	}

	ret = drm_fb_helper_initial_config(fb_helper, preferred_bpp);
	if (ret) {
		DRM_ERROR("Failed to set fbdev configuration\n");
		goto err;
	}

	jdev->fb_helper = fb_helper;
	return 0;
err:
	drm_fb_helper_fini(fb_helper);
	return ret;
}

void mwv207_fbdev_fini(struct mwv207_device *jdev)
{
	struct drm_framebuffer *fb;
	struct mwv207_bo *jbo;
	int ret;

	drm_fb_helper_unregister_fbi(jdev->fb_helper);

	fb = jdev->fb_helper ? jdev->fb_helper->fb : NULL;
	if (fb == NULL)
		return;

	jbo = mwv207_bo_from_gem(fb->obj[0]);
	ret = mwv207_bo_reserve(jbo, true);
	if (ret == 0) {
		mwv207_bo_kunmap_reserved(jbo);
		mwv207_bo_unpin_reserved(jbo);
		mwv207_bo_unreserve(jbo);
		mwv207_gem_object_put(fb->obj[0]);
		drm_framebuffer_cleanup(fb);
	}

	drm_fb_helper_fini(jdev->fb_helper);
}
