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

///////////////////////////////////////////////////////////////////////////////
#ifndef USE_GPULIB
void gpuLoadImage(PtrUnion packet)
{
	uint16_t x0, y0, w0, h0;
	x0 = packet.U2[2] & 1023;
	y0 = packet.U2[3] & 511;
	w0 = packet.U2[4];
	h0 = packet.U2[5];

	if ((y0 + h0) > FRAME_HEIGHT)
	{
		h0 = FRAME_HEIGHT - y0;
	}

	gpu_unai.dma.FrameToWrite = ((w0)&&(h0));

	gpu_unai.dma.px = 0;
	gpu_unai.dma.py = 0;
	gpu_unai.dma.x_end = w0;
	gpu_unai.dma.y_end = h0;
	gpu_unai.dma.pvram = &((uint16_t*)gpu_unai.vram)[x0+(y0*1024)];

	gpu_unai.GPU_GP1 |= 0x08000000;
}
#endif // !USE_GPULIB

///////////////////////////////////////////////////////////////////////////////
#ifndef USE_GPULIB
void gpuStoreImage(PtrUnion packet)
{
	uint16_t x0, y0, w0, h0;
	x0 = packet.U2[2] & 1023;
	y0 = packet.U2[3] & 511;
	w0 = packet.U2[4];
	h0 = packet.U2[5];

	if ((y0 + h0) > FRAME_HEIGHT)
	{
		h0 = FRAME_HEIGHT - y0;
	}
	gpu_unai.dma.FrameToRead = ((w0)&&(h0));

	gpu_unai.dma.px = 0;
	gpu_unai.dma.py = 0;
	gpu_unai.dma.x_end = w0;
	gpu_unai.dma.y_end = h0;
	gpu_unai.dma.pvram = &((uint16_t*)gpu_unai.vram)[x0+(y0*1024)];
	
	gpu_unai.GPU_GP1 |= 0x08000000;
}
#endif // !USE_GPULIB

void gpuMoveImage(PtrUnion packet)
{
	uint32_t x0, y0, x1, y1;
	int32_t w0, h0;
	x0 = packet.U2[2] & 1023;
	y0 = packet.U2[3] & 511;
	x1 = packet.U2[4] & 1023;
	y1 = packet.U2[5] & 511;
	w0 = packet.U2[6];
	h0 = packet.U2[7];

	if( (x0==x1) && (y0==y1) ) return;
	if ((w0<=0) || (h0<=0)) return;
	
	#ifdef ENABLE_GPU_LOG_SUPPORT
		fprintf(stdout,"gpuMoveImage(x0=%u,y0=%u,x1=%u,y1=%u,w0=%d,h0=%d)\n",x0,y0,x1,y1,w0,h0);
	#endif
	
	if (((y0+h0)>512)||((x0+w0)>1024)||((y1+h0)>512)||((x1+w0)>1024))
	{
		uint16_t *psxVuw=gpu_unai.vram;
		int32_t i,j;
	    for(j=0;j<h0;j++)
		 for(i=0;i<w0;i++)
		  psxVuw [(1024*((y1+j)&511))+((x1+i)&0x3ff)]=
		   psxVuw[(1024*((y0+j)&511))+((x0+i)&0x3ff)];
	}
	else if ((x0&1)||(x1&1))
	{
		uint16_t *lpDst, *lpSrc;
		lpDst = lpSrc = (uint16_t*)gpu_unai.vram;
		lpSrc += FRAME_OFFSET(x0, y0);
		lpDst += FRAME_OFFSET(x1, y1);
		x1 = FRAME_WIDTH - w0;
		do {
			x0=w0;
			do { *lpDst++ = *lpSrc++; } while (--x0);
			lpDst += x1;
			lpSrc += x1;
		} while (--h0);
	}
	else
	{
		uint32_t *lpDst, *lpSrc;
		lpDst = lpSrc = (uint32_t*)(void*)gpu_unai.vram;
		lpSrc += ((FRAME_OFFSET(x0, y0))>>1);
		lpDst += ((FRAME_OFFSET(x1, y1))>>1);
		if (w0&1)
		{
			x1 = (FRAME_WIDTH - w0 +1)>>1;
			w0>>=1;
			if (!w0) {
				do {
					*((uint16_t*)lpDst) = *((uint16_t*)lpSrc);
					lpDst += x1;
					lpSrc += x1;
				} while (--h0);
			} else
			do {
				x0=w0;
				do { *lpDst++ = *lpSrc++; } while (--x0);
				*((uint16_t*)lpDst) = *((uint16_t*)lpSrc);
				lpDst += x1;
				lpSrc += x1;
			} while (--h0);
		}
		else
		{
			x1 = (FRAME_WIDTH - w0)>>1;
			w0>>=1;
			do {
				x0=w0;
				do { *lpDst++ = *lpSrc++; } while (--x0);
				lpDst += x1;
				lpSrc += x1;
			} while (--h0);
		}
	}
}

void gpuClearImage(PtrUnion packet)
{
	int32_t   x0, y0, w0, h0;
	x0 = packet.S2[2];
	y0 = packet.S2[3];
	w0 = packet.S2[4] & 0x3ff;
	h0 = packet.S2[5] & 0x3ff;
	 
	w0 += x0;
	if (x0 < 0) x0 = 0;
	if (w0 > FRAME_WIDTH) w0 = FRAME_WIDTH;
	w0 -= x0;
	if (w0 <= 0) return;
	h0 += y0;
	if (y0 < 0) y0 = 0;
	if (h0 > FRAME_HEIGHT) h0 = FRAME_HEIGHT;
	h0 -= y0;
	if (h0 <= 0) return;

	#ifdef ENABLE_GPU_LOG_SUPPORT
		fprintf(stdout,"gpuClearImage(x0=%d,y0=%d,w0=%d,h0=%d)\n",x0,y0,w0,h0);
	#endif
	
	if (x0&1)
	{
		uint16_t* pixel = (uint16_t*)gpu_unai.vram + FRAME_OFFSET(x0, y0);
		uint16_t rgb = GPU_RGB16(packet.U4[0]);
		y0 = FRAME_WIDTH - w0;
		do {
			x0=w0;
			do { *pixel++ = rgb; } while (--x0);
			pixel += y0;
		} while (--h0);
	}
	else
	{
		uint32_t* pixel = (uint32_t*)gpu_unai.vram + ((FRAME_OFFSET(x0, y0))>>1);
		uint32_t rgb = GPU_RGB16(packet.U4[0]);
		rgb |= (rgb<<16);
		if (w0&1)
		{
			y0 = (FRAME_WIDTH - w0 +1)>>1;
			w0>>=1;
			do {
				x0=w0;
				do { *pixel++ = rgb; } while (--x0);
				*((uint16_t*)pixel) = (uint16_t)rgb;
				pixel += y0;
			} while (--h0);
		}
		else
		{
			y0 = (FRAME_WIDTH - w0)>>1;
			w0>>=1;
			do {
				x0=w0;
				do { *pixel++ = rgb; } while (--x0);
				pixel += y0;
			} while (--h0);
		}
	}
}
