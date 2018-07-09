// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>          // assert()

#include "../ImageObject.h"  // ImageObject, EPixelFormat

#include "IRCLog.h"          // RCLogError()


template <class T>
static void SwizzlePixels(T* pixels, const int channelCount, uint32 pixelCount, const T zero, const T one, const int* const pSwizzleIndices)
{
	T tmp[6] = { zero, zero, zero, one, zero, one };

	if (channelCount == 3)
	{
		while (pixelCount--)
		{
			tmp[0] = pixels[0];
			tmp[1] = pixels[1];
			tmp[2] = pixels[2];
			pixels[0] = tmp[pSwizzleIndices[0]];
			pixels[1] = tmp[pSwizzleIndices[1]];
			pixels[2] = tmp[pSwizzleIndices[2]];
			pixels += 3;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			tmp[0] = pixels[0];
			tmp[1] = pixels[1];
			tmp[2] = pixels[2];
			tmp[3] = pixels[3];
			pixels[0] = tmp[pSwizzleIndices[0]];
			pixels[1] = tmp[pSwizzleIndices[1]];
			pixels[2] = tmp[pSwizzleIndices[2]];
			pixels[3] = tmp[pSwizzleIndices[3]];
			pixels += 4;
		}
	}
	else
	{
		assert(0);
	}
}


bool ImageObject::Swizzle(const char* swizzle)
{
	if (swizzle == 0 || swizzle[0] == 0)
	{
		return true;
	}

	const EPixelFormat eFormat = GetPixelFormat();
	const PixelFormatInfo* const pFormatInfo = CPixelFormats::GetPixelFormatInfo(eFormat);

	if (!pFormatInfo || pFormatInfo->eSampleType == eSampleType_Compressed || pFormatInfo->nChannels < 3 || pFormatInfo->nChannels > 4)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return false;
	}

	if (strlen(swizzle) < pFormatInfo->nChannels)
	{
		assert(0);
		RCLogError("%s: bad number of channels in swizzle parameter: %s. Contact an RC programmer.", __FUNCTION__, swizzle);
		return false;
	}

	if (stricmp(swizzle, "rgba") == 0 || stricmp(swizzle, "rgb") == 0)
	{
		// No changes requested
		return true;
	}

	int swizzleIndices[4];
	{
		const bool bBgrOrder = eFormat == ePixelFormat_A8R8G8B8 || eFormat == ePixelFormat_X8R8G8B8 || eFormat == ePixelFormat_R8G8B8;

		for (int i = 0; i < pFormatInfo->nChannels; ++i)
		{
			switch (swizzle[i])
			{
			case 'r':
			case 'R':
				swizzleIndices[i] = bBgrOrder ? 2 : 0;
				break;
			case 'g':
			case 'G':
				swizzleIndices[i] = 1;
				break;
			case 'b':
			case 'B':
				swizzleIndices[i] = bBgrOrder ? 0 : 2;
				break;
			case 'a':
			case 'A':
				swizzleIndices[i] = 3;
				break;
			case '0':
				swizzleIndices[i] = 4;
				break;
			case '1':
				swizzleIndices[i] = 5;
				break;
			}
		}

		if (bBgrOrder)
		{
			std::swap(swizzleIndices[0], swizzleIndices[2]);
		}
	}

	for (uint32 mip = 0; mip < GetMipCount(); ++mip)
	{
		char *pMem;
		uint32 dwPitch;
		GetImagePointer(mip, pMem, dwPitch);

		if (pFormatInfo->eSampleType == eSampleType_Uint8)
		{
			::SwizzlePixels<uint8>((uint8*)pMem, pFormatInfo->nChannels, GetPixelCount(mip), 0, 255, swizzleIndices);
		}
		else if (pFormatInfo->eSampleType == eSampleType_Uint16)
		{
			::SwizzlePixels<uint16>((uint16*)pMem, pFormatInfo->nChannels, GetPixelCount(mip), 0, 0xffff, swizzleIndices);
		}
		else if (pFormatInfo->eSampleType == eSampleType_Half)
		{
			::SwizzlePixels<SHalf>(reinterpret_cast<SHalf*>(pMem), pFormatInfo->nChannels, GetPixelCount(mip), SHalf(0.0f), SHalf(1.0f), swizzleIndices);
		}
		else if (pFormatInfo->eSampleType == eSampleType_Float)
		{
			::SwizzlePixels<float>((float*)(void*)pMem, pFormatInfo->nChannels, GetPixelCount(mip), 0.0f, 1.0f, swizzleIndices);
		}
		else
		{
			assert(0);
			RCLogError("%s: unsupported sample type. Contact an RC programmer.", __FUNCTION__);
			return false;
		}
	}

	return true;
}
