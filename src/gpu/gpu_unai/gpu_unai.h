/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*   Copyright (C) 2016 Senquack (dansilsby <AT> gmail <DOT> com)          *
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
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

#ifndef GPU_UNAI_H
#define GPU_UNAI_H

#include <psxcommon.h>
#include "gpu.h"

// Header shared between both standalone gpu_unai (gpu.cpp) and new
// gpulib-compatible gpu_unai (gpulib_if.cpp)
// -> Anything here should be for gpu_unai's private use. <-

///////////////////////////////////////////////////////////////////////////////
//  Compile Options

//#define ENABLE_GPU_NULL_SUPPORT   // Enables NullGPU support
//#define ENABLE_GPU_LOG_SUPPORT    // Enables gpu logger, very slow only for windows debugging
//#define ENABLE_GPU_ARMV7			// Enables ARMv7 optimized assembly

//Poly routine options (default is integer math and accurate division)
//#define GPU_UNAI_USE_FLOATMATH         // Use float math in poly routines
//#define GPU_UNAI_USE_FLOAT_DIV_MULTINV // If GPU_UNAI_USE_FLOATMATH is defined,
                                         //  use multiply-by-inverse for division
//#define GPU_UNAI_USE_INT_DIV_MULTINV   // If GPU_UNAI_USE_FLOATMATH is *not*
                                         //  defined, use old inaccurate division


#define uint8_t  uint8_t
#define int8_t  int8_t
#define uint16_t uint16_t
#define int16_t int16_t
#define uint32_t uint32_t
#define int32_t int32_t
#define int64_t int64_t

union PtrUnion
{
	uint32_t  *U4;
	int32_t  *S4;
	uint16_t  *U2;
	int16_t  *S2;
	uint8_t   *U1;
	int8_t   *S1;
	void *ptr;
};

union GPUPacket
{
	uint32_t U4[16];
	int32_t S4[16];
	uint16_t U2[32];
	int16_t S2[32];
	uint8_t  U1[64];
	int8_t  S1[64];
};

template<class T> static inline void SwapValues(T &x, T &y)
{
	T tmp(x);  x = y;  y = tmp;
}

template<typename T>
static inline T Min2 (const T a, const T b)
{
	return (a<b)?a:b;
}

template<typename T>
static inline T Min3 (const T a, const T b, const T c)
{
	return  Min2(Min2(a,b),c);
}

template<typename T>
static inline T Max2 (const T a, const T b)
{
	return  (a>b)?a:b;
}

template<typename T>
static inline T Max3 (const T a, const T b, const T c)
{
	return  Max2(Max2(a,b),c);
}


///////////////////////////////////////////////////////////////////////////////
//  GPU Raster Macros

// Convert 24bpp color parameter of GPU command to 16bpp (15bpp + mask bit)
#define	GPU_RGB16(rgb) ((((rgb)&0xF80000)>>9)|(((rgb)&0xF800)>>6)|(((rgb)&0xF8)>>3))

// Sign-extend 11-bit coordinate command param
#define GPU_EXPANDSIGN(x) (((int32_t)(x)<<(32-11))>>(32-11))

// Max difference between any two X or Y primitive coordinates
#define CHKMAX_X 1024
#define CHKMAX_Y 512

#define	FRAME_BUFFER_SIZE	(1024*512*2)
#define	FRAME_WIDTH			  1024
#define	FRAME_HEIGHT		  512
#define	FRAME_OFFSET(x,y)	(((y)<<10)+(x))
#define FRAME_BYTE_STRIDE     2048
#define FRAME_BYTES_PER_PIXEL 2

static inline int32_t GPU_DIV(int32_t rs, int32_t rt)
{
	return rt ? (rs / rt) : (0);
}

// 'Unsafe' version of above that doesn't check for div-by-zero
#define GPU_FAST_DIV(rs, rt) ((signed)(rs) / (signed)(rt))

struct gpu_unai_t {
	uint32_t GPU_GP1;
	GPUPacket PacketBuffer;
	uint16_t *vram;

	////////////////////////////////////////////////////////////////////////////
	// Variables used only by older standalone version of gpu_unai (gpu.cpp)
#ifndef USE_GPULIB
	uint32_t  GPU_GP0;
	uint32_t  tex_window;       // Current texture window vals (set by GP0(E2h) cmd)
	int32_t  PacketCount;
	int32_t  PacketIndex;
	uint_fast8_t fb_dirty;         // Framebuffer is dirty (according to GPU)

	//  Display status
	//  NOTE: Standalone older gpu_unai didn't care about horiz display range
	uint16_t  DisplayArea[6];   // [0] : Start of display area (in VRAM) X
	                       // [1] : Start of display area (in VRAM) Y
	                       // [2] : Display mode resolution HORIZONTAL
	                       // [3] : Display mode resolution VERTICAL
	                       // [4] : Vertical display range (on TV) START
	                       // [5] : Vertical display range (on TV) END

	////////////////////////////////////////////////////////////////////////////
	//  Dma Transfers info
	struct {
		int32_t  px,py;
		int32_t  x_end,y_end;
		uint16_t* pvram;
		uint32_t *last_dma;     // Last dma pointer
		uint_fast8_t FrameToRead;  // Load image in progress
		uint_fast8_t FrameToWrite; // Store image in progress
	} dma;

	////////////////////////////////////////////////////////////////////////////
	//  Frameskip
	struct {
		int  skipCount;    // Frame skip (0,1,2,3...)
		uint_fast8_t isSkip;       // Skip frame (according to GPU)
		uint_fast8_t skipFrame;    // Skip this frame (according to frame skip)
		uint_fast8_t wasSkip;      // Skip frame old value (according to GPU)
		uint_fast8_t skipGPU;      // Skip GPU primitives
	} frameskip;
#endif
	// END of standalone gpu_unai variables
	////////////////////////////////////////////////////////////////////////////

	uint32_t TextureWindowCur;  // Current setting from last GP0(0xE2) cmd (raw form)
	uint8_t  TextureWindow[4];  // [0] : Texture window offset X
	                       // [1] : Texture window offset Y
	                       // [2] : Texture window mask X
	                       // [3] : Texture window mask Y

	uint16_t DrawingArea[4];    // [0] : Drawing area top left X
	                       // [1] : Drawing area top left Y
	                       // [2] : Drawing area bottom right X
	                       // [3] : Drawing area bottom right Y

	int16_t DrawingOffset[2];  // [0] : Drawing offset X (signed)
	                       // [1] : Drawing offset Y (signed)

	uint16_t* TBA;              // Ptr to current texture in VRAM
	uint16_t* CBA;              // Ptr to current CLUT in VRAM

	////////////////////////////////////////////////////////////////////////////
	//  Inner Loop parameters

	// 22.10 Fixed-pt texture coords, mask, scanline advance
	// NOTE: U,V are no longer packed together into one uint32_t, this proved to be
	//  too imprecise, leading to pixel dropouts.  Example: NFS3's skybox.
	uint32_t u, v;
	uint32_t u_msk, v_msk;
	int32_t u_inc, v_inc;

	// Color for Gouraud-shaded prims
	// Packed fixed-pt 8.3:8.3:8.2 rgb triplet
	//  layout:  rrrrrrrrXXXggggggggXXXbbbbbbbbXX
	//           ^ bit 31                       ^ bit 0
	uint32_t gCol;
	uint32_t gInc;          // Increment along scanline for gCol

	// Color for flat-shaded, texture-blended prims
	uint8_t  r5, g5, b5;    // 5-bit light for undithered prims
	uint8_t  r8, g8, b8;    // 8-bit light for dithered prims

	// Color for flat-shaded, untextured prims
	uint16_t PixelData;      // bgr555 color for untextured flat-shaded polys

	// End of inner Loop parameters
	////////////////////////////////////////////////////////////////////////////


	uint8_t blit_mask;           // Determines what pixels to skip when rendering.
	                        //  Only useful on low-resolution devices using
	                        //  a simple pixel-dropping downscaler for PS1
	                        //  high-res modes. See 'pixel_skip' option.

	uint8_t ilace_mask;          // Determines what lines to skip when rendering.
	                        //  Normally 0 when PS1 240 vertical res is in
	                        //  use and ilace_force is 0. When running in
	                        //  PS1 480 vertical res on a low-resolution
	                        //  device (320x240), will usually be set to 1
	                        //  so odd lines are not rendered. (Unless future
	                        //  full-screen scaling option is in use ..TODO)

	uint_fast8_t prog_ilace_flag;   // Tracks successive frames for 'prog_ilace' option

	uint8_t BLEND_MODE;
	uint8_t TEXT_MODE;
	uint8_t Masking;

	uint16_t PixelMSB;

	gpu_unai_config_t config;

	uint8_t  LightLUT[32*32];    // 5-bit lighting LUT (gpu_inner_light.h)
	uint32_t DitherMatrix[64];   // Matrix of dither coefficients
};

static gpu_unai_t gpu_unai;

// Global config that frontend can alter.. Values are read in GPU_init().
// TODO: if frontend menu modifies a setting, add a function that can notify
// GPU plugin to use new setting.
gpu_unai_config_t gpu_unai_config_ext;

///////////////////////////////////////////////////////////////////////////////
// Internal inline funcs to get option status: (Allows flexibility)
static inline uint_fast8_t LightingEnabled()
{
	return gpu_unai.config.lighting;
}

static inline uint_fast8_t FastLightingEnabled()
{
	return gpu_unai.config.fast_lighting;
}

static inline uint_fast8_t BlendingEnabled()
{
	return gpu_unai.config.blending;
}

static inline uint_fast8_t DitheringEnabled()
{
	return gpu_unai.config.dithering;
}

// For now, this is just for development/experimentation purposes..
// If modified to return true, it will allow ignoring the status register
//  bit 9 setting (dither enable). It will still restrict dithering only
//  to Gouraud-shaded or texture-blended polys.
static inline uint_fast8_t ForcedDitheringEnabled()
{
	return false;
}

static inline uint_fast8_t ProgressiveInterlaceEnabled()
{
#ifdef USE_GPULIB
	// Using this old option greatly decreases quality of image. Disabled
	//  for now when using new gpulib, since it also adds more work in loops.
	return false;
#else
	return gpu_unai.config.prog_ilace;
#endif
}

// For now, 320x240 output resolution is assumed, using simple line-skipping
//  and pixel-skipping downscaler.
// TODO: Flesh these out so they return useful values based on whether
//       running on higher-res device or a resampling downscaler is enabled.
static inline uint_fast8_t PixelSkipEnabled()
{
#ifdef NO_HWSCALE
	return gpu_unai.config.pixel_skip;
#else
	return Config.VideoScaling == 1 && gpu_unai.config.pixel_skip;
#endif
}

static inline uint_fast8_t LineSkipEnabled()
{
	return true;
}

#endif // GPU_UNAI_H
