// SPDX-License-Identifier: GPL-2.0+

#include "lsdc_drv.h"

/*
 * Processor ID (implementation) values for bits 15:8 of the PRID register.
 */
#define LOONGSON_CPU_PRID_IMP_MASK	0xff00
/*
 * Particular Revision values for bits 7:0 of the PRID register.
 */
#define LOONGSON_CPU_PRID_REV_MASK      0x00ff

#define LOONGARCH_CPU_PRID_LS2K1000     0xa000
#define LOONGARCH_CPU_PRID_LS2K2000     0xb000
#define LOONGARCH_CPU_PRID_LS3A5000     0xc000

#define LOONGSON_CPU_PRID_IMP_LS2K      0x6100  /* Loongson 2K series SoC */

#define LOONGARCH_CPUCFG_PRID_REG       0x0

unsigned int loongson_cpu_get_prid(void)
{
	unsigned int prid = 0;

#if defined(__loongarch__)
	__asm__ volatile("cpucfg %0, %1\n\t"
			: "=&r"(prid)
			: "r"(LOONGARCH_CPUCFG_PRID_REG)
			);
#endif

#if defined(__mips__)
	__asm__ volatile("mfc0\t%0, $15\n\t"
			: "=r" (prid)
			);
#endif

	return prid;
}

/* LS2K2000 has only LoongArch edition (LA364) */
bool lsdc_is_ls2k2000(void)
{
	unsigned int prid = loongson_cpu_get_prid();

	if ((prid & LOONGSON_CPU_PRID_IMP_MASK) == LOONGARCH_CPU_PRID_LS2K2000)
		return true;

	return false;
}

/*
 * LS2K1000 has loongarch edition(LA264) and mips edition(mips64r2),
 * CPU core and instruction set change, but remain is basically same.
 */
bool lsdc_is_ls2k1000(void)
{
	unsigned int prid;

	prid = loongson_cpu_get_prid();

#if defined(__loongarch__)
	if ((prid & LOONGSON_CPU_PRID_IMP_MASK) == LOONGARCH_CPU_PRID_LS2K1000)
		return true;
#endif

#if defined(__mips__)
	if ((prid & LOONGSON_CPU_PRID_IMP_MASK) == LOONGSON_CPU_PRID_IMP_LS2K)
		return true;
#endif

	return false;
}
