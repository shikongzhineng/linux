// SPDX-License-Identifier: GPL-2.0+

#include <linux/pci.h>
#include <linux/of_address.h>
#include <drm/drm_aperture.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include "lsdc_drv.h"
#include "lsdc_ttm.h"

#define DRIVER_AUTHOR		"Sui Jingfeng <suijingfeng@loongson.cn>"
#define DRIVER_NAME		"lsdc"
#define DRIVER_DESC		"drm driver for loongson's display controller"
#define DRIVER_DATE		"20220701"
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

static const struct lsdc_desc dc_in_ls7a1000 = {
	.chip = CHIP_LS7A1000,
	.num_of_crtc = LSDC_NUM_CRTC,
	.max_pixel_clk = 200000,
	.max_width = 2048,
	.max_height = 2048,
	.num_of_hw_cursor = 1,
	.hw_cursor_w = 32,
	.hw_cursor_h = 32,
	.pitch_align = 256,
	.mc_bits = 40,
	.has_vblank_counter = false,
	.has_scan_pos = true,
	.has_builtin_i2c = true,
	.has_vram = true,
	.has_hpd_reg = false,
	.is_soc = false,
};

static const struct lsdc_desc dc_in_ls7a2000 = {
	.chip = CHIP_LS7A2000,
	.num_of_crtc = LSDC_NUM_CRTC,
	.max_pixel_clk = 350000,
	.max_width = 4096,
	.max_height = 4096,
	.num_of_hw_cursor = 2,
	.hw_cursor_w = 64,
	.hw_cursor_h = 64,
	.pitch_align = 64,
	.mc_bits = 40, /* support 48, but use 40 for backward compatibility */
	.has_vblank_counter = true,
	.has_scan_pos = true,
	.has_builtin_i2c = true,
	.has_vram = true,
	.has_hpd_reg = true,
	.is_soc = false,
};

static const struct lsdc_desc dc_in_ls2k1000 = {
	.chip = CHIP_LS2K1000,
	.num_of_crtc = LSDC_NUM_CRTC,
	.max_pixel_clk = 200000,
	.max_width = 2048,
	.max_height = 2048,
	.num_of_hw_cursor = 1,
	.hw_cursor_w = 32,
	.hw_cursor_h = 32,
	.pitch_align = 256,
	.mc_bits = 40,
	.has_vblank_counter = false,
	.has_scan_pos = true,
	.has_builtin_i2c = false,
	.has_vram = false,
	.has_hpd_reg = false,
	.is_soc = true,
};

static const struct lsdc_desc dc_in_ls2k2000 = {
	.chip = CHIP_LS2K2000,
	.num_of_crtc = LSDC_NUM_CRTC,
	.max_pixel_clk = 350000,
	.max_width = 4096,
	.max_height = 4096,
	.num_of_hw_cursor = 2,
	.hw_cursor_w = 64,
	.hw_cursor_h = 64,
	.pitch_align = 256,
	.mc_bits = 40,
	.has_vblank_counter = true,
	.has_scan_pos = true,
	.has_builtin_i2c = true,
	.has_vram = false,
	.has_hpd_reg = true,
	.is_soc = true,
};

const char *chip_to_str(enum loongson_chip_family chip)
{
	if (chip == CHIP_LS7A2000)
		return "LS7A2000";

	if (chip == CHIP_LS7A1000)
		return "LS7A1000";

	if (chip == CHIP_LS2K1000)
		return "LS2K1000";

	if (chip == CHIP_LS2K2000)
		return "LS2K2000";

	return "unknown";
}

DEFINE_DRM_GEM_FOPS(lsdc_gem_fops);

static const struct drm_driver lsdc_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
	.fops = &lsdc_gem_fops,

	/* for downstream userspace driver compatibility */
	.name = "loongson-drm",
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,

	.debugfs_init = lsdc_debugfs_init,
	.dumb_create = lsdc_dumb_create,
	.dumb_map_offset = lsdc_dumb_map_offset,
	.gem_prime_mmap = drm_gem_prime_mmap,
};

static enum drm_mode_status
lsdc_bo_size_valid(struct drm_device *ddev,
		   const struct drm_display_mode *mode)
{
	struct lsdc_device *ldev = to_lsdc(ddev);
	const unsigned long max_bpp = 4; /* DRM_FORMAT_XRGB8888 */
	unsigned long fbsize, fbpages, max_fbpages;

	max_fbpages = (ldev->vram_size / 2) >> PAGE_SHIFT;
	fbsize = mode->hdisplay * mode->vdisplay * max_bpp;
	fbpages = DIV_ROUND_UP(fbsize, PAGE_SIZE);

	if (fbpages > max_fbpages)
		return MODE_MEM;

	return MODE_OK;
}

static const struct drm_mode_config_funcs lsdc_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
	.mode_valid = lsdc_bo_size_valid,
};

static int lsdc_modeset_init(struct lsdc_device *ldev,
			     const struct lsdc_desc *descp)
{
	struct drm_device *ddev = &ldev->base;
	unsigned int num_crtc = descp->num_of_crtc;
	unsigned int i;
	int ret;

	for (i = 0; i < num_crtc; i++) {
		ret = lsdc_create_output(ldev, i);
		if (ret)
			return ret;
	}

	for (i = 0; i < num_crtc; i++) {
		struct lsdc_display_pipe *dispipe = &ldev->dispipe[i];
		struct lsdc_pll *pixpll = &dispipe->pixpll;
		struct drm_plane *primary = &dispipe->primary;
		struct drm_plane *cursor = &dispipe->cursor.base;
		struct drm_crtc *crtc = &dispipe->crtc;

		dispipe->index = i;

		ret = lsdc_pixpll_init(pixpll, ddev, i);
		if (ret)
			return ret;

		ret = lsdc_plane_init(ldev, primary, DRM_PLANE_TYPE_PRIMARY, i);
		if (ret)
			return ret;

		ret = lsdc_plane_init(ldev, cursor, DRM_PLANE_TYPE_CURSOR, i);
		if (ret)
			return ret;

		ret = lsdc_crtc_init(ddev, crtc, i, primary, cursor);
		if (ret)
			return ret;

		drm_info(ddev, "display pipe %u initialized\n", i);
	}

	drm_mode_config_reset(ddev);

	return 0;
}

static int lsdc_mode_config_init(struct drm_device *ddev,
				 const struct lsdc_desc *descp)
{
	int ret;

	ret = drmm_mode_config_init(ddev);
	if (ret)
		return ret;

	ddev->mode_config.funcs = &lsdc_mode_config_funcs;
	ddev->mode_config.min_width = 1;
	ddev->mode_config.min_height = 1;
	ddev->mode_config.max_width = descp->max_width * LSDC_NUM_CRTC;
	ddev->mode_config.max_height = descp->max_height * LSDC_NUM_CRTC;
	ddev->mode_config.preferred_depth = 24;
	ddev->mode_config.prefer_shadow = descp->has_vram;

	ddev->mode_config.cursor_width = descp->hw_cursor_h;
	ddev->mode_config.cursor_height = descp->hw_cursor_h;

	if (descp->has_vblank_counter)
		ddev->max_vblank_count = 0xffffffff;

	return ret;
}

static const struct lsdc_desc *
lsdc_detect_chip(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	enum loongson_chip_family chip = ent->driver_data;

	if (chip == CHIP_LS7A1000) {
		if (lsdc_is_ls2k1000())
			return &dc_in_ls2k1000;

		return &dc_in_ls7a1000;
	}

	if (chip == CHIP_LS7A2000) {
		if (lsdc_is_ls2k2000())
			return &dc_in_ls2k2000;

		return &dc_in_ls7a2000;
	}

	return NULL;
}

static int lsdc_get_dedicated_vram(struct lsdc_device *ldev,
				   const struct lsdc_desc *descp)
{
	struct drm_device *ddev = &ldev->base;
	struct pci_dev *gpu;
	resource_size_t base, size;

	/*
	 * The GPU and display controller in LS7A1000/LS7A2000 are separated
	 * PCIE devices, they are two devices not one. The DC is a pci device,
	 * but it don't have a dedicate VRAM bar, the BIOS engineer choose to
	 * assign the VRAM to the gpu device. Sadly, after years application,
	 * this decision form as a convention for loongson integrate graphics.
	 * For LS7A1000 and LS7A2000, bar 2 of GPU device contain the VRAM,
	 * both the GPU and the DC can make use of the VRAM depend on how DRM
	 * device driver is written. Therefore, we have to do some tricks here.
	 */
	if (descp->chip == CHIP_LS7A1000)
		gpu = pci_get_device(PCI_VENDOR_ID_LOONGSON, 0x7A15, NULL);
	else if (descp->chip == CHIP_LS7A2000)
		gpu = pci_get_device(PCI_VENDOR_ID_LOONGSON, 0x7A25, NULL);

	if (!gpu) {
		drm_warn(ddev, "No GPU device found\n");
		return -ENODEV;
	}

	base = pci_resource_start(gpu, 2);
	size = pci_resource_len(gpu, 2);

	ldev->vram_base = base;
	ldev->vram_size = size;

	drm_info(ddev, "dedicated vram start: 0x%llx, size: %uMB\n",
		 (u64)base, (u32)(size >> 20));

	return 0;
}

static int lsdc_of_get_reserved_ram(struct lsdc_device *ldev)
{
	struct drm_device *ddev = &ldev->base;
	unsigned long size = 0;
	struct device_node *node;
	struct resource r;
	int ret;

	node = of_parse_phandle(ddev->dev->of_node, "memory-region", 0);
	if (!node) {
		drm_err(ddev, "No memory-region property or no DT, abort\n");
		return -ENOENT;
	}

	ret = of_address_to_resource(node, 0, &r);
	of_node_put(node);
	if (ret)
		return ret;

	size = r.end - r.start + 1;

	ldev->vram_base = r.start;
	ldev->vram_size = size;

	drm_info(ddev, "using VRAM carveout: %lx@%pa\n", size, &r.start);

	return 0;
}

static struct lsdc_device *
lsdc_create_device(struct pci_dev *pdev,
		   const struct pci_device_id *ent,
		   const struct drm_driver *drv)
{
	struct lsdc_device *ldev;
	struct drm_device *ddev;
	const struct lsdc_desc *descp;
	int ret;

	ldev = devm_drm_dev_alloc(&pdev->dev, drv, struct lsdc_device, base);
	if (IS_ERR(ldev))
		return ldev;

	ddev = &ldev->base;

	pci_set_drvdata(pdev, ddev);

	descp = lsdc_detect_chip(pdev, ent);
	if (!descp) {
		drm_err(ddev, "Not known device, the driver need update!\n");
		return NULL;
	}

	drm_info(ddev, "%s found, revision: %u", chip_to_str(descp->chip), pdev->revision);

	ldev->descp = descp;

	spin_lock_init(&ldev->reglock);

	/* BAR 0 the DC device contains registers */
	ldev->reg_base = pcim_iomap(pdev, 0, 0);
	if (!ldev->reg_base)
		return ERR_PTR(-EIO);

	if (descp->has_vram)
		ret = lsdc_get_dedicated_vram(ldev, descp);
	else
		ret = lsdc_of_get_reserved_ram(ldev);

	if (ret) {
		drm_err(ddev, "Init VRAM failed: %d\n", ret);
		return ERR_PTR(ret);
	}

	ret = drm_aperture_remove_conflicting_framebuffers(ldev->vram_base,
							   ldev->vram_size,
							   false,
							   drv);
	if (ret) {
		drm_err(ddev, "remove firmware framebuffers failed: %d\n", ret);
		return ERR_PTR(ret);
	}

	ret = lsdc_ttm_init(ldev);
	if (ret) {
		drm_err(ddev, "memory manager init failed: %d\n", ret);
		return ERR_PTR(ret);
	}

	ret = lsdc_mode_config_init(ddev, descp);
	if (ret)
		return ERR_PTR(ret);

	ret = lsdc_modeset_init(ldev, descp);
	if (ret)
		return ERR_PTR(ret);

	ret = drm_vblank_init(ddev, descp->num_of_crtc);
	if (ret)
		return ERR_PTR(ret);

	ret = request_threaded_irq(pdev->irq,
				   lsdc_get_irq_handler(ldev),
				   lsdc_irq_thread_handler,
				   IRQF_ONESHOT,
				   dev_name(ddev->dev),
				   ddev);
	if (ret) {
		drm_err(ddev, "Failed to register lsdc interrupt\n");
		return ERR_PTR(ret);
	}

	drm_kms_helper_poll_init(ddev);

	return ldev;
}

static int lsdc_pci_probe(struct pci_dev *pdev,
			  const struct pci_device_id *ent)
{
	struct drm_device *ddev;
	struct lsdc_device *ldev;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(40));
	if (ret)
		return ret;

	ldev = lsdc_create_device(pdev, ent, &lsdc_drm_driver);
	if (IS_ERR(ldev))
		return PTR_ERR(ldev);

	ddev = &ldev->base;

	ret = drm_dev_register(ddev, 0);
	if (ret)
		return ret;

	drm_fbdev_generic_setup(ddev, 32);

	return 0;
}

static void lsdc_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *ddev = pci_get_drvdata(pdev);

	drm_dev_unregister(ddev);
	drm_atomic_helper_shutdown(ddev);
}

static int lsdc_drm_freeze(struct drm_device *ddev)
{
	int error;

	error = drm_mode_config_helper_suspend(ddev);
	if (error)
		return error;

	pci_save_state(to_pci_dev(ddev->dev));

	return 0;
}

static int lsdc_drm_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);

	return drm_mode_config_helper_resume(ddev);
}

static int lsdc_pm_freeze(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);

	return lsdc_drm_freeze(ddev);
}

static int lsdc_pm_thaw(struct device *dev)
{
	return lsdc_drm_resume(dev);
}

static int lsdc_pm_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	int error;

	error = lsdc_pm_freeze(dev);
	if (error)
		return error;

	pci_save_state(pdev);
	/* Shut down the device */
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return 0;
}

static int lsdc_pm_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	if (pcim_enable_device(pdev))
		return -EIO;

	pci_set_power_state(pdev, PCI_D0);

	pci_restore_state(pdev);

	return lsdc_pm_thaw(dev);
}

static const struct dev_pm_ops lsdc_pm_ops = {
	.suspend = lsdc_pm_suspend,
	.resume = lsdc_pm_resume,
	.freeze = lsdc_pm_freeze,
	.thaw = lsdc_pm_thaw,
	.poweroff = lsdc_pm_freeze,
	.restore = lsdc_pm_resume,
};

static const struct pci_device_id lsdc_pciid_list[] = {
	{PCI_VENDOR_ID_LOONGSON, 0x7a06, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_LS7A1000},
	{PCI_VENDOR_ID_LOONGSON, 0x7a36, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_LS7A2000},
	{0, 0, 0, 0, 0, 0, 0}
};

static struct pci_driver lsdc_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = lsdc_pciid_list,
	.probe = lsdc_pci_probe,
	.remove = lsdc_pci_remove,
	.driver.pm = &lsdc_pm_ops,
};

static int __init lsdc_module_init(void)
{
	struct pci_dev *pdev = NULL;

	if (drm_firmware_drivers_only())
		return -ENODEV;

	while ((pdev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, pdev))) {
		/* Multiple video card workaround */
		if (pdev->vendor != PCI_VENDOR_ID_LOONGSON) {
			pr_info("Discrete graphic card detected, abort\n");
			return 0;
		}
	}

	return pci_register_driver(&lsdc_pci_driver);
}
module_init(lsdc_module_init);

static void __exit lsdc_module_exit(void)
{
	pci_unregister_driver(&lsdc_pci_driver);
}
module_exit(lsdc_module_exit);

MODULE_DEVICE_TABLE(pci, lsdc_pciid_list);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
