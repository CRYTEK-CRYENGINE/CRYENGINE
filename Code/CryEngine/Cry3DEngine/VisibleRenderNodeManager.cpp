// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "3dEngine.h"

#include <CryEntitySystem/IEntity.h>

//////////////////////////////////////////////////////////////////////////
void SRenderNodeTempData::Free()
{
	if (hasValidRenderObjects)
	{
		FreeRenderObjects();
	}

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
	CRY_ASSERT(!invalidRenderObjects);

	// Object creation must be locked because CheckCreateRenderObject can be called on same node from different threads
	arrPermanentObjectLock[lod].WLock();

	// Do we have to create a new permanent render object?
	if (userData.arrPermanentRenderObjects[lod] == nullptr)
	{
		userData.arrPermanentRenderObjects[lod] = gEnv->pRenderer->EF_GetObject();
		hasValidRenderObjects = true;
	}

	CRenderObject* pRenderObject = userData.arrPermanentRenderObjects[lod];
	arrPermanentObjectLock[lod].WUnlock();

	return pRenderObject;
}

void SRenderNodeTempData::FreeRenderObjects()
{
	CRY_ASSERT(hasValidRenderObjects);

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
	if (!hasValidRenderObjects)
		return;

	// Release permanent CRenderObject(s)
	for (int lod = 0; lod < MAX_STATOBJ_LODS_NUM; ++lod)
	{
		// Object creation must be locked because CheckCreateRenderObject can be called on same node from different threads
		arrPermanentObjectLock[lod].RLock();

		if (userData.arrPermanentRenderObjects[lod])
		{
			userData.arrPermanentRenderObjects[lod]->SetInstanceDataDirty();
		}

		arrPermanentObjectLock[lod].RUnlock();
	}
}

void SRenderNodeTempData::SetClipVolume(IClipVolume* pClipVolume, const Vec3& pos)
{
	userData.m_pClipVolume = pClipVolume;
	userData.lastClipVolumePosition = pos;
	userData.bClipVolumeAssigned = true;
}

void SRenderNodeTempData::ResetClipVolume()
{
	userData.m_pClipVolume = nullptr;
	userData.bClipVolumeAssigned = false;
}

//////////////////////////////////////////////////////////////////////////
CVisibleRenderNodesManager::CVisibleRenderNodesManager()
	: m_lastStartUpdateNode(0)
	, m_currentNodesToDelete(0)
	, m_lastUpdateFrame(0)
	, m_firstAddedNode(-1)
{

}

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
				if (pTempData->userData.pOwnerNode->m_pTempData.load() == pTempData)
				{
					OnRenderNodeVisibilityChange(pTempData->userData.pOwnerNode, true);
				}
			}
			m_firstAddedNode = -1;
		}

		int numNodes = m_visibleNodes.size();
		int start = m_lastStartUpdateNode;
		if (start >= numNodes)
		{
			start = 0;
			m_lastStartUpdateNode = 0;
		}
		int end = start + maxNodesToCheck;
		if (end > numNodes)
			end = numNodes;

		int lastNode = numNodes - 1;

		int maxFrames = (uint32)C3DEngine::GetCVars()->e_RNTmpDataPoolMaxFrames;
		int numItemsToDelete = 0;
		for (int i = start; i < end && i <= lastNode; ++i)
		{
			SRenderNodeTempData* pTempData = m_visibleNodes[i];

			int lastSeenFrame = std::max(pTempData->userData.lastSeenFrame[0], pTempData->userData.lastSeenFrame[1]);
			lastSeenFrame = std::max(lastSeenFrame, pTempData->userData.lastSeenShadowFrame);
			int diff = std::abs(currentFrame - lastSeenFrame);
			if (diff > maxFrames)
			{
				// clear reference to use from owning render node.
				SRenderNodeTempData* pOldData = pTempData;
				if (pTempData->userData.pOwnerNode->m_pTempData.compare_exchange_strong(pOldData, nullptr))
				{
					OnRenderNodeVisibilityChange(pTempData->userData.pOwnerNode, false);
				}

				m_toDeleteNodes[m_currentNodesToDelete].push_back(pTempData);
				if (i < lastNode)
				{
					// move item from the end of the array to this spot, and repeat check on same index.
					m_visibleNodes[i] = m_visibleNodes[lastNode];
					lastNode--;
					i--;
				}
				numItemsToDelete++;
			}
			m_lastStartUpdateNode = i + 1;
		}
		// delete not relevant items at the end.
		if (numItemsToDelete > 0)
		{
			m_visibleNodes.resize(m_visibleNodes.size() - numItemsToDelete);
		}
		// LOCK END
	}
	return;
}

//////////////////////////////////////////////////////////////////////////
void CVisibleRenderNodesManager::InvalidateAll()
{
	// LOCK START
	CryAutoCriticalSectionNoRecursive lock(m_accessLock);

	for (auto* node : m_visibleNodes)
	{
		SRenderNodeTempData* oldnode = node;
		if (node->userData.pOwnerNode->m_pTempData.compare_exchange_strong(oldnode, nullptr))
		{
			node->MarkForAutoDelete();
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
	//if (!passInfo.IsCachedShadowPass())
	{
		pRenderNode->OnRenderNodeVisible(bVisible);

		if (pRenderNode->GetOwnerEntity() && (pRenderNode->GetRndFlags() & ERF_ENABLE_ENTITY_RENDER_CALLBACK))
		{
			// When render node becomes visible notify our owner render node that it is now visible.
			pRenderNode->GetOwnerEntity()->OnRenderNodeVisibilityChange(bVisible);
		}
	}
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
	// Remove pointer atomically
	SRenderNodeTempData* pNodeTempData = pRenderNode->m_pTempData.exchange(nullptr);

	if (pNodeTempData)
	{
		LOADING_TIME_PROFILE_SECTION;

		auto iter = std::partition(m_visibleNodes.begin(), m_visibleNodes.end(),
			[pNodeTempData](SRenderNodeTempData* pTempData) { return pTempData != pNodeTempData; });

		// Erase all duplicates, but only add it once (N:1 reduction)
		if (iter != m_visibleNodes.end())
		{
			m_visibleNodes.erase(iter, m_visibleNodes.end());
			m_toDeleteNodes[m_currentNodesToDelete].push_back(pNodeTempData);
		}
	}
}
