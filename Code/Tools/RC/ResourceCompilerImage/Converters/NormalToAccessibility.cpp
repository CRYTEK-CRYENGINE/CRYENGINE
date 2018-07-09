// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////

// convert DDNDIF (unoccluded area direction to accessibility map)
void ImageObject::DirectionToAccessibility()
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	for (uint32 dwMip = 0; dwMip < GetMipCount(); ++dwMip)
	{
		const uint32 dwLocalWidth = GetWidth(dwMip);   // we get error on NVidia with this (assumes input is 4x4 as well)
		const uint32 dwLocalHeight = GetHeight(dwMip);

		char* pInputMem;
		uint32 dwInputPitch;
		GetImagePointer(dwMip,pInputMem,dwInputPitch);

		const float fHalf = 128.0f / 255.0f;

		for (uint32 dwY = 0; dwY < dwLocalHeight; ++dwY)
		{
			float* src2 = (float*)&pInputMem[dwY * dwInputPitch];

			for (uint32 dwX = 0; dwX < dwLocalWidth; ++dwX, src2 += 4)
			{
				const float x = src2[0] - fHalf;
				const float y = src2[1] - fHalf;
				const float z = src2[2] - fHalf;

				const float fLength = Util::getMin(sqrtf(x * x + y * y + z * z ) * 2.0f, 1.0f);

				src2[0] = fLength;
				src2[1] = fLength;
				src2[2] = fLength;
			}
		}
	}
}
