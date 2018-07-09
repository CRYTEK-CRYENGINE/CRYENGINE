// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if 0
void ImageToProcess::NormalToHeight() const
{
	if (GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return false;
	}

	uint32 dwWidth, dwHeight, dwMips;
	get()->GetExtent(dwWidth, dwHeight, dwMips);

	std::auto_ptr<ImageObject> pImageTmp(new ImageObject(dwWidth, dwHeight, dwMips, ePixelFormat_A32B32G32R32F, get()->GetCubemap()));
	assert(pImageTmp->GetMipCount() == dwMips);

//pRet->CopyPropertiesFrom(*get());

	for (uint32 mip = 0; mip < dwMips; ++mip)
	{
		Color4<float>* const pSrc = get()->GetPixelsPointer<Color4<float> >(mip);
		Color4<float>* const pTmp = pImageTmp->GetPixelsPointer<Color4<float> >(mip);

		const int32 w = get()->GetWidth(mip);
		const int32 h = get()->GetHeight(mip);
		const uint32 pixelCount = w * h;

		if (mip == 0)
		{
			WriteTga("in0.tga", w, h, &pSrc[0].components[0], 4);
		}

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			Vec3 vNormal(
				pSrc[i].components[0] * 2.0f - 1.0f, 
				pSrc[i].components[1] * 2.0f - 1.0f, 
				pSrc[i].components[2] * 2.0f - 1.0f);

			vNormal.NormalizeSafe(Vec3(0.0f, 0.0f, 1.0f));

			pSrc[i].components[0] = vNormal.x;
			pSrc[i].components[1] = vNormal.y;
			pSrc[i].components[2] = vNormal.z;
			//			pSrc[i].components[0] = vNormal.x * 0.5f + 0.5f;
			//			pSrc[i].components[1] = vNormal.y * 0.5f + 0.5f;
			//			pSrc[i].components[2] = vNormal.z * 0.5f + 0.5f;

			// TODO: normalize/clamp normal in pSrc[i]
			pTmp[i].components[0] = 0.0f;
			pTmp[i].components[1] = 0.0f;
			pTmp[i].components[2] = 0.0f;
			pTmp[i].components[3] = 0.0f;
		}	

		if (mip == 0)
		{
			WriteTga("inR.tga", w, h, &pSrc[0].components[0], 4);
			WriteTga("inG.tga", w, h, &pSrc[0].components[1], 4);
			WriteTga("inB.tga", w, h, &pSrc[0].components[2], 4);
		}

		int x, y;

#define S(x, y, n) pSrc[(y) * w + (x)].components[n]
#define R(x, y, n) pTmp[(y) * w + (x)].components[n]

		/* top-left to bottom-right */
		for(x = 1; x < w; ++x)
			R(x, 0, 0) = R(x - 1, 0, 0) + S(x - 1, 0, 0);
		for(y = 1; y < h; ++y)
			R(0, y, 0) = R(0, y - 1, 0) + S(0, y - 1, 1);
		for(y = 1; y < h; ++y)
		{
			for(x = 1; x < w; ++x)
			{
				R(x, y, 0) = (R(x, y - 1, 0) + R(x - 1, y, 0) +
					S(x - 1, y, 0) + S(x, y - 1, 1)) * 0.5f;
			}
		}
		if (mip == 0)
		{
			WriteTga("step0.tga", w, h, &pTmp[0].components[0], 4);
		}

		/* top-right to bottom-left */
		for(x = w - 2; x >= 0; --x)
			R(x, 0, 1) = R(x + 1, 0, 1) - S(x + 1, 0, 0);
		for(y = 1; y < h; ++y)
			R(0, y, 1) = R(0, y - 1, 1) + S(0, y - 1, 1);
		for(y = 1; y < h; ++y)
		{
			for(x = w - 2; x >= 0; --x)
			{
				R(x, y, 1) = (R(x, y - 1, 1) + R(x + 1, y, 1) -
					S(x + 1, y, 0) + S(x, y - 1, 1)) * 0.5f;
			}
		}
		if (mip == 0)
		{
			WriteTga("step1.tga", w, h, &pTmp[0].components[1], 4);
		}

		/* bottom-left to top-right */
		for(x = 1; x < w; ++x)
			R(x, 0, 2) = R(x - 1, 0, 2) + S(x - 1, 0, 0);
		for(y = h - 2; y >= 0; --y)
			R(0, y, 2) = R(0, y + 1, 2) - S(0, y + 1, 1);
		for(y = h - 2; y >= 0; --y)
		{
			for(x = 1; x < w; ++x)
			{
				R(x, y, 2) = (R(x, y + 1, 2) + R(x - 1, y, 2) +
					S(x - 1, y, 0) - S(x, y + 1, 1)) * 0.5f;
			}
		}
		if (mip == 0)
		{
			WriteTga("step2.tga", w, h, &pTmp[0].components[2], 4);
		}

		/* bottom-right to top-left */
		for(x = w - 2; x >= 0; --x)
			R(x, 0, 3) = R(x + 1, 0, 3) - S(x + 1, 0, 0);
		for(y = h - 2; y >= 0; --y)
			R(0, y, 3) = R(0, y + 1, 3) - S(0, y + 1, 1);
		for(y = h - 2; y >= 0; --y)
		{
			for(x = w - 2; x >= 0; --x)
			{
				R(x, y, 3) = (R(x, y + 1, 3) + R(x + 1, y, 3) -
					S(x + 1, y, 0) - S(x, y + 1, 1)) * 0.5f;
			}
		}
		if (mip == 0)
		{
			WriteTga("step3.tga", w, h, &pTmp[0].components[3], 4);
		}

#undef S
#undef R

		/* accumulate, find min/max */
		float hmin =  1e10f;
		float hmax = -1e10f;
		for (uint32 i = 0; i < pixelCount; ++i)
		{
			pTmp[i].components[0] += pTmp[i].components[1] + pTmp[i].components[2] + pTmp[i].components[3];
			Util::clampMax(hmin, pTmp[i].components[0]);
			Util::clampMin(hmax, pTmp[i].components[0]);
		}

		/* scale into 0 - 1 range */
		if (hmin >= hmax)
		{
			for (uint32 i = 0; i < pixelCount; ++i)
			{
				pTmp[i].components[0] = 0;
			}	
		}
		else
		{
			for (uint32 i = 0; i < pixelCount; ++i)
			{   
				float v = (pTmp[i].components[0] - hmin) / (hmax - hmin);
				//				/* adjust contrast */
				//				v = (v - 0.5f) * nmapvals.contrast + v;
				//				Util::clampMinMax(v, 0.0f, 1.0f);
				pTmp[i].components[0] = v;
			}
		}

		if (mip == 0)
		{
			WriteTga("asas.tga", w, h, &pTmp[0].components[0], 4);
		}

		for (uint32 i = 0; i < pixelCount; ++i)
		{
			pSrc[i].components[0] = pTmp[i].components[0];
			pSrc[i].components[1] = pTmp[i].components[0];
			pSrc[i].components[2] = pTmp[i].components[0];
			pSrc[i].components[0] = 1.0f;
		}
	}
}
#endif
