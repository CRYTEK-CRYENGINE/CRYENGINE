// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   ZlibDecompressor.h
//  Created:     30/8/2012 by Axel Gneiting
//  Description: zlib inflate wrapper
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ZLIBDECOMPRESSOR_H__
#define __ZLIBDECOMPRESSOR_H__

#include <CrySystem/ZLib/IZlibDecompressor.h>

class CZLibDecompressor : public IZLibDecompressor
{
public:
	virtual IZLibInflateStream* CreateInflateStream();
	virtual void                Release();

private:
	virtual ~CZLibDecompressor() {}
};

#endif // __ZLIBDECOMPRESSOR_H__
