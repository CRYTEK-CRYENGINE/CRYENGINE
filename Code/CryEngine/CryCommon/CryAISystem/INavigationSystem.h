// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INavigationSystem_h__
#define __INavigationSystem_h__

#pragma once

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IMNM.h>
#include <CryAISystem/NavigationSystem/MNMNavMesh.h>
#include <CryAISystem/NavigationSystem/NavigationIdTypes.h>
#include <functional>

struct IOffMeshNavigationManager;
struct INavigationUpdatesManager;
struct INavMeshQueryFilter;
struct SSnapToNavMeshRulesInfo;

#ifdef SW_NAVMESH_USE_GUID
typedef uint64 NavigationMeshGUID;
typedef uint64 NavigationVolumeGUID;
#endif

typedef Functor3<NavigationAgentTypeID, NavigationMeshID, uint32> NavigationMeshChangeCallback;
typedef Functor2wRet<IPhysicalEntity&, uint32&, bool>             NavigationMeshEntityCallback;
typedef uint32                                                    NavigationAgentTypesMask;

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
struct IAnnotationsLibrary;
struct SMarkupVolumeParams;

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
	virtual NavigationAgentTypeID           CreateAgentType(const char* name, const CreateAgentTypeParams& params) = 0;
	virtual NavigationAgentTypeID           GetAgentTypeID(const char* name) const = 0;
	virtual NavigationAgentTypeID           GetAgentTypeID(size_t index) const = 0;
	virtual const char*                     GetAgentTypeName(NavigationAgentTypeID agentTypeID) const = 0;
	virtual size_t                          GetAgentTypeCount() const = 0;

	virtual const MNM::IAnnotationsLibrary& GetAnnotationLibrary() const = 0;
	virtual MNM::AreaAnnotation             GetAreaTypeAnnotation(const NavigationAreaTypeID areaTypeID) const = 0;

#ifdef SW_NAVMESH_USE_GUID
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, NavigationMeshGUID guid) = 0;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, NavigationMeshID requestedID, NavigationMeshGUID guid) = 0;
#else
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params) = 0;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, NavigationMeshID requestedID) = 0;
#endif
	virtual NavigationMeshID CreateMeshForVolumeAndUpdate(const char* name, NavigationAgentTypeID agentTypeID, const CreateMeshParams& params, const NavigationVolumeID volumeID) = 0;

	virtual void DestroyMesh(NavigationMeshID meshID) = 0;

	virtual void SetMeshEntityCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshEntityCallback& callback) = 0;
	virtual void AddMeshChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) = 0;
	virtual void RemoveMeshChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) = 0;
	virtual void AddMeshAnnotationChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) = 0;
	virtual void RemoveMeshAnnotationChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) = 0;

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

	virtual NavigationVolumeID CreateMarkupVolume(NavigationVolumeID requestedID) = 0;
	virtual void               SetMarkupVolume(const NavigationAgentTypesMask enabledAgentTypesMask, const Vec3* vertices, size_t vertexCount, const NavigationVolumeID volumeID, const MNM::SMarkupVolumeParams& params) = 0;
	virtual void               DestroyMarkupVolume(NavigationVolumeID volumeID) = 0;
	virtual void               SetAnnotationForMarkupTriangles(NavigationVolumeID volumeID, const MNM::AreaAnnotation& areaAnnotation) = 0;

#ifdef SW_NAVMESH_USE_GUID
	virtual void SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID, NavigationVolumeGUID guid) = 0;
#else
	virtual void SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID) = 0;
#endif

	virtual NavigationMeshID GetMeshID(const char* name, NavigationAgentTypeID agentTypeID) const = 0;
	virtual const char*  GetMeshName(NavigationMeshID meshID) const = 0;
	virtual void         SetMeshName(NavigationMeshID meshID, const char* name) = 0;

	virtual WorkingState GetState() const = 0;

	// MNM regeneration tasks are queued up, but not executed
	virtual void         PauseNavigationUpdate() = 0;
	virtual void         RestartNavigationUpdate() = 0;
	virtual WorkingState Update(bool blocking = false) = 0;
	virtual uint32       GetWorkingQueueSize() const = 0;

	//! deprecated - RequestQueueMeshUpdate(meshID, aabb) should be used instead
	virtual size_t QueueMeshUpdate(NavigationMeshID meshID, const AABB& aabb) = 0;

	virtual void   ProcessQueuedMeshUpdates() = 0;

	virtual void   Clear() = 0;

	//! ClearAndNotify is used when the listeners need to be notified about the performed clear operation.
	virtual void                       ClearAndNotify() = 0;

	virtual bool                       ReloadConfig() = 0;
	virtual void                       DebugDraw() = 0;
	virtual void                       Reset() = 0;

	virtual INavigationUpdatesManager* GetUpdateManager() = 0;

	virtual void                       SetDebugDisplayAgentType(NavigationAgentTypeID agentTypeID) = 0;
	virtual NavigationAgentTypeID      GetDebugDisplayAgentType() const = 0;

	virtual bool                       ReadFromFile(const char* fileName, bool bAfterExporting) = 0;
	virtual bool                       SaveToFile(const char* fileName) const = 0;

	virtual void                       RegisterListener(INavigationSystemListener* pListener, const char* name = NULL) = 0;
	virtual void                       UnRegisterListener(INavigationSystemListener* pListener) = 0;

	virtual void                       RegisterUser(INavigationSystemUser* pExtension, const char* name = NULL) = 0;
	virtual void                       UnRegisterUser(INavigationSystemUser* pExtension) = 0;

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

	virtual bool               RegisterEntityMarkups(const IEntity& owningEntity, const char** shapeNamesArray, const size_t count, NavigationVolumeID* pOutIdsArray) = 0;
	virtual void               UnregisterEntityMarkups(const IEntity& owningEntity) = 0;

	virtual void               StartWorldMonitoring() = 0;
	virtual void               StopWorldMonitoring() = 0;

	virtual bool               IsInUse() const = 0;

	virtual void               CalculateAccessibility() = 0;


	//! NavMesh queries functions

	virtual MNM::TileID        GetTileIdWhereLocationIsAtForMesh(NavigationMeshID meshID, const Vec3& location, const INavMeshQueryFilter* pFilter) = 0;
	virtual void               GetTileBoundsForMesh(NavigationMeshID meshID, uint32 tileID, AABB& bounds) const = 0;

	//! Get a MNM::INavMesh, which provides an access to the NavMesh data
	virtual const MNM::INavMesh*             GetMNMNavMesh(NavigationMeshID meshID) const = 0;

	virtual const IOffMeshNavigationManager& GetIOffMeshNavigationManager() const = 0;
	virtual IOffMeshNavigationManager&       GetIOffMeshNavigationManager() = 0;

	virtual TileGeneratorExtensionID         RegisterTileGeneratorExtension(MNM::TileGenerator::IExtension& extension) = 0;
	virtual bool                             UnRegisterTileGeneratorExtension(const TileGeneratorExtensionID extensionId) = 0;

	virtual NavigationMeshID                 GetEnclosingMeshID(NavigationAgentTypeID agentTypeID, const Vec3& location) const = 0;

	virtual bool                             GetClosestPointInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float vrange, float hrange, Vec3* meshLocation, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const = 0;

	//! A cheap test to see if two points are connected, without the expense of computing a full path between them.
	virtual bool   IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation, const INavMeshQueryFilter* pFilter) const = 0;

	virtual bool   IsLocationValidInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter, float downRange = 1.0f, float upRange = 1.0f) const = 0;
	virtual size_t GetTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const Vec3& location, const AABB& searchAABB, Vec3* centerLocations, size_t maxCenterLocationCount, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const = 0;

	//! Performs raycast on navmesh.
	//! \param agentTypeID navigation agent type Id
	//! \param startPos Starting position of the ray
	//! \param endPos End position of the ray
	//! \param pOutHit Optional pointer for a return value of additional information about the hit. This structure is valid only when the hit is reported.
	//! \return Returns true if the ray is hits navmesh boundary before end position.
	virtual bool NavMeshTestRaycastHit(NavigationAgentTypeID agentTypeID, const Vec3& startPos, const Vec3& endPos, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const = 0;

	//! Returns all borders (unconnected edges) in the specified AABB.
	//! There are 3 Vec3's per border edge, vert 0, vert 1, and a normal pointing out from the edge.
	//! You can pass NULL for pBorders to return the total number of borders (multiply this by 3 to get the total number of Vec3's you need to pass in).
	virtual size_t GetTriangleBorders(const NavigationMeshID meshID, const AABB& aabb, Vec3* pBorders, size_t maxBorderCount, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const = 0;

	//! Gets triangle centers, and island ids - this is used to compute spawn points for an area.
	virtual size_t GetTriangleInfo(const NavigationMeshID meshID, const AABB& aabb, Vec3* centerLocations, uint32* islandids, size_t max_count, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const = 0;

	//! Returns island id of the triangle at the current position.
	virtual MNM::GlobalIslandID GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location) = 0;

	virtual MNM::TriangleID     GetTriangleIDWhereLocationIsAtForMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter) = 0;
	
	//! Snaps provided position to NavMesh`
	//! \param agentTypeID navigation agent type Id
	//! \param position Position to be snapped to NavMesh
	//! \param pFilter Pointer to navigation query filter. Can be null.
	//! \param snappingRules Snapping rules structure.
	//! \param outSnappedPosition Returned snapped position, valid only when the function returned true.
	//! \param pOutTriangleId Optional pointer for a triangle Id, which contains snapped position.
	//! \return Returns true if the position was successfully snapped to NavMesh, false otherwise.
	virtual bool                SnapToNavMesh(const NavigationAgentTypeID agentTypeID, const Vec3& position, const INavMeshQueryFilter* pFilter, const SSnapToNavMeshRulesInfo& snappingRules, Vec3& outSnappedPosition, MNM::TriangleID* pOutTriangleId) const = 0;

	// </interfuscator:shuffle>
};

#endif // __INavigationSystem_h__
