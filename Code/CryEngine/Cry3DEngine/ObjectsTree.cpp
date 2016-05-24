// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

const float COctreeNode::fMinShadowCasterViewDist = 8.f;

PodArray<COctreeNode*> COctreeNode::m_arrEmptyNodes;
PodArray<COctreeNode*> COctreeNode::m_arrStreamedInNodes;
int COctreeNode::m_nInstStreamTasksInProgress = 0;
FILE* COctreeNode::m_pFileForSyncRead = 0;
int COctreeNode::m_nNodesCounterAll = 0;
int COctreeNode::m_nNodesCounterStreamable = 0;
int COctreeNode::m_nInstCounterLoaded = 0;

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
		float fSwitchRange = min(fSpriteSwitchDist * GetCVars()->e_DissolveSpriteDistRatio, GetCVars()->e_DissolveSpriteMinDist);

		if (pObj->m_pSpriteInfo)
		{
			CStatObj* pStatObj = vegetGroup.GetStatObj();

			if (fEntDistance2D < (fSpriteSwitchDist - fSwitchRange) && pObj && pObj->m_pTempData)
			{
				int nLodA;

				nLodA = CLAMP(pObj->m_pTempData->userData.nWantedLod, (uint32)pStatObj->GetMinUsableLod(), (uint32)pStatObj->m_nMaxUsableLod);
				nLodA = pStatObj->FindNearesLoadedLOD(nLodA);

				// start dissolve transition to 3d lod
				SLodDistDissolveTransitionState* pLodDissolveTransitionState = &pObj->m_pTempData->userData.lodDistDissolveTransitionState;
				GetObjManager()->GetLodDistDissolveRef(pLodDissolveTransitionState, fEntDistance2D, nLodA, passInfo);
			}

			float fDissolveRef = 1.0f;
			bool dissolveFinished = false;

			if (pObj->m_pTempData)
			{
				SLodDistDissolveTransitionState* pLodDistDissolveTransitionState = &pObj->m_pTempData->userData.lodDistDissolveTransitionState;

				if (passInfo.IsGeneralPass() && passInfo.IsZoomInProgress())
					pLodDistDissolveTransitionState->nOldLod = pLodDistDissolveTransitionState->nNewLod;

				float fDissolve = GetObjManager()->GetLodDistDissolveRef(pLodDistDissolveTransitionState, fEntDistance2D, pLodDistDissolveTransitionState->nNewLod, passInfo);
				fDissolveRef = SATURATE(pLodDistDissolveTransitionState->nOldLod == -1 ? 1.f - fDissolve : fDissolve);

				if (pLodDistDissolveTransitionState->nOldLod == pLodDistDissolveTransitionState->nNewLod)
					fDissolveRef = 1.0f;

				dissolveFinished = (pLodDistDissolveTransitionState->nOldLod != -1 &&
				                    pLodDistDissolveTransitionState->nNewLod != -1);
			}

			if (dissolveFinished || fEntDistanceSqr > sqr(pObj->m_fWSMaxViewDist * 1.1f))
			{
				SAFE_DELETE(pObj->m_pSpriteInfo);

				UnlinkObject(pObj);
				LinkObject(pObj, eERType_Vegetation); //We know that only eERType_Vegetation can get into the vegetation list, see GetRenderNodeListId()

				SetCompiled(false);

				continue;
			}

			float dist3D = fSpriteSwitchDist - fSwitchRange + GetFloatCVar(e_DissolveDistband);

			pObj->UpdateSpriteInfo(*pObj->m_pSpriteInfo, fDissolveRef, pTerrainTexInfo, passInfo);
			pObj->m_pSpriteInfo->ucShow3DModel = (fEntDistance2D < dist3D);
		}
		else if (!pObj->m_pInstancingInfo)
		{
			if (fEntDistance2D > (fSpriteSwitchDist - fSwitchRange) && fEntDistance2D + GetFloatCVar(e_DissolveDistband) < pObj->m_fWSMaxViewDist)
			{
				UnlinkObject(pObj);
				LinkObject(pObj, eERType_Vegetation, false); //We know that only eERType_Vegetation can get into the vegetation list, see GetRenderNodeListId()

				assert(pObj->m_pSpriteInfo == NULL);
				pObj->m_pSpriteInfo = new SVegetationSpriteInfo;
				SVegetationSpriteInfo& si = *pObj->m_pSpriteInfo;

				if (pObj->m_pTempData)
				{
					// start dissolve transition to -1 (sprite)
					SLodDistDissolveTransitionState* pLodDissolveTransitionState = &pObj->m_pTempData->userData.lodDistDissolveTransitionState;
					GetObjManager()->GetLodDistDissolveRef(pLodDissolveTransitionState, fEntDistance2D, -1, passInfo);

					if (passInfo.IsGeneralPass() && passInfo.IsZoomInProgress())
						pLodDissolveTransitionState->nOldLod = pLodDissolveTransitionState->nNewLod;

					si.ucAlphaTestRef = 0;
					si.ucDissolveOut = 0;
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

	if (GetCVars()->e_VegetationSpritesBatching)
		CheckManageVegetationSprites(fNodeDistance, (int)(fNodeDistance * 0.2f), passInfo);

	if (HasAnyRenderableCandidates(passInfo))
	{
		// when using the occlusion culler, push the work to the jobs doing the occlusion checks, else just compute in main thread
		if (GetCVars()->e_StatObjBufferRenderTasks == 1 && passInfo.IsGeneralPass() && JobManager::InvokeAsJob("CheckOcclusion"))
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
			int nFlags = pObj->GetRndFlags();

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

			bool bVegetHasAlphaTrans = false;

			// update vegetation instances data
			EERType eRType = pObj->GetRenderNodeType();
			if (eRType == eERType_Vegetation)
			{
				CVegetation* pInst = (CVegetation*)pObj;
				pInst->UpdateRndFlags();
				StatInstGroup& vegetGroup = pInst->GetStatObjGroup();
				if (vegetGroup.pStatObj && vegetGroup.bUseAlphaBlending)
					bVegetHasAlphaTrans = true;
			}

			// update max view distances
			const float fNewMaxViewDist = pObj->GetMaxViewDist();
			pObj->m_fWSMaxViewDist = fNewMaxViewDist;

			// update REQUIRES_FORWARD_RENDERING flag
			//IF(GetCVars()->e_ShadowsOnAlphaBlend,0)
			{
				pObj->m_nInternalFlags &= ~(IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP);
				if (eRType != eERType_Light &&
				    eRType != eERType_Cloud &&
				    eRType != eERType_FogVolume &&
				    eRType != eERType_Decal &&
				    eRType != eERType_Road &&
				    eRType != eERType_DistanceCloud)
				{
					if (eRType == eERType_ParticleEmitter)
						pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING | IRenderNode::REQUIRES_NEAREST_CUBEMAP;

					if (CMatInfo* pMatInfo = (CMatInfo*)pObj->GetMaterial())
					{
						if (bVegetHasAlphaTrans || pMatInfo->IsForwardRenderingRequired())
							pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;

						if (pMatInfo->IsNearestCubemapRequired())
							pObj->m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
					}

					if (eRType == eERType_RenderProxy)
					{
						int nSlotCount = pObj->GetSlotCount();

						for (int s = 0; s < nSlotCount; s++)
						{
							if (CMatInfo* pMat = (CMatInfo*)pObj->GetEntitySlotMaterial(s))
							{
								if (pMat->IsForwardRenderingRequired())
									pObj->m_nInternalFlags |= IRenderNode::REQUIRES_FORWARD_RENDERING;
								if (pMat->IsNearestCubemapRequired())
									pObj->m_nInternalFlags |= IRenderNode::REQUIRES_NEAREST_CUBEMAP;
							}

							if (IStatObj* pStatObj = pObj->GetEntityStatObj(s))
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
						CompileCharacter(pObj->GetEntityCharacter(0), pObj->m_nInternalFlags);
				}
			}

			int nFlags = pObj->GetRndFlags();

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
		CStatObj* pStatObj = (CStatObj*)pInst->GetEntityStatObj(0, 0, &objMatrix);
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
				PodArrayAABB<CRenderObject::SInstanceData>* pInsts = pFirstNode->m_pInstancingInfo = new PodArrayAABB<CRenderObject::SInstanceData>;
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

					(*pInsts)[i].m_MatInst = ii.nodeMatrix;
					(*pInsts)[i].m_vDissolveInfo.zero();
					(*pInsts)[i].m_vBendInfo = Vec4(pStatObj->m_fRadiusVert, 0, 0, 0.1f * vegetGroup.fBending);

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

				if (eRType == eERType_RenderProxy)
				{
					if (pObj->IsMovableByGame())
						continue;
				}
				else if (
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

void C3DEngine::GetObjectsByTypeGlobal(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamReady)
{
	for (int nSID = 0; nSID < Get3DEngine()->m_pObjectsTree.Count(); nSID++)
		if (Get3DEngine()->IsSegmentSafeToUse(nSID))
			Get3DEngine()->m_pObjectsTree[nSID]->GetObjectsByType(lstObjects, objType, pBBox, pInstStreamReady);
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

bool COctreeNode::GetShadowCastersTimeSliced(IRenderNode* pIgnoreNode, ShadowMapFrustum* pFrustum, int renderNodeExcludeFlags, int& totalRemainingNodes, int nCurLevel, const SRenderingPassInfo& passInfo)
{
	assert(pFrustum->pShadowCacheData);

	if (totalRemainingNodes <= 0)
		return false;

	if (!pFrustum->pShadowCacheData->mOctreePathNodeProcessed[nCurLevel])
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

					const int nFlags = pNode->GetRndFlags();
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
					for (int nSlot = 0; nSlot < pNode->GetSlotCount(); ++nSlot)
					{
						bool bCanRender = false;

						if (IStatObj* pStatObj = pNode->GetEntityStatObj(nSlot))
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
						else if (ICharacterInstance* pCharacter = pNode->GetEntityCharacter(nSlot))
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
			bool bDone = m_arrChilds[i]->GetShadowCastersTimeSliced(pIgnoreNode, pFrustum, renderNodeExcludeFlags, totalRemainingNodes, nCurLevel + 1, passInfo);

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
	COMPILE_TIME_ASSERT(eERType_TypesNum == 25);//if eERType number is changed, have to check this code.
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

	if (GetCVars()->e_VegetationSpritesBatching)
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
