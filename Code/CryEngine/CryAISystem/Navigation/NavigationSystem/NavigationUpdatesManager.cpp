// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationUpdatesManager.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "DebugDrawContext.h"

//------------------------------------------------------------------------
CMNMUpdatesManager::CMNMUpdatesManager(NavigationSystem* pNavSystem)
	: m_pNavigationSystem(pNavSystem)
	, m_bIsRegenerationRequestExecutionEnabled(true)
	, m_bWasRegenerationRequestedThisUpdateCycle(false)
	, m_bPostponeUpdatesForStabilization(false)
	, m_bExplicitRegenerationToggle(false)
	, m_lastUpdateTime(0.0f)
{
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::Clear()
{
	m_updateRequests.Clear();

	m_activeUpdateRequestsQueue.clear();
	m_postponedEntityUpdatesMap.clear();
	
	m_updateRequestsMap.clear();
	m_pospondedUpdateRequests.clear();
	m_ignoredUpdateRequests.clear();

	m_pendingOldestAabbsOfChangedMeshes.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	UpdatePostponedChanges();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::UpdatePostponedChanges()
{
	if (m_bPostponeUpdatesForStabilization)
	{
		if (gEnv->pTimer->GetFrameStartTime().GetDifferenceInSeconds(m_lastUpdateTime) < gAIEnv.CVars.NavmeshStabilizationTimeToUpdate)
		{
			return;
		}
	}

	SRequestParams queueAndState = GetRequestParamsAfterStabilization(m_bExplicitRegenerationToggle);

	//tiles update requests
	if (m_pospondedUpdateRequests.size())
	{
		if (queueAndState.status == EUpdateRequestStatus::RequestInQueue)
		{
			const size_t activeSize = m_activeUpdateRequestsQueue.size();
			for (size_t i = 0; i < m_pospondedUpdateRequests.size(); ++i)
			{
				const size_t requestId = m_pospondedUpdateRequests[i];
				TileUpdateRequest& request = m_updateRequests[requestId];
				CRY_ASSERT(request.state == TileUpdateRequest::EState::Postponed);

				request.idx = -1;
				request.state = TileUpdateRequest::EState::Active;
			}
			
			m_activeUpdateRequestsQueue.insert(m_activeUpdateRequestsQueue.end(), m_pospondedUpdateRequests.begin(), m_pospondedUpdateRequests.end());
			std::sort(m_activeUpdateRequestsQueue.begin(), m_activeUpdateRequestsQueue.end());
		}
		else
		{
			//m_ignoredUpdateRequestsSet.insert(m_activeUpdateRequestsQueue.end(), m_posponedUpdateRequestsSet.begin(), m_posponedUpdateRequestsSet.end());
		}
		m_pospondedUpdateRequests.clear();
	}

	//entity updates
	for (const auto& it : m_postponedEntityUpdatesMap)
	{
		const EntityUpdate& entityUpdate = it.second;

		if (!entityUpdate.aabbNew.IsReset())
		{
			if (((entityUpdate.aabbOld.min - entityUpdate.aabbNew.min).len2() + (entityUpdate.aabbOld.max - entityUpdate.aabbNew.max).len2()) > 0.0f)
			{
				RequestQueueWorldUpdate(queueAndState, entityUpdate.aabbOld);
				RequestQueueWorldUpdate(queueAndState, entityUpdate.aabbNew);
			}
		}
		else
		{
			RequestQueueWorldUpdate(queueAndState, entityUpdate.aabbOld);
		}
	}
	m_postponedEntityUpdatesMap.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::OnMeshDestroyed(NavigationMeshID meshID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	for (size_t requestId : m_activeUpdateRequestsQueue)
	{
		TileUpdateRequest& request = m_updateRequests[requestId];
		if (request.meshID == meshID)
			request.SetFlag(TileUpdateRequest::EFlag::Aborted);
	}
	
	RemoveMeshUpdatesFromVector(m_pospondedUpdateRequests, meshID);
	RemoveMeshUpdatesFromVector(m_ignoredUpdateRequests, meshID);

	m_pendingOldestAabbsOfChangedMeshes.erase(meshID);
}

//------------------------------------------------------------------------
const CMNMUpdatesManager::TileUpdateRequest& CMNMUpdatesManager::GetFrontRequest() const
{
	return m_updateRequests[m_activeUpdateRequestsQueue.front()];
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::PopFrontRequest()
{ 
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	size_t requestId = m_activeUpdateRequestsQueue.front();
	TileUpdateRequest& request = m_updateRequests[requestId];
	
	uint64 key = TileUpdateRequestKey(request.meshID, request.x, request.y, request.z);
	m_updateRequestsMap.erase(key);

	m_updateRequests.FreeTileUpdateRequest(requestId);
	m_activeUpdateRequestsQueue.pop_front();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::RemoveMeshUpdatesFromVector(TileUpdatesVector& tileUpdatesVector, NavigationMeshID meshID)
{
	for (size_t i = 0; i < tileUpdatesVector.size();)
	{
		const size_t requestId = tileUpdatesVector[i];
		const TileUpdateRequest& request = m_updateRequests[requestId];
		if (request.meshID == meshID)
		{
			std::iter_swap(tileUpdatesVector.begin() + i, tileUpdatesVector.end() - 1);
			m_updateRequests[tileUpdatesVector[i]].idx = i;

			m_updateRequests.FreeTileUpdateRequest(requestId);
			tileUpdatesVector.pop_back();
		}
		else
		{
			++i;
		}
	}
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::EntityChanged(int physicalEntityId, const AABB& aabb)
{
#if NAVIGATION_SYSTEM_PC_ONLY
	if (physicalEntityId == -1)
	{
		WorldChanged(aabb);
		return;
	}

	m_lastUpdateTime = gEnv->pTimer->GetFrameStartTime();

	auto it = m_postponedEntityUpdatesMap.find(physicalEntityId);
	if (it != m_postponedEntityUpdatesMap.end())
	{
		it->second.aabbNew = aabb;
	}
	else if(aabb.IsNonZero())
	{
		EntityUpdate entityUpdate;

		entityUpdate.aabbOld = aabb;
		entityUpdate.aabbNew = AABB(AABB::RESET);
		m_postponedEntityUpdatesMap[physicalEntityId] = entityUpdate;
	}
#endif //NAVIGATION_SYSTEM_PC_ONLY
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::WorldChanged(const AABB& aabb)
{
#if NAVIGATION_SYSTEM_PC_ONLY
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_lastUpdateTime = gEnv->pTimer->GetFrameStartTime();

	SRequestParams queueAndState = GetRequestParams(false);
	RequestQueueWorldUpdate(queueAndState, aabb);
#endif //NAVIGATION_SYSTEM_PC_ONLY
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::RequestGlobalUpdate()
{
	m_bExplicitRegenerationToggle = true;
	m_pNavigationSystem->ClearAndNotify();
	m_bExplicitRegenerationToggle = false;
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::RequestGlobalUpdateForAgentType(NavigationAgentTypeID agentTypeID)
{
	if (!agentTypeID || agentTypeID > m_pNavigationSystem->m_agentTypes.size())
		return;
	
	const AgentType& agentType = m_pNavigationSystem->m_agentTypes[agentTypeID - 1];

	m_bExplicitRegenerationToggle = true;
	for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
	{
		const NavigationMeshID meshID = meshInfo.id;
		const NavigationMesh& mesh = m_pNavigationSystem->m_meshes[meshID];
		const NavigationVolumeID boundaryID = mesh.boundary;

		if (boundaryID)
		{
			RequestMeshUpdate(meshID, m_pNavigationSystem->m_worldAABB);
		}
	}
	m_bExplicitRegenerationToggle = false;
}

//------------------------------------------------------------------------
CMNMUpdatesManager::EUpdateRequestStatus CMNMUpdatesManager::RequestMeshUpdate(NavigationMeshID meshID, const AABB& aabb, bool bImmediateUpdate /*= true*/, bool bMarkupUpdate /*= false*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!bImmediateUpdate)
	{
		m_lastUpdateTime = gEnv->pTimer->GetFrameStartTime();
	}

	SRequestParams requestParams = GetRequestParams(m_bExplicitRegenerationToggle);
	requestParams.flags |= bMarkupUpdate ? uint16(TileUpdateRequest::EFlag::MarkupUpdate) : 0;

	if (!RequestQueueMeshUpdate(requestParams, meshID, aabb))
	{
		return EUpdateRequestStatus::RequestInvalid;
	}

	return requestParams.status;
}

//------------------------------------------------------------------------
CMNMUpdatesManager::EUpdateRequestStatus CMNMUpdatesManager::RequestMeshDifferenceUpdate(
	NavigationMeshID meshID,
	const NavigationBoundingVolume& oldVolume,
	const NavigationBoundingVolume& newVolume)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	
	m_lastUpdateTime = gEnv->pTimer->GetFrameStartTime();
	
	SRequestParams queueAndState = GetRequestParams(m_bExplicitRegenerationToggle);
	if (!RequestQueueMeshDifferenceUpdate(queueAndState, meshID, oldVolume, newVolume))
	{
		return EUpdateRequestStatus::RequestInvalid;
	}

	if (queueAndState.status == EUpdateRequestStatus::RequestIgnoredAndBuffered)
	{
		m_pendingOldestAabbsOfChangedMeshes.insert(std::make_pair(meshID, oldVolume.aabb));
	}

	return queueAndState.status;
}

//------------------------------------------------------------------------
CMNMUpdatesManager::SRequestParams CMNMUpdatesManager::GetRequestParams(bool bIsExplicit)
{
	SRequestParams params;
	
	static_assert(sizeof(params.flags) == sizeof(TileUpdateRequest::EFlag), "Invalid type size!");
	params.flags = bIsExplicit ? uint16(TileUpdateRequest::EFlag::Explicit) : 0;

	if (bIsExplicit)
	{
		params.status = EUpdateRequestStatus::RequestInQueue;
		params.requestState = TileUpdateRequest::EState::Active;
	}
	else
	{
		if (!m_bIsRegenerationRequestExecutionEnabled)
		{
			params.status = EUpdateRequestStatus::RequestIgnoredAndBuffered;
			params.requestState = TileUpdateRequest::EState::Ignored;
		}
		else if (m_bPostponeUpdatesForStabilization)
		{
			params.status = EUpdateRequestStatus::RequestDelayedAndBuffered;
			params.requestState = TileUpdateRequest::EState::Postponed;
		}
		else
		{
			params.status = EUpdateRequestStatus::RequestInQueue;
			params.requestState = TileUpdateRequest::EState::Active;
		}
	}
	return params;
}

//------------------------------------------------------------------------
CMNMUpdatesManager::SRequestParams CMNMUpdatesManager::GetRequestParamsAfterStabilization(bool bIsExplicit)
{
	SRequestParams params;
	static_assert(sizeof(params.flags) == sizeof(TileUpdateRequest::EFlag), "Invalid type size!");
	params.flags = bIsExplicit ? uint16(TileUpdateRequest::EFlag::Explicit) : 0;

	if (m_bIsRegenerationRequestExecutionEnabled || bIsExplicit)
	{
		params.status = EUpdateRequestStatus::RequestInQueue;
		params.requestState = TileUpdateRequest::EState::Active;
	}
	else
	{
		params.status = EUpdateRequestStatus::RequestIgnoredAndBuffered;
		params.requestState = TileUpdateRequest::EState::Ignored;
	}
	return params;
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::RequestQueueWorldUpdate(const SRequestParams& queueAndState, const AABB& aabb)
{
	for (const AgentType& agentType : m_pNavigationSystem->m_agentTypes)
	{
		for (const AgentType::MeshInfo& meshInfo : agentType.meshes)
		{
			const NavigationMeshID meshID = meshInfo.id;
			const NavigationMesh& mesh = m_pNavigationSystem->m_meshes[meshID];

			if (mesh.boundary && Overlap::AABB_AABB(aabb, m_pNavigationSystem->m_volumes[mesh.boundary].aabb))
			{
				RequestQueueMeshUpdate(queueAndState, meshID, aabb);
			}
		}
	}
}

//------------------------------------------------------------------------
CMNMUpdatesManager::MeshUpdateBoundaries CMNMUpdatesManager::ComputeMeshUpdateBoundaries(const NavigationMesh& mesh, const AABB& aabb)
{
	const MNM::CNavMesh& navMesh = mesh.navMesh;
	const MNM::CNavMesh::SGridParams& paramsGrid = navMesh.GetGridParams();

	const AABB& boundary = m_pNavigationSystem->m_volumes[mesh.boundary].aabb;
	const AgentType& agentType = m_pNavigationSystem->m_agentTypes[mesh.agentTypeID - 1];

	const float extraH = std::max(paramsGrid.voxelSize.x, paramsGrid.voxelSize.y) * agentType.settings.agent.GetPossibleAffectedSizeH();
	const float extraV = paramsGrid.voxelSize.z * agentType.settings.agent.GetPossibleAffectedSizeV();
	const float extraVM = paramsGrid.voxelSize.z; // tiles above are not directly influenced

	Vec3 bmin(std::max(0.0f, std::max(boundary.min.x, aabb.min.x - extraH) - paramsGrid.origin.x),
		std::max(0.0f, std::max(boundary.min.y, aabb.min.y - extraH) - paramsGrid.origin.y),
		std::max(0.0f, std::max(boundary.min.z, aabb.min.z - extraV) - paramsGrid.origin.z));

	Vec3 bmax(std::max(0.0f, std::min(boundary.max.x, aabb.max.x + extraH) - paramsGrid.origin.x),
		std::max(0.0f, std::min(boundary.max.y, aabb.max.y + extraH) - paramsGrid.origin.y),
		std::max(0.0f, std::min(boundary.max.z, aabb.max.z + extraVM) - paramsGrid.origin.z));

	MeshUpdateBoundaries boundaries;
	boundaries.minX = (uint16)(floor_tpl(bmin.x / (float)paramsGrid.tileSize.x));
	boundaries.maxX = (uint16)(floor_tpl(bmax.x / (float)paramsGrid.tileSize.x));

	boundaries.minY = (uint16)(floor_tpl(bmin.y / (float)paramsGrid.tileSize.y));
	boundaries.maxY = (uint16)(floor_tpl(bmax.y / (float)paramsGrid.tileSize.y));

	boundaries.minZ = (uint16)(floor_tpl(bmin.z / (float)paramsGrid.tileSize.z));
	boundaries.maxZ = (uint16)(floor_tpl(bmax.z / (float)paramsGrid.tileSize.z));
	return boundaries;
}


//------------------------------------------------------------------------
CMNMUpdatesManager::MeshUpdateBoundaries CMNMUpdatesManager::ComputeMeshUpdateDifferenceBoundaries(const NavigationMesh& mesh, const AABB& oldAABB, const AABB& newAABB)
{
	const MNM::CNavMesh& navMesh = mesh.navMesh;

	m_bWasRegenerationRequestedThisUpdateCycle = true;

	const MNM::CNavMesh::SGridParams& paramsGrid = navMesh.GetGridParams();

	AABB aabb = oldAABB;
	aabb.Add(newAABB);

	const AgentType& agentType = m_pNavigationSystem->m_agentTypes[mesh.agentTypeID - 1];

	const float extraH = std::max(paramsGrid.voxelSize.x, paramsGrid.voxelSize.y) * agentType.settings.agent.GetPossibleAffectedSizeH();
	const float extraV = paramsGrid.voxelSize.z * agentType.settings.agent.GetPossibleAffectedSizeV();
	const float extraVM = paramsGrid.voxelSize.z; // tiles above are not directly influenced

	Vec3 bmin(std::max(0.0f, (aabb.min.x - extraH) - paramsGrid.origin.x),
		std::max(0.0f, (aabb.min.y - extraH) - paramsGrid.origin.y),
		std::max(0.0f, (aabb.min.z - extraV) - paramsGrid.origin.z));

	Vec3 bmax(std::max(0.0f, (aabb.max.x + extraH) - paramsGrid.origin.x),
		std::max(0.0f, (aabb.max.y + extraH) - paramsGrid.origin.y),
		std::max(0.0f, (aabb.max.z + extraVM) - paramsGrid.origin.z));

	MeshUpdateBoundaries boundaries;
	boundaries.minX = (uint16)(floor_tpl(bmin.x / (float)paramsGrid.tileSize.x));
	boundaries.maxX = (uint16)(floor_tpl(bmax.x / (float)paramsGrid.tileSize.x));

	boundaries.minY = (uint16)(floor_tpl(bmin.y / (float)paramsGrid.tileSize.y));
	boundaries.maxY = (uint16)(floor_tpl(bmax.y / (float)paramsGrid.tileSize.y));

	boundaries.minZ = (uint16)(floor_tpl(bmin.z / (float)paramsGrid.tileSize.z));
	boundaries.maxZ = (uint16)(floor_tpl(bmax.z / (float)paramsGrid.tileSize.z));
	return boundaries;
}

//------------------------------------------------------------------------
bool CMNMUpdatesManager::RequestQueueMeshUpdate(const SRequestParams& requestParams, NavigationMeshID meshID, const AABB& aabb)
{
	CRY_ASSERT(meshID != 0);

#if NAVIGATION_SYSTEM_PC_ONLY
	if (!meshID || !m_pNavigationSystem->m_meshes.validate(meshID))
	{
		return false;
	}

	CRY_PROFILE_FUNCTION(PROFILE_AI);

	NavigationMesh& mesh = m_pNavigationSystem->m_meshes[meshID];

	if (aabb.IsEmpty() || !mesh.boundary)
		return false;

	m_bWasRegenerationRequestedThisUpdateCycle = true;

	SheduleTileUpdateRequests(requestParams, meshID, ComputeMeshUpdateBoundaries(mesh, aabb));
	return true;
#else //!NAVIGATION_SYSTEM_PC_ONLY
	return false;
#endif //!NAVIGATION_SYSTEM_PC_ONLY
}

//------------------------------------------------------------------------
bool CMNMUpdatesManager::RequestQueueMeshDifferenceUpdate(
	const SRequestParams& queueAndState,
	NavigationMeshID meshID,
	const NavigationBoundingVolume& oldVolume,
	const NavigationBoundingVolume& newVolume)
{
#if NAVIGATION_SYSTEM_PC_ONLY
	// TODO: implement properly by verifying what didn't change
	// since there will be loads of volume-aabb intersection tests,
	// this should be a new job running in a different thread
	// producing an array of all the tiles that need updating
	// which then gets concatenated into m_tileQueue in the main update

	if (!meshID || !m_pNavigationSystem->m_meshes.validate(meshID))
	{
		return false;
	}

	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_bWasRegenerationRequestedThisUpdateCycle = true;

	NavigationMesh& mesh = m_pNavigationSystem->m_meshes[meshID];
	MeshUpdateBoundaries bounds = ComputeMeshUpdateDifferenceBoundaries(mesh, oldVolume.aabb, newVolume.aabb);
	SheduleTileUpdateRequests(queueAndState, meshID, bounds);
	return true;
#else //!NAVIGATION_SYSTEM_PC_ONLY
	return false;
#endif //!NAVIGATION_SYSTEM_PC_ONLY
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::SwitchUpdateRequestState(size_t requestId, TileUpdateRequest::EState newState)
{
	CRY_ASSERT(newState != TileUpdateRequest::EState::Free);
	
	TileUpdateRequest& request = m_updateRequests[requestId];
	if (newState == request.state)
		return;
	
	switch (request.state)
	{
	case TileUpdateRequest::EState::Postponed:
		std::iter_swap(m_pospondedUpdateRequests.begin() + request.idx, m_pospondedUpdateRequests.end() - 1);
		m_updateRequests[m_pospondedUpdateRequests[request.idx]].idx = request.idx;
		m_pospondedUpdateRequests.pop_back();
		break;
	case TileUpdateRequest::EState::Ignored:
		std::iter_swap(m_ignoredUpdateRequests.begin() + request.idx, m_ignoredUpdateRequests.end() - 1);
		m_updateRequests[m_ignoredUpdateRequests[request.idx]].idx = request.idx;
		m_ignoredUpdateRequests.pop_back();
		break;
	case TileUpdateRequest::EState::Active:
		// request was already removed from the active deque
		break;
	}

	request.state = newState;

	switch (newState)
	{
	case TileUpdateRequest::EState::Active:
		request.idx = -1;
		m_activeUpdateRequestsQueue.push_back(requestId);
		break;
	case TileUpdateRequest::EState::Postponed:
		request.idx = m_pospondedUpdateRequests.size();
		m_pospondedUpdateRequests.push_back(requestId);
		break;
	case TileUpdateRequest::EState::Ignored:
		request.idx = m_ignoredUpdateRequests.size();
		m_ignoredUpdateRequests.push_back(requestId);
		break;
	}
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::SheduleTileUpdateRequests(const SRequestParams& requestParams, NavigationMeshID meshID, const MeshUpdateBoundaries& bounds)
{
	if (requestParams.requestState != TileUpdateRequest::EState::Active)
	{
		TileRequestQueue::iterator it = m_activeUpdateRequestsQueue.begin();
		TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
		for (; it != rear; )
		{
			TileUpdateRequest& request = m_updateRequests[*it];

			if ((request.meshID == meshID) && (request.x >= bounds.minX) && (request.x <= bounds.maxX) &&
				(request.y >= bounds.minY) && (request.y <= bounds.maxY) &&
				(request.z >= bounds.minZ) && (request.z <= bounds.maxZ))
			{
				rear = rear - 1;
				std::iter_swap(it, rear);
				continue;
			}
			++it;
		}
		if (rear != m_activeUpdateRequestsQueue.end())
		{
			m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
		}
	}

	// Add new tiles
	for (uint16 y = bounds.minY; y <= bounds.maxY; ++y)
	{
		for (uint16 x = bounds.minX; x <= bounds.maxX; ++x)
		{
			for (uint16 z = bounds.minZ; z <= bounds.maxZ; ++z)
			{
				uint64 key = TileUpdateRequestKey(meshID, x, y, z);
				size_t requestIdx;
				TileUpdateRequest* pTask = nullptr;

				auto iresult = m_updateRequestsMap.insert(TileUpdatesMap::value_type(key, 0));
				if (iresult.second)
				{
					requestIdx = m_updateRequests.AllocateTileUpdateRequest();
					iresult.first->second = requestIdx;
				}
				else
				{
					requestIdx = iresult.first->second;
				}

				TileUpdateRequest& task = m_updateRequests[requestIdx];
				task.ClearFlag(TileUpdateRequest::EFlag::Aborted);
				task.flags |= requestParams.flags;
				task.meshID = meshID;
				task.x = x;
				task.y = y;
				task.z = z;

				SwitchUpdateRequestState(requestIdx, requestParams.requestState);
			}
		}
	}
}

//------------------------------------------------------------------------
bool CMNMUpdatesManager::HasBufferedRegenerationRequests() const
{
	return m_ignoredUpdateRequests.size() > 0;
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::ApplyBufferedRegenerationRequests()
{
	const size_t activeSize = m_activeUpdateRequestsQueue.size();
	for (size_t i = 0; i < m_ignoredUpdateRequests.size(); ++i)
	{
		const size_t requestId = m_ignoredUpdateRequests[i];
		TileUpdateRequest& request = m_updateRequests[requestId];
		request.idx = -1;
		request.state = TileUpdateRequest::EState::Active;
	}

	m_activeUpdateRequestsQueue.insert(m_activeUpdateRequestsQueue.end(), m_ignoredUpdateRequests.begin(), m_ignoredUpdateRequests.end());
	std::sort(m_activeUpdateRequestsQueue.begin(), m_activeUpdateRequestsQueue.end());

	m_ignoredUpdateRequests.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::ClearBufferedRegenerationRequests()
{
	m_ignoredUpdateRequests.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::DisableRegenerationRequestsAndBuffer()
{
	m_bIsRegenerationRequestExecutionEnabled = false;

	TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
	for (auto it = m_activeUpdateRequestsQueue.begin(); it != rear; )
	{
		const size_t requestId = *it;
		TileUpdateRequest& request = m_updateRequests[requestId];

		if (!request.IsExplicit())
		{
			request.idx = m_ignoredUpdateRequests.size();
			m_ignoredUpdateRequests.push_back(requestId);
			
			rear = rear - 1;
			std::iter_swap(it, rear);
			continue;
		}
		++it;
	}

	if (rear != m_activeUpdateRequestsQueue.end())
	{
		m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
	}
}

void CMNMUpdatesManager::EnableRegenerationRequestsExecution(bool updateChangedVolumes)
{
	m_bIsRegenerationRequestExecutionEnabled = true;

	if (updateChangedVolumes)
	{
		for (auto& meshWithAabb : m_pendingOldestAabbsOfChangedMeshes)
		{
			NavigationMeshID meshID = meshWithAabb.first;
			if (!meshID || !m_pNavigationSystem->m_meshes.validate(meshID))
				continue;

			SRequestParams queueAndState = GetRequestParams(m_bExplicitRegenerationToggle);
			NavigationMesh& mesh = m_pNavigationSystem->m_meshes[meshID];

			CRY_ASSERT(m_pNavigationSystem->m_volumes.validate(mesh.boundary));
			const AABB& oldAABB = meshWithAabb.second;
			const AABB& currAABB = m_pNavigationSystem->m_volumes[mesh.boundary].aabb;

			if (oldAABB.min != currAABB.min || oldAABB.max != currAABB.max)
			{
				MeshUpdateBoundaries bounds = ComputeMeshUpdateDifferenceBoundaries(mesh, oldAABB, currAABB);
				SheduleTileUpdateRequests(queueAndState, meshID, bounds);

				m_bWasRegenerationRequestedThisUpdateCycle = true;
			}
		}
	}
	
	m_pendingOldestAabbsOfChangedMeshes.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::DebugDraw()
{
	CRY_ASSERT(m_updateRequests.GetRequestCount() == (m_activeUpdateRequestsQueue.size() + m_pospondedUpdateRequests.size() + m_ignoredUpdateRequests.size()));
	
	CDebugDrawContext dc;
	
	dc->Draw2dLabel(10.0f, 278.0f, 1.2f, Col_White, false, "Delayed tile update requests: %d", m_pospondedUpdateRequests.size());
	dc->Draw2dLabel(10.0f, 256.0f, 1.2f, Col_White, false, "Buffered tile update requests: %d", m_ignoredUpdateRequests.size());
	
	for (const auto& it : m_postponedEntityUpdatesMap)
	{
		const EntityUpdate& entityUpdate = it.second;

		dc->DrawAABB(entityUpdate.aabbOld, IDENTITY, false, Col_Red, eBBD_Faceted);

		if (!entityUpdate.aabbNew.IsReset())
		{
			dc->DrawAABB(entityUpdate.aabbNew, IDENTITY, false, Col_Red, eBBD_Faceted);
		}
	}

	for (size_t requestId : m_pospondedUpdateRequests)
	{
		const TileUpdateRequest& tileRequest = m_updateRequests[requestId];
		
		const NavigationMesh& mesh = m_pNavigationSystem->m_meshes[tileRequest.meshID];
		const MNM::CNavMesh::SGridParams& paramsGrid = mesh.navMesh.GetGridParams();
		const AgentType& agentType = m_pNavigationSystem->m_agentTypes[mesh.agentTypeID - 1];

		AABB aabb(AABB::RESET);
		aabb.Add(Vec3(float(tileRequest.x * paramsGrid.tileSize.x), float(tileRequest.y * paramsGrid.tileSize.y), float(tileRequest.z * paramsGrid.tileSize.z)));
		aabb.Add(Vec3(float((tileRequest.x + 1)* paramsGrid.tileSize.x), float((tileRequest.y + 1) * paramsGrid.tileSize.y), float((tileRequest.z + 1) * paramsGrid.tileSize.z)));
		aabb.Move(paramsGrid.origin);
		
		dc->DrawAABB(aabb, IDENTITY, false, Col_Red, eBBD_Faceted);
	}
}

//////////////////////////////////////////////////////////////////////////
CMNMUpdatesManager::TileUpdateRequestArray::TileUpdateRequestArray()
	: m_updateRequests(nullptr)
	, m_count(0)
	, m_capacity(0)
{
}

CMNMUpdatesManager::TileUpdateRequestArray::TileUpdateRequestArray(const TileUpdateRequestArray& rhs)
	: m_updateRequests(nullptr)
	, m_count(0)
	, m_capacity(0)
{
	// if this assert() triggers, then we must completely re-think the use-cases in which copy-construction can occur
	CRY_ASSERT(rhs.m_updateRequests == nullptr);
}

CMNMUpdatesManager::TileUpdateRequestArray::~TileUpdateRequestArray()
{
	//for (size_t i = 0; i < m_capacity; ++i)
	//	m_updateRequests[i].tile.Destroy();

	delete[] m_updateRequests;
}

void CMNMUpdatesManager::TileUpdateRequestArray::Grow(size_t amount)
{
	const size_t oldCapacity = m_capacity;
	m_capacity += amount;

	TileUpdateRequest* requests = new TileUpdateRequest[m_capacity];

	if (oldCapacity)
		memcpy(requests, m_updateRequests, oldCapacity * sizeof(TileUpdateRequest));

	std::swap(m_updateRequests, requests);

	delete[] requests;
}

void CMNMUpdatesManager::TileUpdateRequestArray::Init(size_t initialCount)
{
	CRY_ASSERT(m_capacity == 0);
	CRY_ASSERT(m_updateRequests == nullptr);
	Grow(initialCount);
}

void CMNMUpdatesManager::TileUpdateRequestArray::Clear()
{
	delete[] m_updateRequests;
	m_updateRequests = nullptr;

	m_count = 0;
	m_capacity = 0;
	m_freeIndexes.clear();
}

const size_t CMNMUpdatesManager::TileUpdateRequestArray::AllocateTileUpdateRequest()
{
	CRY_ASSERT(m_count <= m_capacity);

	++m_count;

	size_t idx;
	if (m_freeIndexes.empty())
	{
		idx = m_count - 1;

		if (m_count > m_capacity)
			Grow(std::max<size_t>(4, m_capacity >> 1));
	}
	else
	{
		idx = m_freeIndexes.back();
		m_freeIndexes.pop_back();
	}

	return idx;
}

void CMNMUpdatesManager::TileUpdateRequestArray::FreeTileUpdateRequest(const size_t requestIdx)
{
	CRY_ASSERT(requestIdx >= 0);
	CRY_ASSERT(requestIdx < m_capacity);

	m_updateRequests[requestIdx].state = TileUpdateRequest::EState::Free;
	m_updateRequests[requestIdx].idx = -1;

	--m_count;
	m_freeIndexes.push_back(requestIdx);
}

const size_t CMNMUpdatesManager::TileUpdateRequestArray::GetRequestCount() const
{
	return m_count;
}

CMNMUpdatesManager::TileUpdateRequest& CMNMUpdatesManager::TileUpdateRequestArray::operator[](size_t requestIdx)
{
	CRY_ASSERT(requestIdx < m_capacity);
	return m_updateRequests[requestIdx];
}

const CMNMUpdatesManager::TileUpdateRequest& CMNMUpdatesManager::TileUpdateRequestArray::operator[](size_t requestIdx) const
{
	CRY_ASSERT(requestIdx < m_capacity);
#ifdef TILE_CONTAINER_ARRAY_STRICT_ACCESS_CHECKS
	assert(!stl::find(m_freeIndexes, index));
	assert(index < m_freeIndexes.size() + m_tileCount);
#endif
	return m_updateRequests[requestIdx];
}

void CMNMUpdatesManager::TileUpdateRequestArray::Swap(TileUpdateRequestArray& other)
{
	std::swap(m_updateRequests, other.m_updateRequests);
	std::swap(m_count, other.m_count);
	std::swap(m_capacity, other.m_capacity);
	m_freeIndexes.swap(other.m_freeIndexes);
}
