// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain_node.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain node
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "Vegetation.h"
#include <CryMath/AABBSV.h>
#include "LightEntity.h"
#include <CryMath/MTPseudoRandom.h>

CProcVegetPoolMan* CTerrainNode::m_pProcObjPoolMan = NULL;
SProcObjChunkPool* CTerrainNode::m_pProcObjChunkPool = NULL;
int CTerrainNode::m_nNodesCounter = 0;

CTerrainNode* CTerrainNode::GetTexuringSourceNode(int nTexMML, eTexureType eTexType)
{
	CTerrainNode* pTexSourceNode = this;
	while (pTexSourceNode->m_pParent)
	{
		if (eTexType == ett_Diffuse)
		{
			if (pTexSourceNode->m_nTreeLevel < nTexMML || pTexSourceNode->m_nNodeTextureOffset < 0)
				pTexSourceNode = pTexSourceNode->m_pParent;
			else
				break;
		}
		else if (eTexType == ett_LM)
		{
			if (pTexSourceNode->m_nTreeLevel < nTexMML || pTexSourceNode->m_nNodeTextureOffset < 0 /*|| pTexSourceNode->GetSectorSizeInHeightmapUnits()<TERRAIN_LM_TEX_RES*/)
				pTexSourceNode = pTexSourceNode->m_pParent;
			else
				break;
		}
		else
			assert(0);

		assert(pTexSourceNode);
	}

	return pTexSourceNode;
}

CTerrainNode* CTerrainNode::GetReadyTexSourceNode(int nTexMML, eTexureType eTexType)
{
	Vec3 vSunDir = Get3DEngine()->GetSunDir().normalized();

	CTerrainNode* pTexSourceNode = this;
	while (pTexSourceNode)
	{
		if (eTexType == ett_Diffuse)
		{
			if (pTexSourceNode->m_nTreeLevel < nTexMML || !pTexSourceNode->m_nNodeTexSet.nTex0)
				pTexSourceNode = pTexSourceNode->m_pParent;
			else
				break;
		}
		else if (eTexType == ett_LM)
		{
			if (pTexSourceNode->m_nTreeLevel < nTexMML || !pTexSourceNode->m_nNodeTexSet.nTex1)
				pTexSourceNode = pTexSourceNode->m_pParent;
			else
				break;
		}
	}

	return pTexSourceNode;
}

void CTerrainNode::RequestTextures(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (passInfo.IsRecursivePass())
		return;

	// check if diffuse need to be updated
	if (CTerrainNode* pCorrectDiffSourceNode = GetTexuringSourceNode(m_cNodeNewTexMML, ett_Diffuse))
	{
		//    if(	pCorrectDiffSourceNode != pDiffSourceNode )
		{
			while (pCorrectDiffSourceNode)
			{
				GetTerrain()->ActivateNodeTexture(pCorrectDiffSourceNode, passInfo);
				pCorrectDiffSourceNode = pCorrectDiffSourceNode->m_pParent;
			}
		}
	}

	// check if lightmap need to be updated
	if (CTerrainNode* pCorrectLMapSourceNode = GetTexuringSourceNode(m_cNodeNewTexMML, ett_LM))
	{
		//    if(	pCorrectLMapSourceNode != pLMSourceNode )
		{
			while (pCorrectLMapSourceNode)
			{
				GetTerrain()->ActivateNodeTexture(pCorrectLMapSourceNode, passInfo);
				pCorrectLMapSourceNode = pCorrectLMapSourceNode->m_pParent;
			}
		}
	}
}

// setup texture id and texgen parameters
void CTerrainNode::SetupTexturing(bool bMakeUncompressedForEditing, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

#ifndef _RELEASE
	if (gEnv->IsEditor())
	{
		if (CTerrainNode* pTextureSourceNode = GetReadyTexSourceNode(passInfo.IsShadowPass() ? 0 : m_cNodeNewTexMML, ett_Diffuse))
		{
			// update RGB
			if (pTextureSourceNode->m_eTextureEditingState == eTES_SectorIsModified_AtlasIsDirty && C3DEngine::m_pEditorHeightmap)
			{
				pTextureSourceNode->UpdateNodeTextureFromEditorData();
				pTextureSourceNode->m_eTextureEditingState = eTES_SectorIsModified_AtlasIsUpToDate;
			}

			// update elevation texture and normal textures
			if (pTextureSourceNode->m_eElevTexEditingState == eTES_SectorIsModified_AtlasIsDirty && GetRenderer()->GetFrameID(false) >= (GetTerrain()->m_terrainPaintingFrameId + GetCVars()->e_TerrainEditPostponeTexturesUpdate))
			{
				pTextureSourceNode->UpdateNodeNormalMapFromHeightMap();

				static Array2d<float> arrHmData;
				pTextureSourceNode->FillSectorHeightMapTextureData(arrHmData);
				m_pTerrain->m_texCache[2].UpdateTexture((byte*)arrHmData.GetData(), pTextureSourceNode->m_nNodeTexSet.nSlot0);

				pTextureSourceNode->m_eElevTexEditingState = eTES_SectorIsModified_AtlasIsUpToDate;
			}
		}
	}
#endif // _RELEASE

	CheckLeafData();

	// find parent node containing requested texture lod
	CTerrainNode* pTextureSourceNode = GetReadyTexSourceNode(passInfo.IsShadowPass() ? 0 : m_cNodeNewTexMML, ett_Diffuse);

	if (!pTextureSourceNode) // at least root texture has to be loaded
		pTextureSourceNode = m_pTerrain->GetParentNode();

	int nRL = passInfo.GetRecursiveLevel();

	float fPrevTexGenScale = GetLeafData()->m_arrTexGen[nRL][2];

	if (pTextureSourceNode)
	{
		// set texture generation parameters for terrain diffuse map

		if (gEnv->IsEditor())
		{
			CalculateTexGen(pTextureSourceNode, GetLeafData()->m_arrTexGen[nRL][0], GetLeafData()->m_arrTexGen[nRL][1], GetLeafData()->m_arrTexGen[nRL][2]);

			pTextureSourceNode->m_nNodeTexSet.fTexOffsetX = GetLeafData()->m_arrTexGen[nRL][0];
			pTextureSourceNode->m_nNodeTexSet.fTexOffsetY = GetLeafData()->m_arrTexGen[nRL][1];
			pTextureSourceNode->m_nNodeTexSet.fTexScale = GetLeafData()->m_arrTexGen[nRL][2];
		}
		else
		{
			GetLeafData()->m_arrTexGen[nRL][0] = pTextureSourceNode->m_nNodeTexSet.fTexOffsetX;
			GetLeafData()->m_arrTexGen[nRL][1] = pTextureSourceNode->m_nNodeTexSet.fTexOffsetY;
			GetLeafData()->m_arrTexGen[nRL][2] = pTextureSourceNode->m_nNodeTexSet.fTexScale;
		}

		GetLeafData()->m_arrTexGen[nRL][3] = gEnv->p3DEngine->GetTerrainTextureMultiplier();
	}

	// set output texture id's
	int nDefaultTexId = GetTerrain()->m_nWhiteTexId;

	m_nTexSet.nTex0 = (pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex0) ?
	                  pTextureSourceNode->m_nNodeTexSet.nTex0 : nDefaultTexId;

	m_nTexSet.nTex1 = (pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex1) ?
	                  pTextureSourceNode->m_nNodeTexSet.nTex1 : nDefaultTexId;

	m_nTexSet.nTex2 = (pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex2) ?
	                  pTextureSourceNode->m_nNodeTexSet.nTex2 : nDefaultTexId;

	if (pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex0)
		m_nTexSet = pTextureSourceNode->m_nNodeTexSet;

	if (fPrevTexGenScale != GetLeafData()->m_arrTexGen[nRL][2])
		InvalidatePermanentRenderObject();
}

float GetPointToBoxDistance(Vec3 vPos, AABB bbox)
{
	if (vPos.x >= bbox.min.x && vPos.x <= bbox.max.x)
		if (vPos.y >= bbox.min.y && vPos.y <= bbox.max.y)
			if (vPos.z >= bbox.min.z && vPos.z <= bbox.max.z)
				return 0;
	// inside

	float dy;
	if (vPos.y < bbox.min.y)
		dy = bbox.min.y - vPos.y;
	else if (vPos.y > bbox.max.y)
		dy = vPos.y - bbox.max.y;
	else
		dy = 0;

	float dx;
	if (vPos.x < bbox.min.x)
		dx = bbox.min.x - vPos.x;
	else if (vPos.x > bbox.max.x)
		dx = vPos.x - bbox.max.x;
	else
		dx = 0;

	float dz;
	if (vPos.z < bbox.min.z)
		dz = bbox.min.z - vPos.z;
	else if (vPos.z > bbox.max.z)
		dz = vPos.z - bbox.max.z;
	else
		dz = 0;

	return sqrt_tpl(dx * dx + dy * dy + dz * dz);
}

// Hierarchically check nodes visibility
// Add visible sectors to the list of visible terrain sectors

bool CTerrainNode::CheckVis(bool bAllInside, bool bAllowRenderIntoCBuffer, const SRenderingPassInfo& passInfo, uint32 passCullMask)
{
	FUNCTION_PROFILER_3DENGINE;

	const AABB& boxWS = GetBBox();
	const CCamera& rCamera = passInfo.GetCamera();

	// get distances
	float distance = m_arrfDistance[passInfo.GetRecursiveLevel()] = GetPointToBoxDistance(rCamera.GetPosition(), boxWS);

	// check culling of all passes
	const float maxViewDistanceUnlimited = 1000000.f; // make sure max view distance check does not cull terrain sectors, only camera far plane is used for that
	passCullMask = COctreeNode::UpdateCullMask(m_onePassTraversalFrameId, m_onePassTraversalShadowCascades, 0, passInfo, boxWS, distance, maxViewDistanceUnlimited, false, bAllInside, &m_occlusionTestClient, passCullMask);

	// stop if no any passes see this node
	if (!passCullMask)
		return false;

	if (m_bHasHoles == 2)
		return false; // has no visible mesh

	// find LOD of this sector
	SetLOD(passInfo);

	m_nSetLodFrameId = passInfo.GetMainFrameID();

	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;

	const int posponeGeomErrorUpdateFrames = 5;
	if (m_geomError == kGeomErrorNotSet && GetRenderer()->GetFrameID(false) > (GetTerrain()->m_terrainPaintingFrameId + posponeGeomErrorUpdateFrames))
		UpdateGeomError();

	bool bContinueRecursion = false;
	if (m_pChilds && (GetCVars()->e_TerrainLodDistanceRatio * distance < (float)nSectorSize || GetCVars()->e_TerrainLodErrorRatio * distance < m_geomError))
		bContinueRecursion = true;

	if (bContinueRecursion)
	{
		Vec3 vCenter = boxWS.GetCenter();

		Vec3 vCamPos = rCamera.GetPosition();

		int nFirst =
		  ((vCamPos.x > vCenter.x) ? 2 : 0) |
		  ((vCamPos.y > vCenter.y) ? 1 : 0);

		m_pChilds[nFirst].CheckVis(bAllInside, bAllowRenderIntoCBuffer, passInfo, passCullMask);
		m_pChilds[nFirst ^ 1].CheckVis(bAllInside, bAllowRenderIntoCBuffer, passInfo, passCullMask);
		m_pChilds[nFirst ^ 2].CheckVis(bAllInside, bAllowRenderIntoCBuffer, passInfo, passCullMask);
		m_pChilds[nFirst ^ 3].CheckVis(bAllInside, bAllowRenderIntoCBuffer, passInfo, passCullMask);
	}
	else
	{
		if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
		{
			GetObjManager()->PushIntoCullQueue(SCheckOcclusionJobData::CreateTerrainJobData(this, boxWS, distance, passCullMask));
		}
		else
		{
			GetTerrain()->AddVisSector(this, passCullMask);
		}

		GetTerrain()->m_checkVisSectorsCount++;

		if (boxWS.min.z < GetTerrain()->GetWaterLevel() && passInfo.IsGeneralPass())
			if (distance < GetTerrain()->m_fDistanceToSectorWithWater)
				GetTerrain()->m_fDistanceToSectorWithWater = distance;

		RequestTextures(passInfo);
	}

	// update procedural vegetation
	if (passInfo.IsGeneralPass() && GetTerrain()->m_bProcVegetationInUse && GetCVars()->e_ProcVegetation && (passCullMask & kPassCullMainMask))
	{
		CTerrainNode* pNode = this;

		while (pNode && pNode->m_nTreeLevel < GetCVars()->e_ProcVegetationMaxCacheLevels)
		{
			if (pNode->m_arrfDistance[passInfo.GetRecursiveLevel()] < GetCVars()->e_ProcVegetationMaxViewDistance * (pNode->GetBBox().GetSize().x / 64.f))
				GetTerrain()->ActivateNodeProcObj(pNode);

			pNode = pNode->m_pParent;
		}
	}

	return true;
}

void CTerrainNode::Init(int x1, int y1, int nNodeSize, CTerrainNode* pParent, bool bBuildErrorsTable)
{
	m_pChilds = NULL;

	m_pProcObjPoolPtr = NULL;

	//	ZeroStruct(m_arrChilds);

	m_pReadStream = NULL;
	m_eTexStreamingStatus = ecss_NotLoaded;

	// flags
	m_bNoOcclusion = 0;
	m_bProcObjectsReady = 0;
	m_bHasHoles = 0;

#ifndef _RELEASE
	m_eTextureEditingState = m_eElevTexEditingState = eTES_SectorIsUnmodified;
#endif // _RELEASE

	m_nOriginX = m_nOriginY = 0; // sector origin
	m_nLastTimeUsed = 0;         // basically last time rendered

	uint8 m_cNodeNewTexMML = m_cNodeNewTexMML_Min = 0;

	m_pLeafData = 0;

	m_nTreeLevel = 0;

	ZeroStruct(m_nNodeTexSet);
	ZeroStruct(m_nTexSet);

	//m_nNodeRenderLastFrameId=0;
	m_nNodeTextureLastUsedSec4 = (~0);
	m_boxHeigtmapLocal.Reset();
	m_fBBoxExtentionByObjectsIntegration = 0;
	m_pParent = NULL;
	//m_nSetupTexGensFrameId=0;

	m_nLastTimeUsed = (int)GetCurTimeSec() + 20;

	m_cNodeNewTexMML = 100;
	m_pParent = NULL;

	m_pParent = pParent;

	m_nOriginX = x1;
	m_nOriginY = y1;

	m_rangeInfo.fOffset = 0;
	m_rangeInfo.fRange = 0;
	m_rangeInfo.nSize = 0;
	m_rangeInfo.pHMData = NULL;
	m_rangeInfo.pSTPalette = NULL;

	for (int iStackLevel = 0; iStackLevel < MAX_RECURSION_LEVELS; iStackLevel++)
	{
		m_arrfDistance[iStackLevel] = 0.f;
	}

	if (nNodeSize == CTerrain::GetSectorSize())
	{
		//		memset(m_arrChilds,0,sizeof(m_arrChilds));
		m_nTreeLevel = 0;
		/*		InitSectorBoundsAndErrorLevels(bBuildErrorsTable);

		    static int tic=0;
		    if((tic&511)==0)
		    {
		      PrintMessagePlus(".");								// to visualize progress
		      PrintMessage("");
		    }
		    tic++;
		 */
	}
	else
	{
		int nSize = nNodeSize / 2;
		m_pChilds = new CTerrainNode[4];
		m_pChilds[0].Init(x1, y1, nSize, this, bBuildErrorsTable);
		m_pChilds[1].Init(x1 + nSize, y1, nSize, this, bBuildErrorsTable);
		m_pChilds[2].Init(x1, y1 + nSize, nSize, this, bBuildErrorsTable);
		m_pChilds[3].Init(x1 + nSize, y1 + nSize, nSize, this, bBuildErrorsTable);
		m_nTreeLevel = m_pChilds[0].m_nTreeLevel + 1;

		m_boxHeigtmapLocal.min = SetMaxBB();
		m_boxHeigtmapLocal.max = SetMinBB();

		for (int nChild = 0; nChild < 4; nChild++)
		{
			m_boxHeigtmapLocal.min.CheckMin(m_pChilds[nChild].m_boxHeigtmapLocal.min);
			m_boxHeigtmapLocal.max.CheckMax(m_pChilds[nChild].m_boxHeigtmapLocal.max);
		}

		m_arrfDistance[0] = 2.f * CTerrain::GetTerrainSize();
		if (m_arrfDistance[0] < 0)
			m_arrfDistance[0] = 0;

		m_nLastTimeUsed = ~0;//GetCurTimeSec() + 100;
	}

	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;
	assert(x1 >= 0 && y1 >= 0 && x1 < CTerrain::GetTerrainSize() && y1 < CTerrain::GetTerrainSize());
	GetTerrain()->m_arrSecInfoPyramid[m_nTreeLevel][x1 / nSectorSize][y1 / nSectorSize] = this;

	m_dwRndFlags |= (ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS);
	m_fWSMaxViewDist = 1000000.f;
}

CTerrainNode::~CTerrainNode()
{
	Get3DEngine()->FreeRenderNodeState(this);
	CRY_ASSERT(!m_pTempData.load());

	if (GetTerrain()->m_pTerrainUpdateDispatcher)
		GetTerrain()->m_pTerrainUpdateDispatcher->RemoveJob(this);

	Get3DEngine()->OnCasterDeleted(this);

	ReleaseHeightMapGeometry();

	UnloadNodeTexture(false);

	SAFE_DELETE_ARRAY(m_pChilds);

	RemoveProcObjects(false);

	m_bHasHoles = 0;

	delete m_pLeafData;
	m_pLeafData = NULL;

	delete[] m_rangeInfo.pHMData;
	m_rangeInfo.pHMData = NULL;

	delete[] m_rangeInfo.pSTPalette;
	m_rangeInfo.pSTPalette = NULL;

	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;
	assert(m_nOriginX < CTerrain::GetTerrainSize() && m_nOriginY < CTerrain::GetTerrainSize());
	GetTerrain()->m_arrSecInfoPyramid[m_nTreeLevel][m_nOriginX / nSectorSize][m_nOriginY / nSectorSize] = NULL;

	m_nNodesCounter--;
}

void CTerrainNode::CheckLeafData()
{
	if (!m_pLeafData)
		m_pLeafData = new STerrainNodeLeafData;
}

PodArray<byte> gTerrainCompressedImgData;

void CTerrainNode::SaveCompressedMipmapLevel(const void* data, size_t size, void* userData)
{
	gTerrainCompressedImgData.Clear();
	gTerrainCompressedImgData.AddList((byte*)data, size);
}

void CTerrainNode::EnableTextureEditingMode(unsigned int nEditorDiffuseTex)
{
	FUNCTION_PROFILER_3DENGINE;

#ifndef _RELEASE
	if (nEditorDiffuseTex > 0)
		m_eTextureEditingState = eTES_SectorIsModified_AtlasIsDirty;
#endif // _RELEASE
}

void CTerrainNode::UpdateNodeTextureFromEditorData()
{
	FUNCTION_PROFILER_3DENGINE;

	int texDim = GetTerrain()->m_arrBaseTexInfos.m_TerrainTextureLayer[0].nSectorSizePixels;
	ETEX_Format texFormat = GetTerrain()->m_arrBaseTexInfos.m_TerrainTextureLayer[0].eTexFormat;

	static Array2d<ColorB> arrRGB;
	arrRGB.Allocate(texDim);

	// prepare lookup table with terrain color multiplier pre-integrated in linear space
	const int CRY_ALIGN(128) lookupTableSize = 128;
	byte lookupTable[lookupTableSize];
	float rgbMult = 1.f / gEnv->p3DEngine->GetTerrainTextureMultiplier();
	ColorF colR;
	for (int i = 0; i < lookupTableSize; i++)
	{
		colR.r = 1.f / float(lookupTableSize - 1) * float(i);
		colR.srgb2rgb();
		colR *= rgbMult;
		colR.Clamp();
		colR.rgb2srgb();
		lookupTable[i] = SATURATEB((uint32)(colR.r * 255.0f + 0.5f));
	}

	float sectorSizeM = GetBBox().GetSize().x;

	int smallestEditorTileSizeM = GetTerrain()->GetTerrainSize() / 16;

	ColorB* colors = (ColorB*)alloca(sizeof(ColorB) * texDim);

	float pixToMeter = sectorSizeM / texDim * (1.f + 1.f / (float)texDim);

	// copy by row
	for (int x = 0; x < texDim; x++)
	{
		C3DEngine::m_pEditorHeightmap->GetColorAtPosition(
		  (float)m_nOriginY + float(0) * pixToMeter, (float)m_nOriginX + float(x) * pixToMeter, colors, texDim, pixToMeter);

		ColorB* dst = &arrRGB[x][0];
		ColorB* dstEnd = dst + texDim;
		ColorB* src = colors;

		while (dst < dstEnd)
		{
			dst->r = lookupTable[src->b >> 1];
			dst->g = lookupTable[src->g >> 1];
			dst->b = lookupTable[src->r >> 1];
			dst->a = 255;
			++dst;
			++src;
		}
	}

	GetRenderer()->DXTCompress((byte*)arrRGB.GetData(), texDim, texDim, texFormat, false, false, 4, SaveCompressedMipmapLevel);

	m_pTerrain->m_texCache[0].UpdateTexture(gTerrainCompressedImgData.GetElements(), m_nNodeTexSet.nSlot0);
}

void CTerrainNode::UpdateNodeNormalMapFromHeightMap()
{
	FUNCTION_PROFILER_3DENGINE;

	int nTexSize = GetTerrain()->m_arrBaseTexInfos.m_TerrainTextureLayer[1].nSectorSizePixels;
	ETEX_Format texFormat = GetTerrain()->m_arrBaseTexInfos.m_TerrainTextureLayer[1].eTexFormat;

	static Array2d<ColorB> arrRGB;
	arrRGB.Allocate(nTexSize);

	float fBoxSize = GetBBox().GetSize().x;

	for (int x = 0; x < nTexSize; x++)
	{
		for (int y = 0; y < nTexSize; y++)
		{
			Vec3 vWSPos(
			  (float)m_nOriginX + fBoxSize * float(x) / nTexSize * (1.f + 1.f / (float)nTexSize),
			  (float)m_nOriginY + fBoxSize * float(y) / nTexSize * (1.f + 1.f / (float)nTexSize), 0);

			Vec3 vNormal = GetTerrain()->GetTerrainSurfaceNormal_Int(vWSPos.x, vWSPos.y);

			uint32 dwR = SATURATEB(uint32(vNormal.x * 127.5f + 127.5f));
			uint32 dwB = SATURATEB(uint32(vNormal.y * 127.5f + 127.5f));
			uint32 dwA = 0;
			uint32 dwG = 0;

			arrRGB[x][y] = (dwA << 24) | (dwB << 16) | (dwG << 8) | (dwR);
		}
	}

	GetRenderer()->DXTCompress((byte*)arrRGB.GetData(), nTexSize, nTexSize, texFormat, false, false, 4, SaveCompressedMipmapLevel);

	m_pTerrain->m_texCache[1].UpdateTexture(gTerrainCompressedImgData.GetElements(), m_nNodeTexSet.nSlot1);
}

void CTerrainNode::CheckNodeGeomUnload(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	float fDistanse = GetPointToBoxDistance(passInfo.GetCamera().GetPosition(), GetBBox()) * passInfo.GetZoomFactor();

	int nTime = fastftol_positive(GetCurTimeSec());

	// support timer reset
	m_nLastTimeUsed = min(m_nLastTimeUsed, nTime);

	if (m_nLastTimeUsed < (nTime - 16) && fDistanse > 512)
	{
		// try to release vert buffer if not in use int time
		ReleaseHeightMapGeometry();
	}
}

void CTerrainNode::RemoveProcObjects(bool bRecursive, bool bReleaseAllObjects)
{
	FUNCTION_PROFILER_3DENGINE;

	// remove procedurally placed objects
	if (m_bProcObjectsReady)
	{
		if (m_pProcObjPoolPtr && bReleaseAllObjects)
		{
			m_pProcObjPoolPtr->ReleaseAllObjects();
			m_pProcObjPoolMan->ReleaseObject(m_pProcObjPoolPtr);
			m_pProcObjPoolPtr = NULL;
		}

		m_bProcObjectsReady = false;
	}

	if (bRecursive && m_pChilds)
		for (int i = 0; i < 4; i++)
			m_pChilds[i].RemoveProcObjects(bRecursive);
}

void CTerrainNode::RenderNodeHeightmap(const SRenderingPassInfo& passInfo, uint32 passCullMask)
{
	FUNCTION_PROFILER_3DENGINE;
	bool bMeshIsUpToDate = true; // actually bUpdateNOTRequired

	m_nLastTimeUsed = fastftol_positive(GetCurTimeSec());

	assert(GetVisAreaManager()->IsOutdoorAreasVisible());

	const CCamera& rCamera = passInfo.GetCamera();
	if (GetCVars()->e_TerrainDrawThisSectorOnly)
	{
		if (
		  rCamera.GetPosition().x > GetBBox().max.x ||
		  rCamera.GetPosition().x < GetBBox().min.x ||
		  rCamera.GetPosition().y > GetBBox().max.y ||
		  rCamera.GetPosition().y < GetBBox().min.y)
			return;
	}

	SetupTexturing(false, passInfo);

	if (passCullMask & kPassCullMainMask)
	{
		bMeshIsUpToDate = RenderSector(passInfo);
	}

	if (passCullMask & ~kPassCullMainMask)
	{
		COctreeNode::RenderObjectIntoShadowViews(passInfo, 0, this, GetBBox(), passCullMask);
	}

	if (GetCVars()->e_TerrainBBoxes)
	{
		ColorB colour = ColorB(255 * ((m_nTreeLevel & 1) > 0), 255 * ((m_nTreeLevel & 2) > 0), 255, 255);
		GetRenderer()->GetIRenderAuxGeom()->DrawAABB(GetBBox(), false, colour, eBBD_Faceted);
		if (GetCVars()->e_TerrainBBoxes == 3)
		{
			if (m_rangeInfo.nSize)
				IRenderAuxText::DrawLabelF(GetBBox().GetCenter(), 2, "%dx%d, %.1f", m_rangeInfo.nSize, m_rangeInfo.nSize, m_geomError);
			else
				IRenderAuxText::DrawLabelF(GetBBox().GetCenter(), 2, "%.1f", m_geomError);
		}
	}

	if (GetCVars()->e_TerrainDetailMaterialsDebug)
	{
		int nLayersNum = 0;
		for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
		{
			if (m_lstSurfaceTypeInfo[i].HasRM() && m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial())
				if (m_lstSurfaceTypeInfo[i].GetIndexCount())
					nLayersNum++;
		}

		if (nLayersNum >= GetCVars()->e_TerrainDetailMaterialsDebug)
		{
			GetRenderer()->GetIRenderAuxGeom()->DrawAABB(GetBBox(), false, ColorB(255 * ((nLayersNum & 1) > 0), 255 * ((nLayersNum & 2) > 0), 255, 255), eBBD_Faceted);
			IRenderAuxText::DrawLabelF(GetBBox().GetCenter(), 2, "%d", nLayersNum);
		}
	}

	// pre-cache surface types
	if (passCullMask & kPassCullMainMask)
	{
		for (int s = 0; s < Cry3DEngineBase::GetTerrain()->m_SSurfaceType.Count(); s++)
		{
			SSurfaceType* pSurf = &Cry3DEngineBase::GetTerrain()->m_SSurfaceType[s];

			if (pSurf->HasMaterial())
			{
				uint8 szProj[] = "XYZ";

				for (int p = 0; p < 3; p++)
				{
					if (CMatInfo* pMatInfo = (CMatInfo*)(IMaterial*)pSurf->GetMaterialOfProjection(szProj[p]))
					{
						pMatInfo->PrecacheMaterial(GetDistance(passInfo), NULL, GetDistance(passInfo) < 32.f);
					}
				}
			}
		}

		if (!bMeshIsUpToDate)
		{
			m_pTerrain->m_pTerrainUpdateDispatcher->QueueJob(this, passInfo);
		}
	}
}

float CTerrainNode::GetSurfaceTypeAmount(Vec3 vPos, int nSurfType)
{
	float unitSize = (float)GetTerrain()->GetHeightMapUnitSize();
	vPos *= 1.f / unitSize;

	int x1 = int(vPos.x);
	int y1 = int(vPos.y);
	int x2 = int(vPos.x + 1);
	int y2 = int(vPos.y + 1);

	float dx = vPos.x - x1;
	float dy = vPos.y - y1;

	float s00 = GetTerrain()->GetSurfaceTypeID((x1 * unitSize), (y1 * unitSize)) == nSurfType;
	float s01 = GetTerrain()->GetSurfaceTypeID((x1 * unitSize), (y2 * unitSize)) == nSurfType;
	float s10 = GetTerrain()->GetSurfaceTypeID((x2 * unitSize), (y1 * unitSize)) == nSurfType;
	float s11 = GetTerrain()->GetSurfaceTypeID((x2 * unitSize), (y2 * unitSize)) == nSurfType;

	if (s00 || s01 || s10 || s11)
	{
		float s0 = s00 * (1.f - dy) + s01 * dy;
		float s1 = s10 * (1.f - dy) + s11 * dy;
		float res = s0 * (1.f - dx) + s1 * dx;
		return res;
	}

	return 0;
}

bool CTerrainNode::CheckUpdateProcObjects(const SRenderingPassInfo& passInfo)
{
	if (GetCVars()->e_TerrainDrawThisSectorOnly)
	{
		const CCamera& rCamera = passInfo.GetCamera();
		if (
		  rCamera.GetPosition().x > GetBBox().max.x ||
		  rCamera.GetPosition().x < GetBBox().min.x ||
		  rCamera.GetPosition().y > GetBBox().max.y ||
		  rCamera.GetPosition().y < GetBBox().min.y)
			return false;
	}

	if (m_bProcObjectsReady)
		return false;

	FUNCTION_PROFILER_3DENGINE;

	if (m_pProcObjPoolPtr)
	{
		m_pProcObjPoolPtr->ReleaseAllObjects();
		m_pProcObjPoolMan->ReleaseObject(m_pProcObjPoolPtr);
		m_pProcObjPoolPtr = NULL;
	}

	int nInstancesCounter = 0;

	CMTRand_int32 rndGen(gEnv->bNoRandomSeed ? 0 : m_nOriginX + m_nOriginY);

	float nSectorSize = (float)(CTerrain::GetSectorSize() << m_nTreeLevel);
	for (int nLayer = 0; nLayer < m_lstSurfaceTypeInfo.Count(); nLayer++)
	{
		for (int g = 0; g < m_lstSurfaceTypeInfo[nLayer].pSurfaceType->lstnVegetationGroups.Count(); g++)
			if (m_lstSurfaceTypeInfo[nLayer].pSurfaceType->lstnVegetationGroups[g] >= 0)
			{
				int nGroupId = m_lstSurfaceTypeInfo[nLayer].pSurfaceType->lstnVegetationGroups[g];
				assert(nGroupId >= 0);
				if (nGroupId < 0)
					continue;
				StatInstGroup* pGroup = std::find_if(GetObjManager()->m_lstStaticTypes.begin(), GetObjManager()->m_lstStaticTypes.end(), [nGroupId](StatInstGroup& a) { return a.nID == nGroupId; });
				if (pGroup == GetObjManager()->m_lstStaticTypes.end() || !pGroup->GetStatObj() || pGroup->fSize <= 0)
					continue;

				if (!CheckMinSpec(pGroup->minConfigSpec)) // Check min spec of this group.
					continue;

				// distribute objects into different tree levels based on object size

				float fSectorViewDistMax = GetCVars()->e_ProcVegetationMaxViewDistance * (GetBBox().GetSize().x / 64.f);
				float fSectorViewDistMin = fSectorViewDistMax * 0.5f;
				float fVegMaxViewDist = pGroup->fVegRadius * pGroup->fSize * pGroup->fMaxViewDistRatio * GetCVars()->e_ViewDistRatioVegetation;

				if (fVegMaxViewDist > fSectorViewDistMax && m_nTreeLevel < (GetCVars()->GetCVars()->e_ProcVegetationMaxCacheLevels - 1))
					continue;
				if (fVegMaxViewDist < fSectorViewDistMin && m_nTreeLevel > 0)
					continue;

				if (GetCVars()->e_ProcVegetation >= 3)
					if (m_nTreeLevel != GetCVars()->e_ProcVegetation - 3)
						continue;

				if (pGroup->fDensity < 0.5f)
					pGroup->fDensity = 0.5f;

				float fMinX = (float)m_nOriginX;
				float fMinY = (float)m_nOriginY;
				float fMaxX = (float)(m_nOriginX + nSectorSize);
				float fMaxY = (float)(m_nOriginY + nSectorSize);

				for (float fX = fMinX; fX < fMaxX; fX += pGroup->fDensity)
				{
					for (float fY = fMinY; fY < fMaxY; fY += pGroup->fDensity)
					{
						Vec3 vPos(fX + (rndGen.GenerateFloat() - 0.5f) * pGroup->fDensity, fY + (rndGen.GenerateFloat() - 0.5f) * pGroup->fDensity, 0);
						vPos.x = CLAMP(vPos.x, fMinX, fMaxX);
						vPos.y = CLAMP(vPos.y, fMinY, fMaxY);

						{
							// filtered surface type lockup
							float fSurfaceTypeAmount = GetSurfaceTypeAmount(vPos, m_lstSurfaceTypeInfo[nLayer].pSurfaceType->ucThisSurfaceTypeId);
							if (fSurfaceTypeAmount <= 0.5)
								continue;

							vPos.z = GetTerrain()->GetZApr(vPos.x, vPos.y);
						}

						Vec3 vWPos = /*GetTerrain()->m_arrSegmentOrigns + */ vPos;
						if (vWPos.x < 0 || vWPos.x >= CTerrain::GetTerrainSize() || vWPos.y < 0 || vWPos.y >= CTerrain::GetTerrainSize())
							continue;

						float fScale = pGroup->fSize + (rndGen.GenerateFloat() - 0.5f) * pGroup->fSizeVar;
						if (fScale <= 0)
							continue;

						if (vWPos.z < pGroup->fElevationMin || vWPos.z > pGroup->fElevationMax)
							continue;

						// check slope range
						if (pGroup->fSlopeMin != 0 || pGroup->fSlopeMax != 255)
						{
							float stepSize = CTerrain::GetHeightMapUnitSize();
							float x = fX;
							float y = fY;

							// calculate surface normal
							float sx;
							if ((x + stepSize) < CTerrain::GetTerrainSize() && x >= stepSize)
								sx = GetTerrain()->GetZ(x + stepSize, y) - GetTerrain()->GetZ(x - stepSize, y);
							else
								sx = 0;

							float sy;
							if ((y + stepSize) < CTerrain::GetTerrainSize() && y >= stepSize)
								sy = GetTerrain()->GetZ(x, y + stepSize) - GetTerrain()->GetZ(x, y - stepSize);
							else
								sy = 0;

							Vec3 vNormal = Vec3(-sx, -sy, stepSize * 2.0f);
							vNormal.NormalizeFast();

							float fSlope = (1 - vNormal.z) * 255;
							if (fSlope < pGroup->fSlopeMin || fSlope > pGroup->fSlopeMax)
								continue;
						}

						if (pGroup->fVegRadius * fScale < GetCVars()->e_VegetationMinSize)
							continue; // skip creation of very small objects

						if (!m_pProcObjPoolPtr)
							m_pProcObjPoolPtr = m_pProcObjPoolMan->GetObject();

						if (!m_pProcObjPoolPtr)
						{
							m_bProcObjectsReady = false;
							return true;
						}

						CVegetation* pEnt = m_pProcObjPoolPtr->AllocateProcObject();
						assert(pEnt);

						pEnt->SetScale(fScale);
						pEnt->m_vPos = vWPos;

						int nGroupIndex = static_cast<int>(std::distance(GetObjManager()->m_lstStaticTypes.begin(), pGroup));
						pEnt->SetStatObjGroupIndex(nGroupIndex);

						const uint32 nRnd = rndGen.GenerateUint32();
						const bool bRandomRotation = pGroup->bRandomRotation;
						const int32 nRotationRange = pGroup->nRotationRangeToTerrainNormal;

						if (bRandomRotation || nRotationRange >= 360)
						{
							pEnt->m_ucAngle = static_cast<byte>(nRnd);
						}
						else if (nRotationRange > 0)
						{
							const Vec3 vTerrainNormal = Get3DEngine()->GetTerrainSurfaceNormal(vPos);

							if (abs(vTerrainNormal.x) == 0.f && abs(vTerrainNormal.y) == 0.f)
							{
								pEnt->m_ucAngle = static_cast<byte>(nRnd);
							}
							else
							{
								const float rndDegree = (float)-nRotationRange * 0.5f + (float)(nRnd % (uint32)nRotationRange);
								const float finaleDegree = RAD2DEG(atan2f(vTerrainNormal.y, vTerrainNormal.x)) + rndDegree;
								pEnt->m_ucAngle = (byte)((finaleDegree / 360.0f) * 255.0f);
							}
						}

						AABB aabb = pEnt->CalcBBox();

						pEnt->SetRndFlags(ERF_PROCEDURAL, true);

						pEnt->m_fWSMaxViewDist = pEnt->GetMaxViewDist(); // note: duplicated

						pEnt->UpdateRndFlags();

						pEnt->Physicalize(true);

						float fObjRadius = aabb.GetRadius();
						if (fObjRadius > MAX_VALID_OBJECT_VOLUME || !_finite(fObjRadius) || fObjRadius <= 0)
						{
							Warning("CTerrainNode::CheckUpdateProcObjects: Object has invalid bbox: %s,%s, GetRadius() = %.2f",
							        pEnt->GetName(), pEnt->GetEntityClassName(), fObjRadius);
						}

						Get3DEngine()->m_pObjectsTree->InsertObject(pEnt, aabb, fObjRadius, aabb.GetCenter());

						nInstancesCounter++;
						if (nInstancesCounter >= (MAX_PROC_OBJ_CHUNKS_NUM / MAX_PROC_SECTORS_NUM) * GetCVars()->e_ProcVegetationMaxObjectsInChunk)
						{
							m_bProcObjectsReady = true;
							return true;
						}
					}
				}
			}
	}

	m_bProcObjectsReady = true;

	//	GetISystem()->VTunePause();
	return true;
}

CVegetation* CProcObjSector::AllocateProcObject()
{
	FUNCTION_PROFILER_3DENGINE;

	// find pool id
	int nLastPoolId = m_nProcVegetNum / GetCVars()->e_ProcVegetationMaxObjectsInChunk;
	if (nLastPoolId >= m_ProcVegetChunks.Count())
	{
		//PrintMessage("CTerrainNode::AllocateProcObject: Sector(%d,%d) %d objects",m_nOriginX,m_nOriginY,m_nProcVegetNum);
		m_ProcVegetChunks.PreAllocate(nLastPoolId + 1, nLastPoolId + 1);
		SProcObjChunk* pChunk = m_ProcVegetChunks[nLastPoolId] = CTerrainNode::m_pProcObjChunkPool->GetObject();

		// init objects
		for (int o = 0; o < GetCVars()->e_ProcVegetationMaxObjectsInChunk; o++)
			pChunk->m_pInstances[o].Init();
	}

	// find empty slot id and return pointer to it
	int nNextSlotInPool = m_nProcVegetNum - nLastPoolId * GetCVars()->e_ProcVegetationMaxObjectsInChunk;
	CVegetation* pObj = &(m_ProcVegetChunks[nLastPoolId]->m_pInstances)[nNextSlotInPool];
	m_nProcVegetNum++;
	return pObj;
}

void CProcObjSector::ReleaseAllObjects()
{
	for (int i = 0; i < m_ProcVegetChunks.Count(); i++)
	{
		SProcObjChunk* pChunk = m_ProcVegetChunks[i];
		for (int o = 0; o < pChunk->nAllocatedItems; o++)
			pChunk->m_pInstances[o].ShutDown();
		CTerrainNode::m_pProcObjChunkPool->ReleaseObject(m_ProcVegetChunks[i]);
	}
	m_ProcVegetChunks.Clear();
	m_nProcVegetNum = 0;
}

void CProcObjSector::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));

	pSizer->AddObject(m_ProcVegetChunks);

	for (int i = 0; i < m_ProcVegetChunks.Count(); i++)
		pSizer->AddObject(m_ProcVegetChunks[i], sizeof(CVegetation) * GetCVars()->e_ProcVegetationMaxObjectsInChunk);
}

CProcObjSector::~CProcObjSector()
{
	FUNCTION_PROFILER_3DENGINE;

	ReleaseAllObjects();
}

void SProcObjChunk::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));

	if (m_pInstances)
		pSizer->AddObject(m_pInstances, sizeof(CVegetation) * GetCVars()->e_ProcVegetationMaxObjectsInChunk);
}

void CTerrainNode::IntersectTerrainAABB(const AABB& aabbBox, PodArray<CTerrainNode*>& lstResult)
{
	if (GetBBox().IsIntersectBox(aabbBox))
	{
		lstResult.Add(this);
		if (m_pChilds)
			for (int i = 0; i < 4; i++)
				m_pChilds[i].IntersectTerrainAABB(aabbBox, lstResult);
	}
}

/*
   int CTerrainNode__Cmp_SSurfaceType_UnitsNum(const void* v1, const void* v2)
   {
   SSurfaceType * p1 = *(SSurfaceType **)v1;
   SSurfaceType * p2 = *(SSurfaceType **)v2;

   assert(p1 && p2);

   if(p1->nUnitsNum > p2->nUnitsNum)
    return -1;
   else if(p1->nUnitsNum < p2->nUnitsNum)
    return 1;

   return 0;
   }
 */
/*
   int CTerrainNode__Cmp_TerrainSurfaceLayerAmount_UnitsNum(const void* v1, const void* v2)
   {
   TerrainSurfaceLayerAmount * p1 = (TerrainSurfaceLayerAmount *)v1;
   TerrainSurfaceLayerAmount * p2 = (TerrainSurfaceLayerAmount *)v2;

   assert(p1 && p2);

   if(p1->nUnitsNum > p2->nUnitsNum)
    return -1;
   else if(p1->nUnitsNum < p2->nUnitsNum)
    return 1;

   return 0;
   }
 */
void CTerrainNode::UpdateDetailLayersInfo(bool bRecursive)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_pChilds)
	{
		// accumulate surface info from childs
		if (bRecursive)
		{
			for (int nChild = 0; nChild < 4; nChild++)
				m_pChilds[nChild].UpdateDetailLayersInfo(bRecursive);
		}

		m_bHasHoles = 0;

		static PodArray<SSurfaceType*> lstChildsLayers;
		lstChildsLayers.Clear();
		for (int nChild = 0; nChild < 4; nChild++)
		{
			if (m_pChilds[nChild].m_bHasHoles)
				m_bHasHoles = 1;

			for (int i = 0; i < m_pChilds[nChild].m_lstSurfaceTypeInfo.Count(); i++)
			{
				SSurfaceType* pType = m_pChilds[nChild].m_lstSurfaceTypeInfo[i].pSurfaceType;
				assert(pType);
				if (lstChildsLayers.Find(pType) < 0 && pType->HasMaterial())
					lstChildsLayers.Add(pType);
			}
		}

		if (m_bHasHoles)
			if (m_pChilds[0].m_bHasHoles == 2)
				if (m_pChilds[1].m_bHasHoles == 2)
					if (m_pChilds[2].m_bHasHoles == 2)
						if (m_pChilds[3].m_bHasHoles == 2)
							m_bHasHoles = 2;

		for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
			m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());
		m_lstSurfaceTypeInfo.Clear();

		m_lstSurfaceTypeInfo.PreAllocate(lstChildsLayers.Count());

		assert(lstChildsLayers.Count() <= SRangeInfo::e_max_surface_types);

		for (int i = 0; i < lstChildsLayers.Count(); i++)
		{
			SSurfaceTypeInfo si;
			si.pSurfaceType = lstChildsLayers[i];
			m_lstSurfaceTypeInfo.Add(si);
		}
	}
	else
	{
		// find all used surface types
		int arrSurfaceTypesInSector[SRangeInfo::e_max_surface_types];
		for (int i = 0; i < SRangeInfo::e_max_surface_types; i++)
			arrSurfaceTypesInSector[i] = 0;

		m_bHasHoles = 0;
		bool hasOnlyHoles = true;

		for (float X = m_nOriginX; X <= m_nOriginX + CTerrain::GetSectorSize(); X += CTerrain::GetHeightMapUnitSize())
		{
			for (float Y = m_nOriginY; Y <= m_nOriginY + CTerrain::GetSectorSize(); Y += CTerrain::GetHeightMapUnitSize())
			{
				const SSurfaceTypeItem& st = GetTerrain()->GetSurfaceTypeItem(X, Y);

				if (st.GetHole())
				{
					m_bHasHoles = 1;
				}
				else
				{
					hasOnlyHoles = false;
				}

				for (int s = 0; s < SSurfaceTypeItem::kMaxSurfaceTypesNum; s++)
				{
					if (st.we[s])
					{
						CRY_ASSERT(st.ty[s] < SRangeInfo::e_max_surface_types);
						arrSurfaceTypesInSector[st.ty[s]]++;
					}
				}
			}
		}

		if (hasOnlyHoles)
			m_bHasHoles = 2; // only holes

		for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); i++)
			m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());
		m_lstSurfaceTypeInfo.Clear();

		int nSurfCount = 0;
		for (int i = 0; i < SRangeInfo::e_max_surface_types; i++)
			if (arrSurfaceTypesInSector[i])
			{
				SSurfaceTypeInfo si;
				si.pSurfaceType = &GetTerrain()->m_SSurfaceType[i];
				if (si.pSurfaceType->HasMaterial())
					nSurfCount++;
			}

		m_lstSurfaceTypeInfo.PreAllocate(nSurfCount);

		for (int i = 0; i < SRangeInfo::e_max_surface_types; i++)
			if (arrSurfaceTypesInSector[i])
			{
				SSurfaceTypeInfo si;
				si.pSurfaceType = &GetTerrain()->m_SSurfaceType[i];
				if (si.pSurfaceType->HasMaterial())
					m_lstSurfaceTypeInfo.Add(si);
			}

		// If necessary, fix the sector palette by removing any unused/wrong entries:
		if (m_rangeInfo.pSTPalette)
		{
			for (int i = 0; i < SRangeInfo::e_index_undefined; ++i)
			{
				const uchar sType = m_rangeInfo.pSTPalette[i];

				if (sType < SRangeInfo::e_undefined && !arrSurfaceTypesInSector[sType] ||
				    !GetTerrain()->m_SSurfaceType[sType].HasMaterial())
					m_rangeInfo.pSTPalette[i] = SRangeInfo::e_undefined;
			}
		}
	}
}

void CTerrainNode::IntersectWithShadowFrustum(bool bAllIn, PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, const float fHalfGSMBoxSize, const SRenderingPassInfo& passInfo)
{
	if (bAllIn || (pFrustum && pFrustum->IntersectAABB(GetBBox(), &bAllIn)))
	{
		float fSectorSize = GetBBox().max.x - GetBBox().min.x;
		if (m_pChilds && (fSectorSize * GetCVars()->e_TerrainMeshInstancingShadowLodRatio > fHalfGSMBoxSize || (m_nTreeLevel > GetCVars()->e_TerrainMeshInstancingMinLod && pFrustum->IsCached())))
		{
			for (int i = 0; i < 4; i++)
				m_pChilds[i].IntersectWithShadowFrustum(bAllIn, plstResult, pFrustum, fHalfGSMBoxSize, passInfo);
		}
		else
		{
			plstResult->Add(this);
		}
	}
}

void CTerrainNode::IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult)
{
	if (aabbBox.IsIntersectBox(GetBBox()))
	{
		for (int i = 0; m_pChilds && i < 4; i++)
			m_pChilds[i].IntersectWithBox(aabbBox, plstResult);

		plstResult->Add(this);
	}
}

CTerrainNode* CTerrainNode::FindMinNodeContainingBox(const AABB& aabbBox)
{
	AABB boxWS = GetBBox();

	if (aabbBox.min.x < boxWS.min.x - 0.01f || aabbBox.min.y < boxWS.min.y - 0.01f ||
	    aabbBox.max.x > boxWS.max.x + 0.01f || aabbBox.max.y > boxWS.max.y + 0.01f)
		return NULL;

	if (m_pChilds)
		for (int i = 0; i < 4; i++)
			if (CTerrainNode* pRes = m_pChilds[i].FindMinNodeContainingBox(aabbBox))
				return pRes;

	return this;
}

void CTerrainNode::UnloadNodeTexture(bool bRecursive)
{
	if (m_pReadStream)
	{
		// We don't need this stream anymore.
		m_pReadStream->Abort();
		m_pReadStream = NULL;
	}

	if (m_nNodeTexSet.nTex0)
		m_pTerrain->m_texCache[0].ReleaseTexture(m_nNodeTexSet.nTex0, m_nNodeTexSet.nSlot0);

	if (m_nNodeTexSet.nTex1)
		m_pTerrain->m_texCache[1].ReleaseTexture(m_nNodeTexSet.nTex1, m_nNodeTexSet.nSlot1);

	if (m_nNodeTexSet.nTex2)
		m_pTerrain->m_texCache[2].ReleaseTexture(m_nNodeTexSet.nTex2, m_nNodeTexSet.nSlot2);

	if (m_nNodeTexSet.nTex0 && GetCVars()->e_TerrainTextureStreamingDebug == 2)
		PrintMessage("Texture released %d, Level=%d", GetSecIndex(), m_nTreeLevel);

	m_nNodeTexSet = SSectorTextureSet();

	for (int i = 0; m_pChilds && bRecursive && i < 4; i++)
		m_pChilds[i].UnloadNodeTexture(bRecursive);

	m_eTexStreamingStatus = ecss_NotLoaded;
}

void CTerrainNode::UpdateDistance(const SRenderingPassInfo& passInfo)
{
	m_arrfDistance[passInfo.GetRecursiveLevel()] = GetPointToBoxDistance(passInfo.GetCamera().GetPosition(), GetBBox());
}

const float CTerrainNode::GetDistance(const SRenderingPassInfo& passInfo)
{
	return m_arrfDistance[passInfo.GetRecursiveLevel()];
}

void CTerrainNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	{
		SIZER_COMPONENT_NAME(pSizer, "TerrainNodeSelf");
		pSizer->AddObject(this, sizeof(*this));
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "SurfaceTypeInfo");
		pSizer->AddObject(m_lstSurfaceTypeInfo);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "HMData");
		pSizer->AddObject(m_rangeInfo.pHMData, m_rangeInfo.nSize * m_rangeInfo.nSize * sizeof(m_rangeInfo.pHMData[0]));
	}

	if (m_pLeafData)
	{
		SIZER_COMPONENT_NAME(pSizer, "LeafData");
		pSizer->AddObject(m_pLeafData, sizeof(*m_pLeafData));
	}

	// childs
	if (m_pChilds)
		for (int i = 0; i < 4; i++)
			m_pChilds[i].GetMemoryUsage(pSizer);
}

void CTerrainNode::GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& cstAABB)
{
	size_t nCurrentChild(0);
	size_t nTotalChildren(4);

	SIZER_COMPONENT_NAME(pSizer, "CTerrainNode");

	if (m_pChilds)
		for (nCurrentChild = 0; nCurrentChild < nTotalChildren; ++nCurrentChild)
		{
			m_pChilds[nCurrentChild].GetResourceMemoryUsage(pSizer, cstAABB);
		}

	size_t nCurrentSurfaceTypeInfo(0);
	size_t nNumberOfSurfaceTypeInfo(0);

	nNumberOfSurfaceTypeInfo = m_lstSurfaceTypeInfo.size();
	for (nCurrentSurfaceTypeInfo = 0; nCurrentSurfaceTypeInfo < nNumberOfSurfaceTypeInfo; ++nCurrentSurfaceTypeInfo)
	{
		SSurfaceTypeInfo& rSurfaceType = m_lstSurfaceTypeInfo[nCurrentSurfaceTypeInfo];
		if (rSurfaceType.pSurfaceType)
		{
			if (rSurfaceType.pSurfaceType->pLayerMat)
			{
				rSurfaceType.pSurfaceType->pLayerMat->GetResourceMemoryUsage(pSizer);
			}
		}
	}

	// Leaf data is dynamic, thus it's not being accounted now.
	//	if (m_pLeafData)
	//	{
	//	}
}

void CTerrainNode::Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (GetCVars()->e_TerrainDrawThisSectorOnly)
	{
		if (
		  passInfo.GetCamera().GetPosition().x > GetBBox().max.x ||
		  passInfo.GetCamera().GetPosition().x < GetBBox().min.x ||
		  passInfo.GetCamera().GetPosition().y > GetBBox().max.y ||
		  passInfo.GetCamera().GetPosition().y < GetBBox().min.y)
			return; // false;
	}

	if (passInfo.IsShadowPass())
	{
		// render shadow gen using fast path and mesh instancing
		DrawArray(passInfo);
	}
	else
	{
		CheckLeafData();

		if (!RenderSector(passInfo) && m_pTerrain)
			m_pTerrain->m_pTerrainUpdateDispatcher->QueueJob(this, passInfo);
	}

	m_nLastTimeUsed = fastftol_positive(GetCurTimeSec());
}

/*bool CTerrainNode::CheckUpdateLightMap()
   {
   Vec3 vSunDir = Get3DEngine()->GetSunDir().normalized();

   bool bUpdateIsRequired = m _nNodeTextureLastUsedFrameId>(passInfo.GetMainFrameID()-30) &&
    (GetSectorSizeInHeightmapUnits()>=TERRAIN_LM_TEX_RES && m_nNodeTextureOffset>=0) && !m_nNodeTexSet.nTex1;

   if(!bUpdateIsRequired)
    return true; // allow to go next node

   int nLM_Finished_Size = 0;
   int nCount = 0;
   float fStartTime = GetCurAsyncTimeSec();
   while(!nLM_Finished_Size && (GetCurAsyncTimeSec()-fStartTime) < 0.001f*GetCVars()->e_terrain_lm_gen_ms_per_frame)
   {
    nLM_Finished_Size = DoLightMapIncrementaly(m_nNodeTexSet.nLM_X, m_nNodeTexSet.nLM_Y, 512);
    nCount++;
   }

   if(nLM_Finished_Size)
   {
    if(m_nNodeTexSet.nTex1)
      GetRenderer()->RemoveTexture(m_nNodeTexSet.nTex1);

    m_nNodeTexSet.nTex1 = GetRenderer()->DownLoadToVideoMemory((uint8*)m_LightMap.GetData(), nLM_Finished_Size, nLM_Finished_Size,
      eTF_A8R8G8B8, eTF_A8R8G8B8, 0, false, FILTER_LINEAR);

    // Mark Vegetations Brightness As Un-compiled
    m_boxHeigtmap.min.z = max(0.f,m_boxHeigtmap.min.z);

    return true; // allow to go next node
   }

   return false; // more time is needed for this node
   }*/

bool CTerrainNode::CheckUpdateDiffuseMap()
{
	if (!m_nNodeTexSet.nTex0 && m_eTexStreamingStatus != ecss_Ready)
	{
		// make texture loading once per several frames
		CheckLeafData();

		if (m_eTexStreamingStatus == ecss_NotLoaded)
			StartSectorTexturesStreaming(Get3DEngine()->IsTerrainSyncLoad());

		if (m_eTexStreamingStatus != ecss_Ready)
			return false;

		return false;
	}

	return true; // allow to go next node
}

bool CTerrainNode::AssignTextureFileOffset(int16*& pIndices, int16& nElementsLeft)
{
	m_nNodeTextureOffset = *pIndices;

	pIndices++;
	nElementsLeft--;

	for (int i = 0; i < 4; i++)
	{
		if (*pIndices >= 0)
		{
			if (!m_pChilds)
				return false;

			if (!m_pChilds[i].AssignTextureFileOffset(pIndices, nElementsLeft))
				return false;
		}
		else
		{
			pIndices++;
			nElementsLeft--;
		}
	}

	return true;
}

int SSurfaceTypeInfo::GetIndexCount()
{
	int nCount = 0;
	for (int i = 0; i < 3; i++)
		if (arrpRM[i])
			nCount += arrpRM[i]->GetIndicesCount();

	return nCount;

}

void SSurfaceTypeInfo::DeleteRenderMeshes(IRenderer* pRend)
{
	for (int i = 0; i < 3; i++)
	{
		arrpRM[i] = NULL;
	}
}

int CTerrainNode::GetSectorSizeInHeightmapUnits() const
{
	int nSectorSize = CTerrain::GetSectorSize() << m_nTreeLevel;
	return int(nSectorSize * CTerrain::GetHeightMapUnitSizeInverted());
}

SProcObjChunk::SProcObjChunk()
{
	nAllocatedItems = GetCVars()->e_ProcVegetationMaxObjectsInChunk;
	m_pInstances = new CVegetation[nAllocatedItems];
}

SProcObjChunk::~SProcObjChunk()
{
	delete[] m_pInstances;
}

CTerrainNode* CTerrain::FindMinNodeContainingBox(const AABB& someBox)
{
	FUNCTION_PROFILER_3DENGINE;
	CTerrainNode* pParentNode = GetParentNode();
	return pParentNode ? pParentNode->FindMinNodeContainingBox(someBox) : NULL;
}

void CTerrainNode::OffsetPosition(const Vec3& delta)
{
	for (int i = 0; i < m_lstSurfaceTypeInfo.Count(); ++i)
	{
		for (int p = 0; p < 3; ++p)
		{
			if (IRenderMesh* pRM = m_lstSurfaceTypeInfo[i].arrpRM[p])
			{
				pRM->OffsetPosition(delta);
			}
		}
	}

	if (m_pChilds)
		for (int i = 0; i < 4; ++i)
			m_pChilds[i].OffsetPosition(delta);
}

void CTerrainNode::FillBBox(AABB& aabb)
{
	aabb = GetBBox();
}

void CTerrainNode::SetTraversalFrameId(uint32 onePassTraversalFrameId, int shadowFrustumLod)
{
	if (m_onePassTraversalFrameId != onePassTraversalFrameId)
	{
		m_onePassTraversalShadowCascades = 0;
		m_onePassTraversalFrameId = onePassTraversalFrameId;
	}

	m_onePassTraversalShadowCascades |= BIT(shadowFrustumLod);

	// mark also the path to this node
	if (m_pParent)
		m_pParent->SetTraversalFrameId(onePassTraversalFrameId, shadowFrustumLod);
}

void CTerrainNode::InvalidateCachedShadowData()
{
	ZeroArray(m_shadowCacheLastRendered);
	ZeroArray(m_shadowCacheLod);

	if (m_pChilds)
	{
		for (int nChild = 0; nChild < 4; nChild++)
		{
			m_pChilds[nChild].InvalidateCachedShadowData();
		}
	}
}
