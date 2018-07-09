// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////

void ImageToProcess::AddNormalMap(const ImageObject* pAddBump)
{
	assert(pAddBump);

	uint32 dwWidth,dwHeight,dwMips;
	uint32 dwBumpWidth,dwBumpHeight,dwBumpMips;

	pAddBump->GetExtent(dwBumpWidth,dwBumpHeight,dwBumpMips);
	assert(dwBumpMips == 1);
	get()->GetExtent(dwWidth,dwHeight,dwMips);
	assert(dwMips == 1);

	char *pSrcMem;
	uint32 dwSrcPitch;
	get()->GetImagePointer(0,pSrcMem,dwSrcPitch);

	char *pBumpMem;
	uint32 dwBumpPitch;
	pAddBump->GetImagePointer(0,pBumpMem,dwBumpPitch);

	std::auto_ptr<ImageObject> pRet(new ImageObject(dwWidth, dwHeight, 1, ePixelFormat_A32B32G32R32F, get()->GetCubemap()));
	pRet->CopyPropertiesFrom(*get());

	char *pDstMem;
	uint32 dwDstPitch;
	pRet->GetImagePointer(0,pDstMem,dwDstPitch);

	Vec3 vHalf(128.0f/255.0f,128.0f/255.0f,128.0f/255.0f);

	for(uint32 dwY=0;dwY<dwHeight;++dwY)
	{				
		const float *pSrcPix = (const float *)(&pSrcMem[dwY*dwSrcPitch]);
		float *pDstPix = (float *)(&pDstMem[dwY*dwDstPitch]);

		uint32 dwBumpY = dwY%dwBumpHeight;

		char *pBumpLine = (char *)&pBumpMem[dwBumpY*dwBumpPitch];

		for(uint32 dwX=0;dwX<dwWidth;++dwX)
		{
			uint32 dwBumpX = dwX%dwBumpWidth;

			float *pBumpPix = (float *)&pBumpLine[dwBumpX*sizeof(float)*4];

			Vec3 vBumpNormal(pBumpPix[0],pBumpPix[1],pBumpPix[2]);								// 0..1

			Vec3 vNormal(pSrcPix[0],pSrcPix[1],pSrcPix[2]);												// 0..1

			vBumpNormal = vBumpNormal*2.0f-Vec3(1.0f,1.0f,1.0f);									// -1..0..1
			vNormal = vNormal*2.0f-Vec3(1.0f,1.0f,1.0f);													// -1..0..1

			Matrix33 mTransform;

			mTransform.SetRotationV0V1(Vec3(0,0,1),vBumpNormal.GetNormalized());

			vNormal = mTransform*vNormal;

			// convert from -1..1 to 0..1
			//			if(m_Props.m_bApplyRangeAdjustment)
			vNormal = vNormal*0.5f+vHalf;

			pDstPix[0] = vNormal.x;
			pDstPix[1] = vNormal.y;
			pDstPix[2] = vNormal.z;
			pDstPix[3] = 0;

			pSrcPix += 4;  // jump over RGBA
			pDstPix += 4;  // jump over RGBA
		}
	}

	set(pRet.release());
}
