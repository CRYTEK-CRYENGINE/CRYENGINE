// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   IZlibDecompressor.h
//  Created:     5/9/2013 by Axel Gneiting
//  Description: Provides the interface for the lz4 hc decompress wrapper
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ILZ4DECOMPRESSOR_H__
#define __ILZ4DECOMPRESSOR_H__

struct ILZ4Decompressor
{
protected:
	virtual ~ILZ4Decompressor() {}   //!< Use Release().

public:
	virtual bool DecompressData(const char* pIn, char* pOut, const uint outputSize) const = 0;

	virtual void Release() = 0;
};

#endif // __ILZ4DECOMPRESSOR_H__
