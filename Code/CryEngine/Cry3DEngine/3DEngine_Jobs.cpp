// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   3dengine.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Implementation of I3DEngine interface methods
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryCore/Platform/CryWindows.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameFramework.h>

#include "3dEngine.h"
#include "terrain.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "terrain_water.h"

#include "DecalManager.h"
#include "Vegetation.h"
#include "IndexedMesh.h"
#include <CryMath/AABBSV.h>

#include "MatMan.h"

#include "Brush.h"
#include "PolygonClipContext.h"
#include "CGF/CGFLoader.h"
#include "CGF/ReadOnlyChunkFile.h"

#include "CloudRenderNode.h"
#include "CloudsManager.h"
#include "SkyLightManager.h"
#include "FogVolumeRenderNode.h"
#include "RoadRenderNode.h"
#include "DecalRenderNode.h"
#include "TimeOfDay.h"
#include "LightEntity.h"
#include "FogVolumeRenderNode.h"
#include "ObjectsTree.h"
#include "WaterVolumeRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "DistanceCloudRenderNode.h"
#include "VolumeObjectRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "RopeRenderNode.h"
#include "RenderMeshMerger.h"
#include "PhysCallbacks.h"
#include "DeferredCollisionEvent.h"
#include "MergedMeshRenderNode.h"
#include "BreakableGlassRenderNode.h"
#include "OpticsManager.h"
#include "ClipVolumeManager.h"

#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)
	#include "PrismRenderNode.h"
#endif // EXCLUDE_DOCUMENTATION_PURPOSE

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct C3DEngine__Cmp_DLightAmount
{
	bool operator()(const DLightAmount& rA, const DLightAmount& rB) const
	{
		return rA.fAmount > rB.fAmount;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SortLightAmount(DLightAmount* pBegin, DLightAmount* pEnd)
{
	std::sort(pBegin, pEnd, C3DEngine__Cmp_DLightAmount());
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::CheckAddLight(CDLight* pLight, const SRenderingPassInfo& passInfo)
{
	if (pLight->m_Id < 0)
	{
		GetRenderer()->EF_ADDDlight(pLight, passInfo);
		assert(pLight->m_Id >= 0);
	}
}

///////////////////////////////////////////////////////////////////////////////
float C3DEngine::GetLightAmount(CDLight* pLight, const AABB& objBox)
{
	// find amount of light
	float fDist = sqrt_tpl(Distance::Point_AABBSq(pLight->m_Origin, objBox));
	float fLightAttenuation = (pLight->m_Flags & DLF_DIRECTIONAL) ? 1.f : 1.f - (fDist) / (pLight->m_fRadius);
	if (fLightAttenuation < 0)
		fLightAttenuation = 0;

	float fLightAmount =
	  (pLight->m_Color.r + pLight->m_Color.g + pLight->m_Color.b) * 0.233f +
	  (pLight->GetSpecularMult()) * 0.1f;

	return fLightAmount * fLightAttenuation;
}

///////////////////////////////////////////////////////////////////////////////
float C3DEngine::GetWaterLevel()
{
	return m_pTerrain ? m_pTerrain->CTerrain::GetWaterLevel() : WATER_LEVEL_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 C3DEngine::GetSkyColor() const
{
	return m_pObjManager ? m_pObjManager->m_vSkyColor : Vec3(0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
bool C3DEngine::IsTessellationAllowed(const CRenderObject* pObj, const SRenderingPassInfo& passInfo, bool bIgnoreShadowPass) const
{
#ifdef MESH_TESSELLATION_ENGINE
	assert(pObj && GetCVars());
	bool rendererTessellation;
	GetRenderer()->EF_Query(EFQ_MeshTessellation, rendererTessellation);
	if (pObj->m_fDistance < GetCVars()->e_TessellationMaxDistance
	    && GetCVars()->e_Tessellation
	    && rendererTessellation
	    && !(pObj->m_ObjFlags & FOB_DISSOLVE)) // dissolve is not working with tessellation for now
	{
		bool bAllowTessellation = true;

		// Check if rendering into shadow map and enable tessellation only if allowed
		if (!bIgnoreShadowPass && passInfo.IsShadowPass())
		{
			if (IsTessellationAllowedForShadowMap(passInfo))
			{
				// NOTE: This might be useful for game projects
				// Use tessellation only for objects visible in main view
				// Shadows will switch to non-tessellated when caster gets out of view
				IRenderNode* pRN = (IRenderNode*)pObj->m_pRenderNode;
				if (pRN)
					bAllowTessellation = ((pRN->GetRenderNodeType() != eERType_TerrainSector) && (pRN->GetDrawFrame() > passInfo.GetFrameID() - 10));
			}
			else
				bAllowTessellation = false;
		}

		return bAllowTessellation;
	}
#endif //#ifdef MESH_TESSELLATION_ENGINE

	return false;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::RenderRenderNode_ShadowPass(IShadowCaster* pShadowCaster, const SRenderingPassInfo& passInfo)
{
	assert(passInfo.IsShadowPass());

	IRenderNode* pRenderNode = static_cast<IRenderNode*>(pShadowCaster);
	if ((pRenderNode->m_dwRndFlags & ERF_HIDDEN) != 0)
	{
		return;
	}

	int nStaticObjectLod = -1;
	if (passInfo.GetShadowMapType() == SRenderingPassInfo::SHADOW_MAP_CACHED)
		nStaticObjectLod = GetCVars()->e_ShadowsCacheObjectLod;
	else if (passInfo.GetShadowMapType() == SRenderingPassInfo::SHADOW_MAP_CACHED_MGPU_COPY)
		nStaticObjectLod = pRenderNode->m_cStaticShadowLod;

	if (!Get3DEngine()->CheckAndCreateRenderNodeTempData(&pRenderNode->m_pTempData, pRenderNode, passInfo))
	{
		return;
	}

	int wantedLod = pRenderNode->m_pTempData->userData.nWantedLod;
	if (pRenderNode->GetShadowLodBias() != IRenderNode::SHADOW_LODBIAS_DISABLE)
	{
		if (passInfo.IsShadowPass() && (pRenderNode->GetDrawFrame(0) < (passInfo.GetFrameID() - 10)))
			wantedLod += GetCVars()->e_ShadowsLodBiasInvis;
		wantedLod += GetCVars()->e_ShadowsLodBiasFixed;
		wantedLod += pRenderNode->GetShadowLodBias();
	}

	if (nStaticObjectLod >= 0)
		wantedLod = nStaticObjectLod;

	switch (pRenderNode->GetRenderNodeType())
	{
	case eERType_Vegetation:
		{
			CVegetation* pVegetation = static_cast<CVegetation*>(pRenderNode);
			const CLodValue lodValue = pVegetation->ComputeLod(wantedLod, passInfo);
			pVegetation->Render(passInfo, lodValue, NULL);
		}
		break;
	case eERType_Brush:
		{
			CBrush* pBrush = static_cast<CBrush*>(pRenderNode);
			const CLodValue lodValue = pBrush->ComputeLod(wantedLod, passInfo);
			pBrush->Render(lodValue, passInfo, NULL, NULL);
		}
		break;
	default:
		{
			const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
			const AABB objBox = pRenderNode->GetBBoxVirtual();
			SRendParams rParams;
			rParams.fDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, objBox)) * passInfo.GetZoomFactor();
			rParams.lodValue = pRenderNode->ComputeLod(wantedLod, passInfo);

			pRenderNode->Render(rParams, passInfo);
		}
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
ITimeOfDay* C3DEngine::GetTimeOfDay()
{
	CTimeOfDay* tod = m_pTimeOfDay;

	if (!tod)
	{
		tod = new CTimeOfDay;
		m_pTimeOfDay = tod;
	}

	return tod;
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::TraceFogVolumes(const Vec3& worldPos, ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo)
{
	CFogVolumeRenderNode::TraceFogVolumes(worldPos, fogVolumeContrib, passInfo);
}

///////////////////////////////////////////////////////////////////////////////
int C3DEngine::GetTerrainSize()
{
	return CTerrain::GetTerrainSize();
}
#include "ParticleEmitter.h"

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::AsyncOctreeUpdate(IRenderNode* pEnt, int nSID, int nSIDConsideredSafe, uint32 nFrameID, bool bUnRegisterOnly)
{
	FUNCTION_PROFILER_3DENGINE;

#ifdef _DEBUG // crash test basically
	const char* szClass = pEnt->GetEntityClassName();
	const char* szName = pEnt->GetName();
	if (!szName[0] && !szClass[0])
		Warning("I3DEngine::RegisterEntity: Entity undefined"); // do not register undefined objects
	//  if(strstr(szName,"Dude"))
	//  int y=0;
#endif

	IF (bUnRegisterOnly, 0)
	{
		UnRegisterEntityImpl(pEnt);
		return;
	}

	AABB aabb;
	pEnt->FillBBox(aabb);
	float fObjRadiusSqr = aabb.GetRadiusSqr();
	EERType eERType = pEnt->GetRenderNodeType();

#ifdef SUPP_HMAP_OCCL
	if (pEnt->m_pTempData)
		pEnt->m_pTempData->userData.m_OcclState.vLastVisPoint.Set(0, 0, 0);
#endif

	UpdateObjectsLayerAABB(pEnt);

	const unsigned int dwRndFlags = pEnt->GetRndFlags();

	if (!(dwRndFlags & ERF_RENDER_ALWAYS) && !(dwRndFlags & ERF_CASTSHADOWMAPS))
		if (GetCVars()->e_ObjFastRegister && pEnt->m_pOcNode && ((COctreeNode*)pEnt->m_pOcNode)->IsRightNode(aabb, fObjRadiusSqr, pEnt->m_fWSMaxViewDist))
		{
			// same octree node
			Vec3 vEntCenter = GetEntityRegisterPoint(pEnt);

			IVisArea* pVisArea = pEnt->GetEntityVisArea();
			if (pVisArea && pVisArea->IsPointInsideVisArea(vEntCenter))
				return; // same visarea

			IVisArea* pVisAreaFromPos = (!m_pVisAreaManager || dwRndFlags & ERF_OUTDOORONLY) ? NULL : GetVisAreaManager()->GetVisAreaFromPos(vEntCenter);
			if (pVisAreaFromPos == pVisArea)
			{
				// NOTE: can only get here when pVisArea==NULL due to 'same visarea' check above. So check for changed clip volume
				if (GetClipVolumeManager()->IsClipVolumeRequired(pEnt))
					GetClipVolumeManager()->UpdateEntityClipVolume(vEntCenter, pEnt);

				return; // same visarea or same outdoor
			}
		}

	if (pEnt->m_pOcNode)
	{
		UnRegisterEntityImpl(pEnt);
	}
	else if (GetCVars()->e_StreamCgf && eERType == eERType_RenderProxy)
	{
		//  Temporary solution: Force streaming priority update for objects that was not registered before
		//  and was not visible before since usual prediction system was not able to detect them

		if ((uint32)pEnt->GetDrawFrame(0) < nFrameID - 16)
		{
			// deferr the render node streaming priority update still we have a correct 3D Engine camera
			int nElementID = m_deferredRenderProxyStreamingPriorityUpdates.Find(pEnt);
			if (nElementID == -1)  // only add elements once
				m_deferredRenderProxyStreamingPriorityUpdates.push_back(pEnt);
		}

	}

	if (eERType == eERType_Vegetation)
	{
		CVegetation* pInst = (CVegetation*)pEnt;
		pInst->UpdateRndFlags();
	}

	pEnt->m_fWSMaxViewDist = pEnt->GetMaxViewDist();

	if (eERType != eERType_Light)
	{
		if (fObjRadiusSqr > sqr(MAX_VALID_OBJECT_VOLUME) || !_finite(fObjRadiusSqr))
		{
			Warning("I3DEngine::RegisterEntity: Object has invalid bbox: name: %s, class name: %s, GetRadius() = %.2f",
			        pEnt->GetName(), pEnt->GetEntityClassName(), fObjRadiusSqr);
			return; // skip invalid objects - usually only objects with invalid very big scale will reach this point
		}

		if (dwRndFlags & ERF_RENDER_ALWAYS)
		{
			if (m_lstAlwaysVisible.Find(pEnt) < 0)
				m_lstAlwaysVisible.Add(pEnt);

			if (dwRndFlags & ERF_HUD)
				return;
		}
	}
	else
	{
		CLightEntity* pLight = (CLightEntity*)pEnt;
		if ((pLight->m_light.m_Flags & (DLF_IGNORES_VISAREAS | DLF_DEFERRED_LIGHT | DLF_THIS_AREA_ONLY)) == (DLF_IGNORES_VISAREAS | DLF_DEFERRED_LIGHT))
		{
			if (m_lstAlwaysVisible.Find(pEnt) < 0)
				m_lstAlwaysVisible.Add(pEnt);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Check for occlusion proxy.
	{
		CStatObj* pStatObj = (CStatObj*)pEnt->GetEntityStatObj();
		if (pStatObj)
		{
			if (pStatObj->m_bHaveOcclusionProxy)
			{
				pEnt->m_dwRndFlags |= ERF_GOOD_OCCLUDER;
				pEnt->m_nInternalFlags |= IRenderNode::HAS_OCCLUSION_PROXY;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (pEnt->m_dwRndFlags & ERF_OUTDOORONLY || !(m_pVisAreaManager && m_pVisAreaManager->SetEntityArea(pEnt, aabb, fObjRadiusSqr)))
	{
#ifndef SEG_WORLD
		if (nSID == -1)
		{
			nSID = 0;

			// if segment is not set - find best one automatically
			Vec3 vCenter = aabb.GetCenter();
			const int iTreeCount = Get3DEngine()->m_pObjectsTree.Count();
			for (int n = 0; n < iTreeCount; n++)
			{
				if (Get3DEngine()->m_pObjectsTree[n])
				{
					if (Overlap::Point_AABB2D(vCenter, m_pObjectsTree[n]->GetNodeBox()))
					{
						nSID = n;
						break;
					}
				}
			}
		}

		if (nSID >= 0 && nSID < m_pObjectsTree.Count())
		{
			if (!m_pObjectsTree[nSID])
			{
				const float terrainSize = (float)GetTerrainSize();
				m_pObjectsTree[nSID] = COctreeNode::Create(nSID, AABB(Vec3(0, 0, 0), Vec3(terrainSize, terrainSize, terrainSize)), NULL);
			}

			m_pObjectsTree[nSID]->InsertObject(pEnt, aabb, fObjRadiusSqr, aabb.GetCenter());
		}
#else
		if (gEnv->IsEditor() || eERType != eERType_Vegetation)
		{
			// CS - opt here
			if (GetITerrain())
			{
				Vec3 vCenter = aabb.GetCenter();
				nSID = GetITerrain()->WorldToSegment(vCenter, GetDefSID());
			}
		}
		else
		{
			// use specified sid from which the objects are actually serialized for vegetation objects
			CVegetation* pInst = (CVegetation*)pEnt;
			nSID = pInst->m_nStaticTypeSlot;
		}

		if (nSID >= 0 && nSID < m_pObjectsTree.Count())
		{
			//#ifdef _DEBUG
			//		  if(nSID != nSIDConsideredSafe)
			//		  {
			//			  Warning("I3DEngine::RegisterEntity: Object %s was not added to the scene", pEnt->GetName());
			//		  }
			//#endif
			// TODO: this should only work if the segment is safe or if it's unsafe and the object is being inserted by SetCompiledData...
			if (nSID == nSIDConsideredSafe || IsSegmentSafeToUse(nSID))
				m_pObjectsTree[nSID]->InsertObject(pEnt, aabb, fObjRadiusSqr, aabb.GetCenter());
		}
#endif
	}

	// update clip volume: use vis area if we have one, otherwise check if we're in the same volume as before. check other volumes as last resort only
	if (pEnt->m_pTempData)
	{
		Vec3 vEntCenter = GetEntityRegisterPoint(pEnt);
		SRenderNodeTempData::SUserData& userData = pEnt->m_pTempData->userData;

		if (IVisArea* pVisArea = pEnt->GetEntityVisArea())
			userData.m_pClipVolume = pVisArea;
		else if (GetClipVolumeManager()->IsClipVolumeRequired(pEnt))
			GetClipVolumeManager()->UpdateEntityClipVolume(vEntCenter, pEnt);
	}

	// register decals, to clean up longer not renderes decals and their render meshes
	if (eERType == eERType_Decal)
	{
		m_decalRenderNodes.push_back((IDecalRenderNode*)pEnt);
	}
}

///////////////////////////////////////////////////////////////////////////////
void C3DEngine::UpdateObjectsLayerAABB(IRenderNode* pEnt)
{
	uint16 nLayerId = pEnt->GetLayerId();
	if (nLayerId && nLayerId < 0xFFFF)
	{
		m_arrObjectLayersActivity.CheckAllocated(nLayerId + 1);

		if (m_arrObjectLayersActivity[nLayerId].objectsBox.GetVolume())
			m_arrObjectLayersActivity[nLayerId].objectsBox.Add(pEnt->GetBBox());
		else
			m_arrObjectLayersActivity[nLayerId].objectsBox = pEnt->GetBBox();
	}
}

///////////////////////////////////////////////////////////////////////////////
bool C3DEngine::UnRegisterEntityImpl(IRenderNode* pEnt)
{
	// make sure we don't try to update the streaming priority if an object
	// was added and removed in the same frame
	int nElementID = m_deferredRenderProxyStreamingPriorityUpdates.Find(pEnt);
	if (nElementID != -1)
		m_deferredRenderProxyStreamingPriorityUpdates.DeleteFastUnsorted(nElementID);

	FUNCTION_PROFILER_3DENGINE;

#ifdef _DEBUG // crash test basically
	const char* szClass = pEnt->GetEntityClassName();
	const char* szName = pEnt->GetName();
	if (!szName[0] && !szClass[0])
		Warning("C3DEngine::RegisterEntity: Entity undefined");
#endif

	EERType eRenderNodeType = pEnt->GetRenderNodeType();

	bool bFound = false;
	//pEnt->PhysicalizeFoliage(false);

	if (pEnt->m_pOcNode)
		bFound = ((COctreeNode*)pEnt->m_pOcNode)->DeleteObject(pEnt);

	if (pEnt->m_dwRndFlags & ERF_RENDER_ALWAYS || (eRenderNodeType == eERType_Light) || (eRenderNodeType == eERType_FogVolume))
	{
		m_lstAlwaysVisible.Delete(pEnt);
	}

	if (eRenderNodeType == eERType_Decal)
	{
		std::vector<IDecalRenderNode*>::iterator it = std::find(m_decalRenderNodes.begin(), m_decalRenderNodes.end(), (IDecalRenderNode*)pEnt);
		if (it != m_decalRenderNodes.end())
		{
			m_decalRenderNodes.erase(it);
		}
	}

	if (CClipVolumeManager* pClipVolumeManager = GetClipVolumeManager())
	{
		pClipVolumeManager->UnregisterRenderNode(pEnt);
	}

	return bFound;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 C3DEngine::GetEntityRegisterPoint(IRenderNode* pEnt)
{
	AABB aabb;
	pEnt->FillBBox(aabb);

	Vec3 vPoint;

	if (pEnt->m_dwRndFlags & ERF_REGISTER_BY_POSITION)
	{
		vPoint = pEnt->GetPos();
		vPoint.z += 0.25f;

		if (pEnt->GetRenderNodeType() != eERType_Light)
		{
			// check for valid position
			if (aabb.GetDistanceSqr(vPoint) > sqr(128.f))
			{
				Warning("I3DEngine::RegisterEntity: invalid entity position: Name: %s, Class: %s, Pos=(%.1f,%.1f,%.1f), BoxMin=(%.1f,%.1f,%.1f), BoxMax=(%.1f,%.1f,%.1f)",
				        pEnt->GetName(), pEnt->GetEntityClassName(),
				        pEnt->GetPos().x, pEnt->GetPos().y, pEnt->GetPos().z,
				        pEnt->GetBBox().min.x, pEnt->GetBBox().min.y, pEnt->GetBBox().min.z,
				        pEnt->GetBBox().max.x, pEnt->GetBBox().max.y, pEnt->GetBBox().max.z
				        );
			}
			// clamp by bbox
			vPoint.CheckMin(aabb.max);
			vPoint.CheckMax(aabb.min + Vec3(0, 0, .5f));
		}
	}
	else
		vPoint = aabb.GetCenter();

	return vPoint;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 C3DEngine::GetSunDirNormalized() const
{
	return m_vSunDirNormalized;
}

///////////////////////////////////////////////////////////////////////////////
void CVegetation::UpdateRndFlags()
{
	StatInstGroup& vegetGroup = GetStatObjGroup();

	const uint32 dwFlagsToUpdate =
	  ERF_CASTSHADOWMAPS | ERF_DYNAMIC_DISTANCESHADOWS | ERF_HIDABLE | ERF_PICKABLE
	  | ERF_SPEC_BITS_MASK | ERF_OUTDOORONLY | ERF_ACTIVE_LAYER;
	m_dwRndFlags &= ~dwFlagsToUpdate;
	m_dwRndFlags |= vegetGroup.m_dwRndFlags & (dwFlagsToUpdate | ERF_HAS_CASTSHADOWMAPS);

	IRenderNode::SetLodRatio((int)(vegetGroup.fLodDistRatio * 100.f));
	IRenderNode::SetViewDistRatio((int)(vegetGroup.fMaxViewDistRatio * 100.f));
}

///////////////////////////////////////////////////////////////////////////////
const AABB CVegetation::GetBBox() const
{
	AABB aabb;
	FillBBoxFromExtends(aabb, m_boxExtends, m_vPos);
	return aabb;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 CTerrain::GetTerrainSurfaceNormal(Vec3 vPos, float fRange, int nSID)
{
	FUNCTION_PROFILER_3DENGINE;

#ifdef SEG_WORLD
	if (nSID < 0)
	{
		nSID = FindSegment((int)vPos.x, (int)vPos.y);
		Vec3 vSegOrigin = GetSegmentOrigin(nSID);
		vPos.x -= vSegOrigin.x;
		vPos.y -= vSegOrigin.y;
	}
#else
	nSID = GetDefSID();
#endif

	fRange += 0.05f;
	Vec3 v1 = Vec3(vPos.x - fRange, vPos.y - fRange, GetZApr(vPos.x - fRange, vPos.y - fRange, nSID));
	Vec3 v2 = Vec3(vPos.x - fRange, vPos.y + fRange, GetZApr(vPos.x - fRange, vPos.y + fRange, nSID));
	Vec3 v3 = Vec3(vPos.x + fRange, vPos.y - fRange, GetZApr(vPos.x + fRange, vPos.y - fRange, nSID));
	Vec3 v4 = Vec3(vPos.x + fRange, vPos.y + fRange, GetZApr(vPos.x + fRange, vPos.y + fRange, nSID));
	return (v3 - v2).Cross(v4 - v1).GetNormalized();
}

///////////////////////////////////////////////////////////////////////////////
