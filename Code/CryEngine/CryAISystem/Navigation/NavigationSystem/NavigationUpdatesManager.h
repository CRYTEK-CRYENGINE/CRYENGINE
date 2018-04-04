// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryAISystem/NavigationSystem/INavigationUpdatesManager.h>

#include "Navigation/MNM/BoundingVolume.h"

class NavigationSystem;
struct NavigationMesh;

class CMNMUpdatesManager : public INavigationUpdatesManager
{
public:
	enum class EUpdateRequestStatus
	{
		RequestInQueue = 0,
		RequestDelayedAndBuffered,
		RequestIgnoredAndBuffered,
		RequestInvalid,
		Count
	};

	struct TileUpdateRequest
	{
		enum EStateFlags : uint16
		{
			Aborted  = BIT(0),
			Explicit = BIT(1),
		};

		TileUpdateRequest()
			: stateFlags(0) {}

		bool        IsAborted() const  { return (stateFlags& EStateFlags::Aborted) != 0; }
		bool        IsExplicit() const { return (stateFlags& EStateFlags::Explicit) != 0; }

		inline bool operator==(const TileUpdateRequest& other) const
		{
			return (meshID == other.meshID) && (x == other.x) && (y == other.y) && (z == other.z);
		}

		inline bool operator<(const TileUpdateRequest& other) const
		{
			if (meshID != other.meshID)
				return meshID < other.meshID;

			if (x != other.x)
				return x < other.x;

			if (y != other.y)
				return y < other.y;

			return z < other.z;
		}

		NavigationMeshID meshID;
		uint16           x;
		uint16           y;
		uint16           z;
		uint16           stateFlags;
	};

	struct TileUpdateRequestHash
	{
		template<class T>
		inline void hash_combine(size_t& seed, const T& v) const
		{
			std::hash<T> hasher;
			seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}

		size_t operator()(const TileUpdateRequest& request) const
		{
			size_t hash = 0;
			hash_combine<uint32>(hash, request.meshID);
			hash_combine<uint16>(hash, request.x);
			hash_combine<uint16>(hash, request.y);
			hash_combine<uint16>(hash, request.z);
			return hash;
		}
	};

	typedef MNM::BoundingVolume NavigationBoundingVolume;

	CMNMUpdatesManager() = delete;

	CMNMUpdatesManager(NavigationSystem* pNavSystem);

	virtual ~CMNMUpdatesManager() override {}

	virtual void Update() override;

	virtual void EntityChanged(int physicalEntityId, const AABB& aabb) override;
	virtual void WorldChanged(const AABB& aabb) override;

	virtual void RequestGlobalUpdate() override;
	virtual void RequestGlobalUpdateForAgentType(NavigationAgentTypeID agentTypeID) override;

	virtual void EnableRegenerationRequestsExecution() override     { m_bIsRegenerationRequestExecutionEnabled = true; }
	virtual void DisableRegenerationRequestsAndBuffer() override;
	virtual bool AreRegenerationRequestsDisabled() const override   { return m_bIsRegenerationRequestExecutionEnabled; }

	virtual bool WasRegenerationRequestedThisCycle() const override { return m_bWasRegenerationRequestedThisUpdateCycle; }
	virtual void ClearRegenerationRequestedThisCycleFlag() override { m_bWasRegenerationRequestedThisUpdateCycle = false; }

	virtual void EnableUpdatesAfterStabilization() override         { m_bPostponeUpdatesForStabilization = true; }
	virtual void DisableUpdatesAfterStabilization() override        { m_bPostponeUpdatesForStabilization = false; }

	virtual bool HasBufferedRegenerationRequests() const override;
	virtual void ApplyBufferedRegenerationRequests() override;
	virtual void ClearBufferedRegenerationRequests() override;

	//! Request MNM regeneration on a specific AABB for a specific meshID
	//! If MNM regeneration is disabled internally, requests will be stored in a buffer
	//! Return values
	//!   - RequestInQueue: request was successfully validated and is in active execution queue
	//!   - RequestDelayedAndBuffered: request is stored for delayed execution after some time without any changes
	//!   - RequestIgnoredAndBuffered: MNM regeneration is turned off, so request is stored in buffer
	//!	  - RequestInvalid: there was something wrong with the request so it was ignored
	EUpdateRequestStatus     RequestMeshUpdate(NavigationMeshID meshID, const AABB& aabb);
	EUpdateRequestStatus     RequestMeshDifferenceUpdate(NavigationMeshID meshID, const NavigationBoundingVolume& oldVolume, const NavigationBoundingVolume& newVolume);

	void                     Clear();
	void                     OnMeshDestroyed(NavigationMeshID meshID);

	size_t                   GetRequestQueueSize() const { return m_activeUpdateRequestsQueue.size(); }
	bool                     HasUpdateRequests() const   { return !m_activeUpdateRequestsQueue.empty(); }
	const TileUpdateRequest& GetFrontRequest() const     { return m_activeUpdateRequestsQueue.front(); }
	void                     PopFrontRequest()           { m_activeUpdateRequestsQueue.pop_front(); }

	void                     DebugDraw();

private:
	struct EntityUpdate
	{
		AABB aabbOld;
		AABB aabbNew;
	};

	struct MeshUpdateBoundaries
	{
		uint16 minX;
		uint16 maxX;
		uint16 minY;
		uint16 maxY;
		uint16 minZ;
		uint16 maxZ;
	};

	typedef std::deque<TileUpdateRequest>                                                              TileRequestQueue;
	typedef std::unordered_set<TileUpdateRequest, TileUpdateRequestHash>                               TileUpdatesSet;
	typedef std::unordered_map<int, EntityUpdate>                                                      EntityUpdatesMap;

	struct SRequestParams
	{
		EUpdateRequestStatus status;
		bool                 bExplicit;
	};

	void                 RemoveMeshUpdatesFromQueue(TileUpdatesSet& tileUpdatesSet, NavigationMeshID meshID);

	void                 UpdatePostponedChanges();

	SRequestParams        GetRequestParams(bool bIsExplicit);
	SRequestParams        GetRequestParamsAfterStabilization(bool bIsExplicit);

	void                 RequestQueueWorldUpdate(const SRequestParams& requestParams, const AABB& aabb);
	bool                 RequestQueueMeshUpdate(const SRequestParams& requestParams, NavigationMeshID meshID, const AABB& aabb);
	bool                 RequestQueueMeshDifferenceUpdate(const SRequestParams& requestParams, NavigationMeshID meshID, const NavigationBoundingVolume& oldVolume, const NavigationBoundingVolume& newVolume);

	MeshUpdateBoundaries ComputeMeshUpdateBoundaries(const NavigationMesh& mesh, const AABB& aabb);
	MeshUpdateBoundaries ComputeMeshUpdateDifferenceBoundaries(const NavigationMesh& mesh, const AABB& oldVolume, const AABB& newVolume);

	void                 SheduleTileUpdateRequests(const SRequestParams& requestParams, NavigationMeshID meshID, const MeshUpdateBoundaries& boundaries);

	NavigationSystem* m_pNavigationSystem;

	TileRequestQueue  m_activeUpdateRequestsQueue;

	TileUpdatesSet    m_postponedUpdateRequestsSet;
	TileUpdatesSet    m_ignoredUpdateRequestsSet;

	EntityUpdatesMap  m_postponedEntityUpdatesMap;

	bool              m_bIsRegenerationRequestExecutionEnabled;
	bool              m_bWasRegenerationRequestedThisUpdateCycle;
	bool              m_bPostponeUpdatesForStabilization;
	bool              m_bExplicitRegenerationToggle;

	CTimeValue        m_lastUpdateTime;
};
