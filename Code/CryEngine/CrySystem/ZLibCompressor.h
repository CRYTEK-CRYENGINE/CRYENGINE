// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** ZLibCompressor.h
** 23/6/10
******************************************************************************/

#ifndef __ZLIBCOMPRESSOR_H__
#define __ZLIBCOMPRESSOR_H__

#include <CrySystem/ZLib/IZLibCompressor.h>

class CZLibCompressor : public IZLibCompressor
{
protected:
	virtual ~CZLibCompressor();

public:
	virtual IZLibDeflateStream* CreateDeflateStream(int inLevel, EZLibMethod inMethod, int inWindowBits, int inMemLevel, EZLibStrategy inStrategy, EZLibFlush inFlushMethod);
	virtual void                Release();

	virtual void                MD5Init(SMD5Context* pIOCtx);
	virtual void                MD5Update(SMD5Context* pIOCtx, const char* pInBuff, unsigned int len);
	virtual void                MD5Final(SMD5Context* pIOCtx, char outDigest[16]);
};

#endif // __ZLIBCOMPRESSOR_H__
