/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *   schultz.ryan@gmail.com, http://rschultz.ath.cx/code.php               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02111-1307 USA.            *
 ***************************************************************************/

#ifndef __PSXHW_H__
#define __PSXHW_H__

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"
#include "sio.h"
#include "psxcounters.h"

#define HW_DMA0_MADR (psxHuint32_tref(0x1080)) // MDEC in DMA
#define HW_DMA0_BCR  (psxHuint32_tref(0x1084))
#define HW_DMA0_CHCR (psxHuint32_tref(0x1088))

#define HW_DMA1_MADR (psxHuint32_tref(0x1090)) // MDEC out DMA
#define HW_DMA1_BCR  (psxHuint32_tref(0x1094))
#define HW_DMA1_CHCR (psxHuint32_tref(0x1098))

#define HW_DMA2_MADR (psxHuint32_tref(0x10a0)) // GPU DMA
#define HW_DMA2_BCR  (psxHuint32_tref(0x10a4))
#define HW_DMA2_CHCR (psxHuint32_tref(0x10a8))

#define HW_DMA3_MADR (psxHuint32_tref(0x10b0)) // CDROM DMA
#define HW_DMA3_BCR  (psxHuint32_tref(0x10b4))
#define HW_DMA3_CHCR (psxHuint32_tref(0x10b8))

#define HW_DMA4_MADR (psxHuint32_tref(0x10c0)) // SPU DMA
#define HW_DMA4_BCR  (psxHuint32_tref(0x10c4))
#define HW_DMA4_CHCR (psxHuint32_tref(0x10c8))

#define HW_DMA6_MADR (psxHuint32_tref(0x10e0)) // GPU DMA (OT)
#define HW_DMA6_BCR  (psxHuint32_tref(0x10e4))
#define HW_DMA6_CHCR (psxHuint32_tref(0x10e8))

#define HW_DMA_PCR   (psxHuint32_tref(0x10f0))
#define HW_DMA_ICR   (psxHuint32_tref(0x10f4))

#define HW_DMA_ICR_BUS_ERROR     (1<<15)
#define HW_DMA_ICR_GLOBAL_ENABLE (1<<23)
#define HW_DMA_ICR_IRQ_SENT      (1<<31)

#define DMA_INTERRUPT(n) { \
	uint32_t icr = SWAPu32(HW_DMA_ICR); \
	if (icr & (1 << (16 + n))) { \
		icr |= 1 << (24 + n); \
		if (icr & HW_DMA_ICR_GLOBAL_ENABLE && !(icr & HW_DMA_ICR_IRQ_SENT)) { \
			psxHuint32_tref(0x1070) |= SWAP32(8); \
			icr |= HW_DMA_ICR_IRQ_SENT; \
		} \
		HW_DMA_ICR = SWAP32(icr); \
		ResetIoCycle(); \
	} \
}

void psxHwReset(void);
uint8_t   psxHwRead8 (uint32_t add);
uint16_t  psxHwRead16(uint32_t add);
uint32_t  psxHwRead32(uint32_t add);
void psxHwWrite8 (uint32_t add, uint8_t  value);
void psxHwWrite16(uint32_t add, uint16_t value);
void psxHwWrite32(uint32_t add, uint32_t value);
int psxHwFreeze(void* f, enum FreezeMode mode);

#endif /* __PSXHW_H__ */
