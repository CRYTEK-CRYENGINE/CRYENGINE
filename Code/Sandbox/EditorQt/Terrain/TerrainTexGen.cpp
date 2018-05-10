// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainTexGen.h"
#include "CryEditDoc.h"

#include "Terrain/TerrainManager.h"
#include "TerrainLighting.h"
#include "QT/Widgets/QWaitProgress.h"

CTerrainTexGen::CTerrainTexGen() : m_LayerTexGen(), m_LightGen()
{
}

bool CTerrainTexGen::GenerateSectorTexture(CPoint sector, const CRect& rect, int flags, CImageEx& surfaceTexture)
{
	bool bOk;

	bOk = m_LayerTexGen.GenerateSectorTexture(sector, rect, flags, surfaceTexture);

	if (bOk)
		bOk = m_LightGen.GenerateSectorTexture(sector, rect, flags, surfaceTexture);

	return bOk;
}

const CImageEx* CTerrainTexGen::GetOcclusionSurfaceTexture() const
{
	return m_LightGen.GetOcclusionSurfaceTexture();
}

void CTerrainTexGen::ReleaseOcclusionSurfaceTexture()
{
	m_LightGen.ReleaseOcclusionSurfaceTexture();
}

bool CTerrainTexGen::GenerateSurfaceTexture(int flags, CImageEx& surfaceTexture)
{
	int num = 0;        // progress

	if (!gEnv->p3DEngine->GetITerrain())
		return false;

	m_LightGen.Init(surfaceTexture.GetWidth(), true);
	m_LayerTexGen.Init(surfaceTexture.GetWidth());

	// Generate texture for all sectors.
	bool bProgress = surfaceTexture.GetWidth() >= 1024;

	if (!m_LightGen.UpdateWholeHeightmap())
		return false;

	//////////////////////////////////////////////////////////////////////////
	LightingSettings* ls = GetIEditorImpl()->GetDocument()->GetLighting();

	// Needed for faster shadow calculations and hemisphere sampling
	m_LightGen.CalcTerrainMaxZ();

	//////////////////////////////////////////////////////////////////////////
	CWaitProgress wait(_T("Generating Surface Texture"));

	if (bProgress)
		wait.Start();

	if (flags & ETTG_INVALIDATE_LAYERS)
	{
		// Only invalidate layres ones at start.
		m_LayerTexGen.InvalidateLayers();
		flags &= ~ETTG_INVALIDATE_LAYERS;
	}

	//	m_IndirectLightingGen.Generate(16);

	m_LightGen.ReleaseOcclusionSurfaceTexture();

	assert(!m_LightGen.m_OcclusionSurfaceTexture.IsValid());

	CImageEx sectorOccmapImage;

	if (ls->eAlgo == eDynamicSun && (flags & ETTG_BAKELIGHTING) == 0)
	{
		uint32 dwLightSurfaceSize = surfaceTexture.GetWidth() / OCCMAP_DOWNSCALE_FACTOR;
		uint32 dwLightSectorSize = m_LightGen.m_sectorResolution / OCCMAP_DOWNSCALE_FACTOR;

		if (!m_LightGen.m_OcclusionSurfaceTexture.Allocate(dwLightSurfaceSize, dwLightSurfaceSize))
		{
			m_LightGen.m_bNotValid = true;
			return false;
		}

		if (!sectorOccmapImage.Allocate(dwLightSectorSize, dwLightSectorSize))
		{
			ReleaseOcclusionSurfaceTexture();
			m_LightGen.m_bNotValid = true;
			return false;
		}

		// diffuse texture should not include lighting because in this mode this info is in the occlusionmap
		flags &= ~ETTG_LIGHTING;
	}

	CImageEx sectorDiffuseImage;

	if (!sectorDiffuseImage.Allocate(m_LightGen.m_sectorResolution, m_LightGen.m_sectorResolution))
	{
		ReleaseOcclusionSurfaceTexture();
		sectorOccmapImage.Release();
		m_LightGen.m_bNotValid = true;
		return false;
	}

	// Normal, not multithreaded surface generation code.
	for (int y = 0; y < m_LightGen.m_numSectors; y++)
	{
		for (int x = 0; x < m_LightGen.m_numSectors; x++)
		{
			if (bProgress)
			{
				if (!wait.Step(num * 100 / (m_LightGen.m_numSectors * m_LightGen.m_numSectors)))
					return false;   // maybe this should be handled better
				num++;
			}

			CPoint sector(x, y);

			{
				CRect sectorRect;

				m_LightGen.GetSectorRect(sector, sectorRect);

				{
					if ((flags & ETTG_NOTEXTURE) == 0)
					{
						float fInvSectorCnt = 1.0f / (float)m_LightGen.m_numSectors;
						float fMinX = x * fInvSectorCnt;
						float fMinY = y * fInvSectorCnt;

						CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();
						pRGBLayer->GetSubImageStretched(fMinX, fMinY, fMinX + fInvSectorCnt, fMinY + fInvSectorCnt, sectorDiffuseImage);

						/*
						   // M.M. to take a look at the result of the calculation
						   char str[80];

						   cry_sprintf(str,"c:\\temp\\out%d_%d.bmp",x,y);
						   CImageUtil::SaveImage( str, sectorDiffuseImage );
						 */
					}
					else sectorDiffuseImage.Fill(255);

					CRect rect(0, 0, sectorDiffuseImage.GetWidth(), sectorDiffuseImage.GetHeight());

					if (!m_LightGen.GenerateSectorTexture(sector, rect, flags, sectorDiffuseImage))
						return false;     // maybe this should be handled better
				}
				// TODO: Remove commented code later (after QA have tested)
				/*
				   else
				   {
				   CRect rect(0,0,sectorDiffuseImage.GetWidth(),sectorDiffuseImage.GetHeight());

				   if(!GenerateSectorTexture( sector,rect,flags,sectorDiffuseImage ))
				    return false;			// maybe this should be handled better
				   }
				   /**/
				surfaceTexture.SetSubImage(sectorRect.left, sectorRect.top, sectorDiffuseImage);
			}

			if (sectorOccmapImage.IsValid())
			{
				CRect sectorRect;

				m_LightGen.GetSectorRect(sector, sectorRect);

				sectorRect.left /= OCCMAP_DOWNSCALE_FACTOR;
				sectorRect.top /= OCCMAP_DOWNSCALE_FACTOR;
				sectorRect.right /= OCCMAP_DOWNSCALE_FACTOR;
				sectorRect.bottom /= OCCMAP_DOWNSCALE_FACTOR;

				CRect rect(0, 0, sectorOccmapImage.GetWidth(), sectorOccmapImage.GetHeight());

				if (!GenerateSectorTexture(sector, rect, flags | ETTG_NOTEXTURE | ETTG_LIGHTING, sectorOccmapImage))
					return false;     // maybe this should be handled better

				m_LightGen.m_OcclusionSurfaceTexture.SetSubImage(sectorRect.left, sectorRect.top, sectorOccmapImage);
			}
		}
	}

	// M.M. to take a look at the result of the calculation
	//	CImageUtil::SaveImage( "c:\\temp\\out.bmp", surfaceTexture );

	if (bProgress)
		wait.Stop();

	return true;
}

CByteImage* CTerrainTexGen::GetLayerMask(CLayer* layer)
{
	return m_LayerTexGen.GetLayerMask(layer);
}

void CTerrainTexGen::InvalidateLayers()
{
	return m_LayerTexGen.InvalidateLayers();
}

void CTerrainTexGen::Init(const int resolution, const bool bFullInit)
{
	m_LightGen.Init(resolution, bFullInit);
	m_LayerTexGen.Init(resolution);
}

void CTerrainTexGen::InvalidateLighting()
{
	return m_LightGen.InvalidateLighting();
}

