/*
Copyright (C) 1994-2000 The XFree86 Project, Inc.  All Rights Reserved.

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

/* this file contains cumulative functions used for card register state
   manipulation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>

#include "s3v.h"
#include "s3v_pciids.h"

/*
 * This function is used to restore a video mode. It writes out all
 * of the standard VGA and extended S3 registers needed to setup a
 * video mode.
 *
 * Note that our life is made more difficult because of the STREAMS
 * processor which must be used for 24bpp. We need to disable STREAMS
 * before we switch video modes, or we risk locking up the machine.
 * We also have to follow a certain order when reenabling it.
 */
/* let's try restoring in the same order as in the 3.3.2.3 driver */
void s3ve_writeMode (ScrnInfoPtr pScrn, vgaRegPtr vgaSavePtr, S3VRegPtr restore)
{
  unsigned char tmp, cr3a=0, cr66, cr67;

  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

    PVERB5("	S3VWriteMode\n");

  vgaHWProtect(pScrn, TRUE);

   /* Are we going to re-enable STREAMS in this new mode? */
   ps3v->STREAMSRunning = restore->CR67 & 0x0c;

   /* First reset GE to make sure nothing is going on */
   if(ps3v->Chipset == S3_ViRGE_VX) {
      VGAOUT8(vgaCRIndex, 0x63);
      if(VGAIN8(vgaCRReg) & 0x01) s3ve_GEReset(pScrn,0,__LINE__,__FILE__);
      }
   else {
      VGAOUT8(vgaCRIndex, 0x66);
      if(VGAIN8(vgaCRReg) & 0x01) s3ve_GEReset(pScrn,0,__LINE__,__FILE__);
      }

   /* As per databook, always disable STREAMS before changing modes */
   VGAOUT8(vgaCRIndex, 0x67);
   cr67 = VGAIN8(vgaCRReg);
   if ((cr67 & 0x0c) == 0x0c) {
      s3ve_disableSTREAMS(pScrn);     /* If STREAMS was running, disable it */
      }

   /* Restore S3 extended regs */
   VGAOUT8(vgaCRIndex, 0x63);
   VGAOUT8(vgaCRReg, restore->CR63);
   VGAOUT8(vgaCRIndex, 0x66);
   VGAOUT8(vgaCRReg, restore->CR66);
   VGAOUT8(vgaCRIndex, 0x3a);
   VGAOUT8(vgaCRReg, restore->CR3A);
   VGAOUT8(vgaCRIndex, 0x31);
   VGAOUT8(vgaCRReg, restore->CR31);
   VGAOUT8(vgaCRIndex, 0x58);
   VGAOUT8(vgaCRReg, restore->CR58);
   VGAOUT8(vgaCRIndex, 0x55);
   VGAOUT8(vgaCRReg, restore->CR55);

   /* Extended mode timings registers */
   VGAOUT8(vgaCRIndex, 0x53);
   VGAOUT8(vgaCRReg, restore->CR53);
   VGAOUT8(vgaCRIndex, 0x5d);
   VGAOUT8(vgaCRReg, restore->CR5D);
   VGAOUT8(vgaCRIndex, 0x5e);
   VGAOUT8(vgaCRReg, restore->CR5E);
   VGAOUT8(vgaCRIndex, 0x3b);
   VGAOUT8(vgaCRReg, restore->CR3B);
   VGAOUT8(vgaCRIndex, 0x3c);
   VGAOUT8(vgaCRReg, restore->CR3C);
   VGAOUT8(vgaCRIndex, 0x43);
   VGAOUT8(vgaCRReg, restore->CR43);
   VGAOUT8(vgaCRIndex, 0x65);
   VGAOUT8(vgaCRReg, restore->CR65);
   VGAOUT8(vgaCRIndex, 0x6d);
   VGAOUT8(vgaCRReg, restore->CR6D);

   /* Restore the desired video mode with CR67 */

   VGAOUT8(vgaCRIndex, 0x67);
   cr67 = VGAIN8(vgaCRReg) & 0xf; /* Possible hardware bug on VX? */
   VGAOUT8(vgaCRReg, 0x50 | cr67);
   usleep(10000);
   VGAOUT8(vgaCRIndex, 0x67);
   VGAOUT8(vgaCRReg, restore->CR67 & ~0x0c); /* Don't enable STREAMS yet */

   /* Other mode timing and extended regs */
   VGAOUT8(vgaCRIndex, 0x34);
   VGAOUT8(vgaCRReg, restore->CR34);
   if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	/* S3_ViRGE_MX_SERIES(ps3v->Chipset) || CR40 reserved on MX */
	S3_ViRGE_MXP_SERIES(ps3v->Chipset) ||
	S3_ViRGE_VX_SERIES(ps3v->Chipset) ||
	/* S3_TRIO_3D_2X_SERIES(ps3v->Chipset) * included in GX2 series */
	ps3v->Chipset == S3_ViRGE_DXGX ||
	ps3v->Chipset == S3_ViRGE
	)
     {
       VGAOUT8(vgaCRIndex, 0x40);
       VGAOUT8(vgaCRReg, restore->CR40);
     }
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(vgaCRIndex, 0x41);
     VGAOUT8(vgaCRReg, restore->CR41);
   }
   VGAOUT8(vgaCRIndex, 0x42);
   VGAOUT8(vgaCRReg, restore->CR42);
   VGAOUT8(vgaCRIndex, 0x45);
   VGAOUT8(vgaCRReg, restore->CR45);
   VGAOUT8(vgaCRIndex, 0x51);
   VGAOUT8(vgaCRReg, restore->CR51);
   VGAOUT8(vgaCRIndex, 0x54);
   VGAOUT8(vgaCRReg, restore->CR54);

   /* Memory timings */
   VGAOUT8(vgaCRIndex, 0x36);
   VGAOUT8(vgaCRReg, restore->CR36);
   VGAOUT8(vgaCRIndex, 0x68);
   VGAOUT8(vgaCRReg, restore->CR68);
   VGAOUT8(vgaCRIndex, 0x69);
   VGAOUT8(vgaCRReg, restore->CR69);

   VGAOUT8(vgaCRIndex, 0x33);
   VGAOUT8(vgaCRReg, restore->CR33);
   if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) || S3_ViRGE_GX2_SERIES(ps3v->Chipset)
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
   {
      VGAOUT8(vgaCRIndex, 0x85);
      VGAOUT8(vgaCRReg, restore->CR85);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX) {
      VGAOUT8(vgaCRIndex, 0x86);
      VGAOUT8(vgaCRReg, restore->CR86);
   }
   if ( (ps3v->Chipset == S3_ViRGE_GX2) ||
	S3_ViRGE_MX_SERIES(ps3v->Chipset) ) {
      VGAOUT8(vgaCRIndex, 0x7B);
      VGAOUT8(vgaCRReg, restore->CR7B);
      VGAOUT8(vgaCRIndex, 0x7D);
      VGAOUT8(vgaCRReg, restore->CR7D);
      VGAOUT8(vgaCRIndex, 0x87);
      VGAOUT8(vgaCRReg, restore->CR87);
      VGAOUT8(vgaCRIndex, 0x92);
      VGAOUT8(vgaCRReg, restore->CR92);
      VGAOUT8(vgaCRIndex, 0x93);
      VGAOUT8(vgaCRReg, restore->CR93);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset) || S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      VGAOUT8(vgaCRIndex, 0x90);
      VGAOUT8(vgaCRReg, restore->CR90);
      VGAOUT8(vgaCRIndex, 0x91);
      VGAOUT8(vgaCRReg, restore->CR91);
   }

   /* Unlock extended sequencer regs */
   VGAOUT8(0x3c4, 0x08);
   VGAOUT8(0x3c5, 0x06);


   /* Restore extended sequencer regs for MCLK. SR10 == 255 indicates that
    * we should leave the default SR10 and SR11 values there.
    */

   if (restore->SR10 != 255) {
       VGAOUT8(0x3c4, 0x10);
       VGAOUT8(0x3c5, restore->SR10);
       VGAOUT8(0x3c4, 0x11);
       VGAOUT8(0x3c5, restore->SR11);
       }

   /* Restore extended sequencer regs for DCLK */
   VGAOUT8(0x3c4, 0x12);
   VGAOUT8(0x3c5, restore->SR12);
   VGAOUT8(0x3c4, 0x13);
   VGAOUT8(0x3c5, restore->SR13);
   if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x29);
     VGAOUT8(0x3c5, restore->SR29);
   }
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x54);
     VGAOUT8(0x3c5, restore->SR54);
     VGAOUT8(0x3c4, 0x55);
     VGAOUT8(0x3c5, restore->SR55);
     VGAOUT8(0x3c4, 0x56);
     VGAOUT8(0x3c5, restore->SR56);
     VGAOUT8(0x3c4, 0x57);
     VGAOUT8(0x3c5, restore->SR57);
   }

   VGAOUT8(0x3c4, 0x18);
   VGAOUT8(0x3c5, restore->SR18);

   /* Load new m,n PLL values for DCLK & MCLK */
   VGAOUT8(0x3c4, 0x15);
   tmp = VGAIN8(0x3c5) & ~0x21;

   /* databook either 0x3 or 0x20, but not both?? */
   VGAOUT8(0x3c5, tmp | 0x03);
   VGAOUT8(0x3c5, tmp | 0x23);
   VGAOUT8(0x3c5, tmp | 0x03);
   VGAOUT8(0x3c5, restore->SR15);
   if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x0a);
     VGAOUT8(0x3c5, restore->SR0A);
     VGAOUT8(0x3c4, 0x0f);
     VGAOUT8(0x3c5, restore->SR0F);
   }

   VGAOUT8(0x3c4, 0x08);
   VGAOUT8(0x3c5, restore->SR08);


   /* Now write out CR67 in full, possibly starting STREAMS */

   VerticalRetraceWait();
   VGAOUT8(vgaCRIndex, 0x67);
   VGAOUT8(vgaCRReg, 0x50);   /* For possible bug on VX?! */
   usleep(10000);
   VGAOUT8(vgaCRIndex, 0x67);
   VGAOUT8(vgaCRReg, restore->CR67);

   VGAOUT8(vgaCRIndex, 0x66);
   cr66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr66 | 0x80);
   VGAOUT8(vgaCRIndex, 0x3a);

   /* workaround cr3a corruption */
   if( ps3v->mx_cr3a_fix )
     {
       VGAOUT8(vgaCRReg, restore->CR3A | 0x80);
     }
   else
     {
       cr3a = VGAIN8(vgaCRReg);
       VGAOUT8(vgaCRReg, cr3a | 0x80);
     }

   /* And finally, we init the STREAMS processor if we have CR67 indicate 24bpp
    * We also restore FIFO and TIMEOUT memory controller registers. (later...)
    */

   if (ps3v->NeedSTREAMS) {
     if(ps3v->STREAMSRunning) s3ve_restoreSTREAMS(pScrn, restore->STREAMS);
      }

   /* Now, before we continue, check if this mode has the graphic engine ON
    * If yes, then we reset it.
    * This fixes some problems with corruption at 24bpp with STREAMS
    * Also restore the MIU registers.
    */

   if(ps3v->Chipset == S3_ViRGE_VX) {
      if(restore->CR63 & 0x01) s3ve_GEReset(pScrn,0,__LINE__,__FILE__);
      }
   else {
      if(restore->CR66 & 0x01) s3ve_GEReset(pScrn,0,__LINE__,__FILE__);
      }

   VerticalRetraceWait();
   if (S3_ViRGE_GX2_SERIES(ps3v->Chipset)
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
      VGAOUT8(vgaCRIndex, 0x85);
      /* primary stream threshold */
      VGAOUT8(vgaCRReg, 0x1f );
     }
   else
     {
       OUTREG(FIFO_CONTROL_REG, restore->MMPR0);
     }
   if( !( S3_ViRGE_GX2_SERIES(ps3v->Chipset)
	  /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) ))
   {
     WaitIdle();                  /* Don't ask... */
     OUTREG(MIU_CONTROL_REG, restore->MMPR1);
     WaitIdle();
     OUTREG(STREAMS_TIMEOUT_REG, restore->MMPR2);
     WaitIdle();
     OUTREG(MISC_TIMEOUT_REG, restore->MMPR3);
   }

   /* Restore the standard VGA registers */
   /* False indicates no fontinfo restore. */
   /* VGA_SR_MODE restores mode info only, no font, no colormap */
   					/* Do all for primary video */
   if (xf86IsPrimaryPci(ps3v->PciInfo))
     vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_ALL);
   					/* Mode only for non-primary? */
   else
     vgaHWRestore(pScrn, vgaSavePtr, VGA_SR_MODE);
 		/* moved from before vgaHWRestore, to prevent segfault? */
   VGAOUT8(vgaCRIndex, 0x66);
   VGAOUT8(vgaCRReg, cr66);
   VGAOUT8(vgaCRIndex, 0x3a);

   /* workaround cr3a corruption */
   if( ps3v->mx_cr3a_fix )
     VGAOUT8(vgaCRReg, restore->CR3A);
   else
     VGAOUT8(vgaCRReg, cr3a);

   if (xf86GetVerbosity() > 1) {
      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
         "ViRGE driver: done restoring mode, dumping CR registers:\n");
      s3ve_printRegs(pScrn);
   }

   vgaHWProtect(pScrn, FALSE);

   return;

}

Bool s3ve_modeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  int width, dclk;
  int i, j;

  		      		/* Store values to current mode register structs */
  S3VRegPtr new = &ps3v->ModeReg;
  vgaRegPtr vganew = &hwp->ModeReg;
  int vgaCRIndex, vgaCRReg, vgaIOBase;

  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

    PVERB5("	S3VModeInit\n");

    /* Set scale factors for mode timings */

    if (ps3v->Chipset == S3_ViRGE_VX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      ps3v->HorizScaleFactor = 1;
    }
    else if (pScrn->bitsPerPixel == 8) {
      ps3v->HorizScaleFactor = 1;
    }
    else if (pScrn->bitsPerPixel == 16) {
      if (S3_TRIO_3D_SERIES(ps3v->Chipset) && mode->Clock > 115000)
	ps3v->HorizScaleFactor = 1;
      else
	ps3v->HorizScaleFactor = 2;
    }
    else {
      ps3v->HorizScaleFactor = 1;
    }


   /* First we adjust the horizontal timings if needed */

   if(ps3v->HorizScaleFactor != 1)
      if (!mode->CrtcHAdjusted) {
             mode->CrtcHDisplay *= ps3v->HorizScaleFactor;
             mode->CrtcHSyncStart *= ps3v->HorizScaleFactor;
             mode->CrtcHSyncEnd *= ps3v->HorizScaleFactor;
             mode->CrtcHTotal *= ps3v->HorizScaleFactor;
             mode->CrtcHSkew *= ps3v->HorizScaleFactor;
             mode->CrtcHAdjusted = TRUE;
             }

   if(!vgaHWInit (pScrn, mode))
      return FALSE;

   /* Now we fill in the rest of the stuff we need for the virge */
   /* Start with MMIO, linear addr. regs */

   VGAOUT8(vgaCRIndex, 0x3a);
   {
       unsigned char tmp = VGAIN8(vgaCRReg);
       if (S3_ViRGE_GX2_SERIES(ps3v->Chipset)
           /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
       {
           if (ps3v->pci_burst)
               /*new->CR3A = (tmp & 0x38) | 0x10; / ENH 256, PCI burst */
               /* Don't clear reserved bits... */
               new->CR3A = (tmp & 0x7f) | 0x10; /* ENH 256, PCI burst */
           else
               new->CR3A = tmp | 0x90;      /* ENH 256, no PCI burst! */
       }
       else
       {
           if (ps3v->pci_burst)
               new->CR3A = (tmp & 0x7f) | 0x15; /* ENH 256, PCI burst */
           else
               new->CR3A = tmp | 0x95;      /* ENH 256, no PCI burst! */
       }
   }


   VGAOUT8(vgaCRIndex, 0x55);
   new->CR55 = VGAIN8(vgaCRReg);
   if (ps3v->hwcursor)
     new->CR55 |= 0x10;  /* Enables X11 hw cursor mode */
   if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     new->CR31 = 0x0c;               /* [trio3d] page 54 */
   } else {
     new->CR53 = 0x08;     /* Enables MMIO */
     new->CR31 = 0x8c;     /* Dis. 64k window, en. ENH maps */
   }

   /* Enables S3D graphic engine and PCI disconnects */
   if(ps3v->Chipset == S3_ViRGE_VX){
      new->CR66 = 0x90;
      new->CR63 = 0x09;
      }
   else {
     new->CR66 = 0x89;
     /* Set display fifo */
     if( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	 S3_ViRGE_MX_SERIES(ps3v->Chipset) )
       {
	 /* Changed from 0x08 based on reports that this */
	 /* prevents MX from running properly below 1024x768 */
	 new->CR63 = 0x10;
       }
     else
       {
	 new->CR63 = 0;
       }
      }

  /* Now set linear addr. registers */
  /* LAW size: we have 2 cases, 2MB, 4MB or >= 4MB for VX */
   VGAOUT8(vgaCRIndex, 0x58);
   new->CR58 = VGAIN8(vgaCRReg) & 0x80;
   if(pScrn->videoRam == 2048){
      new->CR58 |= 0x02 | 0x10;
      }
   else if (pScrn->videoRam == 1024) {
      new->CR58 |= 0x01 | 0x10;
   }
   else {
     if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) && pScrn->videoRam == 8192)
       new->CR58 |= 0x07 | 0x10; /* 8MB window on Trio3D/2X */
     else
       new->CR58 |= 0x03 | 0x10; /* 4MB window on virge, 8MB on VX */
      }
   if(ps3v->Chipset == S3_ViRGE_VX)
      new->CR58 |= 0x40;
   if (ps3v->early_ras_precharge)
      new->CR58 |= 0x80;
   if (ps3v->late_ras_precharge)
      new->CR58 &= 0x7f;

  /* ** On PCI bus, no need to reprogram the linear window base address */

  /* Now do clock PLL programming. Use the s3gendac function to get m,n */
  /* Also determine if we need doubling etc. */

   dclk = mode->Clock;
   new->CR67 = 0x00;             /* Defaults */

   if (!S3_TRIO_3D_SERIES(ps3v->Chipset))
     new->SR15 = 0x03 | 0x80;
   else {
     VGAOUT8(0x3c4, 0x15);
     new->SR15 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x0a);
     new->SR0A = VGAIN8(0x3c5);
     if (ps3v->slow_dram) {
       new->SR15 = 0x03;  /* 3 CYC MWR */
       new->SR0A &= 0x7F;
     } else if (ps3v->fast_dram) {
       new->SR15 = 0x03 | 0x80; /* 2 CYC MWR */
       new->SR0A |= 0x80;
     } else { /* keep BIOS init defaults */
       new->SR15 = (new->SR15 & 0x80) | 0x03;
     }
   }
   new->SR18 = 0x00;
   new->CR43 = 0x00;
   new->CR45 = 0x00;
   				/* Enable MMIO to RAMDAC registers */
   new->CR65 = 0x00;		/* CR65_2 must be zero, doc seems to be wrong */
   new->CR54 = 0x00;

   if ( S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
	/* S3_ViRGE_MX_SERIES(ps3v->Chipset) || CR40 reserved on MX */
	S3_ViRGE_MXP_SERIES(ps3v->Chipset) ||
	S3_ViRGE_VX_SERIES(ps3v->Chipset) ||
	/* S3_TRIO_3D_2X_SERIES(ps3v->Chipset) * included in GX2 series */
	ps3v->Chipset == S3_ViRGE_DXGX ||
	ps3v->Chipset == S3_ViRGE
	) {
     VGAOUT8(vgaCRIndex, 0x40);
     new->CR40 = VGAIN8(vgaCRReg) & ~0x01;
   }

   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     /* fix problems with APM suspend/resume trashing CR90/91 */
     switch(pScrn->bitsPerPixel) {
       case  8: new->CR41 = 0x38; break;
       case 15: new->CR41 = 0x58; break;
       case 16: new->CR41 = 0x48; break;
       default: new->CR41 = 0x77;
     }
   }

    xf86ErrorFVerb(VERBLEV, "	S3VModeInit dclk=%i \n",
   	dclk
	);

   /* Memory controller registers. Optimize for better graphics engine
    * performance. These settings are adjusted/overridden below for other bpp/
    * XConfig options.The idea here is to give a longer number of contiguous
    * MCLK's to both refresh and the graphics engine, to diminish the
    * relative penalty of 3 or 4 mclk's needed to setup memory transfers.
    */
   new->MMPR0 = 0x010400; /* defaults */
   new->MMPR1 = 0x00;
   new->MMPR2 = 0x0808;
   new->MMPR3 = 0x08080810;

   /*
    * These settings look like they ought to be better adjusted for depth,
    * so for problem modes running without any fifo_ option should be
    * usable.  Note that these adjust some memory timings and relate to
    * the boards MCLK setting.
    * */
    if( ps3v->fifo_aggressive || ps3v->fifo_moderate ||
       ps3v->fifo_conservative ) {

         new->MMPR1 = 0x0200;   /* Low P. stream waits before filling */
         new->MMPR2 = 0x1808;   /* Let the FIFO refill itself */
         new->MMPR3 = 0x08081810; /* And let the GE hold the bus for a while */
      }

   /* And setup here the new value for MCLK. We use the XConfig
    * option "set_mclk", whose value gets stored in ps3v->MCLK.
    * I'm not sure what the maximum "permitted" value should be, probably
    * 100 MHz is more than enough for now.
    */

   if(ps3v->MCLK> 0) {
       if (S3_ViRGE_MX_SERIES(ps3v->Chipset))
	  S3VCommonCalcClock(pScrn, mode,
			     (int)(ps3v->MCLK / ps3v->refclk_fact),
			     1, 1, 31, 0, 3,
			     135000, 270000, &new->SR11, &new->SR10);
       else
	  S3VCommonCalcClock(pScrn, mode, ps3v->MCLK, 1, 1, 31, 0, 3,
			     135000, 270000, &new->SR11, &new->SR10);
       }
   else {
       new->SR10 = 255; /* This is a reserved value, so we use as flag */
       new->SR11 = 255;
       }

   					/* most modes don't need STREAMS */
					/* processor, preset FALSE */
   /* support for XVideo needs streams, so added it to some modes */
   ps3v->NeedSTREAMS = FALSE;

   if(ps3v->Chipset == S3_ViRGE_VX){
       if (pScrn->bitsPerPixel == 8) {
          if (dclk <= 110000) new->CR67 = 0x00; /* 8bpp, 135MHz */
          else new->CR67 = 0x10;                /* 8bpp, 220MHz */
          }
       else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
          if (dclk <= 110000) new->CR67 = 0x20; /* 15bpp, 135MHz */
          else new->CR67 = 0x30;                /* 15bpp, 220MHz */
          }
       else if (pScrn->bitsPerPixel == 16) {
          if (dclk <= 110000) new->CR67 = 0x40; /* 16bpp, 135MHz */
          else new->CR67 = 0x50;                /* 16bpp, 220MHz */
          }
       else if ((pScrn->bitsPerPixel == 24) || (pScrn->bitsPerPixel == 32)) {
          new->CR67 = 0xd0 | 0x0c;              /* 24bpp, 135MHz, STREAMS */
	  					/* Flag STREAMS proc. required */
          ps3v->NeedSTREAMS = TRUE;
          s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
          new->MMPR0 = 0xc098;            /* Adjust FIFO slots */
          }
       S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 4,
	   220000, 440000, &new->SR13, &new->SR12);

      } /* end VX if() */
   else if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
       if (pScrn->bitsPerPixel == 8)
	  new->CR67 = 0x00;
       else if (pScrn->bitsPerPixel == 16) {
	 /* XV support needs STREAMS in depth 16 */
          ps3v->NeedSTREAMS = TRUE;
          s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
	  if (pScrn->weight.green == 5)
	     new->CR67 = 0x30 | 0x4;                  /* 15bpp */
	  else
	     new->CR67 = 0x50 | 0x4;                  /* 16bpp */
          }
       else if (pScrn->bitsPerPixel == 24) {
	 new->CR67 = 0x74;              /* 24bpp, STREAMS */
	  					/* Flag STREAMS proc. required */
          ps3v->NeedSTREAMS = TRUE;
          s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
          }
       else if (pScrn->bitsPerPixel == 32) {
          new->CR67 = 0xd0;              /* 32bpp */
	  	/* Missing STREAMs and other stuff here? KJB */
          /* new->MMPR0 = 0xc098;            / Adjust FIFO slots */
          }
       {
         unsigned char ndiv;
	 if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
	   unsigned char sr8;
	   VGAOUT8(0x3c4, 0x08);  /* unlock extended SEQ regs */
	   sr8 = VGAIN8(0x3c5);
	   VGAOUT8(0x3c5, 0x06);
	   VGAOUT8(0x3c4, 0x31);
	   if (VGAIN8(0x3c5) & 0x10) { /* LCD on */
	     if (!ps3v->LCDClk) {  /* entered only once for first mode */
	       int h_lcd, v_lcd;
	       VGAOUT8(0x3c4, 0x61);
	       h_lcd = VGAIN8(0x3c5);
	       VGAOUT8(0x3c4, 0x66);
	       h_lcd |= ((VGAIN8(0x3c5) & 0x02) << 7);
	       h_lcd = (h_lcd+1) * 8;
	       VGAOUT8(0x3c4, 0x69);
	       v_lcd = VGAIN8(0x3c5);
	       VGAOUT8(0x3c4, 0x6e);
	       v_lcd |= ((VGAIN8(0x3c5) & 0x70) << 4);
	       v_lcd++;

	       /* check if first mode has physical LCD resolution */
	       if (pScrn->modes->HDisplay == h_lcd && pScrn->modes->VDisplay == v_lcd)
		 ps3v->LCDClk = mode->Clock;
	       else {
		 int n1, n2, sr12, sr13, sr29;
		 VGAOUT8(0x3c4, 0x12);
		 sr12 = VGAIN8(0x3c5);
		 VGAOUT8(0x3c4, 0x13);
		 sr13 = VGAIN8(0x3c5) & 0x7f;
		 VGAOUT8(0x3c4, 0x29);
		 sr29 = VGAIN8(0x3c5);
		 n1 = sr12 & 0x1f;
		 n2 = ((sr12>>6) & 0x03) | ((sr29 & 0x01) << 2);
		 ps3v->LCDClk = ((int)(ps3v->refclk_fact * 1431818 * (sr13+2)) / (n1+2) / (1 << n2) + 50) / 100;
	       }
	     }
	     S3VCommonCalcClock(pScrn, mode,
			     (int)(ps3v->LCDClk / ps3v->refclk_fact),
			     1, 1, 31, 0, 4,
			     170000, 340000, &new->SR13, &ndiv);
	   }
	   else
	     S3VCommonCalcClock(pScrn, mode,
			     (int)(dclk / ps3v->refclk_fact),
			     1, 1, 31, 0, 4,
			     170000, 340000, &new->SR13, &ndiv);
	   VGAOUT8(0x3c4, 0x08);
	   VGAOUT8(0x3c5, sr8);
	 }
	 else  /* S3_ViRGE_GX2 */
	   S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 4,
			   170000, 340000, &new->SR13, &ndiv);
         new->SR29 = ndiv >> 7;
         new->SR12 = (ndiv & 0x1f) | ((ndiv & 0x60) << 1);
       }
   } /* end GX2 or MX if() */
   else if(S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      new->SR0F = 0x00;
      if (pScrn->bitsPerPixel == 8) {
         if(dclk > 115000) {                     /* We need pixmux */
            new->CR67 = 0x10;
            new->SR15 |= 0x10;                   /* Set DCLK/2 bit */
            new->SR18 = 0x80;                   /* Enable pixmux */
        }
      }
      else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
        if(dclk > 115000) {
           new->CR67 = 0x20;
           new->SR15 |= 0x10;
           new->SR18 = 0x80;
	   new->SR0F = 0x10;
        } else {
           new->CR67 = 0x30;                       /* 15bpp */
        }
      }
      else if (pScrn->bitsPerPixel == 16) {
        if(dclk > 115000) {
            new->CR67 = 0x40;
            new->SR15 |= 0x10;
            new->SR18 = 0x80;
	    new->SR0F = 0x10;
        } else {
           new->CR67 = 0x50;
        }
      }
      else if (pScrn->bitsPerPixel == 24) {
         new->CR67 = 0xd0 | 0x0c;
	 ps3v->NeedSTREAMS = TRUE;
         s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
      }
      else if (pScrn->bitsPerPixel == 32) {
         new->CR67 = 0xd0 | 0x0c;
	 ps3v->NeedSTREAMS = TRUE;
         s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0x10000;            /* Still more FIFO slots */
	 new->SR0F = 0x10;
      }
      S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 4,
                     230000, 460000, &new->SR13, &new->SR12);
   } /* end TRIO_3D if() */
   else if(ps3v->Chipset == S3_ViRGE_DXGX) {
      if (pScrn->bitsPerPixel == 8) {
         if(dclk > 80000) {                     /* We need pixmux */
            new->CR67 = 0x10;
            new->SR15 |= 0x10;                   /* Set DCLK/2 bit */
            new->SR18 = 0x80;                   /* Enable pixmux */
            }
         }
      else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
         new->CR67 = 0x30;                       /* 15bpp */
         }
      else if (pScrn->bitsPerPixel == 16) {
	if(mode->Flags & V_DBLSCAN)
	  {
	    new->CR67 = 0x50;
	  }
	else
	  {
	    new->CR67 = 0x50 | 0x0c;
	    /* Flag STREAMS proc. required */
	    /* XV support needs STREAMS in depth 16 */
	    ps3v->NeedSTREAMS = TRUE;
	    s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
	  }
	 if( ps3v->XVideo )
	   {
	     new->MMPR0 = 0x107c02;            /* Adjust FIFO slots, overlay */
	   }
	 else
	   {
	     new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
	   }
         }
      else if (pScrn->bitsPerPixel == 24) {
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
	 if( ps3v->XVideo )
	   {
	     new->MMPR0 = 0x107c02;            /* Adjust FIFO slots, overlay */
	   }
	 else
	   {
	     new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
	   }
         }
      else if (pScrn->bitsPerPixel == 32) {
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0x10000;            /* Still more FIFO slots */
         }
      S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 3,
	135000, 270000, &new->SR13, &new->SR12);
   } /* end DXGX if() */
   else {           /* Everything else ... (only ViRGE) */
      if (pScrn->bitsPerPixel == 8) {
         if(dclk > 80000) {                     /* We need pixmux */
            new->CR67 = 0x10;
            new->SR15 |= 0x10;                   /* Set DCLK/2 bit */
            new->SR18 = 0x80;                   /* Enable pixmux */
            }
         }
      else if ((pScrn->bitsPerPixel == 16) && (pScrn->weight.green == 5)) {
         new->CR67 = 0x30;                       /* 15bpp */
         }
      else if (pScrn->bitsPerPixel == 16) {
         new->CR67 = 0x50;
         }
      else if (pScrn->bitsPerPixel == 24) {
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
	 new->MMPR0 = 0xc000;            /* Adjust FIFO slots */
         }
      else if (pScrn->bitsPerPixel == 32) {
         new->CR67 = 0xd0 | 0x0c;
	  					/* Flag STREAMS proc. required */
         ps3v->NeedSTREAMS = TRUE;
         s3ve_initSTREAMS(pScrn, new->STREAMS, mode);
         new->MMPR0 = 0x10000;            /* Still more FIFO slots */
         }
      S3VCommonCalcClock(pScrn, mode, dclk, 1, 1, 31, 0, 3,
	135000, 270000, &new->SR13, &new->SR12);
      } /* end great big if()... */


   /* Now adjust the value of the FIFO based upon options specified */
   if( ps3v->fifo_moderate ) {
      if(pScrn->bitsPerPixel < 24)
         new->MMPR0 -= 0x8000;
      else
         new->MMPR0 -= 0x4000;
      }
   else if( ps3v->fifo_aggressive ) {
      if(pScrn->bitsPerPixel < 24)
         new->MMPR0 -= 0xc000;
      else
         new->MMPR0 -= 0x6000;
      }

   /* If we have an interlace mode, set the interlace bit. Note that mode
    * vertical timings are already adjusted by the standard VGA code
    */
   if(mode->Flags & V_INTERLACE) {
        new->CR42 = 0x20; /* Set interlace mode */
        }
   else {
        new->CR42 = 0x00;
        }

   if(S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
      S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
       new->CR34 = 0;
     }
   else
     {
       /* Set display fifo */
       new->CR34 = 0x10;
     }
   /* Now we adjust registers for extended mode timings */
   /* This is taken without change from the accel/s3_virge code */

   i = ((((mode->CrtcHTotal >> 3) - 5) & 0x100) >> 8) |
       ((((mode->CrtcHDisplay >> 3) - 1) & 0x100) >> 7) |
       ((((mode->CrtcHSyncStart >> 3) - 1) & 0x100) >> 6) |
       ((mode->CrtcHSyncStart & 0x800) >> 7);

   if ((mode->CrtcHSyncEnd >> 3) - (mode->CrtcHSyncStart >> 3) > 64)
      i |= 0x08;   /* add another 64 DCLKs to blank pulse width */

   if ((mode->CrtcHSyncEnd >> 3) - (mode->CrtcHSyncStart >> 3) > 32)
      i |= 0x20;   /* add another 32 DCLKs to hsync pulse width */

   /* video playback chokes if sync start and display end are equal */
   if (mode->CrtcHSyncStart - mode->CrtcHDisplay < ps3v->HorizScaleFactor) {
       int tmp = vganew->CRTC[4] + ((i&0x10)<<4) + ps3v->HorizScaleFactor;
       vganew->CRTC[4] = tmp & 0xff;
       i |= ((tmp >> 4) & 0x10);
   }

   j = (  vganew->CRTC[0] + ((i&0x01)<<8)
        + vganew->CRTC[4] + ((i&0x10)<<4) + 1) / 2;

   if (j-(vganew->CRTC[4] + ((i&0x10)<<4)) < 4) {
      if (vganew->CRTC[4] + ((i&0x10)<<4) + 4 <= vganew->CRTC[0]+ ((i&0x01)<<8))
         j = vganew->CRTC[4] + ((i&0x10)<<4) + 4;
      else
         j = vganew->CRTC[0]+ ((i&0x01)<<8) + 1;
   }
   new->CR3B = j & 0xFF;
   i |= (j & 0x100) >> 2;
   new->CR3C = (vganew->CRTC[0] + ((i&0x01)<<8))/2;
   new->CR5D = i;

   new->CR5E = (((mode->CrtcVTotal - 2) & 0x400) >> 10)  |
               (((mode->CrtcVDisplay - 1) & 0x400) >> 9) |
               (((mode->CrtcVSyncStart) & 0x400) >> 8)   |
               (((mode->CrtcVSyncStart) & 0x400) >> 6)   | 0x40;


   width = (pScrn->displayWidth * (pScrn->bitsPerPixel / 8))>> 3;
   vganew->CRTC[19] = 0xFF & width;
   new->CR51 = (0x300 & width) >> 4; /* Extension bits */

   /* Set doublescan */
   if( mode->Flags & V_DBLSCAN)
     vganew->CRTC[9] |= 0x80;

   /* And finally, select clock source 2 for programmable PLL */
   vganew->MiscOutReg |= 0x0c;


   new->CR33 = 0x20;
   if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) || S3_ViRGE_GX2_SERIES(ps3v->Chipset)
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
   {
     new->CR85 = 0x12;  /* avoid sreen flickering */
      /* by increasing FIFO filling, larger # fills FIFO from memory earlier */
      /* on GX2 this affects all depths, not just those running STREAMS. */
      /* new, secondary stream settings. */
      new->CR87 = 0x10;
      /* gx2 - set up in XV init code */
      new->CR92 = 0x00;
      new->CR93 = 0x00;
      /* gx2 primary mclk timeout, def=0xb */
      new->CR7B = 0xb;
      /* gx2 secondary mclk timeout, def=0xb */
      new->CR7D = 0xb;
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      new->CR86 = 0x80;  /* disable DAC power saving to avoid bright left edge */
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset) || S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      int dbytes = pScrn->displayWidth * ((pScrn->bitsPerPixel+7)/8);
      new->CR91 =   (dbytes + 7) / 8;
      new->CR90 = (((dbytes + 7) / 8) >> 8) | 0x80;
   }


   /* Now we handle various XConfig memory options and others */

   VGAOUT8(vgaCRIndex, 0x36);
   new->CR36 = VGAIN8(vgaCRReg);
   /* option "slow_edodram" sets EDO to 2 cycle mode on ViRGE */
   if (ps3v->Chipset == S3_ViRGE) {
      if( ps3v->slow_edodram )
         new->CR36 = (new->CR36 & 0xf3) | 0x08;
      else
         new->CR36 &= 0xf3;
      }

   /* Option "fpm_vram" for ViRGE_VX sets memory in fast page mode */
   if (ps3v->Chipset == S3_ViRGE_VX) {
      if( ps3v->fpm_vram )
         new->CR36 |=  0x0c;
      else
         new->CR36 &= ~0x0c;
   }

   				/* S3_INVERT_VCLK was defaulted to 0 	*/
				/* in 3.3.3 and never changed. 		*/
				/* Also, bit 0 is never set in 3.9Nm,	*/
				/* so I left this out for 4.0.			*/
#if 0
      if (mode->Private[0] & (1 << S3_INVERT_VCLK)) {
	 if (mode->Private[S3_INVERT_VCLK])
	    new->CR67 |= 1;
	 else
	    new->CR67 &= ~1;
      }
#endif
      				/* S3_BLANK_DELAY settings based on 	*/
				/* defaults only. From 3.3.3 		*/
   {
      int blank_delay;

      if(ps3v->Chipset == S3_ViRGE_VX)
	    /* these values need to be changed once CR67_1 is set
	       for gamma correction (see S3V server) ! */
	    if (pScrn->bitsPerPixel == 8)
	       blank_delay = 0x00;
	    else if (pScrn->bitsPerPixel == 16)
	       blank_delay = 0x00;
	    else
	       blank_delay = 0x51;
      else
	    if (pScrn->bitsPerPixel == 8)
	       blank_delay = 0x00;
	    else if (pScrn->bitsPerPixel == 16)
	       blank_delay = 0x02;
	    else
	       blank_delay = 0x04;

      if (ps3v->Chipset == S3_ViRGE_VX)
	    new->CR6D = blank_delay;
      else {
	    new->CR65 = (new->CR65 & ~0x38)
	       | (blank_delay & 0x07) << 3;
	    VGAOUT8(vgaCRIndex, 0x6d);
	    new->CR6D = VGAIN8(vgaCRReg);
      }
   }
   				/* S3_EARLY_SC was defaulted to 0 	*/
				/* in 3.3.3 and never changed. 		*/
				/* Also, bit 1 is never set in 3.9Nm,	*/
				/* so I left this out for 4.0.			*/
#if 0
      if (mode->Private[0] & (1 << S3_EARLY_SC)) {
	 if (mode->Private[S3_EARLY_SC])
	    new->CR65 |= 2;
	 else
	    new->CR65 &= ~2;
      }
#endif

   VGAOUT8(vgaCRIndex, 0x68);
   new->CR68 = VGAIN8(vgaCRReg);
   new->CR69 = 0;

   /* Flat panel centering and expansion registers */
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset) && (ps3v->lcd_center)) {
     new->SR54 = 0x10 ;
     new->SR55 = 0x80 ;
     new->SR56 = 0x10 ;
     new->SR57 = 0x80 ;
   } else {
     new->SR54 = 0x1f ;
     new->SR55 = 0x9f ;
     new->SR56 = 0x1f ;
     new->SR57 = 0xff ;
   }

   pScrn->vtSema = TRUE;

   					/* Do it!  Write the mode registers */
					/* to hardware, start STREAMS if    */
					/* needed, etc.		    	    */
   s3ve_writeMode( pScrn, vganew, new );
   					/* Adjust the viewport */
   S3VAdjustFrame(pScrn, pScrn->frameX0, pScrn->frameY0);

   return TRUE;
}


/* Checks if a mode is suitable for the selected chipset. */

ModeStatus s3ve_validMode(ScrnInfoPtr pScrn, DisplayModePtr mode,
                          Bool verbose, int flags)
{
    if ((pScrn->bitsPerPixel + 7)/8 * mode->HDisplay > 4095)
	return MODE_VIRTUAL_X;

    /* todo -  The virge limit is 2048 vertical & horizontal */
    /* pixels, not clock register settings. */
				/* true for all ViRGE? */
    if (mode->HTotal > 2048)
        return MODE_BAD_HVALUE;

    if (mode->VTotal > 2048)
        return MODE_BAD_VVALUE;

    return MODE_OK;
}

void s3ve_loadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
                    LOCO *colors, VisualPtr pVisual)
{
    S3VPtr ps3v = S3VPTR(pScrn);
    int i, index;

    for(i = 0; i < numColors; i++) {
	index = indices[i];
        VGAOUT8(0x3c8, index);
        VGAOUT8(0x3c9, colors[index].red);
        VGAOUT8(0x3c9, colors[index].green);
        VGAOUT8(0x3c9, colors[index].blue);
    }
}

/* This function is used to debug, it prints out the contents of s3 regs */

void s3ve_printRegs(ScrnInfoPtr pScrn)
{
    unsigned char tmp1, tmp2;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    S3VPtr ps3v = S3VPTR(pScrn);
    int vgaCRIndex, vgaCRReg, vgaIOBase, vgaIR;
    vgaIOBase = hwp->IOBase;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;
    vgaIR = vgaIOBase + 0xa;

/* All registers */
/* New formatted registers, matches s3rc (sort of) */
    xf86DrvMsgVerb( pScrn->scrnIndex, X_INFO, VERBLEV, "START register dump ------------------\n");
    xf86ErrorFVerb(VERBLEV, "Misc Out[3CC]\n  ");
    xf86ErrorFVerb(VERBLEV, "%02x\n",VGAIN8(0x3cc));

    xf86ErrorFVerb(VERBLEV, "\nCR[00-2f]\n  ");
    for(tmp1=0x0;tmp1<=0x2f;tmp1++){
	VGAOUT8(vgaCRIndex, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(vgaCRReg));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }

    xf86ErrorFVerb(VERBLEV, "\nSR[00-27]\n  ");
    for(tmp1=0x0;tmp1<=0x27;tmp1++){
	VGAOUT8(0x3c4, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(0x3c5));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }
    xf86ErrorFVerb(VERBLEV, "\n"); /* odd hex number of digits... */

    xf86ErrorFVerb(VERBLEV, "\nGr Cont GR[00-0f]\n  ");
    for(tmp1=0x0;tmp1<=0x0f;tmp1++){
	VGAOUT8(0x3ce, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(0x3cf));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }

    xf86ErrorFVerb(VERBLEV, "\nAtt Cont AR[00-1f]\n  ");
    VGAIN8(vgaIR); /* preset AR flip-flop by reading 3DA, ignore return value */
    tmp2=VGAIN8(0x3c0) & 0x20;
    for(tmp1=0x0;tmp1<=0x1f;tmp1++){
    VGAIN8(vgaIR); /* preset AR flip-flop by reading 3DA, ignore return value */
	VGAOUT8(0x3c0, (tmp1 & ~0x20) | tmp2);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(0x3c1));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }

    xf86ErrorFVerb(VERBLEV, "\nCR[30-6f]\n  ");
    for(tmp1=0x30;tmp1<=0x6f;tmp1++){
	VGAOUT8(vgaCRIndex, tmp1);
	xf86ErrorFVerb(VERBLEV, "%02x ",VGAIN8(vgaCRReg));
	if((tmp1 & 0x3) == 0x3) xf86ErrorFVerb(VERBLEV, " ");
	if((tmp1 & 0xf) == 0xf) xf86ErrorFVerb(VERBLEV, "\n  ");
    }

    xf86ErrorFVerb(VERBLEV, "\n");
    xf86DrvMsgVerb( pScrn->scrnIndex, X_INFO, VERBLEV, "END register dump --------------------\n");
}
