// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terran_edit.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: add/remove static objects, modify hmap (used by editor)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "3dEngine.h"
#include "Vegetation.h"
#include "PolygonClipContext.h"
#include "RoadRenderNode.h"
#include "MergedMeshRenderNode.h"
#include "MergedMeshGeometry.h"
#include "Brush.h"
#include "DecalRenderNode.h"
#include "WaterVolumeRenderNode.h"
#include <CryEntitySystem/IEntitySystem.h>

#define RAD2BYTE(x) ((x)* 255.0f / float(g_PI2))
#define BYTE2RAD(x) ((x)* float(g_PI2) / 255.0f)

//////////////////////////////////////////////////////////////////////////
IRenderNode* CTerrain::AddVegetationInstance(int nStaticGroupIndex, const Vec3& vPos, const float fScale, uint8 ucBright,
                                             uint8 angle, uint8 angleX, uint8 angleY)
{
	if (vPos.x <= 0 || vPos.y <= 0 || vPos.x >= CTerrain::GetTerrainSize() || vPos.y >= CTerrain::GetTerrainSize() || fScale * VEGETATION_CONV_FACTOR < 1.f)
		return 0;
	IRenderNode* renderNode = NULL;

	if (nStaticGroupIndex < 0 || nStaticGroupIndex >= GetObjManager()->m_lstStaticTypes.Count())
	{
		return 0;
	}

	StatInstGroup& group = GetObjManager()->m_lstStaticTypes[nStaticGroupIndex];
	if (!group.GetStatObj())
	{
		Warning("I3DEngine::AddStaticObject: Attempt to add object of undefined type");
		return 0;
	}

	bool bValidForMerging = group.GetStatObj()->GetRenderTrisCount() < GetCVars()->e_MergedMeshesMaxTriangles;

	if (!group.bAutoMerged || !bValidForMerging)
	{
		CVegetation* pEnt = (CVegetation*)Get3DEngine()->CreateRenderNode(eERType_Vegetation);
		pEnt->SetScale(fScale);
		pEnt->m_vPos = vPos;
		pEnt->SetStatObjGroupIndex(nStaticGroupIndex);
		pEnt->m_ucAngle = angle;
		pEnt->m_ucAngleX = angleX;
		pEnt->m_ucAngleY = angleY;
		pEnt->CalcBBox();

		float fEntLengthSquared = pEnt->GetBBox().GetSize().GetLengthSquared();
		if (fEntLengthSquared > MAX_VALID_OBJECT_VOLUME || !_finite(fEntLengthSquared) || fEntLengthSquared <= 0)
		{
			Warning("CTerrain::AddVegetationInstance: Object has invalid bbox: %s,%s, GetRadius() = %.2f",
			        pEnt->GetName(), pEnt->GetEntityClassName(), sqrt_tpl(fEntLengthSquared) * 0.5f);
		}

		if (group.bAutoMerged && !bValidForMerging)
		{
			static int nLastFrameId = 0;
			if (nLastFrameId != gEnv->pRenderer->GetFrameID()) // log spam prevention
			{
				Warning("%s: Vegetation object is not suitable for merging because of too many polygons: %s (%d triangles)", __FUNCTION__, group.GetStatObj()->GetFilePath(), group.GetStatObj()->GetRenderTrisCount());
				nLastFrameId = gEnv->pRenderer->GetFrameID();
			}
		}

		pEnt->Physicalize();
		Get3DEngine()->RegisterEntity(pEnt);
		renderNode = pEnt;
	}
	else
	{
		SProcVegSample sample;
		sample.InstGroupId = nStaticGroupIndex;
		sample.pos = vPos;
		sample.scale = (uint8)SATURATEB(fScale * VEGETATION_CONV_FACTOR);
		Matrix33 mr = Matrix33::CreateRotationXYZ(Ang3(BYTE2RAD(angleX), BYTE2RAD(angleY), BYTE2RAD(angle)));

		if (group.GetAlignToTerrainAmount() != 0.f)
		{
			Matrix33 m33;
			GetTerrain()->GetTerrainAlignmentMatrix(vPos, group.GetAlignToTerrainAmount(), m33);
			sample.q = Quat(m33) * Quat(mr);
		}
		else
		{
			sample.q = Quat(mr);
		}
		sample.q.NormalizeSafe();
		renderNode = m_pMergedMeshesManager->AddInstance(sample);
	}

	return renderNode;
}

void CTerrain::RemoveAllStaticObjects()
{
	if (!Get3DEngine()->m_pObjectsTree)
		return;

	PodArray<SRNInfo> lstObjects;
	Get3DEngine()->m_pObjectsTree->MoveObjectsIntoList(&lstObjects, NULL);

	for (int i = 0; i < lstObjects.Count(); i++)
	{
		IRenderNode* pNode = lstObjects.GetAt(i).pNode;
		switch (pNode->GetRenderNodeType())
		{
		case eERType_Vegetation:
			if (!(pNode->GetRndFlags() & ERF_PROCEDURAL))
				pNode->ReleaseNode();
			break;
		case eERType_MergedMesh:
			pNode->ReleaseNode();
			break;
		}
	}
}

void CTerrain::HighlightTerrain(int x1, int y1, int x2, int y2)
{
	// Input dimensions are in units.
	float unitSize = GetHeightMapUnitSize();
	float x1u = (float)x1 * unitSize, x2u = (float)x2 * unitSize;
	float y1u = (float)y1 * unitSize, y2u = (float)y2 * unitSize;

	ColorB clrRed(255, 0, 0);
	IRenderAuxGeom* prag = GetISystem()->GetIRenderer()->GetIRenderAuxGeom();

	for (int x = x1; x < x2; x += 1)
	{
		float xu = (float)x * unitSize;
		prag->DrawLine(Vec3(y1u, xu, GetZfromUnits(y1, x) + 0.5f), clrRed, Vec3(y1u, xu + unitSize, GetZfromUnits(y1, x + 1) + 0.5f), clrRed, 5.0f);
		prag->DrawLine(Vec3(y2u, xu, GetZfromUnits(y2, x) + 0.5f), clrRed, Vec3(y2u, xu + unitSize, GetZfromUnits(y2, x + 1) + 0.5f), clrRed, 5.0f);
	}
	for (int y = y1; y < y2; y += 1)
	{
		float yu = (float)y * unitSize;
		prag->DrawLine(Vec3(yu, x1u, GetZfromUnits(y, x1) + 0.5f), clrRed, Vec3(yu + unitSize, x1u, GetZfromUnits(y + 1, x1) + 0.5f), clrRed, 5.0f);
		prag->DrawLine(Vec3(yu, x2u, GetZfromUnits(y, x2) + 0.5f), clrRed, Vec3(yu + unitSize, x2u, GetZfromUnits(y + 1, x2) + 0.5f), clrRed, 5.0f);
	}
}

void CTerrain::SetTerrainElevation(int X1, int Y1, int nSizeX, int nSizeY, float* pTerrainBlock,
                                   SSurfaceTypeItem* pSurfaceData, int nSurfOrgX, int nSurfOrgY, int nSurfSizeX, int nSurfSizeY,
                                   uint32* pResolMap, int nResolMapSizeX, int nResolMapSizeY)
{
#ifndef _RELEASE

	//LOADING_TIME_PROFILE_SECTION;
	FUNCTION_PROFILER_3DENGINE;

	float fStartTime = GetCurAsyncTimeSec();
	float unitSize = CTerrain::GetHeightMapUnitSize();
	int nHmapSize = int(CTerrain::GetTerrainSize() / unitSize);

	ResetHeightMapCache();

	// everything is in units in this function

	assert(nSizeX == nSizeY);
	assert(X1 == ((X1 >> m_nUnitsToSectorBitShift) << m_nUnitsToSectorBitShift));
	assert(Y1 == ((Y1 >> m_nUnitsToSectorBitShift) << m_nUnitsToSectorBitShift));
	assert(nSizeX == ((nSizeX >> m_nUnitsToSectorBitShift) << m_nUnitsToSectorBitShift));
	assert(nSizeY == ((nSizeY >> m_nUnitsToSectorBitShift) << m_nUnitsToSectorBitShift));

	if (X1 < 0 || Y1 < 0 || X1 + nSizeX > nHmapSize || Y1 + nSizeY > nHmapSize)
	{
		Warning("CTerrain::SetTerrainHeightMapBlock: (X1,Y1) values out of range");
		return;
	}

	AABB aabb = Get3DEngine()->m_pObjectsTree->GetNodeBox();

	int x0 = (int)(aabb.min.x * m_fInvUnitSize);
	int y0 = (int)(aabb.min.y * m_fInvUnitSize);

	if (!GetParentNode())
		BuildSectorsTree(false);

	Array2d<struct CTerrainNode*>& sectorLayer = m_arrSecInfoPyramid[0];

	int rangeX1 = max(0, X1 >> m_nUnitsToSectorBitShift);
	int rangeY1 = max(0, Y1 >> m_nUnitsToSectorBitShift);
	int rangeX2 = min(sectorLayer.GetSize(), (X1 + nSizeX) >> m_nUnitsToSectorBitShift);
	int rangeY2 = min(sectorLayer.GetSize(), (Y1 + nSizeY) >> m_nUnitsToSectorBitShift);

	std::vector<float> rawHeightmap;

	AABB modifiedArea;
	modifiedArea.Reset();

	for (int rangeX = rangeX1; rangeX < rangeX2; rangeX++)
	{
		for (int rangeY = rangeY1; rangeY < rangeY2; rangeY++)
		{
			CTerrainNode* pTerrainNode = sectorLayer[rangeX][rangeY];

			int x1 = x0 + (rangeX << m_nUnitsToSectorBitShift);
			int y1 = y0 + (rangeY << m_nUnitsToSectorBitShift);
			int x2 = x0 + ((rangeX + 1) << m_nUnitsToSectorBitShift);
			int y2 = y0 + ((rangeY + 1) << m_nUnitsToSectorBitShift);

			float fMaxTexelSizeMeters = -1;

			if (pResolMap)
			{
				// get max allowed texture resolution here
				int nResMapX = (nResolMapSizeX * (x1 / 2 + x2 / 2) / nHmapSize);
				int nResMapY = (nResolMapSizeY * (y1 / 2 + y2 / 2) / nHmapSize);
				nResMapX = CLAMP(nResMapX, 0, nResolMapSizeX - 1);
				nResMapY = CLAMP(nResMapY, 0, nResolMapSizeY - 1);
				int nTexRes = pResolMap[nResMapY + nResMapX * nResolMapSizeY];
				int nResTileSizeMeters = GetTerrainSize() / nResolMapSizeX;
				fMaxTexelSizeMeters = (float)nResTileSizeMeters / (float)nTexRes;
			}

			int nStep = 1 << 0 /*nGeomMML*/;
			int nMaxStep = 1 << m_nUnitsToSectorBitShift;

			SRangeInfo& ri = pTerrainNode->m_rangeInfo;

			if (ri.nSize != nMaxStep / nStep + 1)
			{
				delete[] ri.pHMData;
				ri.nSize = nMaxStep / nStep + 1;
				ri.pHMData = new SHeightMapItem[ri.nSize * ri.nSize];
				ri.UpdateBitShift(m_nUnitsToSectorBitShift);
			}

			assert(ri.pHMData);

			if (rawHeightmap.size() < (size_t)ri.nSize * ri.nSize)
			{
				rawHeightmap.resize(ri.nSize * ri.nSize);
			}

			// find min/max
			float fMin = pTerrainBlock[CLAMP(x1, 1, nHmapSize - 1) * nHmapSize + CLAMP(y1, 1, nHmapSize - 1)];
			float fMax = fMin;

			// fill height map data array in terrain node, all in units
			for (int x = x1; x <= x2; x += nStep)
			{
				for (int y = y1; y <= y2; y += nStep)
				{
					int ix = min(nHmapSize - 1, x);
					int iy = min(nHmapSize - 1, y);

					float fHeight = pTerrainBlock[ix * nHmapSize + iy];

					if (fHeight > fMax) fMax = fHeight;
					if (fHeight < fMin) fMin = fHeight;

					// TODO: add proper fp16/fp32 support

					int x_local = (x - x1) / nStep;
					int y_local = (y - y1) / nStep;

					rawHeightmap[x_local * ri.nSize + y_local] = fHeight;
				}
			}

			// reserve some space for in-game deformations
			fMin = max(0.f, fMin - TERRAIN_DEFORMATION_MAX_DEPTH);

			pTerrainNode->m_bHMDataIsModified = (ri.fOffset != fMin);

			ri.fOffset = fMin;
			ri.fRange = (fMax - fMin) / float(0x0FFF);

			pTerrainNode->m_boxHeigtmapLocal.min.Set((float)((x1 - x0) * unitSize), (float)((y1 - y0) * unitSize), fMin);
			pTerrainNode->m_boxHeigtmapLocal.max.Set((float)((x2 - x0) * unitSize), (float)((y2 - y0) * unitSize), max(fMax, GetWaterLevel()));

			for (int x = x1; x <= x2; x += nStep)
			{
				for (int y = y1; y <= y2; y += nStep)
				{
					int xlocal = (x - x1) / nStep;
					int ylocal = (y - y1) / nStep;
					int nCellLocal = xlocal * ri.nSize + ylocal;

					uint32 height = ri.fRange ? int((rawHeightmap[nCellLocal] - fMin) / ri.fRange) : 0;

					assert(x >= x0 + X1 && y >= y0 + Y1 && x <= x0 + X1 + nSurfSizeX && y <= y0 + Y1 + nSurfSizeY && pSurfaceData);

					int nSurfX = (x - nSurfOrgX);
					int nSurfY = (y - nSurfOrgY);

					SSurfaceTypeLocal dst;

					assert(nSurfX >= 0 && nSurfY >= 0 && pSurfaceData);

					nSurfX = min(nSurfX, nSurfSizeX - 1);
					nSurfY = min(nSurfY, nSurfSizeY - 1);

					{
						int nSurfCell = nSurfX * nSurfSizeY + nSurfY;
						assert(nSurfCell >= 0 && nSurfCell < nSurfSizeX * nSurfSizeY);

						const SSurfaceTypeItem& src = pSurfaceData[nSurfCell];

						if (src.GetHole())
						{
							dst = SRangeInfo::e_index_hole;
						}
						else
						{
							// read all 3 types, remap to local
							for (int i = 0; i < SSurfaceTypeLocal::kMaxSurfaceTypesNum; i++)
							{
								dst.we[i] = src.we[i] / 16;

								if (src.we[i] || !i)
								{
									dst.ty[i] = (byte)ri.GetLocalSurfaceTypeID(src.ty[i]);
								}
							}

							dst.we[0] = SATURATEB(15 - dst.we[1] - dst.we[2]);
						}
					}

					SHeightMapItem nNewValue;
					nNewValue.height = height;

					// pack SSurfTypeItem into uint32
					uint32 surface = 0;
					SSurfaceTypeLocal::EncodeIntoUint32(dst, surface);
					nNewValue.surface = surface;

					if (nNewValue != ri.pHMData[nCellLocal])
					{
						ri.pHMData[nCellLocal] = nNewValue;

						pTerrainNode->m_bHMDataIsModified = true;
					}
				}
			}
		}
	}

	for (int rangeX = rangeX1; rangeX < rangeX2; rangeX++)
	{
		for (int rangeY = rangeY1; rangeY < rangeY2; rangeY++)
		{
			CTerrainNode* pTerrainNode = sectorLayer[rangeX][rangeY];

			// re-init surface types info and update vert buffers in entire brunch
			if (GetParentNode())
			{
				CTerrainNode* pNode = pTerrainNode;
				while (pNode)
				{
					pNode->m_geomError = kGeomErrorNotSet;

					pNode->ReleaseHeightMapGeometry();
					pNode->RemoveProcObjects(false, false);
					pNode->UpdateDetailLayersInfo(false);

					// propagate bounding boxes and error metrics to parents
					if (pNode != pTerrainNode)
					{
						pNode->m_boxHeigtmapLocal.min = SetMaxBB();
						pNode->m_boxHeigtmapLocal.max = SetMinBB();

						for (int nChild = 0; nChild < 4; nChild++)
						{
							pNode->m_boxHeigtmapLocal.min.CheckMin(pNode->m_pChilds[nChild].m_boxHeigtmapLocal.min);
							pNode->m_boxHeigtmapLocal.max.CheckMax(pNode->m_pChilds[nChild].m_boxHeigtmapLocal.max);
						}
					}

					// request elevation texture update
					if (pNode->m_nNodeTexSet.nSlot0 != 0xffff && pNode->m_nNodeTexSet.nSlot0 < m_pTerrain->m_texCache[2].GetPoolSize())
					{
						if (pTerrainNode->m_bHMDataIsModified)
							pNode->m_eElevTexEditingState = eTES_SectorIsModified_AtlasIsDirty;
					}

					pNode = pNode->m_pParent;
				}
			}

			modifiedArea.Add(pTerrainNode->GetBBox());
		}
	}

	if (Get3DEngine()->m_pObjectsTree)
		Get3DEngine()->m_pObjectsTree->UpdateTerrainNodes();

	// update roads
	if (Get3DEngine()->m_pObjectsTree && (GetCVars()->e_TerrainDeformations || m_bEditor))
	{
		PodArray<IRenderNode*> lstRoads;

		aabb = AABB(
		  Vec3((float)(x0 + X1) * (float)unitSize, (float)(y0 + Y1) * (float)unitSize, 0.f),
		  Vec3((float)(x0 + X1) * (float)unitSize + (float)nSizeX * (float)unitSize, (float)(y0 + Y1) * (float)unitSize + (float)nSizeY * (float)unitSize, 1024.f));

		Get3DEngine()->m_pObjectsTree->GetObjectsByType(lstRoads, eERType_Road, &aabb);
		for (int i = 0; i < lstRoads.Count(); i++)
		{
			CRoadRenderNode* pRoad = (CRoadRenderNode*)lstRoads[i];
			pRoad->OnTerrainChanged();
		}
	}

	if (GetParentNode())
		GetParentNode()->UpdateRangeInfoShift();

	if (!modifiedArea.IsReset())
	{
		modifiedArea.Expand(Vec3(2.f * GetHeightMapUnitSize()));
		ResetTerrainVertBuffers(&modifiedArea);
	}

	m_bHeightMapModified = 0;

	m_terrainPaintingFrameId = GetRenderer()->GetFrameID(false);

#endif // _RELEASE
}

void CTerrain::SetTerrainSectorTexture(int nTexSectorX, int nTexSectorY, unsigned int textureId, bool bMergeNotAllowed)
{
	int nDiffTexTreeLevelOffset = 0;

	if (nTexSectorX < 0 ||
	    nTexSectorY < 0 ||
	    nTexSectorX >= CTerrain::GetSectorsTableSize() >> nDiffTexTreeLevelOffset ||
	    nTexSectorY >= CTerrain::GetSectorsTableSize() >> nDiffTexTreeLevelOffset)
	{
		Warning("CTerrain::LockSectorTexture: (nTexSectorX, nTexSectorY) values out of range");
		return;
	}

	CTerrainNode* pNode = m_arrSecInfoPyramid[nDiffTexTreeLevelOffset][nTexSectorX][nTexSectorY];

	while (pNode)
	{
		pNode->EnableTextureEditingMode(textureId);
		pNode = pNode->m_pParent;
	}
}

void CTerrain::ResetTerrainVertBuffers(const AABB* pBox)
{
	if (GetParentNode())
		GetParentNode()->ReleaseHeightMapGeometry(true, pBox);
}

void CTerrain::SetOceanWaterLevel(float oceanWaterLevel)
{
	SetWaterLevel(oceanWaterLevel);
	pe_params_buoyancy pb;
	pb.waterPlane.origin.Set(0, 0, oceanWaterLevel);
	if (gEnv->pPhysicalWorld)
		gEnv->pPhysicalWorld->AddGlobalArea()->SetParams(&pb);
	extern float g_oceanStep;
	g_oceanStep = -1; // if e_PhysOceanCell is used, make it re-apply the params on Update
}

bool CTerrain::CanPaintSurfaceType(int x, int y, int r, uint16 usGlobalSurfaceType)
{
	// Checks if the palettes of all sectors touching the given brush square can accept usGlobalSurfaceType.

	if (usGlobalSurfaceType == SRangeInfo::e_hole)
		return true;

	int nTerrainSizeSectors = CTerrain::GetTerrainSize() / CTerrain::GetSectorSize();

	int rangeX1 = max(0, (x - r) >> m_nUnitsToSectorBitShift);
	int rangeY1 = max(0, (y - r) >> m_nUnitsToSectorBitShift);
	int rangeX2 = min(nTerrainSizeSectors, ((x + r) >> m_nUnitsToSectorBitShift) + 1);
	int rangeY2 = min(nTerrainSizeSectors, ((y + r) >> m_nUnitsToSectorBitShift) + 1);

	float unitSize = (float)CTerrain::GetSectorSize();
	int nSignedBitShift = IntegerLog2((unsigned int)CTerrain::GetSectorSize());
	while (unitSize > CTerrain::GetHeightMapUnitSize())
	{
		unitSize *= 0.5f;
		nSignedBitShift--;
	}

	// Can we fit this surface type into the palettes of all sectors involved?
	for (int rangeX = rangeX1; rangeX < rangeX2; rangeX++)
		for (int rangeY = rangeY1; rangeY < rangeY2; rangeY++)
		{
			int rx = rangeX, ry = rangeY;

			Array2d<struct CTerrainNode*>& sectorLayer = m_arrSecInfoPyramid[0];
			CTerrainNode* pTerrainNode = sectorLayer[ry][rx];
			SRangeInfo& ri = pTerrainNode->m_rangeInfo;

			if (ri.GetLocalSurfaceTypeID(usGlobalSurfaceType) == SRangeInfo::e_index_hole)
			{
				IRenderAuxText::DrawLabel(Vec3((float)y, (float)x, GetZfromUnits(y, x) + 1.0f), 2.0f, "SECTOR PALETTE FULL!");
				HighlightTerrain(
				  rangeX << m_nUnitsToSectorBitShift, rangeY << m_nUnitsToSectorBitShift,
				  (rangeX + 1) << m_nUnitsToSectorBitShift, (rangeY + 1) << m_nUnitsToSectorBitShift);
				return false;
			}
		}

	return true;
}

ILINE bool CTerrain::IsRenderNodeIncluded(IRenderNode* pNode, const AABB& region, const uint16* pIncludeLayers, int numIncludeLayers)
{
	EERType type = pNode->GetRenderNodeType();

	switch (type)
	{
	case eERType_Brush:
	case eERType_Vegetation:
	case eERType_Decal:
	case eERType_WaterVolume:
	case eERType_MergedMesh:
		break;

	default:
		CRY_ASSERT_TRACE(0, ("Need to support cloning terrain object type %d", type));
		return false;
	}

	if (type == eERType_MergedMesh)
	{
		CMergedMeshRenderNode* pMergedMesh = (CMergedMeshRenderNode*)pNode;

		// The bounding box for a merged mesh can slop over into a neighboring grid
		// chunk, so only clone it if the center is contained in the region we're cloning.
		if (!region.IsContainPoint(pMergedMesh->m_pos))
			return false;
	}

	if (numIncludeLayers == 0)
		return true;

	const uint16 layerId = pNode->GetLayerId();

	// Some terrain objects don't have layer ids (like vegetation), so we have to include
	// them even if we're filtering.
	if (layerId == 0)
	{
		return true;
	}
	else
	{
		for (int e = 0; e < numIncludeLayers; e++)
		{
			if (pIncludeLayers[e] == layerId)
			{
				return true;
			}
		}
	}

	// If we made it to this point none of the include layers matched, so it's not excluded
	return false;
}

void CTerrain::MarkAndOffsetCloneRegion(const AABB& region, const Vec3& offset)
{
	C3DEngine* p3DEngine = Get3DEngine();
	int numObjects = p3DEngine->GetObjectsInBox(region, NULL);

	if (numObjects == 0)
		return;

	PodArray<IRenderNode*> objects;
	objects.resize(numObjects);
	p3DEngine->GetObjectsInBox(region, &objects[0]);

	int skyLayer = gEnv->pEntitySystem->GetLayerId("Sky");

	for (int i = 0; i < numObjects; i++)
	{
		IRenderNode* pNode = objects[i];
		EERType type = pNode->GetRenderNodeType();

		// HACK: Need a way for certain layers to not get offset, like the skybox
		if (type == eERType_Brush && skyLayer != -1)
		{
			CBrush* pBrush = (CBrush*)pNode;
			if (pBrush->GetLayerId() == skyLayer)
				continue;
		}

		if (IsRenderNodeIncluded(pNode, region, NULL, 0))
		{
			CRY_ASSERT_MESSAGE((pNode->GetRndFlags() & ERF_CLONE_SOURCE) == 0, "Marking already marked node, is an object overlapping multiple regions?");

			pNode->SetRndFlags(ERF_CLONE_SOURCE, true);

			pNode->OffsetPosition(offset);
			p3DEngine->RegisterEntity(pNode);
		}
	}
}

void CTerrain::CloneRegion(const AABB& region, const Vec3& offset, float zRotation, const uint16* pIncludeLayers, int numIncludeLayers)
{
	C3DEngine* p3DEngine = Get3DEngine();
	int numObjects = p3DEngine->GetObjectsInBox(region, NULL);

	if (numObjects == 0)
		return;

	PodArray<IRenderNode*> objects;
	objects.resize(numObjects);
	p3DEngine->GetObjectsInBox(region, &objects[0]);

	Vec3 localOrigin = region.GetCenter();
	Matrix34 l2w(Matrix33::CreateRotationZ(zRotation));
	l2w.SetTranslation(offset);

	for (int i = 0; i < numObjects; i++)
	{
		IRenderNode* pNode = objects[i];

		// If this wasn't flagged as a clone source, don't include it
		if ((pNode->GetRndFlags() & ERF_CLONE_SOURCE) == 0)
			continue;

		if (!IsRenderNodeIncluded(pNode, region, pIncludeLayers, numIncludeLayers))
			continue;

		IRenderNode* pNodeClone = NULL;

		EERType type = pNode->GetRenderNodeType();
		if (type == eERType_Brush)
		{
			CBrush* pSrcBrush = (CBrush*)pNode;

			pNodeClone = pSrcBrush->Clone();
			CBrush* pBrush = (CBrush*)pNodeClone;

			pBrush->m_Matrix.SetTranslation(pBrush->m_Matrix.GetTranslation() - localOrigin);
			pBrush->m_Matrix = l2w * pBrush->m_Matrix;

			pBrush->CalcBBox();

			pBrush->m_pOcNode = NULL;

			// to get correct indirect lighting the registration must be done before checking if this object is inside a VisArea
			Get3DEngine()->RegisterEntity(pBrush);

			//if (gEnv->IsEditorGameMode())
			{
				//pBrush->Physicalize();
			}
			//else
			{
				// keep everything deactivated, game will activate it later
				//if(Get3DEngine()->IsAreaActivationInUse())
				pBrush->SetRndFlags(ERF_HIDDEN, false);

				if (!(Get3DEngine()->IsAreaActivationInUse() && GetCVars()->e_ObjectLayersActivationPhysics == 1))// && !(pChunk->m_dwRndFlags&ERF_NO_PHYSICS))
					pBrush->Physicalize();
			}
		}
		else if (type == eERType_Vegetation)
		{
			pNodeClone = pNode->Clone();
			CVegetation* pVeg = (CVegetation*)pNodeClone;

			Matrix34 matrix;
			matrix.SetRotationZ(pVeg->GetZAngle(), pVeg->m_vPos - localOrigin);

			matrix = l2w * matrix;

			pVeg->m_vPos = matrix.GetTranslation();

			// Vegetation stores rotation as a byte representing degrees, so we have to remap it back
			CryQuat rot(matrix);
			float zRot = rot.GetRotZ();
			pVeg->m_ucAngle = (byte)((RAD2DEG(zRot) / 360.0f) * 255.0f);

			pVeg->CalcBBox();

			pVeg->Physicalize();

			Get3DEngine()->RegisterEntity(pVeg);
		}
		else if (type == eERType_Decal)
		{
			pNodeClone = pNode->Clone();
			CDecalRenderNode* pDecal = (CDecalRenderNode*)pNodeClone;

			Matrix34 mat = pDecal->GetMatrix();
			mat.SetTranslation(mat.GetTranslation() - localOrigin);
			mat = l2w * mat;

			pDecal->SetMatrixFull(mat);

			Get3DEngine()->RegisterEntity(pDecal);
			GetObjManager()->m_decalsToPrecreate.push_back(pDecal);
		}
		else if (type == eERType_WaterVolume)
		{
			pNodeClone = pNode->Clone();
			CWaterVolumeRenderNode* pWaterVol = (CWaterVolumeRenderNode*)pNodeClone;

			pWaterVol->Transform(localOrigin, l2w);

			pWaterVol->Physicalize();

			Get3DEngine()->RegisterEntity(pWaterVol);
		}
		else if (type == eERType_MergedMesh)
		{
			CMergedMeshRenderNode* pSrcMergedMesh = (CMergedMeshRenderNode*)pNode;

			// Transform the position of the source merged mesh to our destination location
			Vec3 dstPos;
			{
				Matrix34 dst;
				dst.SetTranslationMat(pSrcMergedMesh->m_pos - localOrigin);
				dst = l2w * dst;
				dstPos = dst.GetTranslation();
			}

			// Get the merged mesh node for this coordinate.
			CMergedMeshRenderNode* pDstMergedMesh = m_pMergedMeshesManager->GetNode(dstPos);
			CRY_ASSERT_MESSAGE(pDstMergedMesh->m_pos == dstPos, "Cloned destination position isn't on grid");
			CRY_ASSERT_MESSAGE(pDstMergedMesh->m_nGroups == 0, "Cloning into a used chunk");

			if (pDstMergedMesh->m_nGroups == 0)
			{
				pDstMergedMesh->m_initPos = pSrcMergedMesh->m_initPos;
				pDstMergedMesh->m_zRotation = zRotation;

				// Transform the AABB's
				AABB visibleAABB = pSrcMergedMesh->m_visibleAABB;
				visibleAABB.Move(-localOrigin);
				visibleAABB.SetTransformedAABB(l2w, visibleAABB);
				pDstMergedMesh->m_visibleAABB = visibleAABB;

				AABB internalAABB = pSrcMergedMesh->m_internalAABB;
				internalAABB.Move(-localOrigin);
				internalAABB.SetTransformedAABB(l2w, internalAABB);
				pDstMergedMesh->m_internalAABB = internalAABB;

				pSrcMergedMesh->CopyIRenderNodeData(pDstMergedMesh);

				if (!gEnv->IsDedicated())
				{
					// In the editor the merged meshes have extra proxy data.  We don't
					// clear out the source merged meshes in the editor, so the easiest
					// solution is to just share the source groups and not delete them
					// on destruction.
					if (gEnv->IsEditorGameMode())
					{
						pDstMergedMesh->m_ownsGroups = 0;
						pDstMergedMesh->m_groups = pSrcMergedMesh->m_groups;
						pDstMergedMesh->m_nGroups = pSrcMergedMesh->m_nGroups;
					}
					else
					{
						for (size_t i = 0; i < pSrcMergedMesh->m_nGroups; ++i)
						{
							SMMRMGroupHeader& pGroup = pSrcMergedMesh->m_groups[i];
							pDstMergedMesh->AddGroup(pGroup.instGroupId, pGroup.numSamples);
						}
					}
				}

				pNodeClone = pDstMergedMesh;

				Get3DEngine()->RegisterEntity(pDstMergedMesh);
			}
		}

		if (pNodeClone)
		{
			pNodeClone->SetRndFlags(ERF_CLONE_SOURCE, false);
			pNodeClone->SetLayerId(pNode->GetLayerId());
		}
	}
}

void CTerrain::ClearCloneSources()
{
	I3DEngine* p3DEngine = Get3DEngine();

	int numObjects = p3DEngine->GetObjectsByFlags(ERF_CLONE_SOURCE, NULL);

	if (numObjects == 0)
		return;

	PodArray<IRenderNode*> objects;
	objects.resize(numObjects);
	p3DEngine->GetObjectsByFlags(ERF_CLONE_SOURCE, &objects[0]);

	for (int i = 0; i < numObjects; i++)
	{
		IRenderNode* pNode = objects[i];

		Get3DEngine()->DeleteRenderNode(pNode);
	}
}
