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

/*
* Handles PSX DMA functions.
*/

#include "psxdma.h"
#include "gpu.h"

// Dma0/1 in Mdec.c
// Dma3   in CdRom.c

//senquack - Updated to new PCSX Reloaded/Rearmed code:
void spuInterrupt() {
	if (HW_DMA4_CHCR & SWAP32(0x01000000))
	{
		HW_DMA4_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(4);
	}
}

//senquack - Updated to new PCSX Reloaded/Rearmed code:
void psxDma4(uint32_t madr, uint32_t bcr, uint32_t chcr) { // SPU
	uint16_t *ptr;
	uint32_t words;

	switch (chcr) {
		case 0x01000201: //cpu to spu transfer
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA4 SPU - mem2spu *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif
			ptr = (uint16_t *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA4 SPU - mem2spu *** NULL Pointer!!!\n");
#endif
				break;
			}
			words = (bcr >> 16) * (bcr & 0xffff);

			SPU_writeDMAMem(ptr, words * 2, psxRegs.cycle);

			HW_DMA4_MADR = SWAPu32(madr + words * 4);
			SPUDMA_INT(words / 2);
			return;

		case 0x01000200: //spu to cpu transfer
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA4 SPU - spu2mem *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif
			ptr = (uint16_t *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA4 SPU - spu2mem *** NULL Pointer!!!\n");
#endif
				break;
			}
			words = (bcr >> 16) * (bcr & 0xffff);

			SPU_readDMAMem(ptr, words * 2, psxRegs.cycle);

#ifdef PSXREC
			psxCpu->Clear(madr, words);
#endif

			HW_DMA4_MADR = SWAPu32(madr + words * 4);
			SPUDMA_INT(words / 2);
			return;

#ifdef PSXDMA_LOG
		default:
			PSXDMA_LOG("*** DMA4 SPU - unknown *** %x addr = %x size = %x\n", chcr, madr, bcr);
			break;
#endif
	}

	HW_DMA4_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(4);
}

// Taken from PEOPS SOFTGPU
static inline uint_fast8_t CheckForEndlessLoop(uint32_t laddr, uint32_t *lUsedAddr) {
	if (laddr == lUsedAddr[1]) return TRUE;
	if (laddr == lUsedAddr[2]) return TRUE;

	if (laddr < lUsedAddr[0]) lUsedAddr[1] = laddr;
	else lUsedAddr[2] = laddr;

	lUsedAddr[0] = laddr;

	return FALSE;
}

static uint32_t gpuDmaChainSize(uint32_t addr) {
	uint32_t size;
	uint32_t DMACommandCounter = 0;
	uint32_t lUsedAddr[3];

	lUsedAddr[0] = lUsedAddr[1] = lUsedAddr[2] = 0xffffff;

	// initial linked list ptr (word)
	size = 1;

	do {
		addr &= 0x1ffffc;

		if (DMACommandCounter++ > 2000000) break;
		if (CheckForEndlessLoop(addr, lUsedAddr)) break;

		// # 32-bit blocks to transfer
		size += psxMuint8_t( addr + 3 );

		// next 32-bit pointer
		addr = psxMuint32_t( addr & ~0x3 ) & 0xffffff;
		size += 1;
	} while (addr != 0xffffff);

	return size;
}

//senquack - updated to match PCSX Rearmed:
void psxDma2(uint32_t madr, uint32_t bcr, uint32_t chcr) { // GPU
	uint32_t *ptr;
	uint32_t words;
	uint32_t size;

	switch(chcr) {
		case 0x01000200: // vram2mem
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA2 GPU - vram2mem *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif
			ptr = (uint32_t *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - vram2mem *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			words = (bcr >> 16) * (bcr & 0xffff);
			GPU_readDataMem(ptr, words);
			#ifdef PSXREC
			psxCpu->Clear(madr, words);
			#endif

			HW_DMA2_MADR = SWAPu32(madr + words * 4);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(words / 4);
			return;

		case 0x01000201: // mem2vram
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU mem2vram *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif
			ptr = (uint32_t *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - mem2vram *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			words = (bcr >> 16) * (bcr & 0xffff);
			GPU_writeDataMem(ptr, words);

			HW_DMA2_MADR = SWAPu32(madr + words * 4);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(words / 4);
			return;

		case 0x01000401: // dma chain
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU dma chain *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif
			size = GPU_dmaChain((uint32_t *)psxM, madr & 0x1fffff);
			if ((int)size <= 0)
				size = gpuDmaChainSize(madr);
			HW_GPU_STATUS &= ~PSXGPU_nBUSY;

			// we don't emulate progress, just busy flag and end irq,
			// so pretend we're already at the last block
			HW_DMA2_MADR = SWAPu32(0xffffff);

			// Tekken 3 = use 1.0 only (not 1.5x)

			// Einhander = parse linked list in pieces (todo)
			// Final Fantasy 4 = internal vram time (todo)
			// Rebel Assault 2 = parse linked list in pieces (todo)
			// Vampire Hunter D = allow edits to linked list (todo)
			GPUDMA_INT(size);
			return;

#ifdef PSXDMA_LOG
		default:
			PSXDMA_LOG("*** DMA 2 - GPU unknown *** %x addr = %x size = %x\n", chcr, madr, bcr);
			break;
#endif
	}

	HW_DMA2_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(2);
}

//senquack - updated to match PCSX Rearmed:
void gpuInterrupt() {
	if (HW_DMA2_CHCR & SWAP32(0x01000000))
	{
		HW_DMA2_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(2);
	}
	HW_GPU_STATUS |= PSXGPU_nBUSY; // GPU no longer busy
}

void psxDma6(uint32_t madr, uint32_t bcr, uint32_t chcr) {
	uint32_t words;
	uint32_t *mem = (uint32_t *)PSXM(madr);

#ifdef PSXDMA_LOG
	PSXDMA_LOG("*** DMA6 OT *** %x addr = %x size = %x\n", chcr, madr, bcr);
#endif

	if (chcr == 0x11000002) {
		if (mem == NULL) {
#ifdef CPU_LOG
			CPU_LOG("*** DMA6 OT *** NULL Pointer!!!\n");
#endif
			HW_DMA6_CHCR &= SWAP32(~0x01000000);
			DMA_INTERRUPT(6);
			return;
		}

		// already 32-bit size
		words = bcr;

		while (bcr--) {
			*mem-- = SWAP32((madr - 4) & 0xffffff);
			madr -= 4;
		}
		mem++; *mem = 0xffffff;

		//GPUOTCDMA_INT(size);
		// halted
		psxRegs.cycle += words;
		GPUOTCDMA_INT(16);
		return;
	}
#ifdef PSXDMA_LOG
	else {
		// Unknown option
		PSXDMA_LOG("*** DMA6 OT - unknown *** %x addr = %x size = %x\n", chcr, madr, bcr);
	}
#endif

	HW_DMA6_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(6);
}

//senquack - New from PCSX Rearmed:
void gpuotcInterrupt()
{
	if (HW_DMA6_CHCR & SWAP32(0x01000000))
	{
		HW_DMA6_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(6);
	}
}
