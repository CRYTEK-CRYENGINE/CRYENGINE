// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include <CryCore/BitFiddling.h>                    // IntegerLog2()

///////////////////////////////////////////////////////////////////////////////////

// Convert from A8L8 to BGRA8L8
void ImageToProcess::ConvertFromAL8ToARGB8()
{
	const EPixelFormat srcPixelformat = get()->GetPixelFormat();
	assert(srcPixelformat == ePixelFormat_A8L8);

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A8R8G8B8));

	// transfer color range
	{
		Vec4 minColor, maxColor;

		get()->GetColorRange(minColor, maxColor);
		minColor.x = minColor.y = minColor.z = minColor.x; minColor.w = minColor.w;
		maxColor.x = maxColor.y = maxColor.z = maxColor.x; maxColor.w = maxColor.w;
		pRet->SetColorRange(minColor, maxColor);
	}

	const uint32 dwMips = pRet->GetMipCount();
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		Color2<uint8>* const pPixelsSrc = get()->GetPixelsPointer<Color2<uint8> >(dwMip);
		Color4<uint8>* const pPixelsDst = pRet->GetPixelsPointer<Color4<uint8> >(dwMip);

		const uint32 pixelCount = get()->GetPixelCount(dwMip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			pPixelsDst[i].components[0] = pPixelsSrc[i].components[0];
			pPixelsDst[i].components[1] = pPixelsSrc[i].components[0];
			pPixelsDst[i].components[2] = pPixelsSrc[i].components[0];
			pPixelsDst[i].components[3] = pPixelsSrc[i].components[1];
		}
	}

	// recursive for all attached images
	{
		const ImageObject* const pAttached = get()->GetAttachedImage();
		if (pAttached)
		{
			ImageObject* const p = pAttached->CopyImage();
			pRet->SetAttachedImage(p);
		}
	}

	set(pRet.release());
}

// Convert from BGR8, BGRA8L8, BGRX8 to A8L8
void ImageToProcess::ConvertFromAnyRGB8ToAL8()
{
	const EPixelFormat srcPixelformat = get()->GetPixelFormat();
	const EPixelFormat dstPixelformat = ePixelFormat_A8L8;

	if (srcPixelformat == dstPixelformat)
	{
		return;
	}

	assert(srcPixelformat == ePixelFormat_R8G8B8 ||
		srcPixelformat == ePixelFormat_A8R8G8B8 ||
		srcPixelformat == ePixelFormat_X8R8G8B8);

	ESampleType srcSampleType;
	int srcChannelCount;
	bool srcHasAlpha;

	if (!CPixelFormats::GetPixelFormatInfo(srcPixelformat, &srcSampleType, &srcChannelCount, &srcHasAlpha))
	{
		assert(0);		
		set(0);
		return;
	}

	if (srcSampleType != eSampleType_Uint8)
	{
		assert(0);		
		set(0);
		return;
	}

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A8L8));

	// transfer color range to grey image
	{
		Vec4 minColor, maxColor;

		get()->GetColorRange(minColor, maxColor);
		minColor.x = minColor.y = minColor.z = ((minColor.x * 11 + minColor.y * 50 + minColor.z * 39 + 100 / 2) / 100); minColor.w = minColor.w;
		maxColor.x = maxColor.y = maxColor.z = ((maxColor.x * 11 + maxColor.y * 50 + maxColor.z * 39 + 100 / 2) / 100); maxColor.w = maxColor.w;
		pRet->SetColorRange(minColor, maxColor);
	}

	const uint32 dwMips = pRet->GetMipCount();
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		const uint32 dwLocalWidth = get()->GetWidth(dwMip);
		const uint32 dwLocalHeight = get()->GetHeight(dwMip);

		char* pSrcMem;
		uint32 dwSrcPitch;
		get()->GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

		char* pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(dwMip, pDstMem, dwDstPitch);

		for (uint32 dwY = 0; dwY < dwLocalHeight; ++dwY)
		{
			const unsigned char* pSrc = (const unsigned char*) &pSrcMem[dwSrcPitch * dwY];
			unsigned char* pDst = (unsigned char*) &pDstMem[dwDstPitch * dwY];

			if (srcHasAlpha)
			{
				for (uint32 dwX = 0; dwX < dwLocalWidth; ++dwX)
				{
					pDst[0] = (unsigned char)((pSrc[0] * 11 + pSrc[1] * 50 + pSrc[2] * 39 + 100 / 2) / 100);  // [0] b, [1] g, [2] r
					pDst[1] = pSrc[3];
					pSrc += srcChannelCount;
					pDst += 2;
				}
			}
			else
			{
				memset(pDst, 255, dwLocalWidth);
			}
		}
	}

	// recursive for all attached images
	{
		const ImageObject* const pAttached = get()->GetAttachedImage();
		if (pAttached)
		{
			ImageObject* const p = pAttached->CopyImage();
			pRet->SetAttachedImage(p);
		}
	}

	set(pRet.release());

	get()->AddImageFlags(CImageExtensionHelper::EIF_Greyscale);
}
