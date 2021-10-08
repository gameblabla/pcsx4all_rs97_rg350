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

#ifndef __GTE_H__
#define __GTE_H__

#include "psxcommon.h"
#include "r3000a.h"

extern uint32_t GteStall;

void gteMFC2(void);
void gteCFC2(void);
void gteMTC2(void);
void gteCTC2(void);
void gteLWC2(void);
void gteSWC2(void);

void gteRTPS(void);
void gteOP(uint32_t gteop);
void gteNCLIP(void);
void gteDPCS(uint32_t gteop);
void gteINTPL(uint32_t gteop);
void gteMVMVA(uint32_t gteop);
void gteNCDS(void);
void gteNCDT(void);
void gteCDP(void);
void gteNCCS(void);
void gteCC(void);
void gteNCS(void);
void gteNCT(void);
void gteSQR(uint32_t gteop);
void gteDCPL(uint32_t gteop);
void gteDPCT(void);
void gteAVSZ3(void);
void gteAVSZ4(void);
void gteRTPT(void);
void gteGPF(uint32_t gteop);
void gteGPL(uint32_t gteop);
void gteNCCT(void);

// for the recompiler
uint32_t gtecalcMFC2(int reg);
void gtecalcMTC2(uint32_t value, int reg);
void gtecalcCTC2(uint32_t value, int reg);

#endif /* __GTE_H__ */
