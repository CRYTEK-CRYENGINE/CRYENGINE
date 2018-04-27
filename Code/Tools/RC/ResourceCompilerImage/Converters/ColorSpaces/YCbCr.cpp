// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../../ImageCompiler.h"            // CImageCompiler
#include "../../ImageObject.h"              // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "YCbCr.h"                          // YCbCr

void ImageToProcess::ConvertBetweenRGB32FAndYCbCr32F(bool bEncode)
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
			YCbCr* const pPixelsOut = pRet->GetPixelsPointer<YCbCr>(dwMip);

			const uint32 pixelCount = get()->GetPixelCount(dwMip);

			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixelsOut[i] = pPixelsIn[i];
			}
		}

		set(pRet.release());

		get()->RemoveImageFlags(CImageExtensionHelper::EIF_Colormodel);
		get()->AddImageFlags(CImageExtensionHelper::EIF_Colormodel_YCC);
	}
	else
	{
		assert((get()->GetImageFlags() & CImageExtensionHelper::EIF_Colormodel) == CImageExtensionHelper::EIF_Colormodel_YCC);

		std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A32B32G32R32F));

		const uint32 dwMips = pRet->GetMipCount();
		for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
		{
			const YCbCr* const pPixelsIn = get()->GetPixelsPointer<YCbCr>(dwMip);
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
