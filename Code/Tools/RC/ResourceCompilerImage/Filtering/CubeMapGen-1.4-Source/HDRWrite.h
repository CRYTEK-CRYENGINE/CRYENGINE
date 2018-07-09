// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//HDRWrite.h
// Routines for writing .HDR images.. included to work around D3DX .hdr writing bug
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef HDRREADWRITE_H
#define HDRREADWRITE_H

#include <stdio.h>
#include "Types.h"

// Write HDR file header
void HDR_WriteHeader(FILE *fp, int32 a_Width, int32 a_Height);

// Write out HDR data
void HDR_WritePixels(FILE *fp, float32 *a_RawData, int32 a_NumPixels);


#endif  //HDRREADWRITE_H



