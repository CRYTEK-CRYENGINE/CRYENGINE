// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/INavigationUpdatesManager.h>

#include "Navigation/MNM/BoundingVolume.h"
#include "Navigation/MNM/NavMesh.h"

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
		RequestIgnoredAndBufferedDuringLoad,
		RequestInvalid,
		Count
	};

	struct TileUpdateRequest
	{
		enum class EFlag : uint16
		{
			Aborted      = BIT(0),
			Explicit     = BIT(1),
			MarkupUpdate = BIT(2),
		};

		enum class EState : uint16
		{
			Free,
			Active,
			Postponed,
			Ignored,
			IgnoredDuringLoad,
			Count,
		};

		TileUpdateRequest()
			: flags(0)
		    , state(EState::Free)
			, idx(-1)
		{}

		bool        CheckFlag(EFlag flag) const { return (flags & uint16(flag)) != 0; }
		void        SetFlag(EFlag flag) { flags |= uint16(flag); }
		void        ClearFlag(EFlag flag) { flags &= ~uint16(flag); }

		bool        IsAborted() const  { return CheckFlag(EFlag::Aborted); }
		bool        IsExplicit() const { return CheckFlag(EFlag::Explicit); }

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

		EState           state;
		size_t           idx;

		NavigationMeshID meshID;
		uint16           flags;
		uint16           x;
		uint16           y;
		uint16           z;
	};

	static uint64 TileUpdateRequestKey(NavigationMeshID meshID, uint16 x, uint16 y, uint16 z)
	{
		static_assert((MNM::CNavMesh::x_bits + MNM::CNavMesh::y_bits + MNM::CNavMesh::z_bits) <= 32, "Unexpected TileId size!");
		static_assert(sizeof(NavigationMeshID) <= 4, "Unexpected NavigationMeshID size!");

		const uint64 tileName = uint64(MNM::CNavMesh::ComputeTileName(x, y, z));
		return (uint64(meshID) << 32) | tileName;
	};

	class TileUpdateRequestArray
	{
	public:
		TileUpdateRequestArray();
		TileUpdateRequestArray(const TileUpdateRequestArray& rhs);
		~TileUpdateRequestArray();

		void Init(size_t initialCount);
		void Clear();

		const size_t AllocateTileUpdateRequest();
		void         FreeTileUpdateRequest(const size_t requestIdx);
		const size_t GetRequestCount() const;
		const size_t GetCapacity() const { return m_capacity; }

		TileUpdateRequest&       operator[](size_t requestIdx);
		const TileUpdateRequest& operator[](size_t requestIdx) const;

		void          Swap(TileUpdateRequestArray& other);

	private:
		void          Grow(size_t amount);

		TileUpdateRequest* m_updateRequests;
		size_t         m_count;
		size_t         m_capacity;

		typedef std::vector<size_t> FreeIndexes;
		FreeIndexes m_freeIndexes;
	};

	typedef MNM::BoundingVolume NavigationBoundingVolume;

	CMNMUpdatesManager() = delete;

	CMNMUpdatesManager(NavigationSystem* pNavSystem);

	virtual ~CMNMUpdatesManager() override {}

	virtual void Update(const CTimeValue frameStartTime, const float frameDeltaTime) override;

	virtual void EntityChanged(int physicalEntityId, const AABB& aabb) override;
	virtual void WorldChanged(const AABB& aabb) override;

	virtual void RequestGlobalUpdate() override;
	virtual void RequestGlobalUpdateForAgentType(NavigationAgentTypeID agentTypeID) override;

	virtual bool WasRegenerationRequestedThisCycle() const override { return m_wasRegenerationRequestedThisUpdateCycle; }
	virtual void ClearRegenerationRequestedThisCycleFlag() override { m_wasRegenerationRequestedThisUpdateCycle = false; }

	virtual bool HasBufferedRegenerationRequests() const override;
	virtual void ApplyBufferedRegenerationRequests(bool applyAll) override;
	virtual void ClearBufferedRegenerationRequests() override;

	virtual void SetUpdateMode(const INavigationUpdatesManager::EUpdateMode updateMode) override;
	virtual INavigationUpdatesManager::EUpdateMode GetUpdateMode() const override { return m_updateMode; }

	//! Request MNM regeneration on a specific AABB for a specific meshID
	//! If MNM regeneration is disabled internally, requests will be stored in a buffer
	//! Return values
	//!   - RequestInQueue: request was successfully validated and is in active execution queue
	//!   - RequestDelayedAndBuffered: request is stored for delayed execution after some time without any changes
	//!   - RequestIgnoredAndBuffered: MNM regeneration is turned off, so request is stored in buffer
	//!	  - RequestInvalid: there was something wrong with the request so it was ignored
	EUpdateRequestStatus     RequestMeshUpdate(NavigationMeshID meshID, const AABB& aabb, bool bImmediateUpdate = true, bool bMarkupUpdate = false);
	EUpdateRequestStatus     RequestMeshDifferenceUpdate(NavigationMeshID meshID, const NavigationBoundingVolume& oldVolume, const NavigationBoundingVolume& newVolume);

	void                     Clear();
	void                     OnMeshDestroyed(NavigationMeshID meshID);

	void                     SaveData(CCryFile& file, const uint16 version) const;
	void                     LoadData(CCryFile& file, const uint16 version);

	size_t                   GetRequestQueueSize() const { return m_activeUpdateRequestsQueue.size(); }
	bool                     HasUpdateRequests() const   { return !m_activeUpdateRequestsQueue.empty(); }
	const TileUpdateRequest& GetFrontRequest() const;
	void                     PopFrontRequest();

	void                     DebugDraw();
	void                     DebugDrawMeshTilesState(const NavigationMeshID meshId) const;

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

	typedef std::deque<size_t>                         TileRequestQueue;
	typedef std::vector<size_t>                        TileUpdatesVector;
	typedef std::unordered_map<uint64, size_t>         TileUpdatesMap;
	typedef std::unordered_map<int, EntityUpdate>      EntityUpdatesMap;

	struct SRequestParams
	{
		bool CheckFlag(TileUpdateRequest::EFlag flag) const { return (flags & uint16(flag)) != 0; }
		
		EUpdateRequestStatus status;
		TileUpdateRequest::EState requestState;
		uint16 flags;
	};

	void                 RemoveMeshUpdatesFromVector(TileUpdatesVector& tileUpdatesVector, NavigationMeshID meshID);

	void                 UpdatePostponedChanges();

	SRequestParams        GetRequestParams(bool bIsExplicit);
	SRequestParams        GetRequestParamsAfterStabilization(bool bIsExplicit);

	void                 RequestQueueWorldUpdate(const SRequestParams& requestParams, const AABB& aabb);
	bool                 RequestQueueMeshUpdate(const SRequestParams& requestParams, NavigationMeshID meshID, const AABB& aabb);
	bool                 RequestQueueMeshDifferenceUpdate(const SRequestParams& requestParams, NavigationMeshID meshID, const NavigationBoundingVolume& oldVolume, const NavigationBoundingVolume& newVolume);

	MeshUpdateBoundaries ComputeMeshUpdateBoundaries(const NavigationMesh& mesh, const AABB& aabb);
	MeshUpdateBoundaries ComputeMeshUpdateDifferenceBoundaries(const NavigationMesh& mesh, const AABB& oldVolume, const AABB& newVolume);

	void                 ScheduleTileUpdateRequests(const SRequestParams& requestParams, NavigationMeshID meshID, const MeshUpdateBoundaries& boundaries);

	void SwitchUpdateRequestState(size_t requestId, TileUpdateRequest::EState newState);

	NavigationSystem* m_pNavigationSystem;

	TileUpdateRequestArray m_updateRequests;

	TileUpdatesMap    m_updateRequestsMap;

	TileRequestQueue  m_activeUpdateRequestsQueue;
	TileUpdatesVector m_pospondedUpdateRequests;
	TileUpdatesVector m_ignoredUpdateRequests;
	TileUpdatesVector m_ignoredUpdateRequestsAfterLoad;

	EntityUpdatesMap  m_postponedEntityUpdatesMap;

	// map for storing AABBs of navigation areas in the case when navmesh updates are disabled and we will need to update them later after enabling
	std::unordered_map<NavigationMeshID, AABB> m_pendingOldestAabbsOfChangedMeshes;

	INavigationUpdatesManager::EUpdateMode m_updateMode;

	bool              m_wasRegenerationRequestedThisUpdateCycle;
	bool              m_explicitRegenerationToggle;

	CTimeValue        m_lastUpdateTime;
	CTimeValue        m_frameStartTime;
	float             m_frameDeltaTime;
};

namespace MNM
{
namespace Utils
{
// Helper functions to read/write various navigationId types from file without creating intermediate uint32.
// These functions are currently used only in NavigationUpdatesManager and NavigationSystem. They should be moved to separate file when they were needed somewhere else too.
template<typename TId>
void ReadNavigationIdType(CCryFile& file, TId& outId)
{
	static_assert(sizeof(TId) == sizeof(uint32), "Navigation ID underlying type have changed");
	uint32 id;
	file.ReadType(&id);
	outId = TId(id);
}

template<typename TId>
void WriteNavigationIdType(CCryFile& file, const TId& id)
{
	static_assert(sizeof(TId) == sizeof(uint32), "Navigation ID underlying type have changed");
	const uint32 uid = id;
	file.WriteType<uint32>(&uid);
}
} // namespace Utils
} // namespace MNM