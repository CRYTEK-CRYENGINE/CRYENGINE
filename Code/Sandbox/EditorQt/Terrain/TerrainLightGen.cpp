// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainTexGen.h"                  // temporary needed
#include "TerrainLightGen.h"
#include "CryEditDoc.h"
#include "TerrainLighting.h"
#include "TerrainGrid.h"
#include "Layer.h"
#include "Vegetation/VegetationMap.h"
#include "Sky Accessibility/HeightmapAccessibility.h"

#include "IndirectLighting/TerrainGIGen.h"

#include "Terrain/TerrainManager.h"

// Sector flags.
enum
{
	eSectorHeightmapValid = 0x01,
	eSectorLightmapValid  = 0x02,
};

#define MAX_BRIGHTNESS 100

CTerrainLightGen::CTerrainLightGen
(
  const int cApplySS,
  CPakFile* pLevelPakFile,
  const bool cUpdateIndirLighting
)
	: m_pLevelPakFile(pLevelPakFile)
{
	m_resolution = 0;
	m_bLog = true;
	m_iCachedSkyAccessiblityQuality = 0;
	//	m_iCachedSkyBrightening=0;
	m_vCachedSunDirection = Vec3(0, 0, 0);
	//	m_iCachedSunBlurLevel=0;
	m_bNotValid = false;

	m_heightmap = GetIEditorImpl()->GetHeightmap();
	assert(m_heightmap);

	m_UpdateIndirLighting = cUpdateIndirLighting;
	m_IndirLightingDataRetrieved = false;
	m_ApplySS = cApplySS;
	//	m_TerrainGIClamp = cTerrainGIClamp;

	if (!m_pLevelPakFile)
		m_UpdateIndirLighting = false;

	m_pTerrainAccMap = NULL;
	m_TerrainAccMapRes = 0;
}

//////////////////////////////////////////////////////////////////////////
CTerrainLightGen::~CTerrainLightGen()
{
	delete[] m_pTerrainAccMap;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::Init(const int resolution, const bool bFullInit)
{
	int i;
	m_heightmap = GetIEditorImpl()->GetHeightmap();
	assert(m_heightmap);

	m_terrainMaxZ = 255.0f;

	SSectorInfo si;
	m_heightmap->GetSectorsInfo(si);

	m_resolution = resolution;
	m_numSectors = si.numSectors;

	if (m_numSectors > 0)
		m_sectorResolution = m_resolution / m_numSectors;

	m_pixelSizeInMeters = float(si.numSectors * si.sectorSize) / (float)m_resolution;

	//! Allocate heightmap big enough.
	if (m_hmap.GetWidth() != resolution)
	{
		if (bFullInit)
		{
			if (!m_hmap.Allocate(resolution, resolution))
				m_bNotValid = true;
		}

		// Invalidate all sectors.
		m_sectorGrid.resize(m_numSectors * m_numSectors);
		if (m_sectorGrid.size() > 0)
		{
			memset(&m_sectorGrid[0], 0, m_sectorGrid.size() * sizeof(m_sectorGrid[0]));
		}
	}

	// Resolution is always power of 2.
	m_resolutionShift = 1;
	for (i = 0; i < 32; i++)
	{
		if ((1 << i) == m_resolution)
		{
			m_resolutionShift = i;
			break;
		}
	}
}

float CTerrainLightGen::GetSunAmount(const float* inpHeightmapData, int iniX, int iniY,
                                     float infInvHeightScale, const Vec3& vSunShadowVector, const float infShadowBlur) const
{
	assert(inpHeightmapData);
	assert(iniX < m_resolution);
	assert(iniY < m_resolution);
	assert(m_resolution);

	float fForwardStepSize = 1.0f;                      // [0.1f .. [ the less size, the better quality but it will be slower

	//	fForwardStepSize *= (float)iniWidth/1024.0f;

	const int iFixPointBits = 8;                                // wide range .. more precision
	const int iFixPointBase = 1 << iFixPointBits;               //

	float fZInit = inpHeightmapData[iniX + (iniY << m_resolutionShift)];

	float fSlope = vSunShadowVector.z * infInvHeightScale * fForwardStepSize;

	float fSlopeTop = fSlope + infShadowBlur;
	float fSlopeBottom = fSlope - infShadowBlur;

	assert(fSlopeTop >= 0.0f);

	fZInit += 0.1f;     // Bias to avoid little bumps in the result

	int iDirX = (int)(vSunShadowVector.x * fForwardStepSize * iFixPointBase);
	int iDirY = (int)(vSunShadowVector.y * fForwardStepSize * iFixPointBase);

	float fLen = 0.0f;

	int iX = iniX * iFixPointBase + iFixPointBase / 2, iY = iniY * iFixPointBase + iFixPointBase / 2;

	float fZBottom = fZInit + 0.1f, fZTop = fZInit + 1.4f;
	float fArea = 1.0f;

	// inner loop
	for (; fZBottom < m_terrainMaxZ; )
	{
		assert(fZBottom <= fZTop);

		iX += iDirX;
		iY += iDirY;
		fZBottom += fSlopeBottom;
		fZTop += fSlopeTop;

		int iXBound = (iX >> iFixPointBits);                      // shift right iFixPointBits bits = /iFixPointBase
		int iYBound = (iY >> iFixPointBits);                      // shift right iFixPointBits bits = /iFixPointBase

		if (iXBound >= m_resolution) break;
		if (iYBound >= m_resolution) break;

		float fGround = inpHeightmapData[iXBound + (iYBound << m_resolutionShift)];

		if (fZTop < fGround)      // ground hit
			return(0.0f);           // full shadow

		if (fZBottom < fGround)      // ground hit
		{
			float fNewArea = (fZTop - fGround) / (fZTop - fZBottom);              // this is slow in the penumbra of the shadow (but this is a rare case)

			if (fNewArea < fArea) fArea = fNewArea;
			assert(fArea >= 0.0f);
			assert(fArea <= 1.0f);
		}
	}

	return(fArea);
}

float CTerrainLightGen::GetSkyAccessibilityFloat(const float fX, const float fY) const
{
	if (!m_SkyAccessiblity.GetData()) { return 1.0f; }

	assert(m_SkyAccessiblity.GetWidth() != 0);
	assert(m_SkyAccessiblity.GetHeight() != 0);

	return CImageUtil::GetBilinearFilteredAt(fX * (m_SkyAccessiblity.GetWidth() << 8), fY * (m_SkyAccessiblity.GetHeight() << 8), m_SkyAccessiblity) * (1.0f / 255.0f);
}

float CTerrainLightGen::GetSunAccessibilityFloat(const float fX, const float fY) const
{
	if (!m_SunAccessiblity.GetData())
	{ return 1.0f; }                              // RefreshAccessibility was not sucessful

	assert(m_SunAccessiblity.GetWidth() != 0);
	assert(m_SunAccessiblity.GetHeight() != 0);

	return CImageUtil::GetBilinearFilteredAt(fX * (m_SunAccessiblity.GetWidth() << 8), fY * (m_SunAccessiblity.GetHeight() << 8), m_SunAccessiblity) * (1.0f / 255.0f);
}

float CTerrainLightGen::GetSkyAccessibilityFast(const int iniX, const int iniY) const
{
	//	assert(m_resolution);

	if (!m_SkyAccessiblity.GetData()) { return 1.0f; }

	assert(m_SkyAccessiblity.GetWidth() != 0);
	assert(m_SkyAccessiblity.GetHeight() != 0);

	int invScaleX = 1, invScaleY = 1;

	if (m_resolution > m_SkyAccessiblity.GetWidth())
	{
		invScaleX = m_resolution / m_SkyAccessiblity.GetWidth();
		invScaleY = m_resolution / m_SkyAccessiblity.GetHeight();
	}
	int iXmul256 = 256 / invScaleX;
	int iYmul256 = 256 / invScaleY;

	return CImageUtil::GetBilinearFilteredAt(iniX * iXmul256, iniY * iYmul256, m_SkyAccessiblity) * (1.0f / 255.0f);
}

float CTerrainLightGen::GetSunAccessibilityFast(const int iniX, const int iniY) const
{
	assert(m_resolution);

	if (!m_SunAccessiblity.GetData())
	//	{ assert(0);return 1.0f; }															// RefreshAccessibility was not successful
	{ return 1.0f; }                              // RefreshAccessibility was not successful

	assert(m_SunAccessiblity.GetWidth() != 0);
	assert(m_SunAccessiblity.GetHeight() != 0);

	int invScaleX = m_resolution / m_SunAccessiblity.GetWidth();
	int invScaleY = m_resolution / m_SunAccessiblity.GetHeight();

	int iXmul256 = 256 / invScaleX;
	int iYmul256 = 256 / invScaleY;

	return CImageUtil::GetBilinearFilteredAt(iniX * iXmul256, iniY * iYmul256, m_SunAccessiblity) * (1.0f / 255.0f);
}

////////////////////////////////////////////////////////////////////////
// Generate the surface texture with the current layer and lighting
// configuration and write the result to surfaceTexture.
// Also give out the results of the terrain lighting if pLightingBit is not NULL.
////////////////////////////////////////////////////////////////////////
bool CTerrainLightGen::GenerateSectorTexture(CPoint sector, const CRect& rect, int flags, CImageEx& surfaceTexture)
{
	if (m_bNotValid)
		return false;

	// set flags.
	bool bUseLighting = flags & ETTG_LIGHTING;
	//	bool bShowWater = flags & ETTG_SHOW_WATER;
	bool bNoTexture = flags & ETTG_NOTEXTURE;
	bool bUseLightmap = flags & ETTG_USE_LIGHTMAPS;
	m_bLog = !(flags & ETTG_QUIET);

	CCryEditDoc* pDocument = GetIEditorImpl()->GetDocument();
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	int sectorFlags = GetCLightGenSectorFlag(sector);

	assert(pDocument);
	assert(pHeightmap);

	float waterLevel = pHeightmap->GetWaterLevel();

	// Update heightmap for that sector.
	UpdateSectorHeightmap(sector);

	if (bNoTexture)
	{
		// fill texture with white color if there is no texture present
		surfaceTexture.Fill(255);
	}
	else
	{
		// todo - no need for if
	}

	////////////////////////////////////////////////////////////////////////
	// Light the texture
	////////////////////////////////////////////////////////////////////////
	if (bUseLighting)
	{
		LightingSettings* ls = pDocument->GetLighting();

		if (bUseLightmap)
		{
			// todo: remove if
		}
		else
		{
			// If not lightmaps.
			// Pass base texture instead of lightmap (Higher precision).
			if (!GenerateLightmap(sector, ls, surfaceTexture, flags))
				return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::GetSectorRect(CPoint sector, CRect& rect)
{
	rect.left = sector.x * m_sectorResolution;
	rect.top = sector.y * m_sectorResolution;
	rect.right = rect.left + m_sectorResolution;
	rect.bottom = rect.top + m_sectorResolution;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::Log(const char* format, ...)
{
	if (!m_bLog)
		return;

	va_list args;
	char szBuffer[1024];

	va_start(args, format);
	cry_vsprintf(szBuffer, format, args);
	va_end(args);

	CLogFile::WriteLine(szBuffer);
}

//////////////////////////////////////////////////////////////////////////
// Lighting.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
inline Vec3 CalcVertexNormal(const int x, const int y, const float* pHeightmapData, const int resolution, const float fHeightScale)
{
	if (x < resolution - 1 && y < resolution - 1 && x > 0 && y > 0)
	{
		Vec3 v1, v2, vNormal;

		// faster and better quality
		v1 = Vec3(2, 0, (pHeightmapData[x + 1 + y * resolution] - pHeightmapData[x - 1 + y * resolution]) * fHeightScale);
		v2 = Vec3(0, 2, (pHeightmapData[x + (y + 1) * resolution] - pHeightmapData[x + (y - 1) * resolution]) * fHeightScale);

		vNormal = v1.Cross(v2);
		vNormal.Normalize();
		return vNormal;
	}
	else
		return(Vec3(0, 0, 1));
}

//////////////////////////////////////////////////////////////////////////
inline Vec3 CalcBilinearVertexNormal(const float fX, const float fY, const float* pHeightmapData, const int resolution, const float fHeightScale)
{
	int x = (int)fX;
	int y = (int)fY;

	Vec3 vNormal[4];

	vNormal[0] = CalcVertexNormal(x, y, pHeightmapData, resolution, fHeightScale);
	vNormal[1] = CalcVertexNormal(x + 1, y, pHeightmapData, resolution, fHeightScale);
	vNormal[2] = CalcVertexNormal(x, y + 1, pHeightmapData, resolution, fHeightScale);
	vNormal[3] = CalcVertexNormal(x + 1, y + 1, pHeightmapData, resolution, fHeightScale);

	float fLerpX = fX - (float)x, fLerpY = fY - (float)y;

	Vec3 vHoriz[2];

	vHoriz[0] = vNormal[0] * (1.0f - fLerpX) + vNormal[1] * fLerpX;
	vHoriz[1] = vNormal[2] * (1.0f - fLerpX) + vNormal[3] * fLerpX;

	Vec3 vRet = vHoriz[0] * (1.0f - fLerpY) + vHoriz[1] * fLerpY;

	vRet.Normalize();

	return vRet;
}

// r,g,b in range 0-1.
inline float ColorLuminosity(float r, float g, float b)
{
	float mx, mn;
	mx = MAX(r, g);
	mx = MAX(mx, b);
	mn = MIN(r, g);
	mn = MIN(mn, b);
	return (mx + mn) / 2.0f;
}

// r,g,b in range 0-255.
inline uint32 ColorLuminosityInt(uint32 r, uint32 g, uint32 b)
{
	uint32 mx, mn;
	mx = MAX(r, g);
	mx = MAX(mx, b);
	mn = MIN(r, g);
	mn = MIN(mn, b);
	return (mx + mn) >> 1;
}

float CTerrainLightGen::CalcHeightScaleForLighting(const LightingSettings* pSettings, const DWORD indwTargetResolution) const
{
	assert(m_heightmap);

	float fRet = (float)indwTargetResolution * m_heightmap->CalcHeightScale();

	return fRet;
}

static void RGB2BGR(float p[3])
{
	float h = p[0];
	p[0] = p[2];
	p[2] = h;
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainLightGen::GenerateLightmap(CPoint sector, LightingSettings* pSettings, CImageEx& lightmap, int genFlags)
{
	assert(m_resolution);

	////////////////////////////////////////////////////////////////////////
	// Light the color values in a DWORD array with the generated lightmap.
	// Parameters are queried from the document. In case of the lighting
	// bit you are supposed to pass a NULL pointer if your don't want it.
	////////////////////////////////////////////////////////////////////////
	float fDP3;

	eLightAlgorithm lightAlgo = pSettings->eAlgo;

	float fWaterZ = -FLT_MAX;

	bool bUseFastLighting = genFlags & ETTG_FAST_LLIGHTING;
	bool bBakeLighting = genFlags & ETTG_BAKELIGHTING;
	bool bConvertToABGR = genFlags & ETTG_ABGR;

	if (bUseFastLighting)
		lightAlgo = eDP3;

	CRect rc;
	GetSectorRect(sector, rc);

	int iSurfaceStep = rc.Width() / lightmap.GetWidth();

	float fSunCol[3], fSkyCol[3];

	if (bBakeLighting || lightAlgo != eDynamicSun)
	{
		/*		Vec3 vSun = GetIEditorImpl()->Get3DEngine()->GetSunColor();
		    Vec3 vSky = GetIEditorImpl()->Get3DEngine()->GetSkyColor();

		    fSunCol[0]=vSun.x; fSunCol[1]=vSun.y; fSunCol[2]=vSun.z;
		    fSkyCol[0]=vSky.x; fSkyCol[1]=vSky.y; fSkyCol[2]=vSky.z;

		    float fMax = 1.0f;

		    fMax = max(fSunCol[0]+fSkyCol[0],fMax);
		    fMax = max(fSunCol[1]+fSkyCol[1],fMax);
		    fMax = max(fSunCol[2]+fSkyCol[2],fMax);

		    fSunCol[0]/=fMax; fSunCol[1]/=fMax; fSunCol[2]/=fMax;
		    fSkyCol[0]/=fMax; fSkyCol[1]/=fMax; fSkyCol[2]/=fMax;
		 */
		// preview colors (cannot exceed 0..1 range)
		fSunCol[0] = 0.0f;
		fSunCol[1] = 0.0f;
		fSunCol[2] = 0.0f;                                    // Sun is not needed
		fSkyCol[0] = 0.8f;
		fSkyCol[1] = 0.8f;
		fSkyCol[2] = 1.0f;
	}
	else
	{
		// setup for occlusionmaps
		fSunCol[0] = 1;
		fSunCol[1] = 0;
		fSunCol[2] = 0;                           // blue channel for the sun
		fSkyCol[0] = 0;
		fSkyCol[1] = 1;
		fSkyCol[2] = 0;                           // green channel for the sky
	}

	if (bConvertToABGR)
	{
		RGB2BGR(fSunCol);
		RGB2BGR(fSkyCol);
	}

	assert(fSunCol[0] >= 0 && fSunCol[1] >= 0 && fSunCol[2] >= 0);
	assert(fSkyCol[0] >= 0 && fSkyCol[1] >= 0 && fSkyCol[2] >= 0);

	float fHeighmapSizeInMeters = m_heightmap->GetWidth() * m_heightmap->GetUnitSize();

	// Calculate a height scalation factor. This is needed to convert the
	// relative nature of the height values. The contrast value is used to
	// raise the slope of the triangles, whoch results in higher contrast
	// lighting

	float fHeightScale = CalcHeightScaleForLighting(pSettings, m_resolution);
	float fInvHeightScale = 1.0f / fHeightScale;

	if (genFlags & ETTG_SHOW_WATER)
		fWaterZ = m_heightmap->GetWaterLevel();

	uint32 iWidth = m_resolution;
	uint32 iHeight = m_resolution;

	//////////////////////////////////////////////////////////////////////////
	// Prepare constants.
	//////////////////////////////////////////////////////////////////////////
	// Calculate the light vector
	Vec3 sunVector = pSettings->GetSunVector();

	//////////////////////////////////////////////////////////////////////////
	// Get heightmap data for this sector.
	//////////////////////////////////////////////////////////////////////////
	UpdateSectorHeightmap(sector);
	float* pHeightmapData = m_hmap.GetData();

	//////////////////////////////////////////////////////////////////////////
	// Generate shadowmap.
	//////////////////////////////////////////////////////////////////////////
	CByteImage shadowmap;
	float shadowAmmount = 255.0f * pSettings->iShadowIntensity / 100.0f;
	//	float fShadowIntensity = (float)pSettings->iShadowIntensity/100.0f;
	float fShadowBlur = pSettings->iShadowBlur * 0.04f;       // defines the slope blurring, 0=no blurring, .. (angle would be better but is slower)

	//	bool bStatObjShadows = genFlags & ETTG_STATOBJ_SHADOWS;
	bool bPaintBrightness = (genFlags & ETTG_STATOBJ_PAINTBRIGHTNESS) && (GetIEditorImpl()->GetVegetationMap() != 0);
	bool bTerrainShadows = pSettings->bTerrainShadows && (!(genFlags & ETTG_NO_TERRAIN_SHADOWS));

	//////////////////////////////////////////////////////////////////////////
	// generate accessiblity for this sector.
	//////////////////////////////////////////////////////////////////////////
	if (!bUseFastLighting)
	{
		if (!RefreshAccessibility(pSettings, genFlags | ETTG_PREVIEW))
		{
			CryLog("RefreshAccessibility Failed.");
			return false;
		}
	}

	/*
	   //	if(!pSettings->bObjectShadows)		// no shadows for objects
	    bStatObjShadows=false;

	   if(bStatObjShadows)
	   {
	    if (!shadowmap.Allocate(lightmap.GetWidth(),lightmap.GetHeight()))
	    {
	      m_bNotValid = true;
	      return false;
	    }

	    if(pSettings->eAlgo==ePrecise || pSettings->eAlgo==eDynamicSun)
	      GenerateShadowmap( sector,shadowmap,255,sunVector );										// shadow is full shadow (realistic)
	    else
	      GenerateShadowmap( sector,shadowmap,shadowAmmount,sunVector );					// shadow percentage (more flexible?)
	   }
	 */

	////////////////////////////////////////////////////////////////////////
	// Perform lighting
	////////////////////////////////////////////////////////////////////////

	//	float fAmbient255 = pSettings->iAmbient;
	float fAmbient255 = 0.0f;     // will be removed soon

	float fAmbient = fAmbient255 / 255.0f;

	int brightness, brightness_shadowmap;

	uint32* pLightmap = lightmap.GetData();

	Vec3 lightVector = -sunVector;

	Vec3 vSunShadowVector;
	{
		float invR = 1.0f / (sqrtf(lightVector.x * lightVector.x + lightVector.y * lightVector.y) + 0.001f);
		vSunShadowVector = lightVector * invR;
	}

	float fSkyLuminosity = ColorLuminosity(fSkyCol[0], fSkyCol[1], fSkyCol[2]);
	float fBrightness, fBrightnessShadowmap;
	float fBrighter = 0.f;

	uint32 dwWidth = lightmap.GetWidth();
	uint32 dwHeight = lightmap.GetHeight();

	//	if(lightAlgo==ePrecise || lightAlgo==eDynamicSun)
	{
		assert(m_heightmap->GetWidth() == m_heightmap->GetHeight());
		bool bHaveSkyColor = (fSkyCol[0] != 0) || (fSkyCol[1] != 0) || (fSkyCol[2] != 0);
		float fSunBrightness = 0;

		////////////////////////////////////////////////////////////////////////
		// Precise lighting
		////////////////////////////////////////////////////////////////////////

		//calc shift for resolution conversions
		int resShiftL = 0;
		if (m_resolution < m_SkyAccessiblity.GetWidth())
		{
			while ((m_resolution << resShiftL) != m_SkyAccessiblity.GetWidth())
				++resShiftL;
			assert((m_resolution << resShiftL) == m_SkyAccessiblity.GetWidth());
		}

		CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();

		for (int y = 0; y < dwHeight; y++)
		{
			int j = rc.top + y * iSurfaceStep;

			for (int x = 0; x < dwWidth; x++)
			{
				int i = rc.left + x * iSurfaceStep;

				float fr = 0.0f, fg = 0.0f, fb = 0.0f;  // 0..1..

				// Calc vertex normal and Make the dot product
				Vec3 vNormal = CalcVertexNormal(i, j, pHeightmapData, m_resolution, fHeightScale);

				//assert(vNormal.z>=0.0f);
				fDP3 = lightVector.Dot(vNormal);

				// Calculate the intensity (basic lambert, this is incorrect for fuzzy materials like grass, more smooth around 0 would be better)
				float fLight = max(fDP3, 0.0f);        // 0..1

				fBrightness = min(1.0f, (fDP3 + 1.0f) + fAmbient);

				float fSunVisibility = 1;
				// in shadow
				if (bTerrainShadows)
				{
					fSunVisibility = GetSunAccessibilityFast(i, j);

					fBrightness = fAmbient * (1.0f - fSunVisibility) + fSunVisibility * fBrightness;
				}
				fLight *= fSunVisibility;

				fBrightnessShadowmap = fBrightness;

				float fSkyWeight = 0;

				if (pSettings->iHemiSamplQuality)
					fSkyWeight = GetSkyAccessibilityFast(i << resShiftL, j << resShiftL);    // 0..1
				//					fSkyWeight=GetSkyAccessibilityFloat(i/(float)m_resolution,j/(float)m_resolution);		// 0..1
				else
					fSkyWeight = vNormal.z * 0.6f + 0.4f; // approximate the sky accessibility without hills, only based only on surface slope

				assert(fSkyWeight >= 0.0f && fSkyWeight <= 1.0f);

				//! Multiply light color with lightmap and sun color.
				fr += fSkyWeight * fSkyCol[0];  // 0..1..
				fg += fSkyWeight * fSkyCol[1];  // 0..1..
				fb += fSkyWeight * fSkyCol[2];  // 0..1..

				fr += fLight * fSunCol[0];  // 0..1..
				fg += fLight * fSunCol[1];  // 0..1..
				fb += fLight * fSunCol[2];  // 0..1..

				// Get Current LM pixel.
				uint32* pLMPixel = &pLightmap[x + y * dwWidth];     // uint32 RGB

				// Clamp color to 255.
				uint32 lr = min(int(fr * GetRValue(*pLMPixel)), 255); // 0..255
				uint32 lg = min(int(fg * GetGValue(*pLMPixel)), 255); // 0..255
				uint32 lb = min(int(fb * GetBValue(*pLMPixel)), 255); // 0..255

				float fZ = pHeightmapData[i + j * m_resolution];

				if (fZ < fWaterZ)
				{
					lb /= 2;
					lg /= 2;        // tint blue
				}

				// store it
				*pLMPixel = RGB(lr, lg, lb);

				// brightness for vegetation
				if (bPaintBrightness)
				{
					brightness = min(MAX_BRIGHTNESS, int(MAX_BRIGHTNESS * fBrightness));
					brightness_shadowmap = min(MAX_BRIGHTNESS, int(MAX_BRIGHTNESS * fBrightnessShadowmap));

					// swap X/Y
					float worldPixelX = i * m_pixelSizeInMeters;
					float worldPixelY = j * m_pixelSizeInMeters;
					pVegetationMap->PaintBrightness(worldPixelY, worldPixelX, m_pixelSizeInMeters, m_pixelSizeInMeters, brightness, brightness_shadowmap);
				}
			}
		}
	}

	SetSectorFlags(sector, eSectorLightmapValid);
	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Generate shadows from static objects and place them in shadow map bitarray.
void CTerrainLightGen::GenerateShadowmap(CPoint sector, CByteImage& shadowmap, float shadowAmmount, const Vec3& sunVector)
{
	//	if(!m_pTerrain)
	//	return;
	SSectorInfo si;
	GetIEditorImpl()->GetHeightmap()->GetSectorsInfo(si);

	int width = shadowmap.GetWidth();
	int height = shadowmap.GetHeight();

	int numSectors = si.numSectors;

	int sectorTexSize = shadowmap.GetWidth();
	int sectorTexSize2 = sectorTexSize * 2;
	assert(sectorTexSize > 0);

	uint32 shadowValue = shadowAmmount;

	CMemoryBlock mem;
	if (!mem.Allocate(sectorTexSize2 * sectorTexSize2 * 3))
	{
		m_bNotValid = true;
		return;
	}
	unsigned char* sectorImage2 = (unsigned char*)mem.GetBuffer();

	Vec3 wp = GetIEditorImpl()->GetHeightmap()->GetTerrainGrid()->SectorToWorld(sector);
	//	GetIEditorImpl()->Get3DEngine()->MakeLightMap( wp.x+0.1f,wp.y+0.1f,sectorImage2,sectorTexSize2 );
	//GetIEditorImpl()->Get3DEngine()->MakeTerrainLightMap( wp.x,wp.y,si.sectorSize,sectorImage2,sectorTexSize2 );
	memset(sectorImage2, 255, sectorTexSize2 * sectorTexSize2 * 3);

	// Scale sector image down and store into the shadow map.
	int pos;
	uint32 color;
	for (int j = 0; j < sectorTexSize; j++)
	{
		int sx1 = sectorTexSize - j - 1;
		for (int i = 0; i < sectorTexSize; i++)
		{
			pos = (i + j * sectorTexSize2) * 2 * 3;
			color = (shadowValue *
			         ((uint32)
			          (255 - sectorImage2[pos]) +
			          (255 - sectorImage2[pos + 3]) +
			          (255 - sectorImage2[pos + sectorTexSize2 * 3]) +
			          (255 - sectorImage2[pos + sectorTexSize2 * 3 + 3])
			         )) >> 10;
			//						color = color*shadowValue >> 8;
			// swap x/y
			//color = (255-sectorImage2[(i+j*sectorTexSize)*3]);
			shadowmap.ValueAt(sx1, i) = color;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::CalcTerrainMaxZ()
{
	// Caluclate max terrain height.
	m_terrainMaxZ = 0;
	int hmapSize = m_hmap.GetSize() / sizeof(float);
	float* pData = m_hmap.GetData();
	for (int i = 0; i < hmapSize; i++)
	{
		if (pData[i] > m_terrainMaxZ)
			m_terrainMaxZ = pData[i];
	}
}

//////////////////////////////////////////////////////////////////////////
const CImageEx* CTerrainLightGen::GetOcclusionSurfaceTexture() const
{
	if (m_OcclusionSurfaceTexture.IsValid())
		return &m_OcclusionSurfaceTexture;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::ReleaseOcclusionSurfaceTexture()
{
	m_OcclusionSurfaceTexture.Release();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::UpdateSectorHeightmap(CPoint sector)
{
	int sectorFlags = GetSectorFlags(sector);
	if (!(sectorFlags & eSectorHeightmapValid))
	{
		CRect rect;
		GetSectorRect(sector, rect);

		// Increase rectangle by one pixel..
		rect.InflateRect(2, 2, 2, 2);

		m_heightmap->GetData(rect, m_hmap.GetWidth(), CPoint(0, 0), m_hmap, true, true);
		SetSectorFlags(sector, eSectorHeightmapValid);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainLightGen::UpdateWholeHeightmap()
{
	if (m_numSectors)
	{
		assert(m_sectorGrid.size());
		if (!m_sectorGrid.size())
			return false;
	}

	bool bAllValid = true;
	for (int i = 0; i < m_numSectors * m_numSectors; i++)
	{
		if (!(m_sectorGrid[i] & eSectorHeightmapValid))
		{
			// Mark all heighmap sector flags as valid.
			bAllValid = false;
			m_sectorGrid[i] |= eSectorHeightmapValid;
		}
	}

	if (!bAllValid)
	{
		CRect rect(0, 0, m_hmap.GetWidth(), m_hmap.GetHeight());
		if (!m_heightmap->GetData(rect, m_hmap.GetWidth(), CPoint(0, 0), m_hmap, true, true))
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
int CTerrainLightGen::GetSectorFlags(CPoint sector)
{
	assert(sector.x >= 0 && sector.x < m_numSectors && sector.y >= 0 && sector.y < m_numSectors);
	int index = sector.x + sector.y * m_numSectors;
	return m_sectorGrid[index];
}

//////////////////////////////////////////////////////////////////////////
int& CTerrainLightGen::GetCLightGenSectorFlag(CPoint sector)
{
	assert(sector.x >= 0 && sector.x < m_numSectors && sector.y >= 0 && sector.y < m_numSectors);
	int index = sector.x + sector.y * m_numSectors;
	return m_sectorGrid[index];
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::SetSectorFlags(CPoint sector, int flags)
{
	assert(sector.x >= 0 && sector.x < m_numSectors && sector.y >= 0 && sector.y < m_numSectors);
	m_sectorGrid[sector.x + sector.y * m_numSectors] |= flags;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::InvalidateLighting()
{
	size_t nCount(0);
	size_t nTotal(m_sectorGrid.size());
	for (int i = 0; i < nTotal; i++)
	{
		m_sectorGrid[i] &= ~eSectorLightmapValid;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLightGen::GetSubImageStretched(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImageEx& rOutImage, const int genFlags)
{
	assert(fSrcLeft >= 0.0f && fSrcLeft <= 1.0f);
	assert(fSrcTop >= 0.0f && fSrcTop <= 1.0f);
	assert(fSrcRight >= 0.0f && fSrcRight <= 1.0f);
	assert(fSrcBottom >= 0.0f && fSrcBottom <= 1.0f);
	assert(fSrcRight >= fSrcLeft);
	assert(fSrcBottom >= fSrcTop);

	uint32 dwDestWidth = rOutImage.GetWidth();
	uint32 dwDestHeight = rOutImage.GetHeight();

	assert(dwDestWidth);
	assert(dwDestHeight);

	//subtract 1 pixel due to the filter mechanism
	float fScaleX = (fSrcRight - fSrcLeft) / (float)(dwDestWidth - 1);
	float fScaleY = (fSrcBottom - fSrcTop) / (float)(dwDestHeight - 1);

	CCryEditDoc* pDocument = GetIEditorImpl()->GetDocument();
	LightingSettings* pSettings = pDocument->GetLighting();
	bool bTerrainShadows = pSettings->bTerrainShadows && (!(genFlags & ETTG_NO_TERRAIN_SHADOWS));
	bool bPaintBrightness = (genFlags & ETTG_STATOBJ_PAINTBRIGHTNESS) && (GetIEditorImpl()->GetVegetationMap() != 0);

	if (!RefreshAccessibility(pSettings, genFlags & ~ETTG_PREVIEW))
	{
		assert(0);
		CryLog("RefreshAccessibility Failed.");
	}

	/*
	   // m.m. test
	   {
	    CImage a;

	    a.Allocate(m_SunAccessiblity.GetWidth(),m_SunAccessiblity.GetHeight());

	    for(int y=0;y<m_SunAccessiblity.GetHeight();++y)
	      for(int x=0;x<m_SunAccessiblity.GetWidth();++x)
	        a.ValueAt(x,y)=m_SunAccessiblity.ValueAt(x,y);

	    CImageUtil::SaveBitmap("C:\\temp\\sun.bmp",a);

	    a.Allocate(m_SkyAccessiblity.GetWidth(),m_SkyAccessiblity.GetHeight());

	    for(int y=0;y<m_SkyAccessiblity.GetHeight();++y)
	      for(int x=0;x<m_SkyAccessiblity.GetWidth();++x)
	        a.ValueAt(x,y)=m_SkyAccessiblity.ValueAt(x,y);

	    CImageUtil::SaveBitmap("C:\\temp\\sky.bmp",a);
	   }
	 */

	float* pHeightmapData = GetIEditorImpl()->GetHeightmap()->GetData();
	assert(m_resolution == GetIEditorImpl()->GetHeightmap()->GetWidth());

	float fHeightScale = CalcHeightScaleForLighting(pSettings, m_resolution);
	Vec3 lightVector = -pSettings->GetSunVector();

	const float cMaxResValue = 1.f - 0.5f / (float)m_resolution;//half a texel off texture border
	//bilinear texel offset
	const float cTexelXOffset = 0.5f / (float)dwDestWidth;
	const float cTexelYOffset = 0.5f / (float)dwDestHeight;
	for (uint32 dwDestY = 0; dwDestY < dwDestHeight; ++dwDestY)
	{
		float fSrcY = fSrcTop + ((float)dwDestY + cTexelYOffset) * fScaleY;
		fSrcY = min(fSrcY, cMaxResValue);

		for (uint32 dwDestX = 0; dwDestX < dwDestWidth; ++dwDestX)
		{
			float fSrcX = fSrcLeft + ((float)dwDestX + cTexelXOffset) * fScaleX;
			fSrcX = min(fSrcX, cMaxResValue);
			float fSkyWeight = GetSkyAccessibilityFloat(fSrcX, fSrcY); // 0..1
			uint32 lg = pos_directed_rounding(fSkyWeight * 255.0f);          // 0..255
			rOutImage.ValueAt(dwDestX, dwDestY) = RGB(255, lg, 0);
		}
	}
	/*
	   //	bool bStatObjShadows = (genFlags & ETTG_STATOBJ_SHADOWS)!=0 && pSettings->bObjectShadows;
	   bool bStatObjShadows = false;

	   if(bStatObjShadows)
	   {
	    int dwDestWidth2 = dwDestWidth*2;

	    CMemoryBlock mem;

	    if(!mem.Allocate( dwDestWidth2*dwDestWidth2*3 ))
	      return;

	    unsigned char *sectorImage2 = (unsigned char*)mem.GetBuffer();

	    float wx = fSrcLeft * (pDocument->GetHeightmap().GetWidth()*pDocument->GetHeightmap().GetUnitSize());
	    float wy = fSrcTop * (pDocument->GetHeightmap().GetHeight()*pDocument->GetHeightmap().GetUnitSize());
	    float wsize = (fSrcRight-fSrcLeft) * (pDocument->GetHeightmap().GetHeight()*pDocument->GetHeightmap().GetUnitSize());
	    // removed +0.1f,+0.1f from wx,wy
	   //		GetIEditorImpl()->Get3DEngine()->MakeTerrainLightMap( wx,wy,wsize,sectorImage2,dwDestWidth2 );
	    memset(sectorImage2, 255, dwDestWidth2*dwDestWidth2*3);

	    uint32 dwShadowAmount = (uint32)(256.0f*pSettings->iShadowIntensity/100.0f);		// 0..256

	    // Scale sector image down and store into the shadow map.
	    int pos;
	    uint32 color;
	    for(uint32 dwDestY=0;dwDestY<dwDestHeight;++dwDestY)
	    {
	      for(uint32 dwDestX=0;dwDestX<dwDestWidth;++dwDestX)
	      {
	   //				pos = (dwDestY + dwDestX*dwDestWidth2)*3;
	   //				color = (uint32) (sectorImage2[pos]);

	        pos = (dwDestY + dwDestX*dwDestWidth2)*2*3;

	        // calc average between 4 samples
	        color = ((uint32) (sectorImage2[pos]) +
	          (sectorImage2[pos+3]) +
	          (sectorImage2[pos+dwDestWidth2*3]) +
	          (sectorImage2[pos+dwDestWidth2*3+3])) >> 2;

	        color = 255-(((255-color)*dwShadowAmount)>>8);

	        uint32 dwABGR = rOutImage.ValueAt(dwDestX,dwDestY);

	        // store it
	        uint32 outcol = dwABGR;
	   //				uint32 outcol = RGB(GetRValue(dwABGR),GetGValue(dwABGR), (((uint32)color*(uint32)GetBValue(dwABGR)) >>8) );
	        // put shadow map in r channel.
	        outcol = (outcol&(~0xFF)) | ((unsigned char)(color));
	        rOutImage.ValueAt(dwDestX,dwDestY) = outcol;
	      }
	    }
	   }
	 */

	if (bPaintBrightness)
	{
		CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();

		CHeightmap& roHeightMap = *GetIEditorImpl()->GetHeightmap();

		float fTerrainWidth = roHeightMap.GetWidth() * roHeightMap.GetUnitSize();
		float fTerrainHeight = roHeightMap.GetWidth() * roHeightMap.GetUnitSize();

		//////////////////////////////////////////////////////////////////////////
		// Paint vegetation brighness.
		//////////////////////////////////////////////////////////////////////////

		for (uint32 dwDestY = 0; dwDestY < dwDestHeight; ++dwDestY)
		{
			float fSrcY = fSrcTop + dwDestY * fScaleY;

			for (uint32 dwDestX = 0; dwDestX < dwDestWidth; ++dwDestX)
			{
				float fSrcX = fSrcLeft + dwDestX * fScaleX;

				uint32 col = rOutImage.ValueAt(dwDestX, dwDestY);

				float fSunVisibility = (col >> 24) * (1.0f / 255.0f);
				float fSkyVisibility = GetGValue(col) * (1.0f / 255.0f);
				float fSunWeight;

				float fDP3;
				{
					// Calc vertex normal and Make the dot product
					Vec3 vNormal = CalcBilinearVertexNormal(fSrcX * m_resolution, fSrcY * m_resolution, pHeightmapData, m_resolution, fHeightScale);

					// Calculate the intensity (basic lambert, this is incorrect for fuzzy materials like grass, more smooth around 0 would be better)
					fDP3 = lightVector.Dot(vNormal);
					fSunWeight = fSunVisibility * max(fDP3, 0.0f);
				}

				//////////////////////////////////////////////////////////////////////////
				// Timur.
				/*				float fAmbient = 0.1f;
				        float fBrightness = min(1.0f,(fDP3+1.0f) + fAmbient );
				        fBrightness = fAmbient*(1.0f-fSunVisibility) + fSunVisibility*fBrightness;
				        uint32 la = pos_directed_rounding(fBrightness*255.0f);
				        if(la>255)
				          la=255;
				 */

				float fAmbient = 0.3f * fSkyVisibility;
				float fBrightness = min(1.0f, (fDP3 + 1.0f));
				fBrightness = fBrightness * fAmbient * (1.0f - fSunVisibility) + fSunVisibility * fBrightness;
				uint32 la = (int)(fBrightness * 255.0f);
				if (la > 255)
					la = 255;

				uint32 brightness = la;
				uint32 shadowmap = GetRValue(col);
				uint32 brightness_shadowmap = (shadowmap * brightness) >> 8;

				CHeightmap& roHeightMap = *GetIEditorImpl()->GetHeightmap();

				float wx = fSrcX * (roHeightMap.GetWidth() * roHeightMap.GetUnitSize());
				float wy = fSrcY * (roHeightMap.GetHeight() * roHeightMap.GetUnitSize());
				float wsize = fScaleX * (roHeightMap.GetHeight() * roHeightMap.GetUnitSize());

				// Swap X/Y
				//				pVegetationMap->PaintBrightness( wy,wx,wsize,wsize,GetRValue(col),GetRValue(col) );
				pVegetationMap->PaintBrightness(wy, wx, wsize, wsize, brightness, brightness_shadowmap);
			}
		}
	}

	// M.M. test
	//	CImageUtil::SaveBitmap("C:\\temp\\test.bmp",rOutImage);
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainLightGen::RefreshAccessibility(const LightingSettings* inpLSettings, int genFlags)
{
	assert(m_resolution);

	DWORD w = m_heightmap->GetWidth(), h = m_heightmap->GetHeight();   // // unscaled

	bool bTerrainShadows = inpLSettings->bTerrainShadows && (!(genFlags & ETTG_NO_TERRAIN_SHADOWS));
	// refresh sun?
	bool bUpdateSunAccessiblity = false;
	if (bTerrainShadows)
		if (!m_SunAccessiblity.GetData()
		    //			|| m_iCachedSunBlurLevel!=inpLSettings->iShadowBlur
		    || m_vCachedSunDirection != inpLSettings->GetSunVector())
		{
			//			m_iCachedSunBlurLevel=inpLSettings->iShadowBlur;
			m_vCachedSunDirection = inpLSettings->GetSunVector();

			bUpdateSunAccessiblity = true;
		}

	// refresh sky?
	//we refresh the sky if we have to refresh the indirect lighting, otherwise the data would overwrite the
	//	convoluted (sky acc + terrain occl.) data
	bool bUpdateSkyAccessiblity = false;
	if (genFlags & ETTG_PREVIEW)
	{
		if (!m_SkyAccessiblity.GetData() || m_iCachedSkyAccessiblityQuality != inpLSettings->iHemiSamplQuality)
		{
			if ((inpLSettings->eAlgo == ePrecise || inpLSettings->eAlgo == eDynamicSun) && !(genFlags & ETTG_FAST_LLIGHTING))
			{
				m_iCachedSkyAccessiblityQuality = inpLSettings->iHemiSamplQuality;
				//			m_iCachedSkyBrightening=inpLSettings->iSkyBrightening;

				if (inpLSettings->iHemiSamplQuality)
					bUpdateSkyAccessiblity = true;
			}
		}
	}
	else
	{
		bUpdateSkyAccessiblity = m_UpdateIndirLighting;//otherwise we would erase the terrain occlusion
		bTerrainShadows = false;
		bUpdateSunAccessiblity = false;
		if (!bUpdateSkyAccessiblity && !m_IndirLightingDataRetrieved)
		{
			//retrieve existing data, get here when just updating surface texture
			if (!m_SkyAccessiblity.Allocate(w, h))
			{
				m_bNotValid = true;
				return false;
			}
			unsigned char* dst = m_SkyAccessiblity.GetData();
			//			IIndirectLighting *pIndirLighting = 0;//gEnv->p3DEngine->GetIIndirectLighting();
			//			if(!pIndirLighting)
			memset(dst, 255, w * h);
			//			else
			//			pIndirLighting->RetrieveTerrainAcc(dst, w, h);

			m_IndirLightingDataRetrieved = true;
			return true;
		}
	}

	if (!m_UpdateIndirLighting && !bUpdateSkyAccessiblity && !bUpdateSunAccessiblity)
		return true;                                                // no update necessary because

	float* pHeightMap = m_heightmap->GetData();         // unscaled

	CFloatImage FloatHeightMap;

	// use minimum needed size (fast in preview)
	if (m_resolution < w || m_resolution < h)
	{
		w = m_resolution;
		h = m_resolution;

		if (!FloatHeightMap.Allocate(w, h))
		{
			m_bNotValid = true;
			return false;
		}

		CRect full(0, 0, m_heightmap->GetWidth(), m_heightmap->GetHeight());

		m_heightmap->GetData(full, FloatHeightMap.GetWidth(), CPoint(0, 0), FloatHeightMap, false, false);    // no smooth, no noise

		pHeightMap = FloatHeightMap.GetData();          // scaled
	}

	/*
	   // limit size to get performance (blurring seems to be more acceptable)
	   if(w>2048)
	    w=2048;
	   if(h>2048)
	    h=2048;

	   that caused problems because the CHeightmapAccessibility assume the heightmap and the reult has the same extend
	 */

	// scale height to fit with the scaling in x and y direction
	assert(w == h);
	float fHeightScale = CalcHeightScaleForLighting(inpLSettings, w);

	//generate indirect lighting

	if (m_UpdateIndirLighting)
	{
		assert(m_pLevelPakFile);
		CTerrainGIGen indirectLightingGen;      // to generate indirect lighting information
		indirectLightingGen.Generate(*m_pLevelPakFile, m_pTerrainAccMap, m_TerrainAccMapRes, m_ApplySS);
		m_UpdateIndirLighting = false;
	}

	bool applyAccMap = (m_pTerrainAccMap != NULL);

	// sky
	if (bUpdateSkyAccessiblity)
	{
		m_IndirLightingDataRetrieved = true;
		DWORD dwSkyAngleSteps = inpLSettings->iHemiSamplQuality * 3 + 1;            // 1 .. 31

		if (dwSkyAngleSteps < 4)
			dwSkyAngleSteps = 4;      // hack: less than 4 slices look bad - if inpLSettings->iHemiSamplQuality is 0 this code should not be called

		CHeightmapAccessibility<CHemisphereSink_Solid> calc(w, h, dwSkyAngleSteps);

		if (!calc.Calc(pHeightMap, fHeightScale))
			return(false);

		//allocate in width unit resolution
		//m_TerrainAccMapRes =  width unit size
		if (m_TerrainAccMapRes == 0)
			m_TerrainAccMapRes = m_heightmap->GetWidth() * m_heightmap->GetUnitSize();
		if (!m_SkyAccessiblity.Allocate(m_TerrainAccMapRes, m_TerrainAccMapRes))
		{
			m_bNotValid = true;
			return false;
		}

		if (w != h)
		{
			CLogFile::WriteLine("Width and height of heightmap do not match");
			applyAccMap = false;
		}

		uint32 calcUnitShift = 0;
		while ((w << calcUnitShift) != m_TerrainAccMapRes)
			++calcUnitShift;

		unsigned char* dst = m_SkyAccessiblity.GetData();
		// copy intermediate to result
		if (applyAccMap)
		{
			for (DWORD i = 0; i < m_TerrainAccMapRes; ++i)
				for (DWORD j = 0; j < m_TerrainAccMapRes; ++j)
				{
					dst[j * m_TerrainAccMapRes + i] =
					  min((uint8)(calc.GetSampleAt((i >> calcUnitShift), (j >> calcUnitShift)) * (255.0f / (256 * 256 - 1))),
					      m_pTerrainAccMap[j * m_TerrainAccMapRes + i]);
				}
		}
		else
		{
			for (DWORD i = 0; i < m_TerrainAccMapRes; i++)
				for (DWORD j = 0; j < m_TerrainAccMapRes; j++)
				{
					float fIn = calc.GetSampleAt((i >> calcUnitShift), (j >> calcUnitShift)) * (255.0f / (256 * 256 - 1)); // 0.0f..255.0f
					uint8 out = (uint8)((fIn + 0.5f));                                                                     // 0..0xff
					dst[j * m_TerrainAccMapRes + i] = out;
				}
		}
	}

	// sun
	if (bUpdateSunAccessiblity)
	{
		// quality depends on blur level
		DWORD dwSunAngleSteps = (inpLSettings->iShadowBlur + 5 * 2 - 1) / 5; // 1..
		float fBlurAngle = DEG2RAD(inpLSettings->iShadowBlur * 0.5f);        // 0.5 means 1 unit is 0.5 degrees
		float fHAngle = -DEG2RAD(inpLSettings->iSunRotation - 90.0f);
		float fMinHAngle = fHAngle - fBlurAngle * 0.5f, fMaxHAngle = fHAngle + fBlurAngle * 0.5f;
		float fVAngle = (gf_PI / 2) - asin(inpLSettings->iSunHeight * 0.01f);
		float fMinVAngle = max(0.0f, fVAngle - fBlurAngle * 0.5f), fMaxVAngle = min(gf_PI / 2, fVAngle + fBlurAngle * 0.5f);
		CHeightmapAccessibility<CHemisphereSink_Slice> calc(w, h, dwSunAngleSteps, fMinHAngle, fMaxHAngle);

		calc.m_Sink.SetMinAndMax(fMinVAngle, fMaxVAngle);

		if (!calc.Calc(pHeightMap, fHeightScale))
			return(false);

		// store result more compressed
		if (!m_SunAccessiblity.Allocate(w, h))
		{
			m_bNotValid = true;
			return false;
		}

		unsigned char* dst = m_SunAccessiblity.GetData();
		// copy intermediate to result
		for (DWORD i = 0; i < w; i++)
			for (DWORD j = 0; j < h; j++)
				dst[j * w + i] = (unsigned char) min(((calc.GetSampleAt(i, j) + 0x80) >> 8), 255);
	}

	return true;
}

