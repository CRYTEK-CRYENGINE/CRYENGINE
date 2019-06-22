// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INavigationSystem_h__
#define __INavigationSystem_h__

#pragma once

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/NavigationSystem/MNMNavMesh.h>
#include <CryAISystem/NavigationSystem/NavigationIdTypes.h>
#include <functional>

struct IOffMeshNavigationManager;
struct INavigationUpdatesManager;
struct INavMeshQueryFilter;

namespace MNM
{
	struct INavMeshQueryManager;
}

#ifdef SW_NAVMESH_USE_GUID
typedef uint64 NavigationMeshGUID;
typedef uint64 NavigationVolumeGUID;
#endif

typedef Functor3<NavigationAgentTypeID, NavigationMeshID, MNM::TileID> NavigationMeshChangeCallback;
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
	enum class ENavigationEvent
	{
		MeshReloaded,               // Sent after the NavMeshes are loaded
		MeshReloadedAfterExporting, // Sent after the NavMeshes are loaded when exporting the level
		NavigationCleared,          // Sent after Navigation System is reset and cleared
		WorkingStateSetToIdle,      // Sent after NavMesh updating is finished
		WorkingStateSetToWorking,   // Sent before NavMesh updating starts
	};

	struct INavigationSystemListener
	{
		// <interfuscator:shuffle>
		virtual ~INavigationSystemListener(){}
		virtual void OnNavigationEvent(const ENavigationEvent event) = 0;
		// </interfuscator:shuffle>
	};

	enum class EWorkingState
	{
		Idle,    // Navigation system doesn't have NavMesh update jobs running and there are no active update requests
		Working, // NavMesh update jobs are running
	};

	struct SCreateAgentTypeParams
	{
		SCreateAgentTypeParams(const Vec3& _voxelSize = Vec3(0.1f), uint16 _radiusVoxelCount = 4,
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

	enum class EMeshFlag : uint32
	{
		RemoveInaccessibleTriangles = BIT32(0),
	};

	struct SCreateMeshParams
	{
		SCreateMeshParams(const Vec3& _origin = Vec3(ZERO), const Vec3i& _tileSize = Vec3i(8), const uint32 _tileCount = 1024)
			: origin(_origin)
			, tileSize(_tileSize)
			, tileCount(_tileCount)
		{
		}

		Vec3   origin;
		Vec3i  tileSize;
		uint32 tileCount;
		CEnumFlags<EMeshFlag> flags;
	};

	//! Helper struct used in GetMeshBorders
	struct SNavMeshBorderWithNormal
	{
		Vec3 v0;
		Vec3 v1;
		Vec3 normalOut;

		SNavMeshBorderWithNormal()
			: v0(ZERO)
			, v1(ZERO)
			, normalOut(ZERO)
		{}

		SNavMeshBorderWithNormal(const Vec3& v0_, const Vec3& v1_, const Vec3& normalOut_)
			: v0(v0_)
			, v1(v1_)
			, normalOut(normalOut_)
		{}
	};

	typedef DynArray<SNavMeshBorderWithNormal> NavMeshBorderWithNormalArray;


	// <interfuscator:shuffle>
	virtual ~INavigationSystem() {}
	virtual NavigationAgentTypeID           CreateAgentType(const char* name, const SCreateAgentTypeParams& params) = 0;
	virtual NavigationAgentTypeID           GetAgentTypeID(const char* name) const = 0;
	virtual NavigationAgentTypeID           GetAgentTypeID(size_t index) const = 0;
	virtual const char*                     GetAgentTypeName(NavigationAgentTypeID agentTypeID) const = 0;
	virtual size_t                          GetAgentTypeCount() const = 0;

	virtual const MNM::IAnnotationsLibrary& GetAnnotationLibrary() const = 0;
	virtual MNM::AreaAnnotation             GetAreaTypeAnnotation(const NavigationAreaTypeID areaTypeID) const = 0;

	//! Sets include and exclude flags of global NavMesh filter.
	//! Global filter is used when no filter is passed to Navigation system functions
	//! \param includeFlags Mask of navigation area flags, at least one of whose must be set in triangle to be accepted by the filter.
	//! \param excludeFlags Mask of navigation area flags, none of whose must be set in triangle to be accepted by the filter.
	virtual void                            SetGlobalFilterFlags(const MNM::AreaAnnotation::value_type includeFlags, const MNM::AreaAnnotation::value_type excludeFlags) = 0;
	
	//! Get include and exclude flags of global NavMesh filter.
	//! Global filter is used when no filter is passed to Navigation system functions
	//! \param includeFlags Reference to mask of navigation area flags, at least one of whose must be set in triangle to be accepted by the filter.
	//! \param excludeFlags Reference to mask of navigation area flags, none of whose must be set in triangle to be accepted by the filter.
	virtual void                            GetGlobalFilterFlags(MNM::AreaAnnotation::value_type& includeFlags, MNM::AreaAnnotation::value_type& excludeFlags) const = 0;

	virtual MNM::INavMeshQueryManager*      GetNavMeshQueryManager() = 0;

#ifdef SW_NAVMESH_USE_GUID
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, NavigationMeshGUID guid) = 0;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, NavigationMeshID requestedID, NavigationMeshGUID guid) = 0;
#else
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params) = 0;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, NavigationMeshID requestedID) = 0;
#endif
	virtual NavigationMeshID CreateMeshForVolumeAndUpdate(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, const NavigationVolumeID volumeID) = 0;

	virtual void DestroyMesh(NavigationMeshID meshID) = 0;
	virtual void SetMeshFlags(NavigationMeshID meshID, const CEnumFlags<EMeshFlag> flags) = 0;
	virtual void RemoveMeshFlags(NavigationMeshID meshID, const CEnumFlags<EMeshFlag> flags) = 0;
	virtual CEnumFlags<EMeshFlag> GetMeshFlags(NavigationMeshID meshID) const = 0;

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
	
	//! Returns array of all navigation mesh ids for an agent type.
	//! \param agentTypeID Navigation agent type id.
	//! \returns Dynamic array containing ids of all navigation meshes registered to specific agent type.
	virtual DynArray<NavigationMeshID> GetMeshIDsForAgentType(const NavigationAgentTypeID agentTypeID) const = 0;
	virtual const char*  GetMeshName(NavigationMeshID meshID) const = 0;
	virtual void         SetMeshName(NavigationMeshID meshID, const char* name) = 0;

	virtual EWorkingState GetState() const = 0;

	// MNM regeneration tasks are queued up, but not executed
	virtual void         PauseNavigationUpdate() = 0;
	virtual void         RestartNavigationUpdate() = 0;
	virtual EWorkingState Update(const CTimeValue frameStartTime, const float frameTime, bool blocking = false) = 0;
	virtual uint32       GetWorkingQueueSize() const = 0;

	virtual void         ProcessQueuedMeshUpdates() = 0;

	virtual void         Clear() = 0;

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

	//! Registers editor Navigation Area shape.
	//! \param shapeName Name of the Navigation Area shape.
	//! \param outVolumeId Returns NavigationVolumeID, if there is a volume already loaded from exported data with the same shapeName.
	//! \returns true if the area name is successfully registered.
	virtual bool                             RegisterArea(const char* shapeName, NavigationVolumeID& outVolumeId) = 0;
	virtual void                             UnRegisterArea(const char* shapeName) = 0;
	virtual NavigationVolumeID               GetAreaId(const char* shapeName) const = 0;
	virtual void                             SetAreaId(const char* shapeName, NavigationVolumeID id) = 0;
	virtual void                             UpdateAreaNameForId(const NavigationVolumeID id, const char* newShapeName) = 0;
	//! Remove navigation meshes and volumes which don't have corresponding registered Navigation Areas. This may happen in editor, when the exported level data is older that the saved level.
	virtual void                             RemoveLoadedMeshesWithoutRegisteredAreas() = 0;

	virtual bool                             RegisterEntityMarkups(const IEntity& owningEntity, const char** shapeNamesArray, const size_t count, NavigationVolumeID* pOutIdsArray) = 0;
	virtual void                             UnregisterEntityMarkups(const IEntity& owningEntity) = 0;

	virtual void                             StartWorldMonitoring() = 0;
	virtual void                             StopWorldMonitoring() = 0;

	virtual bool                             IsInUse() const = 0;

	virtual void                             CalculateAccessibility() = 0;

	virtual const IOffMeshNavigationManager& GetIOffMeshNavigationManager() const = 0;
	virtual IOffMeshNavigationManager&       GetIOffMeshNavigationManager() = 0;

	virtual TileGeneratorExtensionID         RegisterTileGeneratorExtension(MNM::TileGenerator::IExtension& extension) = 0;
	virtual bool                             UnRegisterTileGeneratorExtension(const TileGeneratorExtensionID extensionId) = 0;

	//! Gets a MNM::INavMesh, which provides access to the NavMesh data.
	//! Pointer to the mesh shouldn't be cached because it can be invalidated later when the mesh is destroyed.
	// \param meshID Id of the navigation mesh.
	// \returns Pointer to NavMesh class or nullptr if it wasn't found
	virtual const MNM::INavMesh* GetMNMNavMesh(const NavigationMeshID meshID) const = 0;

	//! Returns the Agent type id of the navigation mesh.
	//! \param meshID Id of the navigation mesh.
	//! \returns Returns Agent type id of for whom the mesh is built.
	virtual NavigationAgentTypeID GetAgentTypeOfMesh(const NavigationMeshID meshID) const = 0;

	//! Finds the enclosing mesh id of a position. Returns valid NavMesh only when the position is directly inside a Navigation Area volume of the NavMesh.
	//! \param agentTypeID Navigation agent type id.
	//! \param position Location used to find NavMesh id.
	//! \returns Navigation mesh id of the enclosing NavMesh.
	virtual NavigationMeshID FindEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& location) const = 0;

	//! Finds the enclosing mesh id of a position, expanding the position using the horizontal or vertical range of the snapping metric. The first NavMesh fulfilling the metric is returned.
	//! \param agentTypeID Navigation agent type id.
	//! \param position Location used to find NavMesh id.
	//! \param snappingMetric Snapping metric params with ranges used to extend the input position.
	//! \returns Navigation mesh id of the enclosing NavMesh.
	virtual NavigationMeshID FindEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SSnappingMetric& snappingMetric) const = 0;

	//! Finds the enclosing mesh id of a position, expanding the position using the horizontal or vertical range of the snapping metric. The first NavMesh fulfilling the metrics is returned.
	//! \param agentTypeID Navigation agent type id.
	//! \param position Location used to find NavMesh id.
	//! \param snappingMetrics Ordered snapping metrics array with ranges used to extend the input position.
	//! \returns Navigation mesh id of the enclosing NavMesh.
	virtual NavigationMeshID FindEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SOrderedSnappingMetrics& snappingMetrics) const = 0;

	//! Checks whether a position lies directly inside a Navigation Area volume.
	//! \param meshID Id of the navigation mesh.
	//! \param location Position of the point to test.
	//! \returns Returns true if the location is inside NavMesh volume (Navigation Area).
	virtual bool IsLocationInMeshVolume(const NavigationMeshID meshID, const Vec3& location) const = 0;

	//! Checks whether a position lies inside or nearby a Navigation Area volume using snapping metric.
	//! \param meshID Id of the navigation mesh.
	//! \param location Position of the point to test.
	//! \param snappingMetric Structure of snapping metric params defining how the location will be extended during the test.
	//! \returns Returns true if the extended location (by snapping metric) is overlapping with NavMesh volume (Navigation Area).
	virtual bool IsLocationInMeshVolume(const NavigationMeshID meshID, const Vec3& location, const MNM::SSnappingMetric& snappingMetric) const = 0;

	//! Checks whether a position lies inside or nearby a Navigation Area volume using ordered snapping metrics.
	//! \param meshID Id of the navigation mesh.
	//! \param location Position of the point to test.
	//! \param snappingMetrics Ordered snapping metrics array with ranges used to extend the input position.
	//! \returns Returns true if the extended location (by snapping metric) is overlapping with NavMesh volume (Navigation Area).
	virtual bool IsLocationInMeshVolume(const NavigationMeshID meshID, const Vec3& location, const MNM::SOrderedSnappingMetrics& snappingMetrics) const = 0;

	//! Function for getting bounding box of the given tile
	//! \param meshID Id of the navigation mesh where the tile is located.
	//! \param tileD Id of the tile.
	//! \param bounds Output reference to axis aligned bounding box of the tile.
	virtual void GetTileBoundsForMesh(const NavigationMeshID meshID, const MNM::TileID tileID, AABB& bounds) const = 0;

	//! Fills Triangle's vertices of the specified triangle
	//! \param meshID Id of the navigation mesh where the triangle is located.
	//! \param triangleID Triangle id.
	//! \param outTriangleVertices Output structure containing triangle's vertices.
	//! \returns True, if the triangle is found and its vertices are returned.
	virtual bool GetTriangleVertices(const NavigationMeshID meshID, const MNM::TriangleID triangleID, Triangle& outTriangleVertices) const = 0;

	//! A cheap test to see if two points are connected, without the expense of computing a full path between them.
	//! This function does not do a full path-find and does not know about dynamic obstacles. It only reasons at a 'high level' about the connectivity of areas that are part of the NavMes.
	//! \param agentID Navigation agent type id.
	//! \param pEntityToTestOffGridLinks Pointer to an entity used for checking whether off-mesh links can be used.
	//! \param startLocation Starting location of the request.
	//! \param endLocation Ending location of the request.
	//! \param snappingMetrics Ordered snapping metrics for finding triangles in NavMesh using input positions.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \returns True if the path possibly exists between input locations.
	virtual bool IsPointReachableFromPosition(
		const NavigationAgentTypeID agentID,
		const IEntity* pEntityToTestOffGridLinks,
		const Vec3& startLocation,
		const Vec3& endLocation,
		const MNM::SOrderedSnappingMetrics& snappingMetrics,
		const INavMeshQueryFilter* pFilter) const = 0;

	//! A cheap test to see if two NavMesh triangles are connected, without the expense of computing a full path between them.
	//! This function does not do a full path-find and does not know about dynamic obstacles. It only reasons at a 'high level' about the connectivity of areas that are part of the NavMes.
	//! \param pEntityToTestOffGridLinks Pointer to an entity used for checking whether off-mesh links can be used.
	//! \param startMeshID Id of the navigation mesh where start triangle is located.
	//! \param startTriangleID Starting triangle of the test.
	//! \param endMeshID Id of the navigation mesh where end triangle is located.
	//! \param endTriangleID Ending triangle of the test.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \returns True if the path possibly exists between input triangles.
	virtual bool IsPointReachableFromPosition(
		const IEntity* pEntityToTestOffGridLinks,
		const NavigationMeshID startMeshID,
		const MNM::TriangleID startTriangleID,
		const NavigationMeshID endMeshID,
		const MNM::TriangleID endTriangleID,
		const INavMeshQueryFilter* pFilter) const = 0;

	//! Performs ray-cast on NavMesh.
	//! \param agentTypeID Navigation agent type id.
	//! \param startPos Starting position of the ray.
	//! \param endPos End position of the ray.
	//! \param pOutHit Optional pointer for a return value of additional information about the hit. This structure is valid only when the hit is reported.
	//! \returns Returns MNM::ERayCastResult::Hit if the ray has hit a NavMesh boundary before end position or other value in case of no hit or an error.
	virtual MNM::ERayCastResult NavMeshRayCast(const NavigationAgentTypeID agentTypeID, const Vec3& startPos, const Vec3& endPos, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const = 0;

	//! Performs ray-cast on NavMesh. Doesn't need to snap starting and ending position to NavMesh since triangle ids are used as parameters.
	//! \param meshID NavMesh id on which ray-cast should be executed.
	//! \param startTriangleId Triangle id where startPos is located.
	//! \param startPos Starting position of the ray.
	//! \param endTriangleId Triangle id where endPos is located.
	//! \param endPos End position of the ray.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \param pOutHit Optional pointer for a return value of additional information about the hit. This structure is valid only when the hit is reported.
	//! \returns MNM::ERayCastResult::Hit if the ray has hit a NavMesh boundary before end position or other value in case of no hit or an error.
	virtual MNM::ERayCastResult NavMeshRayCast(const NavigationMeshID meshID, const MNM::TriangleID startTriangleId, const Vec3& startPos, const MNM::TriangleID endTriangleId, const Vec3& endPos, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const = 0;

	//! Performs ray-cast on NavMesh.
	//! \param meshID NavMesh id on which ray-cast should be executed.
	//! \param startPointOnNavMesh Point on NavMesh from where the ray should start.
	//! \param endPointOnNavMesh Point on NavMesh where the ray should end.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \param pOutHit Optional pointer for a return value of additional information about the hit. This structure is valid only when the hit is reported.
	//! \returns MNM::ERayCastResult::Hit if the ray has hit a NavMesh boundary before end position or other value in case of no hit or an error.
	virtual MNM::ERayCastResult NavMeshRayCast(const NavigationMeshID meshID, const MNM::SPointOnNavMesh& startPointOnNavMesh, const MNM::SPointOnNavMesh& endPointOnNavMesh, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const = 0;

	//! Query for the triangle borders that overlap the aabb. 
	//! Overlapping is calculated using the mode ENavMeshQueryOverlappingMode::BoundingBox_Partial
	//! Triangles are filtered using SAcceptAllQueryTrianglesFilter
	//! Triangles annotations are filtered using SAcceptAllQueryTrianglesFilter
	//! \param meshID Id of the navigation mesh.
	//! \param localAabb Axis aligned bounding box for querying triangles.
	//! \returns An array containing the border and the normal pointing out of the found borders
	virtual NavMeshBorderWithNormalArray QueryTriangleBorders(const NavigationMeshID meshID, const MNM::aabb_t& localAabb) const = 0;

	//! Query for the triangle borders that overlap the aabb. 
	//! \param meshID Id of the navigation mesh.
	//! \param localAabb Axis aligned bounding box for querying triangles.
	//! \param overlappingMode Overlapping mode use to calculate when an overlap between the triangle and the aabb should be considered
	//! \param pQueryFilter Triangle filter used in the query. If nullptr uses SAcceptAllQueryTrianglesFilter
	//! \param pAnnotationFilter Triangle annotation filter used in the query. If nullptr uses SAcceptAllQueryTrianglesFilter
	//! \returns An array containing the border and the normal pointing out of the found borders
	virtual NavMeshBorderWithNormalArray QueryTriangleBorders(const NavigationMeshID meshID, const MNM::aabb_t& localAabb, MNM::ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pQueryFilter, const INavMeshQueryFilter* pAnnotationFilter) const = 0;

	//! Gets triangle centers.
	//! Overlapping is calculated using the mode ENavMeshQueryOverlappingMode::BoundingBox_Partial
	//! Triangles are filtered using SAcceptAllQueryTrianglesFilter
	//! \param meshID Id of the navigation mesh.
	//! \param localAabb Axis aligned bounding box for querying triangles.
	//! \returns An array containing the triangle centers
	virtual DynArray<Vec3> QueryTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const MNM::aabb_t& localAabb) const = 0;

	//! Gets triangle centers.
	//! \param meshID Id of the navigation mesh.
	//! \param localAabb Axis aligned bounding box for querying triangles.
	//! \param overlappingMode Overlapping mode use to calculate when an overlap between the triangle and the aabb should be considered
	//! \param pFilter Pointer to navigation query filter. If nullptr uses SAcceptAllQueryTrianglesFilter
	//! \returns An array containing the triangle centers
	virtual DynArray<Vec3> QueryTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const MNM::aabb_t& localAabb, MNM::ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pFilter) const = 0;

	//! Returns global island id of the triangle at location.
	//! \param agentTypeID Navigation agent type id.
	//! \param location Location for querying the triangle at location.
	//! \param snappingMetrics Ordered snapping metrics for finding triangle in NavMesh using input positions.
	//! \returns Global island id of the triangle at location or MNM::Constants::eGlobalIsland_InvalidIslandID if no triangle is found.
	virtual MNM::GlobalIslandID GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location, const MNM::SOrderedSnappingMetrics& snappingMetrics) const = 0;

	//! Returns global island id of the triangle.
	//! \param meshID Id of the navigation mesh where the triangle is located.
	//! \param triangleID Triangle id whom island id should be returned.
	//! \returns Global island id of the triangle or MNM::Constants::eGlobalIsland_InvalidIslandID in case of invalid triangle.
	virtual MNM::GlobalIslandID GetGlobalIslandIdAtPosition(const NavigationMeshID meshID, const MNM::TriangleID triangleID) const = 0;

	//! Finds the enclosing NavMesh and snaps the point to the NavMesh using the snapping metric.
	//! \param agentTypeID navigation agent type id.
	//! \param position Position to be snapped to NavMesh.
	//! \param snappingMetric Snapping metric structure.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \param pOutSnappedPosition Optional pointer to snapped position, valid only when the function has returned true.
	//! \param pOutTriangleID Optional pointer to a triangle id, which contains snapped position.
	//! \param pOutNavMeshID Optional pointer to a NavMesh id, which contains snapped position.
	//! \returns Returns true if the position can be successfully snapped to NavMesh, false otherwise.
	virtual bool SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID,
		const Vec3& position,
		const MNM::SSnappingMetric& snappingMetric,
		const INavMeshQueryFilter* pFilter,
		Vec3* pOutSnappedPosition,
		MNM::TriangleID* pOutTriangleID,
		NavigationMeshID* pOutNavMeshID) const = 0;

	//! Finds the enclosing NavMesh and snaps the point to the NavMesh using the ordered array of snapping metrics.
	//! For each snapping metric the function first tries to find enclosing NavMesh volume and then snap to it. The first successful snapping is returned.
	//! \param agentTypeID navigation agent type id.
	//! \param position Position to be snapped to NavMesh.
	//! \param snappingMetrics Ordered snapping metrics array.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles
	//! \param pOutSnappedPosition Optional pointer to snapped position, valid only when the function has returned true.
	//! \param pOutTriangleID Optional pointer to a triangle id, which contains snapped position.
	//! \param pOutNavMeshID Optional pointer to a NavMesh id, which contains snapped position.
	//! \returns Returns true if the position can be successfully snapped to NavMesh, false otherwise.
	virtual bool SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID,
		const Vec3& position,
		const MNM::SOrderedSnappingMetrics& snappingMetrics,
		const INavMeshQueryFilter* pFilter,
		Vec3* pOutSnappedPosition,
		MNM::TriangleID* pOutTriangleID,
		NavigationMeshID* pOutNavMeshID) const = 0;

	//! Returns snapped point on the NavMesh using the snapping metric.
	//! \param agentTypeID navigation agent type id.
	//! \param position Position to be snapped to NavMesh.
	//! \param snappingMetric Snapping metric structure.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \param pOutNavMeshID Optional pointer to a NavMesh id, which contains snapped point.
	//! \returns Returns valid point on NavMesh if the position can be successfully snapped to NavMesh.
	virtual MNM::SPointOnNavMesh SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID,
		const Vec3& position,
		const MNM::SSnappingMetric& snappingMetric,
		const INavMeshQueryFilter* pFilter,
		NavigationMeshID* pOutNavMeshID) const = 0;

	//! Returns snapped point on the NavMesh using the ordered array of snapping metrics.
	//! For each snapping metric the function first tries to find enclosing NavMesh volume and then snap to it. The first successful snapping is returned.
	//! \param agentTypeID navigation agent type id.
	//! \param position Position to be snapped to NavMesh.
	//! \param snappingMetrics Ordered snapping metrics array.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \param pOutNavMeshID Optional pointer to a NavMesh id, which contains snapped position.
	//! \returns Returns valid point on NavMesh if the position can be successfully snapped to NavMesh.
	virtual MNM::SPointOnNavMesh SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID,
		const Vec3& position,
		const MNM::SOrderedSnappingMetrics& snappingMetrics,
		const INavMeshQueryFilter* pFilter,
		NavigationMeshID* pOutNavMeshID) const = 0;

	//! (DEPRECATED) Finds the enclosing meshID of a position. FindEnclosingMeshID should be now used instead.
	//! \param agentTypeID Navigation agent type id.
	//! \param position Location used to find NavMesh id.
	//! \returns Navigation mesh id of the enclosing NavMesh.
	virtual NavigationMeshID GetEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& location) const = 0;

	//! (DEPRECATED) Returns id of the tile consisting input location. SnapToNavMesh function should be now used instead.
	//! \param meshID Id of the navigation mesh.
	//! \param location Position of the point lying in the tile.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \returns Tile id where the input locations is lying.
	virtual MNM::TileID GetTileIdWhereLocationIsAtForMesh(const NavigationMeshID meshID, const Vec3& location, const INavMeshQueryFilter* pFilter) = 0;

	//! (DEPRECATED) Function for getting the closest point in NavMesh. SnapToNavMesh should be now used instead.
	//! \param agentID Navigation agent type id.
	//! \param location Position of the input point
	//! \param vrange Vertical search range
	//! \param hrange Horizontal search range
	//! \param meshLocation Pointer to position on the NavMesh closest to input position if it was found
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \returns True if the closest point was found, false otherwise
	virtual bool GetClosestPointInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float vrange, float hrange, Vec3* meshLocation, const INavMeshQueryFilter* pFilter) const = 0;

	//! (DEPRECATED) A cheap test to see if two points are connected, without the expense of computing a full path between them. Variation with snapping metric(s) should be used now instead.
	//! \param agentID Navigation agent type id.
	//! \param pEntityToTestOffGridLinks Pointer to an entity used for checking whether off-mesh links can be used.
	//! \param startLocation Starting location of the request.
	//! \param endLocation Ending location of the request.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \returns True if the path possibly exists between input locations.
	virtual bool IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation, const INavMeshQueryFilter* pFilter) const = 0;

	//! (DEPRECATED) Returns triangle id of the triangle at location. The function is using hard-coded vertical ranges based on agent's height for querying triangles. SnapToNavMesh should be used now instead.
	//! \param agentTypeID Navigation agent type id.
	//! \param location Location for querying the triangle at location.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \returns Triangle id of the triangle at location or MNM::Constants::InvalidTriangleID if no triangle is found.
	virtual MNM::TriangleID GetTriangleIDWhereLocationIsAtForMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter) = 0;

	//! (DEPRECATED) Returns global island id of the triangle at location. The function is using hard-coded vertical ranges (1.0f) for querying triangles.
	//! \param agentTypeID Navigation agent type id.
	//! \param location Location for querying the triangle at location.
	//! \returns Global island id of the triangle at location or MNM::Constants::eGlobalIsland_InvalidIslandID if no triangle is found.
	virtual MNM::GlobalIslandID GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location) const = 0;

	//! (DEPRECATED) Test for checking if input location is in range directly above or below NavMesh. SnapToNavMesh should be used now instead.
	//! \param agentID Navigation agent type id.
	//! \param location Position to check.
	//! \param pFilter Pointer to navigation query filter. Can be null to accept all triangles.
	//! \param downRange Vertical down range.
	//! \param upRange Vertical up range.
	//! \returns True if the location is in the range of NavMesh.
	virtual bool IsLocationValidInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter, float downRange = 1.0f, float upRange = 1.0f) const = 0;

	// </interfuscator:shuffle>
};

#endif // __INavigationSystem_h__
