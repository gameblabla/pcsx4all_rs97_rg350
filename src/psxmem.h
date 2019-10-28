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

#ifndef __PSXMEMORY_H__
#define __PSXMEMORY_H__

#include "psxcommon.h"

#if defined(__BIGENDIAN__)

#define _SWAP16(b) ((((unsigned char*)&(b))[0]&0xff) | (((unsigned char*)&(b))[1]&0xff)<<8)
#define _SWAP32(b) ((((unsigned char*)&(b))[0]&0xff) | ((((unsigned char*)&(b))[1]&0xff)<<8) | ((((unsigned char*)&(b))[2]&0xff)<<16) | (((unsigned char*)&(b))[3]<<24))

#define SWAP16(v) ((((v)&0xff00)>>8) +(((v)&0xff)<<8))
#define SWAP32(v) ((((v)&0xff000000ul)>>24) + (((v)&0xff0000ul)>>8) + (((v)&0xff00ul)<<8) +(((v)&0xfful)<<24))
#define SWAPu32(v) SWAP32((uint32_t)(v))
#define SWAPs32(v) SWAP32((int32_t)(v))

#define SWAPu16(v) SWAP16((uint16_t)(v))
#define SWAPs16(v) SWAP16((int16_t)(v))

#else

#define SWAP16(b) (b)
#define SWAP32(b) (b)

#define SWAPu16(b) (b)
#define SWAPu32(b) (b)

#endif

/* Dynarecs could decide to mmap one of these to address 0, meaning that a
   check of pointer NULLness shouldn't be used. Always use the uint_fast8_ts instead
   to check allocation status. */
extern int8_t *psxM;
extern int8_t *psxP;
extern int8_t *psxR;
extern int8_t *psxH;
extern uint_fast8_t psxM_allocated;
extern uint_fast8_t psxP_allocated;
extern uint_fast8_t psxR_allocated;
extern uint_fast8_t psxH_allocated;

extern uint8_t **psxMemWLUT;
extern uint8_t **psxMemRLUT;

#define psxMs8(mem)		psxM[(mem) & 0x1fffff]
#define psxMs16(mem)	(SWAP16(*(int16_t*)&psxM[(mem) & 0x1fffff]))
#define psxMs32(mem)	(SWAP32(*(int32_t*)&psxM[(mem) & 0x1fffff]))
#define psxMu8(mem)		(*(uint8_t*)&psxM[(mem) & 0x1fffff])
#define psxMu16(mem)	(SWAP16(*(uint16_t*)&psxM[(mem) & 0x1fffff]))
#define psxMu32(mem)	(SWAP32(*(uint32_t*)&psxM[(mem) & 0x1fffff]))

#define psxMs8ref(mem)	psxM[(mem) & 0x1fffff]
#define psxMs16ref(mem)	(*(int16_t*)&psxM[(mem) & 0x1fffff])
#define psxMs32ref(mem)	(*(int32_t*)&psxM[(mem) & 0x1fffff])
#define psxMu8ref(mem)	(*(uint8_t*) &psxM[(mem) & 0x1fffff])
#define psxMu16ref(mem)	(*(uint16_t*)&psxM[(mem) & 0x1fffff])
#define psxMu32ref(mem)	(*(uint32_t*)&psxM[(mem) & 0x1fffff])

#define psxPs8(mem)	    psxP[(mem) & 0xffff]
#define psxPs16(mem)	(SWAP16(*(int16_t*)&psxP[(mem) & 0xffff]))
#define psxPs32(mem)	(SWAP32(*(int32_t*)&psxP[(mem) & 0xffff]))
#define psxPu8(mem)		(*(uint8_t*) &psxP[(mem) & 0xffff])
#define psxPu16(mem)	(SWAP16(*(uint16_t*)&psxP[(mem) & 0xffff]))
#define psxPu32(mem)	(SWAP32(*(uint32_t*)&psxP[(mem) & 0xffff]))

#define psxPs8ref(mem)	psxP[(mem) & 0xffff]
#define psxPs16ref(mem)	(*(int16_t*)&psxP[(mem) & 0xffff])
#define psxPs32ref(mem)	(*(int32_t*)&psxP[(mem) & 0xffff])
#define psxPu8ref(mem)	(*(uint8_t*) &psxP[(mem) & 0xffff])
#define psxPu16ref(mem)	(*(uint16_t*)&psxP[(mem) & 0xffff])
#define psxPu32ref(mem)	(*(uint32_t*)&psxP[(mem) & 0xffff])

#define psxRs8(mem)		psxR[(mem) & 0x7ffff]
#define psxRs16(mem)	(SWAP16(*(int16_t*)&psxR[(mem) & 0x7ffff]))
#define psxRs32(mem)	(SWAP32(*(int32_t*)&psxR[(mem) & 0x7ffff]))
#define psxRu8(mem)		(*(uint8_t* )&psxR[(mem) & 0x7ffff])
#define psxRu16(mem)	(SWAP16(*(uint16_t*)&psxR[(mem) & 0x7ffff]))
#define psxRu32(mem)	(SWAP32(*(uint32_t*)&psxR[(mem) & 0x7ffff]))

#define psxRs8ref(mem)	psxR[(mem) & 0x7ffff]
#define psxRs16ref(mem)	(*(int16_t*)&psxR[(mem) & 0x7ffff])
#define psxRs32ref(mem)	(*(int32_t*)&psxR[(mem) & 0x7ffff])
#define psxRu8ref(mem)	(*(uint8_t* )&psxR[(mem) & 0x7ffff])
#define psxRu16ref(mem)	(*(uint16_t*)&psxR[(mem) & 0x7ffff])
#define psxRu32ref(mem)	(*(uint32_t*)&psxR[(mem) & 0x7ffff])

#define psxHs8(mem)		psxH[(mem) & 0xffff]
#define psxHs16(mem)	(SWAP16(*(int16_t*)&psxH[(mem) & 0xffff]))
#define psxHs32(mem)	(SWAP32(*(int32_t*)&psxH[(mem) & 0xffff]))
#define psxHu8(mem)		(*(uint8_t*) &psxH[(mem) & 0xffff])
#define psxHu16(mem)	(SWAP16(*(uint16_t*)&psxH[(mem) & 0xffff]))
#define psxHu32(mem)	(SWAP32(*(uint32_t*)&psxH[(mem) & 0xffff]))

#define psxHs8ref(mem)	psxH[(mem) & 0xffff]
#define psxHs16ref(mem)	(*(int16_t*)&psxH[(mem) & 0xffff])
#define psxHs32ref(mem)	(*(int32_t*)&psxH[(mem) & 0xffff])
#define psxHu8ref(mem)	(*(uint8_t*) &psxH[(mem) & 0xffff])
#define psxHu16ref(mem)	(*(uint16_t*)&psxH[(mem) & 0xffff])
#define psxHu32ref(mem)	(*(uint32_t*)&psxH[(mem) & 0xffff])

#define PSXM(mem)		(uint8_t*)(psxMemRLUT[(mem) >> 16] + ((mem) & 0xffff))
#define PSXMs8(mem)		(*(int8_t *)PSXM(mem))
#define PSXMs16(mem)	(SWAP16(*(int16_t*)PSXM(mem)))
#define PSXMs32(mem)	(SWAP32(*(int32_t*)PSXM(mem)))
#define PSXMu8(mem)		(*(uint8_t *)PSXM(mem))
#define PSXMu16(mem)	(SWAP16(*(uint16_t*)PSXM(mem)))
#define PSXMu32(mem)	(SWAP32(*(uint32_t*)PSXM(mem)))

#define PSXMu32ref(mem)	(*(uint32_t*)PSXM(mem))

int  psxMemInit(void);
void psxMemReset(void);
void psxMemShutdown(void);

uint8_t   psxMemRead8(uint32_t mem);
uint16_t  psxMemRead16(uint32_t mem);
uint32_t  psxMemRead32(uint32_t mem);
void psxMemWrite8(uint32_t mem, uint8_t value);
void psxMemWrite16(uint32_t mem, uint16_t value);
void psxMemWrite32(uint32_t mem, uint32_t value);

void psxMemWrite32_CacheCtrlPort(uint32_t value);

uint8_t   psxMemRead8_direct(uint32_t mem,void *regs);
uint16_t  psxMemRead16_direct(uint32_t mem,void *regs);
uint32_t  psxMemRead32_direct(uint32_t mem,void *regs);
void psxMemWrite8_direct(uint32_t mem, uint8_t value,void *regs);
void psxMemWrite16_direct(uint32_t mem, uint16_t value,void *regs);
void psxMemWrite32_direct(uint32_t mem, uint32_t value,void *regs);

#endif /* __PSXMEMORY_H__ */
