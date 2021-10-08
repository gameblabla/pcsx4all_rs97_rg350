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

#ifndef __R3000A_H__
#define __R3000A_H__

#include "psxcommon.h"
#include "psxmem.h"
#include "psxcounters.h"
#include "psxbios.h"

/* Possible vals for Notify() func param 'note' in R3000Acpu struct below */
enum {
	R3000ACPU_NOTIFY_CACHE_ISOLATED,
	R3000ACPU_NOTIFY_CACHE_UNISOLATED,
	R3000ACPU_NOTIFY_DMA3_EXE_LOAD
};

typedef struct {
	int  (*Init)(void);
	void (*Reset)(void);
	void (*Execute)(void);
	void (*ExecuteBlock)(unsigned target_pc);
	void (*Clear)(uint32_t Addr, uint32_t Size);
	void (*Notify)(int note, void *data);
	void (*Shutdown)(void);
} R3000Acpu;

extern R3000Acpu *psxCpu;
extern R3000Acpu psxInt;
#ifdef PSXREC
extern R3000Acpu psxRec;
#endif

typedef union {
#if defined(__BIGENDIAN__)
	struct { uint8_t h3, h2, h, l; } b;
	struct { int8_t h3, h2, h, l; } sb;
	struct { uint16_t h, l; } w;
	struct { int16_t h, l; } sw;
#else
	struct { uint8_t l, h, h2, h3; } b;
	struct { uint16_t l, h; } w;
	struct { int8_t l, h, h2, h3; } sb;
	struct { int16_t l, h; } sw;
#endif
} PAIR;

typedef union {
	struct {
		uint32_t	r0, at, v0, v1, a0, a1, a2, a3,
			t0, t1, t2, t3, t4, t5, t6, t7,
			s0, s1, s2, s3, s4, s5, s6, s7,
			t8, t9, k0, k1, gp, sp, s8, ra, lo, hi;
	} n;
	uint32_t r[34]; /* Lo, Hi in r[32] and r[33] */
	PAIR p[34];
} psxGPRRegs;

typedef union {
	struct {
		uint32_t	Index,     Random,    EntryLo0,  EntryLo1,
			Context,   PageMask,  Wired,     Reserved0,
			BadVAddr,  Count,     EntryHi,   Compare,
			Status,    Cause,     EPC,       PRid,
			Config,    LLAddr,    WatchLO,   WatchHI,
			XContext,  Reserved1, Reserved2, Reserved3,
			Reserved4, Reserved5, ECC,       CacheErr,
			TagLo,     TagHi,     ErrorEPC,  Reserved6;
	} n;
	uint32_t r[32];
	PAIR p[32];
} psxCP0Regs;

typedef struct {
	short x, y;
} SVector2D;

typedef struct {
	short z, pad;
} SVector2Dz;

typedef struct {
	short x, y, z, pad;
} SVector3D;

typedef struct {
	short x, y, z, pad;
} LVector3D;

typedef struct {
	unsigned char r, g, b, c;
} CBGR;

typedef struct {
	short m11, m12, m13, m21, m22, m23, m31, m32, m33, pad;
} SMatrix3D;

typedef union {
	struct {
		SVector3D     v0, v1, v2;
		CBGR          rgb;
		int32_t          otz;
		int32_t          ir0, ir1, ir2, ir3;
		SVector2D     sxy0, sxy1, sxy2, sxyp;
		SVector2Dz    sz0, sz1, sz2, sz3;
		CBGR          rgb0, rgb1, rgb2;
		int32_t          reserved;
		int32_t          mac0, mac1, mac2, mac3;
		uint32_t irgb, orgb;
		int32_t          lzcs, lzcr;
	} n;
	uint32_t r[32];
	PAIR p[32];
} psxCP2Data;

typedef union {
	struct {
		SMatrix3D rMatrix;
		int32_t      trX, trY, trZ;
		SMatrix3D lMatrix;
		int32_t      rbk, gbk, bbk;
		SMatrix3D cMatrix;
		int32_t      rfc, gfc, bfc;
		int32_t      ofx, ofy;
		int32_t      h;
		int32_t      dqa, dqb;
		int32_t      zsf3, zsf4;
		int32_t      flag;
	} n;
	uint32_t r[32];
	PAIR p[32];
} psxCP2Ctrl;

// Interrupt/event 'timestamp'
struct intCycle_t {
	uint32_t sCycle; // psxRegs.cycle value when event/interrupt was sheduled
	uint32_t cycle;  // Number of cycles past sCycle above when event should occur
};

typedef struct {
	psxGPRRegs GPR;		/* General Purpose Registers */
	psxCP0Regs CP0;		/* Coprocessor0 Registers */
	psxCP2Data CP2D; 	/* Cop2 data registers */
	psxCP2Ctrl CP2C; 	/* Cop2 control registers */
	uint32_t pc;			/* Program counter */
	uint32_t code;		/* The instruction */
	uint32_t cycle;
	uint32_t interrupt;

	struct intCycle_t intCycle[32];

	uint32_t io_cycle_counter;

	int8_t *psxM;
	int8_t *psxP;
	int8_t *psxR;
	int8_t *psxH;

	void *reserved;
	int writeok;
	
	int32_t GteUnitCycles;
} psxRegisters;

extern psxRegisters psxRegs;

#if defined(__BIGENDIAN__)

#define _i32(x) *(int32_t *)&x
#define _uint32_t(x) x

#define _i16(x) (((short *)&x)[1])
#define _uint16_t(x) (((unsigned short *)&x)[1])

#define _i8(x) (((char *)&x)[3])
#define _uint8_t(x) (((unsigned char *)&x)[3])

#else

#define _i32(x) *(int32_t *)&x
#define _uint32_t(x) x

#define _i16(x) *(short *)&x
#define _uint16_t(x) *(unsigned short *)&x

#define _i8(x) *(char *)&x
#define _uint8_t(x) *(unsigned char *)&x

#endif

/**** R3000A Instruction Macros ****/
#define _PC_       psxRegs.pc       // The next PC to be executed

#define _fOp_(code)		((code >> 26)       )  // The opcode part of the instruction register 
#define _fFunct_(code)	((code      ) & 0x3F)  // The funct part of the instruction register 
#define _fRd_(code)		((code >> 11) & 0x1F)  // The rd part of the instruction register 
#define _fRt_(code)		((code >> 16) & 0x1F)  // The rt part of the instruction register 
#define _fRs_(code)		((code >> 21) & 0x1F)  // The rs part of the instruction register 
#define _fSa_(code)		((code >>  6) & 0x1F)  // The sa part of the instruction register
#define _fIm_(code)		((uint16_t)code)            // The immediate part of the instruction register
#define _fTarget_(code)	(code & 0x03ffffff)    // The target part of the instruction register

#define _fImm_(code)	((int16_t)code)            // sign-extended immediate
#define _fImmU_(code)	(code&0xffff)          // zero-extended immediate

#define _Op_     _fOp_(psxRegs.code)
#define _Funct_  _fFunct_(psxRegs.code)
#define _Rd_     _fRd_(psxRegs.code)
#define _Rt_     _fRt_(psxRegs.code)
#define _Rs_     _fRs_(psxRegs.code)
#define _Sa_     _fSa_(psxRegs.code)
#define _Im_     _fIm_(psxRegs.code)
#define _Target_ _fTarget_(psxRegs.code)

#define _Imm_	 _fImm_(psxRegs.code)
#define _ImmU_	 _fImmU_(psxRegs.code)

#define _rRs_   psxRegs.GPR.r[_Rs_]   // Rs register
#define _rRt_   psxRegs.GPR.r[_Rt_]   // Rt register
#define _rRd_   psxRegs.GPR.r[_Rd_]   // Rd register
#define _rSa_   psxRegs.GPR.r[_Sa_]   // Sa register
#define _rFs_   psxRegs.CP0.r[_Rd_]   // Fs register

#define _c2dRs_ psxRegs.CP2D.r[_Rs_]  // Rs cop2 data register
#define _c2dRt_ psxRegs.CP2D.r[_Rt_]  // Rt cop2 data register
#define _c2dRd_ psxRegs.CP2D.r[_Rd_]  // Rd cop2 data register
#define _c2dSa_ psxRegs.CP2D.r[_Sa_]  // Sa cop2 data register

#define _rHi_   psxRegs.GPR.n.hi   // The HI register
#define _rLo_   psxRegs.GPR.n.lo   // The LO register

#define _JumpTarget_    ((_Target_ * 4) + (_PC_ & 0xf0000000))   // Calculates the target during a jump instruction
#define _BranchTarget_  ((int16_t)_Im_ * 4 + _PC_)                 // Calculates the target during a branch instruction

#define _SetLink(x)     psxRegs.GPR.r[x] = _PC_ + 4;       // Sets the return address in the link register

#define ResetIoCycle() do { psxRegs.io_cycle_counter = 0; } while (0)

int  psxInit(void);
void psxReset(void);
void psxShutdown(void);
void psxException(uint32_t code, uint32_t bd);
void psxBranchTest(void);
void psxExecuteBios(void);
int  psxTestLoadDelay(int reg, uint32_t tmp);
void psxDelayTest(int reg, uint32_t bpc);
void psxTestSWInts(void);

void GTE_AddCycles( int amount );
void GTE_UnitStall( uint32_t newStall );


#endif /* __R3000A_H__ */
