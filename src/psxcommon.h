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
* This file contains common definitions and includes for all parts of the 
* emulator core.
*/

#ifndef __PSXCOMMON_H__
#define __PSXCOMMON_H__

/* System includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <assert.h>

/* Port includes */
#include "port.h"

/* Define types */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.9"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

enum {
	PSXTYPE_NTSC = 0,
	PSXTYPE_PAL  = 1
};


// SPU update frequency  (accomodates slow devices)
// Values used for Config.SpuUpdateFreq
enum {
	SPU_UPDATE_FREQ_MIN = 0,
	SPU_UPDATE_FREQ_1   = 0,
	SPU_UPDATE_FREQ_2   = 1,
	SPU_UPDATE_FREQ_4   = 2,
	SPU_UPDATE_FREQ_8   = 3,
	SPU_UPDATE_FREQ_16  = 4,
	SPU_UPDATE_FREQ_32  = 5,
	SPU_UPDATE_FREQ_MAX = SPU_UPDATE_FREQ_32
};

#ifndef SPU_UPDATE_FREQ_DEFAULT
#define SPU_UPDATE_FREQ_DEFAULT SPU_UPDATE_FREQ_1
#endif

// Forced XA audio update frequency (accomodates slow devices)
// Values used for Config.ForcedXAUpdates
enum {
	FORCED_XA_UPDATES_MIN     = 0,
	FORCED_XA_UPDATES_OFF     = 0,
	FORCED_XA_UPDATES_AUTO    = 1,
	FORCED_XA_UPDATES_2       = 2,
	FORCED_XA_UPDATES_4       = 3,
	FORCED_XA_UPDATES_8       = 4,
	FORCED_XA_UPDATES_16      = 5,
	FORCED_XA_UPDATES_32      = 6,
	FORCED_XA_UPDATES_MAX     = FORCED_XA_UPDATES_32
};

#ifndef FORCED_XA_UPDATES_DEFAULT
#define FORCED_XA_UPDATES_DEFAULT FORCED_XA_UPDATES_OFF
#endif

enum {
	FRAMESKIP_MIN  = -1,
	FRAMESKIP_AUTO = -1,
	FRAMESKIP_OFF  = 0,
	FRAMESKIP_MAX  = 3
};

typedef struct {
	char Bios[MAXPATHLEN];
	char BiosDir[MAXPATHLEN];
	char LastDir[MAXPATHLEN];
	char PatchesDir[MAXPATHLEN];  // PPF patch files
	uint_fast8_t Xa; /* 0=XA enabled, 1=XA disabled */
	uint_fast8_t Mdec; /* 0=Black&White Mdecs Only Disabled, 1=Black&White Mdecs Only Enabled */
	uint_fast8_t PsxAuto; /* 1=autodetect system (pal or ntsc) */
	uint_fast8_t Cdda; /* 0=Enable Cd audio, 1=Disable Cd audio */
	uint_fast8_t HLE; /* 1=HLE, 0=bios */
	uint_fast8_t SlowBoot; /* 0=skip bios logo sequence on boot  1=show sequence (does not apply to HLE) */
	uint_fast8_t AnalogArrow; /* 0=disable 1=use L-stick as D-pad arrow key */
	uint8_t AnalogMode;   /* 0-Digital 1-DualAnalog 2-DualShock */
	uint_fast8_t RCntFix; /* 1=Parasite Eve 2, Vandal Hearts 1/2 Fix */
	uint_fast8_t VSyncWA; /* 1=InuYasha Sengoku Battle Fix */
	uint8_t Cpu; /* 0=recompiler, 1=interpreter */
	uint8_t PsxType; /* 0=ntsc, 1=pal */
    uint8_t McdSlot1; /* mcd slot 1, mcd%03u.mcr */
    uint8_t McdSlot2; /* mcd slot 2, mcd%03u.mcr */

	//senquack - added Config.SpuIrq option from PCSX Rearmed/Reloaded:
	uint_fast8_t SpuIrq; /* 1=SPU IRQ always enabled (needed for audio in some games) */

	//senquack - Added audio syncronization option; if audio buffer is full,
	//           main thread blocks
	uint_fast8_t SyncAudio;

	int8_t      SpuUpdateFreq; // Frequency of SPU updates
	                       // 0: once per frame  1: twice per frame etc
	                       // (Use SPU_UPDATE_FREQ_* enum to set)

	//senquack - Added option to allow queuing CDREAD_INT interrupts sooner
	//           than they'd normally be issued when SPU's XA buffer is not
	//           full. This fixes droupouts in music/speech on slow devices.
	int8_t      ForcedXAUpdates;

	uint_fast8_t ShowFps;     // Show FPS
	uint_fast8_t FrameLimit;  // Limit to NTSC/PAL framerate

	int8_t      FrameSkip;	// -1: AUTO  0: OFF  1-3: FIXED
	int8_t      VideoScaling; // 0: Hardware  1: Software Nearest

	// Options for performance monitor
	uint_fast8_t PerfmonConsoleOutput;
	uint_fast8_t PerfmonDetailedStats;
	
	/* CodeName Takeda requires the player to remove one of the memory cards.
	 * However, the use of such hack for going around this can actually break the behaviour of some games
	 * like Digimon World. (which thinks a save is available, when it's in fact not)
	 * But since this game does need it, let's add a setting for it.
	 * In the future, we could simply allow the user to remove it themselves but an auto-hack
	 * more preferable in this case. (Because it would be less conveniant)
	*/
	uint8_t      MemoryCardHack; 
	
	uint_fast8_t AnalogDigital; /* 0=disable 1=use Map sticks to DPAD/Buttons */

} PcsxConfig;

extern PcsxConfig Config;

/////////////////////////////
// Savestate file handling //
/////////////////////////////
struct PcsxSaveFuncs {
	void *(*open)(const char *name, uint_fast8_t writing);
	int   (*read)(void *file, void *buf, uint32_t len);
	int   (*write)(void *file, const void *buf, uint32_t len);
	long  (*seek)(void *file, long offs, int whence);
	int   (*close)(void *file);

#if !(defined(_WIN32) && !defined(__CYGWIN__))
	int   fd;         // The fd we receive from OS's open()
	int   lib_fd;     // The dupe'd fd we tell compression lib to use
#endif
};

// Defined in misc.cpp:
#ifdef _cplusplus
extern "C" {
#endif
enum FreezeMode {
	FREEZE_LOAD = 0,
	FREEZE_SAVE = 1,
	FREEZE_INFO = 2    // Query plugin for amount of ram to allocate for freeze
};
int freeze_rw(void *file, enum FreezeMode mode, void *buf, unsigned len);
#ifdef _cplusplus
}
#endif

extern struct PcsxSaveFuncs SaveFuncs;

#define BIAS	2
#define PSXCLK	33868800	/* 33.8688 Mhz */

enum {
	PSX_TYPE_NTSC = 0,
	PSX_TYPE_PAL
}; // PSX Types

enum {
	CPU_DYNAREC = 0,
	CPU_INTERPRETER
}; // CPU Types

void EmuUpdate();

#endif /* __PSXCOMMON_H__ */
