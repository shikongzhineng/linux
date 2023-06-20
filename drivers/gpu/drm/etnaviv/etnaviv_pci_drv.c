// SPDX-License-Identifier: GPL-2.0

#include <linux/pci.h>

#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_pci_drv.h"

static int etnaviv_pci_probe(struct pci_dev *pdev,
			     const struct pci_device_id *ent)
{
	struct device *dev = &pdev->dev;
	void __iomem *mmio;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret) {
		dev_err(dev, "failed to enable\n");
		return ret;
	}

	pci_set_master(pdev);

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	/* Map registers, assume the PCI bar 0 contain the registers */
	mmio = pcim_iomap(pdev, 0, 0);
	if (IS_ERR(mmio))
		return PTR_ERR(mmio);

	ret = etnaviv_gpu_driver_create(dev, mmio, pdev->irq, false, false);
	if (ret)
		return ret;

	return etnaviv_drm_bind(dev, false);
}

static void etnaviv_pci_remove(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;

	etnaviv_drm_unbind(dev, false);

	etnaviv_gpu_driver_destroy(dev, false);

	pci_clear_master(pdev);
}

static const struct pci_device_id etnaviv_pci_id_lists[] = {
	{PCI_VDEVICE(LOONGSON, 0x7a15)},
	{PCI_VDEVICE(LOONGSON, 0x7a05)},
	{ }
};

static struct pci_driver etnaviv_pci_driver = {
	.name = "etnaviv",
	.id_table = etnaviv_pci_id_lists,
	.probe = etnaviv_pci_probe,
	.remove = etnaviv_pci_remove,
	.driver.pm = pm_ptr(&etnaviv_gpu_pm_ops),
};

int etnaviv_register_pci_driver(void)
{
	return pci_register_driver(&etnaviv_pci_driver);
}

void etnaviv_unregister_pci_driver(void)
{
	pci_unregister_driver(&etnaviv_pci_driver);
}

MODULE_DEVICE_TABLE(pci, etnaviv_pci_id_lists);
