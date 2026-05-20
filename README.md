  # xf86-video-s3virge-exa

  ## Overview
  
  This project is a fork of xf86-video-s3virge, which is a part of the Xorg project. It is a ddx video driver for S3 Virge/Trio graphics cards. Project's goal is to maintain the driver for compatibility with new xorg versions, and adding new features. 

  ## Supported hardware

  The s3virge driver for Xorg supports the S3 ViRGE, ViRGE DX, GX, GX2, MX, MX+, and VX chipsets. It also supports Trio3D and Trio3D/2x chips. A majority of testing is done on ViRGE DX chips, making them the most stable to date.

  ## Features

  - linear frame buffer

  - supported resolutions up to 2048x2048 (depends on card model)

  - supports color depths of 8, 15, 16 and 24

  - XVideo on DX, GX, GX2, MX, MX+ and Trio3D/2X at color depth 16 and 24 (accelerated)

  - RAM size, RAMDAC and ClockChip autodetection

  - pointer display acceleration (untested, propably unstable)

  - Doublescan modes on DX, possibly others (untested)

  # Authors of original xf86-video-s3virge driver

  - Mark Vojkovich  <mailto:mvojkovich@nvidia.com>

  - Sebastien Marineau

  - Harald Koenig  <mailto:koenig@tat.physik.uni-tuebingen.de>

  - Matt Grossman  <mailto:mattg@oz.net>

  - Kevin Brosius  <mailto:cobra@compuserve.com>

  # New maintainers:
  
  - kopi9999
