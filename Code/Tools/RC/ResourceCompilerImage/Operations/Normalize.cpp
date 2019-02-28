// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog

template<const int qBits>
static void AdjustScaleForQuantization(float fBaseValue, float fBaseLine, float& cScale, float& cMinColor, float& cMaxColor)
{
	const int qOne = (1 << qBits) - 1;
	const int qUpperBits = (8 - qBits);
	const int qLowerBits = qBits - qUpperBits;

	const int v = int(floor(fBaseValue * qOne));
	
	int v0 = v - (v != 0);
	int v1 = v + 0;
	int v2 = v + (v != qOne);
	
	v0 = (v0 << qUpperBits) | (v0 >> qLowerBits);
	v1 = (v1 << qUpperBits) | (v1 >> qLowerBits);
	v2 = (v2 << qUpperBits) | (v2 >> qLowerBits);

	const float f0 = v0 / 255.0f;
	const float f1 = v1 / 255.0f;
	const float f2 = v2 / 255.0f;

	float fBaseLock = -1;

	if (fabsf(f0 - fBaseValue) < fabsf(fBaseLock - fBaseValue)) fBaseLock = f0;
	if (fabsf(f1 - fBaseValue) < fabsf(fBaseLock - fBaseValue)) fBaseLock = f1;
	if (fabsf(f2 - fBaseValue) < fabsf(fBaseLock - fBaseValue)) fBaseLock = f2;

	float lScale = (1.0f - fBaseLock ) / (1.0f - fBaseLine);
	float vScale = (1.0f - fBaseValue) / (1.0f - fBaseLine);
	float sScale = lScale / vScale;

	float csScale = (cScale / sScale);
	float csBias = cMinColor - (1.0f - sScale) * (cScale / sScale);

	if ((csBias > 0.0f) && ((csScale + csBias) < 1.0f))
	{
		cMinColor = csBias;
		cScale    = csScale;
		cMaxColor = csScale + csBias;
	}
}

///////////////////////////////////////////////////////////////////////////////////

void ImageObject::NormalizeImageRange(EColorNormalization eColorNorm, EAlphaNormalization eAlphaNorm, bool bMaintainBlack, int nExponentBits)
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	uint32 dwWidth, dwHeight, dwMips;
	GetExtent(dwWidth, dwHeight, dwMips);

	// find image's range, can be negative
	Vec4 cMinColor = Vec4( FLT_MAX,  FLT_MAX,  FLT_MAX,  FLT_MAX);
	Vec4 cMaxColor = Vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		char* pSrcMem;
		uint32 dwSrcPitch;
		GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

		dwHeight = GetHeight(dwMip);
		dwWidth  = GetWidth (dwMip);
		for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
		{				
			const Vec4f* pSrcPix = (Vec4f*)&pSrcMem[dwY * dwSrcPitch];
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{		
				cMinColor.x = Util::getMin(cMinColor.x, pSrcPix->x);
				cMinColor.y = Util::getMin(cMinColor.y, pSrcPix->y);
				cMinColor.z = Util::getMin(cMinColor.z, pSrcPix->z);
				cMinColor.w = Util::getMin(cMinColor.w, pSrcPix->w);
				cMaxColor.x = Util::getMax(cMaxColor.x, pSrcPix->x);
				cMaxColor.y = Util::getMax(cMaxColor.y, pSrcPix->y);
				cMaxColor.z = Util::getMax(cMaxColor.z, pSrcPix->z);
				cMaxColor.w = Util::getMax(cMaxColor.w, pSrcPix->w);

				pSrcPix++;
			}
		}
	}

	if (bMaintainBlack)
	{
		cMinColor.x = Util::getMin(0.f, cMinColor.x);
		cMinColor.y = Util::getMin(0.f, cMinColor.y);
		cMinColor.z = Util::getMin(0.f, cMinColor.z);
		cMinColor.w = Util::getMin(0.f, cMinColor.w);
	}

	assert(cMaxColor.x >= cMinColor.x && cMaxColor.y >= cMinColor.y && 
	       cMaxColor.z >= cMinColor.z && cMaxColor.w >= cMinColor.w);

	// some graceful threshold to avoid extreme cases
	if (cMaxColor.x - cMinColor.x < (3.f / 255))
	{
		cMinColor.x = Util::getMax(0.f, cMinColor.x - (2.f / 255));
		cMaxColor.x = Util::getMin(1.f, cMaxColor.x + (2.f / 255));
	}
	if (cMaxColor.y - cMinColor.y < (3.f / 255))
	{
		cMinColor.y = Util::getMax(0.f, cMinColor.y - (2.f / 255));
		cMaxColor.y = Util::getMin(1.f, cMaxColor.y + (2.f / 255));
	}
	if (cMaxColor.z - cMinColor.z < (3.f / 255))
	{
		cMinColor.z = Util::getMax(0.f, cMinColor.z - (2.f / 255));
		cMaxColor.z = Util::getMin(1.f, cMaxColor.z + (2.f / 255));
	}
	if (cMaxColor.w - cMinColor.w < (3.f / 255))
	{
		cMinColor.w = Util::getMax(0.f, cMinColor.w - (2.f / 255));
		cMaxColor.w = Util::getMin(1.f, cMaxColor.w + (2.f / 255));
	}

	// calculate range to normalize to
	const float fMaxExponent = powf(2.0f, nExponentBits) - 1.0f;
	const float cUprValue    = powf(2.0f, fMaxExponent);

	if (eColorNorm == eColorNormalization_PassThrough)
	{
		cMinColor.x = cMinColor.y = cMinColor.z = 0.f;
		cMaxColor.x = cMaxColor.y = cMaxColor.z = 1.f;
	}

	// don't touch alpha channel if not used
	if (eAlphaNorm == eAlphaNormalization_SetToZero)
	{
		// Store the range explicitly into the structure for read-back.
		// The formats which request range expansion don't support alpha.
		cMinColor.w = 0.f;
		cMaxColor.w = cUprValue;
	}
	else if (eAlphaNorm == eAlphaNormalization_PassThrough)
	{
		cMinColor.w = 0.f;
		cMaxColor.w = 1.f;
	}

	// get the origins of the color model's lattice for the range of values
	// these values need to be encoded as precise as possible under quantization
	Vec4 cBaseLines = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	Vec4 cScale = cMaxColor - cMinColor;

#if 0
	// NOTE: disabled for now, in the future we can turn this on to force availability
	// of value to guarantee for example perfect grey-scales (using YFF)
	switch (GetImageFlags() & CImageExtensionHelper::EIF_Colormodel)
	{
	case CImageExtensionHelper::EIF_Colormodel_RGB:
		cBaseLines = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		break;
	case CImageExtensionHelper::EIF_Colormodel_CIE:
		cBaseLines = Vec4(0.0f, 1.f/3, 1.f/3, 0.0f);
		break;
	case CImageExtensionHelper::EIF_Colormodel_IRB:
		cBaseLines = Vec4(0.0f, 1.f/2, 1.f/2, 0.0f);
		break;
	case CImageExtensionHelper::EIF_Colormodel_YCC:
	case CImageExtensionHelper::EIF_Colormodel_YFF:
		cBaseLines = Vec4(1.f/2, 0.0f, 1.f/2, 0.0f);
		break;
	}

	Vec4 cBaseScale = cBaseLines;
	cBaseLines = cBaseLines - cMinColor;
	cBaseLines = cBaseLines / cScale;

	if ((cBaseLines.x > 0.0f) && (cBaseLines.x < 1.0f))
		AdjustScaleForQuantization<5>(cBaseLines.x, cBaseScale.x, cScale.x, cMinColor.x, cMaxColor.x);
	if ((cBaseLines.y > 0.0f) && (cBaseLines.y < 1.0f))
		AdjustScaleForQuantization<6>(cBaseLines.y, cBaseScale.y, cScale.y, cMinColor.y, cMaxColor.y);
	if ((cBaseLines.z > 0.0f) && (cBaseLines.z < 1.0f))
		AdjustScaleForQuantization<5>(cBaseLines.z, cBaseScale.z, cScale.z, cMinColor.z, cMaxColor.z);
#endif

	// normalize the image
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		char* pSrcMem;
		uint32 dwSrcPitch;
		GetImagePointer(dwMip,pSrcMem,dwSrcPitch);

		dwHeight = GetHeight(dwMip);
		dwWidth  = GetWidth (dwMip);
		for(uint32 dwY = 0; dwY < dwHeight; ++dwY)
		{				
			Vec4f* pSrcPix = (Vec4f*)&pSrcMem[dwY * dwSrcPitch];
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{
				Vec4 color(*pSrcPix);
				color = color - cMinColor;
				color = color / cScale;
				color = color * cUprValue;
				*pSrcPix = color;

				pSrcPix++;
			}
		}
	}

	// set up a range
	SetColorRange(cMinColor, cMaxColor);

	// set up a flag
	AddImageFlags(CImageExtensionHelper::EIF_RenormalizedTexture);
}

void ImageObject::ExpandImageRange(EColorNormalization eColorMode, EAlphaNormalization eAlphaMode, int nExponentBits)
{
	assert(!((eAlphaMode != eAlphaNormalization_SetToZero) && (nExponentBits != 0)));

	if (!HasImageFlags(CImageExtensionHelper::EIF_RenormalizedTexture))
	{
		return;
	}

	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	uint32 dwWidth, dwHeight, dwMips;
	GetExtent(dwWidth, dwHeight, dwMips);

	// calculate range to normalize to
	const float fMaxExponent = powf(2.0f, nExponentBits) - 1.0f;
	float cUprValue = powf(2.0f, fMaxExponent);

	// find image's range, can be negative
	Vec4 cMinColor = Vec4( FLT_MAX,  FLT_MAX,  FLT_MAX,  FLT_MAX);
	Vec4 cMaxColor = Vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);

	GetColorRange(cMinColor, cMaxColor);

	// don't touch alpha channel if not used
	if (eAlphaMode == eAlphaNormalization_SetToZero)
	{
		// Overwrite the range explicitly into the structure.
		// The formats which request range expansion don't support alpha.
		cUprValue = cMaxColor.w;

		cMinColor.w = 1.f;
		cMaxColor.w = 1.f;
	}

	// expand the image
	const Vec4 cScale = cMaxColor - cMinColor;
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		char* pSrcMem;
		uint32 dwSrcPitch;
		GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

		dwHeight = GetHeight(dwMip);
		dwWidth  = GetWidth (dwMip);
		for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
		{				
			Vec4f* pSrcPix = (Vec4f*)&pSrcMem[dwY * dwSrcPitch];
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{
				Vec4 color(*pSrcPix);
				color = color / cUprValue;
				color = color * cScale;
				color = color + cMinColor;
				*pSrcPix = color;

				pSrcPix++;
			}
		}
	}

	// set up a range
	SetColorRange(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	// set up a flag
	RemoveImageFlags(CImageExtensionHelper::EIF_RenormalizedTexture);
}

///////////////////////////////////////////////////////////////////////////////////

void ImageObject::NormalizeVectors(uint32 firstMip, uint32 maxMipCount)
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	uint32 lastMip = Util::getMin(firstMip + maxMipCount, GetMipCount());
	for (uint32 mip = firstMip; mip < lastMip; ++mip)
	{
		const uint32 pixelCount = GetPixelCount(mip);
		Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			Vec3 vNormal = Vec3(pPixels[i].components[0] * 2.0f - 1.0f, pPixels[i].components[1] * 2.0f - 1.0f, pPixels[i].components[2] * 2.0f - 1.0f);

			// TODO: every opposing vector addition produces the zero-vector for
			// normals on the entire sphere, in that case the forward vector [0,0,1]
			// isn't necessarily right and we should look at the adjacent normals
			// for a direction
			vNormal.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));

			pPixels[i].components[0] = vNormal.x * 0.5f + 0.5f;
			pPixels[i].components[1] = vNormal.y * 0.5f + 0.5f;
			pPixels[i].components[2] = vNormal.z * 0.5f + 0.5f;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
void ImageObject::ScaleAndBiasChannels(uint32 firstMip, uint32 maxMipCount, const Vec4& scale, const Vec4& bias)
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	const uint32 lastMip = Util::getMin(firstMip + maxMipCount, GetMipCount());
	for (uint32 mip = firstMip; mip < lastMip; ++mip)
	{
		const uint32 pixelCount = GetPixelCount(mip);
		Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			Color4<float>& v = pPixels[i];

			v.components[0] = v.components[0] * scale.x + bias.x;
			v.components[1] = v.components[1] * scale.y + bias.y;
			v.components[2] = v.components[2] * scale.z + bias.z;
			v.components[3] = v.components[3] * scale.w + bias.w;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
void ImageObject::ClampChannels(uint32 firstMip, uint32 maxMipCount, const Vec4& min, const Vec4& max)
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	const uint32 lastMip = Util::getMin(firstMip + maxMipCount, GetMipCount());
	for (uint32 mip = firstMip; mip < lastMip; ++mip)
	{
		const uint32 pixelCount = GetPixelCount(mip);
		Color4<float>* const pPixels = GetPixelsPointer<Color4<float> >(mip);

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			Color4<float>& v = pPixels[i];

			Util::clampMinMax(v.components[0], min.x, max.x);
			Util::clampMinMax(v.components[1], min.y, max.y);
			Util::clampMinMax(v.components[2], min.z, max.z);
			Util::clampMinMax(v.components[3], min.w, max.w);
		}
	}
}
