// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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
	m_activeUpdateRequestsQueue.clear();

	m_postponedEntityUpdatesMap.clear();
	m_postponedUpdateRequestsSet.clear();
	m_ignoredUpdateRequestsSet.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::Update()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	
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
	if (m_postponedUpdateRequestsSet.size())
	{
		if (queueAndState.status == EUpdateRequestStatus::RequestInQueue)
		{
			TileRequestQueue::iterator it = m_activeUpdateRequestsQueue.begin();
			TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
			for (; it != rear; )
			{
				TileUpdateRequest& task = *it;

				if (m_postponedUpdateRequestsSet.find(task) != m_postponedUpdateRequestsSet.end())
				{
					rear = rear - 1;
					std::swap(task, *rear);
					continue;
				}
				++it;
			}
			if (rear != m_activeUpdateRequestsQueue.end())
			{
				m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
			}
			m_activeUpdateRequestsQueue.insert(m_activeUpdateRequestsQueue.end(), m_postponedUpdateRequestsSet.begin(), m_postponedUpdateRequestsSet.end());

			std::sort(m_activeUpdateRequestsQueue.begin(), m_activeUpdateRequestsQueue.end());
		}
		else
		{
			//m_ignoredUpdateRequestsSet.insert(m_activeUpdateRequestsQueue.end(), m_posponedUpdateRequestsSet.begin(), m_posponedUpdateRequestsSet.end());
		}
		m_postponedUpdateRequestsSet.clear();
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
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	
	for (TileUpdateRequest& task : m_activeUpdateRequestsQueue)
	{
		if (task.meshID == meshID)
			task.stateFlags |= TileUpdateRequest::Aborted;
	}
	
	RemoveMeshUpdatesFromQueue(m_ignoredUpdateRequestsSet, meshID);
	RemoveMeshUpdatesFromQueue(m_postponedUpdateRequestsSet, meshID);
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::RemoveMeshUpdatesFromQueue(TileUpdatesSet& tileUpdatesSet, NavigationMeshID meshID)
{
	TileUpdatesSet::iterator it = tileUpdatesSet.begin();
	while (it != tileUpdatesSet.end())
	{
		if (it->meshID == meshID)
		{
			tileUpdatesSet.erase(it++);
		}
		else
		{
			++it;
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
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

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
CMNMUpdatesManager::EUpdateRequestStatus CMNMUpdatesManager::RequestMeshUpdate(NavigationMeshID meshID, const AABB& aabb)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	SRequestParams requestParams = GetRequestParams(m_bExplicitRegenerationToggle);
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
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	
	m_lastUpdateTime = gEnv->pTimer->GetFrameStartTime();
	
	SRequestParams queueAndState = GetRequestParams(m_bExplicitRegenerationToggle);
	if (!RequestQueueMeshDifferenceUpdate(queueAndState, meshID, oldVolume, newVolume))
	{
		return EUpdateRequestStatus::RequestInvalid;
	}
	return queueAndState.status;
}

//------------------------------------------------------------------------
CMNMUpdatesManager::SRequestParams CMNMUpdatesManager::GetRequestParams(bool bIsExplicit)
{
	SRequestParams params;
	params.bExplicit = bIsExplicit;
	if (bIsExplicit)
	{
		params.status = EUpdateRequestStatus::RequestInQueue;
	}
	else
	{
		if (!m_bIsRegenerationRequestExecutionEnabled)
		{
			params.status = EUpdateRequestStatus::RequestIgnoredAndBuffered;
		}
		else if (m_bPostponeUpdatesForStabilization)
		{
			params.status = EUpdateRequestStatus::RequestDelayedAndBuffered;
		}
		else
		{
			params.status = EUpdateRequestStatus::RequestInQueue;
		}
	}
	return params;
}

//------------------------------------------------------------------------
CMNMUpdatesManager::SRequestParams CMNMUpdatesManager::GetRequestParamsAfterStabilization(bool bIsExplicit)
{
	SRequestParams params;
	params.bExplicit = bIsExplicit;
	if (m_bIsRegenerationRequestExecutionEnabled || bIsExplicit)
	{
		params.status = EUpdateRequestStatus::RequestInQueue;
	}
	else
	{
		params.status = EUpdateRequestStatus::RequestIgnoredAndBuffered;
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

	const float extraH = std::max(paramsGrid.voxelSize.x, paramsGrid.voxelSize.y) * (agentType.settings.radiusVoxelCount + 1);
	const float extraV = paramsGrid.voxelSize.z * (agentType.settings.heightVoxelCount + 1);
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

	const float extraH = std::max(paramsGrid.voxelSize.x, paramsGrid.voxelSize.y) * (agentType.settings.radiusVoxelCount + 1);
	const float extraV = paramsGrid.voxelSize.z * (agentType.settings.heightVoxelCount + 1);
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
	assert(meshID != 0);

#if NAVIGATION_SYSTEM_PC_ONLY
	if (!meshID || !m_pNavigationSystem->m_meshes.validate(meshID))
	{
		return false;
	}

	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

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

	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

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
void CMNMUpdatesManager::SheduleTileUpdateRequests(const SRequestParams& requestParams, NavigationMeshID meshID, const MeshUpdateBoundaries& bounds)
{
	if (requestParams.status == EUpdateRequestStatus::RequestInQueue)
	{
		TileRequestQueue::iterator it = m_activeUpdateRequestsQueue.begin();
		TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
		for (; it != rear; )
		{
			TileUpdateRequest& task = *it;

			if ((task.meshID == meshID) && (task.x >= bounds.minX) && (task.x <= bounds.maxX) &&
				(task.y >= bounds.minY) && (task.y <= bounds.maxY) &&
				(task.z >= bounds.minZ) && (task.z <= bounds.maxZ))
			{
				rear = rear - 1;
				std::swap(task, *rear);

				continue;
			}
			++it;
		}
		
		if (rear != m_activeUpdateRequestsQueue.end())
		{
			m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
		}

		for (uint16 y = bounds.minY; y <= bounds.maxY; ++y)
		{
			for (uint16 x = bounds.minX; x <= bounds.maxX; ++x)
			{
				for (uint16 z = bounds.minZ; z <= bounds.maxZ; ++z)
				{
					TileUpdateRequest task;
					task.meshID = meshID;
					task.stateFlags = requestParams.bExplicit ? TileUpdateRequest::EStateFlags::Explicit : 0;
					task.x = x;
					task.y = y;
					task.z = z;
					m_activeUpdateRequestsQueue.push_back(task);

					TileUpdatesSet::const_iterator ignoredFindIt = m_ignoredUpdateRequestsSet.find(task);
					if (ignoredFindIt != m_ignoredUpdateRequestsSet.end())
					{
						m_ignoredUpdateRequestsSet.erase(ignoredFindIt);
					}
				}
			}
		}
	}
	else if (requestParams.status == EUpdateRequestStatus::RequestDelayedAndBuffered)
	{
		TileRequestQueue::iterator it = m_activeUpdateRequestsQueue.begin();
		TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
		for (; it != rear; )
		{
			TileUpdateRequest& task = *it;

			if ((task.meshID == meshID) && (task.x >= bounds.minX) && (task.x <= bounds.maxX) &&
				(task.y >= bounds.minY) && (task.y <= bounds.maxY) &&
				(task.z >= bounds.minZ) && (task.z <= bounds.maxZ))
			{
				rear = rear - 1;
				std::swap(task, *rear);

				continue;
			}
			++it;
		}
		if (rear != m_activeUpdateRequestsQueue.end())
		{
			m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
		}
		
		for (uint16 y = bounds.minY; y <= bounds.maxY; ++y)
		{
			for (uint16 x = bounds.minX; x <= bounds.maxX; ++x)
			{
				for (uint16 z = bounds.minZ; z <= bounds.maxZ; ++z)
				{
					TileUpdateRequest task;
					task.meshID = meshID;
					task.stateFlags = requestParams.bExplicit ? TileUpdateRequest::EStateFlags::Explicit : 0;
					task.x = x;
					task.y = y;
					task.z = z;

					TileUpdatesSet::const_iterator ignoredFindIt = m_ignoredUpdateRequestsSet.find(task);
					if (ignoredFindIt != m_ignoredUpdateRequestsSet.end())
					{
						m_ignoredUpdateRequestsSet.erase(ignoredFindIt);
					}
					m_postponedUpdateRequestsSet.insert(task);
				}
			}
		}
	}
	else
	{
		for (uint16 y = bounds.minY; y <= bounds.maxY; ++y)
		{
			for (uint16 x = bounds.minX; x <= bounds.maxX; ++x)
			{
				for (uint16 z = bounds.minZ; z <= bounds.maxZ; ++z)
				{
					TileUpdateRequest task;
					task.meshID = meshID;
					task.stateFlags = requestParams.bExplicit ? TileUpdateRequest::EStateFlags::Explicit : 0;
					task.x = x;
					task.y = y;
					task.z = z;

					TileUpdatesSet::const_iterator posponedFindIt = m_postponedUpdateRequestsSet.find(task);
					if (posponedFindIt == m_postponedUpdateRequestsSet.end())
					{
						m_ignoredUpdateRequestsSet.insert(task);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CMNMUpdatesManager::HasBufferedRegenerationRequests() const
{
	return m_ignoredUpdateRequestsSet.size() > 0;
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::ApplyBufferedRegenerationRequests()
{
	TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
	for (auto it = m_activeUpdateRequestsQueue.begin(); it != rear; )
	{
		TileUpdateRequest& task = *it;

		const auto findIt = m_ignoredUpdateRequestsSet.find(task);
		if (findIt != m_ignoredUpdateRequestsSet.end())
		{
			rear = rear - 1;
			std::swap(task, *rear);
			continue;
		}
		++it;
	}

	if (rear != m_activeUpdateRequestsQueue.end())
	{
		m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
	}

	m_activeUpdateRequestsQueue.insert(m_activeUpdateRequestsQueue.end(), m_ignoredUpdateRequestsSet.begin(), m_ignoredUpdateRequestsSet.end());
	std::sort(m_activeUpdateRequestsQueue.begin(), m_activeUpdateRequestsQueue.end());

	m_ignoredUpdateRequestsSet.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::ClearBufferedRegenerationRequests()
{
	m_ignoredUpdateRequestsSet.clear();
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::DisableRegenerationRequestsAndBuffer()
{
	m_bIsRegenerationRequestExecutionEnabled = false;

	TileRequestQueue::iterator rear = m_activeUpdateRequestsQueue.end();
	for (auto it = m_activeUpdateRequestsQueue.begin(); it != rear; )
	{
		TileUpdateRequest& task = *it;

		if (!task.IsExplicit())
		{
			rear = rear - 1;
			std::swap(task, *rear);
			continue;
		}
		++it;
	}

	if (rear != m_activeUpdateRequestsQueue.end())
	{
		m_ignoredUpdateRequestsSet.insert(rear, m_activeUpdateRequestsQueue.end());
		m_activeUpdateRequestsQueue.erase(rear, m_activeUpdateRequestsQueue.end());
	}
}

//------------------------------------------------------------------------
void CMNMUpdatesManager::DebugDraw()
{
	CDebugDrawContext dc;
	
	dc->Draw2dLabel(10.0f, 278.0f, 1.2f, Col_White, false, "Delayed tile update requests: %d", m_postponedUpdateRequestsSet.size());
	dc->Draw2dLabel(10.0f, 256.0f, 1.2f, Col_White, false, "Buffered tile update requests: %d", m_ignoredUpdateRequestsSet.size());
	
	for (const auto& it : m_postponedEntityUpdatesMap)
	{
		const EntityUpdate& entityUpdate = it.second;

		dc->DrawAABB(entityUpdate.aabbOld, IDENTITY, false, Col_Red, eBBD_Faceted);

		if (!entityUpdate.aabbNew.IsReset())
		{
			dc->DrawAABB(entityUpdate.aabbNew, IDENTITY, false, Col_Red, eBBD_Faceted);
		}
	}

	for (const TileUpdateRequest& tileRequest : m_postponedUpdateRequestsSet)
	{
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
