// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BMPHelper.h
//  Version:     v1.00
//  Created:     28/11/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: BMPHelper
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BMPHELPER_H__
#define __BMPHELPER_H__

#pragma once

namespace BMPHelper
{
// load a BMP. if pByteData is 0, only reports dimensions
// when pFile is given, restores read location after getting dimensions only
bool LoadBMP(const char* filename, uint8* pByteData, int& width, int& height, int& depth, bool bForceInverseY = false);
bool LoadBMP(FILE* pFile, uint8* pByteData, int& width, int& height, int& depth, bool bForceInverseY = false);

// save pByteData BGR[A] into a new file 'filename'. if bFlipY flips y.
// if depth==4 assumes BGRA
bool SaveBMP(const char* filename, uint8* pByteData, int width, int height, int depth, bool inverseY);
// save pByteData BGR[A] into a file pFile. if bFlipY flips y.
// if depth==4 assumes BGRA
bool   SaveBMP(FILE* pFile, uint8* pByteData, int width, int height, int depth, bool inverseY);
// calculate size of BMP incl. Header and padding bytes
size_t CalcBMPSize(int width, int height, int depth);
};

#endif
