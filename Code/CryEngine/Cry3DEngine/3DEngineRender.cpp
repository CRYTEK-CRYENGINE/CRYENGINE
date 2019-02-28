// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   3denginerender.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "3dEngine.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_water.h"
#include <CryParticleSystem/IParticles.h>
#include "DecalManager.h"
#include "SkyLightManager.h"
#include "terrain.h"
#include "PolygonClipContext.h"
#include "LightEntity.h"
#include "FogVolumeRenderNode.h"
#include "ObjectsTree.h"
#include "WaterWaveRenderNode.h"
#include "MatMan.h"
#include <CryString/CryPath.h>
#include <CryMemory/ILocalMemoryUsage.h>
#include <CryCore/BitFiddling.h>
#include "ObjMan.h"
#include "ParticleMemory.h"
#include "ParticleSystem/ParticleSystem.h"
#include "ObjManCullQueue.h"
#include "MergedMeshRenderNode.h"
#include "GeomCacheManager.h"
#include "DeformableNode.h"
#include "Brush.h"
#include "ClipVolumeManager.h"
#include <Cry3DEngine/ITimeOfDay.h>
#include <CrySystem/Scaleform/IScaleformHelper.h>
#include <CryGame/IGameFramework.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAISystem/IAISystem.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CryGame/IGame.h>
#include "WaterRippleManager.h"

#if defined(FEATURE_SVO_GI)
	#include "SVO/SceneTreeManager.h"
#endif

#ifdef GetCharWidth
	#undef GetCharWidth
#endif //GetCharWidth

////////////////////////////////////////////////////////////////////////////////////////
// RenderScene
////////////////////////////////////////////////////////////////////////////////////////
#define FREE_MEMORY_YELLOW_LIMIT (30)
#define FREE_MEMORY_RED_LIMIT    (10)
#define DISPLAY_INFO_SCALE       (1.25f)
#define DISPLAY_INFO_SCALE_SMALL (1.1f)
#define STEP_SMALL_DIFF          (2.f)

#if CRY_PLATFORM_WINDOWS
// for panorama screenshots
class CStitchedImage : public Cry3DEngineBase
{
public:
	CStitchedImage(C3DEngine& rEngine,
	               const uint32 dwWidth,
	               const uint32 dwHeight,
	               const uint32 dwVirtualWidth,
	               const uint32 dwVirtualHeight,
	               const uint32 dwSliceCount,
	               const f32 fTransitionSize,
	               const bool bMetaData = false) :
		m_rEngine(rEngine),
		m_dwWidth(dwWidth),
		m_dwHeight(dwHeight),
		m_fInvWidth(1.f / static_cast<f32>(dwWidth)),
		m_fInvHeight(1.f / static_cast<f32>(dwHeight)),
		m_dwVirtualWidth(dwVirtualWidth),
		m_dwVirtualHeight(dwVirtualHeight),
		m_fInvVirtualWidth(1.f / static_cast<f32>(dwVirtualWidth)),
		m_fInvVirtualHeight(1.f / static_cast<f32>(dwVirtualHeight)),
		m_nFileId(0),
		m_dwSliceCount(dwSliceCount),
		m_fHorizFOV(2 * gf_PI / dwSliceCount),
		m_bFlipY(true),
		m_fTransitionSize(fTransitionSize),
		m_bMetaData(bMetaData)
	{
		assert(dwWidth);
		assert(dwHeight);

		const char* szExtension = m_rEngine.GetCVars()->e_ScreenShotFileFormat->GetString();

		m_bFlipY = (stricmp(szExtension, "tga") == 0);

		m_RGB.resize(m_dwWidth * 3 * m_dwHeight);

		// ratio between width and height defines angle 1 (angle from mid to cylinder edges)
		float fVert1Frac = (2 * gf_PI * m_dwHeight) / m_dwWidth;

		// slice count defines angle 2
		float fHorizFrac = tanf(GetHorizFOVWithBorder() * 0.5f);
		float fVert2Frac = 2.0f * fHorizFrac / rEngine.GetRenderer()->GetOverlayWidth() * rEngine.GetRenderer()->GetOverlayHeight();

		// the bigger one defines the needed angle
		float fVertFrac = max(fVert1Frac, fVert2Frac);

		// planar image becomes a barrel after projection and we need to zoom in to only utilize the usable part (inner rect)
		// this is not always needed - for quality with low slice count we could be save some quality here
		fVertFrac /= cosf(GetHorizFOVWithBorder() * 0.5f);

		// compute FOV from Frac
		float fVertFOV = 2 * atanf(0.5f * fVertFrac);

		m_fPanoramaShotVertFOV = fVertFOV;

		CryLog("RenderFov = %f degrees (%f = max(%f,%f)*fix)", RAD2DEG(m_fPanoramaShotVertFOV), fVertFrac, fVert1Frac, fVert2Frac);
		Clear();
	}

	void Clear()
	{
		memset(&m_RGB[0], 0, m_dwWidth * m_dwHeight * 3);
	}

	// szDirectory + "/" + file_id + "." + extension
	// logs errors in the case there are problems
	bool SaveImage(const char* szDirectory)
	{
		assert(szDirectory);

		const char* szExtension = m_rEngine.GetCVars()->e_ScreenShotFileFormat->GetString();

		if (stricmp(szExtension, "dds") != 0 &&
		    stricmp(szExtension, "tga") != 0 &&
		    stricmp(szExtension, "jpg") != 0)
		{
			gEnv->pLog->LogError("Format e_ScreenShotFileFormat='%s' not supported", szExtension);
			return false;
		}

		char sFileName[_MAX_PATH];

		cry_sprintf(sFileName, "%s/ScreenShots/%s", PathUtil::GetGameFolder().c_str(), szDirectory);
		CryCreateDirectory(sFileName);

		// find free file id
		for (;; )
		{
			cry_sprintf(sFileName, "%s/ScreenShots/%s/%.5d.%s", PathUtil::GetGameFolder().c_str(), szDirectory, m_nFileId, szExtension);

			FILE* fp = gEnv->pCryPak->FOpen(sFileName, "rb");

			if (!fp)
				break; // file doesn't exist

			gEnv->pCryPak->FClose(fp);
			m_nFileId++;
		}

		bool bOk;

		if (stricmp(szExtension, "dds") == 0)
			bOk = gEnv->pRenderer->WriteDDS((byte*)&m_RGB[0], m_dwWidth, m_dwHeight, 4, sFileName, eTF_BC3, 1);
		else if (stricmp(szExtension, "tga") == 0)
			bOk = gEnv->pRenderer->WriteTGA((byte*)&m_RGB[0], m_dwWidth, m_dwHeight, sFileName, 24, 24);
		else
			bOk = gEnv->pRenderer->WriteJPG((byte*)&m_RGB[0], m_dwWidth, m_dwHeight, sFileName, 24);

		if (!bOk)
			gEnv->pLog->LogError("Failed to write '%s' (not supported on this platform?)", sFileName);
		else //write meta data
		{
			if (m_bMetaData)
			{
				const f32 nTSize = static_cast<f32>(gEnv->p3DEngine->GetTerrainSize());
				const f32 fSizeX = GetCVars()->e_ScreenShotMapSizeX;
				const f32 fSizeY = GetCVars()->e_ScreenShotMapSizeY;
				const f32 fTLX = GetCVars()->e_ScreenShotMapCenterX - fSizeX;
				const f32 fTLY = GetCVars()->e_ScreenShotMapCenterY - fSizeY;
				const f32 fBRX = GetCVars()->e_ScreenShotMapCenterX + fSizeX;
				const f32 fBRY = GetCVars()->e_ScreenShotMapCenterY + fSizeY;

				cry_sprintf(sFileName, "%s/ScreenShots/%s/%.5d.%s", PathUtil::GetGameFolder().c_str(), szDirectory, m_nFileId, "xml");

				FILE* metaFile = gEnv->pCryPak->FOpen(sFileName, "wt");
				if (metaFile)
				{
					char sFileData[1024];
					cry_sprintf(sFileData, "<MiniMap Filename=\"%.5d.%s\" startX=\"%f\" startY=\"%f\" endX=\"%f\" endY=\"%f\"/>",
					            m_nFileId, szExtension, fTLX, fTLY, fBRX, fBRY);
					string data(sFileData);
					gEnv->pCryPak->FWrite(data.c_str(), data.size(), metaFile);
					gEnv->pCryPak->FClose(metaFile);
				}
			}
		}

		return bOk;
	}

	// rasterize rectangle
	// Arguments:
	//   x0 - <x1, including
	//   y0 - <y1, including
	//   x1 - >x0, excluding
	//   y1 - >y0, excluding
	void RasterizeRect(const uint32* pRGBAImage,
	                   const uint32 dwWidth,
	                   const uint32 dwHeight,
	                   const uint32 dwSliceX,
	                   const uint32 dwSliceY,
	                   const f32 fTransitionSize,
	                   const bool bFadeBordersX,
	                   const bool bFadeBordersY)
	{
		//		if(bFadeBordersX|bFadeBordersY)
		{
			//calculate rect inside the whole image
			const int32 OrgX0 = static_cast<int32>(static_cast<f32>((dwSliceX * dwWidth) * m_dwWidth) * m_fInvVirtualWidth);
			const int32 OrgY0 = static_cast<int32>(static_cast<f32>((dwSliceY * dwHeight) * m_dwHeight) * m_fInvVirtualHeight);
			const int32 OrgX1 = min(static_cast<int32>(static_cast<f32>(((dwSliceX + 1) * dwWidth) * m_dwWidth) * m_fInvVirtualWidth), static_cast<int32>(m_dwWidth)) - (m_rEngine.GetCVars()->e_ScreenShotDebug == 1 ? 1 : 0);
			const int32 OrgY1 = min(static_cast<int32>(static_cast<f32>(((dwSliceY + 1) * dwHeight) * m_dwHeight) * m_fInvVirtualHeight), static_cast<int32>(m_dwHeight)) - (m_rEngine.GetCVars()->e_ScreenShotDebug == 1 ? 1 : 0);
			//expand bounds for borderblending
			const int32 CenterX = (OrgX0 + OrgX1) / 2;
			const int32 CenterY = (OrgY0 + OrgY1) / 2;
			const int32 X0 = static_cast<int32>(static_cast<f32>(OrgX0 - CenterX) * (1.f + fTransitionSize)) + CenterX;
			const int32 Y0 = static_cast<int32>(static_cast<f32>(OrgY0 - CenterY) * (1.f + fTransitionSize)) + CenterY;
			const int32 X1 = static_cast<int32>(static_cast<f32>(OrgX1 - CenterX) * (1.f + fTransitionSize)) + CenterX;
			const int32 Y1 = static_cast<int32>(static_cast<f32>(OrgY1 - CenterY) * (1.f + fTransitionSize)) + CenterY;
			//clip bounds -- disabled due to some artifacts on edges
			//X0	=	min(max(X0,0),static_cast<int32>(m_dwWidth-1));
			//Y0	=	min(max(Y0,0),static_cast<int32>(m_dwHeight-1));
			//X1	=	min(max(X1,0),static_cast<int32>(m_dwWidth-1));
			//Y1	=	min(max(Y1,0),static_cast<int32>(m_dwHeight-1));
			const f32 InvBlendX = 1.f / max(static_cast<f32>(X1 - OrgX1), 0.01f);//0.5 is here because the border is two times wider then the border of the single segment in total
			const f32 InvBlendY = 1.f / max(static_cast<f32>(Y1 - OrgY1), 0.01f);
			const int32 DebugScale = (m_rEngine.GetCVars()->e_ScreenShotDebug == 2) ? 65536 : 0;
			for (int32 y = max(Y0, 0); y < Y1 && y < (int)m_dwHeight; y++)
			{
				const f32 WeightY = bFadeBordersY ? min(1.f, static_cast<f32>(min(y - Y0, Y1 - y)) * InvBlendY) : 1.f;
				for (int32 x = max(X0, 0); x < X1 && x < (int)m_dwWidth; x++)
				{
					uint8* pDst = &m_RGB[m_bFlipY ? 3 * (x + (m_dwHeight - y - 1) * m_dwWidth) : 3 * (x + y * m_dwWidth)];
					const f32 WeightX = bFadeBordersX ? min(1.f, static_cast<f32>(min(x - X0, X1 - x)) * InvBlendX) : 1.f;
					GetBilinearFilteredBlend(static_cast<int32>(static_cast<f32>(x - X0) / static_cast<f32>(X1 - X0) * static_cast<f32>(dwWidth) * 16.f),
					                         static_cast<int32>(static_cast<f32>(y - Y0) / static_cast<f32>(Y1 - Y0) * static_cast<f32>(dwHeight) * 16.f),
					                         pRGBAImage, dwWidth, dwHeight,
					                         max(static_cast<int32>(WeightX * WeightY * 65536.f), DebugScale), pDst);
				}
			}
		}
		/*		else
		   {
		   const int32 X0	=	static_cast<int32>(static_cast<f32>((dwSliceX*dwWidth)*m_dwWidth)*m_fInvVirtualWidth);
		   const int32 Y0	=	static_cast<int32>(static_cast<f32>((dwSliceY*dwHeight)*m_dwHeight)*m_fInvVirtualHeight);
		   const int32 X1	=	min(static_cast<int32>(static_cast<f32>(((dwSliceX+1)*dwWidth)*m_dwWidth)*m_fInvVirtualWidth),static_cast<int32>(m_dwWidth))-(m_rEngine.GetCVars()->e_ScreenShotDebug==1?1:0);
		   const int32 Y1	=	min(static_cast<int32>(static_cast<f32>(((dwSliceY+1)*dwHeight)*m_dwHeight)*m_fInvVirtualHeight),static_cast<int32>(m_dwHeight))-(m_rEngine.GetCVars()->e_ScreenShotDebug==1?1:0);
		   for(int32 y=Y0;y<Y1;y++)
		   for(int32 x=X0;x<X1;x++)
		   {
		   uint8 *pDst = &m_RGB[m_bFlipY?3*(x+(m_dwHeight-y-1)*m_dwWidth):3*(x+y*m_dwWidth)];
		   GetBilinearFiltered(static_cast<int32>(static_cast<f32>(x*m_dwVirtualWidth*16)*m_fInvWidth)-((dwSliceX*dwWidth)*16),
		   static_cast<int32>(static_cast<f32>(y*m_dwVirtualHeight*16)*m_fInvHeight)-((dwSliceY*dwHeight)*16),
		   pRGBAImage,dwWidth,dwHeight,pDst);
		   }
		   }*/
	}

	void RasterizeCylinder(const uint32* pRGBAImage,
	                       const uint32 dwWidth,
	                       const uint32 dwHeight,
	                       const uint32 dwSlice,
	                       const bool bFadeBorders,
	                       const CCamera& camera)
	{
		float fSrcAngleMin = GetSliceAngle(dwSlice - 1);
		float fFractionVert = tanf(m_fPanoramaShotVertFOV * 0.5f);
		float fFractionHoriz = fFractionVert * camera.GetProjRatio();
		float fInvFractionHoriz = 1.0f / fFractionHoriz;

		// for soft transition
		float fFadeOutFov = GetHorizFOVWithBorder();
		float fFadeInFov = GetHorizFOV();

		int x0 = 0, y0 = 0, x1 = m_dwWidth, y1 = m_dwHeight;

		float fScaleX = 1.0f / m_dwWidth;
		float fScaleY = 0.5f * fInvFractionHoriz / (m_dwWidth / (2 * gf_PI)) / dwHeight * dwWidth;                // this value is not correctly computed yet - but using many slices reduced the problem

		if (m_bFlipY)
			fScaleY = -fScaleY;

		// it's more efficient to process colums than lines
		for (int x = x0; x < x1; ++x)
		{
			uint8* pDst = &m_RGB[3 * (x + y0 * m_dwWidth)];
			float fSrcX = x * fScaleX - 0.5f;   // -0.5 .. 0.5
			float fSrcAngleX = fSrcAngleMin + 2 * gf_PI * fSrcX;

			if (fSrcAngleX > gf_PI)
				fSrcAngleX -= 2 * gf_PI;
			if (fSrcAngleX < -gf_PI)
				fSrcAngleX += 2 * gf_PI;

			if (fabs(fSrcAngleX) > fFadeOutFov * 0.5f)
				continue;                             // clip away curved parts of the barrel

			float fScrPosX = (tanf(fSrcAngleX) * 0.5f * fInvFractionHoriz + 0.5f) * dwWidth;
			//			float fInvCosSrcX = 1.0f / cos(fSrcAngleX);
			float fInvCosSrcX = 1.0f / cosf(fSrcAngleX);

			if (fScrPosX >= 0 && fScrPosX <= (float)dwWidth)   // this is an optimization - but it could be done even more efficient
				if (fInvCosSrcX > 0)                             // don't render the viewer opposing direction
				{
					int iSrcPosX16 = (int)(fScrPosX * 16.0f);

					float fYOffset = 16 * 0.5f * dwHeight - 16 * 0.5f * m_dwHeight * fScaleY * fInvCosSrcX * dwHeight;
					float fYMul = 16 * fScaleY * fInvCosSrcX * dwHeight;

					float fSrcY = y0 * fYMul + fYOffset;

					uint32 dwLerp64k = 256 * 256 - 1;

					if (!bFadeBorders)
					{
						// first pass - every second image without soft borders
						for (int y = y0; y < y1; ++y, fSrcY += fYMul, pDst += m_dwWidth * 3)
							GetBilinearFiltered(iSrcPosX16, (int)fSrcY, pRGBAImage, dwWidth, dwHeight, pDst);
					}
					else
					{
						// second pass - do all the inbetween with soft borders
						float fOffSlice = fabs(fSrcAngleX / fFadeInFov) - 0.5f;

						if (fOffSlice < 0)
							fOffSlice = 0;      // no transition in this area

						float fBorder = (fFadeOutFov - fFadeInFov) * 0.5f;

						if (fBorder < 0.001f)
							fBorder = 0.001f;   // we do not have border

						float fFade = 1.0f - fOffSlice * fFadeInFov / fBorder;

						if (fFade < 0.0f)
							fFade = 0.0f;       // don't use this slice here

						dwLerp64k = (uint32)(fFade * (256.0f * 256.0f - 1.0f));   // 0..64k

						if (dwLerp64k)                             // optimization
							for (int y = y0; y < y1; ++y, fSrcY += fYMul, pDst += m_dwWidth * 3)
								GetBilinearFilteredBlend(iSrcPosX16, (int)fSrcY, pRGBAImage, dwWidth, dwHeight, dwLerp64k, pDst);
					}
				}
		}
	}

	// fast, rgb only
	static inline ColorB lerp(const ColorB x, const ColorB y, const uint32 a, const uint32 dwBase)
	{
		const int32 b = dwBase - a;
		const int32 RC = dwBase / 2; //rounding correction

		return ColorB(((int)x.r * b + (int)y.r * a + RC) / dwBase,
		              ((int)x.g * b + (int)y.g * a + RC) / dwBase,
		              ((int)x.b * b + (int)y.b * a + RC) / dwBase);
	}

	static inline ColorB Mul(const ColorB x, const int32 a, const int32 dwBase)
	{
		return ColorB(((int)x.r * (int)a) / dwBase,
		              ((int)x.g * (int)a) / dwBase,
		              ((int)x.b * (int)a) / dwBase);
	}
	static inline ColorB MadSaturate(const ColorB x, const int32 a, const int32 dwBase, const ColorB y)
	{
		const int32 MAX_COLOR = 0xff;
		const ColorB PreMuled = Mul(x, a, dwBase);
		return ColorB(min((int)PreMuled.r + (int)y.r, MAX_COLOR),
		              min((int)PreMuled.g + (int)y.g, MAX_COLOR),
		              min((int)PreMuled.b + (int)y.b, MAX_COLOR));
	}

	// bilinear filtering in fixpoint,
	// 4bit fractional part -> multiplier 16
	// --lookup outside of the image is not defined
	//	lookups outside the image are now clamped, needed due to some float inaccuracy while rasterizing a rect-screenshot
	// Arguments:
	//   iX16 - fX mul 16
	//   iY16 - fY mul 16
	//   result - [0]=red, [1]=green, [2]=blue
	static inline bool GetBilinearFilteredRaw(const int iX16, const int iY16,
	                                          const uint32* pRGBAImage,
	                                          const uint32 dwWidth, const uint32 dwHeight,
	                                          ColorB& result)
	{
		int iLocalX = min(max(iX16 / 16, 0), static_cast<int>(dwWidth - 1));
		int iLocalY = min(max(iY16 / 16, 0), static_cast<int>(dwHeight - 1));

		int iLerpX = iX16 & 0xf;      // 0..15
		int iLerpY = iY16 & 0xf;      // 0..15

		ColorB colS[4];

		const uint32* pRGBA = &pRGBAImage[iLocalX + iLocalY * dwWidth];

		colS[0] = pRGBA[0];
		colS[1] = pRGBA[1];
		colS[2] = pRGBA[iLocalY + 1uL < dwHeight ? dwWidth : 0];
		colS[3] = pRGBA[(iLocalX + 1uL < dwWidth ? 1 : 0) + (iLocalY + 1uL < dwHeight ? dwWidth : 0)];

		ColorB colTop, colBottom;

		colTop = lerp(colS[0], colS[1], iLerpX, 16);
		colBottom = lerp(colS[2], colS[3], iLerpX, 16);

		result = lerp(colTop, colBottom, iLerpY, 16);
		return true;
	}

	// blend with background
	static inline bool GetBilinearFiltered(const int iX16, const int iY16,
	                                       const uint32* pRGBAImage,
	                                       const uint32 dwWidth, const uint32 dwHeight,
	                                       uint8 result[3])
	{
		ColorB colFiltered;
		if (GetBilinearFilteredRaw(iX16, iY16, pRGBAImage, dwWidth, dwHeight, colFiltered))
		{
			result[0] = colFiltered.r;
			result[1] = colFiltered.g;
			result[2] = colFiltered.b;
			return true;
		}
		return false;
	}

	static inline bool GetBilinearFilteredBlend(const int iX16, const int iY16,
	                                            const uint32* pRGBAImage,
	                                            const uint32 dwWidth, const uint32 dwHeight,
	                                            const uint32 dwLerp64k,
	                                            uint8 result[3])
	{
		ColorB colFiltered;
		if (GetBilinearFilteredRaw(iX16, iY16, pRGBAImage, dwWidth, dwHeight, colFiltered))
		{
			ColorB colRet = lerp(ColorB(result[0], result[1], result[2]), colFiltered, dwLerp64k, 256 * 256);

			result[0] = colRet.r;
			result[1] = colRet.g;
			result[2] = colRet.b;
			return true;
		}
		return false;
	}

	static inline bool GetBilinearFilteredAdd(const int iX16, const int iY16,
	                                          const uint32* pRGBAImage,
	                                          const uint32 dwWidth, const uint32 dwHeight,
	                                          const uint32 dwLerp64k,
	                                          uint8 result[3])
	{
		ColorB colFiltered;
		if (GetBilinearFilteredRaw(iX16, iY16, pRGBAImage, dwWidth, dwHeight, colFiltered))
		{
			ColorB colRet = MadSaturate(colFiltered, dwLerp64k, 256 * 256, ColorB(result[0], result[1], result[2]));

			result[0] = colRet.r;
			result[1] = colRet.g;
			result[2] = colRet.b;
			return true;
		}
		return false;
	}

	float GetSliceAngle(const uint32 dwSlice) const
	{
		uint32 dwAlternatingSlice = (dwSlice * 2) % m_dwSliceCount;

		float fAngleStep = m_fHorizFOV;

		float fRet = fAngleStep * dwAlternatingSlice;

		if (dwSlice * 2 >= m_dwSliceCount)
			fRet += fAngleStep;

		return fRet;
	}

	float GetHorizFOV() const
	{
		return m_fHorizFOV;
	}

	float GetHorizFOVWithBorder() const
	{
		return m_fHorizFOV * (1.0f + m_fTransitionSize);
	}

	void*  GetBuffer() { return &m_RGB[0]; }
	uint32 GetWidth()  { return m_dwWidth; }
	uint32 GetHeight() { return m_dwHeight; }

	//private: // -------------------------------------------------------------------

	uint32             m_dwWidth;                       // >0
	uint32             m_dwHeight;                      // >0
	f32                m_fInvWidth;                     // >0
	f32                m_fInvHeight;                    // >0
	uint32             m_dwVirtualWidth;                // >0
	uint32             m_dwVirtualHeight;               // >0
	f32                m_fInvVirtualWidth;              // >0
	f32                m_fInvVirtualHeight;             // >0
	std::vector<uint8> m_RGB;                           // [channel + x*3 + m_dwWidth*3*y], channel=0..2, x<m_dwWidth, y<m_dwHeight, no alpha channel to occupy less memory
	uint32             m_nFileId;                       // counts up until it finds free file id
	bool               m_bFlipY;                        // might be useful for some image formats
	bool               m_bMetaData;                     // output additional metadata

	float              m_fPanoramaShotVertFOV;          // -1 means not set yet - in radians

private:

	uint32     m_dwSliceCount;                          //
	C3DEngine& m_rEngine;                               //
	float      m_fHorizFOV;                             // - in radians
	float      m_fTransitionSize;                       // [0..1], 0=no transition, 1.0=full transition
};
#endif

enum EScreenShotType
{
	ESST_NONE    = 0,
	ESST_HIGHRES = 1,
	ESST_PANORAMA,
	ESST_MAP_DELAYED,
	ESST_MAP,
	ESST_SWMAP,
	ESST_SWMAP_DELAYED,
};

void C3DEngine::ScreenshotDispatcher(const int nRenderFlags, const SRenderingPassInfo& passInfo)
{
#if CRY_PLATFORM_WINDOWS
	CStitchedImage* pStitchedImage = 0;

	const uint32 ImageWidth = max(1, min(4096, GetCVars()->e_ScreenShotWidth));
	const uint32 ImageHeight = max(1, min(4096, GetCVars()->e_ScreenShotHeight));

	switch (abs(GetCVars()->e_ScreenShot))
	{
	case ESST_HIGHRES:
		{
			GetConsole()->ShowConsole(false);
			pStitchedImage = new CStitchedImage(*this, ImageWidth, ImageHeight, 0, 0, 1, 0);

			CCamera newCamera = passInfo.GetCamera();
			newCamera.SetFrustum(ImageWidth, ImageHeight, passInfo.GetCamera().GetFov(), passInfo.GetCamera().GetNearPlane(),
								 passInfo.GetCamera().GetFarPlane(), passInfo.GetCamera().GetPixelAspectRatio());

			ScreenShotHighRes(pStitchedImage, nRenderFlags, SRenderingPassInfo::CreateTempRenderingInfo(newCamera, passInfo), 0, 0);

			pStitchedImage->SaveImage("HiRes");
			pStitchedImage->Clear();    // good for debugging
			delete pStitchedImage;
			if (GetCVars()->e_ScreenShot > 0)      // <0 is used for multiple frames (videos)
				GetCVars()->e_ScreenShot = 0;
		}
		break;
	case ESST_PANORAMA:
		{
			CRY_ASSERT_MESSAGE(0, "Panorama screenshot not supported right now !");
			GetConsole()->ShowConsole(false);
			pStitchedImage = new CStitchedImage(*this, ImageWidth, ImageHeight, 0, 0, 1, 0);
			ScreenShotPanorama(pStitchedImage, nRenderFlags, passInfo, 0, 0);
			pStitchedImage->SaveImage("Panorama");
			pStitchedImage->Clear();    // good for debugging
			delete pStitchedImage;
			if (GetCVars()->e_ScreenShot > 0)      // <0 is used for multiple frames (videos)
				GetCVars()->e_ScreenShot = 0;
		}
		break;
	case ESST_MAP_DELAYED:
		{
			GetCVars()->e_ScreenShot = sgn(GetCVars()->e_ScreenShot) * ESST_MAP;   // sgn() to keep sign bit , <0 is used for multiple frames (videos)
			if (CTerrain* const pTerrain = GetTerrain())
			{
				pTerrain->ResetTerrainVertBuffers(NULL);
			}
		}
		break;
	case ESST_SWMAP_DELAYED:
		{
			GetCVars()->e_ScreenShot = sgn(GetCVars()->e_ScreenShot) * ESST_SWMAP;   // sgn() to keep sign bit , <0 is used for multiple frames (videos)
			if (CTerrain* const pTerrain = GetTerrain())
			{
				pTerrain->ResetTerrainVertBuffers(NULL);
			}
		}
		break;
	case ESST_SWMAP:
	case ESST_MAP:
		{
			uint32 mipMapSnapshotSize = 512;
			if (ICVar *pMiniMapRes = gEnv->pConsole->GetCVar("e_ScreenShotMapResolution"))
			{
				mipMapSnapshotSize = pMiniMapRes->GetIVal();
			}
			
			GetConsole()->ShowConsole(false);
			pStitchedImage = new CStitchedImage(*this, mipMapSnapshotSize, mipMapSnapshotSize, 0, 0, 1, 0, true);

			ScreenShotMap(pStitchedImage, nRenderFlags, passInfo, 0, 0);
			if (abs(GetCVars()->e_ScreenShot) == ESST_MAP)
				pStitchedImage->SaveImage("Map");

			if (m_pScreenshotCallback)
			{
				const f32 fSizeX = GetCVars()->e_ScreenShotMapSizeX;
				const f32 fSizeY = GetCVars()->e_ScreenShotMapSizeY;
				const f32 fTLX = GetCVars()->e_ScreenShotMapCenterX - fSizeX;
				const f32 fTLY = GetCVars()->e_ScreenShotMapCenterY - fSizeY;
				const f32 fBRX = GetCVars()->e_ScreenShotMapCenterX + fSizeX;
				const f32 fBRY = GetCVars()->e_ScreenShotMapCenterY + fSizeY;

				m_pScreenshotCallback->SendParameters(pStitchedImage->GetBuffer(), mipMapSnapshotSize, mipMapSnapshotSize, fTLX, fTLY, fBRX, fBRY);
			}

			pStitchedImage->Clear();    // good for debugging
			delete pStitchedImage;

		}
		if (GetCVars()->e_ScreenShot > 0)      // <0 is used for multiple frames (videos)
			GetCVars()->e_ScreenShot = 0;

		if (CTerrain* const pTerrain = GetTerrain())
		{
			pTerrain->ResetTerrainVertBuffers(NULL);
		}
		break;
	default:
		GetCVars()->e_ScreenShot = 0;
	}
#endif //#if CRY_PLATFORM_WINDOWS
}

struct SDebugFrustrum
{
	Vec3        m_vPos[8];
	const char* m_szName;
	CTimeValue  m_TimeStamp;
	ColorB      m_Color;
	float       m_fQuadDist;        // < 0 if not used
};

static std::vector<SDebugFrustrum> g_DebugFrustrums;

void C3DEngine::DebugDraw_Draw()
{
#ifndef _RELEASE
	if (!gEnv->pRenderer)
	{
		return;
	}

	if (m_DebugDrawListMgr.IsEnabled())
		m_DebugDrawListMgr.Update();

	CTimeValue CurrentTime = gEnv->pTimer->GetFrameStartTime();

	IRenderAuxGeom* pAux = IRenderAuxGeom::GetAux();

	SAuxGeomRenderFlags oldFlags = pAux->GetRenderFlags();
	SAuxGeomRenderFlags newFlags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	newFlags.SetDepthWriteFlag(e_DepthWriteOff);
	pAux->SetRenderFlags(newFlags);
	std::vector<SDebugFrustrum>::iterator it;

	for (it = g_DebugFrustrums.begin(); it != g_DebugFrustrums.end(); )
	{
		SDebugFrustrum& ref = *it;

		float fRatio = (CurrentTime - ref.m_TimeStamp).GetSeconds() * 2.0f;

		if (fRatio > 1.0f)
		{
			it = g_DebugFrustrums.erase(it);
			continue;
		}

		vtx_idx pnInd[8] = { 0, 4, 1, 5, 2, 6, 3, 7 };

		float fRadius = ((ref.m_vPos[0] + ref.m_vPos[1] + ref.m_vPos[2] + ref.m_vPos[3]) - (ref.m_vPos[4] + ref.m_vPos[5] + ref.m_vPos[6] + ref.m_vPos[7])).GetLength() * 0.25f;
		float fDistance = min(fRadius, 33.0f);   // in meters

		float fRenderRatio = fRatio * fDistance / fRadius;

		if (ref.m_fQuadDist > 0)
			fRenderRatio = ref.m_fQuadDist / fRadius;

		Vec3 vPos[4];

		for (uint32 i = 0; i < 4; ++i)
			vPos[i] = ref.m_vPos[i] * fRenderRatio + ref.m_vPos[i + 4] * (1.0f - fRenderRatio);

		Vec3 vMid = (vPos[0] + vPos[1] + vPos[2] + vPos[3]) * 0.25f;

		ColorB col = ref.m_Color;

		if (ref.m_fQuadDist <= 0)
		{
			for (uint32 i = 0; i < 4; ++i)
				vPos[i] = vPos[i] * 0.95f + vMid * 0.05f;

			// quad
			if (ref.m_fQuadDist != -999.f)
			{
				pAux->DrawTriangle(vPos[0], col, vPos[2], col, vPos[1], col);
				pAux->DrawTriangle(vPos[2], col, vPos[0], col, vPos[3], col);
			}
			// projection lines
			pAux->DrawLines(ref.m_vPos, 8, pnInd, 2, RGBA8(0xff, 0xff, 0x1f, 0xff));
			pAux->DrawLines(ref.m_vPos, 8, pnInd + 2, 2, RGBA8(0xff, 0xff, 0x1f, 0xff));
			pAux->DrawLines(ref.m_vPos, 8, pnInd + 4, 2, RGBA8(0xff, 0xff, 0x1f, 0xff));
			pAux->DrawLines(ref.m_vPos, 8, pnInd + 6, 2, RGBA8(0xff, 0xff, 0x1f, 0xff));
		}
		else
		{
			// rectangle
			pAux->DrawPolyline(vPos, 4, true, RGBA8(0xff, 0xff, 0x1f, 0xff));
		}

		++it;
	}

	pAux->SetRenderFlags(oldFlags);

	if (GetCVars()->e_DebugDraw == 16)
	{
		DebugDraw_UpdateDebugNode();
	}
	else if (GetRenderer())
	{
		GetRenderer()->SetDebugRenderNode(NULL);
	}

#endif //_RELEASE
}

void C3DEngine::DebugDraw_UpdateDebugNode()
{
#ifndef _RELEASE

	ray_hit rayHit;

	// use cam, no need for firing pos/dir
	const CCamera& cam = GetISystem()->GetViewCamera();
	const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
	const float hitRange = 2000.f;

	if (gEnv->pPhysicalWorld->RayWorldIntersection(cam.GetPosition() + cam.GetViewdir(), cam.GetViewdir() * hitRange, ent_all, flags, &rayHit, 1))
	{
		int type = rayHit.pCollider->GetiForeignData();

		if (type == PHYS_FOREIGN_ID_STATIC)
		{
			IRenderNode* pRenderNode = (IRenderNode*)rayHit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);

			if (pRenderNode)
			{
				gEnv->pRenderer->SetDebugRenderNode(pRenderNode);

				//CryLogAlways(	"Hit: %s, ipart: %d, partid: %d, surafce_idx: %d, iNode: %d, \n",
				//pRenderNode->GetName(), rayHit.ipart, rayHit.partid, rayHit.surface_idx, rayHit.iNode);
			}
		}
		else if (type == PHYS_FOREIGN_ID_ENTITY)
		{
			IEntity* pEntity = (IEntity*)rayHit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

			if (pEntity)
			{
				IEntityRender* pIEntityRender = pEntity->GetRenderInterface();

				{
					IRenderNode* pRenderNode = pIEntityRender->GetRenderNode();

					if (pRenderNode)
					{
						gEnv->pRenderer->SetDebugRenderNode(pRenderNode);

						//CryLogAlways(	"Hit: %s(0x%p), ipart: %d, partid: %d, surafce_idx: %d, iNode: %d, \n",
						//pRenderNode->GetName(), pRenderNode, rayHit.ipart, rayHit.partid, rayHit.surface_idx, rayHit.iNode);
					}
				}
			}
		}
	}
#endif //_RELEASE
}

void C3DEngine::RenderWorld(const int nRenderFlags, const SRenderingPassInfo& passInfo, const char* szDebugName)
{
	CRY_PROFILE_REGION(PROFILE_3DENGINE, "3DEngine: RenderWorld");
	CRYPROFILE_SCOPE_PROFILE_MARKER("RenderWorld");

	passInfo.GetIRenderView()->SetShaderRenderingFlags(nRenderFlags);

#if defined(FEATURE_SVO_GI)
	if (passInfo.IsGeneralPass() && (nRenderFlags & SHDF_ALLOW_AO))
		CSvoManager::OnFrameStart(passInfo);
#endif

	if (passInfo.IsGeneralPass())
	{
		if (m_szLevelFolder[0] != 0)
		{
			m_nFramesSinceLevelStart++;
		}
	}

	assert(szDebugName);

	if (!GetCVars()->e_Render)
		return;

	IF (!m_bEditor && (m_bInShutDown || m_bInUnload) && !GetRenderer()->IsPost3DRendererEnabled(), 0)
	{
		// Do not render during shutdown/unloading (should never reach here, unless something wrong with game/editor code)
		return;
	}

#ifdef ENABLE_LW_PROFILERS
	int64 renderStart = CryGetTicks();
#endif

	if (passInfo.IsGeneralPass())
	{
		if (GetCVars()->e_ScreenShot)
		{
			ScreenshotDispatcher(nRenderFlags, passInfo);
			// screenshots can mess up the frame ids, be safe and recreate the rendering passinfo object after a screenshot
			const_cast<SRenderingPassInfo&>(passInfo) = SRenderingPassInfo::CreateGeneralPassRenderingInfo(passInfo.GetCamera());
		}

		if (GetCVars()->e_DefaultMaterial)
		{
			_smart_ptr<IMaterial> pMat = GetMaterialManager()->LoadMaterial("%ENGINE%/EngineAssets/Materials/material_default");
			_smart_ptr<IMaterial> pTerrainMat = GetMaterialManager()->LoadMaterial("%ENGINE%/EngineAssets/Materials/material_terrain_default");
			GetRenderer()->SetDefaultMaterials(pMat, pTerrainMat);
		}
		else
			GetRenderer()->SetDefaultMaterials(NULL, NULL);
	}

	// skip rendering if camera is invalid
	if (IsCameraAnd3DEngineInvalid(passInfo, szDebugName))
		return;

	// update camera 3d engine uses for rendering
	if (passInfo.IsGeneralPass())
	{
		// this will also set the camera in passInfo for the General Pass (done here to support e_camerafreeze)
		UpdateRenderingCamera(szDebugName, passInfo);
	}

	if (passInfo.IsGeneralPass())
	{
		// Update visible nodes, do it at the beginning of the rendering to avoid running in parallel with using visible nodes for rendering
		if (m_bResetRNTmpDataPool)
		{
			m_visibleNodesManager.InvalidateAll();
			m_bResetRNTmpDataPool = false;
		}
		m_visibleNodesManager.UpdateVisibleNodes(passInfo.GetFrameID());
	}

	RenderInternal(nRenderFlags, passInfo, szDebugName);

	IF (GetCVars()->e_DebugDraw, 0)
	{
		f32 fColor[4] = { 1, 1, 0, 1 };

		float fYLine = 8.0f, fYStep = 20.0f;

		IRenderAuxText::Draw2dLabel(8.0f, fYLine += fYStep, 2.0f, fColor, false, "e_DebugDraw = %d", GetCVars()->e_DebugDraw);

		char* szMode = "";

		switch (GetCVars()->e_DebugDraw)
		{
		case  -1:
			szMode = "Showing bounding boxes";
			break;
		case  1:
			szMode = "bounding boxes, name of the used cgf, polycount, used LOD";
			break;
		case  -2:
		case  2:
			szMode = "color coded polygon count(red,yellow,green,turqoise, blue)";
			break;
		case  -3:
			szMode = "show color coded LODs count, flashing color indicates LOD.";
			break;
		case  3:
			szMode = "show color coded LODs count, flashing color indicates LOD.\nFormat: (Current LOD [Min LOD; Max LOD] (LOD Ratio / Distance to camera)";
			break;
		case  -4:
		case  4:
			szMode = "object texture memory usage in KB";
			break;
		case  -5:
		case  5:
			szMode = "number of render materials (color coded)";
			break;
		case  6:
			szMode = "ambient color (R,G,B,A)";
			break;
		case  7:
			szMode = "triangle count, number of render materials, texture memory in KB";
			break;
		case  8:
			szMode = "Per Object MaxViewDist (color coded)";
			break;
		case  9:
			szMode = "Free slot";
			break;
		case 10:
			szMode = "Deprecated option, use \"r_showlines 2\" instead";
			break;
		case 11:
			szMode = "Free slot";
			break;
		case 12:
			szMode = "Free slot";
			break;
		case 13:
			szMode = "occlusion amount (used during AO computations)";
			break;
		//			case 14:	szMode="";break;
		case 15:
			szMode = "display helpers";
			break;
		case 16:
			szMode = "Debug Gun";
			break;
		case 17:
			szMode = "streaming: buffer sizes (black: geometry, blue: texture)";
			if (gEnv->pLocalMemoryUsage) gEnv->pLocalMemoryUsage->OnRender(GetRenderer(), &passInfo.GetCamera());
			break;
		case 18:
			szMode = "Free slot";
			break;
		case 19:
			szMode = "physics proxy triangle count";
			break;
		case 20:
			szMode = "Character attachments texture memory usage";
			break;
		case 21:
			szMode = "Display animated objects distance to camera";
			break;
		case -22:
		case 22:
			szMode = "object's current LOD vertex count";
			break;

		default:
			assert(0);
		}

		IRenderAuxText::Draw2dLabel(8.0f, fYLine += fYStep, 2.0f, fColor, false, "   %s", szMode);

		if (GetCVars()->e_DebugDraw == 17)
		{
			IRenderAuxText::Draw2dLabel(8.0f, fYLine += fYStep, 2.0f, fColor, false, "   StatObj geometry used: %.2fMb / %dMb", CObjManager::s_nLastStreamingMemoryUsage / (1024.f * 1024.f), GetCVars()->e_StreamCgfPoolSize);

			ICVar* cVar = GetConsole()->GetCVar("r_TexturesStreaming");
			if (!cVar || !cVar->GetIVal())
			{
				IRenderAuxText::Draw2dLabel(8.0f, fYLine += fYStep, 2.0f, fColor, false, "   You have to set r_TexturesStreaming = 1 to see texture information!");
			}
		}
	}

#if !defined(_RELEASE)
	PrintDebugInfo(passInfo);
#endif
}

void C3DEngine::PreWorldStreamUpdate(const CCamera& cam)
{
	const int nGlobalSystemState = gEnv->pSystem->GetSystemGlobalState();
	if (nGlobalSystemState == ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END)
	{
		ClearPrecacheInfo();
	}

	if (m_szLevelFolder[0] != 0 && (nGlobalSystemState > ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END))
	{
		m_nStreamingFramesSinceLevelStart++;
	}

	// force preload terrain data if camera was teleported more than 32 meters
	if (GetRenderer() && (!IsAreaActivationInUse() || m_bLayersActivated))
	{
		float fDistance = m_vPrevMainFrameCamPos.GetDistance(cam.GetPosition());

		if (m_vPrevMainFrameCamPos != Vec3(-1000000.f, -1000000.f, -1000000.f))
		{
			m_vAverageCameraMoveDir = m_vAverageCameraMoveDir * .75f + (cam.GetPosition() - m_vPrevMainFrameCamPos) / max(0.01f, GetTimer()->GetFrameTime()) * .25f;
			if (m_vAverageCameraMoveDir.GetLength() > 10.f)
				m_vAverageCameraMoveDir.SetLength(10.f);

			float fNewSpeed = fDistance / max(0.001f, gEnv->pTimer->GetFrameTime());
			if (fNewSpeed > m_fAverageCameraSpeed)
				m_fAverageCameraSpeed = fNewSpeed * .20f + m_fAverageCameraSpeed * .80f;
			else
				m_fAverageCameraSpeed = fNewSpeed * .02f + m_fAverageCameraSpeed * .98f;
			m_fAverageCameraSpeed = CLAMP(m_fAverageCameraSpeed, 0, 10.f);
		}

		// Adjust streaming mip bias based on camera speed and depending on installed on HDD or not
		bool bStreamingFromHDD = gEnv->pSystem->GetStreamEngine()->IsStreamDataOnHDD();
		if (GetCVars()->e_StreamAutoMipFactorSpeedThreshold)
		{
			if (m_fAverageCameraSpeed > GetCVars()->e_StreamAutoMipFactorSpeedThreshold)
				GetRenderer()->SetTexturesStreamingGlobalMipFactor(bStreamingFromHDD ? GetCVars()->e_StreamAutoMipFactorMax * .5f : GetCVars()->e_StreamAutoMipFactorMax);
			else
				GetRenderer()->SetTexturesStreamingGlobalMipFactor(bStreamingFromHDD ? GetCVars()->e_StreamAutoMipFactorMin * .5f : GetCVars()->e_StreamAutoMipFactorMin);
		}
		else
		{
			if (bStreamingFromHDD)
				GetRenderer()->SetTexturesStreamingGlobalMipFactor(0);
			else
				GetRenderer()->SetTexturesStreamingGlobalMipFactor(GetCVars()->e_StreamAutoMipFactorMaxDVD);
		}

		if (GetCVars()->e_AutoPrecacheCameraJumpDist && fDistance > GetCVars()->e_AutoPrecacheCameraJumpDist)
		{
			m_bContentPrecacheRequested = true;
			m_bResetRNTmpDataPool = true;

			// Invalidate existing precache info
			m_pObjManager->m_nUpdateStreamingPrioriryRoundIdFast += 8;
			m_pObjManager->m_nUpdateStreamingPrioriryRoundId += 8;
		}

		m_vPrevMainFrameCamPos = cam.GetPosition();
	}
}

void C3DEngine::WorldStreamUpdate()
{
#if defined(STREAMENGINE_ENABLE_STATS)
	static uint32 nCurrentRequestCount = 0;
	static uint64 nCurrentBytesRead = 0;
	if (m_nStreamingFramesSinceLevelStart == 1)
	{
		// store current streaming stats
		SStreamEngineStatistics& fullStats = gEnv->pSystem->GetStreamEngine()->GetStreamingStatistics();
		nCurrentBytesRead = fullStats.nTotalBytesRead;
		nCurrentRequestCount = fullStats.nTotalRequestCount;
	}
#endif

	static float fTestStartTime = 0;
	if (m_nStreamingFramesSinceLevelStart == 1)
	{
		fTestStartTime = GetCurAsyncTimeSec();
		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_PRECACHE_FIRST_FRAME, 0, 0);
	}

	// Simple streaming performance test: Wait until all startup texture streaming jobs finish and print a message
	if (!m_bEditor)
	{
		if (!m_bPreCacheEndEventSent)
		{
			IStreamEngine* pSE = gEnv->pSystem->GetStreamEngine();
			SStreamEngineOpenStats openStats;
			pSE->GetStreamingOpenStatistics(openStats);
			bool bStarted =
			  (openStats.nOpenRequestCountByType[eStreamTaskTypeTexture] > 0) ||
			  (openStats.nOpenRequestCountByType[eStreamTaskTypeGeometry] > 0);

			float fTime = GetCurAsyncTimeSec() - fTestStartTime;

			switch (m_nStreamingFramesSinceLevelStart)
			{
			case 1:
				pSE->PauseStreaming(true, (1 << eStreamTaskTypeTexture) | (1 << eStreamTaskTypeGeometry));
				break;
			case 4:
				pSE->PauseStreaming(false, (1 << eStreamTaskTypeGeometry));
				break;
			case 8:
				pSE->PauseStreaming(false, (1 << eStreamTaskTypeTexture));
				break;
			}

			const int nGlobalSystemState = gEnv->pSystem->GetSystemGlobalState();

			if (nGlobalSystemState == ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END)
			{
				gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_PRECACHE_START, 0, 0);

				m_pSystem->SetThreadState(ESubsys_Physics, true);

				gEnv->pSystem->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_ENDING);
			}

			if (nGlobalSystemState == ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_ENDING && (!bStarted || fTime >= 10.0f) && m_nStreamingFramesSinceLevelStart > 16)
			{
				gEnv->pSystem->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_COMPLETE);

				if (!bStarted)
					PrintMessage("Textures startup streaming finished in %.1f sec", fTime);
				else
					PrintMessage("Textures startup streaming timed out after %.1f sec", fTime);

				m_fTimeStateStarted = fTime;
			}

			if (nGlobalSystemState == ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_COMPLETE && (fTime - m_fTimeStateStarted) > 0.4f)
			{
				pSE->PauseStreaming(false, (1 << eStreamTaskTypeTexture) | (1 << eStreamTaskTypeGeometry));

				m_bPreCacheEndEventSent = true;
				gEnv->pSystem->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_RUNNING);
				gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_PRECACHE_END, 0, 0);

				fTestStartTime = 0.f;

#if defined(STREAMENGINE_ENABLE_STATS)
				SStreamEngineStatistics& fullStats = pSE->GetStreamingStatistics();
				uint64 nBytesRead = fullStats.nTotalBytesRead - nCurrentBytesRead;
				uint32 nRequestCount = fullStats.nTotalRequestCount - nCurrentRequestCount;

				uint32 nOverallFileReadKB = (uint32)(nBytesRead / 1024);
				uint32 nOverallFileReadNum = nRequestCount;
				uint32 nBlockSize = (uint32)(nBytesRead / max((uint32)1, nRequestCount));
				float fReadBandwidthMB = (float)fullStats.nTotalSessionReadBandwidth / (1024 * 1024);

				PrintMessage("Average block size: %d KB, Average throughput: %.1f MB/sec, Jobs processed: %d (%.1f MB), File IO Bandwidth: %.2fMB/s",
				             (nBlockSize) / 1024, (float)(nOverallFileReadKB / max(fTime, 1.f)) / 1024.f,
				             nOverallFileReadNum, (float)nOverallFileReadKB / 1024.f,
				             fReadBandwidthMB);

				if (GetCVars()->e_StreamSaveStartupResultsIntoXML)
				{
					const char* szTestResults = "%USER%/TestResults";
					char path[ICryPak::g_nMaxPath] = "";
					gEnv->pCryPak->AdjustFileName(string(string(szTestResults) + "\\" + "Streaming_Level_Start_Throughput.xml").c_str(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
					gEnv->pCryPak->MakeDir(szTestResults);

					if (FILE* f = ::fopen(path, "wb"))
					{
						fprintf(f,
						        "<phase name=\"Streaming_Level_Start_Throughput\">\n"
						        "<metrics name=\"Streaming\">\n"
						        "<metric name=\"Duration_Sec\"	value=\"%.1f\"/>\n"
						        "<metric name=\"BlockSize_KB\" value=\"%u\"/>\n"
						        "<metric name=\"Throughput_MB_Sec\" value=\"%.1f\"/>\n"
						        "<metric name=\"Jobs_Num\"	value=\"%u\"/>\n"
						        "<metric name=\"Read_MB\" value=\"%.1f\"/>\n"
						        "</metrics>\n"
						        "</phase>\n",
						        fTime,
						        (nOverallFileReadKB / nOverallFileReadNum),
						        (float)nOverallFileReadKB / max(fTime, 1.f) / 1024.f,
						        nOverallFileReadNum,
						        (float)nOverallFileReadKB / 1024.f);

						::fclose(f);
					}
				}
#endif
				// gEnv->pCryPak->GetFileReadSequencer()->EndSection(); // STREAMING
			}
		}
	}
	else
	{
		if (!m_bPreCacheEndEventSent && m_nStreamingFramesSinceLevelStart == 4)
		{
			m_bPreCacheEndEventSent = true;
			gEnv->pSystem->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_RUNNING);
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_PRECACHE_END, 0, 0);
		}
	}
}

void C3DEngine::PrintDebugInfo(const SRenderingPassInfo& passInfo)
{
	float fTextPosX = 10, fTextPosY = 10, fTextStepY = 12;

	// print list of streamed meshes
	if (m_pObjManager && GetCVars()->e_StreamCgf && GetCVars()->e_StreamCgfDebug >= 3)
	{
		// overall status
		{
			static char szCGFStreaming[256];
			static SObjectsStreamingStatus objectsStreamingStatus = { 0 };

			{
				m_pObjManager->GetObjectsStreamingStatus(objectsStreamingStatus);
				cry_sprintf(szCGFStreaming, "CgfStrm: Loaded:%d InProg:%d All:%d Act:%d MemUsed:%2.2f MemReq:%2.2f Pool:%d",
				            objectsStreamingStatus.nReady, objectsStreamingStatus.nInProgress, objectsStreamingStatus.nTotal, objectsStreamingStatus.nActive, float(objectsStreamingStatus.nAllocatedBytes) / 1024 / 1024, float(objectsStreamingStatus.nMemRequired) / 1024 / 1024, GetCVars()->e_StreamCgfPoolSize);
			}

			bool bOutOfMem((float(objectsStreamingStatus.nMemRequired) / 1024 / 1024) > GetCVars()->e_StreamCgfPoolSize);
			bool bCloseToOutOfMem((float(objectsStreamingStatus.nMemRequired) / 1024 / 1024) > GetCVars()->e_StreamCgfPoolSize * 90 / 100);

			ColorF color = Col_White;
			if (bOutOfMem)
				color = Col_Red;
			else if (bCloseToOutOfMem)
				color = Col_Orange;
			//		if(bTooManyRequests)
			//		color = Col_Magenta;

			DrawTextLeftAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, color, "%s", szCGFStreaming);
			fTextPosY += fTextStepY;
		}

		DrawTextLeftAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_White, "------------------- List of meshes bigger than %d KB -------------------", GetCVars()->e_StreamCgfDebugMinObjSize);

		for (int nObjId = 0; nObjId < m_pObjManager->m_arrStreamableObjects.Count(); nObjId++)
		{
			CStatObj* pStatObj = (CStatObj*)m_pObjManager->m_arrStreamableObjects[nObjId].GetStreamAbleObject();

			int nKB = pStatObj->GetStreamableContentMemoryUsage() >> 10;
			int nSel = (pStatObj->m_nSelectedFrameId >= passInfo.GetMainFrameID() - 2);

			string sName;
			pStatObj->GetStreamableName(sName);

			if ((nKB >= GetCVars()->e_StreamCgfDebugMinObjSize && strstr(sName.c_str(), GetCVars()->e_StreamCgfDebugFilter->GetString())) || nSel)
			{
				char* pComment = 0;

				if (!pStatObj->m_bCanUnload)
					pComment = "NO_STRM";
				else if (pStatObj->m_pLod0)
					pComment = "  LOD_X";
				else if (!pStatObj->m_bLodsAreLoadedFromSeparateFile && pStatObj->m_nLoadedLodsNum > 1)
					pComment = " SINGLE";
				else if (pStatObj->m_nLoadedLodsNum > 1)
					pComment = "  LOD_0";
				else
					pComment = "NO_LODS";

				int nDiff = SATURATEB(int(float(nKB - GetCVars()->e_StreamCgfDebugMinObjSize) / max(1, (int)GetCVars()->e_StreamCgfDebugMinObjSize) * 255));
				ColorB col(nDiff, 255 - nDiff, 0, 255);
				if (nSel && (1 & (int)(GetCurTimeSec() * 5.f)))
					col = Col_Yellow;
				ColorF fColor(col[0] / 255.f, col[1] / 255.f, col[2] / 255.f, col[3] / 255.f);

				char* pStatusText = "Unload";
				if (pStatObj->m_eStreamingStatus == ecss_Ready)
					pStatusText = "Ready ";
				else if (pStatObj->m_eStreamingStatus == ecss_InProgress)
					pStatusText = "InProg";

				DrawTextLeftAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, fColor, "%1.2f mb, %s, %s, %s",
				                    1.f / 1024.f * nKB, pComment, pStatusText, sName.c_str());

				if (fTextPosY > (float)gEnv->pRenderer->GetOverlayHeight())
					break;
			}
		}
	}

	if (m_arrProcessStreamingLatencyTestResults.Count())
	{
		float fAverTime = 0;
		for (int i = 0; i < m_arrProcessStreamingLatencyTestResults.Count(); i++)
			fAverTime += m_arrProcessStreamingLatencyTestResults[i];
		fAverTime /= m_arrProcessStreamingLatencyTestResults.Count();

		int nAverTexNum = 0;
		for (int i = 0; i < m_arrProcessStreamingLatencyTexNum.Count(); i++)
			nAverTexNum += m_arrProcessStreamingLatencyTexNum[i];
		nAverTexNum /= m_arrProcessStreamingLatencyTexNum.Count();

		DrawTextLeftAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_Yellow, "------ SQT Average Time = %.1f, TexNum = %d ------", fAverTime, nAverTexNum);

		for (int i = 0; i < m_arrProcessStreamingLatencyTestResults.Count(); i++)
			DrawTextLeftAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_Yellow, "Run %d: Time = %.1f, TexNum = %d",
			                    i, m_arrProcessStreamingLatencyTestResults[i], m_arrProcessStreamingLatencyTexNum[i]);
	}

#if defined(USE_GEOM_CACHES)
	#ifndef _RELEASE
	if (GetCVars()->e_GeomCacheDebug)
	{
		m_pGeomCacheManager->DrawDebugInfo();
	}
	else
	{
		m_pGeomCacheManager->ResetDebugInfo();
	}
	#endif
#endif
}

void C3DEngine::DebugDrawStreaming(const SRenderingPassInfo& passInfo)
{
#ifndef CONSOLE_CONST_CVAR_MODE
	static Array2d<int> memUsage;
	int nArrayDim = 256;

	if (GetCVars()->e_StreamCgfDebugHeatMap == 1)
	{
		memUsage.Allocate(nArrayDim);
		CCamera camOld = passInfo.GetCamera();

		int nStep = Get3DEngine()->GetTerrainSize() / nArrayDim;

		PrintMessage("Computing mesh streaming heat map");

		for (int x = 0; x < Get3DEngine()->GetTerrainSize(); x += nStep)
		{
			for (int y = 0; y < Get3DEngine()->GetTerrainSize(); y += nStep)
			{
				CCamera camTmp = camOld;
				camTmp.SetPosition(Vec3((float)x + (float)nStep / 2.f, (float)y + (float)nStep / 2.f, Get3DEngine()->GetTerrainElevation((float)x, (float)y)));
				//SetCamera(camTmp);
				m_pObjManager->ProcessObjectsStreaming(passInfo);

				SObjectsStreamingStatus objectsStreamingStatus;
				m_pObjManager->GetObjectsStreamingStatus(objectsStreamingStatus);

				memUsage[x / nStep][y / nStep] = objectsStreamingStatus.nMemRequired;
			}

			if (!((x / nStep) & 31))
				PrintMessage(" working ...");
		}

		PrintMessage(" done");

		GetCVars()->e_StreamCgfDebugHeatMap = 2;
		//SetCamera(camOld);
	}
	else if (GetCVars()->e_StreamCgfDebugHeatMap == 2)
	{
		float fStep = (float)Get3DEngine()->GetTerrainSize() / (float)nArrayDim;

		for (int x = 0; x < memUsage.GetSize(); x++)
		{
			for (int y = 0; y < memUsage.GetSize(); y++)
			{
				Vec3 v0((float)x * fStep, (float)y * fStep, Get3DEngine()->GetTerrainElevation((float)x * fStep, (float)y * fStep));
				Vec3 v1((float)x * fStep + fStep, (float)y * fStep + fStep, v0.z + fStep);
				v0 += Vec3(.25f, .25f, .25f);
				v1 -= Vec3(.25f, .25f, .25f);
				AABB box(v0, v1);
				if (!passInfo.GetCamera().IsAABBVisible_F(box))
					continue;

				int nMemUsageMB = memUsage[(int)(x)][(int)(y)] / 1024 / 1024;

				int nOverLoad = nMemUsageMB - GetCVars()->e_StreamCgfPoolSize;

				ColorB col = Col_Red;

				if (nOverLoad < GetCVars()->e_StreamCgfPoolSize / 2)
					col = Col_Yellow;

				if (nOverLoad < 0)
					col = Col_Green;

				DrawBBox(box, col);
			}
		}
	}
#endif //CONSOLE_CONST_CVAR_MODE
}

void C3DEngine::RenderInternal(const int nRenderFlags, const SRenderingPassInfo& passInfo, const char* szDebugName)
{
	m_bProfilerEnabled = gEnv->pFrameProfileSystem->IsProfiling();

	//Cache for later use
	if (m_pObjManager)
		m_pObjManager->SetCurrentTime(gEnv->pTimer->GetCurrTime());

	// test if recursion causes problems
	if (passInfo.IsRecursivePass() && !GetCVars()->e_Recursion)
		return;

	if (passInfo.IsGeneralPass())
	{
		m_fGsmRange = GetCVars()->e_GsmRange;
		m_fGsmRangeStep = GetCVars()->e_GsmRangeStep;

		//!!!also formulas for computing biases per gsm needs to be changed
		m_fShadowsConstBias = GetCVars()->e_ShadowsConstBias;
		m_fShadowsSlopeBias = GetCVars()->e_ShadowsSlopeBias;

		if (m_eShadowMode == ESM_HIGHQUALITY)
		{
			m_fGsmRange = min(0.15f, GetCVars()->e_GsmRange);
			m_fGsmRangeStep = min(2.8f, GetCVars()->e_GsmRangeStep);

			m_fShadowsConstBias = GetCVars()->e_ShadowsConstBias;
			m_fShadowsSlopeBias = GetCVars()->e_ShadowsSlopeBias;
		}
	}

	// Update particle system as late as possible, only renderer is dependent on it.
	m_pPartManager->GetLightProfileCounts().ResetFrameTicks();
	if (passInfo.IsGeneralPass() && m_pPartManager)
		m_pPartManager->Update();

	if (passInfo.IsGeneralPass() && passInfo.RenderClouds())
	{
		// move procedural volumetric clouds with global wind.
		{
			Vec3 cloudParams(0, 0, 0);
			GetGlobalParameter(E3DPARAM_VOLCLOUD_WIND_ATMOSPHERIC, cloudParams);
			const float windInfluence = cloudParams.x;
			Vec3 wind = GetGlobalWind(false);
			const float elapsedTime = windInfluence * gEnv->pTimer->GetFrameTime();

			// move E3DPARAM_VOLCLOUD_TILING_OFFSET with global wind.
			m_vVolCloudTilingOffset.x -= wind.x * elapsedTime;
			m_vVolCloudTilingOffset.y -= wind.y * elapsedTime;
		}
	}

	if (m_pObjManager)
	{
		m_pObjManager->m_fMaxViewDistanceScale = 1.f;

		const int nCascadeCount = Get3DEngine()->GetShadowsCascadeCount(NULL);
		m_pObjManager->m_fGSMMaxDistance = Get3DEngine()->m_fGsmRange * powf(Get3DEngine()->m_fGsmRangeStep, (float)nCascadeCount);
	}

	if (passInfo.GetRecursiveLevel() >= MAX_RECURSION_LEVELS)
	{
		assert(!"Recursion deeper than MAX_RECURSION_LEVELS is not supported");
		return;
	}

	if (passInfo.IsGeneralPass())
		UpdateScene(passInfo);

	CheckPhysicalized(passInfo.GetCamera().GetPosition() - Vec3(2, 2, 2), passInfo.GetCamera().GetPosition() + Vec3(2, 2, 2));

	RenderScene(nRenderFlags, passInfo);

#ifndef _RELEASE
	IF (GetCVars()->e_LightVolumesDebug, 0)
		m_LightVolumesMgr.DrawDebug(passInfo);
#endif

	if (m_pObjManager && passInfo.IsGeneralPass())
	{
		m_pObjManager->CheckTextureReadyFlag();
		if (GetCVars()->e_StreamCgf)
		{
			DebugDrawStreaming(passInfo);

			m_pObjManager->ProcessObjectsStreaming(passInfo);
		}
		else
		{
			m_pObjManager->m_vStreamPreCacheCameras[0].vPosition = passInfo.GetCamera().GetPosition();
			if (Distance::Point_AABBSq(m_pObjManager->m_vStreamPreCacheCameras[0].vPosition, m_pObjManager->m_vStreamPreCacheCameras[0].bbox) > 0.0f)
				m_pObjManager->m_vStreamPreCacheCameras[0].bbox = AABB(m_pObjManager->m_vStreamPreCacheCameras[0].vPosition, GetCVars()->e_StreamPredictionBoxRadius);
			m_pObjManager->UpdateObjectsStreamingPriority(false, passInfo);
		}

		// Unload old object instances
		COctreeNode::StreamingCheckUnload(passInfo, GetObjManager()->m_vStreamPreCacheCameras);
	}

	if (passInfo.IsGeneralPass())
		m_bContentPrecacheRequested = false;

}

int __cdecl C3DEngine__Cmp_SRNInfo(const void* v1, const void* v2)
{
	SRNInfo* p1 = (SRNInfo*)v1;
	SRNInfo* p2 = (SRNInfo*)v2;

	float fViewDist1 = p1->fMaxViewDist - p1->objSphere.radius;
	float fViewDist2 = p2->fMaxViewDist - p2->objSphere.radius;

	// if same - give closest sectors higher priority
	if (fViewDist1 > fViewDist2)
		return 1;
	else if (fViewDist1 < fViewDist2)
		return -1;

	return 0;
}

IMaterial* C3DEngine::GetSkyMaterial()
{
	IMaterial* pRes(0);
	if (GetCVars()->e_SkyType == 0)
	{
		if (!m_pSkyLowSpecMat)
		{
			m_pSkyLowSpecMat = m_skyLowSpecMatName.empty() ? 0 : m_pMatMan->LoadMaterial(m_skyLowSpecMatName.c_str(), false);
		}
		pRes = m_pSkyLowSpecMat;
	}
	else
	{
		if (!m_pSkyMat)
		{
			m_pSkyMat = m_skyMatName.empty() ? 0 : GetMatMan()->LoadMaterial(m_skyMatName.c_str(), false);
		}
		pRes = m_pSkyMat;
	}
	return pRes;
}

void C3DEngine::SetSkyMaterial(IMaterial* pSkyMat)
{
	m_pSkyMat = pSkyMat;
}

bool C3DEngine::IsHDRSkyMaterial(IMaterial* pMat) const
{
	return pMat && pMat->GetShaderItem().m_pShader && !stricmp(pMat->GetShaderItem().m_pShader->GetName(), "SkyHDR");
}

void C3DEngine::RenderScene(const int nRenderFlags, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;
	assert(passInfo.GetRecursiveLevel() < MAX_RECURSION_LEVELS);

	////////////////////////////////////////////////////////////////////////////////////////
	// Begin scene drawing
	////////////////////////////////////////////////////////////////////////////////////////
	if (passInfo.IsGeneralPass())
		GetObjManager()->m_CullThread.SetActive(true);

	////////////////////////////////////////////////////////////////////////////////////////
	// Sync asynchronous cull queue processing if enabled
	////////////////////////////////////////////////////////////////////////////////////////
#ifdef USE_CULL_QUEUE
	if (GetCVars()->e_CoverageBuffer)
		GetObjManager()->CullQueue().Wait();
#endif

	////////////////////////////////////////////////////////////////////////////////////////
	// Draw potential occluders into z-buffer
	////////////////////////////////////////////////////////////////////////////////////////

	if (GetCVars()->e_Sleep > 0)
	{
		CrySleep(GetCVars()->e_Sleep);
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Output debug data from last frame
	////////////////////////////////////////////////////////////////////////////////////////
	if (passInfo.IsGeneralPass())
		m_pObjManager->RenderAllObjectDebugInfo();

	////////////////////////////////////////////////////////////////////////////////////////
	// From here we add render elements of main scene
	////////////////////////////////////////////////////////////////////////////////////////

	GetRenderer()->EF_StartEf(passInfo);

	////////////////////////////////////////////////////////////////////////////////////////
	// Define indoor visibility
	////////////////////////////////////////////////////////////////////////////////////////

	if (m_pVisAreaManager)
	{
		m_pVisAreaManager->DrawOcclusionAreasIntoCBuffer(passInfo);
		m_pVisAreaManager->CheckVis(passInfo);
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Add clip volumes to renderer
	////////////////////////////////////////////////////////////////////////////////////////
	if (m_pClipVolumeManager && GetCVars()->e_ClipVolumes)
		m_pClipVolumeManager->PrepareVolumesForRendering(passInfo);

	if (m_pPartManager)
		m_pPartManager->PrepareForRender(passInfo);

	////////////////////////////////////////////////////////////////////////////////////////
	// Make sure all async octree updates are done
	////////////////////////////////////////////////////////////////////////////////////////
	if (passInfo.IsGeneralPass())
		m_bIsInRenderScene = true;

	if (passInfo.IsGeneralPass())
		COctreeNode::ReleaseEmptyNodes();

	////////////////////////////////////////////////////////////////////////////////////////
	// Add lsources to the renderer and register into sectors
	////////////////////////////////////////////////////////////////////////////////////////
	UpdateLightSources(passInfo);
	PrepareLightSourcesForRendering_0(passInfo);
	PrepareLightSourcesForRendering_1(passInfo);

	////////////////////////////////////////////////////////////////////////////////////////
	// Add render elements for indoor
	////////////////////////////////////////////////////////////////////////////////////////
	if (passInfo.IsGeneralPass() && IsStatObjBufferRenderTasksAllowed() && JobManager::InvokeAsJob("CheckOcclusion"))
		m_pObjManager->BeginOcclusionCulling(passInfo);

	////////////////////////////////////////////////////////////////////////////////////////
	// Collect shadow passes
	////////////////////////////////////////////////////////////////////////////////////////
	std::vector<SRenderingPassInfo> shadowPassInfo;                                 // Shadow passes used in scene
	std::vector<std::pair<ShadowMapFrustum*, const CLightEntity*>> shadowFrustums;  // Shadow frustums and lights used in scene
	uint32 passCullMask = kPassCullMainMask;                                        // initialize main view bit as visible

	if (passInfo.RenderShadows() && !passInfo.IsRecursivePass())
	{
		// Collect shadow passes used in scene and allocate render view for each of them
		uint32 nTimeSlicedShadowsUpdatedThisFrame = 0;
		PrepareShadowPasses(passInfo, nTimeSlicedShadowsUpdatedThisFrame, shadowFrustums, shadowPassInfo);

		// initialize shadow view bits
		for (uint32 p = 0; p < shadowPassInfo.size(); p++)
			passCullMask |= BIT(p + 1);

		// store ptr to collected shadow passes into main view pass
		const_cast<SRenderingPassInfo&>(passInfo).SetShadowPasses(&shadowPassInfo);
	}
	else
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "Traverse Outdoor Lights");

		// render point lights
		CLightEntity::SetShadowFrustumsCollector(nullptr);
		m_pObjectsTree->Render_LightSources(false, passInfo);
	}

	// draw objects inside visible vis areas
	if (m_pVisAreaManager)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "Traverse Indoor Octrees");
		m_pVisAreaManager->DrawVisibleSectors(passInfo, passCullMask);
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Clear current sprites list
	////////////////////////////////////////////////////////////////////////////////////////
	for (int t = 0; t < nThreadsNum; t++)
	{
		CThreadSafeRendererContainer<SVegetationSpriteInfo>& rList = m_pObjManager->m_arrVegetationSprites[passInfo.GetRecursiveLevel()][t];
		rList.resize(0);
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Add render elements for outdoor
	////////////////////////////////////////////////////////////////////////////////////////

	// Reset ocean volume
	if (passInfo.IsGeneralPass())
		m_nOceanRenderFlags &= ~OCR_OCEANVOLUME_VISIBLE;

	if (m_pTerrain)
		m_pTerrain->ClearVisSectors();

	if (passCullMask || GetRenderer()->IsPost3DRendererEnabled())
	{
		if (m_pVisAreaManager && m_pVisAreaManager->m_lstOutdoorPortalCameras.Count() &&
		    (m_pVisAreaManager->m_pCurArea || m_pVisAreaManager->m_pCurPortal))
		{
			// enable multi-camera culling
			//const_cast<CCamera&>(passInfo.GetCamera()).m_pMultiCamera = &m_pVisAreaManager->m_lstOutdoorPortalCameras;
		}

		if (IsOutdoorVisible())
		{
			RenderSkyBox(GetSkyMaterial(), passInfo);
		}

		// start processing terrain
		if (IsOutdoorVisible() && m_pTerrain && passInfo.RenderTerrain() && Get3DEngine()->m_bShowTerrainSurface && !gEnv->IsDedicated())
			m_pTerrain->CheckVis(passInfo, passCullMask);

		// process streaming and procedural vegetation distribution
		if (passInfo.IsGeneralPass() && m_pTerrain)
			m_pTerrain->UpdateNodesIncrementaly(passInfo);

		passInfo.GetRendItemSorter().IncreaseOctreeCounter();
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "Traverse Outdoor Octree");
			m_pObjectsTree->Render_Object_Nodes(false, OCTREENODE_RENDER_FLAG_OBJECTS, GetSkyColor(), passCullMask, passInfo);
		}

		if (GetCVars()->e_ObjectsTreeBBoxes >= 3)
			m_pObjectsTree->RenderDebug();

		passInfo.GetRendItemSorter().IncreaseGroupCounter();
	}

	if (GetCVars()->e_ObjectLayersActivation == 4)
	{
		for (int l = 0; l < m_arrObjectLayersActivity.Count(); l++)
			if (m_arrObjectLayersActivity[l].objectsBox.GetVolume())
				DrawBBoxLabeled(m_arrObjectLayersActivity[l].objectsBox, Matrix34::CreateIdentity(), Col_White, "id=%d, %s", l, m_arrObjectLayersActivity[l].bActive ? "ON" : "OFF");
	}

	// render outdoor entities very near of camera - fix for 1p vehicle entering into indoor
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "COctreeNode::Render_Object_Nodes_NEAR");
		passInfo.GetRendItemSorter().IncreaseOctreeCounter();
		if (GetCVars()->e_PortalsBigEntitiesFix)
		{
			if (!IsOutdoorVisible() && GetVisAreaManager() && GetVisAreaManager()->GetCurVisArea() && GetVisAreaManager()->GetCurVisArea()->IsConnectedToOutdoor())
			{
				CCamera cam = passInfo.GetCamera();
				cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), cam.GetFov(), min(cam.GetNearPlane(), 1.f), 2.f, cam.GetPixelAspectRatio());
				m_pObjectsTree->Render_Object_Nodes(false, OCTREENODE_RENDER_FLAG_OBJECTS | OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES, GetSkyColor(), passCullMask & kPassCullMainMask, SRenderingPassInfo::CreateTempRenderingInfo(cam, passInfo));
			}
		}
	}
	passInfo.GetRendItemSorter().IncreaseGroupCounter();

	// render special objects like laser beams intersecting entire level
	for (int i = 0; i < m_lstAlwaysVisible.Count(); i++)
	{
		IRenderNode* pObj = m_lstAlwaysVisible[i];
		const AABB& objBox = pObj->GetBBox();
		// don't frustum cull the HUD. When e.g. zooming the FOV for this camera is very different to the
		// fixed HUD FOV, and this can cull incorrectly.
		auto dwRndFlags = pObj->GetRndFlags();
		if (dwRndFlags & ERF_HUD || passInfo.GetCamera().IsAABBVisible_E(objBox))
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "C3DEngine::RenderScene_DrawAlwaysVisible");

			Vec3 vCamPos = passInfo.GetCamera().GetPosition();
			float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, objBox)) * passInfo.GetZoomFactor();
			assert(fEntDistance >= 0 && _finite(fEntDistance));
			if (fEntDistance < pObj->m_fWSMaxViewDist)
			{
				if (pObj->GetRenderNodeType() == eERType_Brush || pObj->GetRenderNodeType() == eERType_MovableBrush)
				{
					GetObjManager()->RenderBrush((CBrush*)pObj, NULL, NULL, objBox, fEntDistance, false, passInfo, passCullMask & kPassCullMainMask);
				}
				else
				{
					GetObjManager()->RenderObject(pObj, NULL, GetSkyColor(), objBox, fEntDistance, pObj->GetRenderNodeType(), passInfo, passCullMask & kPassCullMainMask);
				}
			}
		}
	}
	passInfo.GetRendItemSorter().IncreaseGroupCounter();

	ProcessOcean(passInfo);

	if (m_pWaterRippleManager)
	{
		m_pWaterRippleManager->Render(passInfo);
	}

	if (m_pDecalManager && passInfo.RenderDecals())
	{
		m_pDecalManager->Render(passInfo);
	}

	// tell the occlusion culler that no new work will be submitted
	if (IsStatObjBufferRenderTasksAllowed() && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
	{
		GetObjManager()->PushIntoCullQueue(SCheckOcclusionJobData::CreateQuitJobData());
	}

	if (passInfo.IsGeneralPass())
	{
		gEnv->pSystem->DoWorkDuringOcclusionChecks();
	}

#if defined(FEATURE_SVO_GI)
	if (passInfo.IsGeneralPass() && (nRenderFlags & SHDF_ALLOW_AO))
	{
		CSvoManager::Render((nRenderFlags & SHDF_CUBEMAPGEN) != 0);
	}
#endif

	// submit non-job render nodes into render views
	// we wait for all render jobs to finish here
	if (passInfo.IsGeneralPass() && IsStatObjBufferRenderTasksAllowed() && JobManager::InvokeAsJob("CheckOcclusion"))
	{
		m_pObjManager->RenderNonJobObjects(passInfo);
	}

	// render terrain ground in case of non job mode
	if (m_pTerrain)
		m_pTerrain->DrawVisibleSectors(passInfo);

	// Call postrender on the meshes that require it.
	if (passInfo.IsGeneralPass())
	{
		m_pMergedMeshesManager->SortActiveInstances(passInfo);
		m_pMergedMeshesManager->PostRenderMeshes(passInfo);
	}

	// all shadow casters are submitted, switch render views into eUsageModeWritingDone mode
	for (const auto& pair : shadowFrustums)
	{
		auto& shadowFrustum = pair.first;
		CRY_ASSERT(shadowFrustum->pOnePassShadowView);
		shadowFrustum->pOnePassShadowView->SwitchUsageMode(IRenderView::eUsageModeWritingDone);
	}

	// start render jobs for shadow map
	if (!passInfo.IsShadowPass() && passInfo.RenderShadows() && !passInfo.IsRecursivePass())
	{
		GetRenderer()->EF_InvokeShadowMapRenderJobs(passInfo, 0);
	}

	// add sprites render item
	if (passInfo.RenderFarSprites())
		m_pObjManager->RenderFarObjects(passInfo);

	if (m_pPartManager)
		m_pPartManager->FinishParticleRenderTasks(passInfo);

	if (passInfo.IsGeneralPass())
		m_LightVolumesMgr.Update(passInfo);

	if (auto* pGame = gEnv->pGameFramework->GetIGame())
		pGame->OnRenderScene(passInfo);

	////////////////////////////////////////////////////////////////////////////////////////
	// Start asynchronous cull queue processing if enabled
	////////////////////////////////////////////////////////////////////////////////////////

	//Tell the c-buffer that the item queue is ready. The render thread supplies the depth buffer to test against and this is prepared asynchronously
	GetObjManager()->CullQueue().FinishedFillingTestItemQueue();

	////////////////////////////////////////////////////////////////////////////////////////
	// Finalize frame
	////////////////////////////////////////////////////////////////////////////////////////

	{
		SRenderGlobalFogDescription fog;
		fog.bEnable = GetCVars()->e_Fog > 0;
		fog.color = ColorF(m_vFogColor.x, m_vFogColor.y, m_vFogColor.z, 1.0f);
		passInfo.GetIRenderView()->SetGlobalFog(fog);
	}

	if (passInfo.IsGeneralPass())
		SetupClearColor(passInfo);

	// Update the sector meshes
	if (m_pTerrain)
		m_pTerrain->UpdateSectorMeshes(passInfo);

	ICharacterManager* pCharManager = gEnv->pCharacterManager;
	if (pCharManager)
	{
		pCharManager->UpdateStreaming(GetObjManager()->m_nUpdateStreamingPrioriryRoundId, GetObjManager()->m_nUpdateStreamingPrioriryRoundIdFast);
	}

	FinishWindGridJob();

	{
		CRY_PROFILE_REGION(PROFILE_RENDERER, "Renderer::EF_EndEf3D");
		GetRenderer()->EF_EndEf3D(IsShadersSyncLoad() ? (nRenderFlags | SHDF_NOASYNC) : nRenderFlags, GetObjManager()->m_nUpdateStreamingPrioriryRoundId, GetObjManager()->m_nUpdateStreamingPrioriryRoundIdFast, passInfo);
	}

	if (passInfo.IsGeneralPass())
	{
		m_pMergedMeshesManager->Update(passInfo);
	}

	// unload old meshes
	if (passInfo.IsGeneralPass() && m_pTerrain)
	{
		m_pTerrain->CheckNodesGeomUnload(passInfo);
	}

	if (passInfo.IsGeneralPass())
	{
		m_bIsInRenderScene = false;
	}

	if (passInfo.IsGeneralPass() && IsStatObjBufferRenderTasksAllowed() && JobManager::InvokeAsJob("CheckOcclusion"))
		m_pObjManager->EndOcclusionCulling();

	// release shadow views (from now only renderer owns it)
	for (const auto& pair : shadowFrustums)
	{
		auto& shadowFrustum = pair.first;
		CRY_ASSERT(shadowFrustum->pOnePassShadowView);
		shadowFrustum->pOnePassShadowView.reset();
	}
}

void C3DEngine::ResetCoverageBufferSignalVariables()
{
	GetObjManager()->CullQueue().ResetSignalVariables();
}

void C3DEngine::ProcessOcean(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (GetOceanRenderFlags() & OCR_NO_DRAW || !GetVisAreaManager() || GetCVars()->e_DefaultMaterial || !GetTerrain())
		return;

	bool bOceanIsForcedByVisAreaFlags = GetVisAreaManager()->IsOceanVisible();

	if (!IsOutdoorVisible() && !bOceanIsForcedByVisAreaFlags)
		return;

	float fOceanLevel = GetTerrain()->GetWaterLevel();

	// check for case when no any visible terrain sectors has minz lower than ocean level
	bool bOceanVisible = !Get3DEngine()->m_bShowTerrainSurface;
	bOceanVisible |= m_pTerrain->GetDistanceToSectorWithWater() >= 0 && fOceanLevel && m_pTerrain->IsOceanVisible();

	if (bOceanVisible && passInfo.RenderWaterOcean() && m_bOcean)
	{
		Vec3 vCamPos = passInfo.GetCamera().GetPosition();
		float fWaterPlaneSize = passInfo.GetCamera().GetFarPlane();

		AABB boxOcean(Vec3(vCamPos.x - fWaterPlaneSize, vCamPos.y - fWaterPlaneSize, 0),
		              Vec3(vCamPos.x + fWaterPlaneSize, vCamPos.y + fWaterPlaneSize, fOceanLevel + 0.5f));

		if ((!bOceanIsForcedByVisAreaFlags && passInfo.GetCamera().IsAABBVisible_EM(boxOcean)) ||
		    (bOceanIsForcedByVisAreaFlags && passInfo.GetCamera().IsAABBVisible_E(boxOcean)))
		{
			bool bOceanIsVisibleFromIndoor = true;
			if (class PodArray<CCamera>* pMultiCamera = passInfo.GetCamera().m_pMultiCamera)
			{
				for (int i = 0; i < pMultiCamera->Count(); i++)
				{
					CVisArea* pExitPortal = (CVisArea*)(pMultiCamera->Get(i))->m_pPortal;
					float fMinZ = pExitPortal->GetAABBox()->min.z;
					float fMaxZ = pExitPortal->GetAABBox()->max.z;

					if (!bOceanIsForcedByVisAreaFlags)
					{
						if (fMinZ > fOceanLevel && vCamPos.z < fMinZ)
							bOceanIsVisibleFromIndoor = false;

						if (fMaxZ < fOceanLevel && vCamPos.z > fMaxZ)
							bOceanIsVisibleFromIndoor = false;
					}
				}
			}

			if (bOceanIsVisibleFromIndoor)
			{
				m_pTerrain->UpdateOcean(passInfo);

				if ((GetOceanRenderFlags() & OCR_OCEANVOLUME_VISIBLE))
				{
					m_pTerrain->RenderOcean(passInfo);

					if (passInfo.RenderWaterWaves())
						GetWaterWaveManager()->Update(passInfo);
				}
			}
		}
	}
}

void C3DEngine::RenderSkyBox(IMaterial* pMat, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	const float fForceDrawLastSortOffset = 100000.0f;

	Vec3 vSkyLight(0.0f, 0.0f, 0.0f);

	// hdr sky dome
	// TODO: temporary workaround to force the right sky dome for the selected shader
	if (m_pREHDRSky && IsHDRSkyMaterial(pMat))
	{
		if (GetCVars()->e_SkyBox)
		{
#ifndef CONSOLE_CONST_CVAR_MODE
			if (GetCVars()->e_SkyQuality < 1)
				GetCVars()->e_SkyQuality = 1;
			else if (GetCVars()->e_SkyQuality > 2)
				GetCVars()->e_SkyQuality = 2;
#endif
			m_pSkyLightManager->SetQuality(GetCVars()->e_SkyQuality);

			// set sky light incremental update rate and perform update
			if (GetCVars()->e_SkyUpdateRate <= 0.0f)
				GetCVars()->e_SkyUpdateRate = 0.01f;
			m_pSkyLightManager->IncrementalUpdate(GetCVars()->e_SkyUpdateRate, passInfo);

			// prepare render object
			CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
			if (!pObj)
				return;
			pObj->SetMatrix(Matrix34::CreateTranslationMat(passInfo.GetCamera().GetPosition()), passInfo);
			pObj->m_ObjFlags |= FOB_TRANS_TRANSLATE;
			pObj->m_pRenderNode = 0;//m_pREHDRSky;
			pObj->m_fSort = fForceDrawLastSortOffset; // force sky to draw last

			/*			if( 0 == m_nRenderStackLevel )
			   {
			   // set scissor rect
			   pObj->m_nScissorX1 = GetCamera().m_ScissorInfo.x1;
			   pObj->m_nScissorY1 = GetCamera().m_ScissorInfo.y1;
			   pObj->m_nScissorX2 = GetCamera().m_ScissorInfo.x2;
			   pObj->m_nScissorY2 = GetCamera().m_ScissorInfo.y2;
			   }*/

			m_pREHDRSky->m_pRenderParams = m_pSkyLightManager->GetRenderParams();
			m_pREHDRSky->m_moonTexId = m_nNightMoonTexId;

			// add sky dome to render list
			passInfo.GetIRenderView()->AddRenderObject(m_pREHDRSky, pMat->GetShaderItem(), pObj, passInfo, EFSLIST_GENERAL, 1);

			// get sky lighting parameter.
			const SSkyLightRenderParams* pSkyParams = m_pSkyLightManager->GetRenderParams();
			if (pSkyParams)
			{
				Vec4 skylightRayleighInScatter;
				skylightRayleighInScatter = pSkyParams->m_hazeColorRayleighNoPremul * pSkyParams->m_partialRayleighInScatteringConst;
				vSkyLight = Vec3(skylightRayleighInScatter);
			}
		}
	}
	// skybox
	else
	{
		if (pMat && m_pRESky && GetCVars()->e_SkyBox)
		{
			CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
			if (!pObj)
				return;
			Matrix34 mat = Matrix34::CreateTranslationMat(passInfo.GetCamera().GetPosition());
			pObj->SetMatrix(mat * Matrix33::CreateRotationZ(DEG2RAD(m_fSkyBoxAngle)), passInfo);
			pObj->m_ObjFlags |= FOB_TRANS_TRANSLATE | FOB_TRANS_ROTATE;
			pObj->m_fSort = fForceDrawLastSortOffset; // force sky to draw last

			m_pRESky->m_fTerrainWaterLevel = max(0.0f, m_pTerrain->GetWaterLevel());
			m_pRESky->m_fSkyBoxStretching = m_fSkyBoxStretching;

			passInfo.GetIRenderView()->AddRenderObject(m_pRESky, pMat->GetShaderItem(), pObj, passInfo, EFSLIST_GENERAL, 1);
		}
	}

	SetGlobalParameter(E3DPARAM_SKYLIGHT_RAYLEIGH_INSCATTER, vSkyLight);
}

void C3DEngine::DrawTextRightAligned(const float x, const float y, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	IRenderAuxText::DrawText(Vec3(x, y, 1.0f), DISPLAY_INFO_SCALE, NULL, eDrawText_FixedSize | eDrawText_Right | eDrawText_2D | eDrawText_Monospace, format, args);
	va_end(args);
}

void C3DEngine::DrawTextAligned(int flags, const float x, const float y, const float scale, const ColorF& color, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	IRenderAuxText::DrawText(Vec3(x, y, 1.0f), scale, color, flags, format, args);

	va_end(args);
}

void C3DEngine::DrawTextLeftAligned(const float x, const float y, const float scale, const ColorF& color, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	IRenderAuxText::DrawText(Vec3(x, y, 1.0f), scale, color, eDrawText_FixedSize | eDrawText_2D | eDrawText_Monospace, format, args);
	va_end(args);

}

void C3DEngine::DrawTextRightAligned(const float x, const float y, const float scale, const ColorF& color, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	IRenderAuxText::DrawText(Vec3(x, y, 1.0f), scale, color, eDrawText_FixedSize | eDrawText_Right | eDrawText_2D | eDrawText_Monospace, format, args);
	va_end(args);
}

int __cdecl C3DEngine__Cmp_FPS(const void* v1, const void* v2)
{
	float f1 = *(float*)v1;
	float f2 = *(float*)v2;

	if (f1 > f2)
		return 1;
	else if (f1 < f2)
		return -1;

	return 0;
}

inline void Blend(float& Stat, float StatCur, float fBlendCur)
{
	Stat = Stat * (1.f - fBlendCur) + StatCur * fBlendCur;
}

inline void Blend(float& Stat, int& StatCur, float fBlendCur)
{
	Blend(Stat, float(StatCur), fBlendCur);
	StatCur = int_round(Stat);
}

static void AppendString(char*& szEnd, const char* szToAppend)
{
	assert(szToAppend);

	while (*szToAppend)
		*szEnd++ = *szToAppend++;

	*szEnd++ = ' ';
	*szEnd = 0;
}

void C3DEngine::DisplayInfo(float& fTextPosX, float& fTextPosY, float& fTextStepY, const bool bEnhanced)
{

#ifdef ENABLE_LW_PROFILERS
	//  FUNCTION_PROFILER_3DENGINE; causes 0 fps in stats
	static ICVar* pDisplayInfo = GetConsole()->GetCVar("r_DisplayInfo");
	int displayInfoVal = pDisplayInfo->GetIVal();
	if (displayInfoVal <= 0 || gEnv->IsDedicated())
	{
		return;
	}

	IRenderAuxGeom* pAux = GetRenderer()->GetIRenderAuxGeom();
	const SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
	SAuxGeomRenderFlags newFlags(flags);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetMode2D3DFlag(e_Mode2D);
	newFlags.SetCullMode(e_CullModeNone);
	newFlags.SetDepthWriteFlag(e_DepthWriteOff);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	newFlags.SetFillMode(e_FillModeSolid);

	pAux->SetRenderFlags(newFlags);

	const int iDisplayResolutionX = pAux->GetCamera().GetViewSurfaceX();
	const int iDisplayResolutionY = pAux->GetCamera().GetViewSurfaceZ();
	const float fDisplayMarginRes = 5.0f;
	const float fDisplayMarginNormX = (float)fDisplayMarginRes / (float)iDisplayResolutionX;
	const float fDisplayMarginNormY = (float)fDisplayMarginRes / (float)iDisplayResolutionY;
	Vec2 overscanBorderNorm = Vec2(0.0f, 0.0f);
	gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorderNorm);

	#if defined(INFO_FRAME_COUNTER)
	static int frameCounter = 0;
	#endif

	// If stat averaging is on, compute blend amount for current stats.
	float fFPS = GetTimer()->GetFrameRate();

	arrFPSforSaveLevelStats.push_back(SATURATEB((int)fFPS));

	float fBlendTime = GetTimer()->GetCurrTime();
	int iBlendMode = 0;
	float fBlendCur = GetTimer()->GetProfileFrameBlending(&fBlendTime, &iBlendMode);

	fTextPosY = -10;
	fTextStepY = 13;
	fTextPosX = iDisplayResolutionX - fDisplayMarginRes;

	if (displayInfoVal == 3 || displayInfoVal == 4)
	{
		static float fCurrentFPS, fCurrentFrameTime;
		Blend(fCurrentFPS, fFPS, fBlendCur);
		Blend(fCurrentFrameTime, GetTimer()->GetRealFrameTime() * 1000.0f, fBlendCur);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 1.5f, ColorF(1.0f, 1.0f, 0.5f, 1.0f),
		                     "FPS %.1f - %.1fms", fCurrentFPS, fCurrentFrameTime);

		if (displayInfoVal == 4)
		{
			const float fBarBottomYRes = fTextPosY + UIDRAW_TEXTSIZEFACTOR;
			const float fBarYNorm = (fBarBottomYRes / iDisplayResolutionY) + overscanBorderNorm.y;
			const float fBarMarkersTopYNorm = ((fBarBottomYRes - 10) / iDisplayResolutionY) + overscanBorderNorm.y;
			const float fBarMarkersBottomYNorm = ((fBarBottomYRes + 10) / iDisplayResolutionY) + overscanBorderNorm.y;
			const float fBarMarkersTextYRes = fBarBottomYRes + 10;

			const float barMinNorm = fDisplayMarginNormX + overscanBorderNorm.x;
			const float barMaxNorm = 0.85f - overscanBorderNorm.x;
			const float barSizeNorm = barMaxNorm - barMinNorm;
			const float barSizeRes = barSizeNorm * iDisplayResolutionX;

			float targetFPS = gEnv->pConsole->GetCVar("r_displayinfoTargetFPS")->GetFVal();
			if (targetFPS <= 0)
			{
				targetFPS = 30;
			}
			float millisecTarget = (1000.0f / targetFPS);
			float millisecMin = max((millisecTarget - 10), 0.0f);
			float millisecMax = millisecTarget + (millisecTarget - millisecMin);

			float barPercentFilled = max(min(((fCurrentFrameTime - millisecMin) / (millisecMax - millisecMin)), 1.0f), 0.0f); // clamping bar to +-10ms from target

			// bar
			ColorB barColor = (fCurrentFrameTime > millisecTarget) ? ColorB(255, 0, 0) : ColorB(0, 255, 0);
			pAux->DrawLine(
			  Vec3(barMinNorm, fBarYNorm, 0), barColor,
			  Vec3(barMinNorm + (barPercentFilled * barSizeNorm), fBarYNorm, 0), barColor, (5760.0f / iDisplayResolutionY)); // 5760 = 8*720

			// markers
			pAux->DrawLine(Vec3(barMinNorm, fBarMarkersBottomYNorm, 0), Col_White, Vec3(barMinNorm, fBarMarkersTopYNorm, 0), Col_White, 1.0f);
			if (millisecMin == 0)
			{
				DrawTextLeftAligned(fDisplayMarginRes, fBarMarkersTextYRes, DISPLAY_INFO_SCALE_SMALL, Col_White, "%.1fms (inf FPS)", millisecMin);
			}
			else
			{
				DrawTextLeftAligned(fDisplayMarginRes, fBarMarkersTextYRes, DISPLAY_INFO_SCALE_SMALL, Col_White, "%.1fms (%.1f FPS)", millisecMin, (1000.0f / millisecMin));
			}

			float xPos = (0.5f * barSizeNorm + barMinNorm);
			pAux->DrawLine(Vec3(xPos, fBarMarkersBottomYNorm, 0), Col_White, Vec3(xPos, fBarMarkersTopYNorm, 0), Col_White, 1.0f);
			DrawTextLeftAligned(fDisplayMarginRes + (0.5f * barSizeRes), fBarMarkersTextYRes, DISPLAY_INFO_SCALE_SMALL, Col_White, "%.1fms (%.1f FPS)", millisecTarget, targetFPS);

			pAux->DrawLine(Vec3(barMaxNorm, fBarMarkersBottomYNorm, 0), Col_White, Vec3(barMaxNorm, fBarMarkersTopYNorm, 0), Col_White, 1.0f);
			DrawTextLeftAligned(fDisplayMarginRes + barSizeRes, fBarMarkersTextYRes, DISPLAY_INFO_SCALE_SMALL, Col_White, "%.1fms (%.1f FPS)", millisecMax, (1000.0f / millisecMax));
		}

		pAux->SetRenderFlags(flags);
		return;
	}

	// make level name
	char szLevelName[128];

	*szLevelName = 0;
	{
		int ii;
		for (ii = strlen(m_szLevelFolder) - 2; ii > 0; ii--)
			if (m_szLevelFolder[ii] == '\\' || m_szLevelFolder[ii] == '/')
				break;

		if (ii >= 0)
		{
			cry_strcpy(szLevelName, &m_szLevelFolder[ii + 1]);

			for (int i = strlen(szLevelName) - 1; i > 0; i--)
				if (szLevelName[i] == '\\' || szLevelName[i] == '/')
					szLevelName[i] = 0;
		}
	}

	Matrix33 m = Matrix33(GetRenderingCamera().GetMatrix());
	//m.OrthonormalizeFast();		// why is that needed? is it?
	Ang3 aAng = RAD2DEG(Ang3::GetAnglesXYZ(m));
	Vec3 vPos = GetRenderingCamera().GetPosition();

	// display out of memory message if an allocation failed
	IF (gEnv->bIsOutOfMemory, 0)
	{
		ColorF fColor(1.0f, 0.0f, 0.0f, 1.0f);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 4.0f, fColor, "**** Out of Memory ****");
		fTextPosY += 40.0f;
	}
	// display out of memory message if an allocation failed
	IF (gEnv->bIsOutOfVideoMemory, 0)
	{
		ColorF fColor(1.0f, 0.0f, 0.0f, 1.0f);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 4.0f, fColor, "**** Out of Video Memory ****");
		fTextPosY += 40.0f;
	}

	float fogCullDist = 0.0f;
	Vec2 vViewportScale = Vec2(0.0f, 0.0f);
	m_pRenderer->EF_Query(EFQ_GetFogCullDistance, fogCullDist);
	m_pRenderer->EF_Query(EFQ_GetViewportDownscaleFactor, vViewportScale);

	DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "CamPos=%.2f %.2f %.2f Angl=%3d %2d %3d ZN=%.2f ZF=%d FC=%.2f VS=%.2f,%.2f Zoom=%.2f Speed=%1.2f",
	                     vPos.x, vPos.y, vPos.z, (int)aAng.x, (int)aAng.y, (int)aAng.z,
	                     GetRenderingCamera().GetNearPlane(), (int)GetRenderingCamera().GetFarPlane(), fogCullDist,
	                     vViewportScale.x, vViewportScale.y,
	                     GetZoomFactor(), GetAverageCameraSpeed());

	// get version
	const SFileVersion& ver = GetSystem()->GetFileVersion();
	//char sVersion[128];
	//ver.ToString(sVersion);

	// Get memory usage.
	static IMemoryManager::SProcessMemInfo processMemInfo;
	{
		static int nGetMemInfoCount = 0;
		if ((nGetMemInfoCount & 0x1F) == 0 && GetISystem()->GetIMemoryManager())
		{
			// Only get mem stats every 32 frames.
			GetISystem()->GetIMemoryManager()->GetProcessMemInfo(processMemInfo);
		}
		nGetMemInfoCount++;
	}

	bool bMultiGPU;
	m_pRenderer->EF_Query(EFQ_MultiGPUEnabled, bMultiGPU);

	const char* pRenderType(0);

	switch (gEnv->pRenderer->GetRenderType())
	{
	case ERenderType::Direct3D11:
		pRenderType = STR_DX11_RENDERER;
		break;
	case ERenderType::Direct3D12:
		pRenderType = STR_DX12_RENDERER;
		break;
	case ERenderType::OpenGL:
		pRenderType = STR_GL_RENDERER;
		break;
	case ERenderType::Vulkan:
		pRenderType = STR_VK_RENDERER;
		break;
	case ERenderType::GNM:
		pRenderType = STR_GNM_RENDERER;
		break;
	case ERenderType::Undefined:
	default:
		assert(0);
		pRenderType = "Undefined";
		break;
	}

	assert(gEnv->pSystem);
	bool bTextureStreamingEnabled = false;
	m_pRenderer->EF_Query(EFQ_TextureStreamingEnabled, bTextureStreamingEnabled);
	const bool bCGFStreaming = GetCVars()->e_StreamCgf && m_pObjManager;
	const bool bTexStreaming = gEnv->pSystem->GetStreamEngine() && bTextureStreamingEnabled;
	char szFlags[128], * szFlagsEnd = szFlags;

	#ifndef _RELEASE
	ESystemConfigSpec spec = GetISystem()->GetConfigSpec();
	switch (spec)
	{
	case CONFIG_CUSTOM:
		AppendString(szFlagsEnd, "Custom");
		break;
	case CONFIG_LOW_SPEC:
		AppendString(szFlagsEnd, "LowSpec");
		break;
	case CONFIG_MEDIUM_SPEC:
		AppendString(szFlagsEnd, "MedSpec");
		break;
	case CONFIG_HIGH_SPEC:
		AppendString(szFlagsEnd, "HighSpec");
		break;
	case CONFIG_VERYHIGH_SPEC:
		AppendString(szFlagsEnd, "VeryHighSpec");
		break;
	case CONFIG_DURANGO:
		AppendString(szFlagsEnd, "XboxOneSpec");
		break;
	case CONFIG_ORBIS:
		AppendString(szFlagsEnd, "PS4Spec");
		break;
	default:
		assert(0);
	}
	#endif
	#ifndef CONSOLE_CONST_CVAR_MODE
	static ICVar* pMultiThreaded = GetConsole()->GetCVar("r_MultiThreaded");
	if (pMultiThreaded && pMultiThreaded->GetIVal() > 0)
	#endif
	AppendString(szFlagsEnd, "MT");

	char* sAAMode = NULL;
	m_pRenderer->EF_Query(EFQ_AAMode, sAAMode);
	AppendString(szFlagsEnd, sAAMode);

	#if defined(FEATURE_SVO_GI)
	if (GetCVars()->e_svoTI_Apply)
		AppendString(szFlagsEnd, "SVOGI");
	#endif

	if (IsAreaActivationInUse())
		AppendString(szFlagsEnd, "LA");

	if (bMultiGPU)
		AppendString(szFlagsEnd, "MGPU");

	if (gEnv->pCryPak->GetLvlResStatus())
		AppendString(szFlagsEnd, "LvlRes");

	if (gEnv->pSystem->IsDevMode())
		AppendString(szFlagsEnd, gEnv->IsEditor() ? "DevMode (Editor)" : "DevMode");

	if (bCGFStreaming || bTexStreaming)
	{
		if (bCGFStreaming && !bTexStreaming)
			AppendString(szFlagsEnd, "StG");
		if (bTexStreaming && !bCGFStreaming)
			AppendString(szFlagsEnd, "StT");
		if (bTexStreaming && bCGFStreaming)
			AppendString(szFlagsEnd, "StGT");
	}

	// remove last space
	if (szFlags != szFlagsEnd)
		*(szFlagsEnd - 1) = 0;
	#ifdef _RELEASE
	const char* mode = "Release";
	#else
	const char* mode = "Profile";
	#endif

	DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s %s %dbit %s %s [%d.%d]",
	                     pRenderType, mode, (int)sizeof(char*) * 8, szFlags, szLevelName, ver.v[1], ver.v[0]);

	// Polys in scene
	int nPolygons, nShadowPolygons;
	GetRenderer()->GetPolyCount(nPolygons, nShadowPolygons);
	int nDrawCalls, nShadowGenDrawCalls;
	GetRenderer()->GetCurrentNumberOfDrawCalls(nDrawCalls, nShadowGenDrawCalls);

	int nGeomInstances = GetRenderer()->GetNumGeomInstances();
	int nGeomInstanceDrawCalls = GetRenderer()->GetNumGeomInstanceDrawCalls();

	if (fBlendCur != 1.f)
	{
		// Smooth over time.
		static float fPolygons, fShadowVolPolys, fDrawCalls, fShadowGenDrawCalls, fGeomInstances, fGeomInstanceDrawCalls;
		Blend(fPolygons, nPolygons, fBlendCur);
		Blend(fShadowVolPolys, nShadowPolygons, fBlendCur);
		Blend(fDrawCalls, nDrawCalls, fBlendCur);
		Blend(fShadowGenDrawCalls, nShadowGenDrawCalls, fBlendCur);
		Blend(fGeomInstances, nGeomInstances, fBlendCur);
		Blend(fGeomInstanceDrawCalls, nGeomInstanceDrawCalls, fBlendCur);
	}

	//
	static float m_lastAverageDPTime = -FLT_MAX;
	float curTime = gEnv->pTimer->GetAsyncCurTime();
	static int lastDrawCalls = 0;
	static int lastShadowGenDrawCalls = 0;
	static int avgPolys = 0;
	static int avgShadowPolys = 0;
	static int sumPolys = 0;
	static int sumShadowPolys = 0;
	static int nPolysFrames = 0;
	if (curTime < m_lastAverageDPTime)
	{
		m_lastAverageDPTime = curTime;
	}
	if (curTime - m_lastAverageDPTime > 1.0f)
	{
		lastDrawCalls = nDrawCalls;
		lastShadowGenDrawCalls = nShadowGenDrawCalls;
		m_lastAverageDPTime = curTime;
		avgPolys = nPolysFrames ? sumPolys / nPolysFrames : 0;
		avgShadowPolys = nPolysFrames ? sumShadowPolys / nPolysFrames : 0;
		sumPolys = nPolygons;
		sumShadowPolys = nShadowPolygons;
		nPolysFrames = 1;
	}
	else
	{
		nPolysFrames++;
		sumPolys += nPolygons;
		sumShadowPolys += nShadowPolygons;
	}
	//

	int nMaxDrawCalls = GetCVars()->e_MaxDrawCalls <= 0 ? 2000 : GetCVars()->e_MaxDrawCalls;
	bool bInRed = (nDrawCalls + nShadowGenDrawCalls) > nMaxDrawCalls;

	DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, bInRed ? Col_Red : Col_White, "DP: %04d (%04d) ShadowGen:%04d (%04d) - Total: %04d Instanced: %04d",
	                     nDrawCalls, lastDrawCalls, nShadowGenDrawCalls, lastShadowGenDrawCalls, nDrawCalls + nShadowGenDrawCalls, nDrawCalls + nShadowGenDrawCalls - nGeomInstances + nGeomInstanceDrawCalls);
	#if CRY_PLATFORM_MOBILE
	bInRed = nPolygons > 500000;
	#else
	bInRed = nPolygons > 1500000;
	#endif

	DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, bInRed ? Col_Red : Col_White, "Polys: %03d,%03d (%03d,%03d) Shadow:%03d,%03d (%03d,%03d)",
	                     nPolygons / 1000, nPolygons % 1000, avgPolys / 1000, avgPolys % 1000,
	                     nShadowPolygons / 1000, nShadowPolygons % 1000, avgShadowPolys / 1000, avgShadowPolys % 1000);

	{
		SShaderCacheStatistics stats;
		m_pRenderer->EF_Query(EFQ_GetShaderCacheInfo, stats);
		{

			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_White, "ShaderCache: %d GCM | %d Async Reqs | Compile: %s",
			                     (int)stats.m_nGlobalShaderCacheMisses, (int)stats.m_nNumShaderAsyncCompiles, stats.m_bShaderCompileActive ? "On" : "Off");
		}
	}

	// print stats about CGF's streaming
	if (bCGFStreaming)
	{
		static char szCGFStreaming[256] = "";
		static SObjectsStreamingStatus objectsStreamingStatus = { 0 };

		if (!(GetRenderer()->GetFrameID(false) & 15) || !szCGFStreaming[0] || GetCVars()->e_StreamCgfDebug)
		{
			m_pObjManager->GetObjectsStreamingStatus(objectsStreamingStatus);
			cry_sprintf(szCGFStreaming, "CgfStrm: Loaded:%d InProg:%d All:%d Act:%d PcP:%d MemUsed:%2.2f MemReq:%2.2f Pool:%d",
			            objectsStreamingStatus.nReady, objectsStreamingStatus.nInProgress, objectsStreamingStatus.nTotal, objectsStreamingStatus.nActive,
			            (int)m_pObjManager->m_vStreamPreCachePointDefs.size(),
			            float(objectsStreamingStatus.nAllocatedBytes) / 1024 / 1024, float(objectsStreamingStatus.nMemRequired) / 1024 / 1024, GetCVars()->e_StreamCgfPoolSize);
		}

		bool bOutOfMem((float(objectsStreamingStatus.nMemRequired) / 1024 / 1024) > GetCVars()->e_StreamCgfPoolSize);
		bool bCloseToOutOfMem((float(objectsStreamingStatus.nMemRequired) / 1024 / 1024) > GetCVars()->e_StreamCgfPoolSize * 90 / 100);

		ColorF color = Col_White;
		if (bOutOfMem)
			color = Col_Red;
		else if (bCloseToOutOfMem)
			color = Col_Orange;
		//		if(bTooManyRequests)
		//		color = Col_Magenta;

		if ((displayInfoVal == 2 || GetCVars()->e_StreamCgfDebug) || bOutOfMem || bCloseToOutOfMem)
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, color, "%s", szCGFStreaming);
	}

	bool bShowAnimationOutOfBudget = (gEnv->pCharacterManager != NULL);
	if (bShowAnimationOutOfBudget && gEnv->pCharacterManager)
	{
		uint32 nAnimKeysBudget = 10 * 1024 * 1024; // 10MB for now.

		SAnimMemoryTracker animMem = gEnv->pCharacterManager->GetAnimMemoryTracker();
		bool bOutOfMem = animMem.m_nAnimsCurrent > nAnimKeysBudget;

		static char szAnimString[64] = "";
		if (0 == (GetRenderer()->GetFrameID(false) & 15))
		{
			if (bOutOfMem)
			{
				cry_sprintf(szAnimString, "Animation Keys:  %.1f", animMem.m_nAnimsCurrent / (float)(1024 * 1024));
			}
		}

		if (bOutOfMem)
		{
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, bOutOfMem ? Col_Red : Col_White, "%s", szAnimString);
		}
	}

	if (gEnv->pCharacterManager)
	{
		static ICVar* pAttachmentMerging = GetConsole()->GetCVar("ca_DrawAttachmentsMergedForShadows");
		static ICVar* pAttachmentMergingBudget = GetConsole()->GetCVar("ca_AttachmentMergingMemoryBudget");

		if (pAttachmentMerging && pAttachmentMergingBudget && pAttachmentMerging->GetIVal() > 0)
		{
			const IAttachmentMerger& pAttachmentMerger = gEnv->pCharacterManager->GetIAttachmentMerger();
			uint nAllocatedBytes = pAttachmentMerger.GetAllocatedBytes();
			bool bOutOfMemory = pAttachmentMerger.IsOutOfMemory();

			if (bOutOfMemory || pAttachmentMerging->GetIVal() > 1)
			{
				char buffer[64];
				cry_sprintf(buffer, "Character Attachment Merging (Shadows): %0.2f of %0.2f MB", nAllocatedBytes / (1024.f * 1024.f), pAttachmentMergingBudget->GetIVal() / (1024.0f * 1024.0f));
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, bOutOfMemory ? Col_Red : Col_White, "%s", buffer);
			}
		}
	}

	// print stats about textures' streaming
	if (bTexStreaming)
	{
		static char szTexStreaming[256] = "";
		static bool bCloseToOutOfMem = false;
		static bool bOutOfMem = false;
		static bool bTooManyRequests = false;
		static bool bOverloadedPool = false;
		static uint32 nTexCount = 0;
		static uint32 nTexSize = 0;

		float fTexBandwidthRequired = 0.f;
		m_pRenderer->GetBandwidthStats(&fTexBandwidthRequired);

		if (!(GetRenderer()->GetFrameID(false) % 30) || !szTexStreaming[0])
		{
			STextureStreamingStats stats(!(GetRenderer()->GetFrameID(false) % 120));
			m_pRenderer->EF_Query(EFQ_GetTexStreamingInfo, stats);

			if (!(GetRenderer()->GetFrameID(false) % 120))
			{
				bOverloadedPool = stats.bPoolOverflowTotally;
				nTexCount = stats.nRequiredStreamedTexturesCount;
				nTexSize = stats.nRequiredStreamedTexturesSize;
			}

			int nPlatformSize = nTexSize;

			const int iPercentage = int((float)stats.nCurrentPoolSize / stats.nMaxPoolSize * 100.f);
			const int iStaticPercentage = int((float)stats.nStaticTexturesSize / stats.nMaxPoolSize * 100.f);
			cry_sprintf(szTexStreaming, "TexStrm: TexRend: %u NumTex: %u Req:%.1fMB Mem(strm/stat/tot):%.1f/%.1f/%.1fMB(%d%%/%d%%) PoolSize:%" PRISIZE_T "MB PoolFrag:%.1f%%",
			            stats.nNumTexturesPerFrame, nTexCount, (float)nPlatformSize / 1024 / 1024,
			            (float)stats.nStreamedTexturesSize / 1024 / 1024, (float)stats.nStaticTexturesSize / 1024 / 1024, (float)stats.nCurrentPoolSize / 1024 / 1024,
			            iPercentage, iStaticPercentage, stats.nMaxPoolSize / 1024 / 1024,
			            stats.fPoolFragmentation * 100.0f
			            );
			bOverloadedPool |= stats.bPoolOverflowTotally;

			bCloseToOutOfMem = iPercentage >= 90;
			bOutOfMem = stats.bPoolOverflow;
		}

		if (displayInfoVal == 2 || bCloseToOutOfMem || bTooManyRequests || bOverloadedPool)
		{
			ColorF color = Col_White;
			if (bOutOfMem)
				color = Col_Red;
			else if (bCloseToOutOfMem)
				color = Col_Orange;
			if (bTooManyRequests)
				color = Col_Magenta;
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, color, "%s", szTexStreaming);
		}

		if (bOverloadedPool)
		{
			DrawTextLeftAligned(0, 10, 2.3f, Col_Red, "Texture pool totally overloaded!");
		}
	}

	{
		static char szMeshPoolUse[256] = "";
		static unsigned nFlushFrameId = 0U;
		static unsigned nFallbackFrameId = 0U;
		static SMeshPoolStatistics lastStats;
		static SMeshPoolStatistics stats;

		const unsigned nMainFrameId = GetRenderer()->GetFrameID(false);
		m_pRenderer->EF_Query(EFQ_GetMeshPoolInfo, stats);
		const int iPercentage = int((float)stats.nPoolInUse / (stats.nPoolSize ? stats.nPoolSize : 1U) * 100.f);
		const int iVolatilePercentage = int((float)stats.nInstancePoolInUse / (stats.nInstancePoolSize ? stats.nInstancePoolSize : 1U) * 100.f);
		nFallbackFrameId = lastStats.nFallbacks < stats.nFallbacks ? nMainFrameId : nFallbackFrameId;
		nFlushFrameId = lastStats.nFlushes < stats.nFlushes ? nMainFrameId : nFlushFrameId;
		const bool bOverflow = nMainFrameId - nFlushFrameId < 50;
		const bool bFallback = nMainFrameId - nFallbackFrameId < 50;

		cry_sprintf(szMeshPoolUse,
		            "Mesh Pool: MemUsed:%.2fKB(%d%%%%) Peak %.fKB PoolSize:%" PRISIZE_T "KB Flushes %" PRISIZE_T " Fallbacks %.3fKB %s",
		            (float)stats.nPoolInUse / 1024,
		            iPercentage,
		            (float)stats.nPoolInUsePeak / 1024,
		            stats.nPoolSize / 1024,
		            stats.nFlushes,
		            (float)stats.nFallbacks / 1024.0f,
		            (bFallback ? "FULL!" : bOverflow ? "OVERFLOW" : ""));

		if (stats.nPoolSize && (displayInfoVal == 2 || bOverflow || bFallback))
		{
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE,
			                     bFallback ? Col_Red : bOverflow ? Col_Orange : Col_White,
			                     "%s", szMeshPoolUse);
		}
		if (stats.nPoolSize && displayInfoVal == 2)
		{
			char szVolatilePoolsUse[256];
			cry_sprintf(szVolatilePoolsUse,
			            "Mesh Instance Pool: MemUsed:%.2fKB(%d%%%%) Peak %.fKB PoolSize:%" PRISIZE_T "KB Fallbacks %.3fKB",
			            (float)stats.nInstancePoolInUse / 1024,
			            iVolatilePercentage,
			            (float)stats.nInstancePoolInUsePeak / 1024,
			            stats.nInstancePoolSize / 1024,
			            (float)stats.nInstanceFallbacks / 1024.0f);

			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE,
			                     Col_White, "%s", szVolatilePoolsUse);
		}

		memcpy(&lastStats, &stats, sizeof(lastStats));
	}

	// streaming info
	{
		IStreamEngine* pSE = gEnv->pSystem->GetStreamEngine();
		if (pSE)
		{
			SStreamEngineStatistics& stats = pSE->GetStreamingStatistics();
			SStreamEngineOpenStats openStats;
			pSE->GetStreamingOpenStatistics(openStats);

			static char szStreaming[128] = "";
			if (!(GetRenderer()->GetFrameID(false) & 7))
			{

				if (displayInfoVal == 2)
				{
					cry_sprintf(szStreaming, "Streaming IO: ACT: %3umsec, Jobs:%2d Total:%5u",
					            (uint32)stats.fAverageCompletionTime, openStats.nOpenRequestCount, stats.nTotalStreamingRequestCount);
				}
				else
				{
					cry_sprintf(szStreaming, "Streaming IO: ACT: %3umsec, Jobs:%2d",
					            (uint32)stats.fAverageCompletionTime, openStats.nOpenRequestCount);
				}
			}
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s", szStreaming);

			if (stats.bTempMemOutOfBudget)
			{
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 1.3f, Col_Red, "Temporary Streaming Memory Pool Out of Budget!");
			}
		}

		if (displayInfoVal == 2)  // more streaming info
		{
			SStreamEngineStatistics& stats = gEnv->pSystem->GetStreamEngine()->GetStreamingStatistics();

			{
				// HDD stats
				static char szStreaming[512];
				cry_sprintf(szStreaming, "HDD: BW:%1.2f|%1.2fMb/s (Eff:%2.1f|%2.1fMb/s) - Seek:%1.2fGB - Active:%2.1f%%%%",
				            (float)stats.hddInfo.nCurrentReadBandwidth / (1024 * 1024), (float)stats.hddInfo.nSessionReadBandwidth / (1024 * 1024),
				            (float)stats.hddInfo.nActualReadBandwidth / (1024 * 1024), (float)stats.hddInfo.nAverageActualReadBandwidth / (1024 * 1024),
				            (float)stats.hddInfo.nAverageSeekOffset / (1024 * 1024), stats.hddInfo.fAverageActiveTime);

				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s", szStreaming);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Display Info about dynamic lights.
	//////////////////////////////////////////////////////////////////////////
	{
		if (GetCVars()->e_DebugLights)
		{
			char sLightsList[512] = "";
			for (int i = 0; i < m_lstDynLights.Count(); i++)
			{
				if (m_lstDynLights[i]->m_Id >= 0 && m_lstDynLights[i]->m_fRadius >= 0.5f)
					if (!(m_lstDynLights[i]->m_Flags & DLF_FAKE))
					{
						if (GetCVars()->e_DebugLights == 2)
						{
							int nShadowCasterNumber = 0;
							for (int l = 0; l < MAX_GSM_LODS_NUM; l++)
							{
								if (ShadowMapFrustum* pFr = m_lstDynLights[i]->m_pOwner->GetShadowFrustum(l))
									nShadowCasterNumber += pFr->GetCasterNum();
							}
							DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s - SM%d", m_lstDynLights[i]->m_sName, nShadowCasterNumber);
						}

						if (i < 4)
						{
							char buff[32];
							cry_strcpy(buff, m_lstDynLights[i]->m_sName, 8);

							if (m_lstDynLights[i]->m_Flags & DLF_CASTSHADOW_MAPS)
							{
								cry_strcat(buff, "-SM");

								int nCastingObjects = 0;
								for (int l = 0; l < MAX_GSM_LODS_NUM; l++)
								{
									if (ShadowMapFrustum* pFr = m_lstDynLights[i]->m_pOwner->GetShadowFrustum(l))
										nCastingObjects += pFr->GetCasterNum();
								}

								if (nCastingObjects)
								{
									char tmp[32];
									cry_sprintf(tmp, "%d", nCastingObjects);
									cry_strcat(buff, tmp);
								}
							}

							cry_strcat(sLightsList, buff);
							if (i < m_lstDynLights.Count() - 1)
								cry_strcat(sLightsList, ",");
						}
					}
			}

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		#pragma warning( push )             //AMD Port
		#pragma warning( disable : 4267 )
	#endif

			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "DLights=%s(%d/%d)", sLightsList, m_nRealLightsNum + m_nDeferredLightsNum, m_lstDynLights.Count());
		}
		else
		{
	#ifndef _RELEASE
			// Checkpoint loading information
			if (!gEnv->bMultiplayer)
			{
				ISystem::ICheckpointData data;
				gEnv->pSystem->GetCheckpointData(data);

				if (data.m_loadOrigin != ISystem::eLLO_Unknown)
				{
					static const char* loadStates[] =
					{
						"",
						"New Level",
						"Level to Level",
						"Resumed Game",
						"Map Command",
					};

					DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 1.3f, Col_White, "%s, Checkpoint loads: %i", loadStates[(int)data.m_loadOrigin], (int)data.m_totalLoads);
				}
			}
	#endif
			size_t vidMemUsedThisFrame;
			size_t vidMemUsedRecently;
			GetRenderer()->GetVideoMemoryUsageStats(vidMemUsedThisFrame, vidMemUsedRecently);
			float nVidMemMB = (float)vidMemUsedThisFrame / (1024 * 1024);
			int nPeakMemMB = (int)(processMemInfo.PeakPagefileUsage >> 20);
			int nVirtMemMB = (int)(processMemInfo.PagefileUsage >> 20);
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "Vid=%.2f Mem=%d Peak=%d DLights=(%d/%d)", nVidMemMB, nVirtMemMB, nPeakMemMB, m_nRealLightsNum + m_nDeferredLightsNum, (int)m_lstDynLights.Count());

			if (GetCVars()->e_StreamInstances)
			{
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, ColorF(1.0f, 0.5f, 1.0f),
				                     "Instances streaming tasks: %d loaded: %00d/%00d/%00d nodes, %00d inst",
				                     COctreeNode::GetStreamingTasksNum(),
				                     COctreeNode::GetStreamedInNodesNum(), COctreeNode::m_nNodesCounterStreamable, COctreeNode::m_nNodesCounterAll, COctreeNode::m_nInstCounterLoaded);
			}

			uint32 nShadowFrustums = 0;
			uint32 nShadowAllocs = 0;
			uint32 nShadowMaskChannels = 0;
			m_pRenderer->EF_Query(EFQ_GetShadowPoolFrustumsNum, nShadowFrustums);
			m_pRenderer->EF_Query(EFQ_GetShadowPoolAllocThisFrameNum, nShadowAllocs);
			m_pRenderer->EF_Query(EFQ_GetShadowMaskChannelsNum, nShadowMaskChannels);
			bool bThrash = (nShadowAllocs & 0x80000000) ? true : false;
			nShadowAllocs &= ~0x80000000;
			uint32 nAvailableShadowMaskChannels = nShadowMaskChannels >> 16;
			uint32 nUsedShadowMaskChannels = nShadowMaskChannels & 0xFFFF;
			bool bTooManyLights = nUsedShadowMaskChannels > nAvailableShadowMaskChannels ? true : false;

			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, (nShadowFrustums || nShadowAllocs) ? Col_Yellow : Col_White, "%d Shadow Mask Channels, %3d Frustums, %3d Frustums This Frame, %d One-Pass Frustums",
			                     nUsedShadowMaskChannels, nShadowFrustums, nShadowAllocs, m_onePassShadowFrustumsCount);

			if (bThrash)
			{
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_Red, "SHADOW POOL THRASHING!!!");
			}

			if (bTooManyLights)
			{
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_Red, "TOO MANY SHADOW CASTING LIGHTS (%d/%d)!!!", nUsedShadowMaskChannels, nAvailableShadowMaskChannels);
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_Red, "Consider increasing 'r_ShadowCastingLightsMaxCount'");
			}

	#ifndef _RELEASE
			uint32 numTiledShadingSkippedLights;
			m_pRenderer->EF_Query(EFQ_GetTiledShadingSkippedLightsNum, numTiledShadingSkippedLights);
			if (numTiledShadingSkippedLights > 0)
			{
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE, Col_Red, "TILED SHADING: SKIPPED %d LIGHTS", numTiledShadingSkippedLights);
			}

			if (GetCVars()->e_levelStartupFrameNum)
			{
				static float startupAvgFPS = 0.f;
				static float levelStartupTime = 0;
				static int levelStartupFrameEnd = GetCVars()->e_levelStartupFrameNum + GetCVars()->e_levelStartupFrameDelay;
				int curFrameID = GetRenderer()->GetFrameID(false);

				if (curFrameID >= GetCVars()->e_levelStartupFrameDelay)
				{
					if (curFrameID == GetCVars()->e_levelStartupFrameDelay)
						levelStartupTime = gEnv->pTimer->GetAsyncCurTime();
					if (curFrameID == levelStartupFrameEnd)
					{
						startupAvgFPS = (float)GetCVars()->e_levelStartupFrameNum / (gEnv->pTimer->GetAsyncCurTime() - levelStartupTime);
					}
					if (curFrameID >= levelStartupFrameEnd)
					{
						DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 2.f, Col_Red, "Startup AVG FPS: %.2f", startupAvgFPS);
						fTextPosY += fTextStepY;
					}
				}
			}
	#endif //_RELEASE
		}

		m_nDeferredLightsNum = 0;
	}
	#if CAPTURE_REPLAY_LOG
	{
		CryReplayInfo replayInfo;
		CryGetIMemReplay()->GetInfo(replayInfo);
		if (replayInfo.filename)
		{

			DrawTextRightAligned(
			  fTextPosX, fTextPosY += fTextStepY,
			  "MemReplay log sz: %lluMB cost: %i MB",
			  (replayInfo.writtenLength + (512ULL * 1024ULL)) / (1024ULL * 1024ULL),
			  (replayInfo.trackingSize + (512 * 1024)) / (1024 * 1024));

		}
	}
	#endif
	if (bEnhanced)
	{
	#define CONVX(x)       (((x) / (float)gUpdateTimesNum))
	#define CONVY(y)       (1.f - ((y) / 720.f))
	#define TICKS_TO_MS(t) (1000.f * gEnv->pTimer->TicksToSeconds(t))
	#define MAX_PHYS_TIME 32.f
	#define MAX_PLE_TIME  4.f
		uint32 gUpdateTimeIdx = 0, gUpdateTimesNum = 0;
		const sUpdateTimes* gUpdateTimes = gEnv->pSystem->GetUpdateTimeStats(gUpdateTimeIdx, gUpdateTimesNum);
		if (displayInfoVal >= 5)
		{
			const ColorF colorPhysFull = Col_Blue;
			const ColorF colorSysFull = Col_Green;
			const ColorF colorRenFull = Col_Red;
			const ColorF colorPhysHalf = colorPhysFull * 0.15f;
			const ColorF colorSysHalf = colorSysFull * 0.15f;
			const ColorF colorRenHalf = colorRenFull * 0.15f;
			float phys = (TICKS_TO_MS(gUpdateTimes[0].PhysStepTime) / 66.f) * 720.f;
			float sys = (TICKS_TO_MS(gUpdateTimes[0].SysUpdateTime) / 66.f) * 720.f;
			float ren = (TICKS_TO_MS(gUpdateTimes[0].RenderTime) / 66.f) * 720.f;
			float _lerp = ((float)(max((int)gUpdateTimeIdx - (int)0, 0) / (float)gUpdateTimesNum));
			ColorB colorPhysLast;
			colorPhysLast.lerpFloat(colorPhysFull, colorPhysHalf, _lerp);
			ColorB colorSysLast;
			colorSysLast.lerpFloat(colorSysFull, colorSysHalf, _lerp);
			ColorB colorRenLast;
			colorRenLast.lerpFloat(colorRenFull, colorRenHalf, _lerp);
			Vec3 lastPhys(CONVX(0), CONVY(phys), 1.f);
			Vec3 lastSys(CONVX(0), CONVY(sys), 1.f);
			Vec3 lastRen(CONVX(0), CONVY(ren), 1.f);
			for (uint32 i = 0; i < gUpdateTimesNum; ++i)
			{
				const float x = (float)i;
				_lerp = ((float)(max((int)gUpdateTimeIdx - (int)i, 0) / (float)gUpdateTimesNum));
				const sUpdateTimes& sample = gUpdateTimes[i];
				phys = (TICKS_TO_MS(sample.PhysStepTime) / 66.f) * 720.f;
				sys = (TICKS_TO_MS(sample.SysUpdateTime) / 66.f) * 720.f;
				ren = (TICKS_TO_MS(sample.RenderTime) / 66.f) * 720.f;
				Vec3 curPhys(CONVX(x), CONVY(phys), 1.f);
				Vec3 curSys(CONVX(x), CONVY(sys), 1.f);
				Vec3 curRen(CONVX(x), CONVY(ren), 1.f);
				ColorB colorPhys;
				colorPhys.lerpFloat(colorPhysFull, colorPhysHalf, _lerp);
				ColorB colorSys;
				colorSys.lerpFloat(colorSysFull, colorSysHalf, _lerp);
				ColorB colorRen;
				colorRen.lerpFloat(colorRenFull, colorRenHalf, _lerp);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(lastPhys, colorPhysLast, curPhys, colorPhys);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(lastSys, colorSysLast, curSys, colorSys);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(lastRen, colorRenLast, curRen, colorRen);
				lastPhys = curPhys;
				colorPhysLast = colorPhys;
				lastSys = curSys;
				colorSysLast = colorSys;
				lastRen = curRen;
				colorRenLast = colorRen;
			}
		}
		const float curPhysTime = TICKS_TO_MS(gUpdateTimes[gUpdateTimeIdx].PhysStepTime);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE_SMALL, curPhysTime > MAX_PHYS_TIME ? Col_Red : Col_White, "%3.1f ms      Phys", curPhysTime);
		const float curPhysWaitTime = TICKS_TO_MS(gUpdateTimes[gUpdateTimeIdx].physWaitTime);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE_SMALL, curPhysTime > MAX_PHYS_TIME ? Col_Red : Col_White, "%3.1f ms   WaitPhys", curPhysWaitTime);

		IF (gEnv->pPhysicalWorld, 1)
		{
			const float curPLETime = TICKS_TO_MS(gEnv->pPhysicalWorld->GetPumpLoggedEventsTicks());
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF), DISPLAY_INFO_SCALE_SMALL, curPLETime > MAX_PLE_TIME ? Col_Red : Col_White, "%3.1f ms    PhysEv", curPLETime);
		}
		float partTicks = 0;

		IParticleManager* pPartMan = gEnv->p3DEngine->GetParticleManager();
		IF (pPartMan != NULL, 1)
		{
	#if CRY_PLATFORM_MOBILE
			const float maxVal = 4.f;
	#else
			const float maxVal = 50.f;
	#endif
			partTicks = 1000.0f * gEnv->pTimer->TicksToSeconds(pPartMan->GetLightProfileCounts().NumFrameTicks());
			float fTimeSyncMS = 1000.0f * gEnv->pTimer->TicksToSeconds(pPartMan->GetLightProfileCounts().NumFrameSyncTicks());

			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, partTicks > maxVal ? Col_Red : Col_White, "%.2f(%.2f) ms	      Part", partTicks, fTimeSyncMS);
		}
		//3dengine stats from RenderWorld
		{
	#if CRY_PLATFORM_MOBILE
			const float maxVal = 12.f;
	#else
			const float maxVal = 50.f;
	#endif
			float fTimeMS = TICKS_TO_MS(m_nRenderWorldUSecs) - partTicks;
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF), DISPLAY_INFO_SCALE_SMALL, fTimeMS > maxVal ? Col_Red : Col_White, "%.2f ms RendWorld", fTimeMS);
		}

		ICharacterManager* pCharManager = gEnv->pCharacterManager;
		IF (pCharManager != NULL, 1)
		{
	#if CRY_PLATFORM_MOBILE
			const float maxVal = 5.f;
	#else
			const float maxVal = 50.f;
	#endif
			uint32 nNumCharacters = pCharManager->NumCharacters();
			float fTimeMS = 1000.0f * gEnv->pTimer->TicksToSeconds(pCharManager->NumFrameTicks());
			float fTimeSyncMS = 1000.0f * gEnv->pTimer->TicksToSeconds(pCharManager->NumFrameSyncTicks());
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, fTimeMS > maxVal ? Col_Red : Col_White, "%.2f(%.2f) ms(%2d)      Anim", fTimeMS, fTimeSyncMS, nNumCharacters);
		}

		IAISystem* pAISystem = gEnv->pAISystem;
		IF (pAISystem != NULL, 1)
		{
	#if CRY_PLATFORM_MOBILE
			const float maxVal = 6.f;
	#else
			const float maxVal = 50.f;
	#endif
			float fTimeMS = 1000.0f * gEnv->pTimer->TicksToSeconds(pAISystem->NumFrameTicks());
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, fTimeMS > maxVal ? Col_Red : Col_White, "%.2f ms        AI", fTimeMS);
		}

		if (gEnv->pGameFramework)
		{
	#if CRY_PLATFORM_MOBILE
			const float maxVal = 4.f;
	#else
			const float maxVal = 50.f;
	#endif
			float fTimeMS = 1000.0f * gEnv->pTimer->TicksToSeconds(gEnv->pGameFramework->GetPreUpdateTicks());
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, fTimeMS > maxVal ? Col_Red : Col_White, "%.2f ms    Action", fTimeMS);
		}

		{
			const float flashCost = gEnv->pScaleformHelper ? gEnv->pScaleformHelper->GetFlashProfileResults() : -1.0f;
			if (flashCost >= 0.0f)
			{
				float flashCostInMs = flashCost * 1000.0f;
				DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF), DISPLAY_INFO_SCALE_SMALL, flashCostInMs > 4.0f ? Col_Red : Col_White, "%.2f ms     Flash", flashCostInMs);
			}
		}

		{
	#if CRY_PLATFORM_MOBILE
			const float maxVal = 3.f;
	#else
			const float maxVal = 50.f;
	#endif

			float fTimeMS = 0.0f;

			IFrameProfileSystem* const pProfiler = gEnv->pSystem->GetIProfileSystem();

			if (pProfiler != NULL)
			{
				//pProfiler->
				uint32 const nProfilerCount = pProfiler->GetProfilerCount();

				for (uint32 i = 0; i < nProfilerCount; ++i)
				{
					CFrameProfiler* const pFrameProfiler = pProfiler->GetProfiler(i);

					if (pFrameProfiler != NULL && pFrameProfiler->m_subsystem == PROFILE_AUDIO)
					{
						fTimeMS += pFrameProfiler->m_selfTime.Average();
					}
				}
			}

			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, fTimeMS > maxVal ? Col_Red : Col_White, "%.2f ms     Audio", fTimeMS);
		}

		{
			SStreamEngineStatistics stat = gEnv->pSystem->GetStreamEngine()->GetStreamingStatistics();

			float fTimeMS = 1000.0f * gEnv->pTimer->TicksToSeconds(stat.nMainStreamingThreadWait);
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, Col_White, "%3.1f ms     StreamFin", fTimeMS);

		}

		{
			SNetworkPerformance stat;
			gEnv->pNetwork->GetPerformanceStatistics(&stat);

			float fTimeMS = 1000.0f * gEnv->pTimer->TicksToSeconds(stat.m_nNetworkSync);
			DrawTextRightAligned(fTextPosX, fTextPosY += (fTextStepY - STEP_SMALL_DIFF),
			                     DISPLAY_INFO_SCALE_SMALL, Col_White, "%3.1f ms     NetworkSync", fTimeMS);
		}
	}

	#if defined(FEATURE_SVO_GI)
	CSvoManager::OnDisplayInfo(fTextPosX, fTextPosY, fTextStepY, DISPLAY_INFO_SCALE);
	#endif

	#undef MAX_PHYS_TIME
	#undef TICKS_TO_MS
	#undef CONVY
	#undef CONVX
	//////////////////////////////////////////////////////////////////////////
	// Display Current fps
	//////////////////////////////////////////////////////////////////////////

	if (iBlendMode)
	{
		// Track FPS frequency, report min/max.
		Blend(m_fAverageFPS, fFPS, fBlendCur);

		Blend(m_fMinFPSDecay, fFPS, fBlendCur);
		if (fFPS <= m_fMinFPSDecay)
			m_fMinFPS = m_fMinFPSDecay = fFPS;

		Blend(m_fMaxFPSDecay, fFPS, fBlendCur);
		if (fFPS >= m_fMaxFPSDecay)
			m_fMaxFPS = m_fMaxFPSDecay = fFPS;

		const char* sMode = "";
		switch (iBlendMode)
		{
		case 1:
			sMode = "frame avg";
			break;
		case 2:
			sMode = "time avg";
			break;
		case 3:
			sMode = "peak hold";
			break;
		}
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 1.5f, ColorF(1.0f, 1.0f, 0.5f, 1.0f),
		                     "FPS %.1f [%.0f..%.0f], %s over %.1f s",
		                     m_fAverageFPS, m_fMinFPS, m_fMaxFPS, sMode, fBlendTime);
	}
	else
	{
		const int nHistorySize = 16;
		static float arrfFrameRateHistory[nHistorySize] = { 0 };

		static int nFrameId = 0;
		nFrameId++;
		int nSlotId = nFrameId % nHistorySize;
		assert(nSlotId >= 0 && nSlotId < nHistorySize);
		arrfFrameRateHistory[nSlotId] = min(9999.f, GetTimer()->GetFrameRate());

		float fMinFPS = 9999.0f;
		float fMaxFPS = 0;
		for (int i = 0; i < nHistorySize; i++)
		{
			if (arrfFrameRateHistory[i] < fMinFPS)
				fMinFPS = arrfFrameRateHistory[i];
			if (arrfFrameRateHistory[i] > fMaxFPS)
				fMaxFPS = arrfFrameRateHistory[i];
		}

		float fFrameRate = 0;
		float fValidFrames = 0;
		for (int i = 0; i < nHistorySize; i++)
		{
			int s = (nFrameId - i) % nHistorySize;
			fFrameRate += arrfFrameRateHistory[s];
			fValidFrames++;
		}
		fFrameRate /= fValidFrames;

		m_fAverageFPS = fFrameRate;
		m_fMinFPS = m_fMinFPSDecay = fMinFPS;
		m_fMaxFPS = m_fMaxFPSDecay = fMaxFPS;

		//only difference to r_DisplayInfo 1, need ms for GPU time
		float fMax = (int(GetCurTimeSec() * 2) & 1) ? 999.f : 888.f;
		if (bEnhanced)
		{
			const RPProfilerStats* pFrameRPPStats = GetRenderer()->GetRPPStats(eRPPSTATS_OverallFrame);
			float gpuTime = pFrameRPPStats ? pFrameRPPStats->gpuTime : 0.0f;
			static float sGPUTime = 0.f;
			if (gpuTime < 1000.f && gpuTime > 0.01f) sGPUTime = gpuTime;//catch sporadic jumps
			if (sGPUTime > 0.01f)
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, DISPLAY_INFO_SCALE_SMALL, (gpuTime >= 40.f) ? Col_Red : Col_White, "%3.1f ms       GPU", sGPUTime);
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 1.4f, ColorF(1.0f, 1.0f, 0.2f, 1.0f), "FPS %5.1f (%3d..%3d)(%3.1f ms)",
			                     min(fMax, fFrameRate), (int)min(fMax, fMinFPS), (int)min(fMax, fMaxFPS), GetTimer()->GetFrameTime() * 1000.0f);
		}
		else
		{
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, 1.4f, ColorF(1.0f, 1.0f, 0.2f, 1.0f), "FPS %5.1f (%3d..%3d)",
			                     min(fMax, fFrameRate), (int)min(fMax, fMinFPS), (int)min(fMax, fMaxFPS));
		}
	}

	if (GetCVars()->e_ParticlesDebug & 1)
	{
		Vec2 location = Vec2(fTextPosX, fTextPosY += fTextStepY);
		m_pPartManager->DisplayStats(location, fTextStepY);
		fTextPosY = location.y;
	}

	if (GetCVars()->e_ParticlesDebug & AlphaBit('m'))
	{
		const stl::SPoolMemoryUsage memParticles = ParticleObjectAllocator().GetTotalMemory();

		ICrySizer* pSizerRE = GetSystem()->CreateSizer();
		gEnv->pRenderer->GetMemoryUsageParticleREs(pSizerRE);

		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "Particle Heap/KB: used %4d, freed %4d, unused %4d; Render: %4d",
		                     (int)memParticles.nUsed >> 10, (int)memParticles.nPoolFree() >> 10, (int)memParticles.nNonPoolFree() >> 10,
		                     (int)pSizerRE->GetTotalSize() >> 10);
		pSizerRE->Release();
	}

	m_pPartManager->RenderDebugInfo();

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		#pragma warning( pop )              //AMD Port
	#endif
	#ifndef _RELEASE
	if (GetCVars()->e_GsmStats)
	{
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "--------------- GSM Stats ---------------");

		if (ShadowMapInfo* pSMI = m_pSun->GetShadowMapInfo())
		{
			int arrGSMCastersCount[MAX_GSM_LODS_NUM];
			memset(arrGSMCastersCount, 0, sizeof(arrGSMCastersCount));
			char szText[256] = "Objects count per shadow map: ";
			for (int nLod = 0; nLod < Get3DEngine()->GetShadowsCascadeCount(NULL) && nLod < MAX_GSM_LODS_NUM; nLod++)
			{
				ShadowMapFrustumPtr& pLsource = pSMI->pGSM[nLod];

				if (nLod)
					cry_strcat(&szText[strlen(szText)], sizeof(szText) - strlen(szText), ", ");

				cry_sprintf(&szText[strlen(szText)], sizeof(szText) - strlen(szText), "%d", pLsource->GetCasterNum());
			}

			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s", szText);
		}

		for (int nSunInUse = 0; nSunInUse < 2; nSunInUse++)
		{
			if (nSunInUse)
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "WithSun  ListId   FrNum UserNum");
			else
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "NoSun    ListId   FrNum UserNum");

			// TODO: For Nick, check if needed anymore
			//for(ShadowFrustumListsCache::iterator it = m_FrustumsCache[nSunInUse].begin(); it != m_FrustumsCache[nSunInUse].end(); ++it)
			//{
			//  int nListId = (int)it->first;
			//  PodArray<ShadowMapFrustum*> * pList = it->second;

			//  DrawTextRightAligned( fTextPosX, fTextPosY+=fTextStepY,
			//    "%8d %8d %8d",
			//    nListId,
			//    pList->Count(), m_FrustumsCacheUsers[nSunInUse][nListId]);
			//}
		}
	}

	// objects counter
	if (GetCVars()->e_ObjStats)
	{
		#define DRAW_OBJ_STATS(_var) DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s: %d", ( # _var), GetInstCount(_var))

		DRAW_OBJ_STATS(eERType_NotRenderNode);
		DRAW_OBJ_STATS(eERType_Brush);
		DRAW_OBJ_STATS(eERType_Vegetation);
		DRAW_OBJ_STATS(eERType_Light);
		DRAW_OBJ_STATS(eERType_FogVolume);
		DRAW_OBJ_STATS(eERType_Decal);
		DRAW_OBJ_STATS(eERType_ParticleEmitter);
		DRAW_OBJ_STATS(eERType_WaterVolume);
		DRAW_OBJ_STATS(eERType_WaterWave);
		DRAW_OBJ_STATS(eERType_Road);
		DRAW_OBJ_STATS(eERType_DistanceCloud);
		DRAW_OBJ_STATS(eERType_Rope);
		DRAW_OBJ_STATS(eERType_TerrainSector);
		DRAW_OBJ_STATS(eERType_MovableBrush);
		DRAW_OBJ_STATS(eERType_GameEffect);
		DRAW_OBJ_STATS(eERType_BreakableGlass);
		DRAW_OBJ_STATS(eERType_CloudBlocker);
		DRAW_OBJ_STATS(eERType_MergedMesh);
		DRAW_OBJ_STATS(eERType_GeomCache);
		DRAW_OBJ_STATS(eERType_Character);

		CVisibleRenderNodesManager::Statistics visNodesStats = m_visibleNodesManager.GetStatistics();
		{
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "--- By list type (indoor): ---");
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "  Main:      %d", Get3DEngine()->GetVisAreaManager()->GetObjectsCount(eMain));
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "Caster:      %d", Get3DEngine()->GetVisAreaManager()->GetObjectsCount(eCasters));
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "Sprite:      %d", Get3DEngine()->GetVisAreaManager()->GetObjectsCount(eSprites));
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "Lights:      %d", Get3DEngine()->GetVisAreaManager()->GetObjectsCount(eLights));
		}
		int nFree = visNodesStats.numFree;
		int nUsed = visNodesStats.numUsed;
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "RNTmpData(Used+Free): %d + %d = %d (%d KB)",
		                     nUsed, nFree, nUsed + nFree, (nUsed + nFree) * (int)sizeof(SRenderNodeTempData) / 1024);

		nUsed = visNodesStats.numUsed;
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "RNTmpData(Used+Free): %d + %d = %d (%d KB)",
		                     nUsed, nFree, nUsed + nFree, (nUsed + nFree) * (int)sizeof(SRenderNodeTempData) / 1024);

		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "COctreeNode::m_arrEmptyNodes = %d", COctreeNode::m_arrEmptyNodes.Count());

		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "COctreeNode::NodesCounterAll = %d", COctreeNode::m_nNodesCounterAll);
	}

		#if defined(INFO_FRAME_COUNTER)
	++frameCounter;
	DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "Frame #%d", frameCounter);
		#endif

	if (GetCVars()->e_TerrainTextureStreamingDebug && m_pTerrain)
	{
		int nCacheSize[2] = { 0, 0 };
		m_pTerrain->GetTextureCachesStatus(nCacheSize[0], nCacheSize[1]);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "Terrain texture streaming status: waiting=%d, all=%d, pool=(%d+%d)",
		                     m_pTerrain->GetNotReadyTextureNodesCount(), m_pTerrain->GetActiveTextureNodesCount(),
		                     nCacheSize[0], nCacheSize[1]);
	}

	if (GetCVars()->e_TerrainBBoxes && m_pTerrain)
	{
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "GetDistanceToSectorWithWater() = %.2f", m_pTerrain->GetDistanceToSectorWithWater());
	}

	if (GetCVars()->e_ProcVegetation == 2)
	{
		CProcVegetPoolMan& pool = *CTerrainNode::GetProcObjPoolMan();
		int nAll;
		int nUsed = pool.GetUsedInstancesCount(nAll);

		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "Procedural sectors pool status: used=%d, all=%d, active=%d",
		                     nUsed, nAll, GetTerrain()->GetActiveProcObjNodesCount());
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
		for (int i = 0; i < min(16, pool.m_lstUsed.Count()); i++)
		{
			CProcObjSector* pSubPool = pool.m_lstUsed[i];
			nUsed = pSubPool->GetUsedInstancesCount(nAll);
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
			                     "Used sector: used=%d, all=%dx%d", nUsed, nAll, (int)GetCVars()->e_ProcVegetationMaxObjectsInChunk);
		}
		for (int i = 0; i < min(16, pool.m_lstFree.Count()); i++)
		{
			CProcObjSector* pSubPool = pool.m_lstFree[i];
			nUsed = pSubPool->GetUsedInstancesCount(nAll);
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
			                     "Free sector: used=%d, all=%dx%d", nUsed, nAll, (int)GetCVars()->e_ProcVegetationMaxObjectsInChunk);
		}
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
		{
			SProcObjChunkPool& chunks = *CTerrainNode::GetProcObjChunkPool();
			nUsed = chunks.GetUsedInstancesCount(nAll);
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
			                     "chunks pool status: used=%d, all=%d, %d MB", nUsed, nAll,
			                     nAll * int(GetCVars()->e_ProcVegetationMaxObjectsInChunk) * (int)sizeof(CVegetation) / 1024 / 1024);
		}
	}
	if (GetCVars()->e_MergedMeshesDebug)
	{
		if (m_pMergedMeshesManager->PoolOverFlow())
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
			                     "!!! merged mesh spine pool overflow!!! %3.3f kb (limit %3.3f kb)"
			                     , (m_pMergedMeshesManager->SpineSize() / 1024.f)
			                     , GetCVars()->e_MergedMeshesPool * GetCVars()->e_MergedMeshesPoolSpines / 100.f);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "%d active nodes, %d streamedout, number of instances : %d (visible %d)",
		                     (int)m_pMergedMeshesManager->ActiveNodes(),
		                     (int)m_pMergedMeshesManager->StreamedOutNodes(),
		                     (int)m_pMergedMeshesManager->InstanceCount(),
		                     (int)m_pMergedMeshesManager->VisibleInstances());
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "total main memory size : %3.3f kb (instances %3.3f kb, spines %3.3f kb, geom %3.3f kb)",
		                     (m_pMergedMeshesManager->CurrentSizeInMainMem() + m_pMergedMeshesManager->GeomSizeInMainMem()) / 1024.f
		                     , (m_pMergedMeshesManager->InstanceSize() / 1024.f)
		                     , (m_pMergedMeshesManager->SpineSize() / 1024.f)
		                     , m_pMergedMeshesManager->GeomSizeInMainMem() / 1024.f);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "dynamic merged rendermeshes vram size : %3.3f kb", (int)m_pMergedMeshesManager->CurrentSizeInVramDynamic() / 1024.f);
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY,
		                     "static merged rendermeshes vram size : %3.3f kb", (int)m_pMergedMeshesManager->CurrentSizeInVramInstanced() / 1024.f);
	}

	ITimeOfDay* pTimeOfDay = Get3DEngine()->GetTimeOfDay();
	if (GetCVars()->e_TimeOfDayDebug && pTimeOfDay)
	{
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "------------ Time of Day  -------------");
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, " ");

		int nVarCount = pTimeOfDay->GetVariableCount();
		for (int v = 0; v < nVarCount; ++v)
		{
			ITimeOfDay::SVariableInfo pVar;
			pTimeOfDay->GetVariableInfo(v, pVar);

			if (pVar.type == ITimeOfDay::TYPE_FLOAT)
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, " %s: %.9f", pVar.displayName, pVar.fValue[0]);
			else
				DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, " %s: %.3f %.3f %.3f", pVar.displayName, pVar.fValue[0], pVar.fValue[1], pVar.fValue[2]);
		}
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
	}

	if (GetCVars()->e_DynamicLights == 2)
	{
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "----------- Forward lights ------------");
		for (int i = 0; i < GetDynamicLightSources()->Count(); i++)
		{
			SRenderLight* pL = GetDynamicLightSources()->GetAt(i);
			DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "%s - %d)", pL->m_sName, pL->m_Id);
		}
		DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, "---------------------------------------");
	}

	#endif
	// We only show memory usage in dev mode.
	if (gEnv->pSystem->IsDevMode())
	{
		if (GetCVars()->e_DisplayMemoryUsageIcon)
		{
			uint64 nAverageMemoryUsage(0);
			uint64 nHighMemoryUsage(0);
			const uint64 nMegabyte(1024 * 1024);

			// Copied from D3DDriver.cpp, function CD3D9Renderer::RT_EndFrame().
			int nIconSize = 16;

	#if CRY_PLATFORM_64BIT
			nAverageMemoryUsage = 3000;
			nHighMemoryUsage = 6000;
	#else
			nAverageMemoryUsage = 800;
			nHighMemoryUsage = 1200;
	#endif

			// This is the same value as measured in the editor.
			uint64 nCurrentMemoryUsage = processMemInfo.PagefileUsage / nMegabyte;

			ITexture* pRenderTexture(m_ptexIconAverageMemoryUsage);
			if (nCurrentMemoryUsage > nHighMemoryUsage)
			{
				pRenderTexture = m_ptexIconHighMemoryUsage;
			}
			else if (nCurrentMemoryUsage < nAverageMemoryUsage)
			{
				pRenderTexture = m_ptexIconLowMemoryUsage;
			}

			if (pRenderTexture && gEnv->pRenderer)
			{
				// TODO: relative/normalized coordinate system in screen-space
				float vpWidth = (float)gEnv->pRenderer->GetOverlayWidth(), vpHeight = (float)gEnv->pRenderer->GetOverlayHeight();
				float iconWidth = (float)nIconSize /* / vpWidth * 800.0f */;
				float iconHeight = (float)nIconSize /* / vpHeight * 600.0f */;
				IRenderAuxImage::Draw2dImage(
				  fTextPosX /* / vpWidth * 800.0f*/ - iconWidth,
				  (fTextPosY += nIconSize + 3) /* / vpHeight * 600.0f*/,
				  iconWidth, iconHeight,
				  pRenderTexture->GetTextureID(), 0, 1.0f, 1.0f, 0);
			}
		}
	}

	pAux->SetRenderFlags(flags);
#endif

	if (ICharacterManager* pCharManager = gEnv->pCharacterManager)
		pCharManager->ResetFrameTicks();

	if (IAISystem* pAISystem = gEnv->pAISystem)
		pAISystem->ResetFrameTicks();
}

void C3DEngine::DrawFarTrees(const SRenderingPassInfo& passInfo)
{
	m_pObjManager->DrawFarObjects(GetMaxViewDistance(), passInfo);
}

void C3DEngine::GenerateFarTrees(const SRenderingPassInfo& passInfo)
{
	m_pObjManager->GenerateFarObjects(GetMaxViewDistance(), passInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void C3DEngine::ScreenShotHighRes(CStitchedImage* pStitchedImage, const int nRenderFlags, const SRenderingPassInfo& passInfo, uint32 SliceCount, f32 fTransitionSize)
{
#if CRY_PLATFORM_WINDOWS

	const uint32 prevScreenWidth = GetRenderer()->GetOverlayWidth();
	const uint32 prevScreenHeight = GetRenderer()->GetOverlayHeight();

	// finish frame started by system
	GetRenderer()->EndFrame();
	GetConsole()->SetScrollMax(0);

	GetRenderer()->EnableSwapBuffers(false);

	GetRenderer()->ResizeContext(passInfo.GetDisplayContextKey(), pStitchedImage->GetWidth(), pStitchedImage->GetHeight());

	GetRenderer()->BeginFrame(passInfo.GetDisplayContextKey());

	SRenderingPassInfo screenShotPassInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(passInfo.GetCamera());
	UpdateRenderingCamera("ScreenShotHighRes", screenShotPassInfo);
	RenderInternal(nRenderFlags, screenShotPassInfo, "ScreenShotHighRes");

	GetRenderer()->EndFrame();

	// Check output format and adjust acordingly
	const char* szExtension = GetCVars()->e_ScreenShotFileFormat->GetString();
	EReadTextureFormat dstFormat = (stricmp(szExtension, "tga") == 0) ? EReadTextureFormat::BGR8 : EReadTextureFormat::RGB8;
	
	GetRenderer()->ReadFrameBuffer((uint32*)pStitchedImage->GetBuffer(), pStitchedImage->GetWidth(), pStitchedImage->GetHeight(), false, dstFormat);
	GetRenderer()->ResizeContext(passInfo.GetDisplayContextKey(), prevScreenWidth, prevScreenHeight);

	GetRenderer()->EnableSwapBuffers(true);

	// Re-start frame so system can safely finish it
	GetRenderer()->BeginFrame(passInfo.GetDisplayContextKey());

	if(!m_bEditor)
	{
		// Making sure we don't run into trouble with the culling thread in pure-game mode
		m_pObjManager->PrepareCullbufferAsync(passInfo.GetCamera());
	}

	// restore initial state
	GetConsole()->SetScrollMax(300);
#endif // #if CRY_PLATFORM_WINDOWS
}

bool C3DEngine::ScreenShotMap(CStitchedImage* pStitchedImage,
                              const int nRenderFlags,
                              const SRenderingPassInfo& passInfo,
                              const uint32 SliceCount,
                              const f32 fTransitionSize)
{
#if CRY_PLATFORM_WINDOWS

	const f32 fTLX = GetCVars()->e_ScreenShotMapCenterX - GetCVars()->e_ScreenShotMapSizeX;
	const f32 fTLY = GetCVars()->e_ScreenShotMapCenterY - GetCVars()->e_ScreenShotMapSizeY;
	const f32 fBRX = GetCVars()->e_ScreenShotMapCenterX + GetCVars()->e_ScreenShotMapSizeX;
	const f32 fBRY = GetCVars()->e_ScreenShotMapCenterY + GetCVars()->e_ScreenShotMapSizeY;
	const f32 Height = GetCVars()->e_ScreenShotMapCamHeight;
	const int Orient = 0;

	string SettingsFileName = GetLevelFilePath("ScreenshotMap.Settings");

	FILE* metaFile = gEnv->pCryPak->FOpen(SettingsFileName, "wt");
	if (metaFile)
	{
		char Data[1024 * 8];
		cry_sprintf(Data, "<Map CenterX=\"%f\" CenterY=\"%f\" SizeX=\"%f\" SizeY=\"%f\" Height=\"%f\"  Quality=\"%d\" Orientation=\"%d\" />",
		            GetCVars()->e_ScreenShotMapCenterX,
		            GetCVars()->e_ScreenShotMapCenterY,
		            GetCVars()->e_ScreenShotMapSizeX,
		            GetCVars()->e_ScreenShotMapSizeY,
		            GetCVars()->e_ScreenShotMapCamHeight,
		            GetCVars()->e_ScreenShotQuality,
		            GetCVars()->e_ScreenShotMapOrientation);
		string data(Data);
		gEnv->pCryPak->FWrite(data.c_str(), data.size(), metaFile);
		gEnv->pCryPak->FClose(metaFile);
	}

	CCamera cam = passInfo.GetCamera();
	Matrix34 tmX, tmY;
	float xrot = -gf_PI * 0.5f;
	float yrot = Orient == 0 ? -gf_PI * 0.5f : -0.0f;
	tmX.SetRotationX(xrot);
	tmY.SetRotationY(yrot);
	Matrix34 tm = tmX * tmY;
	tm.SetTranslation(Vec3((fTLX + fBRX) * 0.5f, (fTLY + fBRY) * 0.5f, Height));
	cam.SetMatrix(tm);

	const f32 AngleX = atanf(((fBRX - fTLX) * 0.5f) / Height);
	const f32 AngleY = atanf(((fBRY - fTLY) * 0.5f) / Height);

	ICVar* r_drawnearfov = GetConsole()->GetCVar("r_DrawNearFoV");
	assert(r_drawnearfov);
	const f32 drawnearfov_backup = r_drawnearfov->GetFVal();
	if (max(AngleX, AngleY) <= 0)
		return false;
	cam.SetFrustum(pStitchedImage->GetWidth(), pStitchedImage->GetHeight(), max(0.001f, max(AngleX, AngleY) * 2.f), Height - 8000.f, Height + 1000.f);
	r_drawnearfov->Set(-1);
	ScreenShotHighRes(pStitchedImage, nRenderFlags, SRenderingPassInfo::CreateTempRenderingInfo(cam, passInfo), SliceCount, fTransitionSize);
	r_drawnearfov->Set(drawnearfov_backup);

	return true;
#else   // #if CRY_PLATFORM_WINDOWS
	return false;
#endif  // #if CRY_PLATFORM_WINDOWS
}

bool C3DEngine::ScreenShotPanorama(CStitchedImage* pStitchedImage, const int nRenderFlags, const SRenderingPassInfo& passInfo, uint32 SliceCount, f32 fTransitionSize)
{
#if CRY_PLATFORM_WINDOWS
	GetRenderer()->EndFrame();

	CRY_ASSERT_MESSAGE(0, "TODO: Fix omnidirectional camera");

	const uint32 prevScreenWidth = GetRenderer()->GetOverlayWidth();
	const uint32 prevScreenHeight = GetRenderer()->GetOverlayHeight();
	const uint32 ImageWidth = pStitchedImage->GetWidth();
	const uint32 ImageHeight = pStitchedImage->GetHeight();

	Matrix34 cubeFaceOrientation[6] = 
	{ 
		Matrix34::CreateRotationZ(DEG2RAD(-90)),
		Matrix34::CreateRotationZ(DEG2RAD(90)),
		Matrix34::CreateRotationX(DEG2RAD(90)),
		Matrix34::CreateRotationX(DEG2RAD(-90)),
		Matrix34::CreateIdentity(),
		Matrix34::CreateRotationZ(DEG2RAD(180)) 
	};

	CCamera newCamera = passInfo.GetCamera();
	newCamera.m_bOmniCamera = true;

	GetRenderer()->EnableSwapBuffers(false);

	GetRenderer()->ResizeContext(passInfo.GetDisplayContextKey(), ImageWidth, ImageHeight);

	for (int i = 0; i < 1; i++)
	{
		newCamera.m_curCubeFace = i;
		newCamera.SetMatrix(newCamera.GetMatrix() * cubeFaceOrientation[i]);
		newCamera.SetFrustum(ImageWidth, ImageHeight, DEG2RAD(90), newCamera.GetNearPlane(), newCamera.GetFarPlane(), 1.0f);

		// render scene
		GetRenderer()->BeginFrame(passInfo.GetDisplayContextKey());
		
		SRenderingPassInfo screenShotPassInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(newCamera);
		UpdateRenderingCamera("ScreenShotPanorama", screenShotPassInfo);
		RenderInternal(nRenderFlags, screenShotPassInfo, "ScreenShotPanorama");

		GetRenderer()->EndFrame();
	}
	
	GetRenderer()->ReadFrameBuffer((uint32*)pStitchedImage->GetBuffer(), ImageWidth, ImageHeight, false);
	GetRenderer()->ResizeContext(passInfo.GetDisplayContextKey(), prevScreenWidth, prevScreenHeight);

	GetRenderer()->EnableSwapBuffers(true);

	// Re-start frame so system can safely finish it
	GetRenderer()->BeginFrame(passInfo.GetDisplayContextKey());

	if (!m_bEditor)
	{
		// Making sure we don't run into trouble with the culling thread in pure-game mode
		m_pObjManager->PrepareCullbufferAsync(passInfo.GetCamera());
	}

	return true;
#else   // #if CRY_PLATFORM_WINDOWS
	return false;
#endif  // #if CRY_PLATFORM_WINDOWS
}

void C3DEngine::SetupClearColor(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	bool bCameraInOutdoors = m_pVisAreaManager && !m_pVisAreaManager->m_pCurArea && !(m_pVisAreaManager->m_pCurPortal && m_pVisAreaManager->m_pCurPortal->m_lstConnections.Count() > 1);

	passInfo.GetIRenderView()->SetTargetClearColor(bCameraInOutdoors ? m_vFogColor : Vec3(0, 0, 0), true);
	/*
	   if(bCameraInOutdoors)
	   if(GetCamera().GetPosition().z<GetWaterLevel() && m_pTerrain)
	   {
	   CTerrainNode * pSectorInfo = m_pTerrain ? m_pTerrain->GetSecInfo(GetCamera().GetPosition()) : 0;
	   if(!pSectorInfo || !pSectorInfo->m_pFogVolume || GetCamera().GetPosition().z>pSectorInfo->m_pFogVolume->box.max.z)
	   {
	   if(GetCamera().GetPosition().z<GetWaterLevel() && m_pTerrain &&
	   m_lstFogVolumes.Count() &&
	   m_lstFogVolumes[0].bOcean)
	   GetRenderer()->SetClearColor( m_lstFogVolumes[0].vColor );
	   }
	   //else if( pSectorInfo->m_pFogVolume->bOcean ) // makes problems if there is no skybox
	   //GetRenderer()->SetClearColor( pSectorInfo->m_pFogVolume->vColor );
	   }*/
}

void C3DEngine::FillDebugFPSInfo(SDebugFPSInfo& info)
{
	size_t c = 0;
	float average = 0.0f, min = 0.0f, max = 0.0f;
	const float clampFPS = 200.0f;
	for (size_t i = 0, end = arrFPSforSaveLevelStats.size(); i < end; ++i)
	{
		if (arrFPSforSaveLevelStats[i] > 1.0f && arrFPSforSaveLevelStats[i] < clampFPS)
		{
			++c;
			average += arrFPSforSaveLevelStats[i];
			//if (arrFPSforSaveLevelStats[i] < min)
			//	min = arrFPSforSaveLevelStats[i];
			//if (arrFPSforSaveLevelStats[i] > max)
			//	max = arrFPSforSaveLevelStats[i];
		}
	}

	if (c)
		average /= (float)c;

	int minc = 0, maxc = 0;
	for (size_t i = 0, end = arrFPSforSaveLevelStats.size(); i < end; ++i)
	{
		if (arrFPSforSaveLevelStats[i] > average && arrFPSforSaveLevelStats[i] < clampFPS)
		{
			++maxc;
			max += arrFPSforSaveLevelStats[i];
		}

		if (arrFPSforSaveLevelStats[i] < average && arrFPSforSaveLevelStats[i] < clampFPS)
		{
			++minc;
			min += arrFPSforSaveLevelStats[i];
		}
	}

	if (minc == 0)
		minc = 1;
	if (maxc == 0)
		maxc = 1;

	info.fAverageFPS = average;
	info.fMinFPS = min / (float)minc;
	info.fMaxFPS = max / (float)maxc;
}

void C3DEngine::PrepareShadowPasses(const SRenderingPassInfo& passInfo, uint32& nTimeSlicedShadowsUpdatedThisFrame, std::vector<std::pair<ShadowMapFrustum*, const CLightEntity*>>& shadowFrustums, std::vector<SRenderingPassInfo>& shadowPassInfo)
{
	// enable collection of all accessed frustums
	CLightEntity::SetShadowFrustumsCollector(&shadowFrustums);

	// collect dynamic sun frustums
	InitShadowFrustums(passInfo);

	// render outdoor point lights and collect dynamic point light frustums
	if (IsObjectsTreeValid())
		m_pObjectsTree->Render_LightSources(false, passInfo);

	// render indoor point lights and collect dynamic point light frustums
	for (int i = 0; i < m_pVisAreaManager->m_lstVisibleAreas.Count(); ++i)
	{
		CVisArea* pArea = m_pVisAreaManager->m_lstVisibleAreas[i];

		if (pArea->IsObjectsTreeValid())
		{
			for (int c = 0; c < pArea->m_lstCurCamerasLen; ++c)
				pArea->GetObjectsTree()->Render_LightSources(false, SRenderingPassInfo::CreateTempRenderingInfo(CVisArea::s_tmpCameras[pArea->m_lstCurCamerasIdx + c], passInfo));
		}
	}

	// disable collection of frustums
	CLightEntity::SetShadowFrustumsCollector(nullptr);

	// Prepare shadowpools
	GetRenderer()->PrepareShadowPool(passInfo.GetRenderView());

	// Limit frustums to kMaxShadowPassesNum
	if (shadowFrustums.size() > kMaxShadowPassesNum)
		shadowFrustums.resize(kMaxShadowPassesNum);
	m_onePassShadowFrustumsCount = shadowFrustums.size();

	shadowPassInfo.reserve(kMaxShadowPassesNum);
	CRenderView* pMainRenderView = passInfo.GetRenderView();
	for (const auto& pair : shadowFrustums)
	{
		auto* pFr = pair.first;
		auto* light = pair.second;

		assert(!pFr->pOnePassShadowView);

		// Prepare time-sliced shadow frustum updates
		GetRenderer()->PrepareShadowFrustumForShadowPool(pFr,
		                                                 static_cast<uint32>(passInfo.GetFrameID()),
		                                                 light->GetLightProperties(),
		                                                 &nTimeSlicedShadowsUpdatedThisFrame);

		IRenderViewPtr pShadowsView = GetRenderer()->GetNextAvailableShadowsView((IRenderView*)pMainRenderView, pFr);
		for (int cubeSide = 0; cubeSide < pFr->GetNumSides() && shadowPassInfo.size() < kMaxShadowPassesNum; ++cubeSide)
		{
			if (pFr->ShouldCacheSideHint(cubeSide))
			{
				pFr->MarkShadowGenMaskForSide(cubeSide);
				continue;
			}

			// create a matching rendering pass info for shadows
			auto pass = SRenderingPassInfo::CreateShadowPassRenderingInfo(
			  pShadowsView,
			  pFr->GetCamera(cubeSide),
			  pFr->m_Flags,
			  pFr->nShadowMapLod,
			  pFr->nShadowCacheLod,
			  pFr->IsCached(),
			  pFr->bIsMGPUCopy,
			  cubeSide,
			  SRenderingPassInfo::DEFAULT_SHADOWS_FLAGS);
			shadowPassInfo.push_back(std::move(pass));
		}

		pShadowsView->SetShadowFrustumOwner(pFr);
		pShadowsView->SwitchUsageMode(IRenderView::eUsageModeWriting);
		pFr->pOnePassShadowView = std::move(pShadowsView);
	}
}
