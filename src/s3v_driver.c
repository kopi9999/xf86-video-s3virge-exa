
/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>

#include "xf86.h"
#include "vbe.h"

/* Needed by the Shadow Framebuffer */
#include "shadowfb.h"

/*
 * s3v_driver.c
 * Port to 4.0 design level
 *
 * S3 ViRGE driver
 *
 * 10/98 - 3/99 Kevin Brosius
 * based largely on the SVGA ViRGE driver from 3.3.3x,
 * Started 09/03/97 by S. Marineau
 *
 *
 */


/* Most xf86 commons are already in s3v.h */
#include "s3v.h"
#include "s3v_pciids.h"


#include "globals.h"
#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif


#ifndef USE_INT10
#define USE_INT10 0
#endif

/*
 * Internals
 */
static void S3VEnableMmio(ScrnInfoPtr pScrn);
static void S3VDisableMmio(ScrnInfoPtr pScrn);

/*
 * Forward definitions for the functions that make up the driver.
 */

/* Mandatory functions */
static void S3VIdentify(int flags);
static Bool S3VProbe(DriverPtr drv, int flags);
static Bool S3VPreInit(ScrnInfoPtr pScrn, int flags);

static Bool S3VEnterVT(ScrnInfoPtr pScrn);
static void S3VLeaveVT(ScrnInfoPtr pScrn);
static void S3VSave (ScrnInfoPtr pScrn);

static Bool S3VScreenInit(ScreenPtr pScreen, int argc, char **argv);
static int S3VInternalScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen);

static Bool S3VMapMem(ScrnInfoPtr pScrn);
static void S3VUnmapMem(ScrnInfoPtr pScrn);
static Bool S3VCloseScreen(ScreenPtr pScreen);
static Bool S3VSaveScreen(ScreenPtr pScreen, int mode);

static void S3VDisplayPowerManagementSet(ScrnInfoPtr pScrn,
					 int PowerManagementMode,
					 int flags);

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

#define S3VIRGE_NAME "S3VIRGE_EXA"
#define S3VIRGE_DRIVER_NAME "s3ve"
#define S3VIRGE_VERSION_NAME PACKAGE_VERSION
#define S3VIRGE_VERSION_MAJOR   PACKAGE_VERSION_MAJOR
#define S3VIRGE_VERSION_MINOR   PACKAGE_VERSION_MINOR
#define S3VIRGE_PATCHLEVEL      PACKAGE_VERSION_PATCHLEVEL
#define S3VIRGE_DRIVER_VERSION ((S3VIRGE_VERSION_MAJOR << 24) | \
				(S3VIRGE_VERSION_MINOR << 16) | \
				S3VIRGE_PATCHLEVEL)

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup function in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

_X_EXPORT DriverRec S3VIRGE_EXA =
{
    S3VIRGE_DRIVER_VERSION,
    S3VIRGE_DRIVER_NAME,
    S3VIdentify,
    S3VProbe,
    s3ve_availableOptions,
    NULL,
    0
};


/* Supported chipsets */
static SymTabRec S3VChipsets[] = {
				  	/* base (86C325) */
  { PCI_CHIP_VIRGE,			"virge" },
  { PCI_CHIP_VIRGE,			"86C325" },
  					/* VX (86C988) */
  { PCI_CHIP_VIRGE_VX,		"virge vx" },
  { PCI_CHIP_VIRGE_VX,		"86C988" },
  					/* DX (86C375) GX (86C385) */
  { PCI_CHIP_VIRGE_DXGX,	"virge dx" },
  { PCI_CHIP_VIRGE_DXGX,	"virge gx" },
  { PCI_CHIP_VIRGE_DXGX,	"86C375" },
  { PCI_CHIP_VIRGE_DXGX,	"86C385" },
                                        /* GX2 (86C357) */
  { PCI_CHIP_VIRGE_GX2,		"virge gx2" },
  { PCI_CHIP_VIRGE_GX2,		"86C357" },
  					/* MX (86C260) */
  { PCI_CHIP_VIRGE_MX,		"virge mx" },
  { PCI_CHIP_VIRGE_MX,		"86C260" },
  					/* MX+ (86C280) */
  { PCI_CHIP_VIRGE_MXP,		"virge mx+" },
  { PCI_CHIP_VIRGE_MXP,		"86C280" },
  					/* Trio3D (86C365) */
  { PCI_CHIP_Trio3D,		"trio 3d" },
  { PCI_CHIP_Trio3D,		"86C365" },
  					/* Trio3D/2x (86C362/86C368) */
  { PCI_CHIP_Trio3D_2X,		"trio 3d/2x" },
  { PCI_CHIP_Trio3D_2X,		"86C362" },
  { PCI_CHIP_Trio3D_2X,		"86C368" },
  {-1,			NULL }
};

static PciChipsets S3VPciChipsets[] = {
  /* numChipset,		PciID,			Resource */
  { PCI_CHIP_VIRGE,      PCI_CHIP_VIRGE,     	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_VX,   PCI_CHIP_VIRGE_VX,     RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_DXGX, PCI_CHIP_VIRGE_DXGX,	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_GX2,  PCI_CHIP_VIRGE_GX2,  	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_MX,   PCI_CHIP_VIRGE_MX,   	RES_SHARED_VGA },
  { PCI_CHIP_VIRGE_MXP,  PCI_CHIP_VIRGE_MXP,  	RES_SHARED_VGA },
  { PCI_CHIP_Trio3D,     PCI_CHIP_Trio3D,  	RES_SHARED_VGA },
  { PCI_CHIP_Trio3D_2X,  PCI_CHIP_Trio3D_2X,  	RES_SHARED_VGA },
  { -1,                       -1,   		RES_UNDEFINED }
};


#ifdef XFree86LOADER

static MODULESETUPPROTO(s3virgeSetup);

static XF86ModuleVersionInfo S3VVersRec =
{
    S3VIRGE_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    S3VIRGE_VERSION_MAJOR, S3VIRGE_VERSION_MINOR, S3VIRGE_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,		       /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};


/*
 * This is the module init data for XFree86 modules.
 *
 * Its name has to be the driver name followed by ModuleData.
 */
_X_EXPORT XF86ModuleData s3veModuleData = {
    &S3VVersRec,
    s3virgeSetup,
    NULL
};

static pointer
s3virgeSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&S3VIRGE_EXA, module, 0);

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer) 1;
    } else {
	if (errmaj)
	    *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

#endif /* XFree86LOADER */


static unsigned char *find_bios_string(S3VPtr ps3v, int BIOSbase,
                                       const char *match1, const char *match2)
{
#define BIOS_BSIZE 0x10000
#define BIOS_BASE  0xc0000

   static unsigned char bios[BIOS_BSIZE];
   static int init=0;
   int i,j,l1,l2;

   if (!init) {
      init = 1;
      if (pci_device_read_rom(ps3v->PciInfo, bios))
	return NULL;
      if ((bios[0] != 0x55) || (bios[1] != 0xaa))
	 return NULL;
   }
   if (match1 == NULL)
      return NULL;

   l1 = strlen(match1);
   if (match2 != NULL)
      l2 = strlen(match2);
   else	/* for compiler-warnings */
      l2 = 0;

   for (i=0; i<BIOS_BSIZE-l1; i++)
       if (bios[i] == match1[0] && !memcmp(&bios[i],match1,l1)) {
	 if (match2 == NULL)
	    return &bios[i+l1];
	 else
	    for(j=i+l1; (j<BIOS_BSIZE-l2) && bios[j]; j++)
	       if (bios[j] == match2[0] && !memcmp(&bios[j],match2,l2))
		   return &bios[j+l2];
       }

   return NULL;
}


static Bool
S3VGetRec(ScrnInfoPtr pScrn)
{
    PVERB5("	S3VGetRec\n");
    /*
     * Allocate an 'Chip'Rec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = XNFcallocarray(sizeof(S3VRec), 1);
    /* Initialise it here when needed (or possible) */

    return TRUE;
}

static void
S3VFreeRec(ScrnInfoPtr pScrn)
{
    PVERB5("	S3VFreeRec\n");
    if (pScrn->driverPrivate == NULL)
	return;
    free(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static void
S3VIdentify(int flags)
{
    PVERB5("	S3VIdentify\n");
    xf86PrintChipsets(S3VIRGE_NAME,
	"driver (version " S3VIRGE_VERSION_NAME ") for S3 ViRGE chipsets",
	S3VChipsets);
}


static Bool
S3VProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;

    PVERB5("	S3VProbe begin\n");

    if ((numDevSections = xf86MatchDevice(S3VIRGE_DRIVER_NAME,
					  &devSections)) <= 0) {
	/*
	 * There's no matching device section in the config file, so quit
	 * now.
	 */
	return FALSE;
    }

    numUsed = xf86MatchPciInstances(S3VIRGE_NAME, PCI_S3_VENDOR_ID,
				    S3VChipsets, S3VPciChipsets, devSections,
				    numDevSections, drv, &usedChips);

    /* Free it since we don't need that list after this */
    free(devSections);
    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
	/* Allocate a ScrnInfoRec and claim the slot */
	ScrnInfoPtr pScrn = NULL;
	if ((pScrn = xf86ConfigPciEntity(pScrn,0,usedChips[i],
					       S3VPciChipsets,NULL,NULL, NULL,
					       NULL,NULL))) {
	    /* Fill in what we can of the ScrnInfoRec */
	    pScrn->driverVersion = S3VIRGE_DRIVER_VERSION;
	    pScrn->driverName	 = S3VIRGE_DRIVER_NAME;
	    pScrn->name		 = S3VIRGE_NAME;
	    pScrn->Probe	 = S3VProbe;
	    pScrn->PreInit	 = S3VPreInit;
	    pScrn->ScreenInit	 = S3VScreenInit;
	    pScrn->SwitchMode	 = S3VSwitchMode;
	    pScrn->AdjustFrame	 = S3VAdjustFrame;
	    pScrn->EnterVT	 = S3VEnterVT;
	    pScrn->LeaveVT	 = S3VLeaveVT;
	    pScrn->FreeScreen	 = NULL; /*S3VFreeScreen;*/
	    pScrn->ValidMode	 = s3ve_validMode;
	    foundScreen = TRUE;
	}
    }
    free(usedChips);
    PVERB5("	S3VProbe end\n");
    return foundScreen;
}


/* Mandatory */
static Bool
S3VPreInit(ScrnInfoPtr pScrn, int flags)
{
    EntityInfoPtr pEnt;
    S3VPtr ps3v;
    MessageType logMsgType;
    int i;
    ClockRangePtr clockRanges;

    unsigned char config1, config2, m, n, n1, n2, cr66 = 0;
    int mclk;

    vgaHWPtr hwp;
    int vgaCRIndex, vgaCRReg, vgaIOBase;

    PVERB5("	S3VPreInit 1\n");

    if (flags & PROBE_DETECT) {
      s3ve_probeDDC( pScrn, xf86GetEntityInfo(pScrn->entityList[0])->index );
      return TRUE;
    }

    /*
     * Note: This function is only called once at server startup, and
     * not at the start of each server generation.  This means that
     * only things that are persistent across server generations can
     * be initialised here.  xf86Screens[] is (pScrn is a pointer to one
     * of these).  Privates allocated using xf86AllocateScrnInfoPrivateIndex()
     * are too, and should be used for data that must persist across
     * server generations.
     *
     * Per-generation data should be allocated with
     * AllocateScreenPrivateIndex() from the ScreenInit() function.
     */

    /* The vgahw module should be loaded here when needed */

    if (!xf86LoadSubModule(pScrn, "vgahw"))
	return FALSE;

    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn))
	return FALSE;
    vgaHWSetStdFuncs(VGAHWPTR(pScrn));

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * We support both 24bpp and 32bpp layouts, so indicate that.
     */
    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support24bppFb | Support32bppFb |
				SupportConvert32to24 | PreferConvert32to24)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	case 15:
	case 16:
	case 24:
	    /* OK */
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }
    xf86PrintDepthBpp(pScrn);

    /* Get the depth24 pixmap format */
    if (pScrn->depth == 24 && pix24bpp == 0)
	pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	/* The defaults are OK for us */
	rgb zeros = {0, 0, 0};

	if (!xf86SetWeight(pScrn, zeros, zeros)) {
	    return FALSE;
	} else {
	    /* XXX check that weight returned is supported */
            ;
        }
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	return FALSE;
    } else {		/* editme - from MGA, does ViRGE? */
	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported at depth %d\n",
		       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	    return FALSE;
	}
    }

    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    /* Allocate the S3VRec driverPrivate */
    if (!S3VGetRec(pScrn)) {
	return FALSE;
    }
    ps3v = S3VPTR(pScrn);

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8) {
    				/* ViRGE supports 6 RGB bits in depth 8 */
				/* modes (with 256 entry LUT) */
      pScrn->rgbBits = 6;
    }

    /* Find the PCI slot for this screen */
    /*
     * XXX Ignoring the Type list for now.  It might be needed when
     * multiple cards are supported.
     */
    if (pScrn->numEntities > 1) {
	S3VFreeRec(pScrn);
	return FALSE;
    }

    pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

#if USE_INT10
    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86Int10InfoPtr pInt;
#if 1
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,"initializing int10\n");
	pInt = xf86InitInt10(pEnt->index);
	xf86FreeInt10(pInt);
#endif /* 1 */
    }
#endif /* USE_INT10 */
    if (xf86LoadSubModule(pScrn, "vbe")) {
	ps3v->pVbe =  VBEInit(NULL,pEnt->index);
    }

    ps3v->PciInfo = xf86GetPciInfoForEntity(pEnt->index);

    /*
     * Set the Chipset and ChipRev, allowing config file entries to
     * override. This is done before option parsing to log with wider context.
     */
    if (pEnt->device->chipset && *pEnt->device->chipset) {
	pScrn->chipset = pEnt->device->chipset;
        ps3v->Chipset = xf86StringToToken(S3VChipsets, pScrn->chipset);
        logMsgType = X_CONFIG;
    } else if (pEnt->device->chipID >= 0) {
	ps3v->Chipset = pEnt->device->chipID;
	pScrn->chipset = (char *)xf86TokenToString(S3VChipsets, ps3v->Chipset);
	logMsgType = X_CONFIG;
  	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
		   ps3v->Chipset);
    } else {
	logMsgType = X_PROBED;
	ps3v->Chipset = ps3v->PciInfo->device_id;
	pScrn->chipset = (char *)xf86TokenToString(S3VChipsets, ps3v->Chipset);
    }

    if (pEnt->device->chipRev >= 0) {
	ps3v->ChipRev = pEnt->device->chipRev;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
		   ps3v->ChipRev);
    } else {
        ps3v->ChipRev = ps3v->PciInfo->revision;
    }
    free(pEnt);

    /*
     * This shouldn't happen because such problems should be caught in
     * S3VProbe(), but check it just in case.
     */
    if (pScrn->chipset == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", ps3v->Chipset);
	vbeFree(ps3v->pVbe);
	ps3v->pVbe = NULL;
	return FALSE;
    }
    if (ps3v->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
	vbeFree(ps3v->pVbe);
	ps3v->pVbe = NULL;
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, logMsgType, "Chipset: \"%s\"\n", pScrn->chipset);

    /* Process the options */
    s3ve_parseOptions(pScrn, ps3v); /* from s3ve_options.c */

    
  S3VMapMem(pScrn);
  hwp = VGAHWPTR(pScrn);
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

    xf86ErrorFVerb(VERBLEV,
	"	S3VPreInit vgaCRIndex=%x, vgaIOBase=%x, MMIOBase=%p\n",
	vgaCRIndex, vgaIOBase, hwp->MMIOBase );


#if 0	/* Not needed in 4.0 flavors */
   /* Unlock sys regs */
   VGAOUT8(vgaCRIndex, 0x38);
   VGAOUT8(vgaCRReg, 0x48);
#endif

   /* Next go on to detect amount of installed ram */

   VGAOUT8(vgaCRIndex, 0x36);           /* for register CR36 (CONFG_REG1),*/
   config1 = VGAIN8(vgaCRReg);          /* get amount of vram installed   */

   VGAOUT8(vgaCRIndex, 0x37);           /* for register CR37 (CONFG_REG2),*/
   config2 = VGAIN8(vgaCRReg);          /* get amount of off-screen ram   */

   s3ve_readDDC(pScrn, ps3v->pVbe);     /* from s3ve_ddc.c */

   if (ps3v->pVbe) {
       vbeFree(ps3v->pVbe);
       ps3v->pVbe = NULL;
   }

   /*
    * If the driver can do gamma correction, it should call xf86SetGamma()
    * here. (from MGA, no ViRGE gamma support yet, but needed for
    * xf86HandleColormaps support.)
    */
   {
       Gamma zeros = {0.0, 0.0, 0.0};

       if (!xf86SetGamma(pScrn, zeros)) {
	   return FALSE;
       }
   }

   /* And compute the amount of video memory and offscreen memory */
   ps3v->MemOffScreen = 0;

   if (!pScrn->videoRam) {
      if (ps3v->Chipset == S3_ViRGE_VX) {
	  switch((config2 & 0x60) >> 5) {
         case 1:
            ps3v->MemOffScreen = 4 * 1024;
            break;
         case 2:
            ps3v->MemOffScreen = 2 * 1024;
            break;
         }
         switch ((config1 & 0x60) >> 5) {
         case 0:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         case 1:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 2:
            ps3v->videoRamKbytes = 6 * 1024;
            break;
         case 3:
            ps3v->videoRamKbytes = 8 * 1024;
            break;
         }
         ps3v->videoRamKbytes -= ps3v->MemOffScreen;
      }
      else if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset)) {
         switch((config1 & 0xE0) >> 5) {
         case 0:  /* 8MB -- only 4MB usable for display/cursor */
            ps3v->videoRamKbytes = 4 * 1024;
            ps3v->MemOffScreen   = 4 * 1024;
            break;
         case 1:    /* 32 bit interface -- yuck */
	   xf86ErrorFVerb(VERBLEV,
			  "	found 32 bit interface for video memory -- yuck:(\n");
         case 2:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 6:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         }
      }
      else if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
        switch((config1 & 0xE0) >> 5) {
        case 0:
        case 2:
           ps3v->videoRamKbytes = 4 * 1024;
           break;
        case 4:
           ps3v->videoRamKbytes = 2 * 1024;
           break;
        }
      }
      else if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
	  switch((config1 & 0xC0) >> 6) {
	  case 1:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 3:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         }
      }
      else {
         switch((config1 & 0xE0) >> 5) {
         case 0:
            ps3v->videoRamKbytes = 4 * 1024;
            break;
         case 4:
            ps3v->videoRamKbytes = 2 * 1024;
            break;
         case 6:
            ps3v->videoRamKbytes = 1 * 1024;
            break;
         }
      }
      					/* And save a byte value also */
      ps3v->videoRambytes = ps3v->videoRamKbytes * 1024;
      				       	/* Make sure the screen also */
					/* has correct videoRam setting */
      pScrn->videoRam = ps3v->videoRamKbytes;

      if (ps3v->MemOffScreen)
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	    	"videoram:  %dk (plus %dk off-screen)\n",
                ps3v->videoRamKbytes, ps3v->MemOffScreen);
      else
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "videoram:  %dk\n",
                ps3v->videoRamKbytes);
   } else {
   					/* Note: if ram is not probed then */
					/* ps3v->videoRamKbytes will not be init'd */
					/* should we? can do it here... */


      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "videoram:  %dk\n",
              	ps3v->videoRamKbytes);
   }

   /* reset S3 graphics engine to avoid memory corruption */
   if (ps3v->Chipset != S3_ViRGE_VX) {
      VGAOUT8(vgaCRIndex, 0x66);
      cr66 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRReg, cr66 | 0x02);
      usleep(10000);  /* wait a little bit... */
   }

   /*
    * There was a lot of plainly wrong code here. pScrn->clock is just a list
    * of supported _dotclocks_ used when you don't have a programmable clock.
    *
    * S3V and Savage seem to think that this is the max ramdac speed. This
    * driver just ignores the whole mess done before and sets
    * clockRange->maxClock differently slightly later.
    *
    * In order to not ditch information, here is a table of what the dacspeeds
    * "were" before the cleanup.
    *
    * Chipset              ### >= 24bpp ### lower
    *
    *  S3_ViRGE_VX               135000     220000
    *  S3_TRIO_3D_2X_SERIES      135000     230000
    *  S3_ViRGE_DXGX             135000     170000
    *  S3_ViRGE_GX2_SERIES       135000     170000
    *  S3_ViRGE_MX_SERIES        100000     135000
    *
    * Others devices get:
    *      > 24bpp:  57000
    *      = 24bpp:  95000
    *      < 24bpp: 135000
    *
    * Special case is the MELCO BIOS:
    *      > 24bpp:  83500
    *      = 24bpp: 111500
    *      >  8bpp: 162500
    *      <= 8bpp: 191500
    */

   if (find_bios_string(ps3v, BIOS_BASE, "S3 86C325",
			"MELCO WGP-VG VIDEO BIOS") != NULL) {
      if (xf86GetVerbosity())
	 xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "MELCO BIOS found\n");
      if (ps3v->MCLK <= 0)
          ps3v->MCLK = 74000;
   }

   if (ps3v->Chipset != S3_ViRGE_VX) {
      VGAOUT8(vgaCRIndex, 0x66);
      VGAOUT8(vgaCRReg, cr66 & ~0x02);  /* clear reset flag */
      usleep(10000);  /* wait a little bit... */
   }

   /* Detect current MCLK and print it for user */
   VGAOUT8(0x3c4, 0x08);
   VGAOUT8(0x3c5, 0x06);
   VGAOUT8(0x3c4, 0x10);
   n = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x11);
   m = VGAIN8(0x3c5);
   m &= 0x7f;
   n1 = n & 0x1f;
   n2 = (n>>5) & 0x03;
   mclk = ((1431818 * (m+2)) / (n1+2) / (1 << n2) + 50) / 100;
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      MessageType is_probed = X_PROBED;
      /*
       * try to figure out which reference clock is used:
       * Toshiba Tecra 5x0/7x0 seems to use 28.636 MHz
       * Compaq Armada 7x00 uses 14.318 MHz
       */
      if (find_bios_string(ps3v, BIOS_BASE, "COMPAQ M5 BIOS", NULL) != NULL) {
	 if (xf86GetVerbosity())
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "COMPAQ M5 BIOS found\n");
	 /* ps3v->refclk_fact = 1.0; */
      }
      else if (find_bios_string(ps3v, BIOS_BASE, "TOSHIBA Video BIOS", NULL) != NULL) {
	 if (xf86GetVerbosity())
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "TOSHIBA Video BIOS found\n");
	 /* ps3v->refclk_fact = 2.0; */
      }
      /* else */ {  /* always use guessed value... */
	 if (mclk > 60000)
	    ps3v->refclk_fact = 1.0;
	 else
	    ps3v->refclk_fact = 2.0;  /* don't know why ??? */
      }
      if (ps3v->REFCLK != 0) {
	 ps3v->refclk_fact = ps3v->REFCLK / 14318.0;
	 is_probed = X_CONFIG;
      }
      else
	 ps3v->REFCLK = (int)(14318.18 * ps3v->refclk_fact);

      mclk = (int)(mclk * ps3v->refclk_fact);
      xf86DrvMsg(pScrn->scrnIndex, is_probed, "assuming RefCLK value of %1.3f MHz\n",
		 ps3v->REFCLK / 1000.0);
   }
   xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Detected current MCLK value of %1.3f MHz\n",
	     mclk / 1000.0);

   if (S3_ViRGE_MX_SERIES(ps3v->Chipset) && xf86GetVerbosity()) {
       int lcdclk, h_lcd, v_lcd;
       if (ps3v->LCDClk) {
	  lcdclk = ps3v->LCDClk;
       } else {
	  unsigned char sr12, sr13, sr29;
          VGAOUT8(0x3c4, 0x12);
          sr12 = VGAIN8(0x3c5);
          VGAOUT8(0x3c4, 0x13);
          sr13 = VGAIN8(0x3c5) & 0x7f;
          VGAOUT8(0x3c4, 0x29);
          sr29 = VGAIN8(0x3c5);
    	  n1 = sr12 & 0x1f;
    	  n2 = ((sr12>>6) & 0x03) | ((sr29 & 0x01) << 2);
          lcdclk = ((int)(ps3v->refclk_fact * 1431818 * (sr13+2)) / (n1+2) / (1 << n2) + 50) / 100;
       }
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
       xf86DrvMsg(pScrn->scrnIndex
	      , ps3v->LCDClk ? X_CONFIG : X_PROBED
       	      , "LCD size %dx%d, clock %1.3f MHz\n"
	      , h_lcd, v_lcd
	      , lcdclk / 1000.0);
   }

   S3VDisableMmio(pScrn);
   S3VUnmapMem(pScrn);

   /* And finally set various possible option flags */

   ps3v->bankedMono = FALSE;


#if 0
   vga256InfoRec.directMode = XF86DGADirectPresent;
#endif

    				/* Lower depths default to config file */
  pScrn->virtualX = pScrn->display->virtualX;
				/* Adjust the virtualX to meet ViRGE hardware */
				/* limits for depth 24, bpp 24 & 32.  This is */
				/* mostly for 32 bpp as 1024x768 is one pixel */
				/* larger than supported. */
  if (pScrn->depth == 24)
      if ( ((pScrn->bitsPerPixel/8) * pScrn->display->virtualX) > 4095 ) {
        pScrn->virtualX = 4095 / (pScrn->bitsPerPixel / 8);
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	   "Virtual width adjusted, max for this depth & bpp is %d.\n",
	   pScrn->virtualX );
      }

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = XNFcallocarray(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = 10000;
    if (ps3v->Chipset == S3_ViRGE_VX )
        clockRanges->maxClock = 440000;
    else
        clockRanges->maxClock = 270000;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;	/* yes, S3V SVGA 3.3.2 */
    clockRanges->doubleScanAllowed = TRUE;

  					/* Screen pointer 		*/
    i = xf86ValidateModes(pScrn,
  					/* Available monitor modes 	*/
					/* (DisplayModePtr availModes)  */
		pScrn->monitor->Modes,
					/* req mode names for screen 	*/
					/* (char **modesNames)  	*/
		pScrn->display->modes,
					/* list of clock ranges allowed */
					/* (ClockRangePtr clockRanges) 	*/
		clockRanges,
					/* list of driver line pitches, */
					/* supply or NULL and use min/	*/
					/* max below			*/
					/* (int *linePitches)		*/
		NULL,
					/* min lin pitch (width)	*/
					/* (int minPitch)		*/
		256,
					/* max line pitch (width)	*/
					/* (int maxPitch)		*/
		2048,
					/* bits of granularity for line	*/
					/* pitch (width) above, reguired*/
					/* (int pitchInc)		*/
		pScrn->bitsPerPixel,
					/* min virt height, 0 no limit	*/
					/* (int minHeight)		*/
		128,
					/* max virt height, 0 no limit	*/
					/* (int maxHeight)		*/
		2048,
					/* force virtX, 0 for auto 	*/
					/* (int VirtualX) 		*/
					/* value is adjusted above for  */
					/* hardware limits */
		pScrn->virtualX,
					/* force virtY, 0 for auto	*/
					/* (int VirtualY)		*/
		pScrn->display->virtualY,
					/* size (bytes) of aper used to	*/
					/* access video memory		*/
					/* (unsigned long apertureSize)	*/
		ps3v->videoRambytes,
					/* how to pick mode */
					/* (LookupModeFlags strategy)	*/
		LOOKUP_BEST_REFRESH);

    if (i == -1) {
	S3VFreeRec(pScrn);
	return FALSE;
    }
    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	S3VFreeRec(pScrn);
	return FALSE;
    }

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);
    /*xf86SetCrtcForModes(pScrn, 0);*/

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    /* Load bpp-specific modules */
    if( xf86LoadSubModule(pScrn, "fb") == NULL )
    {
	S3VFreeRec(pScrn);
	return FALSE;
    }

    if (!ps3v->shadowFB | !ps3v->NoAccel) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
		       "EXA acceleration not yet supported. Falling back to shadowFB\n");
	ps3v->shadowFB = 1;
	ps3v->NoAccel = 1;
    }

    /* Load ramdac if needed */
    if (ps3v->hwcursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    S3VFreeRec(pScrn);
	    return FALSE;
	}
    }

    if (ps3v->shadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    S3VFreeRec(pScrn);
	    return FALSE;
	}
    }

    /* Setup WAITFIFO() for accel and ModeInit() */
    /* Needs to be done prior to first ModeInit call */
    /* and any accel activity. */
    switch(ps3v->Chipset)
      {
	/* GX2_SERIES chips, GX2 & TRIO_3D_2X */
      case S3_ViRGE_GX2:
      case S3_TRIO_3D_2X:
	ps3v->pWaitFifo = s3ve_waitFifoGX2;
	ps3v->pWaitCmd = s3ve_waitCmdGX2;
	break;
      case S3_ViRGE:
      case S3_ViRGE_VX:
      default:
	ps3v->pWaitFifo = s3ve_waitFifoMain;
	/* Do nothing... */
	ps3v->pWaitCmd = s3ve_waitDummy;
	break;
      }

    return TRUE;
}


/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */


/* Mandatory */
static Bool
S3VEnterVT(ScrnInfoPtr pScrn)
{
    PVERB5("	S3VEnterVT\n");
    				/* Enable MMIO and map memory */
#ifdef unmap_always
    S3VMapMem(pScrn);
#endif
    S3VEnableMmio(pScrn);

    S3VSave(pScrn);
    return s3ve_modeInit(pScrn, pScrn->currentMode);
}


/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 * We may wish to remap video/MMIO memory too.
 *
 */

/* Mandatory */
static void
S3VLeaveVT(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    S3VPtr ps3v = S3VPTR(pScrn);
    vgaRegPtr vgaSavePtr = &hwp->SavedReg;
    S3VRegPtr S3VSavePtr = &ps3v->SavedReg;

    PVERB5("	S3VLeaveVT\n");
    					/* Like S3VRestore, but uses passed */
					/* mode registers.		    */
    s3ve_writeMode(pScrn, vgaSavePtr, S3VSavePtr);
    					/* Restore standard register access */
					/* and unmap memory.		    */
    S3VDisableMmio(pScrn);
#ifdef unmap_always
    S3VUnmapMem(pScrn);
#endif
    /*vgaHWLockMMIO(hwp);*/

}


/*
 * This function performs the inverse of the restore function: It saves all
 * the standard and extended registers that we are going to modify to set
 * up a video mode. Again, we also save the STREAMS context if it is needed.
 *
 * prototype
 *   void ChipSave(ScrnInfoPtr pScrn)
 *
 */

static void
S3VSave (ScrnInfoPtr pScrn)
{
  unsigned char cr3a, cr66;
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  S3VPtr ps3v = S3VPTR(pScrn);
  S3VRegPtr save = &ps3v->SavedReg;
  int vgaCRIndex, vgaCRReg, vgaIOBase;
  vgaIOBase = hwp->IOBase;
  vgaCRIndex = 0;

  vgaCRReg = 0;

  vgaCRReg = vgaIOBase + 5;
  vgaCRIndex = vgaIOBase + 4;

    PVERB5("	S3VSave\n");

   /*
    * This function will handle creating the data structure and filling
    * in the generic VGA portion.
    */

   VGAOUT8(vgaCRIndex, 0x66);
   cr66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr66 | 0x80);
   VGAOUT8(vgaCRIndex, 0x3a);
   cr3a = VGAIN8(vgaCRReg);
   save->CR3A = cr3a;

   VGAOUT8(vgaCRReg, cr3a | 0x80);

   /* VGA_SR_MODE saves mode info only, no fonts, no colormap */
					/* Save all for primary, anything */
					/* for secondary cards?, do MODE */
					/* for the moment. */
   if (xf86IsPrimaryPci(ps3v->PciInfo))
   	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_ALL);
   else
   	vgaHWSave(pScrn, vgaSavePtr, VGA_SR_MODE);

   VGAOUT8(vgaCRIndex, 0x66);
   VGAOUT8(vgaCRReg, cr66);
   VGAOUT8(vgaCRIndex, 0x3a);
   VGAOUT8(vgaCRReg, cr3a);

   /* First unlock extended sequencer regs */
   VGAOUT8(0x3c4, 0x08);
   save->SR08 = VGAIN8(0x3c5);
   VGAOUT8(0x3c5, 0x06);

   /* Now we save all the s3 extended regs we need */
   VGAOUT8(vgaCRIndex, 0x31);
   save->CR31 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x34);
   save->CR34 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x36);
   save->CR36 = VGAIN8(vgaCRReg);

   /* workaround cr3a corruption */
   if( !(ps3v->mx_cr3a_fix))
     {
       VGAOUT8(vgaCRIndex, 0x3a);
       save->CR3A = VGAIN8(vgaCRReg);
     }

   if (!S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     VGAOUT8(vgaCRIndex, 0x40);
     save->CR40 = VGAIN8(vgaCRReg);
   }
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(vgaCRIndex, 0x41);
     save->CR41 = VGAIN8(vgaCRReg);
   }
   VGAOUT8(vgaCRIndex, 0x42);
   save->CR42 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x45);
   save->CR45 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x51);
   save->CR51 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x53);
   save->CR53 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x54);
   save->CR54 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x55);
   save->CR55 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x58);
   save->CR58 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x63);
   save->CR63 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x66);
   save->CR66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x67);
   save->CR67 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x68);
   save->CR68 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x69);
   save->CR69 = VGAIN8(vgaCRReg);

   VGAOUT8(vgaCRIndex, 0x33);
   save->CR33 = VGAIN8(vgaCRReg);
   if (S3_TRIO_3D_2X_SERIES(ps3v->Chipset) || S3_ViRGE_GX2_SERIES(ps3v->Chipset)
       /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
   {
      VGAOUT8(vgaCRIndex, 0x85);
      save->CR85 = VGAIN8(vgaCRReg);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX) {
      VGAOUT8(vgaCRIndex, 0x86);
      save->CR86 = VGAIN8(vgaCRReg);
   }
   if ((ps3v->Chipset == S3_ViRGE_GX2) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset) ) {
      VGAOUT8(vgaCRIndex, 0x7B);
      save->CR7B = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x7D);
      save->CR7D = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x87);
      save->CR87 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x92);
      save->CR92 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x93);
      save->CR93 = VGAIN8(vgaCRReg);
   }
   if (ps3v->Chipset == S3_ViRGE_DXGX || S3_ViRGE_GX2_SERIES(ps3v->Chipset) ||
       S3_ViRGE_MX_SERIES(ps3v->Chipset) || S3_TRIO_3D_SERIES(ps3v->Chipset)) {
      VGAOUT8(vgaCRIndex, 0x90);
      save->CR90 = VGAIN8(vgaCRReg);
      VGAOUT8(vgaCRIndex, 0x91);
      save->CR91 = VGAIN8(vgaCRReg);
   }

   /* Extended mode timings regs */

   VGAOUT8(vgaCRIndex, 0x3b);
   save->CR3B = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x3c);
   save->CR3C = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x43);
   save->CR43 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x5d);
   save->CR5D = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x5e);
   save->CR5E = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x65);
   save->CR65 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRIndex, 0x6d);
   save->CR6D = VGAIN8(vgaCRReg);


   /* Save sequencer extended regs for DCLK PLL programming */

   VGAOUT8(0x3c4, 0x10);
   save->SR10 = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x11);
   save->SR11 = VGAIN8(0x3c5);

   VGAOUT8(0x3c4, 0x12);
   save->SR12 = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x13);
   save->SR13 = VGAIN8(0x3c5);
   if (S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x29);
     save->SR29 = VGAIN8(0x3c5);
   }
        /* SR 54,55,56,57 undocumented for GX2.  Was this supposed to be CR? */
        /* (These used to be part of the above if() */
   if (S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x54);
     save->SR54 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x55);
     save->SR55 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x56);
     save->SR56 = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x57);
     save->SR57 = VGAIN8(0x3c5);
   }

   VGAOUT8(0x3c4, 0x15);
   save->SR15 = VGAIN8(0x3c5);
   VGAOUT8(0x3c4, 0x18);
   save->SR18 = VGAIN8(0x3c5);
   if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
     VGAOUT8(0x3c4, 0x0a);
     save->SR0A = VGAIN8(0x3c5);
     VGAOUT8(0x3c4, 0x0F);
     save->SR0F = VGAIN8(0x3c5);
   }

   VGAOUT8(vgaCRIndex, 0x66);
   cr66 = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr66 | 0x80);
   VGAOUT8(vgaCRIndex, 0x3a);
   cr3a = VGAIN8(vgaCRReg);
   VGAOUT8(vgaCRReg, cr3a | 0x80);

   /* And if streams is to be used, save that as well */

   if(ps3v->NeedSTREAMS) {
      s3ve_saveSTREAMS(pScrn, save->STREAMS);
      }

   /* Now save Memory Interface Unit registers */
   if( S3_ViRGE_GX2_SERIES(ps3v->Chipset)
      /* MXTESTME */ || S3_ViRGE_MX_SERIES(ps3v->Chipset) )
     {
     /* No MMPR regs on MX & GX2 */
     }
   else
     {
     save->MMPR0 = INREG(FIFO_CONTROL_REG);
     save->MMPR1 = INREG(MIU_CONTROL_REG);
     save->MMPR2 = INREG(STREAMS_TIMEOUT_REG);
     save->MMPR3 = INREG(MISC_TIMEOUT_REG);
     }

   if (xf86GetVerbosity() > 1) {
      /* Debug */
     /* Which chipsets? */
     if (
	 /* virge */
	 ps3v->Chipset == S3_ViRGE ||
	 /* VX */
	 S3_ViRGE_VX_SERIES(ps3v->Chipset) ||
	 /* DX & GX */
	 ps3v->Chipset == S3_ViRGE_DXGX ||
	 /* GX2 & Trio3D_2X */
	 /* S3_ViRGE_GX2_SERIES(ps3v->Chipset) || */
	 /* Trio3D_2X */
	 /* S3_TRIO_3D_2X_SERIES(ps3v->Chipset) */
	 /* MX & MX+ */
	 /* S3_ViRGE_MX_SERIES(ps3v->Chipset) || */
	 /* MX+ only */
	 /* S3_ViRGE_MXP_SERIES(ps3v->Chipset) || */
	 /* Trio3D */
	 ps3v->Chipset == S3_TRIO_3D
         )
       {

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, VERBLEV,
         "MMPR regs: %08lx %08lx %08lx %08lx\n",
	     (unsigned long)INREG(FIFO_CONTROL_REG),
	     (unsigned long)INREG(MIU_CONTROL_REG),
	     (unsigned long)INREG(STREAMS_TIMEOUT_REG),
	     (unsigned long)INREG(MISC_TIMEOUT_REG));
       }

      PVERB5("\n\nViRGE driver: saved current video mode. Register dump:\n\n");
   }

   VGAOUT8(vgaCRIndex, 0x3a);
   VGAOUT8(vgaCRReg, cr3a);
   VGAOUT8(vgaCRIndex, 0x66);
   VGAOUT8(vgaCRReg, cr66);
   				/* Dup the VGA & S3V state to the */
				/* new mode state, but only first time. */
   if( !ps3v->ModeStructInit ) {
     /* XXX Should check the return value of vgaHWCopyReg() */
     vgaHWCopyReg( &hwp->ModeReg, vgaSavePtr );
     memcpy( &ps3v->ModeReg, save, sizeof(S3VRegRec) );
     ps3v->ModeStructInit = TRUE;
   }

   if (xf86GetVerbosity() > 1) s3ve_printRegs(pScrn);

   return;
}


/* MapMem - contains half of pre-4.0 EnterLeave function */
/* The EnterLeave function which en/dis access to IO ports and ext. regs */
                /********************************************************/
		/* Aaagh...  So many locations!  On my machine (KJB) the*/
		/* following is true. 					*/
		/* PciInfo->memBase[0] returns e400 0000 		*/
		/* From my ViRGE manual, the memory map looks like 	*/
		/* Linear mem - 16M  	000 0000 - 0ff ffff 		*/
		/* Image xfer - 32k  	100 0000 - 100 7fff 		*/
		/* PCI cnfg    		100 8000 - 100 8043 		*/
		/* ...				   			*/
		/* CRT VGA 3b? reg	100 83b0 - 			*/
		/* And S3_NEWMMIO_VGABASE = S3_NEWMMIO_REGBASE + 0x8000	*/
		/* where S3_NEWMMIO_REGBASE = 0x100 0000  ( 16MB )      */
		/* S3_NEWMMIO_REGSIZE = 0x1 0000  ( 64KB )		*/
		/* S3V_MMIO_REGSIZE = 0x8000 ( 32KB ) - above includes	*/
		/* the image transfer area, so this one is used instead.*/
		/* ps3v->IOBase is assigned the virtual address returned*/
		/* from MapPciMem, it is the address to base all 	*/
		/* register access. (It is a pointer.)  		*/
		/* hwp->MemBase is a CARD32, containing the register	*/
		/* base. (It's a conversion from IOBase above.) 	*/
                /********************************************************/


static Bool
S3VMapMem(ScrnInfoPtr pScrn)
{
  S3VPtr ps3v;
  vgaHWPtr hwp;

    PVERB5("	S3VMapMem\n");

  ps3v = S3VPTR(pScrn);

    					/* Map the ViRGE register space */
					/* Starts with Image Transfer area */
					/* so that we can use registers map */
					/* structure - see newmmio.h */
					/* around 0x10000 from MemBase */
  {
    void** result = (void**)&ps3v->MapBase;
    int err = pci_device_map_range(ps3v->PciInfo,
				   ps3v->PciInfo->regions[0].base_addr + S3_NEWMMIO_REGBASE,
				   S3_NEWMMIO_REGSIZE,
				   PCI_DEV_MAP_FLAG_WRITABLE,
				   result);

    if (err)
      return FALSE;
  }
  ps3v->MapBaseDense = ps3v->MapBase;

  if( !ps3v->MapBase ) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	"Internal error: could not map registers.\n");
    return FALSE;
  }
					/* Map the framebuffer */
  if (ps3v->videoRambytes) { /* not set in PreInit() */
      {
	void** result = (void**)&ps3v->FBBase;
	int err = pci_device_map_range(ps3v->PciInfo,
				       ps3v->PciInfo->regions[0].base_addr,
				       ps3v->videoRambytes,
				       PCI_DEV_MAP_FLAG_WRITABLE |
				       PCI_DEV_MAP_FLAG_WRITE_COMBINE,
				       result);

	if (err)
	  return FALSE;
      }

      if( !ps3v->FBBase ) {
	  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		     "Internal error: could not map framebuffer.\n");
	  return FALSE;
      }
  		       		/* Initially the visual display start */
				/* is the same as the mapped start. */
      ps3v->FBStart = ps3v->FBBase;
  }

  pScrn->memPhysBase = ps3v->PciInfo->regions[0].base_addr;
  pScrn->fbOffset = 0;

  				/* Set up offset to hwcursor memory area */
  				/* It's a 1K chunk at the end of the frame buffer */
  ps3v->FBCursorOffset = ps3v->videoRambytes - 1024;
  S3VEnableMmio( pScrn);
   					/* Assign hwp->MemBase & IOBase here */
  hwp = VGAHWPTR(pScrn);
					/* Sets MMIOBase and Offset, assigns */
					/* functions. Offset from map area   */
					/* to VGA reg area is 0x8000. */
  vgaHWSetMmioFuncs( hwp, ps3v->MapBase, S3V_MMIO_REGSIZE );
  					/* assigns hwp->IOBase to 3D0 or 3B0 */
					/* needs hwp->MMIOBase to work */
  vgaHWGetIOBase(hwp);

    					/* Map the VGA memory when the */
					/* primary video */

  if (xf86IsPrimaryPci(ps3v->PciInfo)) {
    hwp->MapSize = 0x10000;
    if (!vgaHWMapMem(pScrn))
      return FALSE;
    ps3v->PrimaryVidMapped = TRUE;
  }

  return TRUE;
}



/* UnMapMem - contains half of pre-4.0 EnterLeave function */
/* The EnterLeave function which en/dis access to IO ports and ext. regs */

static void
S3VUnmapMem(ScrnInfoPtr pScrn)
{
  S3VPtr ps3v;

  ps3v = S3VPTR(pScrn);
					/* Unmap VGA mem if mapped. */
  if( ps3v->PrimaryVidMapped ) {
    vgaHWUnmapMem( pScrn );
    ps3v->PrimaryVidMapped = FALSE;
  }

  pci_device_unmap_range(ps3v->PciInfo, ps3v->MapBase,
			 S3_NEWMMIO_REGSIZE);

  pci_device_unmap_range(ps3v->PciInfo, ps3v->FBBase,
			 ps3v->videoRambytes);
  return;
}



/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
S3VScreenInit(ScreenPtr pScreen, int argc, char **argv)
{
  ScrnInfoPtr pScrn;
  S3VPtr ps3v;
  int ret;

  PVERB5("	S3VScreenInit\n");
                                        /* First get the ScrnInfoRec */
  pScrn = xf86ScreenToScrn(pScreen);
  					/* Get S3V rec */
  ps3v = S3VPTR(pScrn);
   					/* Map MMIO regs and framebuffer */
  if( !S3VMapMem(pScrn) )
    return FALSE;
    					/* Save the chip/graphics state */
  S3VSave(pScrn);
				 	/* Blank the screen during init */
  vgaHWBlankScreen(pScrn, TRUE );
    					/* Initialise the first mode */
  if (!s3ve_modeInit(pScrn, pScrn->currentMode))
    return FALSE;

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     *
     * For most PC hardware at depths >= 8, the defaults that fb uses
     * are not appropriate.  In this driver, we fixup the visuals after.
     */

    /*
     * Reset the visual list.
     */
  miClearVisualTypes();

    /* Setup the visuals we support. */

    /*
     * For bpp > 8, the default visuals are not acceptable because we only
     * support TrueColor and not DirectColor.  To deal with this, call
     * miSetVisualTypes with the appropriate visual mask.
     */

  if (pScrn->bitsPerPixel > 8) {
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
				pScrn->defaultVisual))
	    return FALSE;

	if (!miSetPixmapDepths ())
	    return FALSE;
  } else {
	if (!miSetVisualTypes(pScrn->depth,
			      miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
	    return FALSE;

	if (!miSetPixmapDepths ())
	    return FALSE;
  }

  ret = S3VInternalScreenInit(pScrn, pScreen);

  if (!ret)
    return FALSE;

  xf86SetBlackWhitePixels(pScreen);

  if (pScrn->bitsPerPixel > 8) {
    	VisualPtr visual;
					/* Fixup RGB ordering */
	visual = pScreen->visuals + pScreen->numVisuals;
	while (--visual >= pScreen->visuals) {
	    if ((visual->class | DynamicClass) == DirectColor) {
		visual->offsetRed = pScrn->offset.red;
		visual->offsetGreen = pScrn->offset.green;
		visual->offsetBlue = pScrn->offset.blue;
		visual->redMask = pScrn->mask.red;
		visual->greenMask = pScrn->mask.green;
		visual->blueMask = pScrn->mask.blue;
	    }
	}
  }

  /* must be after RGB ordering fixed */
  fbPictureInit (pScreen, 0, 0);

  	      				/* Initialize acceleration layer */
  if (!ps3v->NoAccel) {
    if(pScrn->bitsPerPixel == 32) {
      /* 32 bit Accel currently broken
      if (!S3VAccelInit32(pScreen))
        return FALSE;
	*/
	;
    } else
      if (!s3ve_accelInit(pScreen))
        return FALSE;
  }

  xf86SetBackingStore(pScreen);
  xf86SetSilkenMouse(pScreen);
  					/* hardware cursor needs to wrap this layer */
  S3VDGAInit(pScreen);

    					/* Initialise cursor functions */
  miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer.
	Must follow software cursor initialization*/
  if (ps3v->hwcursor) {
  if(!s3ve_HWCursorInit(pScreen)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Hardware cursor initialization failed\n");
		}
  }

  if (ps3v->shadowFB) {
      RefreshAreaFuncPtr refreshArea = s3vRefreshArea;

      if(ps3v->rotate) {
	  if (!ps3v->PointerMoved) {
	      ps3v->PointerMoved = pScrn->PointerMoved;
	      pScrn->PointerMoved = s3vPointerMoved;
	  }

	  switch(pScrn->bitsPerPixel) {
	  case 8:	refreshArea = s3vRefreshArea8;	break;
	  case 16:	refreshArea = s3vRefreshArea16;	break;
	  case 24:	refreshArea = s3vRefreshArea24;	break;
	  case 32:	refreshArea = s3vRefreshArea32;	break;
	  }
      }

      ShadowFBInit(pScreen, refreshArea);
  }

    					/* Initialise default colourmap */
  if (!miCreateDefColormap(pScreen))
    return FALSE;
  					/* Initialize colormap layer.   */
					/* Must follow initialization   */
					/* of the default colormap. 	*/
					/* And SetGamma call, else it 	*/
					/* will load palette with solid */
					/* white. */
  if(!xf86HandleColormaps(pScreen, 256, 6, s3ve_loadPalette, NULL,
			CMAP_RELOAD_ON_MODE_SWITCH ))
	return FALSE;
				    	/* All the ugly stuff is done, 	*/
					/* so re-enable the screen. 	*/
  vgaHWBlankScreen(pScrn, FALSE );

#if 0
  pScrn->racMemFlags = RAC_COLORMAP | RAC_CURSOR | RAC_FB | RAC_VIEWPORT;
#endif
  pScreen->SaveScreen = S3VSaveScreen;

    					/* Wrap the current CloseScreen function */
  ps3v->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = S3VCloseScreen;

  if(xf86DPMSInit(pScreen, S3VDisplayPowerManagementSet, 0) == FALSE)
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "DPMS initialization failed!\n");

  S3VInitVideo(pScreen);

    /* Report any unused options (only for the first generation) */
  if (serverGeneration == 1) {
    xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
  }
    					/* Done */
  return TRUE;
}



/* Common init routines needed in EnterVT and ScreenInit */

static int
S3VInternalScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen)
{
  int ret = TRUE;
  S3VPtr ps3v;
  int width, height, displayWidth;
  unsigned char* FBStart;

  ps3v = S3VPTR(pScrn);

  displayWidth = pScrn->displayWidth;
  if (ps3v->rotate) {
      height = pScrn->virtualX;
      width = pScrn->virtualY;
  } else {
      width = pScrn->virtualX;
      height = pScrn->virtualY;
  }

  if(ps3v->shadowFB) {
      ps3v->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
      ps3v->ShadowPtr = malloc(ps3v->ShadowPitch * height);
      displayWidth = ps3v->ShadowPitch / (pScrn->bitsPerPixel >> 3);
      FBStart = ps3v->ShadowPtr;
  } else {
      ps3v->ShadowPtr = NULL;
      FBStart = ps3v->FBStart;
  }

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    switch (pScrn->bitsPerPixel)
    {
	case 8:
	case 16:
	case 24:
	case 32:
	    ret = fbScreenInit(pScreen, FBStart, width,
			       height, pScrn->xDpi, pScrn->yDpi,
			       displayWidth, pScrn->bitsPerPixel);
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Internal error: invalid bpp (%d) in S3VScreenInit\n",
		       pScrn->bitsPerPixel);
	    ret = FALSE;
	    break;
    }

  return ret;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
S3VCloseScreen(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
  vgaHWPtr hwp = VGAHWPTR(pScrn);
  S3VPtr ps3v = S3VPTR(pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  S3VRegPtr S3VSavePtr = &ps3v->SavedReg;

    					/* Like S3VRestore, but uses passed */
					/* mode registers.		    */
  if (pScrn->vtSema) {
      s3ve_writeMode(pScrn, vgaSavePtr, S3VSavePtr);
      vgaHWLock(hwp);
      S3VDisableMmio(pScrn);
      S3VUnmapMem(pScrn);
  }

  if (ps3v->DGAModes)
  	free(ps3v->DGAModes);

  pScrn->vtSema = FALSE;

  pScreen->CloseScreen = ps3v->CloseScreen;

  return (*pScreen->CloseScreen)(pScreen);
}

/* Do screen blanking */

/* Mandatory */
static Bool
S3VSaveScreen(ScreenPtr pScreen, int mode)
{
  return vgaHWSaveScreen(pScreen, mode);
}

/* Used to adjust start address in frame buffer. We use the new
 * CR69 reg for this purpose instead of the older CR31/CR51 combo.
 * If STREAMS is running, we program the STREAMS start addr. registers.
 */

void
S3VAdjustFrame(ScrnInfoPtr pScrn, int x, int y)
{
   vgaHWPtr hwp = VGAHWPTR(pScrn);
   S3VPtr ps3v = S3VPTR(pScrn);
   int Base;
   int vgaCRIndex, vgaCRReg, vgaIOBase;
   vgaIOBase = hwp->IOBase;
   vgaCRIndex = vgaIOBase + 4;
   vgaCRReg = vgaIOBase + 5;

   if(ps3v->ShowCache && y)
	y += pScrn->virtualY - 1;

   if( (ps3v->STREAMSRunning == FALSE) ||
      S3_ViRGE_GX2_SERIES(ps3v->Chipset) || S3_ViRGE_MX_SERIES(ps3v->Chipset)) {
      Base = ((y * pScrn->displayWidth + x)
		* (pScrn->bitsPerPixel / 8)) >> 2;
      if (pScrn->bitsPerPixel == 24)
	Base = Base+2 - (Base+2) % 3;
      if (pScrn->bitsPerPixel == 16)
	if (S3_TRIO_3D_SERIES(ps3v->Chipset) && pScrn->modes->Clock > 115000)
	  Base &= ~1;

      /* Now program the start address registers */
      VGAOUT16(vgaCRIndex, (Base & 0x00FF00) | 0x0C);
      VGAOUT16(vgaCRIndex, ((Base & 0x00FF) << 8) | 0x0D);
      VGAOUT8(vgaCRIndex, 0x69);
      VGAOUT8(vgaCRReg, (Base & 0x0F0000) >> 16);
      }
   else {          /* Change start address for STREAMS case */
      VerticalRetraceWait();
      if(ps3v->Chipset == S3_ViRGE_VX)
	OUTREG(PSTREAM_FBADDR0_REG,
		   ((y * pScrn->displayWidth + (x & ~7)) *
		    pScrn->bitsPerPixel / 8));
      else
	OUTREG(PSTREAM_FBADDR0_REG,
		   ((y * pScrn->displayWidth + (x & ~3)) *
		    pScrn->bitsPerPixel / 8));
      }

   return;
}

/* Usually mandatory */
Bool
S3VSwitchMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  return s3ve_modeInit(pScrn, mode);
}

/*
 * Functions to support getting a ViRGE card into MMIO mode if it fails to
 * default to MMIO enabled.
 */

void
S3VEnableMmio(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp;
  S3VPtr ps3v;
  unsigned int vgaCRIndex, vgaCRReg;
  unsigned char val;
  unsigned int PIOOffset = 0;

  PVERB5("	S3VEnableMmio\n");

  hwp = VGAHWPTR(pScrn);
  ps3v = S3VPTR(pScrn);

  /*
   * enable chipset (seen on uninitialized secondary cards)
   * might not be needed once we use the VGA softbooter
   * (EE 05/04/99)
   */
  vgaHWSetStdFuncs(hwp);
  /*
   * any access to the legacy VGA ports is done here.
   * If legacy VGA is inaccessible the MMIO base _has_
   * to be set correctly already and MMIO _has_ to be
   * enabled.
   */
  val = inb(PIOOffset + 0x3C3);               /*@@@EE*/
  outb(PIOOffset + 0x3C3, val | 0x01);
  /*
   * set CR registers to color mode
   * in mono mode extended CR registers
   * are not accessible. (EE 05/04/99)
   */
  val = inb(PIOOffset + VGA_MISC_OUT_R);      /*@@@EE*/
  outb(PIOOffset + VGA_MISC_OUT_W, val | 0x01);
  vgaHWGetIOBase(hwp);             	/* Get VGA I/O base */
  vgaCRIndex = PIOOffset + hwp->IOBase + 4;
  vgaCRReg = vgaCRIndex + 1;
#if 1
  /*
   * set linear base register to the PCI register values
   * some DX chipsets don't seem to do it automatically
   * (EE 06/03/99)
   */
  outb(vgaCRIndex, 0x59);         /*@@@EE*/
  outb(vgaCRReg, ps3v->PciInfo->regions[0].base_addr >> 24);
  outb(vgaCRIndex, 0x5A);
  outb(vgaCRReg, ps3v->PciInfo->regions[0].base_addr >> 16);
  outb(vgaCRIndex, 0x53);
#endif
  /* Save register for restore */
  ps3v->EnableMmioCR53 = inb(vgaCRReg);
  			      	/* Enable new MMIO, if TRIO mmio is already */
				/* enabled, then it stays enabled. */
  outb(vgaCRReg, ps3v->EnableMmioCR53 | 0x08);
  outb(PIOOffset + VGA_MISC_OUT_W, val);
  if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
    outb(vgaCRIndex, 0x40);
    val = inb(vgaCRReg);
    outb(vgaCRReg, val | 1);
  }
}


void
S3VDisableMmio(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp;
  S3VPtr ps3v;
  unsigned int vgaCRIndex, vgaCRReg;

  PVERB5("	S3VDisableMmio\n");

  hwp = VGAHWPTR(pScrn);
  ps3v = S3VPTR(pScrn);

  vgaCRIndex = hwp->IOBase + 4;
  vgaCRReg = vgaCRIndex + 1;
  outb(vgaCRIndex, 0x53);
				/* Restore register's original state */
  outb(vgaCRReg, ps3v->EnableMmioCR53);
  if (S3_TRIO_3D_SERIES(ps3v->Chipset)) {
    unsigned char val;
    outb(vgaCRIndex, 0x40);
    val = inb(vgaCRReg);
    outb(vgaCRReg, val | 1);
  }
}


/* this is just a debugger hook */
/*
void print_subsys_stat(void *s3vMmioMem);
void
print_subsys_stat(void *s3vMmioMem)
{
  ErrorF("IN_SUBSYS_STAT() = %x\n", IN_SUBSYS_STAT());
  return;
}
*/

/*
 * S3VDisplayPowerManagementSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 */
static void
S3VDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
			     int flags)
{
  S3VPtr ps3v;
  unsigned char sr8 = 0x0, srd = 0x0;
  char modestr[][40] = { "On","Standby","Suspend","Off" };

  ps3v = S3VPTR(pScrn);

  /* unlock extended sequence registers */

  VGAOUT8(0x3c4, 0x08);
  sr8 = VGAIN8(0x3c5);
  sr8 |= 0x6;
  VGAOUT8(0x3c5, sr8);

  /* load SRD */
  VGAOUT8(0x3c4, 0x0d);
  srd = VGAIN8(0x3c5);

  srd &= 0x03; /* clear the sync control bits of srd */

  switch (PowerManagementMode) {
  case DPMSModeOn:
    /* Screen: On; HSync: On, VSync: On */
    break;
  case DPMSModeStandby:
    /* Screen: Off; HSync: Off, VSync: On */
    srd |= 0x10;
    break;
  case DPMSModeSuspend:
    /* Screen: Off; HSync: On, VSync: Off */
    srd |= 0x40;
    break;
  case DPMSModeOff:
    /* Screen: Off; HSync: Off, VSync: Off */
    srd |= 0x50;
    break;
  default:
    xf86ErrorFVerb(VERBLEV, "Invalid PowerManagementMode %d passed to S3VDisplayPowerManagementSet\n", PowerManagementMode);
    break;
  }

  VGAOUT8(0x3c4, 0x0d);
  VGAOUT8(0x3c5, srd);

  xf86ErrorFVerb(VERBLEV, "Power Manag: set:%s\n",
		 modestr[PowerManagementMode]);

  return;
}

/*EOF*/
