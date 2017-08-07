// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
#ifndef SEG_WORLD
	m_nSectorsTableSize = TerrainInfo.sectorsTableSize_InSectors;
#endif
	m_fHeightmapZRatio = TerrainInfo.heightmapZRatio;
	m_fOceanWaterLevel = TerrainInfo.oceanWaterLevel;

	m_nUnitsToSectorBitShift = 0;
	float nSecSize = (float)m_nSectorSize;
	while (nSecSize > m_fUnitSize)
	{
		nSecSize /= 2;
		m_nUnitsToSectorBitShift++;
	}

	// make flat 0 level heightmap
	//	m_arrusHightMapData.Allocate(TerrainInfo.heightMapSize_InUnits+1);
	//	m_arrHightMapRangeInfo.Allocate((TerrainInfo.heightMapSize_InUnits>>m_nRangeBitShift)+1);

#ifndef SEG_WORLD
	assert(m_nSectorsTableSize == m_nTerrainSize / m_nSectorSize);
#endif

	m_nTerrainSizeDiv = int(m_nTerrainSize * m_fInvUnitSize) - 1;

	m_lstSectors.reserve(512); // based on inspection in MemReplay

	// create default segment
	ISegmentsManager* pSM = Get3DEngine()->m_pSegmentsManager;
	if (!pSM || !pSM->CreateSegments(this))
	{
		float f = (float) GetTerrainSize();
		CreateSegment(Vec3(f, f, f));
	}
	InitHeightfieldPhysics(0);

	if (GetRenderer())
	{
		SInputShaderResourcesPtr pIsr = GetRenderer()->EF_CreateInputShaderResource();
		pIsr->m_LMaterial.m_Opacity = 1.0f;
		m_pTerrainEf = MakeSystemMaterialFromShader("Terrain", pIsr);
	}

	//	memset(m_arrImposters,0,sizeof(m_arrImposters));
	//	memset(m_arrImpostersTopBottom,0,sizeof(m_arrImpostersTopBottom));

	m_StoredModifications.SetTerrain(*this);

	assert(m_SSurfaceType[0].GetDataSize() < SRangeInfo::e_max_surface_types * 1000);

	m_pTerrainUpdateDispatcher = GetRenderer() ? new CTerrainUpdateDispatcher() : nullptr;

#if defined(FEATURE_SVO_GI)
	m_pTerrainRgbLowResSystemCopy = 0;
#endif
}

void CTerrain::CloseTerrainTextureFile(int nSID)
{
	if (nSID < 0)
	{
		int nEndSID = GetMaxSegmentsCount();
		for (nSID = 0; nSID < nEndSID; ++nSID)
		{
			AABB aabb;
			if (!GetSegmentBounds(nSID, aabb))
				continue;
			CloseTerrainTextureFile(nSID);
		}
		return;
	}

	m_arrBaseTexInfos[nSID].m_nDiffTexIndexTableSize = 0;

	for (int i = m_lstActiveTextureNodes.size() - 1; i >= 0; --i)
	{
#ifdef SEG_WORLD
		if (m_lstActiveTextureNodes[i]->m_nSID == nSID)
#endif
		{
			m_lstActiveTextureNodes[i]->UnloadNodeTexture(false);
			m_lstActiveTextureNodes.Delete(i);
		}
	}

	for (int i = m_lstActiveProcObjNodes.size() - 1; i >= 0; --i)
	{
#ifdef SEG_WORLD
		if (m_lstActiveProcObjNodes[i]->m_nSID == nSID)
#endif
		{
			m_lstActiveProcObjNodes[i]->RemoveProcObjects(false);
			m_lstActiveProcObjNodes.Delete(i);
		}
	}

	if (GetParentNode(nSID))
		GetParentNode(nSID)->UnloadNodeTexture(true);

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

	for (int nSID = 0; nSID < m_SSurfaceType.Count(); nSID++)
		DeleteSegment(nSID, true);

	CTerrainNode::ResetStaticData();
}

void CTerrain::InitHeightfieldPhysics(int nSID)
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
		if (IMaterial* pMat = m_SSurfaceType[nSID][i].pLayerMat)
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

void CTerrain::SetMaterialMapping(int nSID)
{
	int arrMatMapping[SRangeInfo::e_max_surface_types];
	memset(arrMatMapping, 0, sizeof(arrMatMapping));
	for (int i = 0; i < SRangeInfo::e_max_surface_types; i++)
		if (IMaterial* pMat = m_SSurfaceType[nSID][i].pLayerMat)
		{
			if (pMat->GetSubMtlCount() > 2)
				pMat = pMat->GetSubMtl(2);
			arrMatMapping[i] = pMat->GetSurfaceTypeId();
		}

	GetPhysicalWorld()->SetHeightfieldMatMapping(arrMatMapping, SRangeInfo::e_undefined);
}
/*
   int __cdecl CTerrain__Cmp_CVegetationForLoading_Size(const void* v1, const void* v2)
   {
   CVegetationForLoading * p1 = ((CVegetationForLoading*)v1);
   CVegetationForLoading * p2 = ((CVegetationForLoading*)v2);

   PodArray<StatInstGroup> & lstStaticTypes = CStatObj::GetObjManager()->m_lstStaticTypes;

   CStatObj * pStatObj1 = (p1->GetID()<lstStaticTypes.Count()) ? lstStaticTypes[p1->GetID()].GetStatObj() : 0;
   CStatObj * pStatObj2 = (p2->GetID()<lstStaticTypes.Count()) ? lstStaticTypes[p2->GetID()].GetStatObj() : 0;

   if(!pStatObj1)
    return 1;
   if(!pStatObj2)
    return -1;

   int nSecId1 = 0;
   {  // get pos
    Vec3 vCenter = Vec3(p1->GetX(),p1->GetY(),p1->GetZ()) + (pStatObj1->GetBoxMin()+pStatObj1->GetBoxMax())*0.5f*p1->GetScale();
    // get sector ids
    int x = (int)(((vCenter.x)/CTerrain::GetSectorSize()));
    int y = (int)(((vCenter.y)/CTerrain::GetSectorSize()));
    // get obj bbox
    Vec3 vBMin = pStatObj1->GetBoxMin()*p1->GetScale();
    Vec3 vBMax = pStatObj1->GetBoxMax()*p1->GetScale();
    // if outside of the map, or too big - register in sector (0,0)
   //    if( x<0 || x>=CTerrain::GetSectorsTableSize() || y<0 || y>=CTerrain::GetSectorsTableSize() ||
   //    (vBMax.x - vBMin.x)>TERRAIN_SECTORS_MAX_OVERLAPPING*2 || (vBMax.y - vBMin.y)>TERRAIN_SECTORS_MAX_OVERLAPPING*2)
    //  x = y = 0;

    // get sector id
    nSecId1 = (x)*CTerrain::GetSectorsTableSize() + (y);
   }

   int nSecId2 = 0;
   {  // get pos
    Vec3 vCenter = Vec3(p2->GetX(),p2->GetY(),p2->GetZ()) + (pStatObj2->GetBoxMin()+pStatObj2->GetBoxMax())*0.5f*p2->GetScale();
    // get sector ids
    int x = (int)(((vCenter.x)/CTerrain::GetSectorSize()));
    int y = (int)(((vCenter.y)/CTerrain::GetSectorSize()));
    // get obj bbox
    Vec3 vBMin = pStatObj2->GetBoxMin()*p2->GetScale();
    Vec3 vBMax = pStatObj2->GetBoxMax()*p2->GetScale();
    // if outside of the map, or too big - register in sector (0,0)
   //    if( x<0 || x>=CTerrain::GetSectorsTableSize() || y<0 || y>=CTerrain::GetSectorsTableSize() ||
   //    (vBMax.x - vBMin.x)>TERRAIN_SECTORS_MAX_OVERLAPPING*2 || (vBMax.y - vBMin.y)>TERRAIN_SECTORS_MAX_OVERLAPPING*2)
    //  x = y = 0;

    // get sector id
    nSecId2 = (x)*CTerrain::GetSectorsTableSize() + (y);
   }

   if(nSecId1 > nSecId2)
    return -1;
   if(nSecId1 < nSecId2)
    return 1;

   if(p1->GetScale()*pStatObj1->GetRadius() > p2->GetScale()*pStatObj2->GetRadius())
    return 1;
   if(p1->GetScale()*pStatObj1->GetRadius() < p2->GetScale()*pStatObj2->GetRadius())
    return -1;

   return 0;
   }

   int __cdecl CTerrain__Cmp_Int(const void* v1, const void* v2)
   {
   if(*(uint32*)v1 > *(uint32*)v2)
    return 1;
   if(*(uint32*)v1 < *(uint32*)v2)
    return -1;

   return 0;
   }

   void CTerrain::LoadVegetationances()
   {
   assert(this); if(!this) return;

   PrintMessage("Loading static object positions ...");


   for( int x=0; x<CTerrain::GetSectorsTableSize(); x++)
   for( int y=0; y<CTerrain::GetSectorsTableSize(); y++)
    assert(!m_arrSecInfoPyramid[nSID][0][x][y]->m_lstEntities[STATIC_OBJECTS].Count());


   // load static object positions list
   PodArray<CVegetationForLoading> static_objects;
   static_objects.Load(Get3DEngine()->GetLevelFilePath("objects.lst"), gEnv->pCryPak);

   // todo: sorting in not correct for hierarchical system
   qsort(static_objects.GetElements(), static_objects.Count(),
    sizeof(static_objects[0]), CTerrain__Cmp_CVegetationForLoading_Size);

   // put objects into sectors depending on object position and fill lstUsedCGFs
   PodArray<CStatObj*> lstUsedCGFs;
   for(int i=0; i<static_objects.Count(); i++)
   {
    float x       = static_objects[i].GetX();
    float y       = static_objects[i].GetY();
    float z       = static_objects[i].GetZ()>0 ? static_objects[i].GetZ() : GetZApr(x,y);
    int  nId      = static_objects[i].GetID();
    uint8 ucBr    = static_objects[i].GetBrightness();
    uint8 ucAngle = static_objects[i].GetAngle();
    float fScale  = static_objects[i].GetScale();

    if( nId>=0 && nId<GetObjManager()->m_lstStaticTypes.Count() &&
        fScale>0 &&
        x>=0 && x<CTerrain::GetTerrainSize() && y>=0 && y<CTerrain::GetTerrainSize() &&
        GetObjManager()->m_lstStaticTypes[nId].GetStatObj() )
    {
      if(GetObjManager()->m_lstStaticTypes[nId].GetStatObj()->GetRadius()*fScale < GetCVars()->e_VegetationMinSize)
        continue; // skip creation of very small objects

      if(lstUsedCGFs.Find(GetObjManager()->m_lstStaticTypes[nId].GetStatObj())<0)
        lstUsedCGFs.Add(GetObjManager()->m_lstStaticTypes[nId].GetStatObj());

      CVegetation * pEnt = (CVegetation*)Get3DEngine()->CreateRenderNode( eERType_Vegetation );
      pEnt->m_fScale = fScale;
      pEnt->m_vPos = Vec3(x,y,z);
      pEnt->SetStatObjGroupId(nId);
      pEnt->m_ucBright = ucBr;
      pEnt->CalcBBox();
      pEnt->Physicalize( );
    }
   }

   // release not used CGF's
   int nGroupsReleased=0;
   for(int i=0; i<GetObjManager()->m_lstStaticTypes.Count(); i++)
   {
    CStatObj * pStatObj = GetObjManager()->m_lstStaticTypes[i].GetStatObj();
    if(pStatObj && lstUsedCGFs.Find(pStatObj)<0)
    {
      Get3DEngine()->ReleaseObject(pStatObj);
      GetObjManager()->m_lstStaticTypes[i].pStatObj = NULL;
      nGroupsReleased++;
    }
   }

   PrintMessagePlus(" %d objects created", static_objects.Count());
   }

   void CTerrain::CompileObjects()
   {
   // set max view distance and sort by size
   for(int nTreeLevel=0; nTreeLevel<TERRAIN_NODE_TREE_DEPTH; nTreeLevel++)
   for( int x=0; x<m_arrSecInfoPyramid[nSID][nTreeLevel].GetSize(); x++)
   for( int y=0; y<m_arrSecInfoPyramid[nSID][nTreeLevel].GetSize(); y++)
    m_arrSecInfoPyramid[nSID][nTreeLevel][x][y]->CompileObjects(STATIC_OBJECTS);
   }*/

void CTerrain::GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors)
{
	nLoadedSectors = m_nLoadedSectors;
	nTotalSectors = 0;
	int nSegs = GetMaxSegmentsCount();
	for (int nSID = 0; nSID < nSegs; ++nSID)
	{
		if (!m_pParentNodes[nSID])
			continue;
		int nSectors = CTerrain::GetSectorsTableSize(nSID);
		nTotalSectors = nSectors * nSectors;
	}
}

bool CTerrain::m_bOpenTerrainTextureFileNoLog = false;

bool CTerrain::OpenTerrainTextureFile(SCommonFileHeader& hdrDiffTexHdr, STerrainTextureFileHeader& hdrDiffTexInfo, const char* szFileName, uint8*& ucpDiffTexTmpBuffer, int& nDiffTexIndexTableSize, int nSID)
{
	if (!GetRenderer())
		return false;

	FUNCTION_PROFILER_3DENGINE;
	assert(nSID >= 0);

	m_arrBaseTexInfos[nSID].m_nDiffTexIndexTableSize = 0;

	if (GetParentNode(nSID))
		GetParentNode(nSID)->UnloadNodeTexture(true);
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
		CloseTerrainTextureFile(nSID);
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
		CloseTerrainTextureFile(nSID);
		PrintMessage("Error opening terrain texture file: invalid signature");
		return false;
	}

	if (hdrDiffTexHdr.version != FILEVERSION_TERRAIN_TEXTURE_FILE)
	{
		gEnv->pCryPak->FClose(fpDiffTexFile);
		fpDiffTexFile = 0;
		CloseTerrainTextureFile(nSID);
		if (!bNoLog)
			Error("Error opening terrain texture file: version error (you might need to regenerate the surface texture)");
		return false;
	}

	gEnv->pCryPak->FRead(&hdrDiffTexInfo, 1, fpDiffTexFile, m_eEndianOfTexture != GetPlatformEndian());

	memset(m_arrBaseTexInfos[nSID].m_TerrainTextureLayer, 0, sizeof(m_arrBaseTexInfos[nSID].m_TerrainTextureLayer));

	// layers
	for (uint32 dwI = 0; dwI < hdrDiffTexInfo.nLayerCount; ++dwI)
	{
		assert(dwI <= 2);
		if (dwI > 2) break;                     // too many layers

		gEnv->pCryPak->FRead(&m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[dwI], 1, fpDiffTexFile, m_eEndianOfTexture != GetPlatformEndian());

		PrintMessage("  TerrainLayer %d: TexFormat: %s, SectorTextureSize: %dx%d, SectorTextureDataSizeBytes: %d",
		             dwI, GetRenderer()->GetTextureFormatName(m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[dwI].eTexFormat),
		             m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[dwI].nSectorSizePixels, m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[dwI].nSectorSizePixels,
		             m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[dwI].nSectorSizeBytes);
	}

	// unlock all nodes
	for (int nTreeLevel = 0; nTreeLevel < TERRAIN_NODE_TREE_DEPTH; nTreeLevel++)
		for (int x = 0; x < m_arrSecInfoPyramid[nSID][nTreeLevel].GetSize(); x++)
			for (int y = 0; y < m_arrSecInfoPyramid[nSID][nTreeLevel].GetSize(); y++)
			{
#ifndef _RELEASE
				m_arrSecInfoPyramid[nSID][nTreeLevel][x][y]->m_eTextureEditingState = eTES_SectorIsUnmodified;
				m_arrSecInfoPyramid[nSID][nTreeLevel][x][y]->m_eElevTexEditingState = eTES_SectorIsUnmodified;
#endif // _RELEASE

				m_arrSecInfoPyramid[nSID][nTreeLevel][x][y]->m_nNodeTextureOffset = -1;
				m_arrSecInfoPyramid[nSID][nTreeLevel][x][y]->m_bMergeNotAllowed = false;
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
			CTerrainNode* const p = GetParentNode(nSID);
			if (p)
			{
				p->AssignTextureFileOffset(pIndices, nElementsLeft);
				assert(nElementsLeft == 0);
			}
		}

		nDiffTexIndexTableSize = wSize * sizeof(uint16) + sizeof(uint16);

		if (!m_bEditor)
		{
			FRAME_PROFILER("CTerrain::OpenTerrainTextureFile: ReleaseHoleNodes & UpdateTerrainNodes", GetSystem(), PROFILE_3DENGINE);

			int nNodesCounterBefore = CTerrainNode::m_nNodesCounter;
			GetParentNode(nSID)->ReleaseHoleNodes();
			PrintMessage("  %d out of %d nodes cleaned", nNodesCounterBefore - CTerrainNode::m_nNodesCounter, nNodesCounterBefore);

			if (Get3DEngine()->m_pObjectsTree[nSID])
				Get3DEngine()->m_pObjectsTree[nSID]->UpdateTerrainNodes();
		}
	}

	int nSectorHeightMapTextureDim = m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[1].nSectorSizePixels;

	delete[] ucpDiffTexTmpBuffer;
	ucpDiffTexTmpBuffer = new uint8[m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[0].nSectorSizeBytes + m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[1].nSectorSizeBytes + sizeof(float)*nSectorHeightMapTextureDim*nSectorHeightMapTextureDim];

	gEnv->pCryPak->FClose(fpDiffTexFile);
	fpDiffTexFile = 0;

	// if texture compression format is not supported by GPU - use uncompressed RGBA and decompress on CPU
	ETEX_Format eTexPoolFormat = m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[0].eTexFormat;

	if (!GetRenderer()->IsTextureFormatSupported(eTexPoolFormat))
	{
		GetLog()->LogWarning("Warning: Terrain texture compression format (%s) is not supported, fall back to CPU decompression", GetRenderer()->GetTextureFormatName(eTexPoolFormat));

		eTexPoolFormat = eTF_R8G8B8A8;
	}

	// init texture pools
	m_texCache[0].InitPool(0, m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[0].nSectorSizePixels, eTexPoolFormat);
	m_texCache[1].InitPool(0, m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[1].nSectorSizePixels, eTexPoolFormat);
	m_texCache[2].InitPool(0, nSectorHeightMapTextureDim, eTF_R32F);

	return true;
}
