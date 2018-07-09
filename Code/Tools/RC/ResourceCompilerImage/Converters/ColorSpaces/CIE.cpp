// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../../ImageCompiler.h"            // CImageCompiler
#include "../../ImageObject.h"              // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "CIE.h"                            // CIE

void ImageToProcess::ConvertBetweenRGBA32FAndCIE32F(bool bEncode)
{
	if (get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		set(0);
		return;
	}

	if (bEncode)
	{
		assert((get()->GetImageFlags() & CImageExtensionHelper::EIF_Colormodel) == CImageExtensionHelper::EIF_Colormodel_RGB);

		std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A32B32G32R32F));

		const uint32 dwMips = pRet->GetMipCount();
		for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
		{
			const ColorF* const pPixelsIn = get()->GetPixelsPointer<ColorF>(dwMip);
			CIE* const pPixelsOut = pRet->GetPixelsPointer<CIE>(dwMip);
			ColorF sumF = ColorF(0.0f, 0.0f, 0.0f, 1.0f);
			float minF = 1.0f;

			const uint32 pixelCount = get()->GetPixelCount(dwMip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = pPixelsIn[i];

				sumF += pPixelsIn[i];
				minF = min(minF, pPixelsOut[i].i);
			}

			// fill in black color with the chromaticities of the average color
			if (minF == 0.0f)
			{
				CIE avgF;
				avgF = sumF / pixelCount;

				for (uint32 i = 0; i < pixelCount; ++i)
				{
					if (pPixelsOut[i].i == 0.0f)
					{
						pPixelsOut[i].r = avgF.r;
						pPixelsOut[i].b = avgF.b;
					}
				}
			}
		}

		set(pRet.release());

		get()->RemoveImageFlags(CImageExtensionHelper::EIF_Colormodel);
		get()->AddImageFlags(CImageExtensionHelper::EIF_Colormodel_CIE);
	}
	else
	{
		assert((get()->GetImageFlags() & CImageExtensionHelper::EIF_Colormodel) == CImageExtensionHelper::EIF_Colormodel_CIE);

		std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A32B32G32R32F));

		const uint32 dwMips = pRet->GetMipCount();
		for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
		{
			const CIE* const pPixelsIn = get()->GetPixelsPointer<CIE>(dwMip);
			ColorF* const pPixelsOut = pRet->GetPixelsPointer<ColorF>(dwMip);

			const uint32 pixelCount = get()->GetPixelCount(dwMip);

			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = (ColorF)pPixelsIn[i];
			}
		}

		set(pRet.release());

		get()->RemoveImageFlags(CImageExtensionHelper::EIF_Colormodel);
		get()->AddImageFlags(CImageExtensionHelper::EIF_Colormodel_RGB);
	}
}
