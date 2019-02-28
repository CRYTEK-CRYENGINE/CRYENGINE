// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImagePainter.h"
#include "Util/Image.h"
#include "Terrain/Heightmap.h"        // CHeightmap for mask computations
#include "Terrain/Layer.h"            // CLayer for mask computations
#include "Terrain/TerrainManager.h"   // CLayer for mask computations
#include "Terrain/SurfaceType.h"      // CLayer for mask computations

SEditorPaintBrush::SEditorPaintBrush(CHeightmap& rHeightmap, CLayer& rLayer,
                                     const bool bMaskByLayerSettings, const uint32 dwLayerIdMask, const bool bFlood)
	: bBlended(true), m_rHeightmap(rHeightmap), m_rLayer(rLayer), m_cFilterColor(1, 1, 1), m_dwLayerIdMask(dwLayerIdMask), m_bFlood(bFlood)
{
	if (bMaskByLayerSettings)
	{
		m_fMinAltitude = m_rLayer.GetLayerStart();
		m_fMaxAltitude = m_rLayer.GetLayerEnd();

		// HeightMap slope is defined as a ratio not as an angle. So we must convert user input in degrees to slope ratio
		m_fMinSlope = tan(DEG2RAD(m_rLayer.GetLayerMinSlopeAngle()));  // 0..90 -> 0..~1/0
		m_fMaxSlope = tan(DEG2RAD(m_rLayer.GetLayerMaxSlopeAngle()));  // 0..90 -> 0..~1/0
	}
	else
	{
		m_fMinAltitude = -FLT_MAX;
		m_fMaxAltitude = FLT_MAX;
		m_fMinSlope = 0;
		m_fMaxSlope = FLT_MAX;
	}
}

float SEditorPaintBrush::GetMask(const float fX, const float fY) const
{
	int iX = (int)(fX * m_rHeightmap.GetWidth() + 0.5f), iY = (int)(fY * m_rHeightmap.GetHeight() + 0.5f);

	float fAltitude = m_rHeightmap.GetZInterpolated(fX * m_rHeightmap.GetWidth(), fY * m_rHeightmap.GetHeight());

	// Check if altitude is within brush min/max altitude
	if (fAltitude < m_fMinAltitude || fAltitude > m_fMaxAltitude)
		return 0;

	float fSlope = m_rHeightmap.GetAccurateSlope(fX * m_rHeightmap.GetWidth(), fY * m_rHeightmap.GetHeight());

	// Check if slope is within brush min/max slope
	if (fSlope < m_fMinSlope || fSlope > m_fMaxSlope)
		return 0;

	//	Soft slope test
	//	float fSlopeAplha = 1.f;
	//	fSlopeAplha *= CLAMP((m_fMaxSlope-fSlope)*4 + 0.25f,0,1);
	//	fSlopeAplha *= CLAMP((fSlope-m_fMinSlope)*4 + 0.25f,0,1);

	if (m_dwLayerIdMask != 0xffffffff)
	{
		SSurfaceTypeItem& st = m_rHeightmap.GetLayerInfoAt(iX, iY);

		if (!st.HasType(m_dwLayerIdMask))
		{
			return 0;
		}
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
SSurfaceTypeItem CImagePainter::LerpTerrainSurfaceType(const SSurfaceTypeItem& s0, const SSurfaceTypeItem& s1, float t)
{
	byte arrUnroll[CLayer::e_hole];
	memset(arrUnroll, 0, sizeof(arrUnroll));

	int s_min = CLayer::e_hole - 1;
	int s_max = 0;

	for (int c = 0; c < SSurfaceTypeItem::kMaxSurfaceTypesNum; c++)
	{
		const byte ty = s0.ty[c];
		arrUnroll[ty] += (byte)((1.f - t) * (float)s0.we[c]);
		if (arrUnroll[ty])
		{
			s_min = min(s_min, (int)ty);
			s_max = max(s_max, (int)ty);
		}
	}

	for (int c = 0; c < SSurfaceTypeItem::kMaxSurfaceTypesNum; c++)
	{
		const byte ty = s1.ty[c];
		arrUnroll[ty] += (byte)((t) * (float)s1.we[c]);
		if (arrUnroll[ty])
		{
			s_min = min(s_min, (int)ty);
			s_max = max(s_max, (int)ty);
		}
	}

	SSurfaceTypeItem out;

	for (int c = 0; c < SSurfaceTypeItem::kMaxSurfaceTypesNum; c++)
	{
		out.we[c] = 0;

		for (int s = s_min; s <= s_max; s++)
		{
			if (arrUnroll[s] > out.we[c])
			{
				out.we[c] = arrUnroll[s];
				out.ty[c] = s;
			}
		}

		arrUnroll[out.ty[c]] = 0;
	}

	// normalize
	int summ = 0;
	for (int c = 0; c < SSurfaceTypeItem::kMaxSurfaceTypesNum; c++)
		summ += out.we[c];

	for (int c = 1; c < SSurfaceTypeItem::kMaxSurfaceTypesNum; c++)
		out.we[c] = (byte)SATURATEB(int(out.we[c]) * (255 / summ));

	out.we[0] = SATURATEB(255 - out.we[1] - out.we[2] - out.we[3]);

	return out;
}

//////////////////////////////////////////////////////////////////////////
void CImagePainter::PaintBrush(const float fpx, const float fpy, CSurfTypeImage& image, const SEditorPaintBrush& brush)
{
	float fX = fpx * image.GetWidth(), fY = fpy * image.GetHeight();

	const float fScaleX = 1.0f / image.GetWidth();
	const float fScaleY = 1.0f / image.GetHeight();

	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	float fMaxDist, fAttenuation, fYSquared;
	float fHardness = brush.hardness;

	unsigned int pos;

	SSurfaceTypeItem* src = (SSurfaceTypeItem*)image.GetData();

	int value = brush.color;

	// Calculate the maximum distance
	fMaxDist = brush.fRadius * image.GetWidth();

	assert(image.GetWidth() == image.GetHeight());

	int width = image.GetWidth();
	int height = image.GetHeight();

	int iMinX = (int)floor(fX - fMaxDist), iMinY = (int)floor(fY - fMaxDist);
	int iMaxX = (int)ceil(fX + fMaxDist), iMaxY = (int)ceil(fY + fMaxDist);

	for (int iPosY = iMinY; iPosY <= iMaxY; iPosY++)
	{
		// Skip invalid locations
		if (iPosY < 0 || iPosY > height - 1)
			continue;

		float fy = (float)iPosY - fY;

		// Precalculate
		fYSquared = (float)(fy * fy);

		for (int iPosX = iMinX; iPosX <= iMaxX; iPosX++)
		{
			float fx = (float)iPosX - fX;

			// Skip invalid locations
			if (iPosX < 0 || iPosX > width - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + fx * fx);
			if (!brush.m_bFlood && dist > fMaxDist)
				continue;

			float fMask = brush.GetMask(iPosX * fScaleX, iPosY * fScaleY);

			if (fMask < 0.5f)
				continue;

			// Calculate the array index
			pos = iPosX + iPosY * width;

			bool hole = src[pos].GetHole();

			if (brush.bBlended && !brush.m_bFlood)
			{
				// Calculate attenuation factor

				fAttenuation = 1.0f - __min(1.0f, dist / fMaxDist);

				SSurfaceTypeItem newVal;
				newVal = value;

				SSurfaceTypeItem& curVal = src[pos];

				if (!curVal.we[0])
				{
					// fix we[0] if was not initialized yet
					curVal.we[0] = SATURATEB(15 - curVal.we[1] - curVal.we[2]);
				}

				curVal = LerpTerrainSurfaceType(curVal, newVal, fAttenuation * fHardness);
			}
			else
			{
				src[pos] = value;
			}

			src[pos].SetHole(hole);
		}
	}
}

void CImagePainter::PaintBrushWithPattern(const float fpx, const float fpy, CImageEx& outImage,
                                          const uint32 dwOffsetX, const uint32 dwOffsetY, const float fScaleX, const float fScaleY,
                                          const SEditorPaintBrush& brush, const CImageEx& imgPattern)
{
	float fX = fpx * fScaleX, fY = fpy * fScaleY;

	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	float fMaxDist, fAttenuation, fYSquared;
	float fHardness = brush.hardness;

	unsigned int pos;

	uint32* src = outImage.GetData();
	uint32* pat = imgPattern.GetData();

	int value = brush.color;

	// Calculate the maximum distance
	fMaxDist = brush.fRadius;

	int width = outImage.GetWidth();
	int height = outImage.GetHeight();

	int patwidth = imgPattern.GetWidth();
	int patheight = imgPattern.GetHeight();

	int iMinX = (int)floor(fX - fMaxDist), iMinY = (int)floor(fY - fMaxDist);
	int iMaxX = (int)ceil(fX + fMaxDist), iMaxY = (int)ceil(fY + fMaxDist);

	bool bSRGB = imgPattern.GetSRGB();

	ICVar* pTerrainAutoGenerateBaseTexture = gEnv->pSystem->GetIConsole()->GetCVar("e_TerrainAutoGenerateBaseTexture");
	bool bAutoGenerateBaseTexture = pTerrainAutoGenerateBaseTexture->GetIVal() != 0;

	ICVar* pTerrainAutoGenerateBaseTextureTiling = gEnv->pSystem->GetIConsole()->GetCVar("e_TerrainAutoGenerateBaseTextureTiling");
	float baseTextureTiling = pTerrainAutoGenerateBaseTextureTiling->GetFVal();

	for (int iPosY = iMinY; iPosY < iMaxY; iPosY++)
	{
		// Skip invalid locations
		if (iPosY - dwOffsetY < 0 || iPosY - dwOffsetY > height - 1)
			continue;

		float fy = (float)iPosY - fY;

		// Precalculate
		fYSquared = (float)(fy * fy);

		int32 iPatY = ((uint32)iPosY) % patheight;
		assert(iPatY >= 0 && iPatY < patheight);

		for (int iPosX = iMinX; iPosX < iMaxX; iPosX++)
		{
			float fx = (float)iPosX - fX;

			// Skip invalid locations
			if (iPosX - dwOffsetX < 0 || iPosX - dwOffsetX > width - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + fx * fx);

			if (!brush.m_bFlood && dist > fMaxDist)
				continue;

			// Calculate the array index
			pos = (iPosX - dwOffsetX) + (iPosY - dwOffsetY) * width;

			ColorF cOut;

			if (bAutoGenerateBaseTexture)
			{
				// generate base diffuse color from terrain detail materials (ignore all manual RGB layer painting)
				cOut = CalculateBaseColorFromSurfaceInfo((float)iPosX / fScaleX, (float)iPosY / fScaleX, brush.m_rHeightmap, baseTextureTiling);
			}
			else
			{
				// Calculate attenuation factor
				fAttenuation = brush.m_bFlood ? 1.0f : 1.0f - __min(1.0f, dist / fMaxDist);
				assert(fAttenuation >= 0.0f && fAttenuation <= 1.0f);

				float fMask = brush.GetMask(iPosX / fScaleX, iPosY / fScaleX);

				uint32 cDstPix = src[pos];

				int32 iPatX = ((uint32)iPosX) % patwidth;
				assert(iPatX >= 0 && iPatX < patwidth);

				uint32 cSrcPix = pat[iPatX + iPatY * patwidth];

				float s = fAttenuation * fHardness * fMask;
				assert(s >= 0.0f && s <= 1.0f);

				const float fRecip255 = 1.0f / 255.0f;

				// Convert Src to Linear Space (Src is pattern texture, can be in linear or gamma space)
				ColorF cSrc = ColorF((cSrcPix & 0x0ff), (cSrcPix & 0x0ff00) >> 8, (cSrcPix & 0x0ff0000) >> 16) * fRecip255;
				if (bSRGB)
					cSrc.srgb2rgb();

				ColorF cMtl = brush.m_cFilterColor;
				cMtl.srgb2rgb();

				cSrc *= cMtl;
				cSrc.clamp(0.0f, 1.0f);

				// Convert Dst to Linear Space ( Dst is always in gamma space )
				ColorF cDst = ColorF((cDstPix & 0x0ff0000) >> 16, (cDstPix & 0x0ff00) >> 8, (cDstPix & 0x0ff)) * fRecip255;
				cDst.srgb2rgb();

				// Linear space	blend
				cOut = cSrc * s + cDst * (1.0f - s);

				// Convert final result to gamma space and put back in [0..255] range
				cOut.rgb2srgb();
				cOut *= 255.0f;
			}

			src[pos] = (((uint32)cOut.r) << 16) | (((uint32)cOut.g) << 8) | ((uint32)cOut.b);
		}
	}
}

ColorF CImagePainter::ReadColorF(int x, int y, const ColorB* pImg, int imgSizeW, int imgSizeH)
{
	const ColorB& colB = pImg[x + y * imgSizeW];
	ColorF colF;
	colF.r = colB.r;
	colF.g = colB.g;
	colF.b = colB.b;
	colF.a = colB.a;
	return colF;
}

ColorF CImagePainter::ReadColorFBilinear(float fX, float fY, const ColorB* pImg, int imgSizeW, int imgSizeH)
{
	fX *= imgSizeW;
	fY *= imgSizeH;
	int x = (int)floor(fX);
	int y = (int)floor(fY);
	float rx = fX - x;
	float ry = fY - y;
	int maskW = imgSizeW - 1;
	int maskH = imgSizeH - 1;
	ColorF top = ReadColorF(maskW & (x), maskH & (y), pImg, imgSizeW, imgSizeH) * (1.f - rx) +
	             ReadColorF(maskW & (x + 1), maskH & (y), pImg, imgSizeW, imgSizeH) * rx;
	ColorF bot = ReadColorF(maskW & (x), maskH & (y + 1), pImg, imgSizeW, imgSizeH) * (1.f - rx) +
	             ReadColorF(maskW & (x + 1), maskH & (y + 1), pImg, imgSizeW, imgSizeH) * rx;
	return (top * (1.f - ry) + bot * ry);
}

ColorF CImagePainter::CalculateBaseColorFromSurfaceInfo(const float fX, const float fY, CHeightmap& rHeightmap, float baseTextureTiling)
{
	ColorF layerRgbFin = Col_Black;

	int iX = (int)(fX * rHeightmap.GetWidth() + 0.5f), iY = (int)(fY * rHeightmap.GetHeight() + 0.5f);

	SSurfaceTypeItem& st = rHeightmap.GetLayerInfoAt(iX, iY);

	const float terrainSize = rHeightmap.GetWidth() * rHeightmap.GetUnitSize();

	float weightSumm = 0;

	for (int typeSlot = 0; typeSlot < SSurfaceTypeItem::kMaxSurfaceTypesNum; typeSlot++)
	{
		uint32 dwLayerId = st.ty[typeSlot];
		float weight = (float)st.we[typeSlot];

		if (weight <= 0)
		{
			continue;
		}

		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->FindLayerByLayerId(dwLayerId);

		if (!pLayer)
		{
			continue;
		}

		CSurfaceType* pSurfaceType = pLayer->GetSurfaceType();

		if (!pSurfaceType)
		{
			continue;
		}

		string mtlName = pSurfaceType ? pSurfaceType->GetMaterial() : "";

		if (!mtlName.IsEmpty())
		{
			IMaterial* pMaterial = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->FindMaterial(mtlName);

			if (pMaterial)
			{
				SShaderItem& shItem = pMaterial->GetShaderItem();

				int* pLowResSystemCopyAtlasId = 0;

				if (shItem.m_pShaderResources)
				{
					SEfResTexture* pResTexture = shItem.m_pShaderResources->GetTexture(EFTT_DIFFUSE);

					if (pResTexture)
					{
						ITexture* pITex = pResTexture->m_Sampler.m_pITex;

						if (pITex)
						{
							uint16 textureWidth = 0, textureHeight = 0;

							const ColorB* pTexRgb = (ColorB*)pITex->GetLowResSystemCopy(textureWidth, textureHeight, &pLowResSystemCopyAtlasId);

							if (pTexRgb)
							{
								ColorF layerRgb = ReadColorFBilinear(fX * terrainSize * baseTextureTiling, fY * terrainSize * baseTextureTiling, pTexRgb, textureWidth, textureHeight);

								layerRgbFin += layerRgb * weight;
								weightSumm += weight;
							}
						}
					}
				}
			}
		}
	}

	if (weightSumm)
	{
		layerRgbFin /= weightSumm;
	}

	return layerRgbFin;
}

void CImagePainter::FillWithPattern(CImageEx& outImage, const uint32 dwOffsetX, const uint32 dwOffsetY,
                                    const CImageEx& imgPattern)
{
	unsigned int pos;

	uint32* src = outImage.GetData();
	uint32* pat = imgPattern.GetData();

	int width = outImage.GetWidth();
	int height = outImage.GetHeight();

	int patwidth = imgPattern.GetWidth();
	int patheight = imgPattern.GetHeight();

	if (patheight == 0 || patwidth == 0)
		return;

	for (int iPosY = 0; iPosY < height; iPosY++)
	{
		int32 iPatY = ((uint32)iPosY + dwOffsetY) % patheight;
		assert(iPatY >= 0 && iPatY < patheight);

		for (int iPosX = 0; iPosX < width; iPosX++)
		{
			// Calculate the array index
			pos = iPosX + iPosY * width;

			int32 iPatX = ((uint32)iPosX + dwOffsetX) % patwidth;
			assert(iPatX >= 0 && iPatX < patwidth);

			uint32 cSrc = pat[iPatX + iPatY * patwidth];

			float SrcR = (cSrc & 0x0ff);          // RGB flipped
			float SrcG = (cSrc & 0x0ff00) >> 8;
			float SrcB = (cSrc & 0x0ff0000) >> 16;

			uint32 r = SrcR, g = SrcG, b = SrcB;

			src[pos] = (r << 16) | (g << 8) | b;
		}
	}
}

