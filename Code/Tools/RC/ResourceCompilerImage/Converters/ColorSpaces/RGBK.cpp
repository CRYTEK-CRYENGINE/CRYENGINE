// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../../ImageCompiler.h"            // CImageCompiler
#include "../../ImageObject.h"              // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "RGBK.h"                           // RGBK

void ImageToProcess::CompressRGB32FToRGBK8(const CImageProperties* pProps, int rgbkMaxValue, bool bDXTCompressedAlpha, int rgbkType)
{
	if (get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		set(0);
		return;
	}

	if (rgbkType < 1 || rgbkType > 3)
	{
		assert(0);
		RCLogError("%s: unsupported RGBK type. Contact an RC programmer.", __FUNCTION__);
		set(0);
		return;
	}

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A8R8G8B8));

	for (uint32 dwMip = 0; dwMip < pRet->GetMipCount(); ++dwMip)
	{
		const ColorF* const pPixelsIn = get()->GetPixelsPointer<ColorF>(dwMip);
		ColorB* const pPixelsOut = pRet->GetPixelsPointer<ColorB>(dwMip);

		const uint32 pixelCount = get()->GetPixelCount(dwMip);

		if (rgbkType == 1)
		{
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = RGBK::Compress(pPixelsIn[i], rgbkMaxValue, false);
			}
		}
		else
		{
			const bool bSparseK = (rgbkType == 3);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = RGBK::CompressSquared(pPixelsIn[i], rgbkMaxValue, bSparseK);
			}
		}
	}

	if (!bDXTCompressedAlpha)
	{
		{
			ImageToProcess tmp(pRet.release()); 
			tmp.get()->CopyPropertiesFrom(*get());

			// make color white to speed-up DXT5 compression (we are interested in alpha channel only)
			tmp.get()->Swizzle("111a");

			// perform compression and decompression to get actual alpha values
			tmp.ConvertFormat(pProps, ePixelFormat_DXT5);
			tmp.ConvertFormat(pProps, ePixelFormat_A8R8G8B8);

			pRet = std::auto_ptr<ImageObject>(tmp.get());
			tmp.forget();
		}

		// re-compute RGBK by using actual alpha values

		// Our mipmapper doesn't produce 1x1 and 2x2 mips for DXT5,
		// so let's avoid accessing them.
		const uint32 dwMips = Util::getMin(pRet->GetMipCount(), get()->GetMipCount());

		for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
		{
			const ColorF* const pPixelsIn = get()->GetPixelsPointer<ColorF>(dwMip);
			ColorB* const pPixelsOut = pRet->GetPixelsPointer<ColorB>(dwMip);

			const uint32 pixelCount = get()->GetPixelCount(dwMip);

			if (rgbkType == 1)
			{
				for (uint32 i = 0; i < pixelCount; ++i)
				{
					const uint8 forcedK = Util::getMax(pPixelsOut[i].a, (uint8)1);
					pPixelsOut[i] = RGBK::Compress(pPixelsIn[i], rgbkMaxValue, forcedK);
				}
			}
			else
			{
				for (uint32 i = 0; i < pixelCount; ++i)
				{
					const uint8 forcedK = Util::getMax(pPixelsOut[i].a, (uint8)1);
					pPixelsOut[i] = RGBK::CompressSquared(pPixelsIn[i], rgbkMaxValue, forcedK);
				}
			}
		}
	}

	set(pRet.release());
}
