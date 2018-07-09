// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../../ImageCompiler.h"            // CImageCompiler
#include "../../ImageObject.h"              // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "RGBL.h"                           // RGBL

void ImageObject::GetLuminanceInAlpha()
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	uint32 dwWidth, dwHeight, dwMips;

	GetExtent(dwWidth, dwHeight, dwMips);
	assert(dwMips == 1);

	char* pSrcMem;
	uint32 dwSrcPitch;
	GetImagePointer(0, pSrcMem, dwSrcPitch);

	for(uint32 dwY = 0; dwY < dwHeight; ++dwY)
	{				
		ColorF* const pPix = (ColorF*)&pSrcMem[dwY * dwSrcPitch];

		for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
		{		
			pPix[dwX].a = Util::getClamped(RGBL::GetLuminance(pPix[dwX]), 0.0f, 1.0f);
		}
	}
}
