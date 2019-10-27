/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
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

#ifndef _OP_BLEND_H_
#define _OP_BLEND_H_

//  GPU Blending operations functions

////////////////////////////////////////////////////////////////////////////////
// Blend bgr555 color in 'uSrc' (foreground) with bgr555 color
//  in 'uDst' (background), returning resulting color.
//
// INPUT:
//  'uSrc','uDst' input: -bbbbbgggggrrrrr
//                       ^ bit 16
// OUTPUT:
//           uint16_t output: 0bbbbbgggggrrrrr
//                       ^ bit 16
// RETURNS:
// Where '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
template <int BLENDMODE, uint_fast8_t SKIP_USRC_MSB_MASK>
GPU_INLINE uint16_t gpuBlending(uint16_t uSrc, uint16_t uDst)
{
	// These use Blargg's bitwise modulo-clamping:
	//  http://blargg.8bitalley.com/info/rgb_mixing.html
	//  http://blargg.8bitalley.com/info/rgb_clamped_add.html
	//  http://blargg.8bitalley.com/info/rgb_clamped_sub.html

	uint16_t mix;

	// 0.5 x Back + 0.5 x Forward
	if (BLENDMODE==0) {
#ifdef GPU_UNAI_USE_ACCURATE_BLENDING
		// Slower, but more accurate (doesn't lose LSB data)
		uDst &= 0x7fff;
		if (!SKIP_USRC_MSB_MASK)
			uSrc &= 0x7fff;
		mix = ((uSrc + uDst) - ((uSrc ^ uDst) & 0x0421)) >> 1;
#else
		mix = ((uDst & 0x7bde) + (uSrc & 0x7bde)) >> 1;
#endif
	}

	// 1.0 x Back + 1.0 x Forward
	if (BLENDMODE==1) {
		uDst &= 0x7fff;
		if (!SKIP_USRC_MSB_MASK)
			uSrc &= 0x7fff;
		uint32_t sum      = uSrc + uDst;
		uint32_t low_bits = (uSrc ^ uDst) & 0x0421;
		uint32_t carries  = (sum - low_bits) & 0x8420;
		uint32_t modulo   = sum - carries;
		uint32_t clamp    = carries - (carries >> 5);
		mix = modulo | clamp;
	}

	// 1.0 x Back - 1.0 x Forward
	if (BLENDMODE==2) {
		uDst &= 0x7fff;
		if (!SKIP_USRC_MSB_MASK)
			uSrc &= 0x7fff;
		uint32_t diff     = uDst - uSrc + 0x8420;
		uint32_t low_bits = (uDst ^ uSrc) & 0x8420;
		uint32_t borrows  = (diff - low_bits) & 0x8420;
		uint32_t modulo   = diff - borrows;
		uint32_t clamp    = borrows - (borrows >> 5);
		mix = modulo & clamp;
	}

	// 1.0 x Back + 0.25 x Forward
	if (BLENDMODE==3) {
		uDst &= 0x7fff;
		uSrc = ((uSrc >> 2) & 0x1ce7);
		uint32_t sum      = uSrc + uDst;
		uint32_t low_bits = (uSrc ^ uDst) & 0x0421;
		uint32_t carries  = (sum - low_bits) & 0x8420;
		uint32_t modulo   = sum - carries;
		uint32_t clamp    = carries - (carries >> 5);
		mix = modulo | clamp;
	}

	return mix;
}


////////////////////////////////////////////////////////////////////////////////
// Convert bgr555 color in uSrc to padded uint32_t 5.4:5.4:5.4 bgr fixed-pt
//  color triplet suitable for use with HQ 24-bit quantization.
//
// INPUT:
//       'uDst' input: -bbbbbgggggrrrrr
//                     ^ bit 16
// RETURNS:
//         uint32_t output: 000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                     ^ bit 31
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
GPU_INLINE uint32_t gpuGetRGB24(uint16_t uSrc)
{
	return ((uSrc & 0x7C00)<<14)
	     | ((uSrc & 0x03E0)<< 9)
	     | ((uSrc & 0x001F)<< 4);
}


////////////////////////////////////////////////////////////////////////////////
// Blend padded uint32_t 5.4:5.4:5.4 bgr fixed-pt color triplet in 'uSrc24'
//  (foreground color) with bgr555 color in 'uDst' (background color),
//  returning the resulting uint32_t 5.4:5.4:5.4 color.
//
// INPUT:
//     'uSrc24' input: 000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                     ^ bit 31
//       'uDst' input: -bbbbbgggggrrrrr
//                     ^ bit 16
// RETURNS:
//         uint32_t output: 000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                     ^ bit 31
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
template <int BLENDMODE>
GPU_INLINE uint32_t gpuBlending24(uint32_t uSrc24, uint16_t uDst)
{
	// These use techniques adapted from Blargg's techniques mentioned in
	//  in gpuBlending() comments above. Not as much bitwise trickery is
	//  necessary because of presence of 0 padding in uSrc24 format.

	uint32_t uDst24 = gpuGetRGB24(uDst);
	uint32_t mix;

	// 0.5 x Back + 0.5 x Forward
	if (BLENDMODE==0) {
		const uint32_t uMsk = 0x1FE7F9FE;
		// Only need to mask LSBs of uSrc24, uDst24's LSBs are 0 already
		mix = (uDst24 + (uSrc24 & uMsk)) >> 1;
	}

	// 1.0 x Back + 1.0 x Forward
	if (BLENDMODE==1) {
		uint32_t sum     = uSrc24 + uDst24;
		uint32_t carries = sum & 0x20080200;
		uint32_t modulo  = sum - carries;
		uint32_t clamp   = carries - (carries >> 9);
		mix = modulo | clamp;
	}

	// 1.0 x Back - 1.0 x Forward
	if (BLENDMODE==2) {
		// Insert ones in 0-padded borrow slot of color to be subtracted from
		uDst24 |= 0x20080200;
		uint32_t diff    = uDst24 - uSrc24;
		uint32_t borrows = diff & 0x20080200;
		uint32_t clamp   = borrows - (borrows >> 9);
		mix = diff & clamp;
	}

	// 1.0 x Back + 0.25 x Forward
	if (BLENDMODE==3) {
		uSrc24 = (uSrc24 & 0x1FC7F1FC) >> 2;
		uint32_t sum     = uSrc24 + uDst24;
		uint32_t carries = sum & 0x20080200;
		uint32_t modulo  = sum - carries;
		uint32_t clamp   = carries - (carries >> 9);
		mix = modulo | clamp;
	}

	return mix;
}

#endif  //_OP_BLEND_H_
