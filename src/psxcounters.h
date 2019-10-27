/***************************************************************************
 *   Copyright (C) 2010 by Blade_Arma                                      *
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

#ifndef __PSXCOUNTERS_H__
#define __PSXCOUNTERS_H__

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"
#include "plugins.h"

extern uint32_t hSyncCount, frame_counter;

extern const uint32_t FrameRate[2];
extern const uint32_t HSyncTotal[2];

typedef struct Rcnt
{
    uint16_t mode, target;
    uint32_t rate, irq, counterState, irqState;
    uint32_t cycle, cycleStart;
} Rcnt;

void psxRcntInit(void);
void psxRcntUpdate(void);

void psxRcntWcount(uint32_t index, uint32_t value);
void psxRcntWmode(uint32_t index, uint32_t value);
void psxRcntWtarget(uint32_t index, uint32_t value);

uint32_t psxRcntRcount(uint32_t index);
uint32_t psxRcntRmode(uint32_t index);
uint32_t psxRcntRtarget(uint32_t index);

int psxRcntFreeze(void* f, enum FreezeMode mode);
void psxRcntInitFromFreeze(void);

void psxRcntAdjustTimestamps(const uint32_t prev_cycle_val);

#endif /* __PSXCOUNTERS_H__ */
