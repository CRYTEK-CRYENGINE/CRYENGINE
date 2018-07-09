// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   LZ4Decompressor.cpp
//  Created:     5/9/2012 by Axel Gneiting
//  Description: lz4 hc decompress wrapper
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <lz4.h>
#include "LZ4Decompressor.h"

bool CLZ4Decompressor::DecompressData(const char* pIn, char* pOut, const uint outputSize) const
{
	return LZ4_decompress_fast(pIn, pOut, outputSize) >= 0;
}

void CLZ4Decompressor::Release()
{
	delete this;
}
