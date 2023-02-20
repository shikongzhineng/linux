/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LSDC_PLL_H__
#define __LSDC_PLL_H__

#include <drm/drm_device.h>

/*
 * Loongson Pixel PLL hardware structure
 *
 * refclk: reference frequency, 100 MHz from external oscillator
 * outclk: output frequency desired.
 *
 *
 *               L1       Fref                      Fvco     L2
 * refclk   +-----------+      +------------------+      +---------+   outclk
 * ---+---> | Prescaler | ---> | Clock Multiplier | ---> | divider | -------->
 *    |     +-----------+      +------------------+      +---------+     ^
 *    |           ^                      ^                    ^          |
 *    |           |                      |                    |          |
 *    |           |                      |                    |          |
 *    |        div_ref                 loopc               div_out       |
 *    |                                                                  |
 *    +--- sel_out (bypass above software configurable clock if set) ----+
 *
 *  sel_out: PLL clock output selector (for debug purpose only).
 *
 *  If sel_out == 1, it will take refclk as output directly,
 *  the L1 Prescaler and the out divider will be bypassed.
 *
 *  If sel_out == 0, then outclk = refclk / div_ref * loopc / div_out;
 *
 * PLL working requirements:
 *
 *  1) 20 MHz <= refclk / div_ref <= 40Mhz
 *  2) 1.2 GHz <= refclk /div_out * loopc <= 3.2 Ghz
 */

struct lsdc_pll_parms {
	unsigned int div_ref;
	unsigned int loopc;
	unsigned int div_out;
};

struct lsdc_pll;

struct lsdc_pixpll_funcs {
	int (*setup)(struct lsdc_pll * const this);
	bool (*compute)(struct lsdc_pll * const this,
			unsigned int clock,
			struct lsdc_pll_parms *pout);
	int (*update)(struct lsdc_pll * const this,
		      struct lsdc_pll_parms const *pin);
	unsigned int (*get_clock_rate)(struct lsdc_pll * const this,
				       struct lsdc_pll_parms *pout);
};

struct lsdc_pll {
	const struct lsdc_pixpll_funcs *funcs;
	struct drm_device *ddev;
	void __iomem *mmio;

	/* PLL register offset */
	u32 reg_base;
	/* PLL register size in bytes */
	u32 reg_size;

	/* 100000kHz, fixed on all board found */
	unsigned int ref_clock;

	unsigned int index;
};

int lsdc_pixpll_init(struct lsdc_pll * const this,
		     struct drm_device *ddev,
		     unsigned int index);

#endif
