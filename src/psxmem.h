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
#define SWAPuint32_t(v) SWAP32((uint32_t)(v))
#define SWAPint32_t(v) SWAP32((int32_t)(v))

#define SWAPuint16_t(v) SWAP16((uint16_t)(v))
#define SWAPint16_t(v) SWAP16((int16_t)(v))

#else

#define SWAP16(b) (b)
#define SWAP32(b) (b)

#define SWAPuint16_t(b) (b)
#define SWAPuint32_t(b) (b)

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

#define psxMint8_t(mem)		psxM[(mem) & 0x1fffff]
#define psxMint16_t(mem)	(SWAP16(*(int16_t*)&psxM[(mem) & 0x1fffff]))
#define psxMint32_t(mem)	(SWAP32(*(int32_t*)&psxM[(mem) & 0x1fffff]))
#define psxMuint8_t(mem)		(*(uint8_t*)&psxM[(mem) & 0x1fffff])
#define psxMuint16_t(mem)	(SWAP16(*(uint16_t*)&psxM[(mem) & 0x1fffff]))
#define psxMuint32_t(mem)	(SWAP32(*(uint32_t*)&psxM[(mem) & 0x1fffff]))

#define psxMint8_tref(mem)	psxM[(mem) & 0x1fffff]
#define psxMint16_tref(mem)	(*(int16_t*)&psxM[(mem) & 0x1fffff])
#define psxMint32_tref(mem)	(*(int32_t*)&psxM[(mem) & 0x1fffff])
#define psxMuint8_tref(mem)	(*(uint8_t*) &psxM[(mem) & 0x1fffff])
#define psxMuint16_tref(mem)	(*(uint16_t*)&psxM[(mem) & 0x1fffff])
#define psxMuint32_tref(mem)	(*(uint32_t*)&psxM[(mem) & 0x1fffff])

#define psxPint8_t(mem)	    psxP[(mem) & 0xffff]
#define psxPint16_t(mem)	(SWAP16(*(int16_t*)&psxP[(mem) & 0xffff]))
#define psxPint32_t(mem)	(SWAP32(*(int32_t*)&psxP[(mem) & 0xffff]))
#define psxPuint8_t(mem)		(*(uint8_t*) &psxP[(mem) & 0xffff])
#define psxPuint16_t(mem)	(SWAP16(*(uint16_t*)&psxP[(mem) & 0xffff]))
#define psxPuint32_t(mem)	(SWAP32(*(uint32_t*)&psxP[(mem) & 0xffff]))

#define psxPint8_tref(mem)	psxP[(mem) & 0xffff]
#define psxPint16_tref(mem)	(*(int16_t*)&psxP[(mem) & 0xffff])
#define psxPint32_tref(mem)	(*(int32_t*)&psxP[(mem) & 0xffff])
#define psxPuint8_tref(mem)	(*(uint8_t*) &psxP[(mem) & 0xffff])
#define psxPuint16_tref(mem)	(*(uint16_t*)&psxP[(mem) & 0xffff])
#define psxPuint32_tref(mem)	(*(uint32_t*)&psxP[(mem) & 0xffff])

#define psxRint8_t(mem)		psxR[(mem) & 0x7ffff]
#define psxRint16_t(mem)	(SWAP16(*(int16_t*)&psxR[(mem) & 0x7ffff]))
#define psxRint32_t(mem)	(SWAP32(*(int32_t*)&psxR[(mem) & 0x7ffff]))
#define psxRuint8_t(mem)		(*(uint8_t* )&psxR[(mem) & 0x7ffff])
#define psxRuint16_t(mem)	(SWAP16(*(uint16_t*)&psxR[(mem) & 0x7ffff]))
#define psxRuint32_t(mem)	(SWAP32(*(uint32_t*)&psxR[(mem) & 0x7ffff]))

#define psxRint8_tref(mem)	psxR[(mem) & 0x7ffff]
#define psxRint16_tref(mem)	(*(int16_t*)&psxR[(mem) & 0x7ffff])
#define psxRint32_tref(mem)	(*(int32_t*)&psxR[(mem) & 0x7ffff])
#define psxRuint8_tref(mem)	(*(uint8_t* )&psxR[(mem) & 0x7ffff])
#define psxRuint16_tref(mem)	(*(uint16_t*)&psxR[(mem) & 0x7ffff])
#define psxRuint32_tref(mem)	(*(uint32_t*)&psxR[(mem) & 0x7ffff])

#define psxHint8_t(mem)		psxH[(mem) & 0xffff]
#define psxHint16_t(mem)	(SWAP16(*(int16_t*)&psxH[(mem) & 0xffff]))
#define psxHint32_t(mem)	(SWAP32(*(int32_t*)&psxH[(mem) & 0xffff]))
#define psxHuint8_t(mem)		(*(uint8_t*) &psxH[(mem) & 0xffff])
#define psxHuint16_t(mem)	(SWAP16(*(uint16_t*)&psxH[(mem) & 0xffff]))
#define psxHuint32_t(mem)	(SWAP32(*(uint32_t*)&psxH[(mem) & 0xffff]))

#define psxHint8_tref(mem)	psxH[(mem) & 0xffff]
#define psxHint16_tref(mem)	(*(int16_t*)&psxH[(mem) & 0xffff])
#define psxHint32_tref(mem)	(*(int32_t*)&psxH[(mem) & 0xffff])
#define psxHuint8_tref(mem)	(*(uint8_t*) &psxH[(mem) & 0xffff])
#define psxHuint16_tref(mem)	(*(uint16_t*)&psxH[(mem) & 0xffff])
#define psxHuint32_tref(mem)	(*(uint32_t*)&psxH[(mem) & 0xffff])

#define PSXM(mem)		(uint8_t*)(psxMemRLUT[(mem) >> 16] + ((mem) & 0xffff))
#define PSXMint8_t(mem)		(*(int8_t *)PSXM(mem))
#define PSXMint16_t(mem)	(SWAP16(*(int16_t*)PSXM(mem)))
#define PSXMint32_t(mem)	(SWAP32(*(int32_t*)PSXM(mem)))
#define PSXMuint8_t(mem)		(*(uint8_t *)PSXM(mem))
#define PSXMuint16_t(mem)	(SWAP16(*(uint16_t*)PSXM(mem)))
#define PSXMuint32_t(mem)	(SWAP32(*(uint32_t*)PSXM(mem)))

#define PSXMuint32_tref(mem)	(*(uint32_t*)PSXM(mem))

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
