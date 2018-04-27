// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_load.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain loading
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain.h"
#include "terrain_sector.h"
#include "ObjMan.h"
#include "terrain_water.h"
#include "3dEngine.h"
#include "Vegetation.h"

CTerrain::CTerrain(const STerrainInfo& TerrainInfo)
{
	m_bProcVegetationInUse = false;
	m_nLoadedSectors = 0;
	m_bOceanIsVisible = 0;
	m_fDistanceToSectorWithWater = 0;
	//	m_nDiffTexIndexTableSize = 0;
	//	m_nDiffTexTreeLevelOffset = 0;
	//	ZeroStruct(m_hdrDiffTexInfo);
	//	ZeroStruct(m_TerrainTextureLayer);
	//	m_ucpDiffTexTmpBuffer = 0;
	m_pTerrainEf = 0;
	//	ZeroStruct(m_SSurfaceType); // staic
	m_pOcean = 0;
	m_eEndianOfTexture = eLittleEndian;

	// load default textures
	m_nWhiteTexId = Get3DEngine()->GetWhiteTexID();
	m_nBlackTexId = Get3DEngine()->GetBlackTexID();

	// set params
	m_fUnitSize = TerrainInfo.unitSize_InMeters;

	if (m_fUnitSize <= 0 || m_fUnitSize > GetSectorSize())
	{
		m_fUnitSize = (float)*((int32*)&TerrainInfo.unitSize_InMeters);
	}

	m_fInvUnitSize = 1.f / m_fUnitSize;
	m_nTerrainSize = int(TerrainInfo.heightMapSize_InUnits * m_fUnitSize);
	m_nSectorSize = TerrainInfo.sectorSize_InMeters;
	m_nSectorsTableSize = TerrainInfo.sectorsTableSize_InSectors;
	m_fHeightmapZRatio = TerrainInfo.heightmapZRatio;
	m_fOceanWaterLevel = TerrainInfo.oceanWaterLevel;

	m_nUnitsToSectorBitShift = 0;
	float nSecSize = (float)m_nSectorSize;
	while (nSecSize > m_fUnitSize)
	{
		nSecSize /= 2;
		m_nUnitsToSectorBitShift++;
	}

	assert(m_nSectorsTableSize == m_nTerrainSize / m_nSectorSize);

	m_nTerrainSizeDiv = int(m_nTerrainSize * m_fInvUnitSize) - 1;

	assert(!Get3DEngine()->m_pObjectsTree);
	Get3DEngine()->m_pObjectsTree = COctreeNode::Create(AABB(Vec3(0,0,0), Vec3((float)GetTerrainSize())), NULL);

	m_SSurfaceType.PreAllocate(SRangeInfo::e_max_surface_types, SRangeInfo::e_max_surface_types);

	InitHeightfieldPhysics();

	if (GetRenderer())
	{
		SInputShaderResourcesPtr pIsr = GetRenderer()->EF_CreateInputShaderResource();
		pIsr->m_LMaterial.m_Opacity = 1.0f;
		m_pTerrainEf = MakeSystemMaterialFromShader("Terrain", pIsr);
	}

	//	memset(m_arrImposters,0,sizeof(m_arrImposters));
	//	memset(m_arrImpostersTopBottom,0,sizeof(m_arrImpostersTopBottom));

	m_StoredModifications.SetTerrain(*this);

	assert(m_SSurfaceType.GetDataSize() < SRangeInfo::e_max_surface_types * 1000);

	m_pTerrainUpdateDispatcher = GetRenderer() ? new CTerrainUpdateDispatcher() : nullptr;

#if defined(FEATURE_SVO_GI)
	m_pTerrainRgbLowResSystemCopy = 0;
#endif
}

void CTerrain::CloseTerrainTextureFile()
{
	m_arrBaseTexInfos.m_nDiffTexIndexTableSize = 0;

	for (int i = m_lstActiveTextureNodes.size() - 1; i >= 0; --i)
	{
		m_lstActiveTextureNodes[i]->UnloadNodeTexture(false);
		m_lstActiveTextureNodes.Delete(i);
	}

	for (int i = m_lstActiveProcObjNodes.size() - 1; i >= 0; --i)
	{
		m_lstActiveProcObjNodes[i]->RemoveProcObjects(false);
		m_lstActiveProcObjNodes.Delete(i);
	}

	if (GetParentNode())
		GetParentNode()->UnloadNodeTexture(true);

	m_bOpenTerrainTextureFileNoLog = false;
}

CTerrain::~CTerrain()
{
	INDENT_LOG_DURING_SCOPE(true, "Destroying terrain");

	SAFE_DELETE(m_pOcean);
	SAFE_DELETE(m_pTerrainUpdateDispatcher);

#if defined(FEATURE_SVO_GI)
	SAFE_DELETE(m_pTerrainRgbLowResSystemCopy);
#endif

	// terrain texture file info
	CloseTerrainTextureFile();
	SAFE_DELETE_ARRAY(m_arrBaseTexInfos.m_ucpDiffTexTmpBuffer);
	ZeroStruct(m_arrBaseTexInfos);

	// surface types
	for (int i = 0; i < m_SSurfaceType.Count(); i++)
	{
		m_SSurfaceType[i].lstnVegetationGroups.Reset();
		m_SSurfaceType[i].pLayerMat = NULL;
	}

	m_SSurfaceType.Reset();
	ZeroStruct(m_SSurfaceType);

	// terrain nodes tree
	SAFE_DELETE(m_pParentNode);

	// terrain nodes pyramid
	int cnt = m_arrSecInfoPyramid.Count();
	assert(!cnt || cnt == TERRAIN_NODE_TREE_DEPTH);
	for (int i = 0; i < cnt; i++)
		m_arrSecInfoPyramid[i].Reset();
	m_arrSecInfoPyramid.Reset();

	// objects tree
	SAFE_DELETE(Get3DEngine()->m_pObjectsTree);

	CTerrainNode::ResetStaticData();
}

void CTerrain::InitHeightfieldPhysics()
{
	// for phys engine
	primitives::heightfield hf;
	hf.Basis.SetIdentity();
	hf.origin.zero();
	hf.step.x = hf.step.y = (float)CTerrain::GetHeightMapUnitSize();
	hf.size.x = hf.size.y = int(CTerrain::GetTerrainSize() * CTerrain::GetHeightMapUnitSizeInverted());
	hf.stride.set(hf.size.y + 1, 1);
	hf.heightscale = 1.0f;//m_fHeightmapZRatio;
	hf.typemask = SRangeInfo::e_hole | SRangeInfo::e_undefined;
	hf.typehole = SRangeInfo::e_hole;
	hf.heightmask = ~SRangeInfo::e_index_hole; // hf.heightmask does not appear to be used anywhere. What is it for?

	hf.fpGetHeightCallback = GetHeightFromUnits_Callback;
	hf.fpGetSurfTypeCallback = GetSurfaceTypeFromUnits_Callback;

	int arrMatMapping[SRangeInfo::e_max_surface_types];
	memset(arrMatMapping, 0, sizeof(arrMatMapping));
	for (int i = 0; i < SRangeInfo::e_max_surface_types; i++)
		if (IMaterial* pMat = m_SSurfaceType[i].pLayerMat)
		{
			if (pMat->GetSubMtlCount() > 2)
				pMat = pMat->GetSubMtl(2);
			arrMatMapping[i] = pMat->GetSurfaceTypeId();
		}

	// KLUDGE: The 3rd parameter in the following call (nMats) cannot be 128 because it gets assigned to an unsigned:7!
	IPhysicalEntity* pPhysTerrain = GetPhysicalWorld()->SetHeightfieldData(&hf, arrMatMapping, SRangeInfo::e_undefined);
	pe_params_foreign_data pfd;
	pfd.iForeignData = PHYS_FOREIGN_ID_TERRAIN;
	pPhysTerrain->SetParams(&pfd);
}

void CTerrain::SetMaterialMapping()
{
	int arrMatMapping[SRangeInfo::e_max_surface_types];
	memset(arrMatMapping, 0, sizeof(arrMatMapping));
	for (int i = 0; i < SRangeInfo::e_max_surface_types; i++)
		if (IMaterial* pMat = m_SSurfaceType[i].pLayerMat)
		{
			if (pMat->GetSubMtlCount() > 2)
				pMat = pMat->GetSubMtl(2);
			arrMatMapping[i] = pMat->GetSurfaceTypeId();
		}

	GetPhysicalWorld()->SetHeightfieldMatMapping(arrMatMapping, SRangeInfo::e_undefined);
}

void CTerrain::GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors)
{
	nLoadedSectors = m_nLoadedSectors;
	nTotalSectors = 0;

	int nSectors = CTerrain::GetSectorsTableSize();
	nTotalSectors = nSectors * nSectors;
}

bool CTerrain::m_bOpenTerrainTextureFileNoLog = false;

bool CTerrain::OpenTerrainTextureFile(SCommonFileHeader& hdrDiffTexHdr, STerrainTextureFileHeader& hdrDiffTexInfo, const char* szFileName, uint8*& ucpDiffTexTmpBuffer, int& nDiffTexIndexTableSize)
{
	if (!GetRenderer())
		return false;

	FUNCTION_PROFILER_3DENGINE;

	m_arrBaseTexInfos.m_nDiffTexIndexTableSize = 0;

	if (GetParentNode())
		GetParentNode()->UnloadNodeTexture(true);
	else
		return false;

	bool bNoLog = m_bOpenTerrainTextureFileNoLog;
	m_bOpenTerrainTextureFileNoLog = true;

	if (!bNoLog)
		PrintMessage("Opening %s ...", szFileName);

	// rbx open flags, x is a hint to not cache whole file in memory.
	FILE* fpDiffTexFile = gEnv->pCryPak->FOpen(Get3DEngine()->GetLevelFilePath(szFileName), "rbx");

	if (!fpDiffTexFile)
	{
		if (!bNoLog)
			PrintMessage("Error opening terrain texture file: file not found (you might need to regenerate the surface texture)");
		return false;
	}

	if (!gEnv->pCryPak->FRead(&hdrDiffTexHdr, 1, fpDiffTexFile, false))
	{
		gEnv->pCryPak->FClose(fpDiffTexFile);
		fpDiffTexFile = 0;
		CloseTerrainTextureFile();
		if (!bNoLog)
			PrintMessage("Error opening terrain texture file: header not found (file is broken)");
		return false;
	}

	m_eEndianOfTexture = (hdrDiffTexHdr.flags & SERIALIZATION_FLAG_BIG_ENDIAN) ? eBigEndian : eLittleEndian;

	SwapEndian(hdrDiffTexHdr, m_eEndianOfTexture);

	if (strcmp(hdrDiffTexHdr.signature, "CRY") || hdrDiffTexHdr.file_type != eTerrainTextureFile)
	{
		gEnv->pCryPak->FClose(fpDiffTexFile);
		fpDiffTexFile = 0;
		CloseTerrainTextureFile();
		PrintMessage("Error opening terrain texture file: invalid signature");
		return false;
	}

	if (hdrDiffTexHdr.version != FILEVERSION_TERRAIN_TEXTURE_FILE)
	{
		gEnv->pCryPak->FClose(fpDiffTexFile);
		fpDiffTexFile = 0;
		CloseTerrainTextureFile();
		if (!bNoLog)
			Error("Error opening terrain texture file: version error (you might need to regenerate the surface texture)");
		return false;
	}

	gEnv->pCryPak->FRead(&hdrDiffTexInfo, 1, fpDiffTexFile, m_eEndianOfTexture != GetPlatformEndian());

	memset(m_arrBaseTexInfos.m_TerrainTextureLayer, 0, sizeof(m_arrBaseTexInfos.m_TerrainTextureLayer));

	// layers
	for (uint32 dwI = 0; dwI < hdrDiffTexInfo.nLayerCount; ++dwI)
	{
		assert(dwI <= 2);
		if (dwI > 2) break;                     // too many layers

		gEnv->pCryPak->FRead(&m_arrBaseTexInfos.m_TerrainTextureLayer[dwI], 1, fpDiffTexFile, m_eEndianOfTexture != GetPlatformEndian());

		PrintMessage("  TerrainLayer %d: TexFormat: %s, SectorTextureSize: %dx%d, SectorTextureDataSizeBytes: %d",
		             dwI, GetRenderer()->GetTextureFormatName(m_arrBaseTexInfos.m_TerrainTextureLayer[dwI].eTexFormat),
		             m_arrBaseTexInfos.m_TerrainTextureLayer[dwI].nSectorSizePixels, m_arrBaseTexInfos.m_TerrainTextureLayer[dwI].nSectorSizePixels,
		             m_arrBaseTexInfos.m_TerrainTextureLayer[dwI].nSectorSizeBytes);
	}

	// unlock all nodes
	for (int nTreeLevel = 0; nTreeLevel < TERRAIN_NODE_TREE_DEPTH; nTreeLevel++)
		for (int x = 0; x < m_arrSecInfoPyramid[nTreeLevel].GetSize(); x++)
			for (int y = 0; y < m_arrSecInfoPyramid[nTreeLevel].GetSize(); y++)
			{
#ifndef _RELEASE
				m_arrSecInfoPyramid[nTreeLevel][x][y]->m_eTextureEditingState = eTES_SectorIsUnmodified;
				m_arrSecInfoPyramid[nTreeLevel][x][y]->m_eElevTexEditingState = eTES_SectorIsUnmodified;
#endif // _RELEASE

				m_arrSecInfoPyramid[nTreeLevel][x][y]->m_nNodeTextureOffset = -1;
			}

	// index block
	{
		std::vector<int16> IndexBlock;    // for explanation - see saving code

		uint16 wSize;
		gEnv->pCryPak->FRead(&wSize, 1, fpDiffTexFile, m_eEndianOfTexture != GetPlatformEndian());

		IndexBlock.resize(wSize);

		for (uint16 wI = 0; wI < wSize; ++wI)
			gEnv->pCryPak->FRead(&IndexBlock[wI], 1, fpDiffTexFile, m_eEndianOfTexture != GetPlatformEndian());

		PrintMessage("  RGB multiplier: %.f, Texture indices: %d", 1.f / hdrDiffTexInfo.fBrMultiplier, wSize);

		int16* pIndices = &IndexBlock[0];
		int16 nElementsLeft = wSize;

		if (wSize > 0 && pIndices[0] >= 0)
		{
			CTerrainNode* const p = GetParentNode();
			if (p)
			{
				p->AssignTextureFileOffset(pIndices, nElementsLeft);
				assert(nElementsLeft == 0);
			}
		}

		nDiffTexIndexTableSize = wSize * sizeof(uint16) + sizeof(uint16);

		if (!m_bEditor)
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CTerrain::OpenTerrainTextureFile: ReleaseHoleNodes & UpdateTerrainNodes");

			int nNodesCounterBefore = CTerrainNode::m_nNodesCounter;
			GetParentNode()->ReleaseHoleNodes();
			PrintMessage("  %d out of %d nodes cleaned", nNodesCounterBefore - CTerrainNode::m_nNodesCounter, nNodesCounterBefore);

			if (Get3DEngine()->m_pObjectsTree)
				Get3DEngine()->m_pObjectsTree->UpdateTerrainNodes();
		}
	}

	int nSectorHeightMapTextureDim = m_arrBaseTexInfos.m_TerrainTextureLayer[1].nSectorSizePixels;

	delete[] ucpDiffTexTmpBuffer;
	ucpDiffTexTmpBuffer = new uint8[m_arrBaseTexInfos.m_TerrainTextureLayer[0].nSectorSizeBytes + m_arrBaseTexInfos.m_TerrainTextureLayer[1].nSectorSizeBytes + sizeof(float)*nSectorHeightMapTextureDim*nSectorHeightMapTextureDim];

	gEnv->pCryPak->FClose(fpDiffTexFile);
	fpDiffTexFile = 0;

	// if texture compression format is not supported by GPU - use uncompressed RGBA and decompress on CPU
	ETEX_Format eTexPoolFormat = m_arrBaseTexInfos.m_TerrainTextureLayer[0].eTexFormat;

	if (!GetRenderer()->IsTextureFormatSupported(eTexPoolFormat))
	{
		GetLog()->LogWarning("Warning: Terrain texture compression format (%s) is not supported, fall back to CPU decompression", GetRenderer()->GetTextureFormatName(eTexPoolFormat));

		eTexPoolFormat = eTF_R8G8B8A8;
	}

	// init texture pools
	m_texCache[0].InitPool(0, m_arrBaseTexInfos.m_TerrainTextureLayer[0].nSectorSizePixels, eTexPoolFormat);
	m_texCache[1].InitPool(0, m_arrBaseTexInfos.m_TerrainTextureLayer[1].nSectorSizePixels, eTexPoolFormat);
	m_texCache[2].InitPool(0, nSectorHeightMapTextureDim, eTF_R32F);

	return true;
}
