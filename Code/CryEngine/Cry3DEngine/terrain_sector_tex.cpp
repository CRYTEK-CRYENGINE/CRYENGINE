// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_sector_tex.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain texture management
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "ObjMan.h"

// load textures from buffer
int CTerrainNode::CreateSectorTexturesFromBuffer(float * pSectorHeightMap)
{
	FUNCTION_PROFILER_3DENGINE;

	InvalidatePermanentRenderObject();

	STerrainTextureLayerFileHeader* pLayers = GetTerrain()->m_arrBaseTexInfos[m_nSID].m_TerrainTextureLayer;

	//	uint32 nSectorDataSize = pLayers[0].nSectorSizeBytes + pLayers[1].nSectorSizeBytes;

	// make RGB texture
	m_nNodeTexSet.nTex0 = m_pTerrain->m_texCache[0].GetTexture(GetTerrain()->m_arrBaseTexInfos[m_nSID].m_ucpDiffTexTmpBuffer, m_nNodeTexSet.nSlot0);

	// make normal map
	m_nNodeTexSet.nTex1 = m_pTerrain->m_texCache[1].GetTexture(GetTerrain()->m_arrBaseTexInfos[m_nSID].m_ucpDiffTexTmpBuffer + pLayers[0].nSectorSizeBytes, m_nNodeTexSet.nSlot1);

	// make height map
	m_nNodeTexSet.nTex2 = m_pTerrain->m_texCache[2].GetTexture((byte*)pSectorHeightMap, m_nNodeTexSet.nSlot2);

	if (GetCVars()->e_TerrainTextureStreamingDebug == 2)
		PrintMessage("CTerrainNode::CreateSectorTexturesFromBuffer: sector %d, level=%d", GetSecIndex(), m_nTreeLevel);

	assert(m_nNodeTexSet.nTex0);

#if defined(FEATURE_SVO_GI)
	// keep low res terrain colors in system memory for CPU-side voxelization
	if (!m_pParent)
	{
		int nDim = GetTerrain()->m_arrBaseTexInfos[0].m_TerrainTextureLayer[0].nSectorSizePixels;
		ETEX_Format texFormat = GetTerrain()->m_arrBaseTexInfos[0].m_TerrainTextureLayer[0].eTexFormat;
		int nSizeDxtMip0 = GetRenderer()->GetTextureFormatDataSize(nDim, nDim, 1, 1, texFormat);
		int nSizeMip0 = GetRenderer()->GetTextureFormatDataSize(nDim, nDim, 1, 1, eTF_R8G8B8A8);

		SAFE_DELETE(GetTerrain()->m_pTerrainRgbLowResSystemCopy);
		GetTerrain()->m_pTerrainRgbLowResSystemCopy = new PodArray<ColorB>;
		GetTerrain()->m_pTerrainRgbLowResSystemCopy->CheckAllocated(nSizeMip0 / sizeof(ColorB));

		GetRenderer()->DXTDecompress(GetTerrain()->m_arrBaseTexInfos[m_nSID].m_ucpDiffTexTmpBuffer, nSizeDxtMip0,
		                             (byte*)GetTerrain()->m_pTerrainRgbLowResSystemCopy->GetElements(), nDim, nDim, 1, texFormat, false, 4);
	}
#endif

	return (m_nNodeTexSet.nTex0 > 0);
}

void CTerrainNode::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pStream->IsError())
	{
		// file was not loaded successfully
		if (pStream->GetError() == ERROR_USER_ABORT)
			Warning("CTerrainNode::StreamAsyncOnComplete: node streaming aborted, sector %d, level=%d", GetSecIndex(), m_nTreeLevel);
		else
		{
			Warning("CTerrainNode::StreamAsyncOnComplete: error streaming node, sector %d, level=%d, error=%s", GetSecIndex(), m_nTreeLevel, pStream->GetErrorName());
			assert(!"Error streaming node");
		}
		m_eTexStreamingStatus = ecss_NotLoaded;
		m_pReadStream = NULL;
		return;
	}

	// fill sector height map data
	int nTexSize = m_pTerrain->m_texCache[2].m_nDim;
	float fBoxSize = GetBBox().GetSize().x;
	Array2d<float> arrHmData;
	arrHmData.Allocate(nTexSize);
	for (int x = 0; x < nTexSize; x++) for (int y = 0; y < nTexSize; y++)
	{
		arrHmData[x][y] = m_pTerrain->GetZApr(
			(float)m_nOriginX + fBoxSize * float(x) / nTexSize * (1.f + 1.f / (float)nTexSize),
			(float)m_nOriginY + fBoxSize * float(y) / nTexSize * (1.f + 1.f / (float)nTexSize), 0);
	}

	STerrainTextureLayerFileHeader* pLayers = GetTerrain()->m_arrBaseTexInfos[m_nSID].m_TerrainTextureLayer;

	memcpy((float*)pStream->GetUserData(), arrHmData.GetData(), arrHmData.GetDataSize());
}

void CTerrainNode::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pStream->IsError())
	{
		// file was not loaded successfully
		if (pStream->GetError() == ERROR_USER_ABORT)
			Warning("CTerrainNode::StreamOnComplete: node streaming aborted, sector %d, level=%d", GetSecIndex(), m_nTreeLevel);
		else
		{
			Warning("CTerrainNode::StreamOnComplete: error streaming node, sector %d, level=%d, error=%s", GetSecIndex(), m_nTreeLevel, pStream->GetErrorName());
			assert(!"Error streaming node");
		}
		m_eTexStreamingStatus = ecss_NotLoaded;
		m_pReadStream = NULL;
		return;
	}

	memcpy(GetTerrain()->m_arrBaseTexInfos[m_nSID].m_ucpDiffTexTmpBuffer, pStream->GetBuffer(), pStream->GetBytesRead());
	CreateSectorTexturesFromBuffer((float*)pStream->GetUserData());
	delete [] (float*)pStream->GetUserData();

	CalculateTexGen(this, m_nNodeTexSet.fTexOffsetX, m_nNodeTexSet.fTexOffsetY, m_nNodeTexSet.fTexScale);

	m_eTexStreamingStatus = ecss_Ready;

	m_pReadStream = NULL;

#ifndef _RELEASE
	// check and request texture update from editor side data
	if (m_eTextureEditingState == eTES_SectorIsModified_AtlasIsUpToDate)
		m_eTextureEditingState = eTES_SectorIsModified_AtlasIsDirty;
#endif
}

void CTerrainNode::StartSectorTexturesStreaming(bool bFinishNow)
{
	assert(m_eTexStreamingStatus == ecss_NotLoaded);

	CTerrain* pT = GetTerrain();

	if (!pT->m_arrBaseTexInfos[m_nSID].m_nDiffTexIndexTableSize)
		return; // file was not opened

#if defined(FEATURE_SVO_GI)
	// make sure top most node is ready immediately
	if (!m_pParent && GetCVars()->e_svoTI_Apply)
		bFinishNow = true;
#endif

	STerrainTextureLayerFileHeader* pLayers = GetTerrain()->m_arrBaseTexInfos[m_nSID].m_TerrainTextureLayer;

	uint32 nSectorDataSize = pLayers[0].nSectorSizeBytes + pLayers[1].nSectorSizeBytes;

	// calculate sector data offset in file
	int nFileOffset = sizeof(SCommonFileHeader) + sizeof(STerrainTextureFileHeader)
	                  + pT->m_arrBaseTexInfos[m_nSID].m_hdrDiffTexInfo.nLayerCount * sizeof(STerrainTextureLayerFileHeader)
	                  + pT->m_arrBaseTexInfos[m_nSID].m_nDiffTexIndexTableSize
	                  + m_nNodeTextureOffset * nSectorDataSize;

	// start streaming
	StreamReadParams params;	
	params.dwUserData = (DWORD_PTR) new float[m_pTerrain->m_texCache[2].m_nDim * m_pTerrain->m_texCache[2].m_nDim];
	params.nSize = nSectorDataSize;
	params.nLoadTime = 1000;
	params.nMaxLoadTime = 0;
	params.nOffset = nFileOffset;
	if (bFinishNow)
		params.ePriority = estpUrgent;

	m_pReadStream = GetSystem()->GetStreamEngine()->StartRead(eStreamTaskTypeTerrain,
	                                                          Get3DEngine()->GetLevelFilePath(GetTerrain()->m_arrSegmentPaths[m_nSID] + COMPILED_TERRAIN_TEXTURE_FILE_NAME), this, &params);

	m_eTexStreamingStatus = ecss_InProgress;

	if (GetCVars()->e_TerrainTextureStreamingDebug == 2)
		PrintMessage("CTerrainNode::StartSectorTexturesStreaming: sector %d, level=%d", GetSecIndex(), m_nTreeLevel);

	if (bFinishNow)
		m_pReadStream->Wait();
}

void CTerrainNode::CalculateTexGen(const CTerrainNode* pTextureSourceNode, float& fTexOffsetX, float& fTexOffsetY, float& fTexScale)
{
	float fSectorSizeScale = 1.0f;
	uint16 nSectorSizePixels = GetTerrain()->m_arrBaseTexInfos[m_nSID].m_TerrainTextureLayer[0].nSectorSizePixels;
	if (nSectorSizePixels)
		fSectorSizeScale -= 1.0f / (float)(nSectorSizePixels); // we don't use half texel border so we have to compensate

	float dCSS = fSectorSizeScale / (CTerrain::GetSectorSize() << pTextureSourceNode->m_nTreeLevel);
	fTexOffsetX = -dCSS * pTextureSourceNode->m_nOriginY + GetTerrain()->m_arrSegmentOrigns[m_nSID].y;
	fTexOffsetY = -dCSS * pTextureSourceNode->m_nOriginX + GetTerrain()->m_arrSegmentOrigns[m_nSID].x;
	fTexScale = dCSS;

	// shift texture by 0.5 pixel
	if (float fTexRes = nSectorSizePixels)
	{
		fTexOffsetX += 0.5f / fTexRes;
		fTexOffsetY += 0.5f / fTexRes;
	}

	if (pTextureSourceNode->m_nNodeTexSet.nSlot0 != 0xffff)
	{
		RectI region;
		m_pTerrain->m_texCache[0].GetSlotRegion(region, pTextureSourceNode->m_nNodeTexSet.nSlot0);

		float fPoolScale = (float)region.w / (float)m_pTerrain->m_texCache[0].GetPoolTexDim();
		float fPoolTexDim = (float)m_pTerrain->m_texCache[0].GetPoolTexDim();

		fTexOffsetX = fTexOffsetX * fPoolScale + (float)(region.x) / fPoolTexDim;
		fTexOffsetY = fTexOffsetY * fPoolScale + (float)(region.y) / fPoolTexDim;
		fTexScale *= fPoolScale;

		if (GetCVars()->e_TerrainTextureStreamingDebug && GetConsole()->GetCVar("r_ShowTexture"))
			GetConsole()->GetCVar("r_ShowTexture")->Set(m_pTerrain->m_texCache[0].m_nPoolTexId);
	}
}
