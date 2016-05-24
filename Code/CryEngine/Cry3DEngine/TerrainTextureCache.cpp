// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   TerrainTextureCache.cpp
//  Version:     v1.00
//  Created:     28/1/2007 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain texture management
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"

CTextureCache::CTextureCache()
{
	m_eTexFormat = eTF_Unknown;
	m_nDim = 0;
	m_nPoolTexId = 0;
	m_nPoolDim = 0;
}

CTextureCache::~CTextureCache()
{
	if (GetPoolSize())
		assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == GetCVars()->e_TerrainTextureStreamingPoolItemsNum);

	ResetTexturePool();
}

void CTextureCache::ResetTexturePool()
{
	m_FreeTextures.AddList(m_UsedTextures);
	m_UsedTextures.Clear();
	m_FreeTextures.AddList(m_Quarantine);
	m_Quarantine.Clear();
	if (GetRenderer())
	{
		GetRenderer()->RemoveTexture(m_nPoolTexId);
	}
	m_FreeTextures.Clear();
}

uint16 CTextureCache::GetTexture(byte* pData, uint16& nSlotId)
{
	if (!m_FreeTextures.Count())
		Update();

	if (!m_FreeTextures.Count())
	{
		Error("CTextureCache::GetTexture: !m_FreeTextures.Count()");
		return 0;
	}

	nSlotId = m_FreeTextures.Last();

	m_FreeTextures.DeleteLast();

	RectI region;
	GetSlotRegion(region, nSlotId);

	GetRenderer()->DownLoadToVideoMemory(pData, m_nDim, m_nDim, m_eTexFormat, m_eTexFormat, 0, false, FILTER_NONE, m_nPoolTexId,
	                                     NULL, FT_USAGE_ALLOWREADSRGB, GetTerrain()->GetEndianOfTexture(), &region);

	m_UsedTextures.Add(nSlotId);

	assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == GetCVars()->e_TerrainTextureStreamingPoolItemsNum);

	return m_nPoolTexId;
}

void CTextureCache::UpdateTexture(byte* pData, const uint16& nSlotId)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(nSlotId >= 0 && nSlotId < GetPoolSize());

	RectI region;
	GetSlotRegion(region, nSlotId);

	GetRenderer()->DownLoadToVideoMemory(pData, m_nDim, m_nDim, m_eTexFormat, m_eTexFormat, 0, false, FILTER_NONE, m_nPoolTexId,
	                                     NULL, FT_USAGE_ALLOWREADSRGB, GetTerrain()->GetEndianOfTexture(), &region);
}

void CTextureCache::ReleaseTexture(uint16& nTexId, uint16& nTexSlotId)
{
	assert(nTexId == m_nPoolTexId);

	if (m_UsedTextures.Delete(nTexSlotId))
	{
		m_Quarantine.Add(nTexSlotId);

		nTexId = 0;
		nTexSlotId = ~0;
	}
	else
		assert(!"Attempt to release non pooled texture");

	assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == GetCVars()->e_TerrainTextureStreamingPoolItemsNum);
}

bool CTextureCache::Update()
{
	m_FreeTextures.AddList(m_Quarantine);
	m_Quarantine.Clear();

	return GetPoolSize() == GetCVars()->e_TerrainTextureStreamingPoolItemsNum;
}

int CTextureCache::GetPoolSize()
{
	return (m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count());
}

void CTextureCache::InitPool(byte* pData, int nDim, ETEX_Format eTexFormat)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Texture, 0, "Terrain texture cache");

	if (m_eTexFormat != eTexFormat || m_nDim != nDim)
	{
		ResetTexturePool();

		m_eTexFormat = eTexFormat;
		m_nDim = nDim;

		m_nPoolDim = (int)sqrt(GetCVars()->e_TerrainTextureStreamingPoolItemsNum);
		int nPoolTexDim = m_nPoolDim * m_nDim;

		stack_string sPoolTextureName;
		sPoolTextureName.Format("$TERRAIN_TEX_POOL_%p", this);

		m_nPoolTexId = GetRenderer()->DownLoadToVideoMemory(NULL, nPoolTexDim, nPoolTexDim, eTexFormat, eTexFormat, 0, false, FILTER_NONE, 0,
		                                                    sPoolTextureName, (eTexFormat == eTF_R32F) ? 0 : FT_USAGE_ALLOWREADSRGB, GetPlatformEndian(), NULL, false);

		if (m_nPoolTexId <= 0)
		{
			if (!gEnv->IsDedicated())
				Error("Debug: CTextureCache::InitPool: GetRenderer()->DownLoadToVideoMemory returned %d", m_nPoolTexId);

			if (!gEnv->IsDedicated())
				Error("Debug: DownLoadToVideoMemory() params: dim=%d, eTexFormat=%d", nDim, eTexFormat);
		}

		for (int i = GetCVars()->e_TerrainTextureStreamingPoolItemsNum - 1; i >= 0; i--)
		{
			m_FreeTextures.Add(i);
		}
	}

	assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == GetCVars()->e_TerrainTextureStreamingPoolItemsNum);
}

void CTextureCache::GetSlotRegion(RectI& region, int nSlotId)
{
	region.w = region.h = m_nDim;
	region.x = (nSlotId / m_nPoolDim) * m_nDim;
	region.y = (nSlotId % m_nPoolDim) * m_nDim;
}
