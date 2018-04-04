// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ObjectsTree.h"
#include "PolygonClipContext.h"
#include "RoadRenderNode.h"
#include "Brush.h"
#include "DecalRenderNode.h"
#include "RenderMeshMerger.h"
#include "DecalManager.h"
#include "VisAreas.h"
#include <CryAnimation/ICryAnimation.h>
#include "LightEntity.h"
#include "WaterVolumeRenderNode.h"
#include "DistanceCloudRenderNode.h"
#include "MergedMeshRenderNode.h"
#include "MergedMeshGeometry.h"
#include "ShadowCache.h"
#include <CryThreading/IJobManager_JobDelegator.h>

const float COctreeNode::fMinShadowCasterViewDist = 8.f;
PodArray<COctreeNode*> COctreeNode::m_arrEmptyNodes;
PodArray<COctreeNode*> COctreeNode::m_arrStreamedInNodes;
int COctreeNode::m_nInstStreamTasksInProgress = 0;
FILE* COctreeNode::m_pFileForSyncRead = 0;
int COctreeNode::m_nNodesCounterAll = 0;
int COctreeNode::m_nNodesCounterStreamable = 0;
int COctreeNode::m_nInstCounterLoaded = 0;
void* COctreeNode::m_pRenderContentJobQueue = NULL;

DECLARE_JOB("OctreeNodeRender", TRenderContentJob, COctreeNode::RenderContentJobEntry);
typedef PROD_CONS_QUEUE_TYPE(TRenderContentJob, 1024) TRenderContentJobQueue;

#define CHECK_OBJECTS_BOX_WARNING_SIZE (1.0e+10f)

COctreeNode::~COctreeNode()
{
	m_nNodesCounterAll--;

	ReleaseObjects();

	for (int i = 0; i < 8; i++)
	{
		delete m_arrChilds[i];
		m_arrChilds[i] = NULL;
	}

	m_arrEmptyNodes.Delete(this);

	m_arrStreamedInNodes.Delete(this);

	GetObjManager()->m_arrStreamingNodeStack.Delete(this);

	if (m_pReadStream)
		m_pReadStream->Abort();

	ResetStaticInstancing();
}

void COctreeNode::SetVisArea(CVisArea* pVisArea)
{
	m_pVisArea = pVisArea;
	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
			m_arrChilds[i]->SetVisArea(pVisArea);
	}
}

extern float arrVegetation_fSpriteSwitchState[nThreadsNum];

void COctreeNode::CheckManageVegetationSprites(float fNodeDistance, int nMaxFrames, const SRenderingPassInfo& passInfo)
{
	const uint32 iMainFrameId = passInfo.GetMainFrameID();
	if ((uint32)m_nManageVegetationsFrameId >= iMainFrameId - nMaxFrames && !passInfo.IsZoomInProgress())
		return;

	C3DEngine* p3DEngine = Get3DEngine();

	m_fNodeDistance = fNodeDistance;
	m_nManageVegetationsFrameId = iMainFrameId;

	FUNCTION_PROFILER_3DENGINE;

	Vec3 sunDirReady((m_fpSunDirX - 63.5f) / 63.5f, 0.0f, (m_fpSunDirZ - 63.5f) / 63.5f);
	sunDirReady.y = sqrt(clamp_tpl(1.0f - sunDirReady.x * sunDirReady.x - sunDirReady.z * sunDirReady.z, 0.0f, 1.0f));
	if (m_fpSunDirYs)
		sunDirReady.y *= -1.0f;

	if (sunDirReady.Dot(p3DEngine->GetSunDirNormalized()) < 0.99f)
		SetCompiled(false);

	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();
	AABB objBox;

	SSectorTextureSet* pTerrainTexInfo = NULL;
	if (GetCVars()->e_VegetationUseTerrainColor)
		GetObjManager()->FillTerrainTexInfo(this, m_fNodeDistance, pTerrainTexInfo, m_objectsBox);
	/*
	   Vec3 vD = GetBBox().GetCenter() - vCamPos;
	   Vec3 vCenter = GetBBox().GetCenter();
	   Vec3 vD1 = vCenter + vD;
	   m_dwSpritesAngle = ((uint32)(atan2_tpl(vD1.x, vD1.y)*(255.0f/(2*g_PI)))) & 0xff;
	 */

	// check if we need to add some objects to sprites array
	for (CVegetation* pObj = (CVegetation*)m_arrObjects[eRNListType_Vegetation].m_pFirstNode, * pNext; pObj; pObj = pNext)
	{
		pNext = (CVegetation*)pObj->m_pNext;

		if (pObj->m_pNext)
			cryPrefetchT0SSE(pObj->m_pNext);

		pObj->FillBBox_NonVirtual(objBox);

		const float fEntDistanceSqr = Distance::Point_AABBSq(vCamPos, objBox) * passInfo.GetZoomFactor() * passInfo.GetZoomFactor();

		float fEntDistance2D = sqrt_tpl(vCamPos.GetSquaredDistance2D(pObj->m_vPos)) * passInfo.GetZoomFactor();

		StatInstGroup& vegetGroup = pObj->GetStatObjGroup();

		const float fSpriteSwitchDist = pObj->GetSpriteSwitchDist();
		float fSwitchRange = min(fSpriteSwitchDist * GetCVars()->e_LodTransitionSpriteDistRatio, GetCVars()->e_LodTransitionSpriteMinDist);
		float fLodTransitionDistband = 1.f;

		if (pObj->m_pSpriteInfo)
		{
			CStatObj* pStatObj = vegetGroup.GetStatObj();

			if (fEntDistance2D < (fSpriteSwitchDist - fSwitchRange) && pObj && pObj->m_pTempData)
			{
				int nLodA;

				nLodA = CLAMP(pObj->m_pTempData->userData.nWantedLod, (uint32)pStatObj->GetMinUsableLod(), (uint32)pStatObj->m_nMaxUsableLod);
				nLodA = pStatObj->FindNearesLoadedLOD(nLodA);

				// TODO: start dissolve transition to 3d lod
			}

			if (pObj->m_pTempData)
			{
				// TODO: update dissolve transition to 3d lod, detect finish oif transition
			}

			if (fEntDistanceSqr > sqr(pObj->m_fWSMaxViewDist * 1.1f))
			{
				SAFE_DELETE(pObj->m_pSpriteInfo);

				UnlinkObject(pObj);
				LinkObject(pObj, eERType_Vegetation); //We know that only eERType_Vegetation can get into the vegetation list, see GetRenderNodeListId()

				SetCompiled(false);

				continue;
			}

			float dist3D = fSpriteSwitchDist - fSwitchRange + fLodTransitionDistband;

			pObj->UpdateSpriteInfo(*pObj->m_pSpriteInfo, 0, pTerrainTexInfo, passInfo);
			pObj->m_pSpriteInfo->ucShow3DModel = (fEntDistance2D < dist3D);
		}
		else if (!pObj->m_pInstancingInfo)
		{
			if (fEntDistance2D > (fSpriteSwitchDist - fSwitchRange) && fEntDistance2D + fLodTransitionDistband < pObj->m_fWSMaxViewDist)
			{
				UnlinkObject(pObj);
				LinkObject(pObj, eERType_Vegetation, false); //We know that only eERType_Vegetation can get into the vegetation list, see GetRenderNodeListId()

				assert(pObj->m_pSpriteInfo == NULL);
				pObj->m_pSpriteInfo = new SVegetationSpriteInfo;
				SVegetationSpriteInfo& si = *pObj->m_pSpriteInfo;

				if (pObj->m_pTempData)
				{
					// TODO: start lod transition into sprite
				}

				pObj->UpdateSpriteInfo(si, 0.0f, pTerrainTexInfo, passInfo);
				pObj->m_pSpriteInfo->ucShow3DModel = (fEntDistance2D < fSpriteSwitchDist);

				SetCompiled(false);
			}
		}
	}
}

void COctreeNode::Render_Object_Nodes(bool bNodeCompletelyInFrustum, int nRenderMask, const Vec3& vAmbColor, const SRenderingPassInfo& passInfo)
{
	assert(nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS);

	const CCamera& rCam = passInfo.GetCamera();

	if (m_nOccludedFrameId == passInfo.GetFrameID())
		return;

	if (!bNodeCompletelyInFrustum && !rCam.IsAABBVisible_EH(m_objectsBox, &bNodeCompletelyInFrustum))
		return;

	const Vec3& vCamPos = rCam.GetPosition();

	float fNodeDistanceSq = Distance::Point_AABBSq(vCamPos, m_objectsBox) * sqr(passInfo.GetZoomFactor());

	if (fNodeDistanceSq > sqr(m_fObjectsMaxViewDist))
		return;

	float fNodeDistance = sqrt_tpl(fNodeDistanceSq);

	if (m_nLastVisFrameId != passInfo.GetFrameID() && m_pParent)
	{
		if (GetObjManager()->IsBoxOccluded(m_objectsBox, fNodeDistance, &m_occlusionTestClient, m_pVisArea != NULL, eoot_OCCELL, passInfo))
		{
			m_nOccludedFrameId = passInfo.GetFrameID();
			return;
		}
	}

	m_nLastVisFrameId = passInfo.GetFrameID();

	if (!IsCompiled())
		CompileObjects();

	CheckUpdateStaticInstancing();

	if (GetCVars()->e_ObjectsTreeBBoxes)
	{
		if (GetCVars()->e_ObjectsTreeBBoxes == 1)
		{
			const AABB& nodeBox = GetNodeBox();
			DrawBBox(nodeBox, Col_Blue);
		}
		if (GetCVars()->e_ObjectsTreeBBoxes == 2)
			DrawBBox(m_objectsBox, Col_Red);
	}

	m_fNodeDistance = fNodeDistance;
	m_bNodeCompletelyInFrustum = bNodeCompletelyInFrustum;

	if (GetCVars()->e_VegetationSpritesBatching && GetCVars()->e_VegetationSprites)
		CheckManageVegetationSprites(fNodeDistance, (int)(fNodeDistance * 0.2f), passInfo);

	if (HasAnyRenderableCandidates(passInfo))
	{
		// when using the occlusion culler, push the work to the jobs doing the occlusion checks, else just compute in main thread
		if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && GetCVars()->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
		{
			// make sure the affected lights array is init before starting parallel execution
			CheckInitAffectingLights(passInfo);
			GetObjManager()->PushIntoCullQueue(SCheckOcclusionJobData::CreateOctreeJobData(this, nRenderMask, vAmbColor, passInfo));
		}
		else
		{
			RenderContentJobEntry(nRenderMask, vAmbColor, passInfo);
		}

		passInfo.GetRendItemSorter().IncreaseOctreeCounter();
	}

	int nFirst =
	  ((vCamPos.x > m_vNodeCenter.x) ? 4 : 0) |
	  ((vCamPos.y > m_vNodeCenter.y) ? 2 : 0) |
	  ((vCamPos.z > m_vNodeCenter.z) ? 1 : 0);

	if (m_arrChilds[nFirst])
		m_arrChilds[nFirst]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 1])
		m_arrChilds[nFirst ^ 1]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 2])
		m_arrChilds[nFirst ^ 2]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 4])
		m_arrChilds[nFirst ^ 4]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 3])
		m_arrChilds[nFirst ^ 3]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 5])
		m_arrChilds[nFirst ^ 5]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 6])
		m_arrChilds[nFirst ^ 6]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);

	if (m_arrChilds[nFirst ^ 7])
		m_arrChilds[nFirst ^ 7]->Render_Object_Nodes(bNodeCompletelyInFrustum, nRenderMask, vAmbColor, passInfo);
}

void COctreeNode::RenderDebug()
{
	if (GetCVars()->e_ObjectsTreeBBoxes == 3 || GetCVars()->e_ObjectsTreeBBoxes == (int)GetNodeBox().GetSize().x)
	{
		const AABB& nodeBox = GetNodeBox();

		if (m_nFileDataOffset && m_eStreamingStatus == ecss_NotLoaded)
			DrawBBox(nodeBox, Col_Red);
		else if (m_nFileDataOffset && m_eStreamingStatus == ecss_InProgress)
			DrawBBox(nodeBox, Col_Yellow);
		else if (m_nFileDataOffset && m_eStreamingStatus == ecss_Ready)
			DrawBBox(nodeBox, Col_Green);
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->RenderDebug();
}

void COctreeNode::GetStreamedInNodesNum(int& nAllStreamable, int& nReady)
{
	if (m_nFileDataOffset && m_eStreamingStatus == ecss_Ready)
		nReady++;

	if (m_nFileDataOffset)
		nAllStreamable++;

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GetStreamedInNodesNum(nAllStreamable, nReady);
}

void COctreeNode::CompileObjects()
{
	FUNCTION_PROFILER_3DENGINE;

	m_lstCasters.Clear();

	m_bStaticInstancingIsDirty = true;

	CheckUpdateStaticInstancing();

	float fObjMaxViewDistance = 0;

	size_t numCasters = 0;
	const unsigned int nSkipShadowCastersRndFlags = ERF_HIDDEN | ERF_COLLISION_PROXY | ERF_RAYCAST_PROXY | ERF_STATIC_INSTANCING; // shadow casters with these render flags are ignored

	for (int l = 0; l < eRNListType_ListsNum; l++)
	{
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			auto nFlags = pObj->GetRndFlags();

			IF (nFlags & nSkipShadowCastersRndFlags, 0)
				continue;

			IF (GetCVars()->e_ShadowsPerObject && gEnv->p3DEngine->GetPerObjectShadow(pObj), 0)
				continue;

			EERType eRType = pObj->GetRenderNodeType();
			float WSMaxViewDist = pObj->GetMaxViewDist();

			if (nFlags & ERF_CASTSHADOWMAPS && WSMaxViewDist > fMinShadowCasterViewDist && eRType != eERType_Light)
			{
				++numCasters;
			}
		}
	}

	m_lstCasters.reserve(numCasters);

	CObjManager* pObjManager = GetObjManager();

	// update node
	for (int l = 0; l < eRNListType_ListsNum; l++)
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode, * pNext; pObj; pObj = pNext)
		{
			pNext = pObj->m_pNext;

			IF (pObj->m_dwRndFlags & ERF_HIDDEN, 0)
				continue;

			// update vegetation instances data
			EERType eRType = pObj->GetRenderNodeType();
			if (eRType == eERType_Vegetation)
			{
				CVegetation* pInst = (CVegetation*)pObj;
				pInst->UpdateRndFlags();
			}

			// update max view distances
			const float fNewMaxViewDist = pObj->GetMaxViewDist();
			pObj->m_fWSMaxViewDist = fNewMaxViewDist;

			// update REQUIRES_FORWARD_RENDERING flag
			//IF(GetCVars()->e_ShadowsOnAlphaBlend,0)
			{
				pObj->m_nInternalFlags &= ~(IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP);
				if (eRType != eERType_Light &&
				    eRType != eERType_FogVolume &&
				    eRType != eERType_Decal &&
				    eRType != eERType_Road &&
				    eRType != eERType_DistanceCloud &&
				    eRType != eERType_CloudBlocker)
				{
					if (eRType == eERType_ParticleEmitter)
						pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP;

					if (CMatInfo* pMatInfo = (CMatInfo*)pObj->GetMaterial())
					{
						if (pMatInfo->IsForwardRenderingRequired())
							pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;

						if (pMatInfo->IsNearestCubemapRequired())
							pObj->m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
					}

					if (eRType == eERType_MovableBrush)
					{
						if (IStatObj* pStatObj = pObj->GetEntityStatObj())
						{
							if (CMatInfo* pMat = (CMatInfo*)pStatObj->GetMaterial())
							{
								if (pMat->IsForwardRenderingRequired())
									pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;
								if (pMat->IsNearestCubemapRequired())
									pObj->m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
							}
						}
					}

					if (!(pObj->m_nInternalFlags & (IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP)))
					{
						CompileCharacter(pObj->GetEntityCharacter(), pObj->m_nInternalFlags);
					}
				}
			}

			auto nFlags = pObj->GetRndFlags();

			// fill shadow casters list
			const bool bHasPerObjectShadow = GetCVars()->e_ShadowsPerObject && gEnv->p3DEngine->GetPerObjectShadow(pObj);
			if (!(nFlags & nSkipShadowCastersRndFlags) && nFlags & ERF_CASTSHADOWMAPS && fNewMaxViewDist > fMinShadowCasterViewDist &&
			    eRType != eERType_Light && !bHasPerObjectShadow)
			{
				COctreeNode* pNode = this;
				while (pNode && !(pNode->m_renderFlags & ERF_CASTSHADOWMAPS))
				{
					pNode->m_renderFlags |= ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS;
					pNode = pNode->m_pParent;
				}

				float fMaxCastDist = fNewMaxViewDist * GetCVars()->e_ShadowsCastViewDistRatio;
				m_lstCasters.Add(SCasterInfo(pObj, fMaxCastDist, eRType));
			}

			fObjMaxViewDistance = max(fObjMaxViewDistance, fNewMaxViewDist);
		}

	if (fObjMaxViewDistance > m_fObjectsMaxViewDist)
	{
		COctreeNode* pNode = this;
		while (pNode)
		{
			pNode->m_fObjectsMaxViewDist = max(pNode->m_fObjectsMaxViewDist, fObjMaxViewDistance);
			pNode = pNode->m_pParent;
		}
	}

	SetCompiled(true);

	const Vec3& sunDir = Get3DEngine()->GetSunDirNormalized();
	m_fpSunDirX = (uint32) (sunDir.x * 63.5f + 63.5f);
	m_fpSunDirZ = (uint32) (sunDir.z * 63.5f + 63.5f);
	m_fpSunDirYs = sunDir.y < 0.0f ? 1 : 0;
}

void COctreeNode::UpdateStaticInstancing()
{
	FUNCTION_PROFILER_3DENGINE;

	// clear
	if (m_pStaticInstancingInfo)
	{
		for (auto it = m_pStaticInstancingInfo->begin(); it != m_pStaticInstancingInfo->end(); ++it)
		{
			PodArray<SNodeInstancingInfo>*& pInfo = it->second;

			pInfo->Clear();
		}
	}

	// group objects by CStatObj *
	for (IRenderNode* pObj = m_arrObjects[eRNListType_Vegetation].m_pFirstNode, * pNext; pObj; pObj = pNext)
	{
		pNext = pObj->m_pNext;

		CVegetation* pInst = (CVegetation*)pObj;

		pObj->m_dwRndFlags &= ~ERF_STATIC_INSTANCING;

		if (pInst->m_pInstancingInfo)
		{
			SAFE_DELETE(pInst->m_pInstancingInfo)

			pInst->InvalidatePermanentRenderObject();
		}

		IF (pObj->m_dwRndFlags & ERF_HIDDEN, 0)
			continue;

		Matrix34A objMatrix;
		CStatObj* pStatObj = (CStatObj*)pInst->GetEntityStatObj(0, &objMatrix);
		/*
		   int nLodA = -1;
		   {
		   const Vec3 vCamPos = GetSystem()->GetViewCamera().GetPosition();

		   const float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, pInst->GetBBox()));
		   int wantedLod = CObjManager::GetObjectLOD(pInst, fEntDistance);

		   int minUsableLod = pStatObj->GetMinUsableLod();
		   int maxUsableLod = (int)pStatObj->m_nMaxUsableLod;
		   nLodA = CLAMP(wantedLod, minUsableLod, maxUsableLod);
		   if(!(pStatObj->m_nFlags & STATIC_OBJECT_COMPOUND))
		    nLodA = pStatObj->FindNearesLoadedLOD(nLodA);
		   }

		   if(nLodA<0)
		   continue;

		   pStatObj = (CStatObj *)pStatObj->GetLodObject(nLodA);
		 */
		if (!m_pStaticInstancingInfo)
			m_pStaticInstancingInfo = new std::map<std::pair<IStatObj*, IMaterial*>, PodArray<SNodeInstancingInfo>*>;

		std::pair<IStatObj*, IMaterial*> pairKey(pStatObj, pInst->GetMaterial());

		PodArray<SNodeInstancingInfo>*& pInfo = (*m_pStaticInstancingInfo)[pairKey];

		if (!pInfo)
			pInfo = new PodArray<SNodeInstancingInfo>;

		SNodeInstancingInfo ii;
		ii.pRNode = pInst;
		ii.nodeMatrix = objMatrix;

		pInfo->Add(ii);
	}

	// mark
	if (m_pStaticInstancingInfo)
	{
		for (auto it = m_pStaticInstancingInfo->begin(); it != m_pStaticInstancingInfo->end(); ++it)
		{
			PodArray<SNodeInstancingInfo>*& pInfo = it->second;

			if (pInfo->Count() > GetCVars()->e_StaticInstancingMinInstNum)
			{
				CVegetation* pFirstNode = pInfo->GetAt(0).pRNode;

				// put instancing into one of existing vegetations
				PodArrayAABB<CRenderObject::SInstanceInfo>* pInsts = pFirstNode->m_pInstancingInfo = new PodArrayAABB<CRenderObject::SInstanceInfo>;
				pInsts->PreAllocate(pInfo->Count(), pInfo->Count());
				pInsts->m_aabbBox.Reset();

				pFirstNode->InvalidatePermanentRenderObject();

				for (int i = 0; i < pInfo->Count(); i++)
				{
					SNodeInstancingInfo& ii = pInfo->GetAt(i);

					if (i)
					{
						ii.pRNode->SetRndFlags(ERF_STATIC_INSTANCING, true);

						// keep inactive objects in the end of the list
						UnlinkObject(ii.pRNode);
						LinkObject(ii.pRNode, eERType_Vegetation, false);

						for (int s = 0; s < m_lstCasters.Count(); s++)
						{
							if (m_lstCasters[s].pNode == ii.pRNode)
							{
								m_lstCasters.Delete(s);
								break;
							}
						}
					}

					CStatObj* pStatObj = ii.pRNode->GetStatObj();
					const StatInstGroup& vegetGroup = ii.pRNode->GetStatObjGroup();

					(*pInsts)[i].m_Matrix = ii.nodeMatrix;

					pInsts->m_aabbBox.Add(ii.pRNode->GetBBox());
				}
			}
			else
			{
				pInfo->Clear();
			}
		}
	}

	m_bStaticInstancingIsDirty = false;
}

void COctreeNode::AddLightSource(CDLight* pSource, const SRenderingPassInfo& passInfo)
{
	bool bIsVisible = false;
	if (pSource->m_Flags & DLF_DEFERRED_CUBEMAPS)
	{
		OBB obb(OBB::CreateOBBfromAABB(Matrix33(pSource->m_ObjMatrix), AABB(-pSource->m_ProbeExtents, pSource->m_ProbeExtents)));
		bIsVisible = passInfo.GetCamera().IsOBBVisible_F(pSource->m_Origin, obb);
	}
	else if (pSource->m_Flags & DLF_AREA_LIGHT)
	{
		// OBB test for area lights.
		Vec3 vBoxMax(pSource->m_fRadius, pSource->m_fRadius + pSource->m_fAreaWidth, pSource->m_fRadius + pSource->m_fAreaHeight);
		Vec3 vBoxMin(-0.1f, -(pSource->m_fRadius + pSource->m_fAreaWidth), -(pSource->m_fRadius + pSource->m_fAreaHeight));

		OBB obb(OBB::CreateOBBfromAABB(Matrix33(pSource->m_ObjMatrix), AABB(vBoxMin, vBoxMax)));
		bIsVisible = passInfo.GetCamera().IsOBBVisible_F(pSource->m_Origin, obb);
	}
	else
		bIsVisible = m_objectsBox.IsOverlapSphereBounds(pSource->m_Origin, pSource->m_fRadius);

	if (!bIsVisible)
		return;

	CheckInitAffectingLights(passInfo);

	if (m_lstAffectingLights.Find(pSource) < 0)
	{
		m_lstAffectingLights.Add(pSource);

		for (int i = 0; i < 8; i++)
			if (m_arrChilds[i])
				m_arrChilds[i]->AddLightSource(pSource, passInfo);
	}
}

bool IsAABBInsideHull(const SPlaneObject* pHullPlanes, int nPlanesNum, const AABB& aabbBox);
bool IsSphereInsideHull(const SPlaneObject* pHullPlanes, int nPlanesNum, const Sphere& objSphere);

void COctreeNode::FillShadowCastersList(bool bNodeCompletellyInFrustum, CDLight* pLight, ShadowMapFrustum* pFr, PodArray<SPlaneObject>* pShadowHull, uint32 nRenderNodeFlags, const SRenderingPassInfo& passInfo)
{
	if (GetCVars()->e_Objects)
	{
		if (m_renderFlags & ERF_CASTSHADOWMAPS)
		{
			FRAME_PROFILER("COctreeNode::FillShadowMapCastersList", GetSystem(), PROFILE_3DENGINE);

			CRY_ALIGN(64) ShadowMapFrustumParams params;
			params.pLight = pLight;
			params.pFr = pFr;
			params.pShadowHull = pShadowHull;
			params.passInfo = &passInfo;
			params.vCamPos = passInfo.GetCamera().GetPosition();
			params.bSun = (pLight->m_Flags & DLF_SUN) != 0;
			params.nRenderNodeFlags = nRenderNodeFlags;

			FillShadowMapCastersList(params, bNodeCompletellyInFrustum);
		}
	}
}

void COctreeNode::FillShadowMapCastersList(const ShadowMapFrustumParams& params, bool bNodeCompletellyInFrustum)
{
	if (!bNodeCompletellyInFrustum && !params.pFr->IntersectAABB(m_objectsBox, &bNodeCompletellyInFrustum))
		return;

	const int frameID = params.passInfo->GetFrameID();
	if (params.bSun && bNodeCompletellyInFrustum)
		nFillShadowCastersSkipFrameId = frameID;

	if (params.pShadowHull && !IsAABBInsideHull(params.pShadowHull->GetElements(), params.pShadowHull->Count(), m_objectsBox))
	{
		nFillShadowCastersSkipFrameId = frameID;
		return;
	}

	if (!IsCompiled())
	{
		CompileObjects();
	}

	PrefetchLine(&m_lstCasters, 0);

	const float fShadowsCastViewDistRatio = GetCVars()->e_ShadowsCastViewDistRatio;
	if (fShadowsCastViewDistRatio != 0.0f)
	{
		float fNodeDistanceSqr = Distance::Point_AABBSq(params.vCamPos, m_objectsBox);
		if (fNodeDistanceSqr > sqr(m_fObjectsMaxViewDist * fShadowsCastViewDistRatio))
		{
			nFillShadowCastersSkipFrameId = frameID;
			return;
		}
	}

	PrefetchLine(m_lstCasters.begin(), 0);
	PrefetchLine(m_lstCasters.begin(), 128);

	IRenderNode* pNotCaster = ((CLightEntity*)params.pLight->m_pOwner)->m_pNotCaster;
	bool bParticleShadows = params.bSun && params.pFr->nShadowMapLod < GetCVars()->e_ParticleShadowsNumGSMs;

	if (m_arrChilds[0])
	{
		PrefetchLine(&m_arrChilds[0]->m_nLightMaskFrameId, 0);
	}

	SCasterInfo* pCastersEnd = m_lstCasters.end();
	for (SCasterInfo* pCaster = m_lstCasters.begin(); pCaster < pCastersEnd; pCaster++)
	{
#ifdef FEATURE_SVO_GI
		if (!GetCVars()->e_svoTI_Apply || !GetCVars()->e_svoTI_InjectionMultiplier || (params.pFr->nShadowMapLod != GetCVars()->e_svoTI_GsmCascadeLod))
#endif
		if (params.bSun && pCaster->nGSMFrameId == frameID && params.pShadowHull)
			continue;
		if (!IsRenderNodeTypeEnabled(pCaster->nRType))
			continue;
		if (pCaster->pNode == NULL || pCaster->pNode == pNotCaster)
			continue;
		if (!bParticleShadows && (pCaster->nRType == eERType_ParticleEmitter))
			continue;
		if ((pCaster->nRenderNodeFlags & params.nRenderNodeFlags) == 0)
			continue;

		float fDistanceSq = Distance::Point_PointSq(params.vCamPos, pCaster->objSphere.center);
		if (fDistanceSq > sqr(pCaster->fMaxCastingDist + pCaster->objSphere.radius))
		{
			pCaster->nGSMFrameId = frameID;
			continue;
		}

		bool bObjCompletellyInFrustum = bNodeCompletellyInFrustum;
		if (!bObjCompletellyInFrustum && !params.pFr->IntersectAABB(pCaster->objBox, &bObjCompletellyInFrustum))
			continue;
		if (params.bSun && bObjCompletellyInFrustum)
			pCaster->nGSMFrameId = frameID;

		if (params.bSun && bObjCompletellyInFrustum)
			pCaster->nGSMFrameId = frameID;
		if (params.pShadowHull && !IsSphereInsideHull(params.pShadowHull->GetElements(), params.pShadowHull->Count(), pCaster->objSphere))
		{
			pCaster->nGSMFrameId = frameID;
			continue;
		}

		if (pCaster->bCanExecuteAsRenderJob)
		{
			params.pFr->jobExecutedCastersList.Add(pCaster->pNode);
		}
		else
			params.pFr->castersList.Add(pCaster->pNode);

		// This code does not exist yet!
		//if(pCaster->nRType == eERType_ParticleEmitter)
		//((CParticleEmitter*)pCaster->pNode)->AddUpdateParticlesJob();
	}

	enum { maxNodeNum = 7 };
	for (int i = 0; i <= maxNodeNum; i++)
	{
		const bool bPrefetch = i < maxNodeNum && !!m_arrChilds[i + 1];
		if (bPrefetch)
		{
			PrefetchLine(&m_arrChilds[i + 1]->m_nLightMaskFrameId, 0);
		}

		if (m_arrChilds[i] && (m_arrChilds[i]->m_renderFlags & ERF_CASTSHADOWMAPS))
		{
			bool bContinue = m_arrChilds[i]->nFillShadowCastersSkipFrameId != frameID;

#ifdef FEATURE_SVO_GI
			if (GetCVars()->e_svoTI_Apply && GetCVars()->e_svoTI_InjectionMultiplier && (params.pFr->nShadowMapLod == GetCVars()->e_svoTI_GsmCascadeLod))
				bContinue = true;
#endif

			if (!params.bSun || !params.pShadowHull || bContinue)
				m_arrChilds[i]->FillShadowMapCastersList(params, bNodeCompletellyInFrustum);
		}
	}
}

void COctreeNode::MarkAsUncompiled(const IRenderNode* pRenderNode)
{
	if (pRenderNode)
	{
		for (int l = 0; l < eRNListType_ListsNum; l++)
		{
			for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
			{
				if (pObj == pRenderNode)
				{
					SetCompiled(false);
					break;
				}
			}
		}
	}
	else
		SetCompiled(false);

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->MarkAsUncompiled(pRenderNode);
}

AABB COctreeNode::GetShadowCastersBox(const AABB* pBBox, const Matrix34* pShadowSpaceTransform)
{
	if (!IsCompiled())
		CompileObjects();

	AABB result(AABB::RESET);
	if (!pBBox || Overlap::AABB_AABB(*pBBox, GetObjectsBBox()))
	{
		for (size_t i = 0; i < m_lstCasters.size(); ++i)
		{
			AABB casterBox = m_lstCasters[i].objBox;
			if (!pBBox || Overlap::AABB_AABB(*pBBox, casterBox))
			{
				if (pShadowSpaceTransform)
					casterBox = AABB::CreateTransformedAABB(*pShadowSpaceTransform, casterBox);

				result.Add(casterBox);
			}
		}

		for (int i = 0; i < 8; i++)
		{
			if (m_arrChilds[i])
				result.Add(m_arrChilds[i]->GetShadowCastersBox(pBBox, pShadowSpaceTransform));
		}
	}

	return result;
}

COctreeNode* COctreeNode::FindNodeContainingBox(const AABB& objBox)
{
	{
		const AABB& nodeBox = GetNodeBox();
		if (!nodeBox.IsContainSphere(objBox.min, -0.01f) || !nodeBox.IsContainSphere(objBox.max, -0.01f))
			return NULL;
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			if (COctreeNode* pFoundNode = m_arrChilds[i]->FindNodeContainingBox(objBox))
				return pFoundNode;

	return this;
}

void COctreeNode::MoveObjectsIntoList(PodArray<SRNInfo>* plstResultEntities, const AABB* pAreaBox,
                                      bool bRemoveObjects, bool bSkipDecals, bool bSkip_ERF_NO_DECALNODE_DECALS, bool bSkipDynamicObjects,
                                      EERType eRNType)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pAreaBox && !Overlap::AABB_AABB(m_objectsBox, *pAreaBox))
		return;

	for (int l = 0; l < eRNListType_ListsNum; l++)
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode, * pNext; pObj; pObj = pNext)
		{
			pNext = pObj->m_pNext;

			if (eRNType < eERType_TypesNum && pObj->GetRenderNodeType() != eRNType)
				continue;

			if (bSkipDecals && pObj->GetRenderNodeType() == eERType_Decal)
				continue;

			if (bSkip_ERF_NO_DECALNODE_DECALS && pObj->GetRndFlags() & ERF_NO_DECALNODE_DECALS)
				continue;

			if (bSkipDynamicObjects)
			{
				EERType eRType = pObj->GetRenderNodeType();

				if (pObj->GetRndFlags() & ERF_MOVES_EVERY_FRAME)
					continue;

				if (
				  eRType != eERType_Brush &&
				  eRType != eERType_Vegetation)
					continue;
			}

			if (pAreaBox && !Overlap::AABB_AABB(pObj->GetBBox(), *pAreaBox))
				continue;

			if (bRemoveObjects)
			{
				UnlinkObject(pObj);

				SetCompiled(false);
			}

			plstResultEntities->Add(pObj);
		}

	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
		{
			m_arrChilds[i]->MoveObjectsIntoList(plstResultEntities, pAreaBox, bRemoveObjects, bSkipDecals, bSkip_ERF_NO_DECALNODE_DECALS, bSkipDynamicObjects, eRNType);
		}
	}
}

void COctreeNode::DeleteObjectsByFlag(int nRndFlag)
{
	FUNCTION_PROFILER_3DENGINE;
	for (int l = 0; l < eRNListType_ListsNum; l++)
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode, * pNext; pObj; pObj = pNext)
		{
			pNext = pObj->m_pNext;

			if (pObj->GetRndFlags() & nRndFlag)
				DeleteObject(pObj);
		}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->DeleteObjectsByFlag(nRndFlag);
}

void COctreeNode::UnregisterEngineObjectsInArea(const SHotUpdateInfo* pExportInfo, PodArray<IRenderNode*>& arrUnregisteredObjects, bool bOnlyEngineObjects)
{
	FUNCTION_PROFILER_3DENGINE;

	const AABB* pAreaBox = (pExportInfo && !pExportInfo->areaBox.IsReset()) ? &pExportInfo->areaBox : NULL;

	{
		const AABB& nodeBox = GetNodeBox();
		if (pAreaBox && !Overlap::AABB_AABB(nodeBox, *pAreaBox))
			return;
	}

	uint32 nObjTypeMask = pExportInfo ? pExportInfo->nObjTypeMask : (uint32) ~0;

	for (int l = 0; l < eRNListType_ListsNum; l++)
	{
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode, * pNext; pObj; pObj = pNext)
		{
			pNext = pObj->m_pNext;

			EERType eType = pObj->GetRenderNodeType();

			if (bOnlyEngineObjects)
			{
				if (!(nObjTypeMask & (1 << eType)))
					continue;

				if ((eType == eERType_Vegetation && !(pObj->GetRndFlags() & ERF_PROCEDURAL)) ||
				    eType == eERType_Brush ||
				    eType == eERType_Decal ||
				    eType == eERType_WaterVolume ||
				    eType == eERType_Road ||
				    eType == eERType_DistanceCloud ||
				    eType == eERType_WaterWave)
				{
					Get3DEngine()->UnRegisterEntityAsJob(pObj);
					arrUnregisteredObjects.Add(pObj);
					SetCompiled(false);
				}
			}
			else
			{
				Get3DEngine()->UnRegisterEntityAsJob(pObj);
				arrUnregisteredObjects.Add(pObj);
				SetCompiled(false);
			}
		}
	}

	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
			m_arrChilds[i]->UnregisterEngineObjectsInArea(pExportInfo, arrUnregisteredObjects, bOnlyEngineObjects);
	}
}

int COctreeNode::PhysicalizeInBox(const AABB& bbox)
{
	if (!Overlap::AABB_AABB(m_objectsBox, bbox))
		return 0;

	struct _Op
	{
		void operator()(IRenderNode* pObj, const AABB& _bbox, int checkActive, int& _nCount) const
		{
			const AABB& objBox = pObj->GetBBox();
			if (Overlap::AABB_AABB(_bbox, objBox) &&
			    max(objBox.max.x - objBox.min.x, objBox.max.y - objBox.min.y) <=
			    ((C3DEngine*)gEnv->p3DEngine)->GetCVars()->e_OnDemandMaxSize)
			{
				if (!pObj->GetPhysics() && ((checkActive && pObj->GetRndFlags() & ERF_ACTIVE_LAYER) || (!checkActive)))
					pObj->Physicalize(true);
				if (pObj->GetPhysics())
					pObj->GetPhysics()->AddRef(), _nCount++;
			}
		}
	};

	int nCount = 0;
	_Op physicalize;
	if (GetCVars()->e_OnDemandPhysics & 0x1)
		for (IRenderNode* pObj = m_arrObjects[eRNListType_Vegetation].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			assert(pObj->GetRenderNodeType() == eERType_Vegetation);
			physicalize(pObj, bbox, false, nCount);
		}
	if (GetCVars()->e_OnDemandPhysics & 0x2)
		for (IRenderNode* pObj = m_arrObjects[eRNListType_Brush].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			assert(pObj->GetRenderNodeType() == eERType_Brush);
			physicalize(pObj, bbox, true, nCount);
		}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			nCount += m_arrChilds[i]->PhysicalizeInBox(bbox);

	return nCount;
}

int COctreeNode::DephysicalizeInBox(const AABB& bbox)
{
	if (!Overlap::AABB_AABB(m_objectsBox, bbox))
		return 0;

	struct _Op
	{
		void operator()(IRenderNode* pObj, const AABB& _bbox) const
		{
			const AABB& objBox = pObj->GetBBox();
			if (Overlap::AABB_AABB(_bbox, objBox) &&
			    max(objBox.max.x - objBox.min.x, objBox.max.y - objBox.min.y) <=
			    ((C3DEngine*)gEnv->p3DEngine)->GetCVars()->e_OnDemandMaxSize)
				pObj->Dephysicalize(true);
		}
	};

	int nCount = 0;
	_Op dephysicalize;
	if (GetCVars()->e_OnDemandPhysics & 0x1)
		for (IRenderNode* pObj = m_arrObjects[eRNListType_Vegetation].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			assert(pObj->GetRenderNodeType() == eERType_Vegetation);
			dephysicalize(pObj, bbox);
		}
	if (GetCVars()->e_OnDemandPhysics & 0x2)
		for (IRenderNode* pObj = m_arrObjects[eRNListType_Brush].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			assert(pObj->GetRenderNodeType() == eERType_Brush);
			dephysicalize(pObj, bbox);
		}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			nCount += m_arrChilds[i]->DephysicalizeInBox(bbox);

	return nCount;
}

int COctreeNode::PhysicalizeOfType(ERNListType listType, bool bInstant)
{
	for (IRenderNode* pObj = m_arrObjects[listType].m_pFirstNode, * pNext; pObj; pObj = pNext)
	{
		pNext = pObj->m_pNext;
		pObj->Physicalize(bInstant);
	}

	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
			m_arrChilds[i]->PhysicalizeOfType(listType, bInstant);
	}

	return 0;
}

int COctreeNode::DePhysicalizeOfType(ERNListType listType, bool bInstant)
{
	for (IRenderNode* pObj = m_arrObjects[listType].m_pFirstNode, * pNext; pObj; pObj = pNext)
	{
		pNext = pObj->m_pNext;
		pObj->Dephysicalize(bInstant);
	}

	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
			m_arrChilds[i]->DePhysicalizeOfType(listType, bInstant);
	}

	return 0;
}

int COctreeNode::GetObjectsCount(EOcTeeNodeListType eListType)
{
	int nCount = 0;

	switch (eListType)
	{
	case eMain:
		for (int l = 0; l < eRNListType_ListsNum; l++)
			for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
				nCount++;
		break;
	case eCasters:
		for (int l = 0; l < eRNListType_ListsNum; l++)
			for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
				if (pObj->GetRndFlags() & ERF_CASTSHADOWMAPS)
					nCount++;
		break;
	case eSprites:
		for (IRenderNode* pObj = m_arrObjects[eRNListType_Vegetation].m_pFirstNode; pObj; pObj = pObj->m_pNext)
			if (static_cast<CVegetation*>(pObj)->m_pSpriteInfo)
				++nCount;
		break;
	case eLights:
		nCount = m_lstAffectingLights.Count();
		break;
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			nCount += m_arrChilds[i]->GetObjectsCount(eListType);

	return nCount;
}

void COctreeNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	for (int l = 0; l < eRNListType_ListsNum; l++)
	{
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			EERType eType = pObj->GetRenderNodeType();
			if (!(eType == eERType_Vegetation && pObj->GetRndFlags() & ERF_PROCEDURAL))
				pObj->GetMemoryUsage(pSizer);
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "ObjLists");

		pSizer->AddObject(m_lstCasters);
		pSizer->AddObject(m_lstAffectingLights);
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GetMemoryUsage(pSizer);

	if (pSizer)
		pSizer->AddObject(this, sizeof(*this));
}

void COctreeNode::UpdateTerrainNodes(CTerrainNode* pParentNode)
{
	if (pParentNode != 0)
		SetTerrainNode(pParentNode->FindMinNodeContainingBox(GetNodeBox()));
	else if (m_nSID >= 0 && GetTerrain() != 0)
		SetTerrainNode(GetTerrain()->FindMinNodeContainingBox(GetNodeBox(), m_nSID));
	else
		SetTerrainNode(NULL);

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->UpdateTerrainNodes();
}

void C3DEngine::GetObjectsByTypeGlobal(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamReady, uint64 dwFlags)
{
	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
		if (Get3DEngine()->IsSegmentSafeToUse(nSID))
			Get3DEngine()->m_pObjectsTree[nSID]->GetObjectsByType(lstObjects, objType, pBBox, pInstStreamReady, dwFlags);
}

void C3DEngine::MoveObjectsIntoListGlobal(PodArray<SRNInfo>* plstResultEntities, const AABB* pAreaBox,
                                          bool bRemoveObjects, bool bSkipDecals, bool bSkip_ERF_NO_DECALNODE_DECALS, bool bSkipDynamicObjects,
                                          EERType eRNType)
{
	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
		if (Get3DEngine()->IsSegmentSafeToUse(nSID))
			Get3DEngine()->m_pObjectsTree[nSID]->MoveObjectsIntoList(plstResultEntities, pAreaBox, bRemoveObjects, bSkipDecals, bSkip_ERF_NO_DECALNODE_DECALS, bSkipDynamicObjects, eRNType);
}

void COctreeNode::ActivateObjectsLayer(uint16 nLayerId, bool bActivate, bool bPhys, IGeneralMemoryHeap* pHeap, const AABB& layerBox)
{
	if (nLayerId && nLayerId < 0xFFFF && !Overlap::AABB_AABB(layerBox, GetObjectsBBox()))
		return;

	for (IRenderNode* pObj = m_arrObjects[eRNListType_Brush].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if (pObj->GetRenderNodeType() == eERType_Brush)
		{
			CBrush* pBrush = (CBrush*)pObj;
			if (pBrush->m_nLayerId == nLayerId || nLayerId == uint16(~0))
			{
				if ((bActivate && pBrush->m_dwRndFlags & ERF_HIDDEN) || (!bActivate && !(pBrush->m_dwRndFlags & ERF_HIDDEN)))
					SetCompiled(false);

				pBrush->SetRndFlags(ERF_HIDDEN, !bActivate);
				pBrush->SetRndFlags(ERF_ACTIVE_LAYER, bActivate);

				if (GetCVars()->e_ObjectLayersActivationPhysics == 1)
				{
					if (bActivate && bPhys)
						pBrush->PhysicalizeOnHeap(pHeap, false);
					else
						pBrush->Dephysicalize();
				}
				else if (!bPhys)
				{
					pBrush->Dephysicalize();
				}
			}
		}
	}

	for (IRenderNode* pObj = m_arrObjects[eRNListType_DecalsAndRoads].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		EERType eType = pObj->GetRenderNodeType();

		if (eType == eERType_Decal)
		{
			CDecalRenderNode* pDecal = (CDecalRenderNode*)pObj;
			if (pDecal->GetLayerId() == nLayerId || nLayerId == uint16(~0))
			{
				pDecal->SetRndFlags(ERF_HIDDEN, !bActivate);

				if (bActivate)
					pDecal->RequestUpdate();
				else
					pDecal->DeleteDecals();
			}
		}

		if (eType == eERType_Road)
		{
			CRoadRenderNode* pDecal = (CRoadRenderNode*)pObj;
			if (pDecal->GetLayerId() == nLayerId || nLayerId == uint16(~0))
			{
				pDecal->SetRndFlags(ERF_HIDDEN, !bActivate);
			}
		}
	}

	for (IRenderNode* pObj = m_arrObjects[eRNListType_Unknown].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if (pObj->GetRenderNodeType() == eERType_WaterVolume)
		{
			CWaterVolumeRenderNode* pWatVol = (CWaterVolumeRenderNode*)pObj;
			if (pWatVol->GetLayerId() == nLayerId || nLayerId == uint16(~0))
			{
				pWatVol->SetRndFlags(ERF_HIDDEN, !bActivate);

				if (GetCVars()->e_ObjectLayersActivationPhysics)
				{
					if (bActivate && bPhys)
						pWatVol->Physicalize();
					else
						pWatVol->Dephysicalize();
				}
				else if (!bPhys)
				{
					pWatVol->Dephysicalize();
				}
			}
		}

		if (pObj->GetRenderNodeType() == eERType_DistanceCloud)
		{
			CDistanceCloudRenderNode* pCloud = (CDistanceCloudRenderNode*)pObj;
			if (pCloud->GetLayerId() == nLayerId || nLayerId == uint16(~0))
			{
				pCloud->SetRndFlags(ERF_HIDDEN, !bActivate);
			}
		}
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->ActivateObjectsLayer(nLayerId, bActivate, bPhys, pHeap, layerBox);
}

void COctreeNode::GetLayerMemoryUsage(uint16 nLayerId, ICrySizer* pSizer, int* pNumBrushes, int* pNumDecals)
{
	for (IRenderNode* pObj = m_arrObjects[eRNListType_Brush].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if (pObj->GetRenderNodeType() == eERType_Brush)
		{
			CBrush* pBrush = (CBrush*)pObj;
			if (pBrush->m_nLayerId == nLayerId || nLayerId == uint16(~0))
			{
				pBrush->GetMemoryUsage(pSizer);
				if (pNumBrushes)
					(*pNumBrushes)++;
			}
		}
	}

	for (IRenderNode* pObj = m_arrObjects[eRNListType_DecalsAndRoads].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		EERType eType = pObj->GetRenderNodeType();

		if (eType == eERType_Decal)
		{
			CDecalRenderNode* pDecal = (CDecalRenderNode*)pObj;
			if (pDecal->GetLayerId() == nLayerId || nLayerId == uint16(~0))
			{
				pDecal->GetMemoryUsage(pSizer);
				if (pNumDecals)
					(*pNumDecals)++;
			}
		}

		if (eType == eERType_Road)
		{
			CRoadRenderNode* pDecal = (CRoadRenderNode*)pObj;
			if (pDecal->GetLayerId() == nLayerId || nLayerId == uint16(~0))
			{
				pDecal->GetMemoryUsage(pSizer);
				if (pNumDecals)
					(*pNumDecals)++;
			}
		}
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GetLayerMemoryUsage(nLayerId, pSizer, pNumBrushes, pNumDecals);
}

void COctreeNode::GetObjects(PodArray<IRenderNode*>& lstObjects, const AABB* pBBox)
{
	if (pBBox && !Overlap::AABB_AABB(*pBBox, GetObjectsBBox()))
		return;

	unsigned int nCurrentObject(eRNListType_First);
	for (nCurrentObject = eRNListType_First; nCurrentObject < eRNListType_ListsNum; ++nCurrentObject)
	{
		for (IRenderNode* pObj = m_arrObjects[nCurrentObject].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			if (!pBBox || Overlap::AABB_AABB(*pBBox, pObj->GetBBox()))
			{
				lstObjects.Add(pObj);
			}
		}
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GetObjects(lstObjects, pBBox);
}

bool COctreeNode::GetShadowCastersTimeSliced(IRenderNode* pIgnoreNode, ShadowMapFrustum* pFrustum, const PodArray<struct SPlaneObject>* pShadowHull, int renderNodeExcludeFlags, int& totalRemainingNodes, int nCurLevel, const SRenderingPassInfo& passInfo)
{
	assert(pFrustum->pShadowCacheData);

	if (totalRemainingNodes <= 0)
		return false;

	if (!pFrustum->pShadowCacheData->mOctreePathNodeProcessed[nCurLevel])
	{
		if (!pShadowHull || IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), m_objectsBox))
		{
			if (pFrustum->aabbCasters.IsReset() || Overlap::AABB_AABB(pFrustum->aabbCasters, GetObjectsBBox()))
			{
				for (int l = 0; l < eRNListType_ListsNum; l++)
				{
					for (IRenderNode* pNode = m_arrObjects[l].m_pFirstNode; pNode; pNode = pNode->m_pNext)
					{
						if (!IsRenderNodeTypeEnabled(pNode->GetRenderNodeType()))
							continue;

						if (pNode == pIgnoreNode)
							continue;

						auto nFlags = pNode->GetRndFlags();
						if (nFlags & (ERF_HIDDEN | ERF_COLLISION_PROXY | ERF_RAYCAST_PROXY | renderNodeExcludeFlags))
							continue;

						// Ignore ERF_CASTSHADOWMAPS for ambient occlusion casters
						if (pFrustum->m_eFrustumType != ShadowMapFrustum::e_HeightMapAO && (pNode->GetRndFlags() & ERF_CASTSHADOWMAPS) == 0)
							continue;

						if (pFrustum->pShadowCacheData->mProcessedCasters.find(pNode) != pFrustum->pShadowCacheData->mProcessedCasters.end())
							continue;

						AABB objBox = pNode->GetBBox();
						const float fDistanceSq = Distance::Point_PointSq(passInfo.GetCamera().GetPosition(), objBox.GetCenter());
						const float fMaxDist = pNode->GetMaxViewDist() * GetCVars()->e_ShadowsCastViewDistRatio + objBox.GetRadius();

						if (fDistanceSq > sqr(fMaxDist))
							continue;

						// find closest loaded lod

						bool bCanRender = false;

						if (IStatObj* pStatObj = pNode->GetEntityStatObj())
						{
							for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
							{
								IStatObj* pLod = pStatObj->GetLodObject(i);

								if (pLod && pLod->m_eStreamingStatus == ecss_Ready)
								{
									bCanRender = true;
									break;
								}
							}
						}
						else if (ICharacterInstance* pCharacter = pNode->GetEntityCharacter())
						{
							bCanRender = GetCVars()->e_ShadowsCacheRenderCharacters != 0;
						}

						if (bCanRender)
						{
							if (pNode->CanExecuteRenderAsJob())
							{
								pFrustum->jobExecutedCastersList.Add(pNode);
							}
							else
								pFrustum->castersList.Add(pNode);
						}
					}
				}
			}
		}

		pFrustum->pShadowCacheData->mOctreePathNodeProcessed[nCurLevel] = true;
		--totalRemainingNodes;
	}

	for (int i = pFrustum->pShadowCacheData->mOctreePath[nCurLevel]; i < 8; ++i)
	{
		if (m_arrChilds[i] && (m_arrChilds[i]->m_renderFlags & ERF_CASTSHADOWMAPS))
		{
			bool bDone = m_arrChilds[i]->GetShadowCastersTimeSliced(pIgnoreNode, pFrustum, pShadowHull, renderNodeExcludeFlags, totalRemainingNodes, nCurLevel + 1, passInfo);

			if (!bDone)
				return false;

		}

		pFrustum->pShadowCacheData->mOctreePath[nCurLevel] = i;
	}

	// this subtree is fully processed: reset traversal state
	pFrustum->pShadowCacheData->mOctreePath[nCurLevel] = 0;
	pFrustum->pShadowCacheData->mOctreePathNodeProcessed[nCurLevel] = 0;
	return true;
}

bool COctreeNode::IsObjectTypeInTheBox(EERType objType, const AABB& WSBBox)
{
	if (!Overlap::AABB_AABB(WSBBox, GetObjectsBBox()))
		return false;

	if (objType == eERType_Road && !m_bHasRoads)
		return false;

	ERNListType eListType = IRenderNode::GetRenderNodeListId(objType);

	for (IRenderNode* pObj = m_arrObjects[eListType].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if (pObj->GetRenderNodeType() == objType)
		{
			if (Overlap::AABB_AABB(WSBBox, pObj->GetBBox()))
				return true;
		}
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			if (m_arrChilds[i]->IsObjectTypeInTheBox(objType, WSBBox))
				return true;

	return false;
}

void COctreeNode::GenerateStatObjAndMatTables(std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, SHotUpdateInfo* pExportInfo)
{
	static_assert(eERType_TypesNum == 26, "Array size changed, code might need to be updated!");
	AABB* pBox = (pExportInfo && !pExportInfo->areaBox.IsReset()) ? &pExportInfo->areaBox : NULL;

	if (pBox && !Overlap::AABB_AABB(GetNodeBox(), *pBox))
		return;

	uint32 nObjTypeMask = pExportInfo ? pExportInfo->nObjTypeMask : (uint32) ~0;

	for (int l = 0; l < eRNListType_ListsNum; l++)
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			EERType eType = pObj->GetRenderNodeType();

			if (!(nObjTypeMask & (1 << eType)))
				continue;

			if (eType == eERType_Brush)
			{
				CBrush* pBrush = (CBrush*)pObj;
				if (CObjManager::GetItemId<IStatObj>(pStatObjTable, pBrush->GetEntityStatObj(), false) < 0)
					pStatObjTable->push_back(pBrush->m_pStatObj);
			}

			if (eType == eERType_Brush ||
			    eType == eERType_Road ||
			    eType == eERType_Decal ||
			    eType == eERType_WaterVolume ||
			    eType == eERType_DistanceCloud ||
			    eType == eERType_WaterWave)
			{
				if ((eType != eERType_Brush || ((CBrush*)pObj)->m_pMaterial) && CObjManager::GetItemId(pMatTable, pObj->GetMaterial(), false) < 0)
					pMatTable->push_back(pObj->GetMaterial());
			}

			if (eType == eERType_Vegetation)
			{
				CVegetation* pVegetation = (CVegetation*)pObj;
				IStatInstGroup& pStatInstGroup = pVegetation->GetStatObjGroup();
				stl::push_back_unique(*pStatInstGroupTable, &pStatInstGroup);
			}
			if (eType == eERType_MergedMesh)
			{
				CMergedMeshRenderNode* pRN = (CMergedMeshRenderNode*)pObj;
				for (uint32 i = 0; i < pRN->NumGroups(); ++i)
				{
					int grpid = pRN->Group(i)->instGroupId;
					IStatInstGroup& pStatInstGroup = pRN->GetStatObjGroup(grpid);
					stl::push_back_unique(*pStatInstGroupTable, &pStatInstGroup);
				}
			}
			if (eType == eERType_Character)
			{
				ICharacterInstance* pCharacter = pObj->GetEntityCharacter();
				if (pCharacter)
					pMatTable->push_back(pCharacter->GetMaterial());
			}
		}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GenerateStatObjAndMatTables(pStatObjTable, pMatTable, pStatInstGroupTable, pExportInfo);
}

int COctreeNode::Cmp_OctreeNodeSize(const void* v1, const void* v2)
{
	COctreeNode* pNode1 = *((COctreeNode**)v1);
	COctreeNode* pNode2 = *((COctreeNode**)v2);

	if (pNode1->GetNodeRadius2() > pNode2->GetNodeRadius2())
		return +1;
	if (pNode1->GetNodeRadius2() < pNode2->GetNodeRadius2())
		return -1;

	return 0;
}

COctreeNode* COctreeNode::FindChildFor(IRenderNode* pObj, const AABB& objBox, const float fObjRadius, const Vec3& vObjCenter)
{
	int nChildId =
	  ((vObjCenter.x > m_vNodeCenter.x) ? 4 : 0) |
	  ((vObjCenter.y > m_vNodeCenter.y) ? 2 : 0) |
	  ((vObjCenter.z > m_vNodeCenter.z) ? 1 : 0);

	if (!m_arrChilds[nChildId])
	{
		m_arrChilds[nChildId] = COctreeNode::Create(m_nSID, GetChildBBox(nChildId), m_pVisArea, this);
	}

	return m_arrChilds[nChildId];
}

bool COctreeNode::HasChildNodes()
{
	if (!m_arrChilds[0] && !m_arrChilds[1] && !m_arrChilds[2] && !m_arrChilds[3])
		if (!m_arrChilds[4] && !m_arrChilds[5] && !m_arrChilds[6] && !m_arrChilds[7])
			return false;

	return true;
}

int COctreeNode::CountChildNodes()
{
	return
	  (m_arrChilds[0] != 0) +
	  (m_arrChilds[1] != 0) +
	  (m_arrChilds[2] != 0) +
	  (m_arrChilds[3] != 0) +
	  (m_arrChilds[4] != 0) +
	  (m_arrChilds[5] != 0) +
	  (m_arrChilds[6] != 0) +
	  (m_arrChilds[7] != 0);
}

void COctreeNode::ReleaseEmptyNodes()
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_arrEmptyNodes.Count())
		return;

	// sort childs first
	qsort(m_arrEmptyNodes.GetElements(), m_arrEmptyNodes.Count(), sizeof(m_arrEmptyNodes[0]), Cmp_OctreeNodeSize);

	int nInitCunt = m_arrEmptyNodes.Count();

	for (int i = 0; i < nInitCunt && m_arrEmptyNodes.Count(); i++)
	{
		COctreeNode* pNode = m_arrEmptyNodes[0];

		if (pNode && pNode->IsEmpty())
		{
			COctreeNode* pParent = pNode->m_pParent;

			// unregister in parent
			for (int n = 0; n < 8; n++)
				if (pParent->m_arrChilds[n] == pNode)
					pParent->m_arrChilds[n] = NULL;

			delete pNode;

			// request parent validation
			if (pParent && pParent->IsEmpty() && m_arrEmptyNodes.Find(pParent) < 0)
				m_arrEmptyNodes.Add(pParent);
		}

		// remove from list
		m_arrEmptyNodes.Delete(pNode);
	}
}

void COctreeNode::StreamingCheckUnload(const SRenderingPassInfo& passInfo, PodArray<SObjManPrecacheCamera>& rPreCacheCameras)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_arrStreamedInNodes.Count())
		return;

	// sort childs first
	qsort(m_arrStreamedInNodes.GetElements(), m_arrStreamedInNodes.Count(), sizeof(m_arrStreamedInNodes[0]), Cmp_OctreeNodeSize);

	int nMaxTestsPerFrame = m_arrStreamedInNodes.Count() / 30 + 1;

	static int nCurNodeId = -1;

	const float fMaxViewDistance = Get3DEngine()->GetMaxViewDistance();

	for (int t = 0; t < nMaxTestsPerFrame && m_arrStreamedInNodes.Count(); t++)
	{
		nCurNodeId++;
		if (nCurNodeId >= m_arrStreamedInNodes.Count())
			nCurNodeId = 0;

		COctreeNode* pNode = m_arrStreamedInNodes[nCurNodeId];

		bool bOldRound = CObjManager::m_nUpdateStreamingPrioriryRoundId > (pNode->m_nUpdateStreamingPrioriryRoundId + (int)2);

		if (pNode->m_eStreamingStatus == ecss_Ready && pNode->m_nFileDataOffset && bOldRound)
		{
			pNode->ReleaseStreamableContent();

			m_arrStreamedInNodes.Delete(pNode);

			nCurNodeId--;
		}
	}

	// TODO: hard limit number of loaded nodes
}

void COctreeNode::StaticReset()
{
	ReleaseEmptyNodes();
	stl::free_container(m_arrEmptyNodes);

	if (COctreeNode::m_pFileForSyncRead)
		GetPak()->FClose(COctreeNode::m_pFileForSyncRead);
	COctreeNode::m_pFileForSyncRead = 0;
}

static float Distance_PrecacheCam_AABBSq(const SObjManPrecacheCamera& a, const AABB& b)
{
	float d2 = 0.0f;

	if (a.bbox.max.x < b.min.x) d2 += sqr(b.min.x - a.bbox.max.x);
	if (b.max.x < a.bbox.min.x) d2 += sqr(a.bbox.min.x - b.max.x);
	if (a.bbox.max.y < b.min.y) d2 += sqr(b.min.y - a.bbox.max.y);
	if (b.max.y < a.bbox.min.y) d2 += sqr(a.bbox.min.y - b.max.y);
	if (a.bbox.max.z < b.min.z) d2 += sqr(b.min.z - a.bbox.max.z);
	if (b.max.z < a.bbox.min.z) d2 += sqr(a.bbox.min.z - b.max.z);

	return d2;
}

bool COctreeNode::UpdateStreamingPriority(PodArray<COctreeNode*>& arrRecursion, float fMinDist, float fMaxDist, bool bFullUpdate, const SObjManPrecacheCamera* pPrecacheCams, size_t nPrecacheCams, const SRenderingPassInfo& passInfo)
{
	//  FUNCTION_PROFILER_3DENGINE;

	float fNodeDistanceNB = GetNodeStreamingDistance(pPrecacheCams, GetNodeBox(), nPrecacheCams, passInfo);

	float fObjMaxViewDistance = m_vNodeAxisRadius.x * GetCVars()->e_ObjectsTreeNodeSizeRatio * GetCVars()->e_ViewDistRatio * GetCVars()->e_StreamInstancesDistRatio;

	if (fNodeDistanceNB > min(fObjMaxViewDistance, fMaxDist) + GetFloatCVar(e_StreamPredictionDistanceFar))
		return true;

	CheckStartStreaming(bFullUpdate);

	float fNodeDistance = GetNodeStreamingDistance(pPrecacheCams, GetNodeBox(), nPrecacheCams, passInfo);

	if (fNodeDistance > min(m_fObjectsMaxViewDist, fMaxDist) + GetFloatCVar(e_StreamPredictionDistanceFar)) // add || m_objectsBox.IsReset()
		return true;

	if (!IsCompiled())
		CompileObjects();

	const float fPredictionDistanceFar = GetFloatCVar(e_StreamPredictionDistanceFar);

	if (fNodeDistance > min(m_fObjectsMaxViewDist, fMaxDist) + fPredictionDistanceFar)
		return true;

	if (GetCVars()->e_VegetationSpritesBatching && GetCVars()->e_VegetationSprites)
		CheckManageVegetationSprites(fNodeDistance, 64, passInfo);

	AABB objBox;

	const bool bEnablePerNodeDistance = GetCVars()->e_StreamCgfUpdatePerNodeDistance > 0;
	CVisArea* pRoot0 = GetVisAreaManager() ? GetVisAreaManager()->GetCurVisArea() : NULL;

	float fMinDistSq = fMinDist * fMinDist;

	PREFAST_SUPPRESS_WARNING(6255)
	float* pfMinVisAreaDistSq = (float*)alloca(sizeof(float) * nPrecacheCams);

	for (size_t iPrecacheCam = 0; iPrecacheCam < nPrecacheCams; ++iPrecacheCam)
	{
		float fMinVisAreaDist = 0.0f;

		if (pRoot0)
		{
			// search from camera to entity visarea or outdoor
			AABB aabbCam = pPrecacheCams[iPrecacheCam].bbox;
			float fResDist = 10000.0f;
			if (pRoot0->GetDistanceThruVisAreas(aabbCam, m_pVisArea, m_objectsBox, bFullUpdate ? 2 : GetCVars()->e_StreamPredictionMaxVisAreaRecursion, fResDist))
				fMinVisAreaDist = fResDist;
		}
		else if (m_pVisArea)
		{
			// search from entity to outdoor
			AABB aabbCam = pPrecacheCams[iPrecacheCam].bbox;
			float fResDist = 10000.0f;
			if (m_pVisArea->GetDistanceThruVisAreas(m_objectsBox, NULL, aabbCam, bFullUpdate ? 2 : GetCVars()->e_StreamPredictionMaxVisAreaRecursion, fResDist))
				fMinVisAreaDist = fResDist;
		}

		pfMinVisAreaDistSq[iPrecacheCam] = fMinVisAreaDist * fMinVisAreaDist;
	}

	for (int l = 0; l < eRNListType_ListsNum; l++)
	{
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			if (pObj->m_pNext)
				cryPrefetchT0SSE(pObj->m_pNext);

			IF (pObj->m_dwRndFlags & ERF_HIDDEN, 0)
				continue;

#ifdef _DEBUG
			const char* szName = pObj->GetName();
			const char* szClassName = pObj->GetEntityClassName();

			if (pObj->GetRndFlags() & ERF_SELECTED)
			{
				int selected = 1;
			}
#endif // _DEBUG

			pObj->FillBBox(objBox);

			// stream more in zoom mode if in frustum
			float fZoomFactorSq = (passInfo.IsZoomActive() && passInfo.GetCamera().IsAABBVisible_E(objBox))
			                      ? passInfo.GetZoomFactor() * passInfo.GetZoomFactor()
			                      : 1.0f;

			for (size_t iPrecacheCam = 0; iPrecacheCam < nPrecacheCams; ++iPrecacheCam)
			{
				const Vec3& pcPosition = pPrecacheCams[iPrecacheCam].vPosition;

				float fEntDistanceSq = Distance_PrecacheCam_AABBSq(pPrecacheCams[iPrecacheCam], objBox);

				if (pObj->GetRenderNodeType() == eERType_Vegetation && ((CVegetation*)pObj)->m_pInstancingInfo)
				{
					// for instance groups compute distance to the center of bbox
					AABB objBoxS = AABB(objBox.GetCenter() - Vec3(.1f, .1f, .1f), objBox.GetCenter() + Vec3(.1f, .1f, .1f));
					fEntDistanceSq = Distance_PrecacheCam_AABBSq(pPrecacheCams[iPrecacheCam], objBoxS);
				}

				fEntDistanceSq = max(fEntDistanceSq, fMinDistSq);
				fEntDistanceSq *= fZoomFactorSq;
				fEntDistanceSq = max(fEntDistanceSq, pfMinVisAreaDistSq[iPrecacheCam]);

				float fMaxDistComb = min(pObj->m_fWSMaxViewDist, fMaxDist) + fPredictionDistanceFar;
				float fMaxDistCombSq = fMaxDistComb * fMaxDistComb;

				if (/*fMinDistSq <= fEntDistanceSq &&*/ fEntDistanceSq < fMaxDistCombSq)
				{
					float fEntDistance = sqrt_tpl(fEntDistanceSq);
					assert(fEntDistance >= 0 && _finite(fEntDistance));

					float fDist = fEntDistance;
					if (!bFullUpdate && fEntDistance < fNodeDistance && bEnablePerNodeDistance)
						fDist = fNodeDistance;

					// If we're inside the object, very close, or facing the object, set importance scale to 1.0f. Otherwise, 0.8f.
					float fImportanceScale = (float)__fsel(
					  4.0f - fEntDistance,
					  1.0f,
					  (float)__fsel(
					    (objBox.GetCenter() - pcPosition).Dot(pPrecacheCams[iPrecacheCam].vDirection),
					    1.0f,
					    0.8f));

					// I replaced fEntDistance with fNoideDistance here because of Timur request! It's suppose to be unified to-node-distance
					GetObjManager()->UpdateRenderNodeStreamingPriority(pObj, fDist, fImportanceScale, bFullUpdate, passInfo);
				}
			}
		}
	}

	// Prioritise the first camera (probably the real camera)
	int nFirst =
	  ((pPrecacheCams[0].vPosition.x > m_vNodeCenter.x) ? 4 : 0) |
	  ((pPrecacheCams[0].vPosition.y > m_vNodeCenter.y) ? 2 : 0) |
	  ((pPrecacheCams[0].vPosition.z > m_vNodeCenter.z) ? 1 : 0);

	if (m_arrChilds[nFirst])
		arrRecursion.Add(m_arrChilds[nFirst]);

	if (m_arrChilds[nFirst ^ 1])
		arrRecursion.Add(m_arrChilds[nFirst ^ 1]);

	if (m_arrChilds[nFirst ^ 2])
		arrRecursion.Add(m_arrChilds[nFirst ^ 2]);

	if (m_arrChilds[nFirst ^ 4])
		arrRecursion.Add(m_arrChilds[nFirst ^ 4]);

	if (m_arrChilds[nFirst ^ 3])
		arrRecursion.Add(m_arrChilds[nFirst ^ 3]);

	if (m_arrChilds[nFirst ^ 5])
		arrRecursion.Add(m_arrChilds[nFirst ^ 5]);

	if (m_arrChilds[nFirst ^ 6])
		arrRecursion.Add(m_arrChilds[nFirst ^ 6]);

	if (m_arrChilds[nFirst ^ 7])
		arrRecursion.Add(m_arrChilds[nFirst ^ 7]);

	return true;
}

int COctreeNode::Load(FILE*& f, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, AABB* pBox, const SLayerVisibility* pLayerVisibilityMask, const Vec3& segmentOffset)
{
	return Load_T(f, nDataSize, pStatObjTable, pMatTable, eEndian, pBox, pLayerVisibilityMask, segmentOffset);
}
int COctreeNode::Load(uint8*& f, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, AABB* pBox, const SLayerVisibility* pLayerVisibilityMask, const Vec3& segmentOffset)
{
	return Load_T(f, nDataSize, pStatObjTable, pMatTable, eEndian, pBox, pLayerVisibilityMask, segmentOffset);
}

int FTell(FILE*& f)  { return Cry3DEngineBase::GetPak()->FTell(f); }
int FTell(uint8*& f) { return 0; }

template<class T>
int COctreeNode::Load_T(T*& f, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, AABB* pBox, const SLayerVisibility* pLayerVisibilityMask, const Vec3& segmentOffset)
{
	if (pBox && !Overlap::AABB_AABB(GetNodeBox(), *pBox))
		return 0;

	// remember file offset for streaming
	m_nFileDataOffset = FTell(f);

	SOcTreeNodeChunk chunk;

	ELoadObjectsMode eLoadObjectsMode = (GetCVars()->e_StreamInstances && !gEnv->IsEditor()) ? LOM_LOAD_ONLY_NON_STREAMABLE : LOM_LOAD_ALL;

	if (!ReadObjects(f, nDataSize, eEndian, pStatObjTable, pMatTable, pLayerVisibilityMask, segmentOffset, chunk, eLoadObjectsMode))
		return 0;

	if (chunk.nObjectsBlockSize)
	{
		m_nFileDataSize = chunk.nObjectsBlockSize + sizeof(SOcTreeNodeChunk);
		m_nNodesCounterStreamable++;
	}
	else
		m_nFileDataSize = m_nFileDataOffset = 0;

	// count number of nodes loaded
	int nNodesNum = 1;

	// process childs
	for (int nChildId = 0; nChildId < 8; nChildId++)
	{
		if (chunk.ucChildsMask & (1 << nChildId))
		{
			if (!m_arrChilds[nChildId])
			{
				m_arrChilds[nChildId] = COctreeNode::Create(m_nSID, GetChildBBox(nChildId), m_pVisArea, this);
			}

			int nNewNodesNum = m_arrChilds[nChildId]->Load_T(f, nDataSize, pStatObjTable, pMatTable, eEndian, pBox, pLayerVisibilityMask, segmentOffset);

			if (!nNewNodesNum && !pBox)
				return 0; // data error

			nNodesNum += nNewNodesNum;
		}
	}

	return nNodesNum;
}

void COctreeNode::BuildLoadingDatas(PodArray<SOctreeLoadObjectsData>* pQueue, byte* pOrigData, byte*& pData, int& nDataSize, EEndian eEndian)
{
	SOcTreeNodeChunk chunk;
	CTerrain::LoadDataFromFile(&chunk, 1, pData, nDataSize, eEndian);

	assert(chunk.nChunkVersion == OCTREENODE_CHUNK_VERSION);

	if (chunk.nObjectsBlockSize)
	{
		SOctreeLoadObjectsData data = { this, pData - pOrigData, size_t(chunk.nObjectsBlockSize) };
		pQueue->Add(data);

		pData += chunk.nObjectsBlockSize;
		nDataSize -= chunk.nObjectsBlockSize;
	}

	for (int nChildId = 0; nChildId < 8; nChildId++)
	{
		if (chunk.ucChildsMask & (1 << nChildId))
		{
			if (!m_arrChilds[nChildId])
			{
				MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Terrain, EMemStatContextFlags::MSF_Instance, "Octree node");
				m_arrChilds[nChildId] = new COctreeNode(m_nSID, GetChildBBox(nChildId), m_pVisArea, this);
			}

			m_arrChilds[nChildId]->BuildLoadingDatas(pQueue, pOrigData, pData, nDataSize, eEndian);
		}
	}
}

bool COctreeNode::StreamLoad(uint8* pData, int nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, AABB* pBox)
{
	int64 ticks = CryGetTicks();
	if (m_streamComplete)
		return false;

	if (m_loadingDatas.size() == 0)
	{
		BuildLoadingDatas(&m_loadingDatas, pData, pData, nDataSize, eEndian);
	}
	else
	{
		SOctreeLoadObjectsData& data = m_loadingDatas[0];

		if (data.pMemBlock == 0)
		{
			pData += data.offset;
			nDataSize -= (int)data.offset;

			// load objects data into memory buffer, make sure buffer is aligned
			_smart_ptr<IMemoryBlock> pMemBlock = gEnv->pCryPak->PoolAllocMemoryBlock(data.size + 8, "LoadObjectInstances");
			byte* pPtr = (byte*)pMemBlock->GetData();
			while (UINT_PTR(pPtr) & 3)
				pPtr++;

			if (!CTerrain::LoadDataFromFile(pPtr, data.size, pData, nDataSize, eEndian))
				return false;

			data.pMemBlock = pMemBlock;
			data.pObjPtr = pPtr;
			data.pEndObjPtr = pPtr + data.size;
		}
		else
		{
			// if we are not in segmented world mode then the offset is 0
			Vec3 segmentOffset(0, 0, 0);
			if (Get3DEngine()->m_pSegmentsManager)
			{
				segmentOffset = GetTerrain()->GetSegmentOrigin(m_nSID);
			}

			IRenderNode* pRN = 0;
			data.pNode->LoadSingleObject(data.pObjPtr, pStatObjTable, pMatTable, eEndian, OCTREENODE_CHUNK_VERSION, NULL, m_nSID, segmentOffset, LOM_LOAD_ALL, pRN);
			if (data.pObjPtr >= data.pEndObjPtr)
				m_loadingDatas.Delete(0);
		}
	}
	m_streamComplete = m_loadingDatas.size() == 0;
	return !m_streamComplete;
}

#if ENGINE_ENABLE_COMPILATION
int COctreeNode::GetData(byte*& pData, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo, const Vec3& segmentOffset)
{
	AABB* pBox = (pExportInfo && !pExportInfo->areaBox.IsReset()) ? &pExportInfo->areaBox : NULL;

	const AABB& nodeBox = GetNodeBox();
	if (pBox && !Overlap::AABB_AABB(nodeBox, *pBox))
		return 0;

	if (pData)
	{
		// get node data
		SOcTreeNodeChunk chunk;
		chunk.nChunkVersion = OCTREENODE_CHUNK_VERSION;
		chunk.nodeBox = nodeBox;

		// fill ChildsMask
		chunk.ucChildsMask = 0;
		for (int i = 0; i < 8; i++)
			if (m_arrChilds[i])
				chunk.ucChildsMask |= (1 << i);

		CMemoryBlock memblock;
		SaveObjects(&memblock, pStatObjTable, pMatTable, pStatInstGroupTable, eEndian, pExportInfo, segmentOffset);

		chunk.nObjectsBlockSize = memblock.GetSize();

		AddToPtr(pData, nDataSize, chunk, eEndian);

		AddToPtr(pData, nDataSize, (byte*)memblock.GetData(), memblock.GetSize(), eEndian);
	}
	else // just count size
	{
		nDataSize += sizeof(SOcTreeNodeChunk);
		nDataSize += SaveObjects(NULL, NULL, NULL, NULL, eEndian, pExportInfo, segmentOffset);
	}

	// count number of nodes loaded
	int nNodesNum = 1;

	// process childs
	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			nNodesNum += m_arrChilds[i]->GetData(pData, nDataSize, pStatObjTable, pMatTable, pStatInstGroupTable, eEndian, pExportInfo, segmentOffset);

	return nNodesNum;
}
#endif

bool COctreeNode::CleanUpTree()
{
	//  FreeAreaBrushes();

	bool bChildObjectsFound = false;
	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
		{
			if (!m_arrChilds[i]->CleanUpTree())
			{
				delete m_arrChilds[i];
				m_arrChilds[i] = NULL;
			}
			else
				bChildObjectsFound = true;
		}
	}

	// update max view distances

	m_fObjectsMaxViewDist = 0.f;
	m_objectsBox = GetNodeBox();

	for (int l = 0; l < eRNListType_ListsNum; l++)
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			pObj->m_fWSMaxViewDist = pObj->GetMaxViewDist();
			m_fObjectsMaxViewDist = max(m_fObjectsMaxViewDist, pObj->m_fWSMaxViewDist);
			m_objectsBox.Add(pObj->GetBBox());
		}

	for (int i = 0; i < 8; i++)
	{
		if (m_arrChilds[i])
		{
			m_fObjectsMaxViewDist = max(m_fObjectsMaxViewDist, m_arrChilds[i]->m_fObjectsMaxViewDist);
			m_objectsBox.Add(m_arrChilds[i]->m_objectsBox);
		}
	}

	return (bChildObjectsFound || HasObjects());
}

void COctreeNode::FreeLoadingCache()
{
}

//////////////////////////////////////////////////////////////////////////
bool COctreeNode::CheckRenderFlagsMinSpec(uint32 dwRndFlags)
{
	int nRenderNodeMinSpec = (dwRndFlags & ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT;
	return CheckMinSpec(nRenderNodeMinSpec);
}

void COctreeNode::OffsetObjects(const Vec3& offset)
{
	SetCompiled(false);
	m_objectsBox.Move(offset);
	m_vNodeCenter += offset;

	for (int l = 0; l < eRNListType_ListsNum; l++)
	{
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			pObj->OffsetPosition(offset);
		}
	}
	for (int i = 0; i < 8; ++i)
		if (m_arrChilds[i])
			m_arrChilds[i]->OffsetObjects(offset);
}

bool COctreeNode::HasAnyRenderableCandidates(const SRenderingPassInfo& passInfo) const
{
	// This checks if anything will be rendered, assuming we pass occlusion checks
	// This is based on COctreeNode::RenderContentJobEntry's implementation,
	// if that would do nothing, we can skip the running of occlusion and rendering jobs for this node
	const bool bVegetation = passInfo.RenderVegetation() && m_arrObjects[eRNListType_Vegetation].m_pFirstNode != NULL;
	const bool bBrushes = passInfo.RenderBrushes() && m_arrObjects[eRNListType_Brush].m_pFirstNode != NULL;
	const bool bDecalsAndRoads = (passInfo.RenderDecals() || passInfo.RenderRoads()) && m_arrObjects[eRNListType_DecalsAndRoads].m_pFirstNode != NULL;
	const bool bUnknown = m_arrObjects[eRNListType_Unknown].m_pFirstNode != NULL;
	return bVegetation || bBrushes || bDecalsAndRoads || bUnknown;
}

template<class T>
int COctreeNode::ReadObjects(T*& f, int& nDataSize, EEndian eEndian, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, const SLayerVisibility* pLayerVisibilityMask, const Vec3& segmentOffset, SOcTreeNodeChunk& chunk, ELoadObjectsMode eLoadMode)
{
	if (!CTerrain::LoadDataFromFile(&chunk, 1, f, nDataSize, eEndian))
		return 0;

	assert(chunk.nChunkVersion == OCTREENODE_CHUNK_VERSION);
	if (chunk.nChunkVersion != OCTREENODE_CHUNK_VERSION)
		return 0;

	if (chunk.nObjectsBlockSize)
	{
		{
			_smart_ptr<IMemoryBlock> pMemBlock = gEnv->pCryPak->PoolAllocMemoryBlock(chunk.nObjectsBlockSize + 8, "LoadObjectInstances");
			byte* pPtr = (byte*)pMemBlock->GetData();

			while (UINT_PTR(pPtr) & 3)
				pPtr++;

			if (!CTerrain::LoadDataFromFile(pPtr, chunk.nObjectsBlockSize, f, nDataSize, eEndian))
				return 0;

			if (!m_bEditor || Get3DEngine()->IsSegmentOperationInProgress())
				LoadObjects(pPtr, pPtr + chunk.nObjectsBlockSize, pStatObjTable, pMatTable, eEndian, chunk.nChunkVersion, pLayerVisibilityMask, segmentOffset, eLoadMode);
		}

		if (eLoadMode != LOM_LOAD_ALL)
		{
			float fObjMaxViewDistance = m_vNodeAxisRadius.x * GetCVars()->e_ObjectsTreeNodeSizeRatio * GetCVars()->e_ViewDistRatio * GetCVars()->e_StreamInstancesDistRatio;

			COctreeNode* pNode = this;
			while (pNode)
			{
				pNode->m_fObjectsMaxViewDist = max(pNode->m_fObjectsMaxViewDist, fObjMaxViewDistance);
				pNode = pNode->m_pParent;
			}
		}
	}

	return 1;
}

void COctreeNode::StartStreaming(bool bFinishNow, IReadStream_AutoPtr* ppStream)
{
	m_nInstStreamTasksInProgress++;

	assert(m_eStreamingStatus == ecss_NotLoaded);

	m_eStreamingStatus = ecss_InProgress;

	const char* szFileName = Get3DEngine()->GetLevelFilePath(COMPILED_HEIGHT_MAP_FILE_NAME);

	if (bFinishNow)
	{
		// sync file load
		if (!m_pFileForSyncRead)
			m_pFileForSyncRead = GetPak()->FOpen(szFileName, "rb");

		if (m_pFileForSyncRead)
		{
			GetPak()->FSeek(m_pFileForSyncRead, m_nFileDataOffset, SEEK_SET);

			StreamOnCompleteReadObjects(m_pFileForSyncRead, m_nFileDataSize);
		}
		else
		{
			assert(!"File open error: COMPILED_HEIGHT_MAP_FILE_NAME");
		}
	}
	else
	{
		// start streaming
		StreamReadParams params;
		params.nOffset = m_nFileDataOffset;
		params.nSize = m_nFileDataSize;
		params.ePriority = estpAboveNormal;// (EStreamTaskPriority)CLAMP(int((float)estpIdle - log2(GetNodeBox().GetSize().x / fNodeMinSize)), int(estpUrgent), int(estpIdle));

		m_pReadStream = GetSystem()->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, szFileName, this, &params);
	}
}

void COctreeNode::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	if (pStream->GetError() == ERROR_USER_ABORT)
	{
		// valid situation
		m_eStreamingStatus = ecss_NotLoaded;
		m_nInstStreamTasksInProgress--;
		m_pReadStream = 0;
	}
	else if (m_pReadStream && m_pReadStream->GetBuffer() && m_pReadStream->GetBytesRead())
	{
		StreamOnCompleteReadObjects((byte*)m_pReadStream->GetBuffer(), m_pReadStream->GetBytesRead());
	}
	else if (m_pReadStream)
	{
		PrintMessage("%s: Error: %s, BytesRead=%d", __FUNCTION__, pStream->GetErrorName(), m_pReadStream->GetBytesRead());
	}
	else
	{
		PrintMessage("%s: Error: %s, Stream=NULL", __FUNCTION__, pStream->GetErrorName());
	}
}

template<class T>
void COctreeNode::StreamOnCompleteReadObjects(T* f, int nDataSize)
{
	FUNCTION_PROFILER_3DENGINE;

	SOcTreeNodeChunk chunk;

	C3DEngine* pEng = Get3DEngine();

	if (!ReadObjects(f, nDataSize, pEng->m_bLevelFilesEndian, pEng->m_pLevelStatObjTable, pEng->m_pLevelMaterialsTable, NULL, Vec3(0, 0, 0), chunk, LOM_LOAD_ONLY_STREAMABLE))
	{
		PrintMessage("%s: Instances read error", __FUNCTION__);
	}

	//PrintMessage("Loaded %d KB for node size %.f", chunk.nObjectsBlockSize/1024, m_vNodeAxisRadius.x);

	SetCompiled(false);

	m_eStreamingStatus = ecss_Ready;

	m_nInstStreamTasksInProgress--;

	m_pReadStream = 0;

	assert(m_arrStreamedInNodes.Find(this) < 0);

	m_arrStreamedInNodes.Add(this);
}

bool COctreeNode::CheckStartStreaming(bool bFullUpdate)
{
	bool bSyncLoad = Get3DEngine()->IsStatObjSyncLoad();

	if (!gEnv->IsEditor() && GetCVars()->e_StreamInstances == 1 && m_nFileDataOffset && (m_nInstStreamTasksInProgress < GetCVars()->e_StreamInstancesMaxTasks || bSyncLoad))
	{
		if (m_eStreamingStatus == ecss_NotLoaded)
		{
			StartStreaming(bSyncLoad, 0);
		}
		else if (m_eStreamingStatus == ecss_InProgress && m_pReadStream && bSyncLoad)
		{
			m_pReadStream->Abort();
			assert(m_pReadStream == 0 && m_eStreamingStatus == ecss_NotLoaded);

			StartStreaming(bSyncLoad, 0);
		}
	}

	if (GetCVars()->e_StreamInstances == 1)
		m_nUpdateStreamingPrioriryRoundId = CObjManager::m_nUpdateStreamingPrioriryRoundId;

	// return true if data is ready
	return gEnv->IsEditor() || !GetCVars()->e_StreamInstances || !m_nFileDataOffset || m_eStreamingStatus == ecss_Ready;
}

float COctreeNode::GetNodeStreamingDistance(const SObjManPrecacheCamera* pPrecacheCams, AABB objectsBox, size_t nPrecacheCams, const SRenderingPassInfo& passInfo)
{
	// Select the minimum distance to the node
	float fNodeDistanceSq = Distance_PrecacheCam_AABBSq(pPrecacheCams[0], objectsBox);
	for (size_t iPrecacheCam = 1; iPrecacheCam < nPrecacheCams; ++iPrecacheCam)
	{
		float fPcNodeDistanceSq = Distance_PrecacheCam_AABBSq(pPrecacheCams[iPrecacheCam], objectsBox);
		fNodeDistanceSq = min(fNodeDistanceSq, fPcNodeDistanceSq);
	}

	float fNodeDistance = sqrt_tpl(fNodeDistanceSq);

	if (passInfo.IsZoomActive() && passInfo.GetCamera().IsAABBVisible_E(GetNodeBox()))
		fNodeDistance *= passInfo.GetZoomFactor();

	return fNodeDistance;
}

void COctreeNode::ReleaseStreamableContent()
{
	ReleaseObjects(true);

	m_eStreamingStatus = ecss_NotLoaded;

	SetCompiled(false);
}

void COctreeNode::ReleaseObjects(bool bReleaseOnlyStreamable)
{
	for (int l = 0; l < eRNListType_ListsNum; l++)
	{
		for (IRenderNode* pObj = m_arrObjects[l].m_pFirstNode, * pNext; pObj; pObj = pNext)
		{
			pNext = pObj->m_pNext;

			if (pObj->IsAllocatedOutsideOf3DEngineDLL())
			{
				if (!bReleaseOnlyStreamable)
					Get3DEngine()->UnRegisterEntityDirect(pObj);
			}
			else
			{
				if (!bReleaseOnlyStreamable || IsObjectStreamable(pObj->GetRenderNodeType(), pObj->m_dwRndFlags))
				{
					if (IsObjectStreamable(pObj->GetRenderNodeType(), pObj->m_dwRndFlags))
						m_nInstCounterLoaded--;

					pObj->ReleaseNode(true);
				}
			}
		}

		assert(!m_arrObjects[l].m_pFirstNode || bReleaseOnlyStreamable);
	}
}
void COctreeNode::ResetStaticInstancing()
{
	FUNCTION_PROFILER_3DENGINE;

	for (IRenderNode* pObj = m_arrObjects[eRNListType_Vegetation].m_pFirstNode, * pNext; pObj; pObj = pNext)
	{
		pNext = pObj->m_pNext;

		CVegetation* pInst = (CVegetation*)pObj;

		pObj->m_dwRndFlags &= ~ERF_STATIC_INSTANCING;

		if (pInst->m_pInstancingInfo)
		{
			SAFE_DELETE(pInst->m_pInstancingInfo)

			pInst->InvalidatePermanentRenderObject();
		}
	}

	if (m_pStaticInstancingInfo)
	{
		for (auto it = m_pStaticInstancingInfo->begin(); it != m_pStaticInstancingInfo->end(); )
		{
			PodArray<SNodeInstancingInfo>*& pInfo = it->second;

			pInfo->Reset();

			it = m_pStaticInstancingInfo->erase(it);
		}

		SAFE_DELETE(m_pStaticInstancingInfo);
	}

	m_bStaticInstancingIsDirty = true;
}

void COctreeNode::CheckUpdateStaticInstancing()
{
	WriteLock lock(m_updateStaticInstancingLock);

	if (GetCVars()->e_StaticInstancing && m_arrObjects[eRNListType_Vegetation].m_pFirstNode)
	{
		if (m_bStaticInstancingIsDirty)
			UpdateStaticInstancing();
	}
	else if (m_pStaticInstancingInfo)
	{
		ResetStaticInstancing();
	}
}

COctreeNode::COctreeNode(int nSID, const AABB& box, CVisArea* pVisArea, COctreeNode* pParent)
{
	m_fPrevTerrainTexScale = 0;
	m_updateStaticInstancingLock = 0;

	m_nOccludedFrameId = 0;
	m_renderFlags = 0;
	m_errTypesBitField = 0;
	m_fObjectsMaxViewDist = 0;
	m_nLastVisFrameId = 0;

	ZeroStruct(m_arrChilds);
	ZeroStruct(m_arrObjects);
	m_nLightMaskFrameId = 0;
	nFillShadowCastersSkipFrameId = 0;
	m_fNodeDistance = 0;
	m_nManageVegetationsFrameId = 0;

	m_bHasLights = 0;
	m_bHasRoads = 0;
	m_bNodeCompletelyInFrustum = 0;

	m_nFileDataOffset = 0;
	m_nFileDataSize = 0;
	m_eStreamingStatus = ecss_NotLoaded;
	m_pReadStream = 0;
	m_nUpdateStreamingPrioriryRoundId = -1;

	m_nSID = nSID;
	m_vNodeCenter = box.GetCenter();
	m_vNodeAxisRadius = box.GetSize() * 0.5f;
	m_objectsBox.min = box.max;
	m_objectsBox.max = box.min;

#if !defined(_RELEASE)
	// Check if bounding box is crazy
#define CHECK_OBJECTS_BOX_WARNING_SIZE (1.0e+10f)
	if (GetCVars()->e_CheckOctreeObjectsBoxSize != 0) // pParent is checked as silly sized things are added to the root (e.g. the sun)
		if (pParent && (m_objectsBox.min.len() > CHECK_OBJECTS_BOX_WARNING_SIZE || m_objectsBox.max.len() > CHECK_OBJECTS_BOX_WARNING_SIZE))
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR_DBGBRK, "COctreeNode being created with a huge m_objectsBox: [%f %f %f] -> [%f %f %f]\n", m_objectsBox.min.x, m_objectsBox.min.y, m_objectsBox.min.z, m_objectsBox.max.x, m_objectsBox.max.y, m_objectsBox.max.z);
#endif

	SetTerrainNode(m_nSID >= 0 && GetTerrain() ? GetTerrain()->FindMinNodeContainingBox(box, m_nSID) : NULL);
	m_pVisArea = pVisArea;
	m_pParent = pParent;
	m_streamComplete = false;

	//	for(int n=0; n<2 && m_pTerrainNode && m_pTerrainNode->m_pParent; n++)
	//	m_pTerrainNode = m_pTerrainNode->m_pParent;
	m_fpSunDirX = 63;
	m_fpSunDirZ = 0;
	m_fpSunDirYs = 0;

	m_pStaticInstancingInfo = 0;
	m_bStaticInstancingIsDirty = 0;
}

//////////////////////////////////////////////////////////////////////////
COctreeNode* COctreeNode::Create(int nSID, const AABB& box, struct CVisArea* pVisArea, COctreeNode* pParent)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Terrain, EMemStatContextFlags::MSF_Instance, "Octree node");
	m_nNodesCounterAll++;
	return new COctreeNode(nSID, box, pVisArea, pParent);
}

//////////////////////////////////////////////////////////////////////////
bool COctreeNode::HasObjects()
{
	if (m_nFileDataOffset)
		return true;

	for (int l = 0; l < eRNListType_ListsNum; l++)
		if (m_arrObjects[l].m_pFirstNode)
			return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::RenderContent(int nRenderMask, const Vec3& vAmbColor, const SRenderingPassInfo& passInfo)
{
	if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && GetCVars()->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
		GetObjManager()->AddCullJobProducer();
	IF(m_pRenderContentJobQueue == NULL, 0)
	{
		m_pRenderContentJobQueue = CryAlignedNew<TRenderContentJobQueue>();
	}
	TRenderContentJob::packet packet(nRenderMask, vAmbColor, passInfo);
	packet.SetClassInstance(this);
	static_cast<TRenderContentJobQueue*>(m_pRenderContentJobQueue)->AddPacket(packet, JobManager::eHighPriority);
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::DeallocateRenderContentQueue()
{
	if (m_pRenderContentJobQueue != NULL)
	{
		CryAlignedDelete(static_cast<TRenderContentJobQueue*>(m_pRenderContentJobQueue));
		m_pRenderContentJobQueue = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::RenderContentJobEntry(int nRenderMask, Vec3 vAmbColor, SRenderingPassInfo passInfo)
{
	PodArray<CDLight*>* pAffectingLights = GetAffectingLights(passInfo);
	bool bSunOnly = pAffectingLights && (pAffectingLights->Count() == 1) && (pAffectingLights->GetAt(0)->m_Flags & DLF_SUN) && !m_pVisArea;

	SSectorTextureSet* pTerrainTexInfo = NULL;

	if (GetCVars()->e_VegetationUseTerrainColor)
		GetObjManager()->FillTerrainTexInfo(this, m_fNodeDistance, pTerrainTexInfo, m_objectsBox);

	// Detect if terrain texturing is changed
	if (m_fPrevTerrainTexScale != (pTerrainTexInfo ? pTerrainTexInfo->fTexScale : 0))
	{
		m_fPrevTerrainTexScale = (pTerrainTexInfo ? pTerrainTexInfo->fTexScale : 0);

		for (unsigned int nListId = eRNListType_First; nListId < eRNListType_ListsNum; ++nListId)
		{
			for (IRenderNode* pObj = m_arrObjects[nListId].m_pFirstNode; pObj; pObj = pObj->m_pNext)
			{
				// Invalidate objects where terrain texture is used
				if (pObj->m_pTempData && pObj->m_pTempData->userData.bTerrainColorWasUsed)
				{
					pObj->InvalidatePermanentRenderObject();
				}
			}
		}
	}

	if (m_arrObjects[eRNListType_Vegetation].m_pFirstNode && passInfo.RenderVegetation())
	{
		/*if (m_lstVegetationsForRendering.size() > 100)
		{
		auto lambda = [=]
		{
		RenderVegetations(&m_arrObjects[eRNListType_Vegetation], nRenderMask, m_bNodeCompletelyInFrustum != 0, pAffectingLights, bSunOnly, pTerrainTexInfo, passInfo);
		};
		gEnv->pJobManager->AddLambdaJob("Job_RenderVegetations", lambda,JobManager::eRegularPriority,(CryJobState*)passInfo.WriteMutex() );
		}
		else*/
		this->RenderVegetations(&m_arrObjects[eRNListType_Vegetation], nRenderMask, m_bNodeCompletelyInFrustum != 0, pAffectingLights, bSunOnly, pTerrainTexInfo, passInfo);
	}

	if (/*GetCVars()->e_SceneMerging!=3 && */ m_arrObjects[eRNListType_Brush].m_pFirstNode && passInfo.RenderBrushes())
		this->RenderBrushes(&m_arrObjects[eRNListType_Brush], m_bNodeCompletelyInFrustum != 0, pAffectingLights, bSunOnly, pTerrainTexInfo, passInfo);

	if (m_arrObjects[eRNListType_DecalsAndRoads].m_pFirstNode && (passInfo.RenderDecals() || passInfo.RenderRoads()))
		this->RenderDecalsAndRoads(&m_arrObjects[eRNListType_DecalsAndRoads], nRenderMask, vAmbColor, m_bNodeCompletelyInFrustum != 0, pAffectingLights, bSunOnly, passInfo);

	if (m_arrObjects[eRNListType_Unknown].m_pFirstNode)
		this->RenderCommonObjects(&m_arrObjects[eRNListType_Unknown], nRenderMask, vAmbColor, m_bNodeCompletelyInFrustum != 0, pAffectingLights, bSunOnly, pTerrainTexInfo, passInfo);

	if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && GetCVars()->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
	{
		GetObjManager()->RemoveCullJobProducer();
	}
}

///////////////////////////////////////////////////////////////////////////////
void AddSpriteInfo(CThreadSafeRendererContainer<SVegetationSpriteInfo>& arrSpriteInfo, SVegetationSpriteInfo& rSpriteInfo)
{
	arrSpriteInfo.push_back(rSpriteInfo);
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::RenderVegetations(TDoublyLinkedList<IRenderNode>* lstObjects, int nRenderMask, bool bNodeCompletelyInFrustum, PodArray<CDLight*>* pAffectingLights, bool bSunOnly, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	CVars* pCVars = GetCVars();

	AABB objBox;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	bool bCheckPerObjectOcclusion = m_vNodeAxisRadius.len2() > pCVars->e_CoverageBufferCullIndividualBrushesMaxNodeSize * pCVars->e_CoverageBufferCullIndividualBrushesMaxNodeSize;

	const bool bRenderSprites = pCVars->e_VegetationSpritesBatching && !(nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES) && pCVars->e_VegetationSprites;
	CThreadSafeRendererContainer<SVegetationSpriteInfo>& arrSpriteInfo = GetObjManager()->m_arrVegetationSprites[passInfo.GetRecursiveLevel()][passInfo.ThreadID()];

	for (CVegetation* pObj = (CVegetation*)m_arrObjects[eRNListType_Vegetation].m_pFirstNode, *pNext; pObj; pObj = pNext)
	{
		pNext = (CVegetation*)pObj->m_pNext;

		passInfo.GetRendItemSorter().IncreaseObjectCounter();

		if (pNext)
			cryPrefetchT0SSE(pNext);

		IF(pObj->m_dwRndFlags & ERF_HIDDEN, 0)
			continue;

		if (pObj->m_dwRndFlags & ERF_STATIC_INSTANCING)
			break;

#if !defined(_RELEASE)
		if (GetCVars()->e_StaticInstancing == 2 && !pObj->m_pInstancingInfo)
			continue;
		if (GetCVars()->e_StaticInstancing == 3 && !pObj->m_pInstancingInfo)
			continue;
		if (GetCVars()->e_StaticInstancing == 4 && pObj->m_pInstancingInfo)
			continue;
#endif

		if (pObj->m_pInstancingInfo)
			objBox = pObj->m_pInstancingInfo->m_aabbBox;
		else
			pObj->FillBBox_NonVirtual(objBox);

		if (bNodeCompletelyInFrustum || passInfo.GetCamera().IsAABBVisible_FM(objBox))
		{
			float fEntDistanceSq = Distance::Point_AABBSq(vCamPos, objBox) * sqr(passInfo.GetZoomFactor());

			if (fEntDistanceSq < sqr(pObj->m_fWSMaxViewDist))
			{
				if (bRenderSprites && pObj->m_pSpriteInfo && !pObj->m_pSpriteInfo->ucShow3DModel && !pObj->m_pInstancingInfo)
				{
					pObj->m_pSpriteInfo->ucAlphaTestRef = 0;
					pObj->m_pSpriteInfo->ucDissolveOut = 255;

					AddSpriteInfo(arrSpriteInfo, *pObj->m_pSpriteInfo);
					continue;
				}

				float fEntDistance = sqrt_tpl(fEntDistanceSq);

				if (!bCheckPerObjectOcclusion || GetObjManager()->CheckOcclusion_TestAABB(objBox, fEntDistance))
					GetObjManager()->RenderVegetation(pObj, pAffectingLights, objBox, fEntDistance, bSunOnly, pTerrainTexInfo, bCheckPerObjectOcclusion, passInfo);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::RenderBrushes(TDoublyLinkedList<IRenderNode>* lstObjects, bool bNodeCompletelyInFrustum, PodArray<CDLight*>* pAffectingLights, bool bSunOnly, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	CVars* pCVars = GetCVars();
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	float cullMaxNodeSize = static_cast<float>(pCVars->e_CoverageBufferCullIndividualBrushesMaxNodeSize);
	bool bCheckPerObjectOcclusion = GetNodeRadius2() > cullMaxNodeSize * cullMaxNodeSize;

	for (CBrush* pObj = (CBrush*)lstObjects->m_pFirstNode, *pNext; pObj; pObj = pNext)
	{
		passInfo.GetRendItemSorter().IncreaseObjectCounter();
		pNext = (CBrush*)pObj->m_pNext;

		if (pObj->m_pNext)
			cryPrefetchT0SSE(pObj->m_pNext);

		IF(pObj->m_dwRndFlags & ERF_HIDDEN, 0)
			continue;
		AABB objBox;
		memcpy(&objBox, &pObj->m_WSBBox, sizeof(AABB));

		//		if(bNodeCompletelyInFrustum || passInfo.GetCamera().IsAABBVisible_FM( objBox ))
		if (bNodeCompletelyInFrustum || passInfo.GetCamera().IsAABBVisible_F(objBox))  // TODO: we must use multi-camera check here, but it is broken by dx12 development
		{
			float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, objBox)) * passInfo.GetZoomFactor();
			assert(fEntDistance >= 0 && _finite(fEntDistance));
			if (fEntDistance < pObj->m_fWSMaxViewDist)
			{
				if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && pCVars->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
				{
					// if object is visible, start CBrush::Render Job
					if (!bCheckPerObjectOcclusion || GetObjManager()->CheckOcclusion_TestAABB(objBox, fEntDistance))
					{
						GetObjManager()->RenderBrush((CBrush*)pObj, pAffectingLights, pTerrainTexInfo, objBox, fEntDistance, bSunOnly, m_pVisArea, bCheckPerObjectOcclusion, passInfo);
					}
				}
				else
				{
					GetObjManager()->RenderBrush(pObj, pAffectingLights, pTerrainTexInfo, pObj->m_WSBBox, fEntDistance, bSunOnly, m_pVisArea, bCheckPerObjectOcclusion, passInfo);
				}

			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::RenderDecalsAndRoads(TDoublyLinkedList<IRenderNode>* lstObjects, int nRenderMask, const Vec3& vAmbColor, bool bNodeCompletelyInFrustum, PodArray<CDLight*>* pAffectingLights, bool bSunOnly, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	CVars* pCVars = GetCVars();
	AABB objBox;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	bool bCheckPerObjectOcclusion = m_vNodeAxisRadius.len2() > pCVars->e_CoverageBufferCullIndividualBrushesMaxNodeSize * pCVars->e_CoverageBufferCullIndividualBrushesMaxNodeSize;

	for (IRenderNode* pObj = lstObjects->m_pFirstNode, *pNext; pObj; pObj = pNext)
	{
		passInfo.GetRendItemSorter().IncreaseObjectCounter();
		pNext = pObj->m_pNext;

		if (pObj->m_pNext)
			cryPrefetchT0SSE(pObj->m_pNext);

		IF(pObj->m_dwRndFlags & ERF_HIDDEN, 0)
			continue;

		pObj->FillBBox(objBox);

		if (bNodeCompletelyInFrustum || passInfo.GetCamera().IsAABBVisible_FM(objBox))
		{
			float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, objBox)) * passInfo.GetZoomFactor();
			assert(fEntDistance >= 0 && _finite(fEntDistance));
			if (fEntDistance < pObj->m_fWSMaxViewDist)
			{

#if !defined(_RELEASE)
				EERType rnType = pObj->GetRenderNodeType();
				if (!passInfo.RenderDecals() && rnType == eERType_Decal)
					continue;
				if (!passInfo.RenderRoads() && rnType == eERType_Road)
					continue;
#endif  // _RELEASE

				if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && pCVars->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
				{
					// if object is visible, write to output queue for main thread processing
					if (GetObjManager()->CheckOcclusion_TestAABB(objBox, fEntDistance))
					{
						GetObjManager()->PushIntoCullOutputQueue(SCheckOcclusionOutput::CreateDecalsAndRoadsOutput(pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, bSunOnly, bCheckPerObjectOcclusion, passInfo));
					}
				}
				else
				{
					GetObjManager()->RenderDecalAndRoad(pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, bSunOnly, bCheckPerObjectOcclusion, passInfo);
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::RenderCommonObjects(TDoublyLinkedList<IRenderNode>* lstObjects, int nRenderMask, const Vec3& vAmbColor, bool bNodeCompletelyInFrustum, PodArray<CDLight*>* pAffectingLights, bool bSunOnly, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	CVars* pCVars = GetCVars();
	AABB objBox;
	const Vec3 vCamPos = passInfo.GetCamera().GetPosition();

	for (IRenderNode* pObj = lstObjects->m_pFirstNode, *pNext; pObj; pObj = pNext)
	{
		passInfo.GetRendItemSorter().IncreaseObjectCounter();
		pNext = pObj->m_pNext;

		if (pObj->m_pNext)
			cryPrefetchT0SSE(pObj->m_pNext);

		IF(pObj->m_dwRndFlags & ERF_HIDDEN, 0)
			continue;

		pObj->FillBBox(objBox);
		EERType rnType = pObj->GetRenderNodeType();

		if (bNodeCompletelyInFrustum || passInfo.GetCamera().IsAABBVisible_FM(objBox))
		{
			float fEntDistance = sqrt_tpl(Distance::Point_AABBSq(vCamPos, objBox)) * passInfo.GetZoomFactor();
			assert(fEntDistance >= 0 && _finite(fEntDistance));
			if (fEntDistance < pObj->m_fWSMaxViewDist)
			{

				if (nRenderMask & OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES)
				{
					if (rnType == eERType_Light)
					{
						CLightEntity* pEnt = (CLightEntity*)pObj;
						if (!pEnt->GetEntityVisArea() && pEnt->GetEntityTerrainNode() && !(pEnt->m_light.m_Flags & DLF_THIS_AREA_ONLY))
						{
							// not "this area only" outdoor light affects everything
						}
						else
							continue;
					}
					else if (!pObj->GetOwnerEntity())
					{
						continue;
					}
				}

				if (rnType == eERType_Light)
				{
					bool bLightVisible = true;
					CLightEntity* pLightEnt = (CLightEntity*)pObj;

					// first check against camera view frustum
					CDLight* pLight = &pLightEnt->m_light;
					if (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)
					{
						OBB obb(OBB::CreateOBBfromAABB(Matrix33(pLight->m_ObjMatrix), AABB(-pLight->m_ProbeExtents, pLight->m_ProbeExtents)));
						bLightVisible = passInfo.GetCamera().IsOBBVisible_F(pLight->m_Origin, obb);
					}
					else if (pLightEnt->m_light.m_Flags & DLF_AREA_LIGHT)
					{
						// OBB test for area lights.
						Vec3 vBoxMax(pLight->m_fBaseRadius, pLight->m_fBaseRadius + pLight->m_fAreaWidth, pLight->m_fBaseRadius + pLight->m_fAreaHeight);
						Vec3 vBoxMin(-0.1f, -(pLight->m_fBaseRadius + pLight->m_fAreaWidth), -(pLight->m_fBaseRadius + pLight->m_fAreaHeight));

						OBB obb(OBB::CreateOBBfromAABB(Matrix33(pLight->m_ObjMatrix), AABB(vBoxMin, vBoxMax)));
						bLightVisible = passInfo.GetCamera().IsOBBVisible_F(pLight->m_Origin, obb);
					}
					else
						bLightVisible = passInfo.GetCamera().IsSphereVisible_F(Sphere(pLight->m_BaseOrigin, pLight->m_fBaseRadius));

					if (!bLightVisible)
						continue;
				}

				if (Get3DEngine()->IsStatObjBufferRenderTasksAllowed() && pCVars->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
				{
					// if object is visible, write to output queue for main thread processing
					if (rnType == eERType_DistanceCloud || GetObjManager()->CheckOcclusion_TestAABB(objBox, fEntDistance))
					{
						if (pObj->CanExecuteRenderAsJob())
						{
							GetObjManager()->RenderObject(pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, bSunOnly, rnType, passInfo);
						}
						else
						{
							GetObjManager()->PushIntoCullOutputQueue(SCheckOcclusionOutput::CreateCommonObjectOutput(pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, bSunOnly, pTerrainTexInfo, passInfo));
						}
					}
				}
				else
				{
					GetObjManager()->RenderObject(pObj, pAffectingLights, vAmbColor, objBox, fEntDistance, bSunOnly, rnType, passInfo);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::UnlinkObject(IRenderNode* pObj)
{
	ERNListType eListType = pObj->GetRenderNodeListId(pObj->GetRenderNodeType());

	if (GetCVars()->e_VegetationSpritesBatching)
	{
		if (eListType == eRNListType_Vegetation && ((CVegetation*)pObj)->m_pSpriteInfo)
		{
			CVegetation* pVeg = static_cast<CVegetation*>(pObj);
			SAFE_DELETE(pVeg->m_pSpriteInfo);
		}
	}

	assert(eListType >= 0 && eListType < eRNListType_ListsNum);
	TDoublyLinkedList<IRenderNode>& rList = m_arrObjects[eListType];

	assert(pObj != pObj->m_pPrev && pObj != pObj->m_pNext);
	assert(!rList.m_pFirstNode || !rList.m_pFirstNode->m_pPrev);
	assert(!rList.m_pLastNode || !rList.m_pLastNode->m_pNext);

	if (pObj->m_pNext || pObj->m_pPrev || pObj == rList.m_pLastNode || pObj == rList.m_pFirstNode)
		rList.remove(pObj);

	assert(!rList.m_pFirstNode || !rList.m_pFirstNode->m_pPrev);
	assert(!rList.m_pLastNode || !rList.m_pLastNode->m_pNext);
	assert(pObj != pObj->m_pPrev && pObj != pObj->m_pNext);
	assert(!pObj->m_pNext && !pObj->m_pPrev);
}

bool COctreeNode::DeleteObject(IRenderNode* pObj)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pObj->m_pOcNode && pObj->m_pOcNode != this)
		return ((COctreeNode*)(pObj->m_pOcNode))->DeleteObject(pObj);

	UnlinkObject(pObj);
	//	m_lstMergedObjects.Delete(pObj);

	for (int i = 0; i < m_lstCasters.Count(); i++)
	{
		if (m_lstCasters[i].pNode == pObj)
		{
			m_lstCasters.Delete(i);
			break;
		}
	}

	if (pObj->GetRenderNodeType() == eERType_Vegetation)
	{
		if (pObj->m_dwRndFlags & ERF_STATIC_INSTANCING || ((CVegetation*)pObj)->m_pInstancingInfo)
			m_bStaticInstancingIsDirty = true;
	}

	bool bSafeToUse = Get3DEngine()->IsSegmentSafeToUse(pObj->m_nSID);

	pObj->m_pOcNode = NULL;
	pObj->m_nSID = -1;

	if (!gEnv->IsEditor()) // in the editor in huge levels the usage of m_arrEmptyNodes optimization causes very long level loading time and very slow other whole world operations
	{
		if (bSafeToUse && IsEmpty() && m_arrEmptyNodes.Find(this) < 0)
			m_arrEmptyNodes.Add(this);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::InsertObject(IRenderNode* pObj, const AABB& objBox, const float fObjRadiusSqr, const Vec3& vObjCenter)
{
	FUNCTION_PROFILER_3DENGINE;

	COctreeNode* pCurrentNode = this;

	EERType eType = pObj->GetRenderNodeType();
	const uint32 renderFlags = (pObj->GetRndFlags() & (ERF_GOOD_OCCLUDER | ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS));

	const bool bTypeLight = (eType == eERType_Light);
	const float fViewDistRatioVegetation = GetCVars()->e_ViewDistRatioVegetation;
	const float fWSMaxViewDist = pObj->m_fWSMaxViewDist;
	const bool bTypeRoad = (eType == eERType_Road);

	Vec3 vObjectCentre = vObjCenter;

	while (true)
	{
		PrefetchLine(&pCurrentNode->m_arrChilds[0], 0);

#if !defined(_RELEASE)
		if (GetCVars()->e_CheckOctreeObjectsBoxSize != 0) // pCurrentNode->m_pParent is checked as silly sized things are added to the root (e.g. the sun)
			if (pCurrentNode->m_pParent && (objBox.min.len() > CHECK_OBJECTS_BOX_WARNING_SIZE || objBox.max.len() > CHECK_OBJECTS_BOX_WARNING_SIZE))
			{
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR_DBGBRK, "Huge object being added to a COctreeNode, name: '%s', objBox: [%f %f %f] -> [%f %f %f]\n", pObj->GetName(), objBox.min.x, objBox.min.y, objBox.min.z, objBox.max.x, objBox.max.y, objBox.max.z);
			}
#endif

		// parent bbox includes all children
		pCurrentNode->m_objectsBox.Add(objBox);

		pCurrentNode->m_fObjectsMaxViewDist = max(pCurrentNode->m_fObjectsMaxViewDist, fWSMaxViewDist);

		pCurrentNode->m_renderFlags |= renderFlags;

		pCurrentNode->m_bHasLights |= (bTypeLight);
		pCurrentNode->m_bHasRoads |= (bTypeRoad);

		if (pCurrentNode->m_vNodeAxisRadius.x * 2.0f > GetCVars()->e_ObjectsTreeNodeMinSize) // store voxels and roads in root
		{
			float nodeRadius = sqrt(pCurrentNode->GetNodeRadius2());

			if (fObjRadiusSqr < sqr(nodeRadius * GetCVars()->e_ObjectsTreeNodeSizeRatio))
			{
				if (fWSMaxViewDist < nodeRadius * fViewDistRatioVegetation)
				{
					int nChildId =
						((vObjCenter.x > pCurrentNode->m_vNodeCenter.x) ? 4 : 0) |
						((vObjCenter.y > pCurrentNode->m_vNodeCenter.y) ? 2 : 0) |
						((vObjCenter.z > pCurrentNode->m_vNodeCenter.z) ? 1 : 0);

					if (!pCurrentNode->m_arrChilds[nChildId])
					{
						pCurrentNode->m_arrChilds[nChildId] = COctreeNode::Create(pCurrentNode->m_nSID, pCurrentNode->GetChildBBox(nChildId), pCurrentNode->m_pVisArea, pCurrentNode);
					}

					pCurrentNode = pCurrentNode->m_arrChilds[nChildId];

					continue;
				}
			}
		}

		break;
	}

	//disable as it leads to some corruption on 360
	//	PrefetchLine(&pObj->m_pOcNode, 0);	//Writing to m_pOcNode was a common L2 cache miss

	pCurrentNode->LinkObject(pObj, eType);

	pObj->m_pOcNode = pCurrentNode;
#ifndef SEG_WORLD
	pObj->m_nSID = pCurrentNode->m_nSID;
#else
	pObj->m_nSID = pCurrentNode->m_nSID >= 0 ? pCurrentNode->m_nSID : GetTerrain()->WorldToSegment(vObjectCentre, GetDefSID());
#endif

	// only mark octree nodes as not compiled during loading and in the editor
	// otherwise update node (and parent node) flags on per added object basis
	if (m_bLevelLoadingInProgress || gEnv->IsEditor())
		pCurrentNode->SetCompiled(pCurrentNode->IsCompiled() & (eType == eERType_Light));
	else
		pCurrentNode->UpdateObjects(pObj);

	pCurrentNode->m_nManageVegetationsFrameId = 0;
}

//////////////////////////////////////////////////////////////////////////
AABB COctreeNode::GetChildBBox(int nChildId)
{
	int x = (nChildId / 4);
	int y = (nChildId - x * 4) / 2;
	int z = (nChildId - x * 4 - y * 2);
	const Vec3& vSize = m_vNodeAxisRadius;
	Vec3 vOffset = vSize;
	vOffset.x *= x;
	vOffset.y *= y;
	vOffset.z *= z;
	AABB childBox;
	childBox.min = m_vNodeCenter - vSize + vOffset;
	childBox.max = childBox.min + vSize;
	return childBox;
}

//////////////////////////////////////////////////////////////////////////
bool COctreeNode::IsEmpty()
{
	if (m_pParent)
		if (!m_arrChilds[0] && !m_arrChilds[1] && !m_arrChilds[2] && !m_arrChilds[3])
			if (!m_arrChilds[4] && !m_arrChilds[5] && !m_arrChilds[6] && !m_arrChilds[7])
				if (!HasObjects())
					return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool COctreeNode::IsRightNode(const AABB& objBox, const float fObjRadiusSqr, float fObjMaxViewDist)
{
	const AABB& nodeBox = GetNodeBox();
	if (!Overlap::Point_AABB(objBox.GetCenter(), nodeBox))
		if (m_pParent)
			return false;
	// fail if center is not inside or node bbox

	if (2 != Overlap::AABB_AABB_Inside(objBox, m_objectsBox))
		return false; // fail if not completely inside of objects bbox

	float fNodeRadiusRated = GetNodeRadius2() * sqr(GetCVars()->e_ObjectsTreeNodeSizeRatio);

	if (fObjRadiusSqr > fNodeRadiusRated * 4.f)
		if (m_pParent)
			return false;
	// fail if object is too big and we need to register it some of parents

	if (m_vNodeAxisRadius.x * 2.0f > GetCVars()->e_ObjectsTreeNodeMinSize)
		if (fObjRadiusSqr < fNodeRadiusRated)
			//      if(fObjMaxViewDist < m_fNodeRadius*GetCVars()->e_ViewDistRatioVegetation*fObjectToNodeSizeRatio)
			return false;
	// fail if object is too small and we need to register it some of childs

	return true;
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::LinkObject(IRenderNode* pObj, EERType eERType, bool bPushFront)
{
	ERNListType eListType = pObj->GetRenderNodeListId(eERType);

	TDoublyLinkedList<IRenderNode>& rList = m_arrObjects[eListType];

	assert(pObj != pObj->m_pPrev && pObj != pObj->m_pNext);
	assert(!pObj->m_pNext && !pObj->m_pPrev);
	assert(!rList.m_pFirstNode || !rList.m_pFirstNode->m_pPrev);
	assert(!rList.m_pLastNode || !rList.m_pLastNode->m_pNext);

	if (bPushFront)
		rList.insertBeginning(pObj);
	else
		rList.insertEnd(pObj);

	assert(!rList.m_pFirstNode || !rList.m_pFirstNode->m_pPrev);
	assert(!rList.m_pLastNode || !rList.m_pLastNode->m_pNext);
	assert(pObj != pObj->m_pPrev && pObj != pObj->m_pNext);
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::UpdateObjects(IRenderNode* pObj)
{
	FUNCTION_PROFILER_3DENGINE;

	float fObjMaxViewDistance = 0;
	size_t numCasters = 0;
	CObjManager* pObjManager = GetObjManager();

	auto nFlags = pObj->GetRndFlags();
	EERType eRType = pObj->GetRenderNodeType();
	float WSMaxViewDist = pObj->GetMaxViewDist();

	IF(nFlags & ERF_HIDDEN, 0)
		return;

	const Vec3& sunDir = Get3DEngine()->GetSunDirNormalized();
	uint32 sunDirX = (uint32)(sunDir.x * 63.5f + 63.5f);
	uint32 sunDirZ = (uint32)(sunDir.z * 63.5f + 63.5f);
	uint32 sunDirYs = (uint32)(sunDir.y < 0.0f ? 1 : 0);

	pObj->m_nInternalFlags &= ~(IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP);

	if (eRType == eERType_Vegetation)
	{
		CVegetation* pInst = (CVegetation*)pObj;
		pInst->UpdateRndFlags();
	}

	// update max view distances
	const float fNewMaxViewDist = pObj->GetMaxViewDist();
	pObj->m_fWSMaxViewDist = fNewMaxViewDist;

	if (eRType != eERType_Light && eRType != eERType_FogVolume && eRType != eERType_Decal && eRType != eERType_Road && eRType != eERType_DistanceCloud && eRType != eERType_CloudBlocker)
	{
		if (eRType == eERType_ParticleEmitter)
		{
			pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP;
		}
		else
		{
			CMatInfo* pMatInfo = (CMatInfo*)pObj->GetMaterial();
			if (pMatInfo)
			{
				if (pMatInfo->IsForwardRenderingRequired())
					pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;

				if (pMatInfo->IsNearestCubemapRequired())
					pObj->m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
			}

			if (eRType == eERType_MovableBrush)
			{
				if (IStatObj* pStatObj = pObj->GetEntityStatObj())
				{
					if (CMatInfo* pMat = (CMatInfo*)pStatObj->GetMaterial())
					{
						if (pMat->IsForwardRenderingRequired())
							pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;
						if (pMat->IsNearestCubemapRequired())
							pObj->m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
					}
				}

				if (!(pObj->m_nInternalFlags & (IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP)))
				{
					CompileCharacter(pObj->GetEntityCharacter(), pObj->m_nInternalFlags);
				}
			}
		}
	}

	bool bUpdateParentShadowFlags = false;
	bool bUpdateParentOcclusionFlags = false;

	// fill shadow casters list
	const bool bHasPerObjectShadow = GetCVars()->e_ShadowsPerObject && Get3DEngine()->GetPerObjectShadow(pObj);
	if (nFlags & ERF_CASTSHADOWMAPS && fNewMaxViewDist > fMinShadowCasterViewDist && eRType != eERType_Light && !bHasPerObjectShadow)
	{
		bUpdateParentShadowFlags = true;

		float fMaxCastDist = fNewMaxViewDist * GetCVars()->e_ShadowsCastViewDistRatio;
		m_lstCasters.Add(SCasterInfo(pObj, fMaxCastDist, eRType));

		if (pObj->GetRenderNodeType() == eERType_Vegetation && ((CVegetation*)pObj)->m_pInstancingInfo)
		{
			AABB objBox = ((CVegetation*)pObj)->m_pInstancingInfo->m_aabbBox;
			m_lstCasters.Last().objSphere.center = objBox.GetCenter();
			m_lstCasters.Last().objSphere.radius = objBox.GetRadius();
		}
	}

	fObjMaxViewDistance = max(fObjMaxViewDistance, fNewMaxViewDist);

	// traverse the octree upwards and fill in new flags
	COctreeNode* pNode = this;
	bool bContinue = false;
	do
	{
		// update max view dist
		if (pNode->m_fObjectsMaxViewDist < fObjMaxViewDistance)
		{
			pNode->m_fObjectsMaxViewDist = fObjMaxViewDistance;
			bContinue = true;
		}

		// update shadow flags
		if (bUpdateParentShadowFlags && (pNode->m_renderFlags & ERF_CASTSHADOWMAPS) == 0)
		{
			pNode->m_renderFlags |= ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS;
			bContinue = true;
		}

		pNode = pNode->m_pParent;
	} while (pNode != NULL && bContinue);

	m_fpSunDirX = sunDirX;
	m_fpSunDirZ = sunDirZ;
	m_fpSunDirYs = sunDirYs;
}

//////////////////////////////////////////////////////////////////////////
void COctreeNode::CompileCharacter(ICharacterInstance* pChar, uint32& nInternalFlags)
{
	const uint32 nCompileMask = IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP;
	if (pChar)
	{
		CMatInfo* pCharMaterial = (CMatInfo*)pChar->GetIMaterial();

		if (pCharMaterial)
		{
			uint32 nInternalFlagsNew = pCharMaterial->IsForwardRenderingRequired() ? IRenderNode::REQUIRES_FORWARD_RENDERING : 0;
			nInternalFlagsNew |= pCharMaterial->IsNearestCubemapRequired() ? IRenderNode::REQUIRES_NEAREST_CUBEMAP : 0;
			nInternalFlags |= nInternalFlagsNew;

			if (nInternalFlagsNew == nCompileMask)  // can do trivial return (all flags used)
				return;
		}

		if (IAttachmentManager* pAttMan = pChar->GetIAttachmentManager())
		{
			int nCount = pAttMan->GetAttachmentCount();
			for (int i = 0; i < nCount; i++)
			{
				if (IAttachment* pAtt = pAttMan->GetInterfaceByIndex(i))
					if (IAttachmentObject* pAttObj = pAtt->GetIAttachmentObject())
					{
						ICharacterInstance* pCharInstance = pAttObj->GetICharacterInstance();
						if (pCharInstance)
						{
							CompileCharacter(pCharInstance, nInternalFlags);

							if (nInternalFlags == nCompileMask)   // can do trivial return (all flags used)
								return;
						}

						if (IStatObj* pStatObj = pAttObj->GetIStatObj())
						{
							CMatInfo* pMat = (CMatInfo*)pAttObj->GetBaseMaterial();
							if (pMat == 0)
								pMat = (CMatInfo*)pStatObj->GetMaterial();

							if (pMat)
							{
								uint32 nInternalFlagsNew = pMat->IsForwardRenderingRequired() ? IRenderNode::REQUIRES_FORWARD_RENDERING : 0;
								nInternalFlagsNew |= pMat->IsNearestCubemapRequired() ? IRenderNode::REQUIRES_NEAREST_CUBEMAP : 0;
								nInternalFlags |= nInternalFlagsNew;

								if (nInternalFlagsNew == nCompileMask)  // can do trivial return (all flags used)
									return;
							}
						}

						if (IAttachmentSkin* pAttachmentSkin = pAttObj->GetIAttachmentSkin())
						{
							ISkin* pSkin = pAttachmentSkin->GetISkin();
							if (pSkin)
							{
								CMatInfo* pMat = (CMatInfo*)pAttObj->GetBaseMaterial();
								if (pMat == 0)
									pMat = (CMatInfo*)pSkin->GetIMaterial(0);

								if (pMat)
								{
									uint32 nInternalFlagsNew = pMat->IsForwardRenderingRequired() ? IRenderNode::REQUIRES_FORWARD_RENDERING : 0;
									nInternalFlagsNew |= pMat->IsNearestCubemapRequired() ? IRenderNode::REQUIRES_NEAREST_CUBEMAP : 0;
									nInternalFlags |= nInternalFlagsNew;

									if (nInternalFlagsNew == nCompileMask)  // can do trivial return (all flags used)
										return;
								}
							}
						}
					}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int16 CObjManager::GetNearestCubeProbe(PodArray<CDLight*>* pAffectingLights, IVisArea* pVisArea, const AABB& objBox, bool bSpecular, Vec4* pEnvProbeMults)
{
	// Only used for alpha blended geometry - but still should be optimized further
	float fMinDistance = FLT_MAX;
	int nMaxPriority = -1;
	CLightEntity* pNearestLight = NULL;
	int16 nDefaultId = Get3DEngine()->GetBlackCMTexID();

	if (!pVisArea)
	{
		int nObjTreeCount = Get3DEngine()->m_pObjectsTree.Count();
		for (int nCurrTree = 0; nCurrTree < nObjTreeCount; ++nCurrTree)
			if (Get3DEngine()->IsSegmentSafeToUse(nCurrTree))
				Get3DEngine()->m_pObjectsTree[nCurrTree]->GetNearestCubeProbe(fMinDistance, nMaxPriority, pNearestLight, &objBox);
	}
	else
		Get3DEngine()->GetVisAreaManager()->GetNearestCubeProbe(fMinDistance, nMaxPriority, pNearestLight, &objBox);

	if (pNearestLight)
	{
		ITexture* pTexCM = bSpecular ? pNearestLight->m_light.GetSpecularCubemap() : pNearestLight->m_light.GetDiffuseCubemap();
		// Return cubemap ID or -1 if invalid
		if (pEnvProbeMults)
		{
			const ColorF diffuse = pNearestLight->m_light.m_Color;
			*pEnvProbeMults = Vec4(diffuse.r, diffuse.g, diffuse.b, pNearestLight->m_light.m_SpecMult);
		}
		return (pTexCM && pTexCM->GetTextureType() >= eTT_Cube) ? pTexCM->GetTextureID() : nDefaultId;
	}

	if (pEnvProbeMults)
		*pEnvProbeMults = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	// No cubemap found
	return nDefaultId;
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::IsAfterWater(const Vec3& vPos, const Vec3& vCamPos, const SRenderingPassInfo& passInfo, float fUserWaterLevel)
{
	float fWaterLevel = fUserWaterLevel == WATER_LEVEL_UNKNOWN && GetTerrain() ? GetTerrain()->GetWaterLevel() : fUserWaterLevel;

	return (0.5f - passInfo.GetRecursiveLevel()) * (0.5f - passInfo.IsCameraUnderWater()) * (vPos.z - fWaterLevel) > 0;
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::RenderObjectDebugInfo(IRenderNode* pEnt, float fEntDistance, const SRenderingPassInfo& passInfo)
{
	if (!passInfo.IsGeneralPass())
		return;

	m_arrRenderDebugInfo.push_back(SObjManRenderDebugInfo(pEnt, fEntDistance));
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::FillTerrainTexInfo(IOctreeNode* pOcNode, float fEntDistance, struct SSectorTextureSet*& pTerrainTexInfo, const AABB& objBox)
{
	IVisArea* pVisArea = pOcNode->m_pVisArea;
	CTerrainNode* pTerrainNode = pOcNode->GetTerrainNode();

	if ((!pVisArea || pVisArea->IsAffectedByOutLights()) && pTerrainNode)
	{
		// provide terrain texture info
		AABB boxCenter;
		boxCenter.min = boxCenter.max = objBox.GetCenter();
		if (CTerrainNode* pTerNode = pTerrainNode)
			if (pTerNode = pTerNode->FindMinNodeContainingBox(boxCenter))
			{
				while ((!pTerNode->m_nNodeTexSet.nTex0 ||
					fEntDistance * 2.f * 8.f > (pTerNode->m_boxHeigtmapLocal.max.x - pTerNode->m_boxHeigtmapLocal.min.x))
					&& pTerNode->m_pParent)
					pTerNode = pTerNode->m_pParent;

				pTerrainTexInfo = &(pTerNode->m_nNodeTexSet);
			}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::RenderBrush(CBrush* pEnt, PodArray<CDLight*>* pAffectingLights,
	SSectorTextureSet* pTerrainTexInfo,
	const AABB& objBox,
	float fEntDistance,
	bool bSunOnly, CVisArea* pVisArea, bool nCheckOcclusion,
	const SRenderingPassInfo& passInfo)
{

	//	FUNCTION_PROFILER_3DENGINE;
	const CVars* pCVars = GetCVars();

#ifdef _DEBUG
	const char* szName = pEnt->GetName();
	const char* szClassName = pEnt->GetEntityClassName();
#endif // _DEBUG

#if !defined(_RELEASE)
	if (GetCVars()->e_CoverageBufferShowOccluder)
	{
		if (GetCVars()->e_CoverageBufferShowOccluder == 1 && (ERF_GOOD_OCCLUDER & ~pEnt->m_dwRndFlags))
			return;
		if (GetCVars()->e_CoverageBufferShowOccluder == 2 && (ERF_GOOD_OCCLUDER & pEnt->m_dwRndFlags))
			return;
	}
#endif
	if (pEnt->m_dwRndFlags & ERF_HIDDEN)
		return;

	// check cvars
	assert(passInfo.RenderBrushes());

	// check-allocate RNTmpData for visible objects
	if (!Get3DEngine()->CheckAndCreateRenderNodeTempData(&pEnt->m_pTempData, pEnt, passInfo))
	{
		return;
	}

	if (nCheckOcclusion && pEnt->m_pOcNode)
	{
		if (GetObjManager()->IsBoxOccluded(objBox, fEntDistance * passInfo.GetInverseZoomFactor(), &pEnt->m_pTempData->userData.m_OcclState,
			pEnt->m_pOcNode->m_pVisArea != NULL, eoot_OBJECT, passInfo))
			return;
	}
	assert(pEnt && pEnt->m_pTempData);
	if (!pEnt || !pEnt->m_pTempData)
		return;

	//////////////////////////////////////////////////////////////////////////
	const CLodValue lodValue = pEnt->ComputeLod(pEnt->m_pTempData->userData.nWantedLod, passInfo);

	if (GetCVars()->e_LodTransitionTime && passInfo.IsGeneralPass())
	{
		// Render current lod and (if needed) previous lod and perform time based lod transition using dissolve

		CLodValue arrlodVals[2];
		int nLodsNum = ComputeDissolve(lodValue, pEnt, fEntDistance, &arrlodVals[0]);

		for (int i = 0; i < nLodsNum; i++)
			pEnt->Render(arrlodVals[i], passInfo, pTerrainTexInfo, pAffectingLights);
	}
	else
	{
		pEnt->Render(lodValue, passInfo, pTerrainTexInfo, pAffectingLights);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::RemoveCullJobProducer()
{
	m_CheckOcclusionOutputQueue.RemoveProducer();
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::AddCullJobProducer()
{
	m_CheckOcclusionOutputQueue.AddProducer();
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::CheckOcclusion_TestAABB(const AABB& rAABB, float fEntDistance)
{
	return m_CullThread.TestAABB(rAABB, fEntDistance);
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::CheckOcclusion_TestQuad(const Vec3& vCenter, const Vec3& vAxisX, const Vec3& vAxisY)
{
	return m_CullThread.TestQuad(vCenter, vAxisX, vAxisY);
}

#ifndef _RELEASE
//////////////////////////////////////////////////////////////////////////
void CObjManager::CoverageBufferDebugDraw()
{
	m_CullThread.CoverageBufferDebugDraw();
}
#endif

//////////////////////////////////////////////////////////////////////////
bool CObjManager::LoadOcclusionMesh(const char* pFileName)
{
	return m_CullThread.LoadLevel(pFileName);
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::PushIntoCullQueue(const SCheckOcclusionJobData& rCheckOcclusionData)
{
#if !defined(_RELEASE)
	IF(!m_CullThread.IsActive(), 0)
	{
		__debugbreak();
	}
	IF(rCheckOcclusionData.type == SCheckOcclusionJobData::QUIT, 0)
		m_CullThread.SetActive(false);
#endif
	m_CheckOcclusionQueue.Push(rCheckOcclusionData);
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::PopFromCullQueue(SCheckOcclusionJobData* pCheckOcclusionData)
{
	m_CheckOcclusionQueue.Pop(pCheckOcclusionData);
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::PushIntoCullOutputQueue(const SCheckOcclusionOutput& rCheckOcclusionOutput)
{
	m_CheckOcclusionOutputQueue.Push(rCheckOcclusionOutput);
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::PopFromCullOutputQueue(SCheckOcclusionOutput* pCheckOcclusionOutput)
{
	return m_CheckOcclusionOutputQueue.Pop(pCheckOcclusionOutput);
}

///////////////////////////////////////////////////////////////////////////////
int CObjManager::GetObjectLOD(const IRenderNode* pObj, float fDistance)
{
	SFrameLodInfo frameLodInfo = Get3DEngine()->GetFrameLodInfo();
	int resultLod = MAX_STATOBJ_LODS_NUM - 1;

	bool bLodFaceArea = GetCVars()->e_LodFaceArea != 0;
	if (bLodFaceArea)
	{
		float distances[SMeshLodInfo::s_nMaxLodCount];
		bLodFaceArea = pObj->GetLodDistances(frameLodInfo, distances);
		if (bLodFaceArea)
		{
			for (uint i = 0; i < MAX_STATOBJ_LODS_NUM - 1; ++i)
			{
				if (fDistance < distances[i])
				{
					resultLod = i;
					break;
				}
			}
		}
	}

	if (!bLodFaceArea)
	{
		const float fLodRatioNorm = pObj->GetLodRatioNormalized();
		const float fRadius = pObj->GetBBox().GetRadius();
		resultLod = (int)(fDistance * (fLodRatioNorm * fLodRatioNorm) / (max(frameLodInfo.fLodRatio * min(fRadius, GetFloatCVar(e_LodCompMaxSize)), 0.001f)));
	}

	return resultLod;
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::CheckInitAffectingLights(const SRenderingPassInfo& passInfo)
{
	int nFrameID = passInfo.GetFrameID();
	int nRecursiveLevel = passInfo.GetRecursiveLevel();
	int nNewLightMaskFrameID = nFrameID + nRecursiveLevel;

	if (m_nLightMaskFrameId != nNewLightMaskFrameID)
	{
		m_lstAffectingLights.Clear();

		if (!m_pVisArea || m_pVisArea->IsAffectedByOutLights())
		{
			PodArray<CDLight*>* pSceneLights = Get3DEngine()->GetDynamicLightSources();
			if (pSceneLights->Count() && (pSceneLights->GetAt(0)->m_Flags & DLF_SUN))
				m_lstAffectingLights.Add(pSceneLights->GetAt(0));
		}

		m_nLightMaskFrameId = nNewLightMaskFrameID;
	}
}

///////////////////////////////////////////////////////////////////////////////
PodArray<CDLight*>* COctreeNode::GetAffectingLights(const SRenderingPassInfo& passInfo)
{
	CheckInitAffectingLights(passInfo);

	return &m_lstAffectingLights;
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::GetObjectsByFlags(uint dwFlags, PodArray<IRenderNode*>& lstObjects)
{

	unsigned int nCurrentObject(eRNListType_First);
	for (nCurrentObject = eRNListType_First; nCurrentObject < eRNListType_ListsNum; ++nCurrentObject)
	{
		for (IRenderNode* pObj = m_arrObjects[nCurrentObject].m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			if ((pObj->GetRndFlags() & dwFlags) == dwFlags)
				lstObjects.Add(pObj);
		}
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GetObjectsByFlags(dwFlags, lstObjects);
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::GetObjectsByType(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamCheckReady, uint64 dwFlags, bool bRecursive)
{
	if (objType == eERType_Light && !m_bHasLights)
		return;

	AABB objectsBox = m_objectsBox;

	if (objectsBox.IsReset())
		objectsBox = GetNodeBox();

	if (pBBox && !Overlap::AABB_AABB(*pBBox, objectsBox))
		return;

	ERNListType eListType = IRenderNode::GetRenderNodeListId(objType);

	if (pInstStreamCheckReady)
	{
		if (!CheckStartStreaming(false))
		{
			*pInstStreamCheckReady = false;

#if defined(FEATURE_SVO_GI)
			if (Cry3DEngineBase::GetCVars()->e_svoDebug == 7)
			{
				Cry3DEngineBase::Get3DEngine()->DrawBBox(objectsBox, Col_Red);
			}
#endif
		}
	}

	for (IRenderNode* pObj = m_arrObjects[eListType].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if ((pObj->GetRenderNodeType() == objType) && (pObj->GetRndFlags() & dwFlags))
		{
			AABB box;
			pObj->FillBBox(box);
			if (!pBBox || Overlap::AABB_AABB(*pBBox, box))
			{
				lstObjects.Add(pObj);
			}
		}
	}

	if (bRecursive)
	{
		for (int i = 0; i < 8; i++)
			if (m_arrChilds[i])
				m_arrChilds[i]->GetObjectsByType(lstObjects, objType, pBBox, pInstStreamCheckReady, dwFlags);
	}
}

///////////////////////////////////////////////////////////////////////////////
void COctreeNode::GetNearestCubeProbe(float& fMinDistance, int& nMaxPriority, CLightEntity*& pNearestLight, const AABB* pBBox)
{
	if (!m_bHasLights)
		return;

	if (!pBBox || pBBox && !Overlap::AABB_AABB(*pBBox, GetObjectsBBox()))
		return;

	Vec3 vCenter = pBBox->GetCenter();

	ERNListType eListType = IRenderNode::GetRenderNodeListId(eERType_Light);

	for (IRenderNode* pObj = m_arrObjects[eListType].m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		if (pObj->GetRenderNodeType() == eERType_Light)
		{
			CLightEntity* pLightEnt = (CLightEntity*)pObj;
			CDLight* pLight = &pLightEnt->m_light;

			if (pLightEnt->GetLayerId() != uint16(~0) && (pObj->m_dwRndFlags & ERF_HIDDEN))
				continue;
			if ((pLight->m_Flags & DLF_DISABLED) || (!GetCVars()->e_DynamicLights))
				continue;
			if (!(pLight->m_Flags & DLF_DEFERRED_CUBEMAPS))
				continue;

			AABB box;
			pObj->FillBBox(box);
			if (Overlap::AABB_AABB(*pBBox, box))
			{
				Vec3 vCenterRel = vCenter - pLight->GetPosition();
				Vec3 vCenterOBBSpace;
				vCenterOBBSpace.x = pLightEnt->m_Matrix.GetColumn0().GetNormalized().dot(vCenterRel);
				vCenterOBBSpace.y = pLightEnt->m_Matrix.GetColumn1().GetNormalized().dot(vCenterRel);
				vCenterOBBSpace.z = pLightEnt->m_Matrix.GetColumn2().GetNormalized().dot(vCenterRel);

				// Check if object center is within probe OBB
				Vec3 vProbeExtents = pLight->m_ProbeExtents;
				if (fabs(vCenterOBBSpace.x) < vProbeExtents.x && fabs(vCenterOBBSpace.y) < vProbeExtents.y && fabs(vCenterOBBSpace.z) < vProbeExtents.z)
				{
					if (pLight->m_nSortPriority > nMaxPriority)
					{
						pNearestLight = (CLightEntity*)pObj;
						nMaxPriority = pLight->m_nSortPriority;
						fMinDistance = 0;
					}
				}
			}
		}
	}

	for (int i = 0; i < 8; i++)
		if (m_arrChilds[i])
			m_arrChilds[i]->GetNearestCubeProbe(fMinDistance, nMaxPriority, pNearestLight, pBBox);
}

///////////////////////////////////////////////////////////////////////////////
bool CObjManager::IsBoxOccluded(const AABB& objBox,
	float fDistance,
	OcclusionTestClient* const __restrict pOcclTestVars,
	bool /* bIndoorOccludersOnly*/,
	EOcclusionObjectType eOcclusionObjectType,
	const SRenderingPassInfo& passInfo)
{
	// if object was visible during last frames
	const uint32 mainFrameID = passInfo.GetMainFrameID();

	if (GetCVars()->e_OcclusionLazyHideFrames)
	{
		//This causes massive spikes in draw calls when rotating
		if (pOcclTestVars->nLastVisibleMainFrameID > mainFrameID - GetCVars()->e_OcclusionLazyHideFrames)
		{
			// prevent checking all objects in same frame
			int nId = (int)(UINT_PTR(pOcclTestVars) / 256);
			if ((nId & 7) != (mainFrameID & 7))
				return false;
		}
	}

	// use fast and reliable test right here
	CVisAreaManager* pVisAreaManager = GetVisAreaManager();
	if (GetCVars()->e_OcclusionVolumes && pVisAreaManager && pVisAreaManager->IsOccludedByOcclVolumes(objBox, passInfo))
	{
		pOcclTestVars->nLastOccludedMainFrameID = mainFrameID;
		return true;
	}

	if (GetCVars()->e_CoverageBuffer)
	{
		CullQueue().AddItem(objBox, fDistance, pOcclTestVars, mainFrameID);
		return pOcclTestVars->nLastOccludedMainFrameID == mainFrameID - 1;
	}

	pOcclTestVars->nLastVisibleMainFrameID = mainFrameID;

	return false;
}

