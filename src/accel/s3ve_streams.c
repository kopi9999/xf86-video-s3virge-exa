/*
Copyright (C) 1994-2000 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2026 kopi9999

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

/* This file contains functions used for controlling the streams processor. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "s3v.h"


/* This function inits the STREAMS processor variables.
 * This has essentially been taken from the accel/s3_virge code and the databook.
 */
void s3ve_initSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams, DisplayModePtr mode)
{
  PVERB5("	S3VInitSTREAMS\n");

  switch (pScrn->bitsPerPixel)
    {
    case 16:
      streams[0] = 0x05000000;
      break;
    case 24:
                         /* data format 8.8.8 (24 bpp) */
      streams[0] = 0x06000000;
      break;
    case 32:
                         /* one more bit for X.8.8.8, 32 bpp */
      streams[0] = 0x07000000;
      break;
    }
                         /* NO chroma keying... */
   streams[1] = 0x0;
                         /* Secondary stream format KRGB-16 */
                         /* data book suggestion... */
   streams[2] = 0x03000000;

   streams[3] = 0x0;

   streams[4] = 0x0;
                         /* use 0x01000000 for primary over second. */
                         /* use 0x0 for second over prim. */
   streams[5] = 0x01000000;

   streams[6] = 0x0;

   streams[7] = 0x0;
                                /* Stride is 3 bytes for 24 bpp mode and */
                                /* 4 bytes for 32 bpp. */
   switch(pScrn->bitsPerPixel)
     {
     case 16:
       streams[8] =
	 pScrn->displayWidth * 2;
       break;
     case 24:
       streams[8] =
	 pScrn->displayWidth * 3;
      break;
     case 32:
       streams[8] =
	 pScrn->displayWidth * 4;
      break;
     }
                                /* Choose fbaddr0 as stream source. */
   streams[9] = 0x0;
   streams[10] = 0x0;
   streams[11] = 0x0;
   streams[12] = 0x1;

                                /* Set primary stream on top of secondary */
                                /* stream. */
   streams[13] = 0xc0000000;
                               /* Vertical scale factor. */
   streams[14] = 0x0;

   streams[15] = 0x0;
                                /* Vertical accum. initial value. */
   streams[16] = 0x0;
                                /* X and Y start coords + 1. */
   streams[18] =  0x00010001;

         /* Specify window Width -1 and Height of */
         /* stream. */
   streams[19] =
         (mode->HDisplay - 1) << 16 |
         (mode->VDisplay);

                                /* Book says 0x07ff07ff. */
   streams[20] = 0x07ff07ff;

   streams[21] = 0x00010001;
}

/* This function disables the STREAMS processor as per databook.
 * This is useful before we do a mode change
 */

void s3ve_disableSTREAMS(ScrnInfoPtr pScrn)
{
unsigned char tmp;
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

   VerticalRetraceWait();
   OUTREG(FIFO_CONTROL_REG, 0xC000);
   VGAOUT8(vgaCRIndex, 0x67);
   tmp = VGAIN8(vgaCRReg);
                         /* Disable STREAMS processor */
   VGAOUT8( vgaCRReg, tmp & ~0x0C );

   return;
}

/* This function saves the STREAMS registers to driver's private structure */

void s3ve_saveSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams)
{
  S3VPtr ps3v = S3VPTR(pScrn);

   streams[0] = INREG(PSTREAM_CONTROL_REG);
   streams[1] = INREG(COL_CHROMA_KEY_CONTROL_REG);
   streams[2] = INREG(SSTREAM_CONTROL_REG);
   streams[3] = INREG(CHROMA_KEY_UPPER_BOUND_REG);
   streams[4] = INREG(SSTREAM_STRETCH_REG);
   streams[5] = INREG(BLEND_CONTROL_REG);
   streams[6] = INREG(PSTREAM_FBADDR0_REG);
   streams[7] = INREG(PSTREAM_FBADDR1_REG);
   streams[8] = INREG(PSTREAM_STRIDE_REG);
   streams[9] = INREG(DOUBLE_BUFFER_REG);
   streams[10] = INREG(SSTREAM_FBADDR0_REG);
   streams[11] = INREG(SSTREAM_FBADDR1_REG);
   streams[12] = INREG(SSTREAM_STRIDE_REG);
   streams[13] = INREG(OPAQUE_OVERLAY_CONTROL_REG);
   streams[14] = INREG(K1_VSCALE_REG);
   streams[15] = INREG(K2_VSCALE_REG);
   streams[16] = INREG(DDA_VERT_REG);
   streams[17] = INREG(STREAMS_FIFO_REG);
   streams[18] = INREG(PSTREAM_START_REG);
   streams[19] = INREG(PSTREAM_WINDOW_SIZE_REG);
   streams[20] = INREG(SSTREAM_START_REG);
   streams[21] = INREG(SSTREAM_WINDOW_SIZE_REG);
}

/* This function restores the saved STREAMS registers */

void s3ve_restoreSTREAMS(ScrnInfoPtr pScrn, unsigned int *streams)
{
  S3VPtr ps3v = S3VPTR(pScrn);

/* For now, set most regs to their default values for 24bpp
 * Restore only those that are needed for width/height/stride
 * Otherwise, we seem to get lockups because some registers
 * when saved have some reserved bits set.
 */

  OUTREG(PSTREAM_CONTROL_REG, streams[0] & 0x77000000);
  OUTREG(COL_CHROMA_KEY_CONTROL_REG, 0x00);
  OUTREG(SSTREAM_CONTROL_REG, 0x03000000);
  OUTREG(CHROMA_KEY_UPPER_BOUND_REG, 0x00);
  OUTREG(SSTREAM_STRETCH_REG, 0x00);
  OUTREG(BLEND_CONTROL_REG, 0x01000000);
  OUTREG(PSTREAM_FBADDR0_REG, 0x00);
  OUTREG(PSTREAM_FBADDR1_REG, 0x00);
  OUTREG(PSTREAM_STRIDE_REG, streams[8] & 0x0fff);
  OUTREG(DOUBLE_BUFFER_REG, 0x00);
  OUTREG(SSTREAM_FBADDR0_REG, 0x00);
  OUTREG(SSTREAM_FBADDR1_REG, 0x00);
  OUTREG(SSTREAM_STRIDE_REG, 0x01);
  OUTREG(OPAQUE_OVERLAY_CONTROL_REG, 0x40000000);
  OUTREG(K1_VSCALE_REG, 0x00);
  OUTREG(K2_VSCALE_REG, 0x00);
  OUTREG(DDA_VERT_REG, 0x00);
  OUTREG(PSTREAM_START_REG, 0x00010001);
  OUTREG(PSTREAM_WINDOW_SIZE_REG, streams[19] & 0x07ff07ff);
  OUTREG(SSTREAM_START_REG, 0x07ff07ff);
  OUTREG(SSTREAM_WINDOW_SIZE_REG, 0x00010001);
}
