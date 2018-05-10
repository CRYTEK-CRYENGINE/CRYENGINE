// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#define _USE_MATH_DEFINES
#include <math.h>                           // M_PI

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog

///////////////////////////////////////////////////////////////////////////////////

static void FilterCubeSample(
	const bool point,
	float  dwSrcX, float  dwSrcY,
	uint32 dwDstX, uint32 dwDstY,
	uint32 dwSrcWidth, uint32 dwSrcHeight, uint32 dwSrcStride, char *pSrcMem, 
	uint32 dwDstWidth, uint32 dwDstHeight, uint32 dwDstStride, char *pDstMem, 
	uint32 dwBPP)
{
	// TODO: implement bilinear for other datatypes than FP32
	if (point)
	{
		// point filtering
		int32 dwSrcO = int32(floor(dwSrcX));
		int32 dwSrcA = int32(floor(dwSrcY));

		assert((dwSrcO >= 0) && (dwSrcO < dwSrcWidth));
		assert((dwSrcA >= 0) && (dwSrcA < dwSrcHeight));

		char* pDst = pDstMem + dwDstY * dwDstStride + dwDstX * dwBPP;
		char* pSrc = pSrcMem + dwSrcA * dwSrcStride + dwSrcO * dwBPP;

		for (uint32 c = 0; c < dwBPP; c++)
		{
			*pDst++ = *pSrc++;
		}
	}
	else
	{
		// bi-linear filtering
		int32 dwSrcV1 = int32(floor(dwSrcX));
		int32 dwSrcH1 = int32(floor(dwSrcY));

		const int32 dwSrcV2 = int32(ceil(dwSrcX));
		const int32 dwSrcH2 = int32(ceil(dwSrcY));

		const float blendV = 1.0f - (dwSrcX - dwSrcV1);
		const float blendH = 1.0f - (dwSrcY - dwSrcH1);

		if (dwSrcV1 < 0)
		{
			dwSrcV1 += dwSrcWidth;
		}

		if (dwSrcH1 < 0)
		{
			dwSrcH1 += dwSrcHeight;
		}

		char* const pDst = pDstMem + dwDstY  * dwDstStride + dwDstX  * dwBPP;
		const char* const pSrc1 = pSrcMem + dwSrcH1 * dwSrcStride + dwSrcV1 * dwBPP;
		const char* const pSrc2 = pSrcMem + dwSrcH1 * dwSrcStride + dwSrcV2 * dwBPP;
		const char* const pSrc3 = pSrcMem + dwSrcH2 * dwSrcStride + dwSrcV1 * dwBPP;
		const char* const pSrc4 = pSrcMem + dwSrcH2 * dwSrcStride + dwSrcV2 * dwBPP;

		const Vec4 vV1 = *((Vec4f*)pSrc1);
		const Vec4 vV2 = *((Vec4f*)pSrc2);
		const Vec4 vV3 = *((Vec4f*)pSrc3);
		const Vec4 vV4 = *((Vec4f*)pSrc4);

		const Vec4 vH1 = vV1 * blendV + vV2 * (1.0f - blendV);
		const Vec4 vH2 = vV3 * blendV + vV4 * (1.0f - blendV);

		*((Vec4f*)pDst) = vH1 * blendH + vH2 * (1.0f - blendH);
	}
}

static void ResampleCubeFaces(
	const bool point,
	uint32 dwSrcSquare, char *pSrcMem, 
	uint32 dwDstSquare, char *pDstMem, 
	uint32 dwBPP, const uint32 (&map)[6][5])
{
	for (uint32 dwF = 0; dwF < 6; ++dwF)
	{
		uint32 dwSrcOffset = map[dwF][0];
		uint32 dwDstOffset = map[dwF][1];
		 int32 dwDirection = map[dwF][2];
		uint32 dwSrcStride = map[dwF][3];
		uint32 dwDstStride = map[dwF][4];

		for (uint32 dwDY = 0, dwIY = (dwDirection < 0 ? dwDstSquare - 1 : 0); dwDY < dwDstSquare; ++dwDY, dwIY += dwDirection)
		{
			float dwSY = float(dwIY * dwSrcSquare) / dwDstSquare;

			for (uint32 dwDX = 0, dwIX = (dwDirection < 0 ? dwDstSquare - 1 : 0); dwDX < dwDstSquare; dwDX++, dwIX += dwDirection)
			{
				float dwSX = float(dwIX * dwSrcSquare) / dwDstSquare;

				FilterCubeSample(
					point,
					dwSX, dwSY,
					dwDX, dwDY,
					dwSrcSquare, dwSrcSquare, dwSrcStride, pSrcMem + dwSrcOffset,
					dwDstSquare, dwDstSquare, dwDstStride, pDstMem + dwDstOffset,
					dwBPP
				);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////

void ImageToProcess::ConvertProbe(bool bPow2)
{
	const EPixelFormat srcPixelFormat = get()->GetPixelFormat();

	uint32 dwSrcWidth, dwSrcHeight, dwSrcMips;
	get()->GetExtent(dwSrcWidth, dwSrcHeight, dwSrcMips);

	// cube strip
	if (dwSrcWidth * 1 == dwSrcHeight * 6)
	{
		// order of faces:
		// +X -X +Y -Y +Z -Z (y is up)
		// ring:
		// +X -Z -X +Z, +Y -Y connects to upper and lower side of +Z
		const uint32 dwSrcSquare = dwSrcHeight / 1;
		const uint32 dwDstSquare = bPow2 ? Util::getFlooredPowerOfTwo(dwSrcSquare) : dwSrcSquare;

		if (dwSrcHeight != dwDstSquare)
		{
			std::auto_ptr<ImageObject> pRet(new ImageObject(dwDstSquare * 6, dwDstSquare, 1, srcPixelFormat, ImageObject::eCubemap_Yes));
			pRet->CopyPropertiesFrom(*get());

			uint32 dwDstWidth, dwDstHeight, dwDstMips;
			pRet->GetExtent(dwDstWidth, dwDstHeight, dwDstMips);

			char *pSrcMem;
			uint32 dwSrcPitch;
			get()->GetImagePointer(0, pSrcMem, dwSrcPitch);

			char *pDstMem;
			uint32 dwDstPitch;
			pRet->GetImagePointer(0, pDstMem, dwDstPitch);

			const uint32 dwBPP = dwDstPitch / dwDstWidth;

			const uint32 map[6][5] =
			{
				{ dwSrcPitch * 0U / 6U, dwDstPitch * 0U / 6U, +1, dwSrcPitch, dwDstPitch },
				{ dwSrcPitch * 1U / 6U, dwDstPitch * 1U / 6U, +1, dwSrcPitch, dwDstPitch },
				{ dwSrcPitch * 2U / 6U, dwDstPitch * 2U / 6U, +1, dwSrcPitch, dwDstPitch },
				{ dwSrcPitch * 3U / 6U, dwDstPitch * 3U / 6U, +1, dwSrcPitch, dwDstPitch },
				{ dwSrcPitch * 4U / 6U, dwDstPitch * 4U / 6U, +1, dwSrcPitch, dwDstPitch },
				{ dwSrcPitch * 5U / 6U, dwDstPitch * 5U / 6U, +1, dwSrcPitch, dwDstPitch },
			};

			ResampleCubeFaces(
				srcPixelFormat != ePixelFormat_A32B32G32R32F,
				dwSrcSquare, pSrcMem,
				dwDstSquare, pDstMem,
				dwBPP, map
			);

			set(pRet.release());
		}

		get()->SetCubemap(ImageObject::eCubemap_Yes);
	}
	// longitude/latitude probe
	else if (dwSrcWidth * 2 == dwSrcHeight * 4)
	{
		const uint32 dwSrcSquare = dwSrcHeight / 2;
		const uint32 dwDstSquare = bPow2 ? Util::getFlooredPowerOfTwo(dwSrcSquare) : dwSrcSquare;

		std::auto_ptr<ImageObject> pRet(new ImageObject(dwDstSquare * 6, dwDstSquare, 1, srcPixelFormat, ImageObject::eCubemap_Yes));
		pRet->CopyPropertiesFrom(*get());

		uint32 dwDstWidth, dwDstHeight, dwDstMips;
		pRet->GetExtent(dwDstWidth, dwDstHeight, dwDstMips);

		char *pSrcMem;
		uint32 dwSrcPitch;
		get()->GetImagePointer(0, pSrcMem, dwSrcPitch);

		char *pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(0, pDstMem, dwDstPitch);

		// power-of-two most of the time, so it's an exact FP number
		const float dwAPP = 1.0f / dwDstHeight;
		const uint32 dwBPP = dwDstPitch / dwDstWidth;

		for (uint32 dwY = 0; dwY < dwDstHeight; ++dwY)
		{
			for (uint32 dwX = 0; dwX < dwDstWidth; ++dwX)
			{
				const uint32 dwS = dwX / dwDstHeight;
				const uint32 dwP = dwX % dwDstHeight;

				const float cY = float(int32(dwY * 2 - dwDstHeight + 1)) * dwAPP;
				const float cP = float(int32(dwP * 2 - dwDstHeight + 1)) * dwAPP;

				Vec3 fvec;
//			Vec3 fvtl, fvbr, fvtr, fvbl;
				switch (dwS)
				{
					case 0: fvec = Vec3(+1.0f,  cY,         cP       ); /*fvtl = fvec + Vec3(+0.0f, -  dwAPP,        -  dwAPP       ); fvtr = fvec + Vec3(+0.0f, -  dwAPP,        +  dwAPP       ); fvbl = fvec + Vec3(+0.0f,  dwAPP,        -  dwAPP       ); fvbr = fvec + Vec3(+0.0f,  dwAPP,         dwAPP       );*/ break;
					case 1: fvec = Vec3(-1.0f,  cY,        -cP       ); /*fvtl = fvec + Vec3(-0.0f, -  dwAPP,        - -dwAPP       ); fvtr = fvec + Vec3(-0.0f, -  dwAPP,        + -dwAPP       ); fvbl = fvec + Vec3(-0.0f,  dwAPP,        - -dwAPP       ); fvbr = fvec + Vec3(-0.0f,  dwAPP,        -dwAPP       );*/ break;
					case 2: fvec = Vec3(        cP, -1.0f, -cY       ); /*fvtl = fvec + Vec3(       -  dwAPP, -0.0f, - -dwAPP       ); fvtr = fvec + Vec3(       -  dwAPP, -0.0f, + -dwAPP       ); fvbl = fvec + Vec3(        dwAPP, -0.0f, - -dwAPP       ); fvbr = fvec + Vec3(        dwAPP, -0.0f, -dwAPP       );*/ break;
					case 3: fvec = Vec3(        cP, +1.0f,  cY       ); /*fvtl = fvec + Vec3(       -  dwAPP, +0.0f, -  dwAPP       ); fvtr = fvec + Vec3(       -  dwAPP, +0.0f, +  dwAPP       ); fvbl = fvec + Vec3(        dwAPP, +0.0f, -  dwAPP       ); fvbr = fvec + Vec3(        dwAPP, +0.0f,  dwAPP       );*/ break;
					case 4: fvec = Vec3(        cP,         cY, -1.0f); /*fvtl = fvec + Vec3(       -  dwAPP,        -  dwAPP, -0.0f); fvtr = fvec + Vec3(       -  dwAPP,        +  dwAPP, -0.0f); fvbl = fvec + Vec3(        dwAPP,        -  dwAPP, -0.0f); fvbr = fvec + Vec3(        dwAPP,         dwAPP, -0.0f);*/ break;
					case 5: fvec = Vec3(       -cP,         cY, +1.0f); /*fvtl = fvec + Vec3(       - -dwAPP,        -  dwAPP, +0.0f); fvtr = fvec + Vec3(       - -dwAPP,        +  dwAPP, +0.0f); fvbl = fvec + Vec3(       -dwAPP,        -  dwAPP, +0.0f); fvbr = fvec + Vec3(       -dwAPP,         dwAPP, +0.0f);*/ break;
				}

				// CryEngine has the Y and Z vector swapped
				{ float y = fvec.y; fvec.y = fvec.z; fvec.z = y; }

				fvec = fvec.normalize();

				float longitude = dwSrcWidth  * (0.5f + atan2f(fvec.x, -fvec.z) / (M_PI * 2.0f));
				float latitude  = dwSrcHeight * (1.0f - acosf (     fvec.y    ) / (M_PI * 1.0f));

				/* TODO: implement larger filtering kernels from the OBR
				 * calculate the OBR of the area mapped from cube to equirectangular map
				 *
				fvtl = fvtl.normalize();
				fvbr = fvbr.normalize();
				fvtr = fvtr.normalize();
				fvbl = fvbl.normalize();

				float lotl = dwSrcWidth  * (0.5f + atan2f(fvtl.x, -fvtl.z) / (M_PI * 2.0f));
				float latl = dwSrcHeight * (1.0f - acosf (     fvtl.y    ) / (M_PI * 1.0f));

				float lobr = dwSrcWidth  * (0.5f + atan2f(fvbr.x, -fvbr.z) / (M_PI * 2.0f));
				float labr = dwSrcHeight * (1.0f - acosf (     fvbr.y    ) / (M_PI * 1.0f));

				float lotr = dwSrcWidth  * (0.5f + atan2f(fvtr.x, -fvtr.z) / (M_PI * 2.0f));
				float latr = dwSrcHeight * (1.0f - acosf (     fvtr.y    ) / (M_PI * 1.0f));

				float lobl = dwSrcWidth  * (0.5f + atan2f(fvbl.x, -fvbl.z) / (M_PI * 2.0f));
				float labl = dwSrcHeight * (1.0f - acosf (     fvbl.y    ) / (M_PI * 1.0f));
				 */

				// TODO: implement bilinear for other datatypes than FP32
				if ((srcPixelFormat != ePixelFormat_A32B32G32R32F) /*|| POINT_FILTERING*/)
				{
					float dwO = longitude + 0.0f;
					float dwA = latitude  + 0.0f;

					// point filtering
					FilterCubeSample(
						true,
						dwO, dwA,
						dwX, dwY,
						dwSrcWidth, dwSrcHeight, dwSrcPitch, pSrcMem,
						dwDstWidth, dwDstHeight, dwDstPitch, pDstMem,
						dwBPP
					);
				}
				else
				{
					float dwO = longitude - 0.5f;
					float dwA = latitude  - 0.5f;

					// bi-linear filtering
					FilterCubeSample(
						false,
						dwO, dwA,
						dwX, dwY,
						dwSrcWidth, dwSrcHeight, dwSrcPitch, pSrcMem,
						dwDstWidth, dwDstHeight, dwDstPitch, pDstMem,
						dwBPP
					);
				}
			}
		}

		set(pRet.release());
		get()->SetCubemap(ImageObject::eCubemap_Yes);
	}
	// horizontal cross
	else if (dwSrcWidth * 3 == dwSrcHeight * 4)
	{
		const uint32 dwSrcSquare = dwSrcHeight / 3;
		const uint32 dwDstSquare = bPow2 ? Util::getFlooredPowerOfTwo(dwSrcSquare) : dwSrcSquare;

		// layout:
		//     +Y
		//  -X +Z +X -Z
		//     -Y
		std::auto_ptr<ImageObject> pRet(new ImageObject(dwDstSquare * 6, dwDstSquare, 1, srcPixelFormat, ImageObject::eCubemap_Yes));
		pRet->CopyPropertiesFrom(*get());

		uint32 dwDstWidth, dwDstHeight, dwDstMips;
		pRet->GetExtent(dwDstWidth, dwDstHeight, dwDstMips);

		char *pSrcMem;
		uint32 dwSrcPitch;
		get()->GetImagePointer(0, pSrcMem, dwSrcPitch);

		char *pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(0, pDstMem, dwDstPitch);

		const uint32 dwBPP = dwDstPitch / dwDstWidth;

		const uint32 map[6][5] =
		{
			{ dwSrcHeight * 0U / 3U * dwSrcPitch + dwSrcPitch * 1U / 4U, dwDstPitch * 2U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 3U * dwSrcPitch + dwSrcPitch * 0U / 4U, dwDstPitch * 1U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 3U * dwSrcPitch + dwSrcPitch * 1U / 4U, dwDstPitch * 4U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 3U * dwSrcPitch + dwSrcPitch * 2U / 4U, dwDstPitch * 0U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 3U * dwSrcPitch + dwSrcPitch * 3U / 4U, dwDstPitch * 5U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 2U / 3U * dwSrcPitch + dwSrcPitch * 1U / 4U, dwDstPitch * 3U / 6U, +1, dwSrcPitch, dwDstPitch },
		};

		ResampleCubeFaces(
			(srcPixelFormat != ePixelFormat_A32B32G32R32F) || (dwSrcSquare == dwDstSquare),
			dwSrcSquare, pSrcMem,
			dwDstSquare, pDstMem,
			dwBPP, map
		);

		set(pRet.release());
	}
	// vertical cross
	else if (dwSrcWidth * 4 == dwSrcHeight * 3)
	{
		// layout:
		//     +Y
		//  -X +Z +X
		//     -Y
		//     -Z
		const uint32 dwSrcSquare = dwSrcHeight / 4;
		const uint32 dwDstSquare = bPow2 ? Util::getFlooredPowerOfTwo(dwSrcSquare) : dwSrcSquare;

		std::auto_ptr<ImageObject> pRet(new ImageObject(dwDstSquare * 6, dwDstSquare, 1, srcPixelFormat, ImageObject::eCubemap_Yes));
		pRet->CopyPropertiesFrom(*get());

		uint32 dwDstWidth, dwDstHeight, dwDstMips;
		pRet->GetExtent(dwDstWidth, dwDstHeight, dwDstMips);

		char *pSrcMem;
		uint32 dwSrcPitch;
		get()->GetImagePointer(0, pSrcMem, dwSrcPitch);

		char *pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(0, pDstMem, dwDstPitch);

		const uint32 dwBPP = dwDstPitch / dwDstWidth;

		// last image at the bottom is double mirrored
		const uint32 map[6][5] =
		{
			{ dwSrcHeight * 0U / 4U * dwSrcPitch + dwSrcPitch * 1U / 3U, dwDstPitch * 2U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 4U * dwSrcPitch + dwSrcPitch * 0U / 3U, dwDstPitch * 1U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 4U * dwSrcPitch + dwSrcPitch * 1U / 3U, dwDstPitch * 4U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 1U / 4U * dwSrcPitch + dwSrcPitch * 2U / 3U, dwDstPitch * 0U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 2U / 4U * dwSrcPitch + dwSrcPitch * 1U / 3U, dwDstPitch * 3U / 6U, +1, dwSrcPitch, dwDstPitch },
			{ dwSrcHeight * 3U / 4U * dwSrcPitch + dwSrcPitch * 1U / 3U, dwDstPitch * 5U / 6U, -1, dwSrcPitch, dwDstPitch },
		};

		ResampleCubeFaces(
			(srcPixelFormat != ePixelFormat_A32B32G32R32F) || (dwSrcSquare == dwDstSquare),
			dwSrcSquare, pSrcMem,
			dwDstSquare, pDstMem,
			dwBPP, map
		);

		set(pRet.release());
	}
	// sphere
	else if (dwSrcWidth * 1 == dwSrcHeight * 1)
	{
		get()->SetCubemap(ImageObject::eCubemap_No);
	}
	// unknown
	else
	{
		get()->SetCubemap(ImageObject::eCubemap_No);
	}
}
