// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __INavigationSystem_h__
#define __INavigationSystem_h__

#pragma once

#include <CryAISystem/IMNM.h>
#include <CryAISystem/NavigationSystem/NavigationIdTypes.h>
#include <functional>

struct IAIPathAgent;
struct IOffMeshNavigationManager;
struct INavigationUpdatesManager;

#ifdef SW_NAVMESH_USE_GUID
typedef uint64 NavigationMeshGUID;
typedef uint64 NavigationVolumeGUID;
#endif

typedef Functor3<NavigationAgentTypeID, NavigationMeshID, uint32> NavigationMeshChangeCallback;
typedef Functor2wRet<IPhysicalEntity&, uint32&, bool>             NavigationMeshEntityCallback;

struct INavigationSystemUser
{
	virtual ~INavigationSystemUser() {}

	virtual void Reset() = 0;
	virtual void UpdateForSynchronousOrAsynchronousReadingOperation() = 0;
	virtual void UpdateForSynchronousWritingOperations() = 0;
	virtual void CompleteRunningTasks() = 0;
};

namespace MNM
{
struct INavMesh;

namespace TileGenerator
{
struct IExtension;
} // namespace TileGenerator
} // namespace MNM

struct INavigationSystem
{
	enum ENavigationEvent
	{
		MeshReloaded = 0,
		MeshReloadedAfterExporting,
		NavigationCleared,
	};

	struct INavigationSystemListener
	{
		// <interfuscator:shuffle>
		virtual ~INavigationSystemListener(){}
		virtual void OnNavigationEvent(const ENavigationEvent event) = 0;
		// </interfuscator:shuffle>
	};

	enum WorkingState
	{
		Idle = 0,
		Working,
	};

	struct CreateAgentTypeParams
	{
		CreateAgentTypeParams(const Vec3& _voxelSize = Vec3(0.1f), uint16 _radiusVoxelCount = 4,
		                      uint16 _climbableVoxelCount = 4, uint16 _heightVoxelCount = 18,
		                      uint16 _maxWaterDepthVoxelCount = 6)
			: voxelSize(_voxelSize)
			, climbableInclineGradient(1.0f)
			, climbableStepRatio(0.75f)
			, radiusVoxelCount(_radiusVoxelCount)
			, climbableVoxelCount(_climbableVoxelCount)
			, heightVoxelCount(_heightVoxelCount)
			, maxWaterDepthVoxelCount()
		{
		}

		Vec3   voxelSize;

		float  climbableInclineGradient;
		float  climbableStepRatio;

		uint16 radiusVoxelCount;
		uint16 climbableVoxelCount;
		uint16 heightVoxelCount;
		uint16 maxWaterDepthVoxelCount;
	};

	struct CreateMeshParams
	{
		CreateMeshParams(const Vec3& _origin = Vec3(ZERO), const Vec3i& _tileSize = Vec3i(8), const uint32 _tileCount = 1024)
			: origin(_origin)
			, tileSize(_tileSize)
			, tileCount(_tileCount)
		{
		}

		Vec3   origin;
		Vec3i  tileSize;
		uint32 tileCount;
	};

	// <interfuscator:shuffle>
	virtual ~INavigationSystem() {}
	virtual NavigationAgentTypeID CreateAgentType(const char* name, const CreateAgentTypeParams& params) = 0;
	virtual NavigationAgentTypeID GetAgentTypeID(const char* name) const = 0;
	virtual NavigationAgentTypeID GetAgentTypeID(size_t index) const = 0;
	virtual const char*           GetAgentTypeName(NavigationAgentTypeID agentTypeID) const = 0;
	virtual size_t                GetAgentTypeCount() const = 0;

#ifdef SW_NAVMESH_USE_GUID
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, NavigationMeshGUID guid) = 0;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, NavigationMeshID requestedID, NavigationMeshGUID guid) = 0;
#else
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params) = 0;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, NavigationMeshID requestedID) = 0;
#endif
	virtual NavigationMeshID CreateMeshForVolumeAndUpdate(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, const NavigationVolumeID volumeID) = 0;

	virtual void             DestroyMesh(NavigationMeshID meshID) = 0;

	virtual void SetMeshEntityCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshEntityCallback& callback) = 0;
	virtual void AddMeshChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) = 0;
	virtual void RemoveMeshChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) = 0;

#ifdef SW_NAVMESH_USE_GUID
	virtual void SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID, NavigationVolumeGUID guid) = 0;
#else
	virtual void SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID) = 0;
#endif

#ifdef SW_NAVMESH_USE_GUID
	virtual NavigationVolumeID CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeGUID guid) = 0;
#else
	virtual NavigationVolumeID CreateVolume(Vec3* vertices, size_t vertexCount, float height) = 0;
#endif
	virtual NavigationVolumeID CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeID requestedID) = 0;
	virtual void               DestroyVolume(NavigationVolumeID volumeID) = 0;
	virtual void               SetVolume(NavigationVolumeID volumeID, Vec3* vertices, size_t vertexCount, float height) = 0;
	virtual bool               ValidateVolume(NavigationVolumeID volumeID) const = 0;
	virtual NavigationVolumeID GetVolumeID(NavigationMeshID meshID) const = 0;

#ifdef SW_NAVMESH_USE_GUID
	virtual void SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID, NavigationVolumeGUID guid) = 0;
#else
	virtual void SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID) = 0;
#endif

	virtual NavigationMeshID GetEnclosingMeshID(NavigationAgentTypeID agentTypeID, const Vec3& location) const = 0;

	virtual NavigationMeshID GetMeshID(const char* name, NavigationAgentTypeID agentTypeID) const = 0;
	virtual const char*      GetMeshName(NavigationMeshID meshID) const = 0;
	virtual void             SetMeshName(NavigationMeshID meshID, const char* name) = 0;

	virtual WorkingState     GetState() const = 0;

	// MNM regeneration tasks are queued up, but not executed
	virtual void					PauseNavigationUpdate() = 0;
	virtual void					RestartNavigationUpdate() = 0;
	virtual WorkingState     		Update(bool blocking = false) = 0;
	virtual uint32                  GetWorkingQueueSize() const = 0;

	//! deprecated - RequestQueueMeshUpdate(meshID, aabb) should be used instead
	virtual size_t					 QueueMeshUpdate(NavigationMeshID meshID, const AABB& aabb) = 0;

	virtual void					 ProcessQueuedMeshUpdates() = 0;

	virtual void					 Clear() = 0;

	//! ClearAndNotify is used when the listeners need to be notified about the performed clear operation.
	virtual void                  ClearAndNotify() = 0;

	virtual bool                  ReloadConfig() = 0;
	virtual void                  DebugDraw() = 0;
	virtual void                  Reset() = 0;

	
	virtual INavigationUpdatesManager* GetUpdateManager() = 0;

	virtual void                  SetDebugDisplayAgentType(NavigationAgentTypeID agentTypeID) = 0;
	virtual NavigationAgentTypeID GetDebugDisplayAgentType() const = 0;

	virtual bool                  GetClosestPointInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float vrange, float hrange, Vec3* meshLocation, float minIslandArea = 0.f) const = 0;

	virtual bool                  IsLocationValidInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location) const = 0;

	//! A cheap test to see if two points are connected, without the expense of computing a full path between them.
	virtual bool   IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation) const = 0;

	virtual bool   IsLocationContainedWithinTriangleInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float downRange, float upRange) const = 0;
	virtual size_t GetTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const Vec3& location, const AABB& searchAABB, Vec3* centerLocations, size_t maxCenterLocationCount, float minIslandArea = 0.f) const = 0;

	//! Returns all borders (unconnected edges) in the specified AABB.
	//! There are 3 Vec3's per border edge, vert 0, vert 1, and a normal pointing out from the edge.
	//! You can pass NULL for pBorders to return the total number of borders (multiply this by 3 to get the total number of Vec3's you need to pass in).
	virtual size_t GetTriangleBorders(const NavigationMeshID meshID, const AABB& aabb, Vec3* pBorders, size_t maxBorderCount, float minIslandArea = 0.f) const = 0;

	//! Gets triangle centers, and island ids - this is used to compute spawn points for an area.
	virtual size_t GetTriangleInfo(const NavigationMeshID meshID, const AABB& aabb, Vec3* centerLocations, uint32* islandids, size_t max_count, float minIslandArea = 0.f) const = 0;

	//! Returns island id of the triangle at the current position.
	virtual MNM::GlobalIslandID GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location) = 0;

	virtual bool                ReadFromFile(const char* fileName, bool bAfterExporting) = 0;
#if defined(SEG_WORLD)
	virtual bool                SaveToFile(const char* fileName, const AABB& segmentAABB) const = 0;
#else
	virtual bool                SaveToFile(const char* fileName) const = 0;
#endif

	virtual void RegisterListener(INavigationSystemListener* pListener, const char* name = NULL) = 0;
	virtual void UnRegisterListener(INavigationSystemListener* pListener) = 0;

	virtual void RegisterUser(INavigationSystemUser* pExtension, const char* name = NULL) = 0;
	virtual void UnRegisterUser(INavigationSystemUser* pExtension) = 0;

	//! Register editor Navigation Area shape.
	//! \param shapeName Name of the Navigation Area shape.
	//! \param outVolumeId Returns NavigationVolumeID, if there is a volume already loaded from exported data with the same shapeName.
	//! \return true if the area name is successfully registered.
	virtual bool               RegisterArea(const char* shapeName, NavigationVolumeID& outVolumeId) = 0;
	virtual void               UnRegisterArea(const char* shapeName) = 0;
	virtual NavigationVolumeID GetAreaId(const char* shapeName) const = 0;
	virtual void               SetAreaId(const char* shapeName, NavigationVolumeID id) = 0;
	virtual void               UpdateAreaNameForId(const NavigationVolumeID id, const char* newShapeName) = 0;
	//! Remove navigation meshes and volumes which don't have corresponding registered Navigation Areas. This may happen in editor, when the exported level data is older that the saved level.
	virtual void               RemoveLoadedMeshesWithoutRegisteredAreas() = 0;

	virtual void               StartWorldMonitoring() = 0;
	virtual void               StopWorldMonitoring() = 0;

	virtual bool               IsInUse() const = 0;

	virtual void               CalculateAccessibility() = 0;

	virtual MNM::TileID        GetTileIdWhereLocationIsAtForMesh(NavigationMeshID meshID, const Vec3& location) = 0;
	virtual void               GetTileBoundsForMesh(NavigationMeshID meshID, uint32 tileID, AABB& bounds) const = 0;

	virtual MNM::TriangleID    GetTriangleIDWhereLocationIsAtForMesh(const NavigationAgentTypeID agentID, const Vec3& location) = 0;

	//! Get a MNM::INavMesh, which provides an access to the NavMesh data
	virtual const MNM::INavMesh*             GetMNMNavMesh(NavigationMeshID meshID) const = 0;

	virtual const IOffMeshNavigationManager& GetIOffMeshNavigationManager() const = 0;
	virtual IOffMeshNavigationManager&       GetIOffMeshNavigationManager() = 0;

	virtual TileGeneratorExtensionID         RegisterTileGeneratorExtension(MNM::TileGenerator::IExtension& extension) = 0;
	virtual bool                             UnRegisterTileGeneratorExtension(const TileGeneratorExtensionID extensionId) = 0;

	// </interfuscator:shuffle>
};

#endif // __INavigationSystem_h__
