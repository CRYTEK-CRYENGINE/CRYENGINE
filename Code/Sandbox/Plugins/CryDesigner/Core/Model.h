// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Polygon.h"

namespace Designer
{
class ModelDB;
class BSPTree3D;
class HalfEdgeMesh;
class UVIslandManager;
class SmoothingGroupManager;
class EdgeSharpnessManager;

//! An enum that lists flags for a model object.
//! Used as an input parameter or return value in Model::SetFlag, Model::GetFlag, Model::AddFlag and Model::CheckFlag methods
enum EModelFlag
{
	eModelFlag_LeaveFrameAfterPolygonRemoved = BIT(1),
	eModelFlag_Mirror                        = BIT(3),
	eModelFlag_DisplayBackFace               = BIT(4)
};

//! An enum that lists results of finding opposite polygons
//! Used as a return type for Model::QueryOppositePolygon something like that
enum EResultOfFindingOppositePolygon
{
	eFindOppositePolygon_None,
	eFindOppositePolygon_Intersection,
	eFindOppositePolygon_ZeroDistance
};

//! An enum that lists two directions of the opposite side
//! Used as a input param for Model::QueryOppositePolygon
enum EFindOppositeFlag
{
	eFOF_PushDirection,
	eFOF_PullDirection
};

//! An enum that lists a result of clipping a model.
//! Used as a return type for Model::Clip
enum EClipPolygonResult
{
	eCPR_FAILED_CLIP,
	eCPR_SUCCESSED,
	eCPR_CLIPSUCCESSED_BUT_FAILED_FILLFACET
};

//! This structure is used for collecting edges returned by some edge query methods
//! Such as querying nearest edges etc.
struct QueryEdgeResult
{
	QueryEdgeResult(
	  PolygonPtr pPolygon,
	  const BrushEdge3D& edge)
	{
		m_pPolygon = pPolygon;
		m_Edge = edge;
	}
	PolygonPtr  m_pPolygon;
	BrushEdge3D m_Edge;
};

//! This is for storing a bound box of a model with a validation attribute
//! The validation attribute is for checking if the bound box should be updated or not.
//! Usually when polygons in a model instance are added or removed, ModelBound's instance is invalidated to be updated later on.
struct ModelBound
{
	ModelBound() : bValid(false)
	{
		aabb.Reset();
	}
	AABB aabb;
	bool bValid;
};

typedef std::pair<PolygonPtr, BrushVec3> IntersectionPairBetweenTwoEdges;

//! The Model class is one of the core class to be necessary for manipulating a mesh with the designer tools
//! It is a kind of storage of polygons with many methods to add, remove and query polygons etc.
class Model : public _i_reference_target_t
{
public:
	Model();
	Model(const std::vector<PolygonPtr>& polygonList);
	Model(const Model& model);
	~Model();

	//! Copies the whole data of the input model.
	//! GUID is also copied.
	Model& operator=(const Model& model);

	//! Removes all polygons in the current shelf.
	void Clear();

public: //! Add Methods

	//! Adds a polygon to the current shelf without applying any operations.
	//! \param pPolygon the input polygon.
	//! \param bResetUVs if this is true, the uvs of pPolygon will be calculated out of positions and normals of pPolygon.
	//! \return if the method is succeeded or not.
	bool AddPolygon(
	  PolygonPtr pPolygon,
	  bool bResetUVs = true);

	//! Adds a polygon to the current shelf after applying the split operation.
	//! The split operation is that the intersected parts and XOR parts between the input polygon and the polygons are added separately to the current shelf.
	//! The existing polygons in the shelf with the same plane as the input plane are candidates for the split operation.
	//! If the pPolygon is open, it will split the existing polygons crossed by it.
	//! UVs is re-created out of normals and positions.
	//! \param pPolygon the input polygon
	//! \return if the method is succeeded or not
	bool AddSplitPolygon(PolygonPtr pPolygon);

	//! Adds a polygon to the current shelf after applying the XOR operation.
	//! UVs is re-created out of normals and positions.
	//! \param pPolygon the input polygon which has to be closed.
	//! \param bApplyUnionToAdjancentPolygon if pPolygon is adjacent to a polygon in the shelf and bApplyUnionToAdjancentPolygon is true, Union operation will be done between the both.
	//! \return if the method is succeeded or not.
	bool AddXORPolygon(
	  PolygonPtr pPolygon,
	  bool bApplyUnionToAdjancentPolygon = true);

	//! Adds a polygon to the current shelf after union operation.
	//! \param pPolygon the input polygon which has to be closed.
	//! \return if the method is succeeded or not.
	bool AddUnionPolygon(PolygonPtr pPolygon);

	//! Adds a polygon to the current shelf after intersection operation.
	//! \param pPolygon the input polygon which has to be closed.
	//! \return if the method is succeeded or not.
	bool AddIntersectedPolygon(PolygonPtr pPolygon);

	//! Adds a polygon to the current shelf after subtracted operation.
	//! \param pPolygon the input polygon which has to be closed.
	//! \return if the method is succeeded or not.
	bool AddSubtractedPolygon(PolygonPtr pPolygon);

public: //! Remove methods

	//! Removes a polygon in the current shelf.
	//! \param nPolygonIndex the index of the polygon
	//! \param bLeaveFrame if bLeaveFrame is true and eModelFlag_LeaveFrameAfterPolygonRemoved is set to the model flag, thin frame surrounding the removed polygon will be left.
	//! \return if the method is succeeded or not.
	bool RemovePolygon(
	  int nPolygonIndex,
	  bool bLeaveFrame = false);

	//! Removes a polygon in the current shelf.
	//! \param pPolygon the input polygon
	//! \param bLeaveFrame if bLeaveFrame is true and eModelFlag_LeaveFrameAfterPolygonRemoved is set to the model flag, thin frame surrounding the removed polygon will be left.
	//! \return if the method is succeeded or not.
	bool RemovePolygon(
	  PolygonPtr pPolygon,
	  bool bLeaveFrame = false);

	//! Removes polygons with the specific polygon flags(EPolygonFlag) and plane
	//! \param nPolygonFlags combination of EPolygonFlag items.
	//! \param pPlane plane pointer. If it is NULL, plane check will be ignored.
	void RemovePolygonsWithSpecificFlagsAndPlane(
	  int nPolygonFlags,
	  const BrushPlane* pPlane = NULL);

public: //! Query methods

	//! Searches for a polygon with the specific guid.
	//! \param guid polygon's GUID
	//! \return the polygon pointer with guid. If it isn't found, NULL will be returned.
	PolygonPtr QueryPolygon(CryGUID guid) const;

	//! Searches for the index of the closest polygon to the ray origin with the specific plane and intersected by the ray.
	//! \param plane the input plane
	//! \param ray the input ray
	//! \param nOutIndex the found polygon closest to the ray's origin.
	//! \return if more than a polygon is found, it'll return true, otherwise false.
	bool QueryPolygon(
	  const BrushPlane& plane,
	  const BrushRay& ray,
	  int& nOutIndex) const;

	//! Searches for polygons with the specific plane.
	//! \param plane the input plane
	//! \param outPolygons all polygons with the same plane as the input plane.
	//! \return if more than a polygon is found, it'll return true, otherwise false.
	bool QueryPolygons(
	  const BrushPlane& plane,
	  PolygonList& outPolygons) const;

	//! Searches for polygons intersected by the AABB.
	//! \param aabb bounding box
	//! \param outPolygons returned polygons intersected by aabb
	//! \return If any polygons intersected by aabb, returns true. If not, false is returned.
	bool QueryIntersectedPolygonsByAABB(
	  const AABB& aabb,
	  PolygonList& outPolygons) const;

	//! Searches for the closet polygon to the ray's origin intersected by the ray.
	//! \param ray the input ray
	//! \param nOutIndex the index of the closest polygon
	//! \return if the intersected polygon is found, true is returned.
	bool QueryPolygon(
	  const BrushRay& ray,
	  int& nOutIndex) const;

	//! Searches for the equivalent polygon which has the same vertices and edges as pPolygon
	//! \param pPolygon the input polygon
	//! \param pOutPolygonIndex has the index of the equivalent polygon.
	//! \return the equivalent polygon. if finding the equivalent is failed, NULL is returned, otherwise false.
	PolygonPtr QueryEquivalentPolygon(
	  PolygonPtr pPolygon,
	  int* pOutPolygonIndex = NULL) const;

	//! Searches for the nearest edges to the intersected position between the ray and the closest polygon from the ray's origin
	//! \param ray the input ray
	//! \param outPos Returns the intersection between the ray and the closest polygon from the ray's origin.
	//! \param outPosOnEdge Returns the nearest positions from outPos to edges on the intersected polygon.
	//! \param outPlane Returns the plane of the closest polygon
	//! \param outEdges Returns the edges which are different from one another but have the same distance from the intersection.
	//! \param Returns true if the nearest position on the edge is found. Otherwise false
	bool QueryNearestEdges(
	  const BrushRay& ray,
	  BrushVec3& outPos,
	  BrushVec3& outPosOnEdge,
	  BrushPlane& outPlane,
	  std::vector<QueryEdgeResult>& outEdges) const;

	//! Searches for the nearest edges to the intersected position between the ray and the polygon with the intput plane.
	//! \param plane the input plane
	//! \param ray the input ray
	//! \param outPos Returns the intersection between the ray and the closest polygon from the ray's origin.
	//! \param outPosOnEdge Returns the nearest positions from outPos to edges on the intersected polygon.
	//! \param outEdges Returns the edges which are different from one another but have the same distance from the intersection.
	bool QueryNearestEdges(
	  const BrushPlane& plane,
	  const BrushRay& ray,
	  BrushVec3& outPos,
	  BrushVec3& outPosOnEdge,
	  std::vector<QueryEdgeResult>& outEdges) const;

	//! Searches for the nearest edges to the input position. The polygons with the candidate edges should have the same plane as the input plane.
	//! \param plane the input plane
	//! \param position the input position
	//! \param outPosOnEdge Returns the nearest positions from the input position to edges on the polygons on the input plane
	//! \param outEdges Returns the edges which are different from one another but have the same distance from the input position.
	bool QueryNearestEdges(
	  const BrushPlane& plane,
	  const BrushVec3& position,
	  BrushVec3& outPosOnEdge,
	  std::vector<QueryEdgeResult>& outEdges) const;

	//! Searches for the nearest position on this model from the input pos.
	//! \param pos the input position
	//! \param outNearestPos the nearest position
	//! \return return true if the nearest position is found.
	bool QueryNearestPos(
	  const BrushVec3& pos,
	  BrushVec3& outNearestPos) const;

	//! Searches for a position on the closest polygon to the input ray's origin intersected by the input ray
	//! \param ray the input ray
	//! \param outPosition the intersected position between the closest polygon and the input ray
	//! \param outPlane the plane of the closest polygon
	//! \param outDist the distance from the ray's origin to outPosition
	//! \param outPolygon the polygon closest to the ray's origin
	//! \return if the closest polygon is found, true is returned. otherwise false.
	bool QueryPosition(
	  const BrushRay& ray,
	  BrushVec3& outPosition,
	  BrushPlane* outPlane = NULL,
	  BrushFloat* outDist = NULL,
	  PolygonPtr* outPolygon = NULL) const;

	//! Searches for a position on the closest polygon to the input ray's origin intersected by the input ray
	//! The polygons on the input plane are candidate of this query.
	//! \param plane the input plane
	//! \param ray the input ray
	//! \param outPosition the intersected position between the closest polygon and the input ray
	//! \param outDist the distance from the ray's origin to outPosition
	//! \param outPolygon the polygon closest to the ray's origin
	//! \return if the closest polygon is found, true is returned. otherwise false.
	bool QueryPosition(
	  const BrushPlane& plane,
	  const BrushRay& ray,
	  BrushVec3& outPosition,
	  BrushFloat* outDist = NULL,
	  PolygonPtr* outPolygon = NULL) const;

	//! Searches for the polygons containing the input edge. The edge direction isn't considered.
	//! \param edge the input edge
	//! \param outPolygons the polygons with the input edge
	void QueryPolygonsHavingEdge(
	  const BrushEdge3D& edge,
	  std::vector<PolygonPtr>& outPolygons) const;

	//! Searches for edges containing the input position.
	//! \param pos the input position
	//! \param outEdges edges which contains the input position.
	//! \return if the edges are found, true is returned. otherwise false.
	bool QueryEdgesWithPos(
	  const BrushVec3& pos,
	  std::vector<BrushEdge3D>& outEdges) const;

	//! Returns the center position of the AABB of the polygon intersected by the input ray
	//! \param ray the input ray
	//! \param outCenter the center position of the bound box of the AABB of the polygon intersected by the input ray.
	bool QueryCenterOfPolygon(
	  const BrushRay& ray,
	  BrushVec3& outCenter) const;

	//! Searches for the polygons which are adjacent and perpendicular with the input polygon.
	//! \param pPolygon the input polygon
	//! \param outPolygons the returned adjacent and perpendicular polygons
	void QueryAdjacentPerpendicularPolygons(
	  PolygonPtr pPolygon,
	  PolygonList& outPolygons) const;

	//! Searches for the polygons which are closed to pPolygon and perpendicular with the pPolygon's normal or pNormal if pNormal is set.
	//! \param pPolygon the input polygon
	//! \param outPolygons the returned perpendicular polygons
	//! \param pNormal should be set if the different normal from pPolygon should be used
	void QueryPerpendicularPolygons(
	  PolygonPtr pPolygon,
	  PolygonList& outPolygons,
	  BrushVec3* pNormal = NULL) const;

	//! Searches for the opposite and closest polygon from the input polygon in a direction specified by nFlag
	//! \param pPolygon the input polygon
	//! \param nFlag the direction of the opposite side.
	//! \param fScale how much the input polygon is scaled.
	//! \param outPolygon the opposite polygon searched by
	//! \param outDistance the distance from pPolygon to the opposite polygon
	//! \return how the input polygon and the opposite polygon are related.
	EResultOfFindingOppositePolygon QueryOppositePolygon(
	  PolygonPtr pPolygon,
	  EFindOppositeFlag nFlag,
	  BrushFloat fScale,
	  PolygonPtr& outPolygon,
	  BrushFloat& outDistance) const;

	//! Searches for the intersected polygons by the input polygon. The intersected polygons have the same plane with the input polygon's plane.
	//! \param pPolygon the input polygon
	//! \param outIntersectionPolygons the intersected polygons by the input polygon
	void QueryIntersectionByPolygon(
	  PolygonPtr pPolygon,
	  PolygonList& outIntersetionPolygons) const;

	//! Searches for the intersections between the input edge and the edges consisting of each polygon in the current shelf.
	//! \param edge the input edge
	//! \param outIntersections the output intersections consisting of the polygon and the intersected position.
	void QueryIntersectionByEdge(
	  const BrushEdge3D& edge,
	  std::vector<IntersectionPairBetweenTwoEdges>& outIntersections) const;

	//! Searches for the polygons intersected by the rectangle defined as a form of a polygon with the screen space coordinates.
	//! \param pView viewport
	//! \param worldTM world matrix
	//! \param p2DRectPolygonOnScreenSpace the 2D rect polygon on screen space
	//! \param bExcludeBackFace whether the polygons whose back sides are toward the camera are excluded or not.
	//! \param outIntersectionPolygon the output intersected polygons by the 2D input rectangle.
	void QueryIntersectionPolygonsWith2DRect(
	  IDisplayViewport* pView,
	  const BrushMatrix34& worldTM,
	  PolygonPtr p2DRectPolygonOnScreenSpace,
	  bool bExcludeBackFace,
	  PolygonList& outIntersectionPolygons) const;

	//! Searches for the edges intersected by the rectangle defined as a form of a polygon with the screen space coordinates.
	//! \param pView viewport
	//! \param worldTM world matrix
	//! \param p2DRectPolygonOnScreenSpace the 2D rect polygon on screen space
	//! \param bExcludeBackFace whether the polygons whose back sides are toward the camera are excluded or not.
	//! \param outIntersectionEdges the output intersected edges by the 2D input rectangle.
	void QueryIntersectionEdgesWith2DRect(
	  IDisplayViewport* pView,
	  const BrushMatrix34& worldTM,
	  PolygonPtr p2DRectPolygonOnScreenSpace,
	  bool bExcludeBackFace,
	  EdgeQueryResult& outIntersectionEdges) const;

	//! Searches for the open polygons intersected by the input ray.
	//! \param ray the input ray
	//! \param outPolygons the open polygons intersected by the input ray.
	void QueryOpenPolygons(
	  const BrushRay& ray,
	  std::vector<PolygonPtr>& outPolygons) const;

	//! Searches for the polygons which have the whole or part of the input edge.
	//! \param edge the input edge
	//! \param outPolygons the output polygons.
	void QueryNeighbourPolygonsByEdge(
	  const BrushEdge3D& edge,
	  PolygonList& outPolygons) const;

	//! Searches for the polygons having the input edge
	//! \param edge the input edge
	//! \param outPolyons output polygons with the input edge
	void QueryPolygonsSharingEdge(
	  const BrushEdge3D& edge,
	  PolygonList& outPolygons) const;

public: //! Etc methods

	//! Serializes the whole data consisting of this model.
	void Serialize(
	  XmlNodeRef& xmlNode,
	  bool bLoading,
	  bool bUndo);

	//! Replaces the polygon referred by nIndex with pPolygon
	//! \params nIndex the polygon index
	//! \params pPolygon the new polygons with which will replace the polygon referred by nIndex
	void Replace(
	  int nIndex,
	  PolygonPtr pPolygon);

	//! Erases an edge. If the edge is shared by some co-planar polygons, the polygons will be merged.
	bool EraseEdge(const BrushEdge3D& edge);

	//! Erase a vertex with the input position. All polygons with the input position will be removed.
	bool EraseVertex(const BrushVec3& position);

	//! If there are polygons intersected or touched by the input polygon, true is returned.
	//! If bStrongCheck is true, polygons touched by the input polygon will be excluded.
	bool HasIntersection(
	  PolygonPtr pPolygon,
	  bool bStrongCheck = false) const;

	//! If there are polygons not intersected but touched by pPolygon, true is returned.
	//! The candidate polygons should share the same plane as the one of pPolygon.
	bool HasTouched(PolygonPtr pPolygon) const;

	//! If the polygon having the input edge exists, true is returned. otherwise false.
	bool HasEdge(const BrushEdge3D& edge) const;

	//! Checks if polygon instance exists in the current shelf.
	bool HasPolygon(const PolygonPtr polygon) const;

	//! Records the current states including the whole polygon to the undo stack.
	void RecordUndo(
	  const char* sUndoDescription,
	  CBaseObject* pObject) const;

	//! Translates all polygons in the current shelf by the input offset.
	//! \param offset the input offset to move the model
	void Translate(const BrushVec3& offset);

	//! Multiplying the input matrix to all the polygons in the current shelf.
	//! \param tm the input 3x4 matrix
	void Transform(const BrushMatrix34& tm);

	//! Moves all the polygons in the source shelf to the destination shelf.
	//! \param sourceShelfID the source shelf's ID
	//! \param destShelfID the destination shelf's ID
	void MovePolygonsBetweenShelves(
	  ShelfID sourceShelfID,
	  ShelfID destShelfID);

	//! Moves the polygon existing in the source shelf to the destination shelf.
	//! if the polygon doesn't exist in the source shelf, nothing happens.
	//! \param sourceShelfID the source shelf's ID
	//! \param destShelfID the destination shelf's ID
	void MovePolygonBetweenShelves(
	  PolygonPtr polygon,
	  ShelfID sourceShelfID,
	  ShelfID destShelfID);

	//! Returns all polygons in the current shelf. If bIncludeOpenPolygons is set to false, all open polygons are excluded.
	//! \param outPolygonList all the polygons in the current shelf.
	//! \param bIncludedOpenPolygons whether open polygons is included or not.
	void GetPolygonList(
	  PolygonList& outPolygonList,
	  bool bIncludeOpenPolygons = false) const;

	//! Returns the polygon referred by nPolygonIndex
	//! \param nPolygonIndex the input index of polygon
	//! \return the polygon pointer referred by nPolygonIndex
	PolygonPtr GetPolygon(int nPolygonIndex) const;

	//! Returns the polygon index referring to pPolygon
	//! \param pPolygon the input polygon pointer
	//! \return the index referring to pPolygon
	int GetPolygonIndex(PolygonPtr pPolygon) const;

	//! Sets the current shelf.
	//! \param shelfID the shelf ID. Only 0 or 1 is available as shelfID.
	void SetShelf(ShelfID shelfID) const;

	//! Returns the current shelfID.
	//! \return the current shelfID
	ShelfID GetShelf() const { return m_ShelfID; }

	//! Returns the polygon count in the current shelf.
	//! \return the polygon count in the current shelf.
	int GetPolygonCount() const { return m_Polygons[m_ShelfID].size(); }

	//! Setter of the model flag. usually combination of items of EModelFlag.
	void SetFlag(int nFlag) { m_Flags = nFlag; }

	//! Getter of the model flag.
	int GetFlag() const { return m_Flags; }

	//! Adds a new model flag.
	void AddFlag(int nFlag) { m_Flags |= nFlag; }

	//! Checks if the input flags are set or not.
	bool CheckFlag(int nFlags) { return m_Flags & nFlags ? true : false; }

	//! Setter of the subdivision level.
	void SetSubdivisionLevel(unsigned char nLevel) { m_SubdivisionLevel = nLevel; }

	//! Getter of the subdivision level.
	unsigned char GetSubdivisionLevel() const { return m_SubdivisionLevel; }

	//! Returns the model DB instance.
	ModelDB* GetDB() const { return m_pDB; }

	//! Refresh the Model DB instance based on this model
	void ResetDB(int nFlag, ShelfID nValidShelfID = eShelf_Any);

	//! Returns the smoothing group manager instance
	SmoothingGroupManager* GetSmoothingGroupMgr() const { return m_pSmoothingGroupMgr; }

	//! Returns the edge sharpness manager instance
	EdgeSharpnessManager* GetEdgeSharpnessMgr() const { return m_pEdgeSharpnessMgr; }

	//! Returns the uv island manager instance
	UVIslandManager* GetUVIslandMgr() const { return m_pUVIslandMgr; }

	//! Sets the halfEdgeMesh instance with the subdivision result.
	void SetSubdivisionResult(HalfEdgeMesh* pSubdividedHalfMesh);

	//! Returns the halfEdgeMesh with the subdivision result.
	HalfEdgeMesh* GetSubdivisionResult() const { return m_pSubdividionResult; }

	//! Sets whether smoothing surfaces to the subdivided mesh is applied or not.
	void EnableSmoothingSurfaceForSubdividedMesh(bool bEnabled) { m_bSmoothingGroupForSubdividedSurfaces = bEnabled; }

	//! Gets whether smoothing surfaces to the subdivided mesh is applied or not.
	bool IsSmoothingSurfaceForSubdividedMeshEnabled() const { return m_bSmoothingGroupForSubdividedSurfaces; }

	//! Setter of the mirror plane
	void SetMirrorPlane(const BrushPlane& mirrorPlane) { m_MirrorPlane = mirrorPlane; }

	//! Getter of the mirror plane
	const BrushPlane& GetMirrorPlane() const { return m_MirrorPlane; }

	//! All polygons in the current shelf are removed and the shelf are filled with polygons in the input polyon list.
	//! When filled, Union operation is applied.
	//! \param polygonList input polygons
	void ResetAllPolygonsFromList(const std::vector<PolygonPtr>& polygonList);

	//! Assigns the input sub material ID to all polygons in the current shelf
	void SetSubMatID(int nSubMatID);

	//! Checks if the specific shelf is empty or not. If nShelf is any, Checks if both shelves are empty or not.
	//! \param nShelf the input shelf ID
	bool IsEmpty(ShelfID nShelf = eShelf_Any) const;

	//! Checks if the specific shelf has a closed polygon. If nShelf is any, Checks if either shelf has a closed polygon.
	//! \param nShelf the input shelf ID
	bool HasClosedPolygon(ShelfID nShelf = eShelf_Any) const;

	//! Returns AABB surrounding all polygons in the shelf referred by nShelf. If nShelf is any
	//! AABB surrounding all polygons in the both shelves is returned.
	//! \param nShelf the input shelf. any means the both shelves
	AABB GetBoundBox(ShelfID nShelf = eShelf_Any);

	//! Invalidate AABB referred by nShelf so that the AABB will be updated as needed to have the up-to-date aabb
	//! \param nShelf the input shelf. any means the both shelves
	void InvalidateAABB(ShelfID nShelf = eShelf_Any) const;

	//! Invalidate the whole smoothing groups so that they are updated out of the latest data as needed to have the right renderable mesh from the up-to-date smoothing groups
	void InvalidateSmoothingGroups() const;

public://! Clipping by plane and 3D Boolean Operations

	//! Clips polygons in the current shelf by the input clip plane.
	EClipPolygonResult Clip(
	  const BrushPlane& clipPlane,
	  bool bFillFacet,
	  _smart_ptr<Model>& pOutFrontPart,
	  _smart_ptr<Model>& pOutBackPart) const;
	//! Union boolean operation to BModel.
	void Union(Model* BModel);
	//! Subtraction boolean operation to BModel.
	void Subtract(Model* BModel);
	//! Intersection boolean operation to BModel.
	void Intersect(Model* BModel);
	//! Clips the polygons outside BModel. The parts inside BModel will be left.
	void ClipOutside(Model* BModel);
	//! Clips the polygons inside BModel.The parts outside BModel will be left.
	void ClipInside(Model* BModel);
	//! Returns if vPos is inside this model or not.
	bool IsInside(const BrushVec3& vPos) const;

private:
	void Init();

	enum ESurroundType
	{
		eSurroundType_SurroundedByOtherPolygons,
		eSurroundType_SurroundsOtherPolygons,
		eSurroundType_SurroundedPartly,
		eSurroundType_None
	};
	ESurroundType QuerySurroundType(PolygonPtr polygon) const;

	bool          QueryNearestEdge(
	  int nPolygonIndex,
	  const BrushRay& ray,
	  BrushVec3& outPos,
	  BrushVec3& outPosOnEdge,
	  BrushPlane& outPlane,
	  BrushEdge3D& outEdge) const;

	bool QueryNearestEdge(
	  int nPolygonIndex,
	  const BrushVec3& position,
	  BrushVec3& outPosOnEdge,
	  BrushPlane& outPlane,
	  BrushEdge3D& outEdge) const;

	bool SplitPolygonsByOpenPolygon(PolygonPtr pOpenPolygon);
	bool GetPolygonPlane(
	  int nPolygonIndex,
	  BrushPlane& outPlane) const;
	void       DeletePolygons(std::set<PolygonPtr>& deletedPolygons);
	PolygonPtr GetPolygonPtr(int nIndex) const;
	bool       GetVisibleEdge(
	  const BrushEdge3D& edge,
	  const BrushPlane& plane,
	  std::vector<BrushEdge3D>& outVisibleEdges) const;

	void AddPolygonAfterResetingUV(
	  int nShelf,
	  PolygonPtr pPolygon);

	//! Adds islands to the current shelf, which are separate parts in a polygon.
	//! Usually a polygon needs to consist of only an island.
	void AddIslands(
	  PolygonPtr pPolygon,
	  bool bAddedOnlyIslands = false,
	  bool bResetUV = true);

	void Clip(
	  const BSPTree3D* pTree,
	  PolygonList& polygonList,
	  EClipType cliptype,
	  PolygonList& outPolygons,
	  EClipObjective clipObjective);

	std::vector<PolygonPtr> GetIntersectedParts(PolygonPtr pPolygon) const;
	PolygonPtr              QueryEquivalentPolygon(
	  int nShelfID,
	  PolygonPtr pPolygon,
	  int* pOutPolygonIndex = NULL) const;
	static bool GeneratePolygonsFromEdgeList(
	  std::vector<BrushEdge3D>& edgeList,
	  const BrushPlane& plane,
	  PolygonList& outPolygons);

private:
	PolygonList            m_Polygons[cShelfMax];
	mutable ModelBound     m_BoundBox[cShelfMax];
	BrushPlane             m_MirrorPlane;
	unsigned char          m_SubdivisionLevel;
	bool                   m_bSmoothingGroupForSubdividedSurfaces;
	int                    m_Flags;
	mutable ShelfID        m_ShelfID;

	ModelDB*               m_pDB;
	SmoothingGroupManager* m_pSmoothingGroupMgr;
	EdgeSharpnessManager*  m_pEdgeSharpnessMgr;
	UVIslandManager*       m_pUVIslandMgr;
	HalfEdgeMesh*          m_pSubdividionResult;
};

class CShelfIDReconstructor
{
public:
	CShelfIDReconstructor(Model* pModel) : m_pModel(pModel)
	{
		if (m_pModel)
			m_ShelfID = m_pModel->GetShelf();
	}

	CShelfIDReconstructor(Model* pModel, ShelfID shelfID) : m_pModel(pModel), m_ShelfID(shelfID)
	{
	}

	~CShelfIDReconstructor()
	{
		if (m_pModel)
			m_pModel->SetShelf(m_ShelfID);
	}

private:
	Model*  m_pModel;
	ShelfID m_ShelfID;
};
}

#define MODEL_SHELF_RECONSTRUCTOR_POSTFIX(pModel, nPostFix) Designer::CShelfIDReconstructor shelfIDReconstructor ## nPostFix(pModel);
#define MODEL_SHELF_RECONSTRUCTOR(pModel)                   MODEL_SHELF_RECONSTRUCTOR_POSTFIX(pModel, 0);

