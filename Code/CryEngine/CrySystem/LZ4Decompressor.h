// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   LZ4Decompressor.h
//  Created:     5/9/2012 by Axel Gneiting
//  Description: lz4 hc decompress wrapper
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __LZ4DECOMPRESSOR_H__
#define __LZ4DECOMPRESSOR_H__

#include <CrySystem/ZLib/ILZ4Decompressor.h>

class CLZ4Decompressor : public ILZ4Decompressor
{
public:
	virtual bool DecompressData(const char* pIn, char* pOut, const uint outputSize) const;
	virtual void Release();

private:
	virtual ~CLZ4Decompressor() {}
};

#endif // __LZ4DECOMPRESSOR_H__
