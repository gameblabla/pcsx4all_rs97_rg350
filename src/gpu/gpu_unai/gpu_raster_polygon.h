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

//senquack - NOTE: GPU Unai poly routines have been rewritten/adapted
// from DrHell routines to fix multiple issues. See README_senquack.txt

///////////////////////////////////////////////////////////////////////////////
// Shared poly vertex buffer, able to handle 3 or 4-pt polys of any type.
///////////////////////////////////////////////////////////////////////////////

struct PolyVertex {
	s32 x, y; // Sign-extended 11-bit X,Y coords
	union {
		struct { u8 u, v, pad[2]; } tex; // Texture coords (if used)
		u32 tex_word;
	};
	union {
		struct { u8 r, g, b, pad; } col; // 24-bit RGB color (if used)
		u32 col_word;
	};
};

static PolyVertex poly_vbuf[4];

enum PolyAttribute {
	POLYATTR_TEXTURE = (1 << 0),
	POLYATTR_GOURAUD = (1 << 1)
};

enum PolyType {
	POLYTYPE_F  = 0,
	POLYTYPE_FT = (POLYATTR_TEXTURE),
	POLYTYPE_G  = (POLYATTR_GOURAUD),
	POLYTYPE_GT = (POLYATTR_TEXTURE | POLYATTR_GOURAUD)
};

///////////////////////////////////////////////////////////////////////////////
// polyInitVertexBuffer()
// Fills poly_vbuf[] array with data from any type of poly draw-command packet.
///////////////////////////////////////////////////////////////////////////////
static void polyInitVertexBuffer(const PtrUnion packet, PolyType ptype, u32 is_quad)
{
	bool texturing = ptype & POLYATTR_TEXTURE;
	bool gouraud   = ptype & POLYATTR_GOURAUD;

	int vert_stride = 1; // Stride of vertices in cmd packet, in 32-bit words
	if (texturing)
		vert_stride++;
	if (gouraud)
		vert_stride++;

	int num_verts = (is_quad) ? 4 : 3;
	u32 *ptr;

	// X,Y coords, adjusted by draw offsets
	s32 x_off = gpu_unai.DrawingOffset[0];
	s32 y_off = gpu_unai.DrawingOffset[1];
	ptr = &packet.U4[1];
	for (int i=0;  i < num_verts; ++i, ptr += vert_stride) {
		s16* coord_ptr = (s16*)ptr;
		poly_vbuf[i].x = GPU_EXPANDSIGN(coord_ptr[0]) + x_off;
		poly_vbuf[i].y = GPU_EXPANDSIGN(coord_ptr[1]) + y_off;
	}

	// U,V texture coords (if applicable)
	if (texturing) {
		ptr = &packet.U4[2];
		for (int i=0;  i < num_verts; ++i, ptr += vert_stride)
			poly_vbuf[i].tex_word = *ptr;
	}

	// Colors (if applicable)
	if (gouraud) {
		ptr = &packet.U4[0];
		for (int i=0;  i < num_verts; ++i, ptr += vert_stride)
			poly_vbuf[i].col_word = *ptr;
	}
}

///////////////////////////////////////////////////////////////////////////////
//  Helper functions to determine which vertex in a 2 or 3 vertex array
//   has the highest/lowest X/Y coordinate.
//   Note: the comparison logic is such that, given a set of vertices with
//    identical values for a given coordinate, a different index will be
//    returned from vertIdxOfLeast..() than a call to vertIdxOfHighest..().
//    This ensures that, during the vertex-ordering phase of rasterization,
//    all three vertices remain unique.
///////////////////////////////////////////////////////////////////////////////

template<typename T>
static inline int vertIdxOfLeastXCoord2(const T *Tptr)
{
	return (Tptr[0].x <= Tptr[1].x) ? 0 : 1;
}

template<typename T>
static inline int vertIdxOfLeastXCoord3(const T *Tptr)
{
	int least_of_v0_v1 = vertIdxOfLeastXCoord2(Tptr);
	return (Tptr[least_of_v0_v1].x <= Tptr[2].x) ? least_of_v0_v1 : 2;
}

template<typename T>
static inline int vertIdxOfLeastYCoord2(const T *Tptr)
{
	return (Tptr[0].y <= Tptr[1].y) ? 0 : 1;
}

template<typename T>
static inline int vertIdxOfLeastYCoord3(const T *Tptr)
{
	int least_of_v0_v1 = vertIdxOfLeastYCoord2(Tptr);
	return (Tptr[least_of_v0_v1].y <= Tptr[2].y) ? least_of_v0_v1 : 2;
}

template<typename T>
static inline int vertIdxOfHighestXCoord2(const T *Tptr)
{
	return (Tptr[1].x >= Tptr[0].x) ? 1 : 0;
}

template<typename T>
static inline int vertIdxOfHighestXCoord3(const T *Tptr)
{
	int highest_of_v0_v1 = vertIdxOfHighestXCoord2(Tptr);
	return (Tptr[2].x >= Tptr[highest_of_v0_v1].x) ? 2 : highest_of_v0_v1;
}

template<typename T>
static inline int vertIdxOfHighestYCoord2(const T *Tptr)
{
	return (Tptr[1].y >= Tptr[0].y) ? 1 : 0;
}

template<typename T>
static inline int vertIdxOfHighestYCoord3(const T *Tptr)
{
	int highest_of_v0_v1 = vertIdxOfHighestYCoord2(Tptr);
	return (Tptr[2].y >= Tptr[highest_of_v0_v1].y) ? 2 : highest_of_v0_v1;
}

///////////////////////////////////////////////////////////////////////////////
// polyUseTriangle()
//  Determines if the specified triangle should be rendered. If so, it
//  fills the given array of vertex pointers, vert_ptrs, in order of
//  increasing Y coordinate values, as required by rasterization algorithm.
//  Parameter 'tri_num' is 0 for first triangle (idx 0,1,2 of poly_vbuf[]),
//   or 1 for second triangle of a quad (idx 1,2,3 of poly_vbuf[]).
//  Returns true if triangle should be rendered, false if not.
///////////////////////////////////////////////////////////////////////////////
static bool polyUseTriangle(int tri_num, PolyVertex **vert_ptrs)
{
	// Using verts 0,1,2 or is this the 2nd pass of a quad (verts 1,2,3)?
	PolyVertex *triplet_ptr = &poly_vbuf[(tri_num == 0) ? 0 : 1];

	// Get indices of highest/lowest X,Y coords within triplet
	int idx_lowest_x  = vertIdxOfLeastXCoord3(triplet_ptr);
	int idx_highest_x = vertIdxOfHighestXCoord3(triplet_ptr);
	int idx_lowest_y  = vertIdxOfLeastYCoord3(triplet_ptr);
	int idx_highest_y = vertIdxOfHighestYCoord3(triplet_ptr);

	// Maximum absolute distance between any two X coordinates is 1023,
	//  and for Y coordinates is 511 (PS1 hardware limitation)
	int lowest_x  = triplet_ptr[idx_lowest_x].x;
	int highest_x = triplet_ptr[idx_highest_x].x;
	int lowest_y  = triplet_ptr[idx_lowest_y].y;
	int highest_y = triplet_ptr[idx_highest_y].y;
	if ((highest_x - lowest_x) >= CHKMAX_X ||
	    (highest_y - lowest_y) >= CHKMAX_Y)
		return false;

	// Determine if triangle is completely outside clipping range
	int xmin, xmax, ymin, ymax;
	xmin = gpu_unai.DrawingArea[0];  xmax = gpu_unai.DrawingArea[2];
	ymin = gpu_unai.DrawingArea[1];  ymax = gpu_unai.DrawingArea[3];
	int clipped_lowest_x  = Max2(xmin,lowest_x);
	int clipped_lowest_y  = Max2(ymin,lowest_y);
	int clipped_highest_x = Min2(xmax,highest_x);
	int clipped_highest_y = Min2(ymax,highest_y);
	if (clipped_lowest_x >= clipped_highest_x ||
	    clipped_lowest_y >= clipped_highest_y)
		return false;

	// Order vertex ptrs by increasing y value (draw routines need this).
	// The middle index is deduced by a binary math trick that depends
	//  on index range always being between 0..2
	vert_ptrs[0] = triplet_ptr + idx_lowest_y;
	vert_ptrs[1] = triplet_ptr + ((idx_lowest_y + idx_highest_y) ^ 3);
	vert_ptrs[2] = triplet_ptr + idx_highest_y;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
//  GPU internal polygon drawing functions
///////////////////////////////////////////////////////////////////////////////

/*----------------------------------------------------------------------
gpuDrawPolyF - Flat-shaded, untextured poly
----------------------------------------------------------------------*/
void gpuDrawPolyF(const PtrUnion packet, const PP gpuPolySpanDriver, u32 is_quad)
{
	gpu_unai.PixelData = GPU_RGB16(packet.U4[0]);

	polyInitVertexBuffer(packet, POLYTYPE_F, is_quad);

	int total_passes = is_quad ? 2 : 1;
	int cur_pass = 0;
	do
	{
		PolyVertex* vptrs[3];
		if (polyUseTriangle(cur_pass, vptrs) == false)
			continue;

		s32 xa, xb, ya, yb;
		s32 x3, dx3, x4, dx4, dx;
		s32 x0, x1, x2, y0, y1, y2;

		x0 = vptrs[0]->x;  y0 = vptrs[0]->y;
		x1 = vptrs[1]->x;  y1 = vptrs[1]->y;
		x2 = vptrs[2]->x;  y2 = vptrs[2]->y;

		ya = y2 - y0;
		yb = y2 - y1;
		dx = (x2 - x1) * ya - (x2 - x0) * yb;

		for (int loop0 = 2; loop0; loop0--) {
			if (loop0 == 2) {
				ya = y0;  yb = y1;
				x3 = x4 = i2x(x0);
				if (dx < 0) {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx3 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) * FloatInv(y2 - y0)) : 0;
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) * FloatInv(y1 - y0)) : 0;
#else
					dx3 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) / (float)(y2 - y0)) : 0;
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) / (float)(y1 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx3 = ((y2 - y0) != 0) ? xLoDivx((x2 - x0), (y2 - y0)) : 0;
					dx4 = ((y1 - y0) != 0) ? xLoDivx((x1 - x0), (y1 - y0)) : 0;
#else
					dx3 = ((y2 - y0) != 0) ? GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0)) : 0;
					dx4 = ((y1 - y0) != 0) ? GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0)) : 0;
#endif
#endif
				} else {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx3 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) * FloatInv(y1 - y0)) : 0;
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) * FloatInv(y2 - y0)) : 0;
#else
					dx3 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) / (float)(y1 - y0)) : 0;
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) / (float)(y2 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx3 = ((y1 - y0) != 0) ? xLoDivx((x1 - x0), (y1 - y0)) : 0;
					dx4 = ((y2 - y0) != 0) ? xLoDivx((x2 - x0), (y2 - y0)) : 0;
#else
					dx3 = ((y1 - y0) != 0) ? GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0)) : 0;
					dx4 = ((y2 - y0) != 0) ? GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0)) : 0;
#endif
#endif
				}
			} else {
				//senquack - break out of final loop if nothing to be drawn (1st loop
				//           must always be taken to setup dx3/dx4)
				if (y1 == y2) break;

				ya = y1;  yb = y2;

				if (dx < 0) {
					x3 = i2x(x0) + (dx3 * (y1 - y0));
					x4 = i2x(x1);
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) * FloatInv(y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) / (float)(y2 - y1)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? xLoDivx ((x2 - x1), (y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1)) : 0;
#endif
#endif
				} else {
					x3 = i2x(x1);
					x4 = i2x(x0) + (dx4 * (y1 - y0));
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx3 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) * FloatInv(y2 - y1)) : 0;
#else
					dx3 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) / (float)(y2 - y1)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx3 = ((y2 - y1) != 0) ? xLoDivx ((x2 - x1), (y2 - y1)) : 0;
#else
					dx3 = ((y2 - y1) != 0) ? GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1)) : 0;
#endif
#endif
				}
			}

			s32 xmin, xmax, ymin, ymax;
			xmin = gpu_unai.DrawingArea[0];  xmax = gpu_unai.DrawingArea[2];
			ymin = gpu_unai.DrawingArea[1];  ymax = gpu_unai.DrawingArea[3];

			if ((ymin - ya) > 0) {
				x3 += (dx3 * (ymin - ya));
				x4 += (dx4 * (ymin - ya));
				ya = ymin;
			}

			if (yb > ymax) yb = ymax;

			int loop1 = yb - ya;
			if (loop1 <= 0)
				continue;

			u16* PixelBase = &((u16*)gpu_unai.vram)[FRAME_OFFSET(0, ya)];
			int li=gpu_unai.ilace_mask;
			int pi=(ProgressiveInterlaceEnabled()?(gpu_unai.ilace_mask+1):0);
			int pif=(ProgressiveInterlaceEnabled()?(gpu_unai.prog_ilace_flag?(gpu_unai.ilace_mask+1):0):1);

			for (; loop1; --loop1, ya++, PixelBase += FRAME_WIDTH,
					x3 += dx3, x4 += dx4 )
			{
				if (ya&li) continue;
				if ((ya&pi)==pif) continue;

				xa = FixedCeilToInt(x3);  xb = FixedCeilToInt(x4);
				if ((xmin - xa) > 0) xa = xmin;
				if (xb > xmax) xb = xmax;
				if ((xb - xa) > 0) gpuPolySpanDriver(PixelBase + xa, (xb - xa));
			}
		}
	} while (++cur_pass < total_passes);
}

/*----------------------------------------------------------------------
gpuDrawPolyFT - Flat-shaded, textured poly
----------------------------------------------------------------------*/
void gpuDrawPolyFT(const PtrUnion packet, const PP gpuPolySpanDriver, u32 is_quad)
{
	gpu_unai.r4 = packet.U1[0];
	gpu_unai.g4 = packet.U1[1];
	gpu_unai.b4 = packet.U1[2];
	//senquack - TODO: remove these, I don't think they're used in inner driver for FT polys:
	gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;

	polyInitVertexBuffer(packet, POLYTYPE_FT, is_quad);

	int total_passes = is_quad ? 2 : 1;
	int cur_pass = 0;
	do
	{
		PolyVertex* vptrs[3];
		if (polyUseTriangle(cur_pass, vptrs) == false)
			continue;

		s32 xa, xb, ya, yb;
		s32 x3, dx3, x4, dx4, dx;
		s32 u3, du3, v3, dv3;
		s32 x0, x1, x2, y0, y1, y2;
		s32 u0, u1, u2, v0, v1, v2;

		x0 = vptrs[0]->x;      y0 = vptrs[0]->y;
		u0 = vptrs[0]->tex.u;  v0 = vptrs[0]->tex.v;
		x1 = vptrs[1]->x;      y1 = vptrs[1]->y;
		u1 = vptrs[1]->tex.u;  v1 = vptrs[1]->tex.v;
		x2 = vptrs[2]->x;      y2 = vptrs[2]->y;
		u2 = vptrs[2]->tex.u;  v2 = vptrs[2]->tex.v;

		ya = y2 - y0;
		yb = y2 - y1;
		dx4 = (x2 - x1) * ya - (x2 - x0) * yb;
		gpu_unai.du4 = (u2 - u1) * ya - (u2 - u0) * yb;
		gpu_unai.dv4 = (v2 - v1) * ya - (v2 - v0) * yb;
		dx = dx4;
		if (dx4 < 0) {
			dx4 = -dx4;
			gpu_unai.du4 = -gpu_unai.du4;
			gpu_unai.dv4 = -gpu_unai.dv4;
		}

#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
		if (dx4 != 0) {
			float finv = FloatInv(dx4);
			gpu_unai.du4 = (fixed)((gpu_unai.du4 << FIXED_BITS) * finv);
			gpu_unai.dv4 = (fixed)((gpu_unai.dv4 << FIXED_BITS) * finv);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = 0;
		}
#else
		if (dx4 != 0) {
			float fdiv = dx4;
			gpu_unai.du4 = (fixed)((gpu_unai.du4 << FIXED_BITS) / fdiv);
			gpu_unai.dv4 = (fixed)((gpu_unai.dv4 << FIXED_BITS) / fdiv);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = 0;
		}
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
		if (dx4 != 0) {
			int iF, iS;
			xInv(dx4, iF, iS);
			gpu_unai.du4 = xInvMulx(gpu_unai.du4, iF, iS);
			gpu_unai.dv4 = xInvMulx(gpu_unai.dv4, iF, iS);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = 0;
		}
#else
		if (dx4 != 0) {
			gpu_unai.du4 = GPU_FAST_DIV(gpu_unai.du4 << FIXED_BITS, dx4);
			gpu_unai.dv4 = GPU_FAST_DIV(gpu_unai.dv4 << FIXED_BITS, dx4);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = 0;
		}
#endif
#endif

		//senquack - TODO: why is it always going through 2 iterations when sometimes one would suffice here?
		//			 (SAME ISSUE ELSEWHERE)
		for (s32 loop0 = 2; loop0; loop0--) {
			if (loop0 == 2) {
				ya = y0;  yb = y1;
				x3 = x4 = i2x(x0);
				u3 = i2x(u0);  v3 = i2x(v0);
				if (dx < 0) {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y2 - y0) != 0) {
						float finv = FloatInv(y2 - y0);
						dx3 = (fixed)(((x2 - x0) << FIXED_BITS) * finv);
						du3 = (fixed)(((u2 - u0) << FIXED_BITS) * finv);
						dv3 = (fixed)(((v2 - v0) << FIXED_BITS) * finv);
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) * FloatInv(y1 - y0)) : 0;
#else
					if ((y2 - y0) != 0) {
						float fdiv = y2 - y0;
						dx3 = (fixed)(((x2 - x0) << FIXED_BITS) / fdiv);
						du3 = (fixed)(((u2 - u0) << FIXED_BITS) / fdiv);
						dv3 = (fixed)(((v2 - v0) << FIXED_BITS) / fdiv);
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) / (float)(y1 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y2 - y0) != 0) {
						int iF, iS;
						xInv((y2 - y0), iF, iS);
						dx3 = xInvMulx((x2 - x0), iF, iS);
						du3 = xInvMulx((u2 - u0), iF, iS);
						dv3 = xInvMulx((v2 - v0), iF, iS);
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? xLoDivx((x1 - x0), (y1 - y0)) : 0;
#else
					if ((y2 - y0) != 0) {
						dx3 = GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0));
						du3 = GPU_FAST_DIV((u2 - u0) << FIXED_BITS, (y2 - y0));
						dv3 = GPU_FAST_DIV((v2 - v0) << FIXED_BITS, (y2 - y0));
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0)) : 0;
#endif
#endif
				} else {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y1 - y0) != 0) {
						float finv = FloatInv(y1 - y0);
						dx3 = (fixed)(((x1 - x0) << FIXED_BITS) * finv);
						du3 = (fixed)(((u1 - u0) << FIXED_BITS) * finv);
						dv3 = (fixed)(((v1 - v0) << FIXED_BITS) * finv);
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) * FloatInv(y2 - y0)) : 0;
#else
					if ((y1 - y0) != 0) {
						float fdiv = y1 - y0;
						dx3 = (fixed)(((x1 - x0) << FIXED_BITS) / fdiv);
						du3 = (fixed)(((u1 - u0) << FIXED_BITS) / fdiv);
						dv3 = (fixed)(((v1 - v0) << FIXED_BITS) / fdiv);
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) / (float)(y2 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y1 - y0) != 0) {
						int iF, iS;
						xInv((y1 - y0), iF, iS);
						dx3 = xInvMulx((x1 - x0), iF, iS);
						du3 = xInvMulx((u1 - u0), iF, iS);
						dv3 = xInvMulx((v1 - v0), iF, iS);
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? xLoDivx((x2 - x0), (y2 - y0)) : 0;
#else
					if ((y1 - y0) != 0) {
						dx3 = GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0));
						du3 = GPU_FAST_DIV((u1 - u0) << FIXED_BITS, (y1 - y0));
						dv3 = GPU_FAST_DIV((v1 - v0) << FIXED_BITS, (y1 - y0));
					} else {
						dx3 = du3 = dv3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0)) : 0;
#endif
#endif
				}
			} else {
				//senquack - break out of final loop if nothing to be drawn (1st loop
				//           must always be taken to setup dx3/dx4)
				if (y1 == y2) break;

				ya = y1;  yb = y2;

				if (dx < 0) {
					x3 = i2x(x0);
					x4 = i2x(x1);
					u3 = i2x(u0);
					v3 = i2x(v0);
					if ((y1 - y0) != 0) {
						x3 += (dx3 * (y1 - y0));
						u3 += (du3 * (y1 - y0));
						v3 += (dv3 * (y1 - y0));
					}
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) * FloatInv(y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) / (float)(y2 - y1)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? xLoDivx((x2 - x1), (y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1)) : 0;
#endif
#endif
				} else {
					x3 = i2x(x1);
					x4 = i2x(x0) + (dx4 * (y1 - y0));
					u3 = i2x(u1);
					v3 = i2x(v1);
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y2 - y1) != 0) {
						float finv = FloatInv(y2 - y1);
						dx3 = (fixed)(((x2 - x1) << FIXED_BITS) * finv);
						du3 = (fixed)(((u2 - u1) << FIXED_BITS) * finv);
						dv3 = (fixed)(((v2 - v1) << FIXED_BITS) * finv);
					} else {
						dx3 = du3 = dv3 = 0;
					}
#else
					if ((y2 - y1) != 0) {
						float fdiv = y2 - y1;
						dx3 = (fixed)(((x2 - x1) << FIXED_BITS) / fdiv);
						du3 = (fixed)(((u2 - u1) << FIXED_BITS) / fdiv);
						dv3 = (fixed)(((v2 - v1) << FIXED_BITS) / fdiv);
					} else {
						dx3 = du3 = dv3 = 0;
					}
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y2 - y1) != 0) {
						int iF, iS;
						xInv((y2 - y1), iF, iS);
						dx3 = xInvMulx((x2 - x1), iF, iS);
						du3 = xInvMulx((u2 - u1), iF, iS);
						dv3 = xInvMulx((v2 - v1), iF, iS);
					} else {
						dx3 = du3 = dv3 = 0;
					}
#else 
					if ((y2 - y1) != 0) {
						dx3 = GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1));
						du3 = GPU_FAST_DIV((u2 - u1) << FIXED_BITS, (y2 - y1));
						dv3 = GPU_FAST_DIV((v2 - v1) << FIXED_BITS, (y2 - y1));
					} else {
						dx3 = du3 = dv3 = 0;
					}
#endif
#endif
				}
			}

			s32 xmin, xmax, ymin, ymax;
			xmin = gpu_unai.DrawingArea[0];  xmax = gpu_unai.DrawingArea[2];
			ymin = gpu_unai.DrawingArea[1];  ymax = gpu_unai.DrawingArea[3];

			if ((ymin - ya) > 0) {
				x3 += dx3 * (ymin - ya);
				x4 += dx4 * (ymin - ya);
				u3 += du3 * (ymin - ya);
				v3 += dv3 * (ymin - ya);
				ya = ymin;
			}

			if (yb > ymax) yb = ymax;

			int loop1 = yb - ya;
			if (loop1 <= 0)
				continue;

			u16* PixelBase = &((u16*)gpu_unai.vram)[FRAME_OFFSET(0, ya)];
			int li=gpu_unai.ilace_mask;
			int pi=(ProgressiveInterlaceEnabled()?(gpu_unai.ilace_mask+1):0);
			int pif=(ProgressiveInterlaceEnabled()?(gpu_unai.prog_ilace_flag?(gpu_unai.ilace_mask+1):0):1);

			for (; loop1; --loop1, ++ya, PixelBase += FRAME_WIDTH,
					x3 += dx3, x4 += dx4,
					u3 += du3, v3 += dv3 )
			{
				if (ya&li) continue;
				if ((ya&pi)==pif) continue;

				xa = FixedCeilToInt(x3);  xb = FixedCeilToInt(x4);
				gpu_unai.u4 = u3;  gpu_unai.v4 = v3;

				fixed itmp = i2x(xa) - x3;
				if (itmp != 0) {
					gpu_unai.u4 += (gpu_unai.du4 * itmp) >> FIXED_BITS;
					gpu_unai.v4 += (gpu_unai.dv4 * itmp) >> FIXED_BITS;
				}

				gpu_unai.u4 += fixed_HALF;
				gpu_unai.v4 += fixed_HALF;

				if ((xmin - xa) > 0) {
					gpu_unai.u4 += gpu_unai.du4 * (xmin - xa);
					gpu_unai.v4 += gpu_unai.dv4 * (xmin - xa);
					xa = xmin;
				}

				if (xb > xmax) xb = xmax;
				if ((xb - xa) > 0) gpuPolySpanDriver(PixelBase + xa, (xb - xa));
			}
		}
	} while (++cur_pass < total_passes);
}

/*----------------------------------------------------------------------
gpuDrawPolyG - Gouraud-shaded, untextured poly
----------------------------------------------------------------------*/
void gpuDrawPolyG(const PtrUnion packet, const PP gpuPolySpanDriver, u32 is_quad)
{
	polyInitVertexBuffer(packet, POLYTYPE_G, is_quad);

	int total_passes = is_quad ? 2 : 1;
	int cur_pass = 0;
	do
	{
		PolyVertex* vptrs[3];
		if (polyUseTriangle(cur_pass, vptrs) == false)
			continue;

		s32 xa, xb, ya, yb;
		s32 x3, dx3, x4, dx4, dx;
		s32 r3, dr3, g3, dg3, b3, db3;
		s32 x0, x1, x2, y0, y1, y2;
		s32 r0, r1, r2, g0, g1, g2, b0, b1, b2;
		x0 = vptrs[0]->x;      y0 = vptrs[0]->y;
		r0 = vptrs[0]->col.r;  g0 = vptrs[0]->col.g;  b0 = vptrs[0]->col.b;
		x1 = vptrs[1]->x;      y1 = vptrs[1]->y;
		r1 = vptrs[1]->col.r;  g1 = vptrs[1]->col.g;  b1 = vptrs[1]->col.b;
		x2 = vptrs[2]->x;      y2 = vptrs[2]->y;
		r2 = vptrs[2]->col.r;  g2 = vptrs[2]->col.g;  b2 = vptrs[2]->col.b;

		ya = y2 - y0;
		yb = y2 - y1;
		dx4 = (x2 - x1) * ya - (x2 - x0) * yb;
		gpu_unai.dr4 = (r2 - r1) * ya - (r2 - r0) * yb;
		gpu_unai.dg4 = (g2 - g1) * ya - (g2 - g0) * yb;
		gpu_unai.db4 = (b2 - b1) * ya - (b2 - b0) * yb;
		dx = dx4;
		if (dx4 < 0) {
			dx4 = -dx4;
			gpu_unai.dr4 = -gpu_unai.dr4;
			gpu_unai.dg4 = -gpu_unai.dg4;
			gpu_unai.db4 = -gpu_unai.db4;
		}

#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
		if (dx4 != 0) {
			float finv = FloatInv(dx4);
			gpu_unai.dr4 = (fixed)((gpu_unai.dr4 << FIXED_BITS) * finv);
			gpu_unai.dg4 = (fixed)((gpu_unai.dg4 << FIXED_BITS) * finv);
			gpu_unai.db4 = (fixed)((gpu_unai.db4 << FIXED_BITS) * finv);
		} else {
			gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#else
		if (dx4 != 0) {
			float fdiv = dx4;
			gpu_unai.dr4 = (fixed)((gpu_unai.dr4 << FIXED_BITS) / fdiv);
			gpu_unai.dg4 = (fixed)((gpu_unai.dg4 << FIXED_BITS) / fdiv);
			gpu_unai.db4 = (fixed)((gpu_unai.db4 << FIXED_BITS) / fdiv);
		} else {
			gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
		if (dx4 != 0) {
			int iF, iS;
			xInv(dx4, iF, iS);
			gpu_unai.dr4 = xInvMulx(gpu_unai.dr4, iF, iS);
			gpu_unai.dg4 = xInvMulx(gpu_unai.dg4, iF, iS);
			gpu_unai.db4 = xInvMulx(gpu_unai.db4, iF, iS);
		} else {
			gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#else
		if (dx4 != 0) {
			gpu_unai.dr4 = GPU_FAST_DIV(gpu_unai.dr4 << FIXED_BITS, dx4);
			gpu_unai.dg4 = GPU_FAST_DIV(gpu_unai.dg4 << FIXED_BITS, dx4);
			gpu_unai.db4 = GPU_FAST_DIV(gpu_unai.db4 << FIXED_BITS, dx4);
		} else {
			gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#endif
#endif

		// Adapted old Unai code: (New routines use 22.10 fixed point, Unai 16.16)
		// Used by gouraud versions of gpuPolySpanDriver() to increment all three at once:
		u32 dr = (u32)(gpu_unai.dr4 << 14)&(0xffffffff<<21);  if (gpu_unai.dr4<0) dr += 1<<21;
		u32 dg = (u32)(gpu_unai.dg4 <<  3)&(0xffffffff<<10);  if (gpu_unai.dg4<0) dg += 1<<10;
		u32 db = (u32)(gpu_unai.db4 >>  8)&(0xffffffff    );  if (gpu_unai.db4<0) db += 1<< 0;
		gpu_unai.lInc = db + dg + dr;

		for (s32 loop0 = 2; loop0; loop0--) {
			if (loop0 == 2) {
				ya = y0;
				yb = y1;
				x3 = x4 = i2x(x0);
				r3 = i2x(r0);
				g3 = i2x(g0);
				b3 = i2x(b0);
				if (dx < 0) {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y2 - y0) != 0) {
						float finv = FloatInv(y2 - y0);
						dx3 = (fixed)(((x2 - x0) << FIXED_BITS) * finv);
						dr3 = (fixed)(((r2 - r0) << FIXED_BITS) * finv);
						dg3 = (fixed)(((g2 - g0) << FIXED_BITS) * finv);
						db3 = (fixed)(((b2 - b0) << FIXED_BITS) * finv);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) * FloatInv(y1 - y0)) : 0;
#else
					if ((y2 - y0) != 0) {
						float fdiv = y2 - y0;
						dx3 = (fixed)(((x2 - x0) << FIXED_BITS) / fdiv);
						dr3 = (fixed)(((r2 - r0) << FIXED_BITS) / fdiv);
						dg3 = (fixed)(((g2 - g0) << FIXED_BITS) / fdiv);
						db3 = (fixed)(((b2 - b0) << FIXED_BITS) / fdiv);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) / (float)(y1 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y2 - y0) != 0) {
						int iF, iS;
						xInv((y2 - y0), iF, iS);
						dx3 = xInvMulx((x2 - x0), iF, iS);
						dr3 = xInvMulx((r2 - r0), iF, iS);
						dg3 = xInvMulx((g2 - g0), iF, iS);
						db3 = xInvMulx((b2 - b0), iF, iS);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? xLoDivx((x1 - x0), (y1 - y0)) : 0;
#else
					if ((y2 - y0) != 0) {
						dx3 = GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0));
						dr3 = GPU_FAST_DIV((r2 - r0) << FIXED_BITS, (y2 - y0));
						dg3 = GPU_FAST_DIV((g2 - g0) << FIXED_BITS, (y2 - y0));
						db3 = GPU_FAST_DIV((b2 - b0) << FIXED_BITS, (y2 - y0));
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0)) : 0;
#endif
#endif
				} else {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y1 - y0) != 0) {
						float finv = FloatInv(y1 - y0);
						dx3 = (fixed)(((x1 - x0) << FIXED_BITS) * finv);
						dr3 = (fixed)(((r1 - r0) << FIXED_BITS) * finv);
						dg3 = (fixed)(((g1 - g0) << FIXED_BITS) * finv);
						db3 = (fixed)(((b1 - b0) << FIXED_BITS) * finv);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) * FloatInv(y2 - y0)) : 0;
#else
					if ((y1 - y0) != 0) {
						float fdiv = y1 - y0;
						dx3 = (fixed)(((x1 - x0) << FIXED_BITS) / fdiv);
						dr3 = (fixed)(((r1 - r0) << FIXED_BITS) / fdiv);
						dg3 = (fixed)(((g1 - g0) << FIXED_BITS) / fdiv);
						db3 = (fixed)(((b1 - b0) << FIXED_BITS) / fdiv);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) / (float)(y2 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y1 - y0) != 0) {
						int iF, iS;
						xInv((y1 - y0), iF, iS);
						dx3 = xInvMulx((x1 - x0), iF, iS);
						dr3 = xInvMulx((r1 - r0), iF, iS);
						dg3 = xInvMulx((g1 - g0), iF, iS);
						db3 = xInvMulx((b1 - b0), iF, iS);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? xLoDivx((x2 - x0), (y2 - y0)) : 0;
#else
					if ((y1 - y0) != 0) {
						dx3 = GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0));
						dr3 = GPU_FAST_DIV((r1 - r0) << FIXED_BITS, (y1 - y0));
						dg3 = GPU_FAST_DIV((g1 - g0) << FIXED_BITS, (y1 - y0));
						db3 = GPU_FAST_DIV((b1 - b0) << FIXED_BITS, (y1 - y0));
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0)) : 0;
#endif
#endif
				}
			} else {
				//senquack - break out of final loop if nothing to be drawn (1st loop
				//           must always be taken to setup dx3/dx4)
				if (y1 == y2) break;

				ya = y1;  yb = y2;

				if (dx < 0) {
					x3 = i2x(x0);  x4 = i2x(x1);
					r3 = i2x(r0);  g3 = i2x(g0);  b3 = i2x(b0);

					if ((y1 - y0) != 0) {
						x3 += (dx3 * (y1 - y0));
						r3 += (dr3 * (y1 - y0));
						g3 += (dg3 * (y1 - y0));
						b3 += (db3 * (y1 - y0));
					}

#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) * FloatInv(y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) / (float)(y2 - y1)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? xLoDivx((x2 - x1), (y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1)) : 0;
#endif
#endif
				} else {
					x3 = i2x(x1);
					x4 = i2x(x0) + (dx4 * (y1 - y0));

					r3 = i2x(r1);  g3 = i2x(g1);  b3 = i2x(b1);

#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y2 - y1) != 0) {
						float finv = FloatInv(y2 - y1);
						dx3 = (fixed)(((x2 - x1) << FIXED_BITS) * finv);
						dr3 = (fixed)(((r2 - r1) << FIXED_BITS) * finv);
						dg3 = (fixed)(((g2 - g1) << FIXED_BITS) * finv);
						db3 = (fixed)(((b2 - b1) << FIXED_BITS) * finv);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
#else
					if ((y2 - y1) != 0) {
						float fdiv = y2 - y1;
						dx3 = (fixed)(((x2 - x1) << FIXED_BITS) / fdiv);
						dr3 = (fixed)(((r2 - r1) << FIXED_BITS) / fdiv);
						dg3 = (fixed)(((g2 - g1) << FIXED_BITS) / fdiv);
						db3 = (fixed)(((b2 - b1) << FIXED_BITS) / fdiv);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y2 - y1) != 0) {
						int iF, iS;
						xInv((y2 - y1), iF, iS);
						dx3 = xInvMulx((x2 - x1), iF, iS);
						dr3 = xInvMulx((r2 - r1), iF, iS);
						dg3 = xInvMulx((g2 - g1), iF, iS);
						db3 = xInvMulx((b2 - b1), iF, iS);
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
#else
					if ((y2 - y1) != 0) {
						dx3 = GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1));
						dr3 = GPU_FAST_DIV((r2 - r1) << FIXED_BITS, (y2 - y1));
						dg3 = GPU_FAST_DIV((g2 - g1) << FIXED_BITS, (y2 - y1));
						db3 = GPU_FAST_DIV((b2 - b1) << FIXED_BITS, (y2 - y1));
					} else {
						dx3 = dr3 = dg3 = db3 = 0;
					}
#endif
#endif
				}
			}

			s32 xmin, xmax, ymin, ymax;
			xmin = gpu_unai.DrawingArea[0];  xmax = gpu_unai.DrawingArea[2];
			ymin = gpu_unai.DrawingArea[1];  ymax = gpu_unai.DrawingArea[3];

			if ((ymin - ya) > 0) {
				x3 += (dx3 * (ymin - ya));
				x4 += (dx4 * (ymin - ya));
				r3 += (dr3 * (ymin - ya));
				g3 += (dg3 * (ymin - ya));
				b3 += (db3 * (ymin - ya));
				ya = ymin;
			}

			if (yb > ymax) yb = ymax;

			int loop1 = yb - ya;
			if (loop1 <= 0)
				continue;

			u16* PixelBase = &((u16*)gpu_unai.vram)[FRAME_OFFSET(0, ya)];
			int li=gpu_unai.ilace_mask;
			int pi=(ProgressiveInterlaceEnabled()?(gpu_unai.ilace_mask+1):0);
			int pif=(ProgressiveInterlaceEnabled()?(gpu_unai.prog_ilace_flag?(gpu_unai.ilace_mask+1):0):1);

			for (; loop1; --loop1, ++ya, PixelBase += FRAME_WIDTH,
					x3 += dx3, x4 += dx4,
					r3 += dr3, g3 += dg3, b3 += db3 )
			{
				if (ya&li) continue;
				if ((ya&pi)==pif) continue;

				xa = FixedCeilToInt(x3);
				xb = FixedCeilToInt(x4);
				gpu_unai.r4 = r3;  gpu_unai.g4 = g3;  gpu_unai.b4 = b3;

				fixed itmp = i2x(xa) - x3;
				if (itmp != 0) {
					gpu_unai.r4 += (gpu_unai.dr4 * itmp) >> FIXED_BITS;
					gpu_unai.g4 += (gpu_unai.dg4 * itmp) >> FIXED_BITS;
					gpu_unai.b4 += (gpu_unai.db4 * itmp) >> FIXED_BITS;
				}

				gpu_unai.r4 += fixed_HALF;
				gpu_unai.g4 += fixed_HALF;
				gpu_unai.b4 += fixed_HALF;

				if ((xmin - xa) > 0) {
					gpu_unai.r4 += (gpu_unai.dr4 * (xmin - xa));
					gpu_unai.g4 += (gpu_unai.dg4 * (xmin - xa));
					gpu_unai.b4 += (gpu_unai.db4 * (xmin - xa));
					xa = xmin;
				}

				if (xb > xmax) xb = xmax;
				if ((xb - xa) > 0) gpuPolySpanDriver(PixelBase + xa, (xb - xa));
			}
		}
	} while (++cur_pass < total_passes);
}

/*----------------------------------------------------------------------
gpuDrawPolyGT - Gouraud-shaded, textured poly
----------------------------------------------------------------------*/
void gpuDrawPolyGT(const PtrUnion packet, const PP gpuPolySpanDriver, u32 is_quad)
{
	polyInitVertexBuffer(packet, POLYTYPE_GT, is_quad);

	int total_passes = is_quad ? 2 : 1;
	int cur_pass = 0;
	do
	{
		PolyVertex* vptrs[3];
		if (polyUseTriangle(cur_pass, vptrs) == false)
			continue;

		s32 xa, xb, ya, yb;
		s32 x3, dx3, x4, dx4, dx;
		s32 u3, du3, v3, dv3;
		s32 r3, dr3, g3, dg3, b3, db3;
		s32 x0, x1, x2, y0, y1, y2;
		s32 u0, u1, u2, v0, v1, v2;
		s32 r0, r1, r2, g0, g1, g2, b0, b1, b2;

		x0 = vptrs[0]->x;      y0 = vptrs[0]->y;
		u0 = vptrs[0]->tex.u;  v0 = vptrs[0]->tex.v;
		r0 = vptrs[0]->col.r;  g0 = vptrs[0]->col.g;  b0 = vptrs[0]->col.b;
		x1 = vptrs[1]->x;      y1 = vptrs[1]->y;
		u1 = vptrs[1]->tex.u;  v1 = vptrs[1]->tex.v;
		r1 = vptrs[1]->col.r;  g1 = vptrs[1]->col.g;  b1 = vptrs[1]->col.b;
		x2 = vptrs[2]->x;      y2 = vptrs[2]->y;
		u2 = vptrs[2]->tex.u;  v2 = vptrs[2]->tex.v;
		r2 = vptrs[2]->col.r;  g2 = vptrs[2]->col.g;  b2 = vptrs[2]->col.b;

		ya = y2 - y0;
		yb = y2 - y1;
		dx4 = (x2 - x1) * ya - (x2 - x0) * yb;
		gpu_unai.du4 = (u2 - u1) * ya - (u2 - u0) * yb;
		gpu_unai.dv4 = (v2 - v1) * ya - (v2 - v0) * yb;
		gpu_unai.dr4 = (r2 - r1) * ya - (r2 - r0) * yb;
		gpu_unai.dg4 = (g2 - g1) * ya - (g2 - g0) * yb;
		gpu_unai.db4 = (b2 - b1) * ya - (b2 - b0) * yb;
		dx = dx4;
		if (dx4 < 0) {
			dx4 = -dx4;
			gpu_unai.du4 = -gpu_unai.du4;
			gpu_unai.dv4 = -gpu_unai.dv4;
			gpu_unai.dr4 = -gpu_unai.dr4;
			gpu_unai.dg4 = -gpu_unai.dg4;
			gpu_unai.db4 = -gpu_unai.db4;
		}

#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
		if (dx4 != 0) {
			float finv = FloatInv(dx4);
			gpu_unai.du4 = (fixed)((gpu_unai.du4 << FIXED_BITS) * finv);
			gpu_unai.dv4 = (fixed)((gpu_unai.dv4 << FIXED_BITS) * finv);
			gpu_unai.dr4 = (fixed)((gpu_unai.dr4 << FIXED_BITS) * finv);
			gpu_unai.dg4 = (fixed)((gpu_unai.dg4 << FIXED_BITS) * finv);
			gpu_unai.db4 = (fixed)((gpu_unai.db4 << FIXED_BITS) * finv);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#else
		if (dx4 != 0) {
			float fdiv = dx4;
			gpu_unai.du4 = (fixed)((gpu_unai.du4 << FIXED_BITS) / fdiv);
			gpu_unai.dv4 = (fixed)((gpu_unai.dv4 << FIXED_BITS) / fdiv);
			gpu_unai.dr4 = (fixed)((gpu_unai.dr4 << FIXED_BITS) / fdiv);
			gpu_unai.dg4 = (fixed)((gpu_unai.dg4 << FIXED_BITS) / fdiv);
			gpu_unai.db4 = (fixed)((gpu_unai.db4 << FIXED_BITS) / fdiv);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
		if (dx4 != 0) {
			int iF, iS;
			xInv(dx4, iF, iS);
			gpu_unai.du4 = xInvMulx(gpu_unai.du4, iF, iS);
			gpu_unai.dv4 = xInvMulx(gpu_unai.dv4, iF, iS);
			gpu_unai.dr4 = xInvMulx(gpu_unai.dr4, iF, iS);
			gpu_unai.dg4 = xInvMulx(gpu_unai.dg4, iF, iS);
			gpu_unai.db4 = xInvMulx(gpu_unai.db4, iF, iS);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#else
		if (dx4 != 0) {
			gpu_unai.du4 = GPU_FAST_DIV(gpu_unai.du4 << FIXED_BITS, dx4);
			gpu_unai.dv4 = GPU_FAST_DIV(gpu_unai.dv4 << FIXED_BITS, dx4);
			gpu_unai.dr4 = GPU_FAST_DIV(gpu_unai.dr4 << FIXED_BITS, dx4);
			gpu_unai.dg4 = GPU_FAST_DIV(gpu_unai.dg4 << FIXED_BITS, dx4);
			gpu_unai.db4 = GPU_FAST_DIV(gpu_unai.db4 << FIXED_BITS, dx4);
		} else {
			gpu_unai.du4 = gpu_unai.dv4 = gpu_unai.dr4 = gpu_unai.dg4 = gpu_unai.db4 = 0;
		}
#endif
#endif

		// Adapted old Unai code: (New routines use 22.10 fixed point, Unai 16.16)
		// Used by gouraud versions of gpuPolySpanDriver() to increment all three at once:
		u32 dr = (u32)(gpu_unai.dr4 << 14)&(0xffffffff<<21);  if (gpu_unai.dr4<0) dr += 1<<21;
		u32 dg = (u32)(gpu_unai.dg4 <<  3)&(0xffffffff<<10);  if (gpu_unai.dg4<0) dg += 1<<10;
		u32 db = (u32)(gpu_unai.db4 >>  8)&(0xffffffff    );  if (gpu_unai.db4<0) db += 1<< 0;
		gpu_unai.lInc = db + dg + dr;

		for (s32 loop0 = 2; loop0; loop0--) {
			if (loop0 == 2) {
				ya = y0;  yb = y1;
				x3 = x4 = i2x(x0);
				u3 = i2x(u0);  v3 = i2x(v0);
				r3 = i2x(r0);  g3 = i2x(g0);  b3 = i2x(b0);
				if (dx < 0) {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y2 - y0) != 0) {
						float finv = FloatInv(y2 - y0);
						dx3 = (fixed)(((x2 - x0) << FIXED_BITS) * finv);
						du3 = (fixed)(((u2 - u0) << FIXED_BITS) * finv);
						dv3 = (fixed)(((v2 - v0) << FIXED_BITS) * finv);
						dr3 = (fixed)(((r2 - r0) << FIXED_BITS) * finv);
						dg3 = (fixed)(((g2 - g0) << FIXED_BITS) * finv);
						db3 = (fixed)(((b2 - b0) << FIXED_BITS) * finv);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) * FloatInv(y1 - y0)) : 0;
#else
					if ((y2 - y0) != 0) {
						float fdiv = y2 - y0;
						dx3 = (fixed)(((x2 - x0) << FIXED_BITS) / fdiv);
						du3 = (fixed)(((u2 - u0) << FIXED_BITS) / fdiv);
						dv3 = (fixed)(((v2 - v0) << FIXED_BITS) / fdiv);
						dr3 = (fixed)(((r2 - r0) << FIXED_BITS) / fdiv);
						dg3 = (fixed)(((g2 - g0) << FIXED_BITS) / fdiv);
						db3 = (fixed)(((b2 - b0) << FIXED_BITS) / fdiv);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? (fixed)(((x1 - x0) << FIXED_BITS) / (float)(y1 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y2 - y0) != 0) {
						int iF, iS;
						xInv((y2 - y0), iF, iS);
						dx3 = xInvMulx((x2 - x0), iF, iS);
						du3 = xInvMulx((u2 - u0), iF, iS);
						dv3 = xInvMulx((v2 - v0), iF, iS);
						dr3 = xInvMulx((r2 - r0), iF, iS);
						dg3 = xInvMulx((g2 - g0), iF, iS);
						db3 = xInvMulx((b2 - b0), iF, iS);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? xLoDivx((x1 - x0), (y1 - y0)) : 0;
#else
					if ((y2 - y0) != 0) {
						dx3 = GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0));
						du3 = GPU_FAST_DIV((u2 - u0) << FIXED_BITS, (y2 - y0));
						dv3 = GPU_FAST_DIV((v2 - v0) << FIXED_BITS, (y2 - y0));
						dr3 = GPU_FAST_DIV((r2 - r0) << FIXED_BITS, (y2 - y0));
						dg3 = GPU_FAST_DIV((g2 - g0) << FIXED_BITS, (y2 - y0));
						db3 = GPU_FAST_DIV((b2 - b0) << FIXED_BITS, (y2 - y0));
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y1 - y0) != 0) ? GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0)) : 0;
#endif
#endif
				} else {
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y1 - y0) != 0) {
						float finv = FloatInv(y1 - y0);
						dx3 = (fixed)(((x1 - x0) << FIXED_BITS) * finv);
						du3 = (fixed)(((u1 - u0) << FIXED_BITS) * finv);
						dv3 = (fixed)(((v1 - v0) << FIXED_BITS) * finv);
						dr3 = (fixed)(((r1 - r0) << FIXED_BITS) * finv);
						dg3 = (fixed)(((g1 - g0) << FIXED_BITS) * finv);
						db3 = (fixed)(((b1 - b0) << FIXED_BITS) * finv);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) * FloatInv(y2 - y0)) : 0;
#else
					if ((y1 - y0) != 0) {
						float fdiv = y1 - y0;
						dx3 = (fixed)(((x1 - x0) << FIXED_BITS) / fdiv);
						du3 = (fixed)(((u1 - u0) << FIXED_BITS) / fdiv);
						dv3 = (fixed)(((v1 - v0) << FIXED_BITS) / fdiv);
						dr3 = (fixed)(((r1 - r0) << FIXED_BITS) / fdiv);
						dg3 = (fixed)(((g1 - g0) << FIXED_BITS) / fdiv);
						db3 = (fixed)(((b1 - b0) << FIXED_BITS) / fdiv);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? (fixed)(((x2 - x0) << FIXED_BITS) / float(y2 - y0)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y1 - y0) != 0) {
						int iF, iS;
						xInv((y1 - y0), iF, iS);
						dx3 = xInvMulx((x1 - x0), iF, iS);
						du3 = xInvMulx((u1 - u0), iF, iS);
						dv3 = xInvMulx((v1 - v0), iF, iS);
						dr3 = xInvMulx((r1 - r0), iF, iS);
						dg3 = xInvMulx((g1 - g0), iF, iS);
						db3 = xInvMulx((b1 - b0), iF, iS);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? xLoDivx((x2 - x0), (y2 - y0)) : 0;
#else
					if ((y1 - y0) != 0) {
						dx3 = GPU_FAST_DIV((x1 - x0) << FIXED_BITS, (y1 - y0));
						du3 = GPU_FAST_DIV((u1 - u0) << FIXED_BITS, (y1 - y0));
						dv3 = GPU_FAST_DIV((v1 - v0) << FIXED_BITS, (y1 - y0));
						dr3 = GPU_FAST_DIV((r1 - r0) << FIXED_BITS, (y1 - y0));
						dg3 = GPU_FAST_DIV((g1 - g0) << FIXED_BITS, (y1 - y0));
						db3 = GPU_FAST_DIV((b1 - b0) << FIXED_BITS, (y1 - y0));
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
					dx4 = ((y2 - y0) != 0) ? GPU_FAST_DIV((x2 - x0) << FIXED_BITS, (y2 - y0)) : 0;
#endif
#endif
				}
			} else {
				//senquack - break out of final loop if nothing to be drawn (1st loop
				//           must always be taken to setup dx3/dx4)
				if (y1 == y2) break;

				ya = y1;  yb = y2;

				if (dx < 0) {
					x3 = i2x(x0);  x4 = i2x(x1);
					u3 = i2x(u0);  v3 = i2x(v0);
					r3 = i2x(r0);  g3 = i2x(g0);  b3 = i2x(b0);

					if ((y1 - y0) != 0) {
						x3 += (dx3 * (y1 - y0));
						u3 += (du3 * (y1 - y0));
						v3 += (dv3 * (y1 - y0));
						r3 += (dr3 * (y1 - y0));
						g3 += (dg3 * (y1 - y0));
						b3 += (db3 * (y1 - y0));
					}

#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) * FloatInv(y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? (fixed)(((x2 - x1) << FIXED_BITS) / (float)(y2 - y1)) : 0;
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					dx4 = ((y2 - y1) != 0) ? xLoDivx((x2 - x1), (y2 - y1)) : 0;
#else
					dx4 = ((y2 - y1) != 0) ? GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1)) : 0;
#endif
#endif
				} else {
					x3 = i2x(x1);
					x4 = i2x(x0) + (dx4 * (y1 - y0));

					u3 = i2x(u1);  v3 = i2x(v1);
					r3 = i2x(r1);  g3 = i2x(g1);  b3 = i2x(b1);
#ifdef GPU_UNAI_USE_FLOATMATH
#ifdef GPU_UNAI_USE_FLOAT_DIV_MULTINV
					if ((y2 - y1) != 0) {
						float finv = FloatInv(y2 - y1);
						dx3 = (fixed)(((x2 - x1) << FIXED_BITS) * finv);
						du3 = (fixed)(((u2 - u1) << FIXED_BITS) * finv);
						dv3 = (fixed)(((v2 - v1) << FIXED_BITS) * finv);
						dr3 = (fixed)(((r2 - r1) << FIXED_BITS) * finv);
						dg3 = (fixed)(((g2 - g1) << FIXED_BITS) * finv);
						db3 = (fixed)(((b2 - b1) << FIXED_BITS) * finv);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
#else
					if ((y2 - y1) != 0) {
						float fdiv = y2 - y1;
						dx3 = (fixed)(((x2 - x1) << FIXED_BITS) / fdiv);
						du3 = (fixed)(((u2 - u1) << FIXED_BITS) / fdiv);
						dv3 = (fixed)(((v2 - v1) << FIXED_BITS) / fdiv);
						dr3 = (fixed)(((r2 - r1) << FIXED_BITS) / fdiv);
						dg3 = (fixed)(((g2 - g1) << FIXED_BITS) / fdiv);
						db3 = (fixed)(((b2 - b1) << FIXED_BITS) / fdiv);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
#endif
#else  // Integer Division:
#ifdef GPU_UNAI_USE_INT_DIV_MULTINV
					if ((y2 - y1) != 0) {
						int iF, iS;
						xInv((y2 - y1), iF, iS);
						dx3 = xInvMulx((x2 - x1), iF, iS);
						du3 = xInvMulx((u2 - u1), iF, iS);
						dv3 = xInvMulx((v2 - v1), iF, iS);
						dr3 = xInvMulx((r2 - r1), iF, iS);
						dg3 = xInvMulx((g2 - g1), iF, iS);
						db3 = xInvMulx((b2 - b1), iF, iS);
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
#else
					if ((y2 - y1) != 0) {
						dx3 = GPU_FAST_DIV((x2 - x1) << FIXED_BITS, (y2 - y1));
						du3 = GPU_FAST_DIV((u2 - u1) << FIXED_BITS, (y2 - y1));
						dv3 = GPU_FAST_DIV((v2 - v1) << FIXED_BITS, (y2 - y1));
						dr3 = GPU_FAST_DIV((r2 - r1) << FIXED_BITS, (y2 - y1));
						dg3 = GPU_FAST_DIV((g2 - g1) << FIXED_BITS, (y2 - y1));
						db3 = GPU_FAST_DIV((b2 - b1) << FIXED_BITS, (y2 - y1));
					} else {
						dx3 = du3 = dv3 = dr3 = dg3 = db3 = 0;
					}
#endif
#endif
				}
			}

			s32 xmin, xmax, ymin, ymax;
			xmin = gpu_unai.DrawingArea[0];  xmax = gpu_unai.DrawingArea[2];
			ymin = gpu_unai.DrawingArea[1];  ymax = gpu_unai.DrawingArea[3];

			if ((ymin - ya) > 0) {
				x3 += (dx3 * (ymin - ya));
				x4 += (dx4 * (ymin - ya));
				u3 += (du3 * (ymin - ya));
				v3 += (dv3 * (ymin - ya));
				r3 += (dr3 * (ymin - ya));
				g3 += (dg3 * (ymin - ya));
				b3 += (db3 * (ymin - ya));
				ya = ymin;
			}

			if (yb > ymax) yb = ymax;

			int loop1 = yb - ya;
			if (loop1 <= 0)
				continue;

			u16* PixelBase = &((u16*)gpu_unai.vram)[FRAME_OFFSET(0, ya)];
			int li=gpu_unai.ilace_mask;
			int pi=(ProgressiveInterlaceEnabled()?(gpu_unai.ilace_mask+1):0);
			int pif=(ProgressiveInterlaceEnabled()?(gpu_unai.prog_ilace_flag?(gpu_unai.ilace_mask+1):0):1);

			for (; loop1; --loop1, ++ya, PixelBase += FRAME_WIDTH,
					x3 += dx3, x4 += dx4,
					u3 += du3, v3 += dv3,
					r3 += dr3, g3 += dg3, b3 += db3 )
			{
				if (ya&li) continue;
				if ((ya&pi)==pif) continue;

				xa = FixedCeilToInt(x3);
				xb = FixedCeilToInt(x4);
				gpu_unai.u4 = u3;  gpu_unai.v4 = v3;
				gpu_unai.r4 = r3;  gpu_unai.g4 = g3;  gpu_unai.b4 = b3;

				fixed itmp = i2x(xa) - x3;
				if (itmp != 0) {
					gpu_unai.u4 += (gpu_unai.du4 * itmp) >> FIXED_BITS;
					gpu_unai.v4 += (gpu_unai.dv4 * itmp) >> FIXED_BITS;
					gpu_unai.r4 += (gpu_unai.dr4 * itmp) >> FIXED_BITS;
					gpu_unai.g4 += (gpu_unai.dg4 * itmp) >> FIXED_BITS;
					gpu_unai.b4 += (gpu_unai.db4 * itmp) >> FIXED_BITS;
				}

				gpu_unai.u4 += fixed_HALF;
				gpu_unai.v4 += fixed_HALF;
				gpu_unai.r4 += fixed_HALF;
				gpu_unai.g4 += fixed_HALF;
				gpu_unai.b4 += fixed_HALF;

				if ((xmin - xa) > 0) {
					gpu_unai.u4 += gpu_unai.du4 * (xmin - xa);
					gpu_unai.v4 += gpu_unai.dv4 * (xmin - xa);
					gpu_unai.r4 += gpu_unai.dr4 * (xmin - xa);
					gpu_unai.g4 += gpu_unai.dg4 * (xmin - xa);
					gpu_unai.b4 += gpu_unai.db4 * (xmin - xa);
					xa = xmin;
				}

				if (xb > xmax) xb = xmax;
				if ((xb - xa) > 0) gpuPolySpanDriver(PixelBase + xa, (xb - xa));
			}
		}
	} while (++cur_pass < total_passes);
}
