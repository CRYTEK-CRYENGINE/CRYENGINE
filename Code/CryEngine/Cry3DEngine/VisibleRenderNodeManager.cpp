// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "3dEngine.h"

#include <CryEntitySystem/IEntity.h>

//////////////////////////////////////////////////////////////////////////
void SRenderNodeTempData::Free()
{
	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	if (hasValidRenderObjects)
	{
		FreeRenderObjects();
	}

	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
		CRY_ASSERT(!userData.arrPermanentRenderObjects[lod]);

	if (userData.m_pFoliage)
	{
		userData.m_pFoliage->Release();
		userData.m_pFoliage = nullptr;
	}

	userData.pOwnerNode = nullptr;
}

//////////////////////////////////////////////////////////////////////////
CRenderObject* SRenderNodeTempData::GetRenderObject(int lod)
{
	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	CRY_ASSERT(!invalidRenderObjects);

	// Do we have to create a new permanent render object?
	if (userData.arrPermanentRenderObjects[lod] == nullptr)
	{
		userData.arrPermanentRenderObjects[lod] = gEnv->pRenderer->EF_GetObject();
		hasValidRenderObjects = true;
	}

	return userData.arrPermanentRenderObjects[lod];
}

//////////////////////////////////////////////////////////////////////////
CRenderObject* SRenderNodeTempData::RefreshRenderObject(int lod)
{
	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	CRY_ASSERT(!invalidRenderObjects);

	if (userData.arrPermanentRenderObjects[lod] != nullptr)
	{
		gEnv->pRenderer->EF_FreeObject(userData.arrPermanentRenderObjects[lod]);
	}

	userData.arrPermanentRenderObjects[lod] = gEnv->pRenderer->EF_GetObject();

	return userData.arrPermanentRenderObjects[lod];
}

void SRenderNodeTempData::FreeRenderObjects()
{
	if (!hasValidRenderObjects)
		return;

	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	// Release permanent CRenderObject(s)
	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
	{
		if (userData.arrPermanentRenderObjects[lod])
		{
			gEnv->pRenderer->EF_FreeObject(userData.arrPermanentRenderObjects[lod]);
			userData.arrPermanentRenderObjects[lod] = nullptr;
		}
	}

	hasValidRenderObjects = false;
	invalidRenderObjects = false;
}

void SRenderNodeTempData::InvalidateRenderObjectsInstanceData()
{
	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	if (!hasValidRenderObjects)
		return;

	// Invalidate permanent CRenderObject(s) instance-data
	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
	{
		if (userData.arrPermanentRenderObjects[lod])
		{
			userData.arrPermanentRenderObjects[lod]->SetInstanceDataDirty(true);
		}
	}
}

void SRenderNodeTempData::SetClipVolume(IClipVolume* pClipVolume, const Vec3& pos)
{
	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	userData.m_pClipVolume = pClipVolume;
	userData.lastClipVolumePosition = pos;
	userData.bClipVolumeAssigned = true;
}

void SRenderNodeTempData::ResetClipVolume()
{
	DBG_LOCK_TO_THREAD(userData.pOwnerNode);

	userData.m_pClipVolume = nullptr;
	userData.bClipVolumeAssigned = false;
}

//////////////////////////////////////////////////////////////////////////
CVisibleRenderNodesManager::CVisibleRenderNodesManager()
	: m_lastStartUpdateNode(0)
	, m_currentNodesToDelete(0)
	, m_lastUpdateFrame(0)
	, m_firstAddedNode(-1)
{}

CVisibleRenderNodesManager::~CVisibleRenderNodesManager()
{
	ClearAll();
}

SRenderNodeTempData* CVisibleRenderNodesManager::AllocateTempData(int lastSeenFrame) //threadsafe
{
	SRenderNodeTempData* pData = m_pool.New();

	pData->userData.objMat.SetIdentity();

	{
		CryAutoCriticalSectionNoRecursive lock(m_accessLock);
		if (m_firstAddedNode < 0)
			m_firstAddedNode = m_visibleNodes.size();
		m_visibleNodes.push_back(pData);
	}
	return pData;
}

//////////////////////////////////////////////////////////////////////////
bool CVisibleRenderNodesManager::SetLastSeenFrame(SRenderNodeTempData* pTempData, const SRenderingPassInfo& passInfo)
{
	int frame = passInfo.GetFrameID();
	bool bCanRenderThisFrame = true;

	if (passInfo.IsShadowPass())
	{
		pTempData->userData.lastSeenShadowFrame = frame;
	}
	else
	{
		// It is valid to call SetLastSeenFrame multiple times, for example for objects visible through several portals
		// Second call will return false and skip object rendering
		// In case of rendering into multiple cube-map sides multiple full passes with increased frame id must be executed
		// TODO: support multiple general passes in single scene graph traverse (similar to multi-frustum shadow generation)
		// TODO: support omni-directional rendering for single-traverse 360 rendering

		int recursion = passInfo.IsRecursivePass() ? 1 : 0;
		// Only return true if last seen frame is different form the current
		bCanRenderThisFrame = pTempData->userData.lastSeenFrame[recursion] != frame;
		pTempData->userData.lastSeenFrame[recursion] = frame;
	}

	return bCanRenderThisFrame;
}

//////////////////////////////////////////////////////////////////////////
void CVisibleRenderNodesManager::UpdateVisibleNodes(int currentFrame, int maxNodesToCheck)
{
	if (m_lastUpdateFrame == currentFrame) // Fast exit if happens on the same frame
		return;
	m_lastUpdateFrame = currentFrame;

	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	m_currentNodesToDelete++;
	m_currentNodesToDelete = (m_currentNodesToDelete) % MAX_DELETE_BUFFERS; // Always cycle delete buffers.
	for (auto* node : m_toDeleteNodes[m_currentNodesToDelete])
	{
		DBG_LOCK_TO_THREAD(node->userData.pOwnerNode);
		m_pool.Delete(node);
	}

	m_toDeleteNodes[m_currentNodesToDelete].clear();

	{
		// LOCK START
		CryAutoCriticalSectionNoRecursive lock(m_accessLock);

		// Process on new node visible events
		if (m_firstAddedNode >= 0)
		{
			for (size_t i = m_firstAddedNode, num = m_visibleNodes.size(); i < num; ++i)
			{
				SRenderNodeTempData* pTempData = m_visibleNodes[i];

				if (IRenderNode* pOwnerNode = pTempData->userData.pOwnerNode)
				{
					DBG_LOCK_TO_THREAD(pOwnerNode);
					OnRenderNodeVisibilityChange(pOwnerNode, true);
				}
			}
			m_firstAddedNode = -1;
		}

		auto b = m_visibleNodes.begin() + m_lastStartUpdateNode;
		if (b >= m_visibleNodes.end())
		{
			b = m_visibleNodes.begin();
			m_lastStartUpdateNode = 0;
		}

		const auto maxFrames = (uint32)C3DEngine::GetCVars()->e_RNTmpDataPoolMaxFrames;
		for (int i=0; i<maxNodesToCheck && b!=m_visibleNodes.end(); ++i)
		{
			auto pTempData = *b;

			auto lastSeenFrame = std::max(pTempData->userData.lastSeenFrame[0], pTempData->userData.lastSeenFrame[1]);
			lastSeenFrame = std::max(lastSeenFrame, pTempData->userData.lastSeenShadowFrame);
			auto diff = (uint32)std::abs(currentFrame - lastSeenFrame);
			if (diff > maxFrames || !pTempData->userData.pOwnerNode)
			{
				if (IRenderNode* pOwnerNode = pTempData->userData.pOwnerNode)
				{
					DBG_LOCK_TO_THREAD(pOwnerNode);

					// clear reference to use from owning render node.
					OnRenderNodeVisibilityChange(pOwnerNode, false);

					pOwnerNode->m_pTempData = nullptr;
					pTempData->userData.pOwnerNode = nullptr;
				}

				m_toDeleteNodes[m_currentNodesToDelete].push_back(pTempData);

				// Swap and erase
				auto penultimate = std::prev(m_visibleNodes.end());
				if (b == penultimate)
				{
					b = m_visibleNodes.erase(penultimate);
				}
				else
				{
					*b = *penultimate;
					m_visibleNodes.erase(penultimate);
				}
			}
			else
				++b;
		}
		m_lastStartUpdateNode = (int)std::distance(m_visibleNodes.begin(), b);
		// LOCK END
	}
}

//////////////////////////////////////////////////////////////////////////
void CVisibleRenderNodesManager::InvalidateAll()
{
	// LOCK START
	CryAutoCriticalSectionNoRecursive lock(m_accessLock);

	for (auto* pNode : m_visibleNodes)
	{
		if (IRenderNode* pOwnerNode = pNode->userData.pOwnerNode)
		{
			pNode->MarkForAutoDelete();
			pOwnerNode->m_pTempData = nullptr;
		}
	}
	// LOCK END
}

//////////////////////////////////////////////////////////////////////////
void CVisibleRenderNodesManager::ClearAll()
{
	CryAutoCriticalSectionNoRecursive lock(m_accessLock);

	for (auto* node : m_visibleNodes)
	{
		m_pool.Delete(node);
	}
	m_visibleNodes.clear();

	for (auto& nodes : m_toDeleteNodes)
	{
		for (auto* node : nodes)
		{
			m_pool.Delete(node);
		}

		nodes.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CVisibleRenderNodesManager::OnRenderNodeVisibilityChange(IRenderNode* pRenderNode, bool bVisible)
{
	pRenderNode->OnRenderNodeAndEntityVisibilityChanged(bVisible);
}

CVisibleRenderNodesManager::Statistics CVisibleRenderNodesManager::GetStatistics() const
{
	Statistics stats;
	stats.numFree = 0;
	stats.numUsed = m_visibleNodes.size();
	return stats;
}

void CVisibleRenderNodesManager::OnRenderNodeDeleted(IRenderNode* pRenderNode)
{
	SRenderNodeTempData* pNodeTempData = pRenderNode->m_pTempData;

	if (pNodeTempData)
	{
		CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

		CryAutoCriticalSectionNoRecursive lock(m_accessLock);

		auto iter = std::find(m_visibleNodes.begin(), m_visibleNodes.end(), pNodeTempData);
		if (iter != m_visibleNodes.end())
		{
			// Erase by swapping with back
			auto penultimate = std::prev(m_visibleNodes.end());
			*iter = *penultimate;
			m_visibleNodes.erase(penultimate);

			m_toDeleteNodes[m_currentNodesToDelete].push_back(pNodeTempData);
		}

		if (pNodeTempData->userData.pOwnerNode == pRenderNode)
			pNodeTempData->userData.pOwnerNode = nullptr;
	}

	pRenderNode->m_pTempData = nullptr;
}
