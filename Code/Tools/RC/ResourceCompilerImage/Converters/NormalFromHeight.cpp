// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess
#include "SimpleBitmap.h"                   // CSimpleBitmap

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////
void ImageToProcess::BumpToNormalMap(
	const CBumpProperties& bumpProperties,
	const bool bValueFromAlpha)
{
	uint32 dwWidth, dwHeight, dwMips;

	get()->GetExtent(dwWidth, dwHeight, dwMips);
	assert(dwMips == 1);

	std::auto_ptr<ImageObject> pTmp(get()->CopyImage());
	pTmp->CopyPropertiesFrom(*get());
	
	// put luminance into alpha
	if (!bValueFromAlpha)
	{
		pTmp->GetLuminanceInAlpha();
	}

	// calculate blurred/filtered version
	pTmp->FilterImage(bumpProperties.GetBumpToNormalFilterIndex(), 0, bumpProperties.GetBumpBlurAmount(), bumpProperties.GetBumpBlurAmount(), pTmp.get(), 0, 0, NULL, NULL);

	// convert displace to normalmap
	char *pInputMem;
	uint32 dwInputPitch;
	pTmp->GetImagePointer(0, pInputMem, dwInputPitch);

	char *pOutputMem;
	uint32 dwOutputPitch;
	get()->GetImagePointer(0, pOutputMem, dwOutputPitch);

	const float fValue = bumpProperties.GetBumpStrengthAmount();
	const float fInvStrength = (fValue != 0.0f) ? 2.0f / fValue : 9999.9f;		// '2.0f' : samples are 2 pixel apart

	for (int iY = 0; iY < (int)dwHeight; ++iY)
	{
		Vec4f* const dest = (Vec4f*)&pOutputMem[((iY + 0) % dwHeight) * dwInputPitch];
		const Vec4f* const src0 = (const Vec4f*)&pInputMem[((iY - 1) % dwHeight) * dwOutputPitch];
		const Vec4f* const src1 = (const Vec4f*)&pInputMem[((iY + 0) % dwHeight) * dwOutputPitch];
		const Vec4f* const src2 = (const Vec4f*)&pInputMem[((iY + 1) % dwHeight) * dwOutputPitch];

		for (int iX = 0; iX < (int)dwWidth; ++iX)
		{
			float fHorzSamples[2];
			float fVertSamples[2];

			fVertSamples[0] = src0[(iX + 0) % dwWidth][3];
			fHorzSamples[0] = src1[(iX - 1) % dwWidth][3];
			fHorzSamples[1] = src1[(iX + 1) % dwWidth][3];
			fVertSamples[1] = src2[(iX + 0) % dwWidth][3];

			const Vec3 vHoriz = Vec3(fInvStrength, 0.0f, fHorzSamples[1] - fHorzSamples[0]);
			const Vec3 vVert = Vec3(0.0f, fInvStrength, fVertSamples[1] - fVertSamples[0]);
			Vec3 vNormal = vHoriz.Cross(vVert);

			vNormal.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));

			// restore alpha channel
			dest[iX] = Vec4f(vNormal, dest[iX][3]);
		}
	}

	// convert from -1..1 to 0..1
	get()->ScaleAndBiasChannels(0, 100,
		Vec4(0.5f, 0.5f, 0.5f, 1.0f),
		Vec4(0.5f, 0.5f, 0.5f, 0.0f));
}
