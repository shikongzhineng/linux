// SPDX-License-Identifier: GPL-2.0

#include <linux/i2c.h>
#include <drm/drm_managed.h>
#include "lsdc_drv.h"
#include "lsdc_regs.h"

/*
 * ls7a_gpio_i2c_set - set the state of a gpio pin indicated by mask
 * @mask: gpio pin mask
 * @state: "0" for low, "1" for high
 */
static void ls7a_gpio_i2c_set(struct lsdc_i2c * const li2c, int mask, int state)
{
	struct lsdc_device *ldev = to_lsdc(li2c->ddev);
	unsigned long flags;
	u8 val;

	spin_lock_irqsave(&ldev->reglock, flags);

	if (state) {
		/*
		 * Setting this pin as input directly, write 1 for Input.
		 * The external pull-up resistor will pull the level up
		 */
		val = readb(li2c->dir_reg);
		val |= mask;
		writeb(val, li2c->dir_reg);
	} else {
		/* First set this pin as output, write 0 for Output */
		val = readb(li2c->dir_reg);
		val &= ~mask;
		writeb(val, li2c->dir_reg);

		/* Then, make this pin output 0 */
		val = readb(li2c->dat_reg);
		val &= ~mask;
		writeb(val, li2c->dat_reg);
	}

	spin_unlock_irqrestore(&ldev->reglock, flags);
}

/*
 * ls7a_gpio_i2c_get - read value back from the gpio pin indicated by mask
 * @mask: gpio pin mask
 * return "0" for low, "1" for high
 */
static int ls7a_gpio_i2c_get(struct lsdc_i2c * const li2c, int mask)
{
	struct lsdc_device *ldev = to_lsdc(li2c->ddev);
	unsigned long flags;
	u8 val;

	spin_lock_irqsave(&ldev->reglock, flags);

	/* First set this pin as input */
	val = readb(li2c->dir_reg);
	val |= mask;
	writeb(val, li2c->dir_reg);

	/* Then get level state from this pin */
	val = readb(li2c->dat_reg);

	spin_unlock_irqrestore(&ldev->reglock, flags);

	return (val & mask) ? 1 : 0;
}

static void ls7a_i2c_set_sda(void *i2c, int state)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	/* set state on the li2c->sda pin */
	return ls7a_gpio_i2c_set(li2c, li2c->sda, state);
}

static void ls7a_i2c_set_scl(void *i2c, int state)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	/* set state on the li2c->scl pin */
	return ls7a_gpio_i2c_set(li2c, li2c->scl, state);
}

static int ls7a_i2c_get_sda(void *i2c)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	/* read value from the li2c->sda pin */
	return ls7a_gpio_i2c_get(li2c, li2c->sda);
}

static int ls7a_i2c_get_scl(void *i2c)
{
	struct lsdc_i2c * const li2c = (struct lsdc_i2c *)i2c;
	/* read the value from the li2c->scl pin */
	return ls7a_gpio_i2c_get(li2c, li2c->scl);
}

static void lsdc_destroy_i2c(struct drm_device *ddev, void *data)
{
	struct lsdc_i2c *li2c = (struct lsdc_i2c *)data;

	if (li2c) {
		i2c_del_adapter(&li2c->adapter);
		kfree(li2c);
	}
}

/*
 * The DC in ls7a1000/ls7a2000 have builtin gpio hardware
 *
 * @base: gpio reg base
 * @index: output channel index, 0 for DVO0, 1 for DVO1
 */
struct lsdc_i2c *lsdc_create_i2c_chan(struct drm_device *ddev,
				      void *base,
				      unsigned int index)
{
	struct i2c_adapter *adapter;
	struct lsdc_i2c *li2c;
	int ret;

	li2c = kzalloc(sizeof(*li2c), GFP_KERNEL);
	if (!li2c)
		return ERR_PTR(-ENOMEM);

	if (index == 0) {
		li2c->sda = 0x01;  /* pin 0 */
		li2c->scl = 0x02;  /* pin 1 */
	} else if (index == 1) {
		li2c->sda = 0x04;  /* pin 2 */
		li2c->scl = 0x08;  /* pin 3 */
	}

	li2c->reg_base = base;
	li2c->ddev = ddev;
	li2c->dir_reg = li2c->reg_base + LS7A_DC_GPIO_DIR_REG;
	li2c->dat_reg = li2c->reg_base + LS7A_DC_GPIO_DAT_REG;

	li2c->bit.setsda = ls7a_i2c_set_sda;
	li2c->bit.setscl = ls7a_i2c_set_scl;
	li2c->bit.getsda = ls7a_i2c_get_sda;
	li2c->bit.getscl = ls7a_i2c_get_scl;
	li2c->bit.udelay = 5;
	li2c->bit.timeout = usecs_to_jiffies(2200);
	li2c->bit.data = li2c;

	adapter = &li2c->adapter;
	adapter->algo_data = &li2c->bit;
	adapter->owner = THIS_MODULE;
	adapter->class = I2C_CLASS_DDC;
	adapter->dev.parent = ddev->dev;
	adapter->nr = -1;

	snprintf(adapter->name, sizeof(adapter->name), "lsdc-i2c%u", index);

	i2c_set_adapdata(adapter, li2c);

	ret = i2c_bit_add_bus(adapter);
	if (ret) {
		kfree(li2c);
		return ERR_PTR(ret);
	}

	ret = drmm_add_action_or_reset(ddev, lsdc_destroy_i2c, li2c);
	if (ret)
		return NULL;

	drm_info(ddev, "%s(sda=%u, scl=%u) created for connector-%u\n",
		 adapter->name, li2c->sda, li2c->scl, index);

	return li2c;
}

static int lsdc_get_i2c_id(struct lsdc_device *ldev, int index)
{
	return index;
}

struct i2c_adapter *lsdc_get_i2c_adapter(struct lsdc_device *ldev,
					 int index)
{
	const struct lsdc_desc *descp = ldev->descp;
	struct lsdc_display_pipe *dispipe = &ldev->dispipe[index];

	if (descp->has_builtin_i2c) {
		struct lsdc_i2c *li2c = dispipe->li2c;

		if (li2c)
			return &li2c->adapter;
	}

	return i2c_get_adapter(lsdc_get_i2c_id(ldev, index));
}
