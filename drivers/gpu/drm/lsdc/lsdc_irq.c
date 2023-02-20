// SPDX-License-Identifier: GPL-2.0

#include <drm/drm_vblank.h>
#include "lsdc_drv.h"
#include "lsdc_regs.h"

/*
 * For the DC in ls7a2000, clearing interrupt status is achieved by
 * write "1" to LSDC_INT_REG, For the DC in ls7a1000, ls2k1000,
 * Interrupt status clear is achieved by write "0" to LSDC_INT_REG.
 * Two different hardware engineer of Loongson modify it as their will.
 */

/* For the DC in ls7a2000 */
static irqreturn_t lsdc_irq_handler(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 val;

	/* Read the interrupt status */
	val = lsdc_rreg32(ldev, LSDC_INT_REG);
	if ((val & INT_STATUS_MASK) == 0) {
		drm_warn(ddev, "no interrupt occurs\n");
		return IRQ_NONE;
	}

	ldev->irq_status = val;

	/* write "1" to clear the interrupt status */
	lsdc_wreg32(ldev, LSDC_INT_REG, val);

	return IRQ_WAKE_THREAD;
}

/* For the DC in ls7a1000 and ls2k1000  */
static irqreturn_t lsdc_irq_handler_legacy(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 val;

	/* Read the interrupt status */
	val = lsdc_rreg32(ldev, LSDC_INT_REG);
	if ((val & INT_STATUS_MASK) == 0) {
		drm_warn(ddev, "no interrupt occurs\n");
		return IRQ_NONE;
	}

	ldev->irq_status = val;

	/* write "0" to clear the interrupt status */
	lsdc_wreg32(ldev, LSDC_INT_REG, val & ~INT_STATUS_MASK);

	return IRQ_WAKE_THREAD;
}

irq_handler_t lsdc_get_irq_handler(struct lsdc_device *ldev)
{
	const struct lsdc_desc *descp = ldev->descp;

	if (descp->chip == CHIP_LS7A2000)
		return lsdc_irq_handler;

	return lsdc_irq_handler_legacy;
}

irqreturn_t lsdc_irq_thread_handler(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct drm_crtc *crtc;

	if (ldev->irq_status & INT_CRTC0_VSYNC) {
		crtc = drm_crtc_from_index(ddev, 0);
		drm_crtc_handle_vblank(crtc);
	}

	if (ldev->irq_status & INT_CRTC1_VSYNC) {
		crtc = drm_crtc_from_index(ddev, 1);
		drm_crtc_handle_vblank(crtc);
	}

	return IRQ_HANDLED;
}
