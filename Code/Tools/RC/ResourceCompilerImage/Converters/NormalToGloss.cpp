// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"								// CImageCompiler
#include "../ImageUserDialog.h"             // CImageUserDialog
#include "../ImageObject.h"									// ImageToProcess

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////

void ImageObject::ConvertLegacyGloss()
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	uint32 dwWidth = GetWidth(0);
	uint32 dwHeight = GetHeight(0);
	
	char* pMip0Mem;
	uint32 dwMip0Pitch;
	GetImagePointer(0, pMip0Mem, dwMip0Pitch);

	for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
	{
		Vec4f* const pPix = (Vec4f*)&pMip0Mem[dwY * dwMip0Pitch];

		for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
		{
			// Convert from (1 - s * 0.7)^6 to (1 - s)^2
			pPix[dwX].w = 1 - pow(1.0f - pPix[dwX].w * 0.7f, 3.0f);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////

void ImageObject::GlossFromNormals(bool bHasAuthoredGloss)
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return;
	}

	// Derive new roughness from normal variance to preserve the bumpiness of normal map mips and to reduce specular aliasing.
	// The derived roughness is combined with the artist authored roughness stored in the alpha channel of the normal map.
	// The algorithm is based on the Frequency Domain Normal Mapping implementation presented by Neubelt and Pettineo at Siggraph 2013.

	uint32 dwWidth, dwHeight, dwMips;
	GetExtent(dwWidth, dwHeight, dwMips);

	char* pMip0Mem;
	uint32 dwMip0Pitch;
	GetImagePointer(0, pMip0Mem, dwMip0Pitch);

	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		uint32 dwLocalWidth = GetWidth(dwMip);
		uint32 dwLocalHeight = GetHeight(dwMip);

		char* pSrcMem;
		uint32 dwSrcPitch;
		GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

		for (uint32 dwY = 0; dwY < dwLocalHeight; ++dwY)
		{				
			Vec4f* const pPix = (Vec4f*)&pSrcMem[dwY * dwSrcPitch];

			for (uint32 dwX = 0; dwX < dwLocalWidth; ++dwX)
			{
				// Get length of the averaged normal
				Vec4 texel = pPix[dwX];
				Vec3 normal(texel.x * 2.0f - 1.0f, texel.y * 2.0f - 1.0f, texel.z * 2.0f - 1.0f);
				float len = Util::getMax(normal.len(), 1.0f / 32768);

				float authoredSmoothness = bHasAuthoredGloss ? texel.w : 1.0f;
				float finalSmoothness = authoredSmoothness;

				if (len < 1.0f)
				{
					// Convert from smoothness to roughness (needs to match shader code)
					float authoredRoughness = (1.0f - authoredSmoothness) * (1.0f - authoredSmoothness);
					
					// Derive new roughness based on normal variance
					float kappa = (3.0f * len - len * len * len) / (1.0f - len * len);
					float variance = 1.0f / (2.0f * kappa);
					float finalRoughness = sqrtf(authoredRoughness * authoredRoughness + variance);

					// Convert roughness back to smoothness
					finalSmoothness = 1.0f - sqrtf(finalRoughness);
				}

				pPix[dwX].w = finalSmoothness;
			}
		}
	}
}
