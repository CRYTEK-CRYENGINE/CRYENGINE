// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
class BSPTree2D;
class Convexes;
class LoopPolygons;

//! An enum that lists flags for defining the polygon's characteristics.
//! Used as the input parameter and the return value of Polygon::CheckFlags(), Polygon::AddFlags(), Polygon::SetFlag(), Polygon::RemoveFlag() and Polygon::GetFlag()
enum EPolygonFlag
{
	ePolyFlag_Mirrored      = BIT(1),
	ePolyFlag_Hidden        = BIT(4),
	ePolyFlag_NonplanarQuad = BIT(5),
	ePolyFlag_All           = 0xFFFFFFFF
};

//! Used as the parameter in Polygon::Intersect method to define if the co-linear edges touching each other are included or not according to their directions.
enum EIncludeCoEdgeInIntersecting
{
	//! The two edges touching each other are the same direction.
	eICEII_IncludeCoSame       = BIT(0),
	//! The two edges touching each other are the different direction.
	eICEII_IncludeCoDiff       = BIT(1),
	eICEII_IncludeBothBoundary = eICEII_IncludeCoSame | eICEII_IncludeCoDiff
};

//! Used as the return value in Polygon::TrackVertices method.
enum ETrackingResult
{
	//! The tracking fails.
	eTrackingResult_Fail,
	//! The tracking finishes at the end edge
	eTrackingResult_FinishAtEndEdge,
	//! The tracking finishes at the start edge.
	eTrackingResult_FinishAtStartEdge
};

//! A polygon in CryDesigner means both a non-closed path and a simple polygon, which is a flat shape consisting of straight, non-intersecting line segments that are joined pair-wise to form a closed path
//! and can have holes inside. A polygon is a basic unit made up by vertices and edges for manipulating geometry.
class Polygon : public _i_reference_target_t
{
public:
	typedef _smart_ptr<Polygon> PolygonPtr;

public:
	//! Constructors
	Polygon();
	Polygon(const Polygon& polygon);
	Polygon(
	  const std::vector<BrushVec3>& vertices,
	  const std::vector<IndexPair>& edges);
	Polygon(const std::vector<BrushVec3>& vertices);
	Polygon(const std::vector<Vertex>& vertices);
	Polygon(
	  const std::vector<BrushVec2>& points,
	  const BrushPlane& plane,
	  int subMatID,
	  const TexInfo* pTexInfo,
	  bool bClosed);
	Polygon(
	  const std::vector<BrushVec3>& vertices,
	  const std::vector<IndexPair>& edges,
	  const BrushPlane& plane,
	  int subMatID,
	  const TexInfo* pTexInfo,
	  bool bOptimizePolygon = true);
	Polygon(
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges,
	  const BrushPlane& plane,
	  int subMatID,
	  const TexInfo* pTexInfo,
	  bool bOptimizePolygon = true);
	Polygon(
	  const std::vector<BrushVec3>& vertices,
	  const BrushPlane& plane,
	  int subMatID,
	  const TexInfo* pTexInfo,
	  bool bClosed);
	Polygon(
	  const std::vector<Vertex>& vertices,
	  const BrushPlane& plane,
	  int subMatID,
	  const TexInfo* pTexInfo,
	  bool bClosed);

	//! Destructor
	~Polygon();

	void Init();

	//! Serializes all data of this polygon using base64 encoder and decoder.
	void Serialize(
	  XmlNodeRef& xmlNode,
	  bool bLoading,
	  bool bUndo);

	//! Copies the whole data of the input polygon even including the guid.
	Polygon& operator=(const Polygon& polygon);

	//! Clones this polygon and returns it. With this the guid is re-generated.
	PolygonPtr Clone() const;

	//! All the vertices and edges are cleared.
	void Clear();

	//! Checks if the ray pass this polygon.
	//! \param ray the input ray
	//! \outT the distance from the ray origin to the intersection of this polygon
	//! \return true is returned if the ray passes this polygon
	bool IsPassed(const BrushRay& ray, BrushFloat& outT);

	//! Checks if this polygon's boundary encompasses all the edges in the input polygon.
	//! \param pPolygon the input polygon
	//! \return if all the edges of input polygon are enclosed by this polygon, true is returned.
	bool IncludeAllEdges(PolygonPtr pPolygon) const;

	//! Checks if the input position is included by this polygon.
	//! \param position the input position
	//! \return if the input position is included by this polygon, true is returned.
	bool Include(const BrushVec3& position) const;

	//! Checks if the aabb of this polygon is intersected by the input AABB
	//! This method is good for checking the intersection roughly.
	//! \param aabb the input AABB
	//! \return if the input aabb intersects the aabb of this polygon, true is returned.
	bool IntersectedBetweenAABBs(const AABB& aabb) const;

	//! Checks if the polygon made up of the input vertices and edges is open polygon or not.
	//! \param vertices the vertices belonging to the polygon
	//! \param edges the edges belonging to the edges
	//! \param if the polygon made up of the input vertices and edges is open, true is returned.
	bool IsOpen(
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges) const;

	//! Checks if this polygon is open or not.
	bool IsOpen() const { return CheckPrivateFlags(eRPF_Open); }

	//! Checks if the input edge is overlapped by the boundary of the input polygon
	//! \param edge the input edge
	//! \param pOutEdgeIndex the index of the edge overlapped by the input edge.
	//! \param pOutIntersectedEdge the edge referred by pOutEdgeIndex
	//! \return if the input edge is overlapped by the edges of this polygon, return is returned.
	bool IsEdgeOverlappedByBoundary(
	  const BrushEdge3D& edge,
	  int* pOutEdgeIndex = NULL,
	  BrushEdge3D* pOutIntersectedEdge = NULL) const;

	//! Checks if the input polygon is made up of the same vertices and edges of this polygon
	//! \param pPolygon the input polygon
	//! \return if the input polygon is equivalent with this polygon, true is returned.
	bool IsEquivalent(const PolygonPtr& pPolygon) const;

	//! Subtracts the input edge from edges of this polygon.
	//! The candidate edges to be subtracted are co-linear with the input edge
	//! \param edge the input edge
	//! \param outSubtractedEdges the output subtracted edges
	bool SubtractEdge(
	  const BrushEdge3D& edge,
	  std::vector<BrushEdge3D>& outSubtractedEdges) const;

	//! Checks if there are the input polygon's edges overlapped by this polygon's edges
	//! \param polygon the input polygon
	//! \return if the edges overlapped by the edges of this polygon exist, true is returned.
	bool HasOverlappedEdges(PolygonPtr polygon) const;

	//! Checks if there are the input polygon's uv edges overlapped by this polygon's uv edges
	//! \param polygon the input polygon
	//! \return if the uv edges overlapped by the uv edges of this polygon exist, true is returned.
	bool HasOverlappedUVEdges(PolygonPtr polygon) const;

	//! Checks if there is the identical edge with the input edge
	//! \param edge the input edge
	//! \param bApplyDir if this is true, the edge's direction will be considered. otherwise the edge's direction won't matter.
	//! \param pOutEdgeIndex the output edge index having the identical starting and ending position with the input edge.
	//! \param true is returned if the identical edge with the input edge is found.
	bool HasEdge(
	  const BrushEdge3D& edge,
	  bool bApplyDir = false,
	  int* pOutEdgeIndex = NULL) const;

	//! Searches for the intersections between the input edge and the edges of the input polygon.
	//! \param edge the input edge
	//! \param outIntersections the output intersections. key - distance and value - the intersected position.
	//! \return true is returned if the intersected positions are found.
	bool QueryIntersectionsByEdge(
	  const BrushEdge3D& edge,
	  std::map<BrushFloat, BrushVec3>& outIntersections) const;

	//! Searches for intersections between the input plane and all the edges of this polygon.
	//! \param plane the input plane
	//! \param outSortedIntersections the output intersections which are sorted along the intersected line between the input plane and the plane of this polygon.
	//! \return true is returned if the intersected positions are found
	bool QueryIntersectionsByPlane(
	  const BrushPlane& plane,
	  std::vector<BrushEdge3D>& outSortedIntersections) const;

	//! Searches for the closet edge from the input position.
	//! \param the input position
	//! \param outNearestEdge the output closest edge
	//! \param outNearestPos the output position projected to the closest edge from the input position.
	//! \return if the closest position is found, true is returned. otherwise false.
	bool QueryNearestEdge(
	  const BrushVec3& pos,
	  BrushEdge3D& outNearestEdge,
	  BrushVec3& outNearestPos) const;

	//! Searches for the closest position from the input position.
	//! \param pos the input position
	//! \param outNearestPos the closest position on the edges of this polygon from the input position.
	bool QueryNearestPos(
	  const BrushVec3& pos,
	  BrushVec3& outNearestPos) const;

	//! Searches for edges sharing the input position.
	//! \param pos the input position
	//! \param outEdgeIndices the indices of the edge sharing the input position
	//! \return true is returned if the edges are found.
	bool QueryEdgesWithPos(
	  const BrushVec3& pos,
	  std::vector<int>& outEdgeIndices) const;

	//! Searches for the edges aligned with the main axis(X and y axises);
	//! \param outLines the output axis aligned lines from the edges.
	//! \output if the axis aligned edges exist, true is returned.
	bool QueryAxisAlignedLines(std::vector<BrushLine>& outLines);

	//! Searches for intersected edges by the 2D rect.
	//! \param pView viewport instance
	//! \param worldTM world transform matrix
	//! \param pRectPolygon the rect polygon on screen space.
	//! \param bExcludeBackFace whether true is returned when this polygon is intersected but the back side of this polygon is towards the camera.
	//! \param outintserectionEdges the output intersected edges.
	void QueryIntersectionEdgesWith2DRect(
	  IDisplayViewport* pView,
	  const BrushMatrix34& worldTM,
	  PolygonPtr pRectPolygon,
	  bool bExcludeBackFace,
	  EdgeQueryResult& outIntersectionEdges);

	//! Applies the union boolean operation to the input polygon.
	//! The input polygon and this polygon should have the same plane.
	//! \param BPolyon the input polygon
	//! \return true is returned if the boolean operation succeeds.
	bool Union(PolygonPtr BPolygon);

	//! Applies the subtraction boolean operation to the input polygon.
	//! The input polygon and this polygon should have the same plane.
	//! \param BPolyon the input polygon
	//! \return true is returned if the boolean operation succeeds.
	bool Subtract(PolygonPtr BPolygon);

	//! Applies the XOR boolean operation to the input polygon.
	//! The input polygon and this polygon should have the same plane.
	//! \param BPolyon the input polygon
	//! \return true is returned if the boolean operation succeeds.
	bool ExclusiveOR(PolygonPtr BPolygon);

	//! Applies the intersection boolean operation to the input polygon.
	//! The input polygon and this polygon should have the same plane.
	//! \param BPolyon the input polygon
	//! \param includeCoEdgeFlags how to deal with the edges touched by each other
	//! \return true is returned if the boolean operation succeeds.
	bool Intersect(
	  PolygonPtr BPolygon,
	  uint8 includeCoEdgeFlags = eICEII_IncludeBothBoundary);

	//! Clips the inside parts of BPolygon
	//! The input polygon and this polygon should have the same plane.
	//! \param BPolygon the input polygon
	//! \return true is returned if the clip operation is successful.
	bool ClipInside(PolygonPtr BPolygon);

	//! Clips the outside parts of BPolygon
	//! The input polygon and this polygon should have the same plane.
	//! \param BPolygon the input polygon
	//! \return true is returned if the clip operation is successful.
	bool ClipOutside(PolygonPtr BPolygon);

	//! Removes all holes of this polygon
	//! \return this polygon without holes.
	PolygonPtr RemoveAllHoles();

	//! Flips this polygon by reversing all edges and flipping the plane.
	//! \return this polygon after flipped.
	PolygonPtr Flip();

	//! Reverses all the edges.
	void ReverseEdges();

	//! Attaches the input polygon to this polygon
	//! \param pPolygon the input polygon
	bool Attach(PolygonPtr pPolygon);

	//! Returns the closest distance from the plane of this plane to the position of each vertex of the input polygon.
	//! \pPolygon the input polygon
	//! \direction the projection direction from each position to the plane of this polygon to calculate the distance.
	//! \return the closest distance from the plane of this plane to the position of each vertex of the input polygon.
	BrushFloat GetNearestDistance(
	  PolygonPtr pPolygon,
	  const BrushVec3& direction) const;

	//! Divides this polygon by the edge referred by the input edge index.
	//! \param nEdgeIndex the input edge index
	//! \param outDividedPolygons the output divided polygons
	void DivideByEdge(
	  int nEdgeIndex,
	  std::vector<PolygonPtr>& outDividedPolygons) const;

	//! Returns the bound box encompassing this polygon.
	const AABB& GetBoundBox() const;

	//! Returns the bound box encompassing this polygon to which applied the input offset.
	//! \param offset the input offset.
	AABB GetExpandedBoundBox(float offset = 0.001f) const;

	//! Returns the radius of the bound circle of this polygon.
	BrushFloat GetRadius() const;

	//! If the orientation of this polygon isn't CCW, reverse all the edges to make the orientation CCW.
	void ModifyOrientation();

	//! If both the edges and the vertices are not empty, true is returned. Otherwise false.
	bool IsValid() const { return !m_Vertices.empty() && !m_Edges.empty(); }

	//! Returns the plane of this polygon.
	const BrushPlane& GetPlane() const { return m_Plane; }

	//! Sets the plane of this polygon
	void SetPlane(const BrushPlane& plane) { m_Plane = plane; }

	//! Replaces the existing plane with the input plane.
	//! The normal of the input plane is set as the projection direction .
	//! \param plane the input plane
	//! \return true is returned if the plane update is done. Otherwise false.
	bool UpdatePlane(const BrushPlane& plane)
	{
		return UpdatePlane(plane, -plane.Normal());
	}

	//! Replaces the existing plane with the input plane.
	//! The positions of the existing vertices are projected to the input plane.
	//! \param plane the input plane
	//! \param directionForHitTest the projection direction.
	//! \return true is returned if the plane update is done. Otherwise false.
	bool UpdatePlane(
	  const BrushPlane& plane,
	  const BrushVec3& projectionDir);

	//! Sets the sub material ID
	//! \param the sub material ID
	void SetSubMatID(int nSubMatID);

	//! Returns the sub material ID.
	//! \return the current sub material ID
	int GetSubMatID() const { return m_SubMatID; }

	//! Returns the texture info instance.
	//! \return the texture info
	const TexInfo& GetTexInfo() const { return m_TexInfo; }

	//! Sets the texture info.
	//! \return texInfo the new texture info instance.
	void SetTexInfo(const TexInfo& texInfo);

	//! If this polygon is a convex hull, true is returned. Otherwise false.
	bool IsConvex() const { return CheckPrivateFlags(eRPF_ConvexHull); }

	//! The edge's count
	int GetEdgeCount() const { return m_Edges.size(); }

	//! Returns the edge referred by the input edge index.
	//! \param nEdgeIndex the input edge index.
	//! \return the output edge referred by the input edge index.
	BrushEdge3D GetEdge(int nEdgeIndex) const;

	//! Returns the indices of the edges sharing the vertex referred by the input vertex index.
	//! \param nVertexIndex the input vertex index.
	//! \param outEdgeIndices the output indices of the edges
	//! \return if the edges are found, true is returned. Otherwise false.
	bool GetEdgesByVertexIndex(
	  int nVertexIndex,
	  std::vector<int>& outEdgeIndices) const;

	//! Returns the edge index of the edge whose vertices's indexes are same as the two input vertex indices.
	//! \param nVertexIndex0 the first vertex index of the edge
	//! \param nVertexIndex1 the second vertex index of the edge
	int GetEdgeIndex(
	  int nVertexIndex0,
	  int nVertexIndex1) const;

	//! Returns the edge index referring to the input edge
	//! \param edge3D the input edge
	//! \return the edge index referring to the input edge. If the edge doesn't exist, -1 is returned.
	int GetEdgeIndex(const BrushEdge3D& edge3D) const;

	//! Returns the edge index pair referred by the input index
	const IndexPair& GetEdgeIndexPair(int nEdgeIndex) const { return m_Edges[nEdgeIndex]; }

	//! Returns the adjacent edges to the edge referred by the input edge index.
	//! \param nEdgeIndex the input edge index
	//! \param pOutPrevEdgeIndex the output index of the previous edge, or -1 if there is no previous edge.
	//! \param pOutNextEdgeIndex the output index of the next edge, or -1 if there is no next edge.
	//! \return true if any of the edges are found, otherwise false.
	bool GetAdjacentEdgesByEdgeIndex(
	  int nEdgeIndex,
	  int* pOutPrevEdgeIndex,
	  int* pOutNextEdgeIndex) const;

	//! Returns the edges sharing the vertex referred by the input index.
	//! \param nVertexIndex the input vertex index
	//! \param pOutPrevEdgeIndex the output previous edge index of the vertex, or -1 if there is no previous edge.
	//! \param pOutNextEdgeIndex the output next edge index of the vertex, or -1 if there is no next edge.
	//! \return true if any of the edges are found, otherwise false.
	bool GetAdjacentEdgesByVertexIndex(
	  int nVertexIndex,
	  int* pOutPrevEdgeIndex,
	  int* pOutNextEdgeIndex) const;

	//! Returns the center position of the bound box encompassing this polygon.
	BrushVec3 GetCenterPosition() const;

	//! Returns the average position of the all vertices.
	BrushVec3 GetAveragePosition() const;

	//! Returns the representative position of the polygon.
	//! The representative position of the closed polygon is same as the average position.
	//! On the other hand the representative position of the open polygon means the center position of the middle edge.
	BrushVec3 GetRepresentativePosition() const;

	//! Moves each edge by the given scale in a normal direction of it.
	//! \param kScale the input scale
	//! \param bCheckBoundary If this is true, this moves the edges until overlapped edges are found.
	//! \return true is returned if this method succeeds.
	bool Scale(
	  const BrushFloat& kScale,
	  bool bCheckBoundary = false);

	//! The specific vertex broadens in directions of the two connected edges to be a new edge.
	//! \param kScale how much the vertex broadens.
	//! \param nVertexIndex the index referring to the specific vertex.
	//! \param pBaseEdge the helper edge to help the edge broadened.
	bool BroadenVertex(
	  const BrushFloat& kScale,
	  int nVertexIndex,
	  const BrushEdge3D* pBaseEdge = NULL);

	//! Checks if the plane of this polygon is equivalent with the plane of the input polygon.
	bool IsPlaneEquivalent(PolygonPtr pPolygon) const;

	//! Checks if the given flags are set.
	//! see EPolygonFlag enumeration.
	bool CheckFlags(int nFlags) const;

	//! Adds the new flags.
	int AddFlags(int nFlags) { return m_Flag |= nFlags; }

	//! Replaces the existing flag with the new one.
	void SetFlag(int nFlag) { m_Flag = nFlag; }

	//! Removes the specific flags.
	int RemoveFlags(int nFlags) { return m_Flag &= ~nFlags; }

	//! Returns the flag.
	int GetFlag() const { return m_Flag; }

	//! Searches for the index of the vertex having the same position as the input position.
	//! \param pos the input position.
	//! \param nOutIndex the output vertex index.
	//! \return true is returned if the vertex having the input position is found.
	bool GetVertexIndex(
	  const BrushVec3& pos,
	  int& nOutIndex) const;

	//! Searches for the index of the vertex having the same uv as the input uv
	//! \param uv the input uv
	//! \param nOutindex the output vertex index
	//! \return true is returned if the vertex having the input uv is found.
	bool GetVertexIndex(
	  const Vec2& uv,
	  int& nOutIndex) const;

	//! Searches for the index of the vertex whose position is the closest to the input position.
	//! \param position the input position
	//! \param nOutIndex the output vertex index.
	//! \return true is returned if the closest vertex is found.
	bool GetNearestVertexIndex(
	  const BrushVec3& position,
	  int& nOutIndex) const;

	//! Returns the position of the vertex referred by the input index.
	const BrushVec3& GetPos(int nIndex) const { return m_Vertices[nIndex].pos; }

	//! Returns the uv of the vertex referred by the input index.
	const Vec2& GetUV(int nIndex) const { return m_Vertices[nIndex].uv; }

	//! Returns a vertex referred by the input index.
	const Vertex& GetVertex(int nIndex) const { return m_Vertices[nIndex]; }

	//! Returns the number of the vertices.
	int GetVertexCount() const { return m_Vertices.size(); }

	//! Counts the number of the vertices above the given plane.
	int GetVertexCountAbovePlane(const BrushPlane& plane) const;

	//! Replaces the vertex referred by the input index with the given vertex.
	void SetVertex(
	  int nIndex,
	  const Vertex& vertex);

	//! Sets the new position to the new vertex referred by the input index.
	void SetPos(
	  int nIndex,
	  const BrushVec3& pos);

	//! Sets the new UV to the vertex referred by the input index.
	void SetUV(
	  int nIndex,
	  const Vec2& uv);

	//! Inserts the new vertex to the closest edge to split the edge.
	//! \param vertex the input vertex
	//! \param pOutNewVertexIndex the new output vertex index created by inserting the input vertex
	//! \param pOutNewEdgeIndices returned the new edge index created by inserting the input vertex
	//! \return if inserting the vertex succeeds, true is returned.
	bool InsertVertex(
	  const BrushVec3& vertex,
	  int* pOutNewVertexIndex = NULL,
	  EdgeIndexSet* pOutNewEdgeIndices = NULL);

	//! Adds the edge.
	//! \param the input edge
	//! \return the index of the new edge.
	int AddEdge(const BrushEdge3D& edge);

	//! Returns the edge index having the two given vertex indices in order.
	//! \param nVertexIndex0 the first vertex index of the edge.
	//! \param nVertexIndex1 the second vertex index of the edge.
	//! \param nOutEdgeIndex the output edge index with the two vertex indices.
	//! \return true is returned if the edge is found. otherwise false.
	bool GetEdgeIndex(
	  int nVertexIndex0,
	  int nVertexIndex1,
	  int& nOutEdgeIndex) const
	{
		return GetEdgeIndex(m_Edges, nVertexIndex0, nVertexIndex1, nOutEdgeIndex);
	}

	//! Returns the edge index having the two given vertex indices in order in the input edge list.
	//! \param nVertexIndex0 the first vertex index of the edge.
	//! \param nVertexIndex1 the second vertex index of the edge.
	//! \param nOutEdgeIndex the output edge index with the two vertex indices.
	//! \return true is returned if the edge is found. otherwise false.
	static bool GetEdgeIndex(
	  const std::vector<IndexPair>& edgeList,
	  int nVertexIndex0,
	  int nVertexIndex1,
	  int& nOutEdgeIndex);

	//! Returns the next vertex index of the input vertex. if the input vertex
	//! \param nVertexIndex the input vertex index.
	//! \param outVertex the next vertex index of the input vertex index.
	//! \return true is returned if this succeeds.
	bool GetNextVertex(
	  int nVertexIndex,
	  BrushVec3& outVertex) const;

	//! Returns the previous vertex index of the input vertex
	//! \param nVertexIndex the input vertex index.
	//! \param outVertex the previous vertex index of the input vertex index.
	//! \return true is returned if this succeeds.
	bool GetPrevVertex(
	  int nVertexIndex,
	  BrushVec3& outVertex) const;

	//! Returns the list of the vertices making up the outer shell of this polygon.
	//! \param outVertexList the vertex list making up the outer shell of this polygon.
	//! \return true is returned if this method succeeds.
	bool GetLinkedVertices(std::vector<Vertex>& outVertexList) const
	{
		return GetLinkedVertices(outVertexList, m_Vertices, m_Edges);
	}

	//! Optimizes the vertices and edges by removing the duplicated vertices and the vertices on edges something like that.
	void Optimize();

	//! This method is available only if this polygon is open.
	bool IsEndPoint(
	  const BrushVec3& position,
	  bool* bOutFirst = NULL) const;

	//! Translates the positions of the all vertices by the input offset
	//! \param offset the input offset.
	void Translate(const BrushVec3& offset);

	//! Multiplies the input matrix to the positions of all the vertices of this polygon.
	//! The plane is re-calculated and set based on the new transformed positions.
	//! \param tm the input matrix
	void Transform(const Matrix34& tm);

	//! Multiplies the input matrix to the uv positions of all the vertices of this polygon.
	//! \param tm the input matrix.
	void TransformUV(const Matrix33& tm);

	//! This method is available only if the input polygon and this polygon are open types.
	//! And this works when either of the two terminal positions of this polygon is identical with either of the two positions of the input polygon.
	//! \param pPolygon the input polygon
	//! \return if this polygon and the input polygon are concatenated, true is returned. Otherwise false.
	bool Concatenate(PolygonPtr pPolygon);

	//! This method is available only if this polygon is open.
	//! Returns the starting vertex of this open polygon.
	//! \param outVertex the output beginning vertex
	//! \return returns true if the first vertex is found.
	bool GetFirstVertex(BrushVec3& outVertex) const;

	//! This method is available only if this polygon is open.
	//! Returns the ending vertex of this open polygon.
	//! \param outVertex the output ending vertex.
	//! \return returns true if the ending vertex if found.
	bool GetLastVertex(BrushVec3& outVertex) const;

	//! Rearranges the vertex and edge indices so that they are organized in order.
	void Rearrange();

	//! Gets the BSP tree made out of this polygon.
	BSPTree2D* GetBSPTree() const;

	//! Returns the LoopPolygons instance which contains polygons about holes and outer shells made out of this polygon.
	//! \param bOptimizePolygons Sets if the polygons created newly for holes and outer shells are optimized or not.
	//! \return LoopPolygons instance.
	LoopPolygons* GetLoops(bool bOptimizePolygons = true) const;

	//! Is this polygon orientation same as the normal direction of the plane?
	bool IsCCW() const;

	//! Checks if this polygon has holes.
	bool HasHoles() const { return CheckPrivateFlags(eRPF_HasHoles); }

	//! Checks if the input position is on the boundary
	bool IsPositionOnBoundary(const BrushVec3& position) const;

	//! Checks if this polygon has the vertex whose position is same as the input position.
	//! \param pOutVertexIndex the vertex index referring to the vertex whose position is the input position.
	//! \return true is returned if the vertex is found. otherwise false.
	bool HasPosition(const BrushVec3& position, int* pOutVertexIndex = NULL) const;

	//! Is there a bridge edge connecting the outer shell to a hole
	bool HasBridgeEdges() const;

	//! Removes all the bridge edges.
	void RemoveBridgeEdges();

	//! Checks if the two given edges are the bridge edge relation.
	bool IsBridgeEdgeRelation(
	  const IndexPair& edge0,
	  const IndexPair& edge1) const;

	//! Removes the overlapped parts by the input edge.
	//! \param edge the input edge.
	void RemoveEdge(const BrushEdge3D& edge);

	//! Removes the edge referred by the input edge index
	//! \param nEdgeIndex the input index
	void RemoveEdge(int nEdgeIndex);

	//! Searches for the isolated paths
	//! This method works as expected if this polygon is open.
	//! If this polygon is closed, this polygon is returned.
	//! \param outIsolatedPaths the output isolated path polygons.
	void GetIsolatedPaths(std::vector<PolygonPtr>& outIsolatedPaths);

	//! Tracks the vertices from the edge referred by nStartEdgeIdx until it meets one of the edges referred by nStartEdgeIdx and nEndEdgeIdx.
	//! \param nStartEdgeIdx the first input edge index
	//! \param nEndEdgeIdx the second input edge index
	//! \param outVertexList the output vertex position list
	//! \param pOutVertexIndices the output index list of the vertices along the track.
	//! \return how the tracking ends.
	ETrackingResult TrackVertices(
	  int nStartEdgeIdx,
	  int nEndEdgeIdx,
	  std::vector<BrushVec3>& outVertexList,
	  std::vector<int>* pOutVertexIndices = NULL) const;

	//! Checks if this polygon is intersected by the 2D rectangle on screen space.
	//! \param pView the viewport instance
	//! \param worldTM the world matrix
	//! \param pRectPolygon the polygon with 2D rect on screen space.
	//! \param bExcludeBackFace if this is true and this polygon's back face is toward the camera, this intersection is failed.
	//! \return true is returned if the intersection is succeeded.
	bool IsIntersectedBy2DRect(
	  IDisplayViewport* pView,
	  const BrushMatrix34& worldTM,
	  PolygonPtr pRectPolygon,
	  bool bExcludeBackFace);

	//! Checks if the given two polygons are intersected by each other.
	//! \param pPolygon0 the first input polygon
	//! \param pPolygon1 the second input polygon
	//! \return How the given two polygons are intersected.
	static EIntersectionType HasIntersection(
	  PolygonPtr pPolygon0,
	  PolygonPtr pPolygon1);

	//! Returns the guid of this polygon
	CryGUID GetGUID() const { return m_GUID; }

	//! Clips this polygon by the input polygon
	//! \param clipPlane the input plane for clipping
	//! \param pOutFrontPolygons the output polygons made out of the parts in front of the input clip plane.
	//! \param pOutBackPolygons the output polygons made out of the parts behind the input clip plane.
	//! \param pOutBoundaryEdges the output border edges between the front parts and the back parts.
	//! \return true is returned if the clipping is succeeded. otherwise false.
	bool ClipByPlane(
	  const BrushPlane& clipPlane,
	  std::vector<PolygonPtr>& pOutFrontPolygons,
	  std::vector<PolygonPtr>& pOutBackPolygons,
	  std::vector<BrushEdge3D>* pOutBoundaryEdges = NULL) const;

	//! Mirrors this polygon by the input plane.
	//! \return The mirrored polygon is returned.
	PolygonPtr Mirror(const BrushPlane& mirrorPlane);

	//! Calculates the plane based on the vertices and edges of this polygon and returns it.
	//! The plane returned by this method can be different from the plane already set to this polygon.
	//! \param outPlane the newly calculated plane.
	//! \return if the plane is calculated well, true is returned.
	bool GetComputedPlane(BrushPlane& outPlane) const;

	//! Chooses the previous edge among the candidates connected to the input edge.
	//! \param edge the input edge.
	//! \param candidateSecondIndices the indices of the candidate edges connected to the input edge.
	//! \return the index of the previous edge.
	int ChoosePrevEdge(
	  const IndexPair& edge,
	  const EdgeIndexSet& candidateSecondIndices) const;

	//! Chooses the next edge among the candidates connected to the input edge.
	//! \param edge the input edge
	//! \param candidateSecondIndices the indices of the candidate edges connected to the input edge.
	//! \return the index of the next edge.
	int ChooseNextEdge(
	  const IndexPair& edge,
	  const EdgeIndexSet& candidateSecondIndices) const;

	//! Returns the Convexes instance which contains the convex hulls decomposed out of this polygon.
	Convexes* GetConvexes();

	//! Returns the FlexibleMesh instance which contains the triangulated result of this polygon.
	//! \param bCreateBackFaces If this is true, triangles of the back face are included.
	//! \return the FlexibleMesh instance.
	FlexibleMesh* GetTriangles(bool bCreateBackFaces = false) const;

	//! Is this polygon quad?
	bool IsQuad() const { return GetEdgeCount() == 4 && GetVertexCount() == 4 && !IsOpen(); }

	//! Is this polygon a triangle?
	bool IsTriangle() const { return GetEdgeCount() == 3 && GetVertexCount() == 3 && !IsOpen(); }

	//! This method is available only if the count of vertices is 3 or 4.
	//! This returns the opposite edge of the input edge.
	//! \param edge the input edge
	//! \param outEdge the output opposite edge.
	//! \return If the opposite edge is found, true is returned.
	bool FindOppositeEdge(
	  const BrushEdge3D& edge,
	  BrushEdge3D& outEdge) const;

	//! Searches for the edge
	bool FindSharingEdge(
	  PolygonPtr pPolygon,
	  BrushEdge3D& outEdge) const;

	//! each uv of each vertex is calculated based on the normal and position of it.
	void ResetUVs();

private:
	enum ESearchDirection
	{
		eSD_Previous,
		eSD_Next,
	};

	enum EPolygonPrivateFlag
	{
		eRPF_Open             = BIT(0),
		eRPF_ConvexHull       = BIT(1),
		eRPF_HasHoles         = BIT(2),
		eRPF_EnabledBackFaces = BIT(3),
		eRPF_Invalid          = BIT(31)
	};

	struct PolygonBound
	{
		AABB       aabb;
		BrushFloat raidus;
	};

private:
	void SaveBinary(std::vector<char>& buffer);
	int  LoadBinary(std::vector<char>& buffer);
	void SaveUVBinary(std::vector<char>& buffer);
	void LoadUVBinary(
	  std::vector<char>& buffer,
	  int offset);
	void Reset(
	  const std::vector<BrushVec3>& vertices,
	  const std::vector<IndexPair>& edgeList);
	void Reset(
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edgeList);
	void Reset(const std::vector<BrushVec3>& vertices);
	void Reset(const std::vector<Vertex>& vertices);
	bool QueryEdgesContainingVertex(
	  const BrushVec3& vertex,
	  std::vector<int>& outEdgeIndices) const;
	bool BuildBSP() const;
	bool OptimizeEdges(
	  std::vector<Vertex>& vertices,
	  std::vector<IndexPair>& edges) const;
	void OptimizeVertices(
	  std::vector<Vertex>& vertices,
	  std::vector<IndexPair>& edges) const;
	void Transform2Vertices(
	  const std::vector<BrushVec2>& points,
	  const BrushPlane& plane);
	EIntersectionType HasIntersection(PolygonPtr pPolygon);
	void              Clip(
	  const BSPTree2D* pTree,
	  EClipType cliptype,
	  std::vector<Vertex>& vertices,
	  std::vector<IndexPair>& edges,
	  EClipObjective clipObjective,
	  unsigned char vertexID) const;
	bool Optimize(std::vector<Vertex>& vertices, std::vector<IndexPair>& edges);
	bool Optimize(const std::vector<BrushEdge3D>& edges);
	bool GetAdjacentPrevEdgeIndicesWithVertexIndex(
	  int vertexIndex,
	  std::vector<int>& outEdgeIndices,
	  const std::vector<IndexPair>& edges) const;
	bool GetAdjacentNextEdgeIndicesWithVertexIndex(
	  int vertexIndex,
	  std::vector<int>& outEdgeIndices,
	  const std::vector<IndexPair>& edges) const;
	bool GetAdjacentEdgeIndexWithEdgeIndex(
	  int edgeIndex,
	  int& outPrevIndex,
	  int& outNextIndex,
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges) const;
	BrushLine GetLineFromEdge(
	  int edgeIndex,
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges) const
	{
		return BrushLine(m_Plane.W2P(vertices[edges[edgeIndex].m_i[0]].pos), m_Plane.W2P(vertices[edges[edgeIndex].m_i[1]].pos));
	}
	bool GetLinkedVertices(
	  std::vector<Vertex>& outVertexList,
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges) const;
	void SearchLinkedColinearEdges(
	  int edgeIndex,
	  ESearchDirection direction,
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges,
	  std::vector<int>& outLinkedEdges) const;
	void SearchLinkedEdges(
	  int edgeIndex,
	  ESearchDirection direction,
	  const std::vector<Vertex>& vertices,
	  const std::vector<IndexPair>& edges,
	  std::vector<int>& outLinkedEdges) const;
	static bool DoesIdenticalEdgeExist(
	  const std::vector<IndexPair>& edges,
	  const IndexPair& e, int* pOutIndex = NULL);
	static bool DoesReverseEdgeExist(
	  const std::vector<IndexPair>& edges,
	  const IndexPair& e,
	  int* pOutIndex = NULL);
	void Load(
	  XmlNodeRef& xmlNode,
	  bool bUndo);
	void Save(
	  XmlNodeRef& xmlNode,
	  bool bUndo);
	static void CopyEdges(
	  const std::vector<IndexPair>& sourceEdges,
	  std::vector<IndexPair>& destincationEdges);
	void InitializeEdgesAndUpdate(bool bClosed);
	bool FlattenEdges(
	  std::vector<Vertex>& vertices,
	  std::vector<IndexPair>& edges) const;
	static void RemoveEdgesHavingSameIndices(std::vector<IndexPair>& edges);
	void        RemoveEdgesRegardedAsVertex(
	  std::vector<Vertex>& vertices,
	  std::vector<IndexPair>& edges) const;
	static BrushEdge ToEdge2D(
	  const BrushPlane& plane,
	  const BrushEdge3D& edge3D)
	{
		return BrushEdge(plane.W2P(edge3D.m_v[0]), plane.W2P(edge3D.m_v[1]));
	}
	bool ShouldOrderReverse(PolygonPtr BPolygon) const;
	void InvalidateCacheData() const;
	void InitForCreation(
	  const BrushPlane& plane = BrushPlane(BrushVec3(0, 0, 0), 0),
	  int matID = 0);
	BrushFloat Cosine(
	  int i0,
	  int i1,
	  int i2,
	  const std::vector<Vertex>& vertices) const;
	bool IsCCW(
	  int i0,
	  int i1,
	  int i2,
	  const std::vector<Vertex>& vertices) const;
	bool IsCW(
	  int i0,
	  int i1,
	  int i2,
	  const std::vector<Vertex>& vertices) const
	{
		return !IsCCW(i0, i1, i2, vertices);
	}
	void ChoosePrevEdge(
	  const std::vector<Vertex>& vertices,
	  const IndexPair& edge,
	  const EdgeIndexSet& candidateSecondIndices,
	  IndexPair& outEdge) const;
	void ChooseNextEdge(
	  const std::vector<Vertex>& vertices,
	  const IndexPair& edge,
	  const EdgeIndexSet& candidateSecondIndices,
	  IndexPair& outEdge) const;
	bool FindFirstEdgeIndex(
	  const std::set<int>& edgeSet,
	  int& outEdgeIndex) const;
	bool ExtractEdge3DList(std::vector<BrushEdge3D>& outList) const;
	bool SortVerticesAlongCrossLine(
	  std::vector<BrushVec3>& vList,
	  const BrushLine3D& crossLine,
	  std::vector<BrushEdge3D>& outGapEdges) const;
	bool SortEdgesAlongCrossLine(
	  std::vector<BrushEdge3D>& edgeList,
	  const BrushLine3D& crossLine,
	  std::vector<BrushEdge3D>& outGapEdges) const;
	bool QueryEdges(
	  const BrushVec3& vertex,
	  int vIndexInEdge,
	  std::set<int>* pOutEdgeIndices = NULL) const;
	void GetBridgeEdgeSet(
	  std::set<IndexPair>& outBridgeEdgeSet,
	  bool bOutputBothDirection) const;
	void UpdateBoundBox() const;
	void ConnectNearVertices(
	  std::vector<Vertex>& vertices,
	  std::vector<IndexPair>& edges) const;
	void      RemoveUnconnectedEdges(std::vector<IndexPair>& edges) const;
	void      UpdatePrivateFlags() const;
	BrushEdge GetEdge2D(int nEdgeIndex) const;

private:
	void AddVertex_Basic(const Vertex& vertex)
	{
		InvalidateCacheData();
		m_Vertices.push_back(vertex);
	}
	void SetVertex_Basic(int nIndex, const Vertex& vertex)
	{
		InvalidateCacheData();
		m_Vertices[nIndex] = vertex;
	}
	void SetPos_Basic(int nIndex, const BrushVec3& pos)
	{
		InvalidateCacheData();
		m_Vertices[nIndex].pos = pos;
	}
	void SetUV_Basic(int nIndex, const Vec2& uv)
	{
		InvalidateCacheData();
		m_Vertices[nIndex].uv = uv;
	}
	void DeleteAllVertices_Basic()
	{
		InvalidateCacheData();
		m_Vertices.clear();
	}
	void SetVertexList_Basic(const std::vector<Vertex>& vertexList)
	{
		InvalidateCacheData();
		if (&vertexList == &m_Vertices)
			return;
		m_Vertices = vertexList;
	}
	void AddEdge_Basic(const IndexPair& edge)
	{
		InvalidateCacheData();
		m_Edges.push_back(edge);
	}
	void DeleteAllEdges_Basic()
	{
		InvalidateCacheData();
		m_Edges.clear();
	}
	void SetEdgeList_Basic(const std::vector<IndexPair>& edgeList)
	{
		InvalidateCacheData();
		CopyEdges(edgeList, m_Edges);
	}
	void SwapEdgeIndex_Basic(int nIndex)
	{
		InvalidateCacheData();
		std::swap(m_Edges[nIndex].m_i[0], m_Edges[nIndex].m_i[1]);
	}
	bool CheckPrivateFlags(int nFlags) const;
	int  AddPrivateFlags(int nFlags) const    { return m_PrivateFlag |= nFlags; }
	int  RemovePrivateFlags(int nFlags) const { return m_PrivateFlag &= ~nFlags; }

	void InvalidateBspTree() const;
	void InvalidateTriangles() const;
	void InvalidateRepresentativePos() const;
	void InvalidateBoundBox() const;
	void InvalidateLoopPolygons() const;

private:
	std::vector<Vertex>    m_Vertices;
	std::vector<IndexPair> m_Edges;
	CryGUID                m_GUID;
	BrushPlane             m_Plane;
	int                    m_SubMatID;
	TexInfo                m_TexInfo;
	unsigned int           m_Flag;

	//! Cache Data
	//! These are the temporary so not saved but created as necessary from the vertex and edge data.
	//! When the vertices or edges are modified, these cache data are invalidated.
	//! And it is deferred as late as possible to validate each cache data until each one is needed.
	mutable BSPTree2D*    m_pBSPTree;
	mutable Convexes*     m_pConvexes;
	mutable FlexibleMesh* m_pTriangles;
	mutable BrushVec3*    m_pRepresentativePos;
	mutable PolygonBound* m_pBoundBox;
	mutable LoopPolygons* m_pLoopPolygons;
	mutable unsigned int  m_PrivateFlag;
};

typedef Polygon::PolygonPtr     PolygonPtr;
typedef std::vector<PolygonPtr> PolygonList;
}

