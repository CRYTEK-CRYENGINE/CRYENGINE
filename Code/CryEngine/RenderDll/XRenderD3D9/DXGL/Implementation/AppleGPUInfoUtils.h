// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   AppleGPUInfoUtils.h
//  Version:     v1.00
//  Created:     06/02/2014 by Leander Beernaert.
//  Description: Utitilities to access GPU info on Mac OS X and iOS
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __APPLEGPUINFOUTILS__
#define __APPLEGPUINFOUTILS__

// Return -1 on failure and availabe VRAM otherwise
long GetVRAMForDisplay(const int dspNum);

#endif
