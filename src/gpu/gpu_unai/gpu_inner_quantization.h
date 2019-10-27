/***************************************************************************
*   Copyright (C) 2016 PCSX4ALL Team                                      *
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

#ifndef _OP_DITHER_H_
#define _OP_DITHER_H_

static void SetupDitheringConstants()
{
	// Initialize Dithering Constants
	// The screen is divided into 8x8 chunks and sub-unitary noise is applied
	// using the following matrix. This ensures that data lost in color
	// quantization will be added back to the image 'by chance' in predictable
	// patterns that are naturally 'smoothed' by your sight when viewed from a
	// certain distance.
	//
	// http://caca.zoy.org/study/index.html
	//
	// Shading colors are encoded in 4.5, and then are quantitized to 5.0,
	// DitherMatrix constants reflect that.

	static const uint8_t DitherMatrix[] = {
		 0, 32,  8, 40,  2, 34, 10, 42,
		48, 16, 56, 24, 50, 18, 58, 26,
		12, 44,  4, 36, 14, 46,  6, 38,
		60, 28, 52, 20, 62, 30, 54, 22,
		 3, 35, 11, 43,  1, 33,  9, 41,
		51, 19, 59, 27, 49, 17, 57, 25,
		15, 47,  7, 39, 13, 45,  5, 37,
		63, 31, 55, 23, 61, 29, 53, 21
	};

	int i, j;
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 8; j++)
		{
			uint16_t offset = (i << 3) | j;

			uint32_t component = ((DitherMatrix[offset] + 1) << 4) / 65; //[5.5] -> [5]

			// XXX - senquack - hack Dec 2016
			//  Until JohnnyF gets the time to work further on dithering,
			//   force lower bit of component to 0. This fixes grid pattern
			//   affecting quality of dithered image, as well as loss of
			//   detail in dark areas. With lower bit unset like this, existing
			//   27-bit accuracy of dithering math is unneeded, could be 24-bit.
			//   Is 8x8 matrix overkill as a result, can we use 4x4?
			component &= ~1;

			gpu_unai.DitherMatrix[offset] = (component)
			                              | (component << 10)
			                              | (component << 20);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Convert padded uint32_t 5.4:5.4:5.4 bgr fixed-pt triplet to final bgr555 color,
//  applying dithering if specified by template parameter.
//
// INPUT:
//     'uSrc24' input: 000bbbbbXXXX0gggggXXXX0rrrrrXXXX
//                     ^ bit 31
//       'pDst' is a pointer to destination framebuffer pixel, used
//         to determine which DitherMatrix[] entry to apply.
// RETURNS:
//         uint16_t output: 0bbbbbgggggrrrrr
//                     ^ bit 16
// Where 'X' are fixed-pt bits, '0' is zero-padding, and '-' is don't care
////////////////////////////////////////////////////////////////////////////////
template <int DITHER>
GPU_INLINE uint16_t gpuColorQuantization24(uint32_t uSrc24, const uint16_t *pDst)
{
	if (DITHER)
	{
		uint16_t fbpos  = (uint32_t)(pDst - gpu_unai.vram);
		uint16_t offset = ((fbpos & (0x7 << 10)) >> 7) | (fbpos & 0x7);

		//clean overflow flags and add
		uSrc24 = (uSrc24 & 0x1FF7FDFF) + gpu_unai.DitherMatrix[offset];

		if (uSrc24 & (1<< 9)) uSrc24 |= (0x1FF    );
		if (uSrc24 & (1<<19)) uSrc24 |= (0x1FF<<10);
		if (uSrc24 & (1<<29)) uSrc24 |= (0x1FF<<20);
	}

	return ((uSrc24>> 4) & (0x1F    ))
	     | ((uSrc24>> 9) & (0x1F<<5 ))
	     | ((uSrc24>>14) & (0x1F<<10));
}

#endif //_OP_DITHER_H_
