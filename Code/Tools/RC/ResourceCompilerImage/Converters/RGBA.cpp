// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////

void ImageObject::ValidateFloatAlpha() const
{
	assert(ePixelFormat_A32B32G32R32F == GetPixelFormat());

	for (uint32 mip = 0; mip < GetMipCount(); ++mip)
	{
		const Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);

		const uint32 pixelCount = GetPixelCount(mip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			assert(pPixels[i].components[3] >= 0);
			assert(pPixels[i].components[3] <= 1);
		}
	}
}

void ImageObject::ClampMinimumAlpha(float minAlpha)
{
	assert(ePixelFormat_A32B32G32R32F == GetPixelFormat());

	assert(minAlpha >= 0.0f);
	assert(minAlpha <= 255.0f);

	for (uint32 mip = 0; mip < GetMipCount(); ++mip)
	{
		Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);

		const uint32 pixelCount = GetPixelCount(mip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			Util::clampMin(pPixels[i].components[3], minAlpha);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////

bool ImageObject::SetConstantAlpha(const int alphaValue) 
{
	const EPixelFormat srcFormat = GetPixelFormat();
	if (CPixelFormats::IsFormatWithoutAlpha(srcFormat) || !CPixelFormats::IsPixelFormatUncompressed(srcFormat))
	{
		return false;
	}

	for (uint32 mip = 0; mip < GetMipCount(); ++mip)
	{
		const uint32 pixelCount = GetPixelCount(mip);
		if (srcFormat == ePixelFormat_A8)
		{
			uint8* const pPixels = GetPixelsPointer<uint8>(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i] = alphaValue;
			}
		}
		else if (srcFormat == ePixelFormat_A8L8)
		{
			Color2<uint8>* const pPixels = GetPixelsPointer<Color2<uint8> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[1] = alphaValue;
			}
		}
		else if (srcFormat == ePixelFormat_A8R8G8B8 || srcFormat == ePixelFormat_X8R8G8B8)
		{
			Color4<uint8>* const pPixels = GetPixelsPointer<Color4<uint8> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = alphaValue;
			}
		}
		else if (srcFormat == ePixelFormat_A16B16G16R16)
		{
			const uint32 alphaValueAsShort = alphaValue * 0x0101;

			Color4<uint16>* const pPixels = GetPixelsPointer<Color4<uint16> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = alphaValueAsShort;
			}
		}
		else if (srcFormat == ePixelFormat_A16B16G16R16F)
		{
			const SHalf alphaValueAsHalf = SHalf(alphaValue / 255.0f);

			Color4<SHalf>* const pPixels = GetPixelsPointer<Color4<SHalf> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = alphaValueAsHalf;
			}
		}
		else if (srcFormat == ePixelFormat_A32B32G32R32F)
		{
			const float alphaValueAsFloat = alphaValue / 255.0f;

			Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = alphaValueAsFloat;
			}
		}
	}

	return true;
}

bool ImageObject::ThresholdAlpha(const int alphaThresholdValue) 
{
	const EPixelFormat srcFormat = GetPixelFormat();
	if (CPixelFormats::IsFormatWithoutAlpha(srcFormat) || !CPixelFormats::IsPixelFormatUncompressed(srcFormat))
	{
		return false;
	}

	for (uint32 mip = 0; mip < GetMipCount(); ++mip)
	{
		const uint32 pixelCount = GetPixelCount(mip);
		if (srcFormat == ePixelFormat_A8)
		{
			uint8* const pPixels = GetPixelsPointer<uint8>(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i] = (pPixels[i] < alphaThresholdValue) ? 0 : 0xFF;
			}
		}
		else if (srcFormat == ePixelFormat_A8L8)
		{
			Color2<uint8>* const pPixels = GetPixelsPointer<Color2<uint8> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[1] = (pPixels[i].components[1] < alphaThresholdValue) ? 0 : 0xFF;
			}
		}
		else if (srcFormat == ePixelFormat_A8R8G8B8 || srcFormat == ePixelFormat_X8R8G8B8)
		{
			Color4<uint8>* const pPixels = GetPixelsPointer<Color4<uint8> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = (pPixels[i].components[3] < alphaThresholdValue) ? 0 : 0xFF;
			}
		}
		else if (srcFormat == ePixelFormat_A16B16G16R16)
		{
			const float alphaThresholdValueAsShort = alphaThresholdValue * 0x0101;

			Color4<uint16>* const pPixels = GetPixelsPointer<Color4<uint16> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = (pPixels[i].components[3] < alphaThresholdValueAsShort) ? 0 : 0xFFFF;
			}
		}
		else if (srcFormat == ePixelFormat_A16B16G16R16F)
		{
			const float alphaThresholdValueAsFloat = alphaThresholdValue / 255.0f;
			const SHalf halfZero = SHalf(0.0f);
			const SHalf halfOne = SHalf(1.0f);

			Color4<SHalf>* const pPixels = GetPixelsPointer<Color4<SHalf> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = (pPixels[i].components[3] < alphaThresholdValueAsFloat) ? halfZero : halfOne;
			}
		}
		else if (srcFormat == ePixelFormat_A32B32G32R32F)
		{
			const float alphaThresholdValueAsFloat = alphaThresholdValue / 255.0f;

			Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[3] = (pPixels[i].components[3] < alphaThresholdValueAsFloat) ? 0.0f : 1.0f;
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////

template<class UN, class FP>
static void ConvertUnormToFloat(int pixelCount, int channelCount, const void* const pSrc, std::vector<uint8>& dst)
{
	static const float UNone = float(UN(~0));

	assert(pSrc);
	assert(pixelCount > 0);
	assert((channelCount == 2) || (channelCount == 3) || (channelCount == 4));

	// convert unsigned norm to float
	const UN* pSrcMem = (const UN*)pSrc;

	if (dst.empty())
	{
		dst.resize(pixelCount * channelCount * sizeof(FP));
	}

	FP* pDstMem = (FP*)&dst[0];

	if (channelCount == 2)
	{
		while (pixelCount--)
		{
			pDstMem[0] = FP(pSrcMem[0] / UNone);
			pDstMem[1] = FP(pSrcMem[1] / UNone);				
			pSrcMem += 2;
			pDstMem += 2;
		}
	}
	else if (channelCount == 3)
	{
		while (pixelCount--)
		{
			pDstMem[0] = FP(pSrcMem[0] / UNone);
			pDstMem[1] = FP(pSrcMem[1] / UNone);				
			pDstMem[2] = FP(pSrcMem[2] / UNone);				
			pSrcMem += 3;
			pDstMem += 3;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDstMem[0] = FP(pSrcMem[0] / UNone);
			pDstMem[1] = FP(pSrcMem[1] / UNone);
			pDstMem[2] = FP(pSrcMem[2] / UNone);				
			pDstMem[3] = FP(pSrcMem[3] / UNone);				
			pSrcMem += 4;
			pDstMem += 4;
		}
	}
}

template<class FP, class UN>
static void ConvertFloatToUnorm(int pixelCount, int channelCount, const void* const pSrc, std::vector<uint8>& dst)
{
	static const float UNone = float(UN(~0));

	assert(pSrc);
	assert(pixelCount > 0);
	assert((channelCount == 2) || (channelCount == 3) || (channelCount == 4));

	// convert float to unsigned norm
	const FP* pSrcMem = static_cast<const FP*>(pSrc);

	if (dst.empty())
	{
		dst.resize(pixelCount * channelCount * sizeof(UN));
	}

	UN* pDstMem = (UN*)&dst[0];

	if (channelCount == 2)
	{
		while (pixelCount--)
		{
			pDstMem[0] = UN(Util::getClamped(pSrcMem[0] * UNone + 0.5f, 0.0f, UNone));
			pDstMem[1] = UN(Util::getClamped(pSrcMem[1] * UNone + 0.5f, 0.0f, UNone));
			pSrcMem += 2;
			pDstMem += 2;
		}
	}
	else if (channelCount == 3)
	{
		while (pixelCount--)
		{
			pDstMem[0] = UN(Util::getClamped(pSrcMem[0] * UNone + 0.5f, 0.0f, UNone));
			pDstMem[1] = UN(Util::getClamped(pSrcMem[1] * UNone + 0.5f, 0.0f, UNone));
			pDstMem[2] = UN(Util::getClamped(pSrcMem[2] * UNone + 0.5f, 0.0f, UNone));
			pSrcMem += 3;
			pDstMem += 3;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDstMem[0] = UN(Util::getClamped(pSrcMem[0] * UNone + 0.5f, 0.0f, UNone));
			pDstMem[1] = UN(Util::getClamped(pSrcMem[1] * UNone + 0.5f, 0.0f, UNone));
			pDstMem[2] = UN(Util::getClamped(pSrcMem[2] * UNone + 0.5f, 0.0f, UNone));
			pDstMem[3] = UN(Util::getClamped(pSrcMem[3] * UNone + 0.5f, 0.0f, UNone));
			pSrcMem += 4;
			pDstMem += 4;
		}
	}
}

template<class UNIN, class UNOUT>
static void ConvertUnormToUnorm(int pixelCount, int channelCount, const void* const pSrc, std::vector<uint8>& dst)
{
	// 65535 / 65535 = 1
	// 65535 /   255 = 257 / 1
	//   255 / 65535 = 1 / 257
	//   255 /   255 = 1
	static const unsigned long UNINone  = UNIN (~0);
	static const unsigned long UNOUTone = UNOUT(~0);

	assert(pSrc);
	assert(pixelCount > 0);
	assert((channelCount == 2) || (channelCount == 3) || (channelCount == 4));

	// convert float to float
	const UNIN* pSrcMem = (const UNIN*)pSrc;

	if (dst.empty())
	{
		dst.resize(pixelCount * channelCount * sizeof(UNOUT));
	}

	UNOUT* pDstMem = (UNOUT*)&dst[0];

	if (channelCount == 2)
	{
		while (pixelCount--)
		{
			pDstMem[0] = UNOUT(pSrcMem[0] * UNOUTone / UNINone);
			pDstMem[1] = UNOUT(pSrcMem[1] * UNOUTone / UNINone);
			pSrcMem += 2;
			pDstMem += 2;
		}
	}
	else if (channelCount == 3)
	{
		while (pixelCount--)
		{
			pDstMem[0] = UNOUT(pSrcMem[0] * UNOUTone / UNINone);
			pDstMem[1] = UNOUT(pSrcMem[1] * UNOUTone / UNINone);
			pDstMem[2] = UNOUT(pSrcMem[2] * UNOUTone / UNINone);
			pSrcMem += 3;
			pDstMem += 3;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDstMem[0] = UNOUT(pSrcMem[0] * UNOUTone / UNINone);
			pDstMem[1] = UNOUT(pSrcMem[1] * UNOUTone / UNINone);
			pDstMem[2] = UNOUT(pSrcMem[2] * UNOUTone / UNINone);
			pDstMem[3] = UNOUT(pSrcMem[3] * UNOUTone / UNINone);
			pSrcMem += 4;
			pDstMem += 4;
		}
	}
}

template<class FPIN, class FPOUT>
static void ConvertFloatToFloat(int pixelCount, int channelCount, const void* const pSrc, std::vector<uint8>& dst)
{
	assert(pSrc);
	assert(pixelCount > 0);
	assert(channelCount == 4);

	// convert float to float
	const FPIN* pSrcMem = static_cast<const FPIN*>(pSrc);

	if (dst.empty())
	{
		dst.resize(pixelCount * channelCount * sizeof(FPOUT));
	}

	FPOUT* pDstMem = (FPOUT*)&dst[0];

	if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDstMem[0] = FPOUT(pSrcMem[0]);
			pDstMem[1] = FPOUT(pSrcMem[1]);
			pDstMem[2] = FPOUT(pSrcMem[2]);
			pDstMem[3] = FPOUT(pSrcMem[3]);
			pSrcMem += 4;
			pDstMem += 4;
		}
	}
}


template <class T, const bool swap>
static void ConvertChannels(const T& maxAlphaValue, int pixelCount, const T* pSrc, int srcChannelCount, bool srcHasAlpha, T* pDst, int dstChannelCount, bool dstHasAlpha)
{
	if (srcChannelCount == dstChannelCount)
	{
		memcpy(pDst, pSrc, pixelCount * srcChannelCount * sizeof(T));

		if ((dstChannelCount == 4) && (srcHasAlpha != dstHasAlpha))
		{
			while (pixelCount--)
			{
				if (swap)
				{
					T   tmp = pDst[0];
					pDst[0] = pDst[2];
					pDst[2] = tmp;
				}

				pDst[3] = maxAlphaValue;
				pDst += 4;
			}
		}

		else if ((dstChannelCount == 4) || (dstChannelCount == 3))
		{
			while (pixelCount--)
			{
				if (swap)
				{
					T   tmp = pDst[0];
					pDst[0] = pDst[2];
					pDst[2] = tmp;
				}

				pDst += dstChannelCount;
			}
		}
	}
	else
	{
		static const T zero(0);

		if (srcChannelCount == 2 && dstChannelCount == 3)
		{
			while (pixelCount--)
			{
				pDst[0 + swap * 2] = pSrc[0];
				pDst[1           ] = pSrc[1];
				pDst[2 - swap * 2] = zero;
				pSrc += 2;
				pDst += 3;
			}
		}
		else if (srcChannelCount == 2 && dstChannelCount == 4)
		{
			while (pixelCount--)
			{
				pDst[0 + swap * 2] = pSrc[0];
				pDst[1           ] = pSrc[1];
				pDst[2 - swap * 2] = zero;
				pDst[3           ] = maxAlphaValue;
				pSrc += 2;
				pDst += 4;
			}
		}
		else if (srcChannelCount == 3 && dstChannelCount == 2)
		{
			while (pixelCount--)
			{
				pDst[0] = pSrc[0 + swap * 2];
				pDst[1] = pSrc[1           ];
				pSrc += 3;
				pDst += 2;
			}
		}
		else if (srcChannelCount == 3 && dstChannelCount == 4)
		{
			while (pixelCount--)
			{
				pDst[0 + swap * 2] = pSrc[0];
				pDst[1           ] = pSrc[1];
				pDst[2 - swap * 2] = pSrc[2];
				pDst[3           ] = maxAlphaValue;
				pSrc += 3;
				pDst += 4;
			}
		}
		else if (srcChannelCount == 4 && dstChannelCount == 2)
		{
			while (pixelCount--)
			{
				pDst[0] = pSrc[0 + swap * 2];
				pDst[1] = pSrc[1           ];
				pSrc += 4;
				pDst += 2;
			}
		}
		else if (srcChannelCount == 4 && dstChannelCount == 3)
		{
			while (pixelCount--)
			{
				pDst[0] = pSrc[0 + swap * 2];
				pDst[1] = pSrc[1           ];
				pDst[2] = pSrc[2 - swap * 2];
				pSrc += 4;
				pDst += 3;
			}
		}
	}
}

// Convert between GR8, BGR8, BGRA8, BGRX8, RG16H, RGBA16H, RG32F, RGBA32F
void ImageToProcess::ConvertBetweenAnyRGB(EPixelFormat dstFormat)
{
	const EPixelFormat srcFormat = get()->GetPixelFormat();

	if (dstFormat == srcFormat)
	{
		return;
	}
	
	ESampleType srcSampleType;
	int srcChannelCount;
	bool srcHasAlpha;

	if (!CPixelFormats::GetPixelFormatInfo(srcFormat, &srcSampleType, &srcChannelCount, &srcHasAlpha))
	{
		assert(0);		
		set(0);
		return;
	}

	ESampleType dstSampleType;
	int dstChannelCount;
	bool dstHasAlpha;

	if (!CPixelFormats::GetPixelFormatInfo(dstFormat, &dstSampleType, &dstChannelCount, &dstHasAlpha))
	{
		assert(0);		
		set(0);
		return;
	}

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, dstFormat));
	std::vector<uint8> tmp;

	// transfer color range
	{
		Vec4 minColor, maxColor;

		get()->GetColorRange(minColor, maxColor);
		pRet->SetColorRange(minColor, maxColor);
	}

	const uint32 dwMips = pRet->GetMipCount();
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		const uint32 dwLocalWidth = get()->GetWidth(dwMip);
		const uint32 dwLocalHeight = get()->GetHeight(dwMip);

		char *pSrcMem;
		uint32 dwSrcPitch;
		get()->GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

		char *pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(dwMip, pDstMem, dwDstPitch);

		for (uint32 dwY = 0; dwY < dwLocalHeight; ++dwY)
		{
			const void* pSrc = &pSrcMem[dwSrcPitch * dwY];

			if (srcSampleType == eSampleType_Uint8)
			{
				if (dstSampleType == eSampleType_Float)
				{
					ConvertUnormToFloat<uchar,float>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
				else if (dstSampleType == eSampleType_Half)
				{
					ConvertUnormToFloat<uchar,SHalf>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
				else if (dstSampleType == eSampleType_Uint16)
				{
					ConvertUnormToUnorm<uchar,ushort>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
			}
			else if (dstSampleType == eSampleType_Uint8)
			{
				if (srcSampleType == eSampleType_Float)
				{
					ConvertFloatToUnorm<float,uchar>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
				else if (srcSampleType == eSampleType_Half)
				{
					ConvertFloatToUnorm<SHalf,uchar>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
				else if (srcSampleType == eSampleType_Uint16)
				{
					ConvertUnormToUnorm<ushort,uchar>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
			}
			else if (srcSampleType == eSampleType_Uint16)
			{
				if (dstSampleType == eSampleType_Float)
				{
					ConvertUnormToFloat<ushort,float>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
				else if (dstSampleType == eSampleType_Half)
				{
					ConvertUnormToFloat<ushort,SHalf>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
			}
			else if (dstSampleType == eSampleType_Uint16)
			{
				if (srcSampleType == eSampleType_Float)
				{
					ConvertFloatToUnorm<float,ushort>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
				else if (srcSampleType == eSampleType_Half)
				{
					ConvertFloatToUnorm<SHalf,ushort>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
			}
			else if (srcSampleType == eSampleType_Float)
			{
				if (dstSampleType == eSampleType_Half)
				{
					ConvertFloatToFloat<float,SHalf>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
			}
			else if (srcSampleType == eSampleType_Half)
			{
				if (dstSampleType == eSampleType_Float)
				{
					ConvertFloatToFloat<SHalf,float>(dwLocalWidth, srcChannelCount, pSrc, tmp);
					pSrc = &tmp[0];
				}
			}
			else
			{
				assert(srcSampleType == dstSampleType);
			}

			// All 3 or 4 channel 8 bit images are defined to be in BGR and BGRA order
			// All the other configurations are defined to be in R, RG, RGB and RGBA order

			if (dstSampleType == eSampleType_Uint8)
			{
				bool swap = ((srcSampleType != eSampleType_Uint8) && (dstChannelCount >= 3)) ||
				            ((srcSampleType == eSampleType_Uint8) && (srcChannelCount < 3) && (dstChannelCount >= 3)) ||
				            ((srcSampleType == eSampleType_Uint8) && (srcChannelCount >= 3) && (dstChannelCount < 3));
				if (swap)
				{
					ConvertChannels<uchar,true>(
						255, dwLocalWidth, (const uchar*)pSrc, srcChannelCount, srcHasAlpha, 
						(unsigned char*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
				else
				{
					ConvertChannels<uchar,false>(
						255, dwLocalWidth, (const uchar*)pSrc, srcChannelCount, srcHasAlpha, 
						(unsigned char*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
			}
			else if (dstSampleType == eSampleType_Uint16)
			{
				bool swap = (srcSampleType == eSampleType_Uint8) && (srcChannelCount >= 3);
				if (swap)
				{
					ConvertChannels<ushort,true>(
						65535, dwLocalWidth, (const ushort*)pSrc, srcChannelCount, srcHasAlpha, 
						(unsigned short*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
				else
				{
					ConvertChannels<ushort,false>(
						65535, dwLocalWidth, (const ushort*)pSrc, srcChannelCount, srcHasAlpha, 
						(unsigned short*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
			}
			else if (dstSampleType == eSampleType_Float)
			{
				bool swap = (srcSampleType == eSampleType_Uint8) && (srcChannelCount >= 3);
				if (swap)
				{
					ConvertChannels<float,true>(
						1.0f, dwLocalWidth, (const float*)pSrc, srcChannelCount, srcHasAlpha, 
						(float*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
				else
				{
					ConvertChannels<float,false>(
						1.0f, dwLocalWidth, (const float*)pSrc, srcChannelCount, srcHasAlpha, 
						(float*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
			}
			else if (dstSampleType == eSampleType_Half)
			{
				bool swap = (srcSampleType == eSampleType_Uint8) && (srcChannelCount >= 3);
				if (swap)
				{
					ConvertChannels<SHalf,true>(
						SHalf(1.0f), dwLocalWidth, static_cast<const SHalf*>(pSrc), srcChannelCount, srcHasAlpha, 
						(SHalf*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
				else
				{
					ConvertChannels<SHalf,false>(
						SHalf(1.0f), dwLocalWidth, static_cast<const SHalf*>(pSrc), srcChannelCount, srcHasAlpha, 
						(SHalf*)&pDstMem[dwDstPitch * dwY], dstChannelCount, dstHasAlpha);
				}
			}
			else
			{
				assert(0);
				set(0);
				return;
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
