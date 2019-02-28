// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include <CryCore/BitFiddling.h>                    // IntegerLog2()

///////////////////////////////////////////////////////////////////////////////////

// Convert from A8 to BGRA8
void ImageToProcess::ConvertFromA8ToARGB8()
{
	const EPixelFormat srcPixelformat = get()->GetPixelFormat();
	assert(srcPixelformat == ePixelFormat_A8);

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A8R8G8B8));

	// transfer color range
	{
		Vec4 minColor, maxColor;

		get()->GetColorRange(minColor, maxColor);
		minColor.x = minColor.y = minColor.w = 1.0f; minColor.w = minColor.w;
		maxColor.x = maxColor.y = maxColor.w = 1.0f; maxColor.w = maxColor.w;
		pRet->SetColorRange(minColor, maxColor);
	}

	const uint32 dwMips = pRet->GetMipCount();
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		const uint8* const pPixelsSrc = get()->GetPixelsPointer<uint8>(dwMip);
		Color4<uint8>* const pPixelsDst = pRet->GetPixelsPointer<Color4<uint8> >(dwMip);

		const uint32 pixelCount = get()->GetPixelCount(dwMip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			pPixelsDst[i].components[0] = 0xFF;
			pPixelsDst[i].components[1] = 0xFF;
			pPixelsDst[i].components[2] = 0xFF;
			pPixelsDst[i].components[3] = pPixelsSrc[i];
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

// Convert from BGR8, BGRA8, BGRX8 to A8
void ImageToProcess::ConvertFromAnyRGB8ToA8()
{
	const EPixelFormat srcPixelformat = get()->GetPixelFormat();
	const EPixelFormat dstPixelformat = ePixelFormat_A8;

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

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A8));

	// transfer color range
	{
		Vec4 minColor, maxColor;

		get()->GetColorRange(minColor, maxColor);
		minColor.w = minColor.w; minColor.x = minColor.y = minColor.z = 0.0f;
		maxColor.w = maxColor.w; maxColor.x = maxColor.y = maxColor.z = 0.0f;
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
			const unsigned char* pSrc = (const unsigned char*)&pSrcMem[dwSrcPitch * dwY];
			unsigned char* pDst = (unsigned char*)&pDstMem[dwDstPitch * dwY];

			if (srcHasAlpha)
			{
				for (uint32 dwX = 0; dwX < dwLocalWidth; ++dwX)
				{
					pDst[0] = pSrc[3];
					pSrc += srcChannelCount;
					++pDst;
				}
			}
			else
			{
				memset(pDst, 0xFF, dwLocalWidth);
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
}

///////////////////////////////////////////////////////////////////////////////////

// clone this image-object's alpha channel, A32B32G32R32F/A16B16G16R16F/A16B16G16R16/A8R8G8B8/X8R8G8B8
ImageObject* ImageObject::CopyAnyAlphaIntoA8Image(int maxMipCount) const
{
	uint32 width, height, mipCount;
	GetExtent(width, height, mipCount);

	const EPixelFormat pixelFormat = GetPixelFormat();

	if (pixelFormat != ePixelFormat_X8R8G8B8 &&
	    pixelFormat != ePixelFormat_A8R8G8B8 &&
	    pixelFormat != ePixelFormat_A16B16G16R16 &&
	    pixelFormat != ePixelFormat_A16B16G16R16F &&
	    pixelFormat != ePixelFormat_A32B32G32R32F)
	{
		return 0;
	}

	uint32 newMipCount = Util::getMin(uint32(maxMipCount), mipCount);
	std::auto_ptr<ImageObject> pRet(new ImageObject(width, height, newMipCount, ePixelFormat_A8, GetCubemap()));
	assert(pRet->GetMipCount() == newMipCount);

	// transfer color range
	{
		Vec4 minColor, maxColor;

		GetColorRange(minColor, maxColor);
		minColor.w = minColor.w; minColor.x = minColor.y = minColor.z = 0.0f;
		maxColor.w = maxColor.w; maxColor.x = maxColor.y = maxColor.z = 0.0f;
		pRet->SetColorRange(minColor, maxColor);
	}

	for (uint32 mip = 0; mip < newMipCount; ++mip)
	{
		char* pSrc = 0;
		uint32 srcPitch;
		GetImagePointer(mip, pSrc, srcPitch);												
		assert(pSrc);

		char* pDst = 0;
		uint32 dstPitch;
		pRet->GetImagePointer(mip, pDst, dstPitch);		
		assert(pDst);

		const uint32 localWidth = Util::getMax(1U, width >> mip);
		const uint32 localHeight = Util::getMax(1U, height >> mip);

		if (pixelFormat == ePixelFormat_A32B32G32R32F)
		{
			for (uint32 y = 0; y < localHeight; ++y)
			{
				const float* pSrcLine = ((const float*)&pSrc[srcPitch * y]) + 3;
				uint8* pDstLine = (uint8*)&pDst[dstPitch * y];

				for (uint32 x = 0; x < localWidth; ++x)
				{
					*pDstLine++ = (uint8)Util::getClamped((*pSrcLine) * 255.0f + 0.5f, 0.0f, 255.0f);
					pSrcLine += 4;
				}
			}
		}
		else if (pixelFormat == ePixelFormat_A16B16G16R16F)
		{
			for (uint32 y = 0; y < localHeight; ++y)
			{
				const SHalf* pSrcLine = ((const SHalf*)&pSrc[srcPitch * y]) + 3;
				uint8* pDstLine = (uint8*)&pDst[dstPitch * y];

				for (uint32 x = 0; x < localWidth; ++x)
				{
					*pDstLine++ = (uint8)Util::getClamped((*pSrcLine) * 255.0f + 0.5f, 0.0f, 255.0f);
					pSrcLine += 4;
				}
			}
		}
		else if (pixelFormat == ePixelFormat_A16B16G16R16)
		{
			for (uint32 y = 0; y < localHeight; ++y)
			{
				const uint16* pSrcLine = ((const uint16*)&pSrc[srcPitch * y]) + 3;
				uint8* pDstLine = (uint8*)&pDst[dstPitch * y];

				for (uint32 x = 0; x < localWidth; ++x)
				{
					*pDstLine++ = uint8(*pSrcLine >> 8);
					pSrcLine += 4;
				}
			}
		}
		else if (pixelFormat == ePixelFormat_A8R8G8B8 || pixelFormat == ePixelFormat_X8R8G8B8)
		{
			for (uint32 y = 0; y < localHeight; ++y)
			{
				const uint8* pSrcLine = ((const uint8*)&pSrc[srcPitch * y]) + 3;
				uint8* pDstLine = (uint8*)&pDst[dstPitch * y];

				for (uint32 x = 0; x < localWidth; ++x)
				{
					*pDstLine++ = *pSrcLine;
					pSrcLine += 4;
				}
			}	
		}
	}

	return pRet.release();
}

// copy the alpha channel out of the given image, A32B32G32R32F/A16B16G16R16F/A16B16G16R16/A8R8G8B8
void ImageObject::TakeoverAnyAlphaFromA8Image(const ImageObject* pImage)
{
	assert(pImage);

	const EPixelFormat pixelFormat = GetPixelFormat();

	if (pImage->GetPixelFormat() != ePixelFormat_A8)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	if (pixelFormat != ePixelFormat_A8R8G8B8 &&
	    pixelFormat != ePixelFormat_A16B16G16R16 &&
	    pixelFormat != ePixelFormat_A16B16G16R16F &&
	    pixelFormat != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported destination format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	uint32 srcWidth, srcHeight, srcMips;
	pImage->GetExtent(srcWidth, srcHeight, srcMips);

	uint32 dstWidth, dstHeight, dstMips;
	GetExtent(dstWidth, dstHeight, dstMips);

	// input image allowed to have same or lower resolution only
	if (srcWidth > dstWidth || srcHeight > dstHeight)
	{
		assert(0);
		RCLogError("%s: setting alpha from *bigger* image is not supported. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	// transfer color range to alpha
	{
		Vec4 minColorA, maxColorA;
		Vec4 minColorB, maxColorB;

		GetColorRange(minColorA, maxColorA);
		pImage->GetColorRange(minColorB, maxColorB);
		minColorA.w = minColorB.w;
		maxColorA.w = maxColorB.w;
		SetColorRange(minColorA, maxColorA);
	}

	// Check that every mip of the image has corresponding alpha image
	{
		const uint32 minSrcWidth  = pImage->GetWidth(srcMips - 1);
		const uint32 minSrcHeight = pImage->GetHeight(srcMips - 1);
		const uint32 minDstWidth  = GetWidth(dstMips - 1);
		const uint32 minDstHeight = GetHeight(dstMips - 1);

		if (minSrcWidth > minDstWidth || minSrcHeight > minDstHeight)
		{
			assert(0);
			RCLogError("%s: alpha mip chain is too short. Contact an RC programmer.", __FUNCTION__);
			return;
		}
	}

	for (uint32 dstMip = 0; dstMip < dstMips; ++dstMip)
	{
		const uint32 localDstWidth  = GetWidth(dstMip);
		const uint32 localDstHeight = GetHeight(dstMip);

		// Choose appropriate mip of the alpha
		uint32 srcMip = 0;
		uint32 localSrcWidth  = 0;
		uint32 localSrcHeight = 0;
		for (;;)
		{
			localSrcWidth  = pImage->GetWidth(srcMip);
			localSrcHeight = pImage->GetHeight(srcMip);
			if (localSrcWidth <= localDstWidth && localSrcHeight <= localDstHeight)
			{
				break;
			}
			++srcMip;
		}

		const uint32 reduceWidth  = IntegerLog2(localDstWidth  / localSrcWidth);
		const uint32 reduceHeight = IntegerLog2(localDstHeight / localSrcHeight);

		char* pSrc = 0;
		uint32 srcPitch;
		pImage->GetImagePointer(srcMip, pSrc, srcPitch);
		assert(pSrc);

		char* pDst = 0;
		uint32 dstPitch;
		GetImagePointer(dstMip, pDst, dstPitch);
		assert(pDst);

		if (pixelFormat == ePixelFormat_A32B32G32R32F)
		{
			for (uint32 y = 0; y < localDstHeight; ++y)
			{
				float* pDstLine = (float*)&pDst[dstPitch * y];
				const uint8* const pSrcLine = (const uint8*)&pSrc[srcPitch * (y >> reduceHeight)];
				for (uint32 x = 0; x < localDstWidth; ++x)
				{
					const uint8 alphaValue = pSrcLine[x >> reduceWidth];
					const float alphaValueAsFloat = alphaValue / 255.0f;

					*pDstLine = alphaValueAsFloat;
					pDstLine += 4;
				}
			}	
		}
		else if (pixelFormat == ePixelFormat_A16B16G16R16F)
		{
			for (uint32 y = 0; y < localDstHeight; ++y)
			{
				SHalf* pDstLine = (SHalf*)&pDst[dstPitch * y];
				const uint8* const pSrcLine = (const uint8*)&pSrc[srcPitch * (y >> reduceHeight)];
				for (uint32 x = 0; x < localDstWidth; ++x)
				{
					const uint8 alphaValue = pSrcLine[x >> reduceWidth];
					const SHalf alphaValueAsHalf = SHalf(alphaValue / 255.0f);

					*pDstLine = alphaValueAsHalf;
					pDstLine += 4;
				}
			}
		}
		else if (pixelFormat == ePixelFormat_A16B16G16R16)
		{
			for (uint32 y = 0; y < localDstHeight; ++y)
			{
				uint16* pDstLine = (uint16*)&pDst[dstPitch * y];
				const uint8* const pSrcLine = (const uint8*)&pSrc[srcPitch * (y >> reduceHeight)];
				for (uint32 x = 0; x < localDstWidth; ++x)
				{
					const uint8 alphaValue = pSrcLine[x >> reduceWidth];
					const uint16 alphaValueAsShort = alphaValue * 0x0101;

					*pDstLine = alphaValueAsShort;
					pDstLine += 4;
				}
			}
		}
		else if (pixelFormat == ePixelFormat_A8R8G8B8)
		{
			for (uint32 y = 0; y < localDstHeight; ++y)
			{
				uint32* pDstLine = (uint32*)&pDst[dstPitch * y];
				const uint8* const pSrcLine = (const uint8*)&pSrc[srcPitch * (y >> reduceHeight)];
				for (uint32 x = 0; x < localDstWidth; ++x)
				{
					const uint8 alphaValue = pSrcLine[x >> reduceWidth];
					const uint32 argbValue = ((*pDstLine) & 0x00ffffffUL) | (((uint32)alphaValue) << 24);

					*pDstLine = argbValue;
					pDstLine += 1;
				}
			}	
		}
	}
}
