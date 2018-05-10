// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjman.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Loading trees, buildings, ragister/unregister entities for rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CCullThread.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "Brush.h"
#include <CryAnimation/ICryAnimation.h>
#include "DecalRenderNode.h"
#include "FogVolumeRenderNode.h"
#include "MergedMeshRenderNode.h"
#include "RoadRenderNode.h"
#include "GeomCacheRenderNode.h"

int CObjManager::m_nUpdateStreamingPrioriryRoundId = 1;
int CObjManager::m_nUpdateStreamingPrioriryRoundIdFast = 1;
int CObjManager::s_nLastStreamingMemoryUsage = 0;

struct CObjManager_Cmp_Streamable_Priority
{

	// returns true if v1 < v2
	ILINE bool operator()(const SStreamAbleObject& v1, const SStreamAbleObject& v2) const
	{
		IStreamable* arrObj[2] = { v1.GetStreamAbleObject(), v2.GetStreamAbleObject() };

		// compare priorities
		if (v1.fCurImportance > v2.fCurImportance)
			return true;
		if (v1.fCurImportance < v2.fCurImportance)
			return false;

		// give low lod's and small meshes higher priority
		int MemUsage0 = v1.GetStreamableContentMemoryUsage();
		int MemUsage1 = v2.GetStreamableContentMemoryUsage();
		if (MemUsage0 < MemUsage1)
			return true;
		if (MemUsage0 > MemUsage1)
			return false;

		// fix sorting consistency
		if (arrObj[0] > arrObj[1])
			return true;
		if (arrObj[0] < arrObj[1])
			return false;

		return false;
	}
};

void CObjManager::RegisterForStreaming(IStreamable* pObj)
{
	SStreamAbleObject streamAbleObject(pObj);
	if (m_arrStreamableObjects.Find(streamAbleObject) < 0)
	{
		m_arrStreamableObjects.Add(streamAbleObject);

#ifdef OBJMAN_STREAM_STATS
		if (m_pStreamListener)
		{
			string name;
			pObj->GetStreamableName(name);
			m_pStreamListener->OnCreatedStreamedObject(name.c_str(), pObj);
		}
#endif
	}
}

void CObjManager::UnregisterForStreaming(IStreamable* pObj)
{
	if (m_arrStreamableObjects.size() > 0)
	{
		SStreamAbleObject streamAbleObject(pObj, false);
		bool deleted = m_arrStreamableObjects.Delete(streamAbleObject);

#ifdef OBJMAN_STREAM_STATS
		if (deleted && m_pStreamListener)
			m_pStreamListener->OnDestroyedStreamedObject(pObj);
#endif

		if (m_arrStreamableObjects.empty())
			stl::free_container(m_arrStreamableObjects);
	}
}

void CObjManager::UpdateObjectsStreamingPriority(bool bSyncLoad, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	const size_t nPrecachePoints = m_vStreamPreCachePointDefs.size();
	const bool bNeedsUnique = nPrecachePoints > 1;

	// make sure userData.nWantedLod is updated immediately once instancing is enabled
	static int nOld_e_StaticInstancing = GetCVars()->e_StaticInstancing;
	if (nOld_e_StaticInstancing != GetCVars()->e_StaticInstancing)
		bSyncLoad = true;
	nOld_e_StaticInstancing = GetCVars()->e_StaticInstancing;

	if (bSyncLoad)
	{
		PrintMessage("Updating level streaming priorities for %" PRISIZE_T " cameras (LevelFrameId = %d)", nPrecachePoints, Get3DEngine()->GetStreamingFramesSinceLevelStart());
		for (size_t pci = 0; pci < m_vStreamPreCacheCameras.size(); ++pci)
			PrintMessage("-- %f %f %f",
			             m_vStreamPreCacheCameras[pci].vPosition.x,
			             m_vStreamPreCacheCameras[pci].vPosition.y,
			             m_vStreamPreCacheCameras[pci].vPosition.z);
	}

	CVisAreaManager* pVisAreaMgr = GetVisAreaManager();

	if (bSyncLoad)
	{
		m_arrStreamingNodeStack.clear();
	}

	bool bPrecacheNear = true;

	if (bSyncLoad || (passInfo.GetFrameID() & 3) || GetFloatCVar(e_StreamCgfFastUpdateMaxDistance) == 0)
	{
		bPrecacheNear = false;

		if (!m_arrStreamingNodeStack.Count())
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "UpdateObjectsStreamingPriority_Init");

			if (GetCVars()->e_StreamCgf == 2)
				PrintMessage("UpdateObjectsStreamingPriority_Restart %d", passInfo.GetFrameID());

			for (size_t ppIdx = 0, ppCount = nPrecachePoints; ppIdx != ppCount; ++ppIdx)
			{
				const Vec3& vPrecachePoint = m_vStreamPreCacheCameras[ppIdx].vPosition;

				CVisArea* pCurArea = pVisAreaMgr ? (CVisArea*)pVisAreaMgr->GetVisAreaFromPos(vPrecachePoint) : NULL;
				if (CVisArea* pRoot0 = pCurArea)
				{
					m_tmpAreas0.Clear();
					pRoot0->AddConnectedAreas(m_tmpAreas0, GetCVars()->e_StreamPredictionMaxVisAreaRecursion);

					bool bFoundOutside = false;

					if (!GetCVars()->e_StreamPredictionAlwaysIncludeOutside)
					{
						for (int v = 0; v < m_tmpAreas0.Count(); v++)
						{
							CVisArea* pN1 = m_tmpAreas0[v];
							if (pN1->IsPortal() && pN1->m_lstConnections.Count() == 1)
							{
								bFoundOutside = true;
								break;
							}
						}
					}
					else
					{
						bFoundOutside = true;
					}

					if (bFoundOutside && Get3DEngine()->IsObjectsTreeValid())
					{
							m_arrStreamingNodeStack.Add(Get3DEngine()->GetObjectsTree());
					}

					for (int v = 0; v < m_tmpAreas0.Count(); v++)
					{
						CVisArea* pN1 = m_tmpAreas0[v];
						assert(bNeedsUnique || m_arrStreamingNodeStack.Find(pN1->GetObjectsTree()) < 0);
						if (pN1->IsObjectsTreeValid())
						{
							m_arrStreamingNodeStack.Add(pN1->GetObjectsTree());
						}
					}
				}
				else if (GetVisAreaManager())
				{
					if (Get3DEngine()->IsObjectsTreeValid()) 
					{
						m_arrStreamingNodeStack.Add(Get3DEngine()->GetObjectsTree());
					}

					// find portals around
					m_tmpAreas0.Clear();
					GetVisAreaManager()->MakeActiveEntransePortalsList(NULL, m_tmpAreas0, NULL, passInfo);

					// make list of areas for streaming
					m_tmpAreas1.Clear();
					for (int p = 0; p < m_tmpAreas0.Count(); p++)
						if (CVisArea* pRoot = m_tmpAreas0[p])
							pRoot->AddConnectedAreas(m_tmpAreas1, GetCVars()->e_StreamPredictionMaxVisAreaRecursion);

					// fill list of object trees
					for (int v = 0; v < m_tmpAreas1.Count(); v++)
					{
						CVisArea* pN1 = m_tmpAreas1[v];
						assert(bNeedsUnique || m_arrStreamingNodeStack.Find(pN1->GetObjectsTree()) < 0);
						if (pN1->IsObjectsTreeValid())
						{
							m_arrStreamingNodeStack.Add(pN1->GetObjectsTree());
						}
					}
				}
				else
				{
					if (Get3DEngine()->IsObjectsTreeValid())
					{
						m_arrStreamingNodeStack.Add(Get3DEngine()->GetObjectsTree());
					}
				}
			}

			IF_UNLIKELY (bNeedsUnique)
			{
				std::sort(m_arrStreamingNodeStack.begin(), m_arrStreamingNodeStack.end());
				m_arrStreamingNodeStack.resize(std::distance(m_arrStreamingNodeStack.begin(), std::unique(m_arrStreamingNodeStack.begin(), m_arrStreamingNodeStack.end())));
			}
		}

		{
			// Time-sliced scene streaming priority update
			// Update scene faster if in zoom and if camera moving fast
			float fMaxTimeToSpendMS = GetCVars()->e_StreamPredictionUpdateTimeSlice * max(Get3DEngine()->GetAverageCameraSpeed() * .5f, 1.f) / max(passInfo.GetZoomFactor(), 0.1f);
			fMaxTimeToSpendMS = min(fMaxTimeToSpendMS, GetCVars()->e_StreamPredictionUpdateTimeSlice * 2.f);

			CTimeValue maxTimeToSpend;
			maxTimeToSpend.SetSeconds(fMaxTimeToSpendMS * 0.001f);

			const CTimeValue startTime = GetTimer()->GetAsyncTime();

			const float fMinDist = GetFloatCVar(e_StreamPredictionMinFarZoneDistance);
			const float fMaxViewDistance = Get3DEngine()->GetMaxViewDistance();

			{
				CRY_PROFILE_REGION(PROFILE_3DENGINE, "UpdateObjectsStreamingPriority_MarkNodes");

				while (m_arrStreamingNodeStack.Count())
				{
					COctreeNode* pLast = m_arrStreamingNodeStack.Last();
					m_arrStreamingNodeStack.DeleteLast();

					pLast->UpdateStreamingPriority(m_arrStreamingNodeStack, fMinDist, fMaxViewDistance, false, &m_vStreamPreCacheCameras[0], nPrecachePoints, passInfo);

					if (!bSyncLoad && (GetTimer()->GetAsyncTime() - startTime) > maxTimeToSpend)
						break;
				}
			}
		}

		if (!m_arrStreamingNodeStack.Count())
		{
			// Round has done.
			++m_nUpdateStreamingPrioriryRoundId;
		}
	}

	if (bPrecacheNear || bSyncLoad)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "UpdateObjectsStreamingPriority_Mark_NEAR_Nodes");

		PodArray<COctreeNode*> fastStreamingNodeStack;
		const int nVisAreaRecursion = min(GetCVars()->e_StreamPredictionMaxVisAreaRecursion, 2);

		for (size_t ppIdx = 0, ppCount = nPrecachePoints; ppIdx != ppCount; ++ppIdx)
		{
			const Vec3& vPrecachePoint = m_vStreamPreCacheCameras[ppIdx].vPosition;

			CVisArea* pCurArea = pVisAreaMgr ? (CVisArea*)pVisAreaMgr->GetVisAreaFromPos(vPrecachePoint) : NULL;
			if (CVisArea* pRoot0 = pCurArea)
			{
				m_tmpAreas0.Clear();
				pRoot0->AddConnectedAreas(m_tmpAreas0, nVisAreaRecursion);

				bool bFoundOutside = false;

				if (!GetCVars()->e_StreamPredictionAlwaysIncludeOutside)
				{
					for (int v = 0; v < m_tmpAreas0.Count(); v++)
					{
						CVisArea* pN1 = m_tmpAreas0[v];
						if (pN1->IsPortal() && pN1->m_lstConnections.Count() == 1)
						{
							bFoundOutside = true;
							break;
						}
					}
				}
				else
				{
					bFoundOutside = true;
				}

				if (bFoundOutside && Get3DEngine()->IsObjectsTreeValid())
				{
					fastStreamingNodeStack.Add(Get3DEngine()->GetObjectsTree());
				}

				for (int v = 0; v < m_tmpAreas0.Count(); v++)
				{
					CVisArea* pN1 = m_tmpAreas0[v];
					assert(bNeedsUnique || fastStreamingNodeStack.Find(pN1->GetObjectsTree()) < 0);
					if (pN1->IsObjectsTreeValid())
					{
						fastStreamingNodeStack.Add(pN1->GetObjectsTree());
					}
				}
			}
			else if (GetVisAreaManager())
			{
				if (Get3DEngine()->IsObjectsTreeValid())
				{
					fastStreamingNodeStack.Add(Get3DEngine()->GetObjectsTree());
				}

				// find portals around
				m_tmpAreas0.Clear();
				GetVisAreaManager()->MakeActiveEntransePortalsList(NULL, m_tmpAreas0, NULL, passInfo);

				// make list of areas for streaming
				m_tmpAreas1.Clear();
				for (int p = 0; p < m_tmpAreas0.Count(); p++)
					if (CVisArea* pRoot = m_tmpAreas0[p])
						pRoot->AddConnectedAreas(m_tmpAreas1, nVisAreaRecursion);

				// fill list of object trees
				for (int v = 0; v < m_tmpAreas1.Count(); v++)
				{
					CVisArea* pN1 = m_tmpAreas1[v];
					assert(bNeedsUnique || fastStreamingNodeStack.Find(pN1->GetObjectsTree()) < 0);
					if (pN1->IsObjectsTreeValid())
					{
						fastStreamingNodeStack.Add(pN1->GetObjectsTree());
					}
				}
			}
			else
			{
				if (Get3DEngine()->IsObjectsTreeValid())
				{
					fastStreamingNodeStack.Add(Get3DEngine()->GetObjectsTree());
				}
			}
		}

		IF_UNLIKELY (bNeedsUnique)
		{
			std::sort(fastStreamingNodeStack.begin(), fastStreamingNodeStack.end());
			fastStreamingNodeStack.resize(std::distance(fastStreamingNodeStack.begin(), std::unique(fastStreamingNodeStack.begin(), fastStreamingNodeStack.end())));
		}

		const float fMaxDist = max(0.f, GetFloatCVar(e_StreamCgfFastUpdateMaxDistance) - GetFloatCVar(e_StreamPredictionDistanceFar));

		while (fastStreamingNodeStack.Count())
		{
			COctreeNode* pLast = fastStreamingNodeStack.Last();
			fastStreamingNodeStack.DeleteLast();

			pLast->UpdateStreamingPriority(fastStreamingNodeStack, 0.f, fMaxDist, true, &m_vStreamPreCacheCameras[0], nPrecachePoints, passInfo);
		}

		m_nUpdateStreamingPrioriryRoundIdFast++;
	}
}

static bool AreTexturesStreamed(IMaterial* pMaterial, float fMipFactor)
{
	// check texture streaming distances
	if (pMaterial)
	{
		if (IRenderShaderResources* pRes = pMaterial->GetShaderItem().m_pShaderResources)
		{
			for (int iSlot = 0; iSlot < EFTT_MAX; ++iSlot)
			{
				if (SEfResTexture* pResTex = pRes->GetTexture(iSlot))
				{
					if (ITexture* pITex = pResTex->m_Sampler.m_pITex)
					{
						float fCurMipFactor = fMipFactor * pResTex->GetTiling(0) * pResTex->GetTiling(1);

						if (!pITex->IsParticularMipStreamed(fCurMipFactor))
							return false;
					}
				}
			}
		}
	}

	return true;
}

void CObjManager::CheckTextureReadyFlag()
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_lstStaticTypes.empty())
		return;

	static uint32 nGroupId = 0;

	PodArray<StatInstGroup>& rGroupTable = m_lstStaticTypes;

	if (nGroupId >= rGroupTable.size())
	{
		nGroupId = 0;
	}

	for (size_t currentGroup = 0; currentGroup < rGroupTable.size(); currentGroup++)
	{
		StatInstGroup& rGroup = rGroupTable[currentGroup];

		if (CStatObj* pStatObj = rGroup.GetStatObj())
		{
			for (int j = 0; j < FAR_TEX_COUNT; ++j)
			{
				SVegetationSpriteLightInfo& rLightInfo = rGroup.m_arrSSpriteLightInfo[j];
				if (rLightInfo.m_pDynTexture && rLightInfo.m_pDynTexture->GetFlags() & IDynTexture::fNeedRegenerate)
				{
					IMaterial* pMaterial = rGroup.pMaterial;
					if (pMaterial == NULL)
						pMaterial = pStatObj->GetMaterial();

					IRenderMesh* pRenderMesh = pStatObj->GetRenderMesh();
					bool texturesStreamedIn = true;

					if (pRenderMesh)
					{
						TRenderChunkArray* pChunks = &pRenderMesh->GetChunks();

						if (pChunks)
						{
							for (int i = 0; i < pChunks->size(); i++)
							{
								CRenderChunk& currentChunk = (*pChunks)[i];
								IMaterial* pCurrentSubMtl = pMaterial->GetSafeSubMtl(currentChunk.m_nMatID);
								float fMipFactor = rLightInfo.m_MipFactor * currentChunk.m_texelAreaDensity;

								assert(pCurrentSubMtl);
								pCurrentSubMtl->RequestTexturesLoading(fMipFactor);

								if (currentGroup == nGroupId)
								{
									if (!AreTexturesStreamed(pCurrentSubMtl, fMipFactor))
									{
										texturesStreamedIn = false;
									}
								}
							}
						}
					}

					if (currentGroup == nGroupId)
					{
						rGroup.nTexturesAreStreamedIn = texturesStreamedIn;
					}
				}
			}
		}
	}

	nGroupId++;
}

void CObjManager::ProcessObjectsStreaming(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	IF (!GetCVars()->e_StreamCgf, 0)
		return;

	// this assert is most likely triggered by forgetting to call
	// 3dEngine::SyncProcessStreamingUpdate at the end of the frame, leading to multiple
	// updates, but not starting and/or stopping streaming tasks
	assert(m_bNeedProcessObjectsStreaming_Finish == false);
	if (m_bNeedProcessObjectsStreaming_Finish == true)
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_ERROR, "ProcessObjectsStreaming invoked without a following ProcessObjectsStreaming_Finish, please check your update logic");
	}

	const CCamera& rCamera = passInfo.GetCamera();

	float fTimeStart = GetTimer()->GetAsyncCurTime();

	bool bSyncLoad = Get3DEngine()->IsStatObjSyncLoad();

	if (!m_bCameraPrecacheOverridden)
	{
		SObjManPrecacheCamera& precachePoint = m_vStreamPreCacheCameras[0];

		if (rCamera.GetPosition().GetDistance(precachePoint.vPosition) >= GetFloatCVar(e_StreamCgfGridUpdateDistance))
		{
			Vec3 vOffset = Get3DEngine()->GetAverageCameraMoveDir() * GetFloatCVar(e_StreamPredictionAhead);
			vOffset.z *= .5f;
			precachePoint.vPosition = rCamera.GetPosition() + vOffset;

			ray_hit hit;
			int rayFlags = geom_colltype_player << rwi_colltype_bit | rwi_stop_at_pierceable;
			if (m_pPhysicalWorld->RayWorldIntersection(rCamera.GetPosition(), vOffset,
			                                           ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid, rayFlags, &hit, 1))
			{
				precachePoint.vPosition = hit.pt;
			}

			if (GetFloatCVar(e_StreamPredictionAheadDebug))
				DrawSphere(precachePoint.vPosition, 0.5f);
		}

		if (Distance::Point_AABBSq(precachePoint.vPosition, precachePoint.bbox) > 0.0f)
			precachePoint.bbox = AABB(precachePoint.vPosition, GetCVars()->e_StreamPredictionBoxRadius);
	}

	if (bSyncLoad && Get3DEngine()->IsShadersSyncLoad())
		PrintMessage("Pre-caching render meshes, shaders and textures");
	else if (bSyncLoad)
		PrintMessage("Pre-caching render meshes for camera position");

	CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();

	bool bSyncLoadPoints = m_bCameraPrecacheOverridden || Get3DEngine()->IsContentPrecacheRequested() || bSyncLoad || (GetCVars()->e_StreamCgf == 3) || (GetCVars()->e_StreamCgfDebugHeatMap != 0);
	UpdateObjectsStreamingPriority(bSyncLoadPoints, passInfo);

	// Remove stale precache points

	size_t ppWriteIdx = 0;
	for (size_t ppIdx = 0, ppCount = m_vStreamPreCachePointDefs.size(); ppIdx != ppCount; ++ppIdx)
	{
		SObjManPrecachePoint& pp = m_vStreamPreCachePointDefs[ppIdx];

		if (ppIdx == 0 || currentTime < pp.expireTime)
		{
			m_vStreamPreCachePointDefs[ppWriteIdx] = pp;
			m_vStreamPreCacheCameras[ppWriteIdx] = m_vStreamPreCacheCameras[ppIdx];
			++ppWriteIdx;
		}
	}
	m_vStreamPreCachePointDefs.resize(ppWriteIdx);
	m_vStreamPreCacheCameras.resize(ppWriteIdx);

	m_bCameraPrecacheOverridden = false;

	m_bNeedProcessObjectsStreaming_Finish = true;
	ProcessObjectsStreaming_Impl(bSyncLoad, passInfo);

	// during precache don't run asynchrony and sync directly to ensure the ESYSTEM_EVENT_LEVEL_PRECACHED
	// event is send to activate the renderthread
	// this also applies to the editor, since it calls the render function multiple times and thus invoking the
	// function multiple times without syncing
	if (bSyncLoad || gEnv->IsEditor())
	{
		ProcessObjectsStreaming_Finish();
	}

	if (bSyncLoad)
	{
		float t = GetTimer()->GetAsyncCurTime() - fTimeStart;
		if (t > (1.0f / 15.0f))
			PrintMessage("Finished pre-caching in %.1f sec", t);
	}
}

void CObjManager::ProcessObjectsStreaming_Impl(bool bSyncLoad, const SRenderingPassInfo& passInfo)
{
	ProcessObjectsStreaming_Sort(bSyncLoad, passInfo);
	ProcessObjectsStreaming_Release();
#ifdef OBJMAN_STREAM_STATS
	ProcessObjectsStreaming_Stats(passInfo);
#endif
	ProcessObjectsStreaming_InitLoad(bSyncLoad);
}

void CObjManager::ProcessObjectsStreaming_Sort(bool bSyncLoad, const SRenderingPassInfo& passInfo)
{
	int nNumStreamableObjects = m_arrStreamableObjects.Count();

	static float fLastTime = 0;
	const float fTime = GetTimer()->GetAsyncCurTime();

	// call sort only every 100 ms
	if (nNumStreamableObjects && ((fTime > fLastTime + 0.1f) || bSyncLoad))
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "ProcessObjectsStreaming_Sort");

		SStreamAbleObject* arrStreamableObjects = &m_arrStreamableObjects[0];
		assert(arrStreamableObjects);

		const float fMeshStreamingMaxIImportance = 10.f;

		if (bSyncLoad)
		{
			// just put file offset into importance
			for (int i = 0; i < nNumStreamableObjects; i++)
			{
				SStreamAbleObject& rObj = arrStreamableObjects[i];

				if (!rObj.GetStreamAbleObject()->IsUnloadable())
				{
					rObj.fCurImportance = fMeshStreamingMaxIImportance;
					continue;
				}

				string fileName;
				rObj.GetStreamAbleObject()->GetStreamableName(fileName);
				int nOffset = (int)(GetPak()->GetFileOffsetOnMedia(fileName.c_str()) / 1024);
				rObj.fCurImportance = -(float)nOffset;
			}
		}
		else
		{
			// use data of previous prediction round since current round is not finished yet
			int nRoundId = CObjManager::m_nUpdateStreamingPrioriryRoundId - 1;

			for (int i = 0; i < nNumStreamableObjects; i++)
			{
				SStreamAbleObject& rObj = arrStreamableObjects[i];

				if (!rObj.GetStreamAbleObject()->IsUnloadable())
				{
					rObj.fCurImportance = fMeshStreamingMaxIImportance;
					continue;
				}

				// compute importance of objects for selected nRoundId
				rObj.fCurImportance = -1000.f;

				IStreamable::SInstancePriorityInfo* pInfo = &(rObj.GetStreamAbleObject()->m_arrUpdateStreamingPrioriryRoundInfo[0]);

				for (int nRoundIdx = 0; nRoundIdx < 2; nRoundIdx++)
				{
					if (pInfo[nRoundIdx].nRoundId == nRoundId)
					{
						rObj.fCurImportance = pInfo[nRoundIdx].fMaxImportance;
						if (rObj.GetLastDrawMainFrameId() > (passInfo.GetMainFrameID() - GetCVars()->e_RNTmpDataPoolMaxFrames))
							rObj.fCurImportance += GetFloatCVar(e_StreamCgfVisObjPriority);
						break;
					}
				}
			}
		}

		std::sort(&arrStreamableObjects[0], &arrStreamableObjects[nNumStreamableObjects], CObjManager_Cmp_Streamable_Priority());

		fLastTime = fTime;
	}
}

void CObjManager::ProcessObjectsStreaming_Release()
{
	CRY_PROFILE_REGION(PROFILE_3DENGINE, "ProcessObjectsStreaming_Release");
	int nMemoryUsage = 0;

	int nNumStreamableObjects = m_arrStreamableObjects.Count();
	SStreamAbleObject* arrStreamableObjects = nNumStreamableObjects ? &m_arrStreamableObjects[0] : NULL;

	for (int nObjId = 0; nObjId < nNumStreamableObjects; nObjId++)
	{
		const SStreamAbleObject& rObj = arrStreamableObjects[nObjId];

		nMemoryUsage += rObj.GetStreamableContentMemoryUsage();

		bool bUnload = nMemoryUsage >= GetCVars()->e_StreamCgfPoolSize * 1024 * 1024;

		if (!bUnload && GetCVars()->e_StreamCgfDebug == 4)
			if (rObj.GetStreamAbleObject()->m_arrUpdateStreamingPrioriryRoundInfo[0].nRoundId < (CObjManager::m_nUpdateStreamingPrioriryRoundId - 8))
				bUnload = true;

		if (bUnload && rObj.GetStreamAbleObject()->IsUnloadable())
		{
			if (rObj.GetStreamAbleObject()->m_eStreamingStatus == ecss_Ready)
			{
				m_arrStreamableToRelease.push_back(rObj.GetStreamAbleObject());
			}

			// remove from list if not active for long time
			if (rObj.GetStreamAbleObject()->m_eStreamingStatus == ecss_NotLoaded)
			{
				if (rObj.GetStreamAbleObject()->m_arrUpdateStreamingPrioriryRoundInfo[0].nRoundId < (CObjManager::m_nUpdateStreamingPrioriryRoundId - 8))
				{
					rObj.GetStreamAbleObject()->m_arrUpdateStreamingPrioriryRoundInfo[0].nRoundId = 0;
					m_arrStreamableToDelete.push_back(rObj.GetStreamAbleObject());
				}
			}
		}
	}
	s_nLastStreamingMemoryUsage = nMemoryUsage;
}

void CObjManager::ProcessObjectsStreaming_InitLoad(bool bSyncLoad)
{
	CRY_PROFILE_REGION(PROFILE_3DENGINE, "ProcessObjectsStreaming_InitLoad");

	int nMaxInProgress = GetCVars()->e_StreamCgfMaxTasksInProgress;
	int nMaxToStart = GetCVars()->e_StreamCgfMaxNewTasksPerUpdate;
	int nMaxMemUsage = GetCVars()->e_StreamCgfPoolSize * 1024 * 1024;
	int nNumStreamableObjects = m_arrStreamableObjects.Count();
	SStreamAbleObject* arrStreamableObjects = nNumStreamableObjects ? &m_arrStreamableObjects[0] : NULL;

	int nMemoryUsage = 0;
	int nInProgress = 0;
	int nInProgressMem = 0;
	int nStarted = 0;

	SMeshPoolStatistics stats;
	GetRenderer()->EF_Query(EFQ_GetMeshPoolInfo, stats);
	size_t poolLimit = stats.nPoolSize << 2; // 4 times the pool limit because the rendermesh pool has been reduced

	for (int nObjId = 0; nObjId < nNumStreamableObjects; nObjId++)
	{
		const SStreamAbleObject& rObj = m_arrStreamableObjects[nObjId];
		if (rObj.GetStreamAbleObject()->m_eStreamingStatus == ecss_InProgress)
		{
			nInProgress++;
			nInProgressMem += rObj.GetStreamableContentMemoryUsage();
		}
	}

	for (int nObjId = 0; (nObjId < nNumStreamableObjects) && ((nInProgress < nMaxInProgress && (nInProgressMem < static_cast<int>(poolLimit) || !poolLimit) && nStarted < nMaxToStart) || bSyncLoad); nObjId++)
	{
		const SStreamAbleObject& rObj = arrStreamableObjects[nObjId];
		IStreamable* pStatObj = rObj.GetStreamAbleObject();

		int size = rObj.GetStreamableContentMemoryUsage();
		if (poolLimit && poolLimit <= (size_t)(nInProgressMem + size))
		{
			// Actually the above check is an implicit size limit on CGFs - which is awful because it can lead to meshes never
			// streamed in. This has to change after crysis2 - for now, this will include a white list of meshes that need to be
			// loaded for crysis2 in the prism2 level.
			if ((size_t)poolLimit <= (size_t)size)
			{
				string name;
				pStatObj->GetStreamableName(name);
				CryLogAlways("[WARNING] object '%s' skipped because too large (%d kb (>= %" PRISIZE_T " kb limit))", name.c_str(), size / 1024, poolLimit / 1024);
				continue;
			}

			if (!bSyncLoad)
				continue;
		}

		nMemoryUsage += size;
		if (nMemoryUsage >= nMaxMemUsage)
			break;

		if (rObj.GetStreamAbleObject()->m_eStreamingStatus == ecss_NotLoaded)
		{
			m_arrStreamableToLoad.push_back(rObj.GetStreamAbleObject());
			nInProgressMem += size;
			++nInProgress;
			++nStarted;

			if ((GetCVars()->e_AutoPrecacheCgf == 2) && (nObjId > nNumStreamableObjects / 2))
				break;
		}
	}
}

void CObjManager::ProcessObjectsStreaming_Finish()
{
	if (m_bNeedProcessObjectsStreaming_Finish == false)
		return;

	int nNumStreamableObjects = m_arrStreamableObjects.Count();
	m_bNeedProcessObjectsStreaming_Finish = false;

	CRY_PROFILE_REGION(PROFILE_3DENGINE, "ProcessObjectsStreaming_Finish");
	bool bSyncLoad = Get3DEngine()->IsStatObjSyncLoad();

	{
		// now unload the stat object
		while (!m_arrStreamableToRelease.empty())
		{
			IStreamable* pStatObj = m_arrStreamableToRelease.back();
			m_arrStreamableToRelease.DeleteLast();
			pStatObj->ReleaseStreamableContent();

#ifdef OBJMAN_STREAM_STATS
			if (m_pStreamListener)
				m_pStreamListener->OnUnloadedStreamedObject(pStatObj);
#endif

			if (GetCVars()->e_StreamCgfDebug == 2)
			{
				string sName;
				pStatObj->GetStreamableName(sName);
				PrintMessage("Unloaded: %s", sName.c_str());
			}
		}

		int nSyncObjCounter = 0;

		// start streaming of stat objects
		if (bSyncLoad)
			gEnv->pRenderer->EnableBatchMode(true);

		//const uint nMaxNumberOfSyncStreamingRequests = GetCVars()->e_AutoPrecacheCgfMaxTasks;

		std::vector<IReadStreamPtr> streamsToFinish;

		GetISystem()->GetStreamEngine()->BeginReadGroup(); // Make sure new streaming requests are accumulated

		for (size_t nId = 0; nId < m_arrStreamableToLoad.size(); nId++)
		{
			IStreamable* pStatObj = m_arrStreamableToLoad[nId];

			// If there is a rendermesh pool present, only submit streaming job if
			// the pool size in the renderer is sufficient. NOTE: This is a weak
			// test. Better test would be to actually preallocate the memory here and
			// only submit the streaming job if the memory could be obtained.
			int size = pStatObj->GetStreamableContentMemoryUsage();

			if (GetCVars()->e_StreamCgfDebug == 2)
			{
				string sName;
				pStatObj->GetStreamableName(sName);
				PrintMessage("Loading: %s", sName.c_str());
			}

			pStatObj->StartStreaming(false, NULL);

#ifdef OBJMAN_STREAM_STATS
			if (m_pStreamListener)
				m_pStreamListener->OnRequestedStreamedObject(pStatObj);
#endif
		}

		GetISystem()->GetStreamEngine()->EndReadGroup(); // Make sure new streaming requests are accumulated

		if (bSyncLoad)
			gEnv->pRenderer->EnableBatchMode(false);

		if (bSyncLoad && nSyncObjCounter)
			PrintMessage("Finished synchronous pre-cache of render meshes for %d CGF's", nSyncObjCounter);

		m_arrStreamableToLoad.clear();

		// remove no longer needed objects from list
		while (!m_arrStreamableToDelete.empty())
		{
			IStreamable* pStatObj = m_arrStreamableToDelete.back();
			m_arrStreamableToDelete.DeleteLast();
			SStreamAbleObject stramAbleObject(pStatObj);
			m_arrStreamableObjects.Delete(stramAbleObject);

#ifdef OBJMAN_STREAM_STATS
			if (m_pStreamListener)
				m_pStreamListener->OnDestroyedStreamedObject(pStatObj);
#endif
		}
	}
}

#ifdef OBJMAN_STREAM_STATS
void CObjManager::ProcessObjectsStreaming_Stats(const SRenderingPassInfo& passInfo)
{
	IStreamedObjectListener* pListener = m_pStreamListener;
	if (!pListener)
		return;

	int nNumStreamableObjects = m_arrStreamableObjects.Count();
	SStreamAbleObject* arrStreamableObjects = nNumStreamableObjects ? &m_arrStreamableObjects[0] : NULL;

	int nCurrentFrameId = passInfo.GetMainFrameID();

	void* pBegunUse[512];
	void* pEndUse[512];

	int nBegunUse = 0;
	int nEndUse = 0;

	for (int nObjId = 0; nObjId < nNumStreamableObjects; nObjId++)
	{
		const SStreamAbleObject& rObj = arrStreamableObjects[nObjId];
		IStreamable* pObj = rObj.GetStreamAbleObject();

		if (pObj->m_eStreamingStatus == ecss_Ready)
		{
			int framesSinceLastUse = nCurrentFrameId - pObj->GetLastDrawMainFrameId();

			if (framesSinceLastUse < 2)
			{
				if (!pObj->m_nStatsInUse)
				{
					pObj->m_nStatsInUse = 1;
					pBegunUse[nBegunUse++] = pObj;

					IF_UNLIKELY (nBegunUse == CRY_ARRAY_COUNT(pBegunUse))
					{
						pListener->OnBegunUsingStreamedObjects(pBegunUse, nBegunUse);
						nBegunUse = 0;
					}
				}
			}
			else
			{
				if (pObj->m_nStatsInUse)
				{
					pObj->m_nStatsInUse = 0;
					pEndUse[nEndUse++] = pObj;

					IF_UNLIKELY (nEndUse == CRY_ARRAY_COUNT(pEndUse))
					{
						pListener->OnEndedUsingStreamedObjects(pEndUse, nEndUse);
						nEndUse = 0;
					}
				}
			}
		}
	}

	if (nBegunUse)
		pListener->OnBegunUsingStreamedObjects(pBegunUse, nBegunUse);

	if (nEndUse)
		pListener->OnEndedUsingStreamedObjects(pEndUse, nEndUse);
}
#endif

void CObjManager::GetObjectsStreamingStatus(I3DEngine::SObjectsStreamingStatus& outStatus)
{
	outStatus.nReady = outStatus.nInProgress = outStatus.nTotal = outStatus.nAllocatedBytes = outStatus.nMemRequired = 0;
	outStatus.nMeshPoolSize = GetCVars()->e_StreamCgfPoolSize;

	for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
	{
		CStatObj* pStatObj = *it;
		if (pStatObj->m_bSubObject)
			continue;

		if (pStatObj->m_pLod0)
			continue;

		for (int l = 0; l < MAX_STATOBJ_LODS_NUM; l++)
			if (CStatObj* pLod = (CStatObj*)pStatObj->GetLodObject(l))
				outStatus.nTotal++;
	}

	outStatus.nActive = m_arrStreamableObjects.Count();

	for (int nObjId = 0; nObjId < m_arrStreamableObjects.Count(); nObjId++)
	{
		const SStreamAbleObject& rStreamAbleObject = m_arrStreamableObjects[nObjId];

		if (rStreamAbleObject.GetStreamAbleObject()->m_eStreamingStatus == ecss_Ready)
			outStatus.nReady++;

		if (rStreamAbleObject.GetStreamAbleObject()->m_eStreamingStatus == ecss_InProgress)
			outStatus.nInProgress++;

		if (rStreamAbleObject.GetStreamAbleObject()->m_eStreamingStatus == ecss_Ready)
			outStatus.nAllocatedBytes += rStreamAbleObject.GetStreamableContentMemoryUsage();

		if (rStreamAbleObject.GetStreamAbleObject()->m_arrUpdateStreamingPrioriryRoundInfo[0].nRoundId >= (CObjManager::m_nUpdateStreamingPrioriryRoundId - 4))
			outStatus.nMemRequired += rStreamAbleObject.GetStreamableContentMemoryUsage();
	}
}

void CObjManager::PrecacheStatObjMaterial(IMaterial* pMaterial, const float fEntDistance, IStatObj* pStatObj, bool bFullUpdate, bool bDrawNear)
{
	//  FUNCTION_PROFILER_3DENGINE;

	if (!pMaterial && pStatObj)
		pMaterial = pStatObj->GetMaterial();

	if (!pMaterial)
		return;

	if (pStatObj)
		for (int i = 0; i < pStatObj->GetSubObjectCount(); i++)
		{
			PrecacheStatObjMaterial(pMaterial, fEntDistance, pStatObj->GetSubObject(i)->pStatObj, bFullUpdate, bDrawNear);
		}

	if (CMatInfo* pMatInfo = static_cast<CMatInfo*>(pMaterial))
	{
		CStatObj* pCStatObj = static_cast<CStatObj*>(pStatObj);
		pMatInfo->PrecacheMaterial(fEntDistance, (pStatObj ? pStatObj->GetRenderMesh() : NULL), bFullUpdate, bDrawNear);
	}
}

void CObjManager::PrecacheStatObj(CStatObj* pStatObj, int nLod, const Matrix34A& statObjMatrix, IMaterial* pMaterial, float fImportance, float fEntDistance, bool bFullUpdate, bool bHighPriority)
{
	if (!pStatObj)
		return;

	const int minLod = pStatObj->GetMinUsableLod();
	const int maxLod = (int)pStatObj->m_nMaxUsableLod;
	const int minPrecacheLod = clamp_tpl(nLod - 1, minLod, maxLod);
	const int maxPrecacheLod = clamp_tpl(nLod + 1, minLod, maxLod);

	for (int currentLod = minPrecacheLod; currentLod <= maxPrecacheLod; ++currentLod)
	{
		IStatObj* pLodStatObj = pStatObj->GetLodObject(currentLod, true);
		IMaterial* pLodMaterial = pLodStatObj->GetMaterial();
		PrecacheStatObjMaterial(pMaterial ? pMaterial : pLodMaterial, fEntDistance, pLodStatObj, bFullUpdate, bHighPriority);
		pStatObj->UpdateStreamableComponents(fImportance, statObjMatrix, bFullUpdate, nLod);
	}
}

void CObjManager::UpdateRenderNodeStreamingPriority(IRenderNode* pObj, float fEntDistanceReal, float fImportanceFactor, bool bFullUpdate, const SRenderingPassInfo& passInfo, bool bHighPriority)
{
	//  FUNCTION_PROFILER_3DENGINE;

	if (pObj->m_dwRndFlags & ERF_HIDDEN)
		return;

	if (pObj->m_fWSMaxViewDist < 0.01f)
		return;

	// check cvars
	const EERType nodeType = pObj->GetRenderNodeType();

	const float fEntDistance = max(0.f, fEntDistanceReal - GetFloatCVar(e_StreamPredictionDistanceNear));

	float fImportance = (1.f - (fEntDistance / ((pObj->m_fWSMaxViewDist + GetFloatCVar(e_StreamPredictionDistanceFar))))) * fImportanceFactor;

	if (fImportance < 0)
		return;

	float fObjScale = 1.f;
	AABB objBox(pObj->GetBBox());

	if (!passInfo.RenderEntities() && pObj->GetOwnerEntity())
		return;

	switch (nodeType)
	{
	case eERType_Brush:
		if (!passInfo.RenderBrushes()) return;
		fObjScale = max(0.001f, ((CBrush*)pObj)->CBrush::GetMatrix().GetColumn0().GetLength());
		break;
	case eERType_Vegetation:
		if (!passInfo.RenderVegetation()) return;
		fObjScale = max(0.001f, ((CVegetation*)pObj)->CVegetation::GetScale());
		fObjScale /= max(0.01f, ((CVegetation*)pObj)->CVegetation::GetLodRatioNormalized());
		break;
	case eERType_ParticleEmitter:
		if (!passInfo.RenderParticles()) return;
		break;
	case eERType_Decal:
		if (!passInfo.RenderDecals()) return;
		fObjScale = max(0.001f, ((CDecalRenderNode*)pObj)->CDecalRenderNode::GetMatrix().GetColumn0().GetLength());
		break;
	case eERType_WaterWave:
		if (!passInfo.RenderWaterWaves()) return;
		break;
	case eERType_WaterVolume:
		if (!passInfo.RenderWaterVolumes()) return;
		break;
	case eERType_Road:
		if (!passInfo.RenderRoads()) return;
		fObjScale = max(0.001f, ((CRoadRenderNode*)pObj)->m_serializedData.worldSpaceBBox.GetRadius());
		break;
	case eERType_Light:
		if (!GetCVars()->e_DynamicLights) return;
		break;
	case eERType_FogVolume:
		fObjScale = max(0.001f, ((CFogVolumeRenderNode*)pObj)->CFogVolumeRenderNode::GetMatrix().GetColumn0().GetLength());
		break;
	case eERType_DistanceCloud:
		fObjScale = max(0.001f, pObj->GetBBox().GetRadius());
		break;
#if defined(USE_GEOM_CACHES)
	case eERType_GeomCache:
		if (!passInfo.RenderGeomCaches()) return;
		fObjScale = max(0.001f, ((CGeomCacheRenderNode*)pObj)->CGeomCacheRenderNode::GetMatrix().GetColumn0().GetLength());
#endif
	default:
		if (!passInfo.RenderEntities()) return;
		break;
	}

	int nLod = CObjManager::GetObjectLOD(pObj, fEntDistanceReal);
	IMaterial* pRenderNodeMat = pObj->GetMaterialOverride();

	IRenderNode::SUpdateStreamingPriorityContext ctx;
	ctx.pPassInfo = &passInfo;
	ctx.distance = fEntDistance;
	ctx.importance = fImportance;
	ctx.lod = nLod;
	ctx.bFullUpdate = bFullUpdate;

	if (const auto pTempData = pObj->m_pTempData.load())
	{
		if (GetFloatCVar(e_StreamCgfGridUpdateDistance) != 0.0f || GetFloatCVar(e_StreamPredictionAhead) != 0.0f || GetFloatCVar(e_StreamPredictionMinFarZoneDistance) != 0.0f)
		{
			float fDistanceToCam = sqrt_tpl(Distance::Point_AABBSq(passInfo.GetCamera().GetPosition(), objBox)) * passInfo.GetZoomFactor();

			if (nodeType == eERType_Vegetation && ((CVegetation*)pObj)->m_pInstancingInfo)
			{
				// for instance groups compute distance to the center of bbox
				AABB objBoxS = AABB(objBox.GetCenter() - Vec3(.1f, .1f, .1f), objBox.GetCenter() + Vec3(.1f, .1f, .1f));
				fDistanceToCam = sqrt_tpl(Distance::Point_AABBSq(passInfo.GetCamera().GetPosition(), objBoxS)) * passInfo.GetZoomFactor();
			}

			pTempData->userData.nWantedLod = CObjManager::GetObjectLOD(pObj, fDistanceToCam);
		}
		else
		{
			pTempData->userData.nWantedLod = nLod;
		}
	}

	Matrix34A matParent;
	if (CStatObj* pStatObj = (CStatObj*)pObj->GetEntityStatObj(0, &matParent, false))
	{
		PrecacheStatObj(pStatObj, nLod, matParent, pRenderNodeMat ? pRenderNodeMat : pStatObj->GetMaterial(), fImportance, fEntDistanceReal / fObjScale, bFullUpdate, bHighPriority);
	}
	else if (pRenderNodeMat)
	{
		pRenderNodeMat->PrecacheMaterial(fEntDistance / fObjScale, pObj->GetRenderMesh(nLod), bFullUpdate, bHighPriority);
	}

	// Additional precaching for RenderNode types.
	pObj->UpdateStreamingPriority(ctx);
}
