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

/* This file is reponsible for parsing xorg.conf options. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "s3v.h"

typedef enum {
   OPTION_SLOW_EDODRAM,
   OPTION_SLOW_DRAM,
   OPTION_FAST_DRAM,
   OPTION_FPM_VRAM,
   OPTION_PCI_BURST,
   OPTION_FIFO_CONSERV,
   OPTION_FIFO_MODERATE,
   OPTION_FIFO_AGGRESSIVE,
   OPTION_PCI_RETRY,
   OPTION_NOACCEL,
   OPTION_EARLY_RAS_PRECHARGE,
   OPTION_LATE_RAS_PRECHARGE,
   OPTION_LCD_CENTER,
   OPTION_LCDCLOCK,
   OPTION_MCLK,
   OPTION_REFCLK,
   OPTION_SHOWCACHE,
   OPTION_SWCURSOR,
   OPTION_HWCURSOR,
   OPTION_SHADOW_FB,
   OPTION_ROTATE,
   OPTION_FB_DRAW,
   OPTION_MX_CR3A_FIX,
   OPTION_XVIDEO
} S3VOpts;

static const OptionInfoRec s3ve_options[] =
{
  /*    int token, const char* name, OptionValueType type,
	ValueUnion value, Bool found.
  */
   { OPTION_SLOW_EDODRAM, 	"slow_edodram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_SLOW_DRAM, 		"slow_dram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_FAST_DRAM, 		"fast_dram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_FPM_VRAM, 		"fpm_vram",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_PCI_BURST, 		"pci_burst",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_FIFO_CONSERV, 	"fifo_conservative", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_FIFO_MODERATE, 	"fifo_moderate", OPTV_BOOLEAN, 	{0}, FALSE },
   { OPTION_FIFO_AGGRESSIVE, 	"fifo_aggressive", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_PCI_RETRY, 		"pci_retry",	OPTV_BOOLEAN,	{0}, FALSE  },
   { OPTION_NOACCEL, 		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE  },
   { OPTION_EARLY_RAS_PRECHARGE, "early_ras_precharge",	OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_LATE_RAS_PRECHARGE, "late_ras_precharge", OPTV_BOOLEAN, {0}, FALSE },
   { OPTION_LCD_CENTER, 	"lcd_center", 	OPTV_BOOLEAN, 	{0}, FALSE },
   { OPTION_LCDCLOCK, 		"set_lcdclk", 	OPTV_INTEGER, 	{0}, FALSE },
   { OPTION_MCLK, 		"set_mclk", 	OPTV_FREQ, 	{0}, FALSE },
   { OPTION_REFCLK, 		"set_refclk", 	OPTV_FREQ, 	{0}, FALSE },
   { OPTION_SHOWCACHE,		"show_cache",   OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_HWCURSOR,		"HWCursor",     OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_SWCURSOR,		"SWCursor",     OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_SHADOW_FB,          "ShadowFB",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_ROTATE, 	        "Rotate",	OPTV_ANYSTR,	{0}, FALSE },
   { OPTION_MX_CR3A_FIX,        "mxcr3afix",	OPTV_BOOLEAN,	{0}, FALSE },
   { OPTION_XVIDEO,             "XVideo",	OPTV_BOOLEAN,	{0}, FALSE },
   {-1, NULL, OPTV_NONE,	{0}, FALSE}
};

const OptionInfoRec *s3ve_availableOptions(int chipid, int busid)
{
    return s3ve_options;
}

Bool s3ve_parseOptions(ScrnInfoPtr pScrn, S3VPtr ps3v)
{
  double realFreq; 
  MessageType logMsgType;
  const char *rotateOptValue;
  OptionInfoPtr options;
  
  if (!(options = malloc(sizeof(s3ve_options))))
	return FALSE;
    memcpy(options, s3ve_options, sizeof(s3ve_options));

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, options);

    if (xf86ReturnOptValBool(options, OPTION_PCI_BURST, FALSE)) {
	ps3v->pci_burst = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_burst - PCI burst read enabled\n");
    } else
      ps3v->pci_burst = FALSE;
					/* default */
    ps3v->NoPCIRetry = 1;
   					/* Set option */
    if (xf86ReturnOptValBool(options, OPTION_PCI_RETRY, FALSE)) {
      if (xf86ReturnOptValBool(options, OPTION_PCI_BURST, FALSE)) {
      	ps3v->NoPCIRetry = 0;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: pci_retry\n");
	}
      else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		"\"pci_retry\" option requires \"pci_burst\".\n");
	}
    }
    if (xf86IsOptionSet(options, OPTION_FIFO_CONSERV)) {
	ps3v->fifo_conservative = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_conservative set\n");
    } else
   	ps3v->fifo_conservative = FALSE;

    if (xf86IsOptionSet(options, OPTION_FIFO_MODERATE)) {
	ps3v->fifo_moderate = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_moderate set\n");
    } else
   	ps3v->fifo_moderate = FALSE;

    if (xf86IsOptionSet(options, OPTION_FIFO_AGGRESSIVE)) {
	ps3v->fifo_aggressive = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fifo_aggressive set\n");
    } else
   	ps3v->fifo_aggressive = FALSE;

    if (xf86IsOptionSet(options, OPTION_SLOW_EDODRAM)) {
	ps3v->slow_edodram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: slow_edodram set\n");
    } else
   	ps3v->slow_edodram = FALSE;

    if (xf86IsOptionSet(options, OPTION_SLOW_DRAM)) {
	ps3v->slow_dram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: slow_dram set\n");
    } else
   	ps3v->slow_dram = FALSE;

    if (xf86IsOptionSet(options, OPTION_FAST_DRAM)) {
	ps3v->fast_dram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fast_dram set\n");
    } else
   	ps3v->fast_dram = FALSE;

    if (xf86IsOptionSet(options, OPTION_FPM_VRAM)) {
	ps3v->fpm_vram = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: fpm_vram set\n");
    } else
   	ps3v->fpm_vram = FALSE;

    if (xf86ReturnOptValBool(options, OPTION_NOACCEL, FALSE)) {
	ps3v->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: NoAccel - Acceleration disabled\n");
    } else
   	ps3v->NoAccel = FALSE;

    if (xf86ReturnOptValBool(options, OPTION_EARLY_RAS_PRECHARGE, FALSE)) {
	ps3v->early_ras_precharge = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: early_ras_precharge set\n");
    } else
   	ps3v->early_ras_precharge = FALSE;

    if (xf86ReturnOptValBool(options, OPTION_LATE_RAS_PRECHARGE, FALSE)) {
	ps3v->late_ras_precharge = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: late_ras_precharge set\n");
    } else
   	ps3v->late_ras_precharge = FALSE;

    if (xf86ReturnOptValBool(options, OPTION_LCD_CENTER, FALSE)) {
	ps3v->lcd_center = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: lcd_center set\n");
    } else
   	ps3v->lcd_center = FALSE;

    if (xf86ReturnOptValBool(options, OPTION_SHOWCACHE, FALSE)) {
	ps3v->ShowCache = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: show_cache set\n");
    } else
   	ps3v->ShowCache = FALSE;

    if (xf86GetOptValInteger(options, OPTION_LCDCLOCK, &ps3v->LCDClk)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: lcd_setclk set to %1.3f Mhz\n",
		ps3v->LCDClk / 1000.0 );
    } else
   	ps3v->LCDClk = 0;

    if (xf86GetOptValFreq(options, OPTION_MCLK, OPTUNITS_MHZ, &realFreq)) {
	ps3v->MCLK = (int)(realFreq * 1000.0);
    	if (ps3v->MCLK <= 100000) {
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: set_mclk set to %1.3f Mhz\n",
		ps3v->MCLK / 1000.0 );
	} else {
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING
	  	, "Memory Clock value of %1.3f MHz is larger than limit of 100 MHz\n"
		, ps3v->MCLK/1000.0);
	  ps3v->MCLK = 0;
	}
    } else
   	ps3v->MCLK = 0;

    if (xf86GetOptValFreq(options, OPTION_REFCLK, OPTUNITS_MHZ, &realFreq)) {
	ps3v->REFCLK = (int)(realFreq * 1000.0);
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Option: set_refclk set to %1.3f Mhz\n",
		   ps3v->REFCLK / 1000.0 );
    } else
   	ps3v->REFCLK = 0;

    logMsgType = X_DEFAULT;
    ps3v->hwcursor = TRUE;
    if (xf86GetOptValBool(options, OPTION_HWCURSOR, &ps3v->hwcursor))
	  logMsgType = X_CONFIG;
    if (xf86ReturnOptValBool(options, OPTION_SWCURSOR, FALSE)) {
	  ps3v->hwcursor = FALSE;
	  logMsgType = X_CONFIG;
    }
    xf86DrvMsg(pScrn->scrnIndex, logMsgType, "Using %s Cursor\n",
		ps3v->hwcursor ? "HW" : "SW");

    if (xf86GetOptValBool(options, OPTION_SHADOW_FB,&ps3v->shadowFB))
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ShadowFB %s.\n",
		   ps3v->shadowFB ? "enabled" : "disabled");

    if ((rotateOptValue = xf86GetOptValString(options, OPTION_ROTATE))) {
	if(!xf86NameCmp(rotateOptValue, "CW")) {
	    /* accel is disabled below for shadowFB */
	    ps3v->shadowFB = TRUE;
	    ps3v->rotate = 1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Rotating screen clockwise - acceleration disabled\n");
	} else if(!xf86NameCmp(rotateOptValue, "CCW")) {
	    ps3v->shadowFB = TRUE;
	    ps3v->rotate = -1;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,  "Rotating screen"
		       "counter clockwise - acceleration disabled\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
		       "value for Option \"Rotate\"\n", rotateOptValue);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Valid options are \"CW\" or \"CCW\"\n");
	}
    }

    if (ps3v->shadowFB && !ps3v->NoAccel) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "HW acceleration not supported with \"shadowFB\".\n");
	ps3v->NoAccel = TRUE;
    }

    if (ps3v->rotate && ps3v->hwcursor) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "HW cursor not supported with \"rotate\".\n");
	ps3v->hwcursor = FALSE;
    }

    if (xf86IsOptionSet(options, OPTION_MX_CR3A_FIX))
      {
	if (xf86GetOptValBool(options, OPTION_MX_CR3A_FIX ,&ps3v->mx_cr3a_fix))
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "%s mx_cr3a_fix.\n",
		     ps3v->mx_cr3a_fix ? "Enabling (default)" : "Disabling");
      }
    else
      {
	ps3v->mx_cr3a_fix = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "mx_cr3a_fix.\n");
      }

    if (xf86IsOptionSet(options, OPTION_XVIDEO))
      {
	if(S3VQueryXvCapable(pScrn))
	  {
	    if (xf86GetOptValBool(options, OPTION_XVIDEO ,&ps3v->XVideo))
	      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "%s XVideo.\n",
			 ps3v->XVideo ? "Enabling (default)" : "Disabling");
	  }
	else
	  xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "XVideo not supported.\n");
      }
    else
      {
	ps3v->XVideo = S3VQueryXvCapable(pScrn);
	if(ps3v->XVideo)
	  xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "XVideo supported.\n");
	else
	  xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "XVideo not supported.\n");
      }

    free(options);
    return TRUE;
}  
