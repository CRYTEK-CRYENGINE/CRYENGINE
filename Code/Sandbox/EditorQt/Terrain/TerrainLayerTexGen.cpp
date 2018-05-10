// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainLayerTexGen.h"

#include "CryEditDoc.h"
#include "TerrainLighting.h"
#include "TerrainGrid.h"
#include "Layer.h"
#include "Sky Accessibility/HeightmapAccessibility.h"
#include "TerrainTexGen.h"

#include "Terrain/TerrainManager.h"

// Sector flags.
enum
{
	eSectorLayersValid = 0x04
};

//////////////////////////////////////////////////////////////////////////
CTerrainLayerTexGen::CTerrainLayerTexGen(const int resolution)
{
	m_bLog = true;
	m_waterLayer = NULL;

	Init(resolution);
}

CTerrainLayerTexGen::CTerrainLayerTexGen()
{
	m_bLog = true;
	m_waterLayer = NULL;

	SSectorInfo si;
	GetIEditorImpl()->GetHeightmap()->GetSectorsInfo(si);
	Init(si.surfaceTextureSize);
}

//////////////////////////////////////////////////////////////////////////
CTerrainLayerTexGen::~CTerrainLayerTexGen()
{
	// Release masks for all layers to save memory.
	int numLayers = GetLayerCount();

	for (int i = 0; i < numLayers; i++)
	{
		if (m_layers[i].m_pLayerMask)
			delete m_layers[i].m_pLayerMask;

		CLayer* pLayer = GetLayer(i);

		pLayer->ReleaseTempResources();
	}

	if (m_waterLayer)
		delete m_waterLayer;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLayerTexGen::Init(const int resolution)
{
	int i;

	m_resolution = resolution;

	// Fill layers array.
	ClearLayers();

	int numLayers = GetIEditorImpl()->GetTerrainManager()->GetLayerCount();
	m_layers.reserve(numLayers + 2);   // Leave some space for water layers.
	for (i = 0; i < numLayers; i++)
	{
		SLayerInfo li;
		li.m_pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		m_layers.push_back(li);
	}

	SSectorInfo si;
	GetIEditorImpl()->GetHeightmap()->GetSectorsInfo(si);

	m_numSectors = si.numSectors;
	if (m_numSectors > 0)
		m_sectorResolution = resolution / m_numSectors;

	// Invalidate all sectors.
	m_sectorGrid.resize(m_numSectors * m_numSectors);
	if (m_sectorGrid.size() > 0)
	{
		memset(&m_sectorGrid[0], 0, m_sectorGrid.size() * sizeof(m_sectorGrid[0]));
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainLayerTexGen::UpdateSectorLayers(CPoint sector)
{
	bool bFirstUsedLayer = true;

	CRect sectorRect;
	GetSectorRect(sector, sectorRect);

	CFloatImage hmap;                           // Local heightmap (scaled up to the target resolution)

	CRect recHMap;
	GetSectorRect(sector, recHMap);

	const int iBorder = 2;

	// Increase rectangle by .. pixel..
	recHMap.InflateRect(iBorder, iBorder, iBorder, iBorder);

	// Update heightmap for that sector.
	{
		int sectorFlags = GetSectorFlags(sector);

		// Allocate heightmap big enough.
		{
			if (!hmap.Allocate(recHMap.Width(), recHMap.Height()))
				return false;

			hmap.Clear();

			// Invalidate all sectors.
			m_sectorGrid.resize(m_numSectors * m_numSectors);
			memset(&m_sectorGrid[0], 0, m_sectorGrid.size() * sizeof(m_sectorGrid[0]));
		}

		GetIEditorImpl()->GetHeightmap()->GetData(recHMap, m_resolution, CPoint(recHMap.left, recHMap.top), hmap, true, true);
	}

	int numLayers = GetLayerCount();
	for (int i = 0; i < numLayers; i++)
	{
		CLayer* pLayer = GetLayer(i);

		// Skip the layer if it is not in use
		if (!pLayer->IsInUse())
			continue;

		// Skip the layer if it has no texture
		//		if(!pLayer->HasTexture())
		//			continue;

		// For first used layer mask is not needed.
		if (!bFirstUsedLayer)
		{
			if (!m_layers[i].m_pLayerMask)
			{
				m_layers[i].m_pLayerMask = new CByteImage;
			}

			if (pLayer->UpdateMaskForSector(sector, sectorRect, m_resolution, CPoint(recHMap.left, recHMap.top), hmap, *m_layers[i].m_pLayerMask))
			{
			}
		}
		bFirstUsedLayer = false;
	}

	// For this sector all layers are valid.
	SetSectorFlags(sector, eSectorLayersValid);

	return true;
}

////////////////////////////////////////////////////////////////////////
// Generate the surface texture with the current layer and lighting
// configuration and write the result to surfaceTexture.
// Also give out the results of the terrain lighting if pLightingBit is not NULL.
////////////////////////////////////////////////////////////////////////
bool CTerrainLayerTexGen::GenerateSectorTexture(CPoint sector, const CRect& rect, int flags, CImageEx& surfaceTexture)
{
	// set flags.
	bool bShowWater = flags & ETTG_SHOW_WATER;
	bool bConvertToABGR = flags & ETTG_ABGR;
	bool bNoTexture = flags & ETTG_NOTEXTURE;
	bool bUseLightmap = flags & ETTG_USE_LIGHTMAPS;
	m_bLog = !(flags & ETTG_QUIET);

	if (flags & ETTG_INVALIDATE_LAYERS)
	{
		InvalidateLayers();
	}

	uint32 i;

	CCryEditDoc* pDocument = GetIEditorImpl()->GetDocument();
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	CTexSectorInfo& texsectorInfo = GetCTexSectorInfo(sector);
	int sectorFlags = texsectorInfo.m_Flags;

	assert(pDocument);
	assert(pHeightmap);

	float waterLevel = pHeightmap->GetWaterLevel();

	if (bNoTexture)
	{
		// fill texture with white color if there is no texture present
		surfaceTexture.Fill(255);
	}
	else
	{
		// Enable texturing.
		//////////////////////////////////////////////////////////////////////////
		// Setup water layer.
		if (bShowWater && m_waterLayer == NULL)
		{
			// Apply water level.
			// Add a temporary water layer to the list
			SLayerInfo li;
			m_waterLayer = new CLayer;
			m_waterLayer->LoadTexture(MAKEINTRESOURCE(IDB_WATER), 128, 128);
			m_waterLayer->SetAutoGen(true);
			m_waterLayer->SetLayerStart(0);
			m_waterLayer->SetLayerEnd(waterLevel);

			li.m_pLayer = m_waterLayer;
			m_layers.push_back(li);
		}

		////////////////////////////////////////////////////////////////////////
		// Generate the layer masks
		////////////////////////////////////////////////////////////////////////

		// if lLayers not valid for this sector, update them.
		// Update layers for this sector.
		if (!(sectorFlags & eSectorLayersValid))
			if (!UpdateSectorLayers(sector))
				return false;

		bool bFirstLayer = true;

		// Calculate sector rectangle.
		CRect sectorRect;
		GetSectorRect(sector, sectorRect);

		// Calculate surface texture rectangle.
		CRect layerRc(sectorRect.left + rect.left, sectorRect.top + rect.top, sectorRect.left + rect.right, sectorRect.top + rect.bottom);

		CByteImage layerMask;

		////////////////////////////////////////////////////////////////////////
		// Generate the masks and the texture.
		////////////////////////////////////////////////////////////////////////
		int numLayers = GetLayerCount();
		for (i = 0; i < (int)numLayers; i++)
		{
			CLayer* pLayer = GetLayer(i);

			// Skip the layer if it is not in use
			if (!pLayer->IsInUse() || !pLayer->HasTexture())
				continue;

			// Set the write pointer (will be incremented) for the surface data
			unsigned int* pTex = surfaceTexture.GetData();

			uint32 layerWidth = pLayer->GetTextureWidth();
			uint32 layerHeight = pLayer->GetTextureHeight();

			if (bFirstLayer)
			{
				bFirstLayer = false;

				// Draw the first layer, without layer mask.
				for (int y = layerRc.top; y < layerRc.bottom; y++)
				{
					uint32 layerY = y & (layerHeight - 1);
					for (int x = layerRc.left; x < layerRc.right; x++)
					{
						uint32 layerX = x & (layerWidth - 1);

						COLORREF clr;

						// Get the color of the tiling texture at this position
						clr = pLayer->GetTexturePixel(layerX, layerY);

						if (bConvertToABGR)
							clr = ((clr & 0x00FF0000) >> 16) | (clr & 0x0000FF00) | ((clr & 0x000000FF) << 16);

						*pTex++ = clr;
					}
				}
			}
			else
			{
				if (!m_layers[i].m_pLayerMask || !m_layers[i].m_pLayerMask->IsValid())
					continue;

				layerMask.Attach(*m_layers[i].m_pLayerMask);

				uint32 iBlend;
				COLORREF clr;

				// Draw the current layer with layer mask.
				for (int y = layerRc.top; y < layerRc.bottom; y++)
				{
					uint32 layerY = y & (layerHeight - 1);
					for (int x = layerRc.left; x < layerRc.right; x++)
					{
						uint32 layerX = x & (layerWidth - 1);

						// Scale the current preview coordinate to the layer mask and get the value.
						iBlend = layerMask.ValueAt(x, y);
						// Check if this pixel should be drawn.
						if (iBlend == 0)
						{
							pTex++;
							continue;
						}

						// Get the color of the tiling texture at this position
						clr = pLayer->GetTexturePixel(layerX, layerY);

						if (bConvertToABGR)
							clr = ((clr & 0x00FF0000) >> 16) | (clr & 0x0000FF00) | ((clr & 0x000000FF) << 16);

						// Just overdraw when the opaqueness of the new layer is maximum
						if (iBlend == 255)
						{
							*pTex = clr;
						}
						else
						{
							// Blend the layer into the existing color, iBlend is the blend factor taken from the layer
							int iBlendSrc = 255 - iBlend;
							// WAT_EDIT
							*pTex = ((iBlendSrc * (*pTex & 0x000000FF) + (clr & 0x000000FF) * iBlend) >> 8) |
							        (((iBlendSrc * (*pTex & 0x0000FF00) >> 8) + ((clr & 0x0000FF00) >> 8) * iBlend) >> 8) << 8 |
							        (((iBlendSrc * (*pTex & 0x00FF0000) >> 16) + ((clr & 0x00FF0000) >> 16) * iBlend) >> 8) << 16;
							//*pTex = ((iBlendSrc * (*pTex & 0x00ff0000)	+  (clr & 0x00ff0000)        * iBlend) >> 8)      |
							//	(((iBlendSrc * (*pTex & 0x0000FF00) >>  8) + ((clr & 0x0000FF00) >>  8) * iBlend) >> 8) << 8 |
							//	(((iBlendSrc * (*pTex & 0x000000ff) >> 16) + ((clr & 0x000000ff) >> 16) * iBlend) >> 8) << 16;

						}
						pTex++;
					}
				}
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLayerTexGen::GetSectorRect(CPoint sector, CRect& rect)
{
	rect.left = sector.x * m_sectorResolution;
	rect.top = sector.y * m_sectorResolution;
	rect.right = rect.left + m_sectorResolution;
	rect.bottom = rect.top + m_sectorResolution;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLayerTexGen::Log(const char* format, ...)
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
int CTerrainLayerTexGen::GetSectorFlags(CPoint sector)
{
	assert(sector.x >= 0 && sector.x < m_numSectors && sector.y >= 0 && sector.y < m_numSectors);
	int index = sector.x + sector.y * m_numSectors;
	return m_sectorGrid[index].m_Flags;
}

//////////////////////////////////////////////////////////////////////////
CTerrainLayerTexGen::CTexSectorInfo& CTerrainLayerTexGen::GetCTexSectorInfo(CPoint sector)
{
	assert(sector.x >= 0 && sector.x < m_numSectors && sector.y >= 0 && sector.y < m_numSectors);
	int index = sector.x + sector.y * m_numSectors;
	return m_sectorGrid[index];
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLayerTexGen::SetSectorFlags(CPoint sector, int flags)
{
	assert(sector.x >= 0 && sector.x < m_numSectors && sector.y >= 0 && sector.y < m_numSectors);
	m_sectorGrid[sector.x + sector.y * m_numSectors].m_Flags |= flags;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLayerTexGen::InvalidateLayers()
{
	for (int i = 0; i < m_numSectors * m_numSectors; i++)
		m_sectorGrid[i].m_Flags &= ~eSectorLayersValid;
}

//////////////////////////////////////////////////////////////////////////
int CTerrainLayerTexGen::GetLayerCount() const
{
	return m_layers.size();
}

//////////////////////////////////////////////////////////////////////////
CLayer* CTerrainLayerTexGen::GetLayer(int index) const
{
	assert(index >= 0 && index < m_layers.size());
	return m_layers[index].m_pLayer;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLayerTexGen::ClearLayers()
{
	//	for (int i = 0; i < m_layers.size(); i++)
	//	{
	//	}
	m_layers.clear();
}

//////////////////////////////////////////////////////////////////////////
CByteImage* CTerrainLayerTexGen::GetLayerMask(CLayer* layer)
{
	for (int i = 0; i < m_layers.size(); i++)
	{
		SLayerInfo& ref = m_layers[i];

		if (ref.m_pLayer == layer)
			return ref.m_pLayerMask;
	}
	return 0;
}

