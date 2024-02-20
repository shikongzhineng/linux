/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ASM_MMIOWB_H
#define _ASM_MMIOWB_H

#define mmiowb() wmb()

#include <asm/thread_info.h>
#include <asm-generic/mmiowb.h>

#endif	/* _ASM_MMIOWB_H */
