// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

///////////////////////////////////////////////////////////////////////////////////

static inline float GammaToLinear(float x)
{
	return (x <= 0.04045f) ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f);
}

static inline float LinearToGamma(float x)
{
	return (x <= 0.0031308f) ? x * 12.92f : 1.055f * powf(x, 1.0f / 2.4f) - 0.055f;
}

static inline ColorF GammaToLinear(const ColorF& x)
{
	return ColorF(
		GammaToLinear(x.r),
		GammaToLinear(x.g),
		GammaToLinear(x.b),
		x.a);
}

static inline ColorF LinearToGamma(const ColorF& x)
{
	return ColorF(
		LinearToGamma(x.r),
		LinearToGamma(x.g),
		LinearToGamma(x.b),
		x.a);
}

///////////////////////////////////////////////////////////////////////////////////

// converts to DXT1, extracts normals from both source and DXT1 compressed image and gets sum of angle differences divided by number of pixels
float ImageObject::GetDXT1NormalsCompressionError(const CImageProperties* pProps) const
{
	ImageToProcess image1(this->CopyImage());
	image1.get()->CopyPropertiesFrom(*this);
	image1.get()->SetCubemap(ImageObject::eCubemap_No);
	image1.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);

	ImageToProcess image2(this->CopyImage());
	image2.get()->CopyPropertiesFrom(*this);
	image2.get()->SetCubemap(ImageObject::eCubemap_No);
	image2.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);
	image2.ConvertFormat(pProps, ePixelFormat_DXT1, ImageToProcess::eQuality_Preview);
	image2.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);

	uint32 width, height, mips;
	image1.get()->GetExtent(width, height, mips);
	assert(mips == 1);

	char* pSrcMem;
	uint32 srcPitch;
	image1.get()->GetImagePointer(0, pSrcMem, srcPitch);

	char* pDstMem;
	uint32 dstPitch;
	image2.get()->GetImagePointer(0, pDstMem, dstPitch);

	// extract each pair of normals and add angle between them
	float fSumDeltaSq = 0;
	for (uint32 y = 0; y < height; ++y)
	{
		const ColorF* const pSrc = (const ColorF*)(pSrcMem + y * srcPitch);
		const ColorF* const pDst = (const ColorF*)(pDstMem + y * dstPitch);
		for (uint32 x = 0; x < width; ++x)
		{
			const ColorF& srcCol = pSrc[x];
			const ColorF& dxtCol = pDst[x];

			Vec3 norm1(srcCol.r * 2.f - 1.f, srcCol.g * 2.f - 1.f, 0.f);
			norm1.z = sqrtf(1.f - Util::getClamped(norm1.x * norm1.x + norm1.y * norm1.y, 0.f, 1.f) );

			Vec3 norm2(dxtCol.r * 2.f - 1.f, dxtCol.g * 2.f - 1.f, 0.f);
			norm2.z = sqrtf(1.f - Util::getClamped(norm2.x * norm2.x + norm2.y * norm2.y, 0.f, 1.f) );

			fSumDeltaSq += acosf(Util::getClamped(norm1.Dot(norm2), 0.f, 1.f));
		}
	}

	return fSumDeltaSq / (width * height);
}


static float ComputeMse(const bool bGammaSpaceMetric, const Vec3& weights, const ImageObject& image1, const ImageObject& image2)
{
	float fSumDeltaSq = 0;
	float fSum = 0;

	const uint32 mips = std::min(image1.GetMipCount(), image2.GetMipCount());

	for (uint32 m = 0; m < mips; ++m)
	{
		char* pSrcMem;
		uint32 srcPitch;
		image1.GetImagePointer(m, pSrcMem, srcPitch);

		char* pDstMem;
		uint32 dstPitch;
		image2.GetImagePointer(m, pDstMem, dstPitch);

		const uint32 height = image1.GetHeight(m);
		const uint32 width = image1.GetWidth(m);

		assert(height == image2.GetHeight(m));
		assert(width == image2.GetWidth(m));

		for (uint32 y = 0; y < height; ++y)
		{
			const ColorF* const pSrc = (const ColorF*)(pSrcMem + y * srcPitch);
			const ColorF* const pDst = (const ColorF*)(pDstMem + y * dstPitch);
			for (uint32 x = 0; x < width; ++x)
			{
				const ColorF& srcCol = pSrc[x];
				const ColorF& dstCol = pDst[x];

				const ColorF srcColG = (bGammaSpaceMetric ? LinearToGamma(srcCol) : srcCol) * weights;
				const ColorF dstColG = (bGammaSpaceMetric ? LinearToGamma(dstCol) : dstCol) * weights;

				fSumDeltaSq += Vec3(srcColG.r, srcColG.g, srcColG.b).GetSquaredDistance(Vec3(dstColG.r, dstColG.g, dstColG.b));
			}
		}

		fSum += width * height;
	}

	return fSumDeltaSq / fSum;
}


// converts to the specified format, extracts color from both source and the converted image and gets sum of square differences divided by number of pixels
void ImageObject::GetGammaCompressionError(const EPixelFormat format, const CImageProperties* pProps, float* pErrorLinear, float* pErrorSrgb) const
{
	const bool bGammaSpaceMetric = true;
	const Vec3 weights = pProps->GetColorWeights();

	const ImageObject::EColorNormalization eColorNorm = ImageObject::eColorNormalization_Normalize;
	const ImageObject::EAlphaNormalization eAlphaNorm = ImageObject::eAlphaNormalization_Normalize;

	ImageToProcess image1(this->CopyImage());
	image1.get()->CopyPropertiesFrom(*this);
	image1.get()->SetCubemap(ImageObject::eCubemap_No);
	image1.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);
	image1.get()->ExpandImageRange(eColorNorm, eAlphaNorm, 0);
	image1.ConvertModel(pProps, CImageProperties::eColorModel_RGB);

	// i == 0: linear, i == 1: sRGB
	for (int i = 0; i < 2; ++i)
	{
		ImageToProcess image2(this->CopyImage());
		image2.get()->CopyPropertiesFrom(*this);
		image2.get()->SetCubemap(ImageObject::eCubemap_No);
		image2.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);
		if (i)
		{
			image2.LinearRGBAAnyFToGammaRGBAAnyF();
		}
		image2.ConvertFormat(pProps, format, ImageToProcess::eQuality_Preview);
		image2.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);
		if (i)
		{
			image2.GammaToLinearRGBA32F(true);
		}
		image2.get()->ExpandImageRange(eColorNorm, eAlphaNorm, 0);
		image2.ConvertModel(pProps, CImageProperties::eColorModel_RGB);

		((i == 0) ? pErrorLinear : pErrorSrgb)[0] = ComputeMse(bGammaSpaceMetric, weights, *image1.get(), *image2.get());
	}
}

// converts to DXT1, extracts color from both source and DXT1 compressed image and gets sum of square differences divided by number of pixels
void ImageObject::GetDXT1ColorspaceCompressionError(const CImageProperties* pProps, float(&pError)[20]) const
{
	const bool bGammaSpaceMetric = false;
	const Vec3 weights = pProps->GetColorWeights();
	
	ImageToProcess image1(this->CopyImage());
	image1.get()->CopyPropertiesFrom(*this);
	image1.get()->SetCubemap(ImageObject::eCubemap_No);
	image1.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);

	for (int gm = 0; gm <= 1; ++gm)
	for (int nm = 0; nm <= 1; ++nm)
	for (int cs = CImageProperties::eColorModel_RGB; cs <= CImageProperties::eColorModel_IRB; ++cs)
	{
		ImageToProcess image2(this->CopyImage());
		image2.get()->CopyPropertiesFrom(*this);
		image2.get()->SetCubemap(ImageObject::eCubemap_No);
		image2.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);
		image2.ConvertModel(pProps, (CImageProperties::EColorModel)cs);

		if (nm)
		{
			image2.get()->NormalizeImageRange(eColorNormalization_Normalize, eAlphaNormalization_PassThrough, false, 0);
		}
		if (gm)
		{
			image2.LinearRGBAAnyFToGammaRGBAAnyF();
		}
		
		image2.ConvertFormat(pProps, ePixelFormat_DXT1, ImageToProcess::eQuality_Normal);
//		image2.ConvertFormat(pProps, ePixelFormat_A8R8G8B8);
		image2.ConvertFormat(pProps, ePixelFormat_A32B32G32R32F);

		if (gm)
		{
			image2.GammaToLinearRGBA32F(true);
		}
		if (nm)
		{
			image2.get()->ExpandImageRange(eColorNormalization_Normalize, eAlphaNormalization_PassThrough, 0);
		}

		image2.ConvertModel(pProps, CImageProperties::eColorModel_RGB);

		pError[cs * 4 + nm * 2 + gm * 1] = ComputeMse(bGammaSpaceMetric, weights, *image1.get(), *image2.get());
	}
}
