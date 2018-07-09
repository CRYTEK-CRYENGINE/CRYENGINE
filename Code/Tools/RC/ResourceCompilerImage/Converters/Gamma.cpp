// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "MathHelpers.h"                    // FastRoundFloatTowardZero()
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>         // eRcExitCode_FatalError

///////////////////////////////////////////////////////////////////////////////////

// Lookup table for a function 'float fn(float x)'.
// Computed function values are stored in the table for x in [0.0; 1.0].
//
// If passed x is less than xMin (xMin must be >= 0) or greater than 1.0,
// then the original function is called.
// Otherwise, a value from the table (linearly interpolated)
// is returned.
template <int TABLE_SIZE>
class FunctionLookupTable
{
public:
	FunctionLookupTable(float (*fn)(float x), float xMin, float maxAllowedDifference)
		: m_fn(fn)
		, m_xMin(xMin)
	{
		assert(xMin >= 0.0f);
		for (int i = 0; i <= TABLE_SIZE; ++i)
		{
			const float x = i / (float)TABLE_SIZE;
			const float y = (*fn)(x);
			m_table[i] = y;
		}
		selfTest(maxAllowedDifference);
	}

	inline float compute(float x) const
	{
		if (x < m_xMin || x > 1)
		{
			return m_fn(x);
		}

		const float f = x * TABLE_SIZE;
		assert(f >= 0);

		const int i = MathHelpers::FastRoundFloatTowardZero(f);

		if (i >= TABLE_SIZE)
		{
			return m_table[TABLE_SIZE];
		}

		const float alpha = f - i;
		return (1 - alpha) * m_table[i] + alpha * m_table[i + 1];
	}

private:
	void selfTest(const float maxDifferenceAllowed) const
	{
		if (MathHelpers::FastRoundFloatTowardZero(-0.99f) != 0 ||
			MathHelpers::FastRoundFloatTowardZero(+0.00f) != 0 ||
			MathHelpers::FastRoundFloatTowardZero(+0.01f) != 0 ||
			MathHelpers::FastRoundFloatTowardZero(+0.99f) != 0 ||
			MathHelpers::FastRoundFloatTowardZero(+1.00f) != 1 ||
			MathHelpers::FastRoundFloatTowardZero(+1.01f) != 1 ||
			MathHelpers::FastRoundFloatTowardZero(+1.99f) != 1 ||
			MathHelpers::FastRoundFloatTowardZero(+2.00f) != 2 ||
			MathHelpers::FastRoundFloatTowardZero(+2.01f) != 2)
		{
			exit(eRcExitCode_FatalError);
		}

		if (m_xMin < 0)
		{
			exit(eRcExitCode_FatalError);
		}

		const int n = 1000000;
		for (int i = 0; i <= n; ++i)
		{
			const float x = 1.1f * (i / (float)n);
			const float resOriginal = m_fn(x);
			const float resTable = compute(x);
			const float difference = resOriginal - resTable;

			if (fabs(difference) > maxDifferenceAllowed)
			{
				exit(eRcExitCode_FatalError);
			}
		}
	}

private:
	float (*m_fn)(float x);
	float m_xMin;
	float m_table[TABLE_SIZE + 1];
};


static float GammaToLinear(float x)
{
	return (x <= 0.04045f) ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f);
}

static float LinearToGamma(float x)
{
	return (x <= 0.0031308f) ? x * 12.92f : 1.055f * powf(x, 1.0f / 2.4f) - 0.055f;
}

static FunctionLookupTable<1024> s_lutGammaToLinear(GammaToLinear, 0.04045f, 0.00001f);
static FunctionLookupTable<1024> s_lutLinearToGamma(LinearToGamma, 0.05f, 0.00001f);

///////////////////////////////////////////////////////////////////////////////////

template<typename T, const int norm, const bool swap>
static void LinearToLinear(uint32 pixelCount, int channelCount, T* pSrc, float* pDst)
{
	if (channelCount == 1)
	{
		while (pixelCount--)
		{
			pDst[0] = pSrc[0] / float(norm);
			pDst[1] = 0.0f;
			pDst[2] = 0.0f;
			pDst[3] = 1.0f;
			pSrc += 1;
			pDst += 4;
		}
	}
	else if (channelCount == 2)
	{
		while (pixelCount--)
		{
			pDst[0] = pSrc[0] / float(norm);
			pDst[1] = 0.0f;
			pDst[2] = 0.0f;
			pDst[3] = pSrc[1] / float(norm);
			pSrc += 2;
			pDst += 4;
		}
	}
	else if (channelCount == 3)
	{
		while (pixelCount--)
		{
			pDst[0] = pSrc[0 + swap * 2] / float(norm);
			pDst[1] = pSrc[1           ] / float(norm);
			pDst[2] = pSrc[2 - swap * 2] / float(norm);
			pDst[3] = 1.0f;
			pSrc += 3;
			pDst += 4;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDst[0] = pSrc[0 + swap * 2] / float(norm);
			pDst[1] = pSrc[1           ] / float(norm);
			pDst[2] = pSrc[2 - swap * 2] / float(norm);
			pDst[3] = pSrc[3           ] / float(norm);
			pSrc += 4;
			pDst += 4;
		}
	}
}

template<typename T, const int norm, const bool swap>
static void GammaToLinear(uint32 pixelCount, int channelCount, T* pSrc, float* pDst)
{
	if (channelCount == 1)
	{
		while (pixelCount--)
		{
			pDst[0] = s_lutGammaToLinear.compute(pSrc[0] / float(norm));
			pDst[1] = 0.0f;
			pDst[2] = 0.0f;
			pDst[3] = 1.0f;
			pSrc += 1;
			pDst += 4;
		}
	}
	else if (channelCount == 2)
	{
		while (pixelCount--)
		{
			pDst[0] = s_lutGammaToLinear.compute(pSrc[0] / float(norm));
			pDst[1] = 0.0f;
			pDst[2] = 0.0f;
			pDst[3] = pSrc[1] / float(norm) ;
			pSrc += 2;
			pDst += 4;
		}
	}
	else if (channelCount == 3)
	{
		while (pixelCount--)
		{
			pDst[0] = s_lutGammaToLinear.compute(pSrc[0 + swap * 2] / float(norm));
			pDst[1] = s_lutGammaToLinear.compute(pSrc[1           ] / float(norm));
			pDst[2] = s_lutGammaToLinear.compute(pSrc[2 - swap * 2] / float(norm));
			pDst[3] = 1.0f;
			pSrc += 3;
			pDst += 4;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDst[0] = s_lutGammaToLinear.compute(pSrc[0 + swap * 2] / float(norm));
			pDst[1] = s_lutGammaToLinear.compute(pSrc[1           ] / float(norm));
			pDst[2] = s_lutGammaToLinear.compute(pSrc[2 - swap * 2] / float(norm));
			pDst[3] =                            pSrc[3           ] / float(norm) ;
			pSrc += 4;
			pDst += 4;
		}
	}
}

template<typename T, const int norm, const bool swap>
static void GammaToLinearLUT(uint32 pixelCount, int channelCount, T* pSrc, float* pDst)
{
	static float degamma_table[norm + 1];
	if (!degamma_table[norm])
	{
		for (int i = 0; i <= norm; ++i)
		{
			degamma_table[i] = GammaToLinear(i / float(norm));
		}
	}

	if (channelCount == 1)
	{
		while (pixelCount--)
		{
			pDst[0] = degamma_table[pSrc[0]];
			pDst[1] = 0.0f;
			pDst[2] = 0.0f;
			pDst[3] = 1.0f;
			pSrc += 1;
			pDst += 4;
		}
	}
	else if (channelCount == 2)
	{
		while (pixelCount--)
		{
			pDst[0] = degamma_table[pSrc[0]];
			pDst[1] = 0.0f;
			pDst[2] = 0.0f;
			pDst[3] = pSrc[1] / float(norm);
			pSrc += 2;
			pDst += 4;
		}
	}
	else if (channelCount == 3)
	{
		while (pixelCount--)
		{
			pDst[0] = degamma_table[pSrc[0 + swap * 2]];
			pDst[1] = degamma_table[pSrc[1           ]];
			pDst[2] = degamma_table[pSrc[2 - swap * 2]];
			pDst[3] = 1.0f;
			pSrc += 3;
			pDst += 4;
		}
	}
	else if (channelCount == 4)
	{
		while (pixelCount--)
		{
			pDst[0] = degamma_table[pSrc[0 + swap * 2]];
			pDst[1] = degamma_table[pSrc[1           ]];
			pDst[2] = degamma_table[pSrc[2 - swap * 2]];
			pDst[3] =               pSrc[3           ] / float(norm);
			pSrc += 4;
			pDst += 4;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
void ImageToProcess::GammaToLinearRGBA32F(bool bDeGamma)
{
	// return immediately if there is no need to de-gamma image and the source is in the desired format
	const EPixelFormat format = get()->GetPixelFormat();
	if (!bDeGamma && (format == ePixelFormat_A32B32G32R32F))
	{
		return;
	}

	ESampleType sampleType;
	int channelCount;
	bool bHasAlpha;

	if (!CPixelFormats::GetPixelFormatInfo(format, &sampleType, &channelCount, &bHasAlpha) || CPixelFormats::GetPixelFormatInfo(format)->bCompressed)
	{
		assert(0);		
		set(0);
		return;
	}

	std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, ePixelFormat_A32B32G32R32F));

	// make gamma-correction
	const uint32 dwMips = pRet->GetMipCount();
	for (uint32 mip = 0; mip < dwMips; ++mip)
	{
		const uint32 pixelCount = pRet->GetPixelCount(mip);

		char *pSrcMem;
		uint32 dwSrcPitch;
		get()->GetImagePointer(mip, pSrcMem, dwSrcPitch);

		char *pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(mip, pDstMem, dwDstPitch);

		if (format == ePixelFormat_R8 || format == ePixelFormat_G8R8 || format == ePixelFormat_A8R8G8B8 || format == ePixelFormat_X8R8G8B8)
		{
			if (bDeGamma)
			{
				::GammaToLinearLUT<uint8, 255, true>(pixelCount, channelCount, (uint8*)pSrcMem, (float*)(void*)pDstMem);
			}
			else
			{
				::LinearToLinear<uint8, 255, true>(pixelCount, channelCount, (uint8*)pSrcMem, (float*)(void*)pDstMem);
			}
		}
		else if (format == ePixelFormat_R16 || format == ePixelFormat_G16R16 || format == ePixelFormat_A16B16G16R16)
		{
			if (bDeGamma)
			{
				::GammaToLinearLUT<uint16, 65535, false>(pixelCount, channelCount, (uint16*)pSrcMem, (float*)(void*)pDstMem);
			}
			else
			{
				::LinearToLinear<uint16, 65535, false>(pixelCount, channelCount, (uint16*)pSrcMem, (float*)(void*)pDstMem);
			}
		}
		else if (format == ePixelFormat_R16F || format == ePixelFormat_G16R16F || format == ePixelFormat_A16B16G16R16F)
		{
			if (bDeGamma)
			{
				::GammaToLinear<SHalf, 1, false>(pixelCount, channelCount, reinterpret_cast<SHalf*>(pSrcMem), (float*)(void*)pDstMem);
			}
			else
			{
				::LinearToLinear<SHalf, 1, false>(pixelCount, channelCount, reinterpret_cast<SHalf*>(pSrcMem), (float*)(void*)pDstMem);
			}
		}
		else if (format == ePixelFormat_R32F || format == ePixelFormat_G32R32F || format == ePixelFormat_A32B32G32R32F)
		{
			assert(bDeGamma || (format != ePixelFormat_A32B32G32R32F));

			if (bDeGamma)
			{
				::GammaToLinear<float, 1, false>(pixelCount, channelCount, (float*)(void*)pSrcMem, (float*)(void*)pDstMem);
			}
			else
			{
				::LinearToLinear<float, 1, false>(pixelCount, channelCount, (float*)(void*)pSrcMem, (float*)(void*)pDstMem);
			}
		}
		else
		{
			assert(0);
			RCLogError("CImageCompiler::GammaToLinear() unknown input format");
			set(0);
			return;
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

	if (bDeGamma)
	{
		get()->RemoveImageFlags(CImageExtensionHelper::EIF_SRGBRead);
	}
}

void ImageToProcess::LinearRGBAAnyFToGammaRGBAAnyF() 
{
	if ((get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F) && (get()->GetPixelFormat() != ePixelFormat_A16B16G16R16F))
	{
		assert(0 && "not float");
		RCLogError("%s: unsupported input format", __FUNCTION__);
		set(0);
		return;
	}

	if (get()->HasImageFlags(CImageExtensionHelper::EIF_SRGBRead))
	{
		assert(0 && "already SRGB");
		RCLogError("%s: input is already SRGB", __FUNCTION__);
		set(0);
		return;
	}

	// make gamma-correction
	for (uint32 mip = 0; mip < get()->GetMipCount(); ++mip)
	{
		if (get()->GetPixelFormat() == ePixelFormat_A16B16G16R16F)
		{
			Color4<SHalf>* const pPixels = get()->GetPixelsPointer<Color4<SHalf> >(mip);
			const uint32 pixelCount = get()->GetPixelCount(mip);

			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].components[0] = SHalf(s_lutLinearToGamma.compute(pPixels[i].components[0]));
				pPixels[i].components[1] = SHalf(s_lutLinearToGamma.compute(pPixels[i].components[1]));
				pPixels[i].components[2] = SHalf(s_lutLinearToGamma.compute(pPixels[i].components[2]));
			}			
		}
		else
		{
			ColorF* const pPixels = get()->GetPixelsPointer<ColorF>(mip);
			const uint32 pixelCount = get()->GetPixelCount(mip);

			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pPixels[i].r = s_lutLinearToGamma.compute(pPixels[i].r);
				pPixels[i].g = s_lutLinearToGamma.compute(pPixels[i].g);
				pPixels[i].b = s_lutLinearToGamma.compute(pPixels[i].b);
			}			
		}
	}

	get()->AddImageFlags(CImageExtensionHelper::EIF_SRGBRead);
}
