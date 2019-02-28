// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include "CryMath/Cry_Vector3.h"
#include <Cry3DEngine/CGF/CGFContent.h>

namespace VClothPreProcessUtils {
struct STriInfo;
}

struct AttachmentVClothPreProcessNndc
{
	// Nearest Neighbor Distance Constraints
	int   nndcIdx;        // index of closest constraint (i.e., closest attached vertex)
	float nndcDist;       // distance to closest constraint
	int   nndcNextParent; // index of next parent on path to closest constraint
	AttachmentVClothPreProcessNndc() : nndcIdx(-1), nndcDist(0), nndcNextParent(-1) {}
};

struct SBendTriangle // one triangle which is used for bending forces by neighboring triangle angle
{
	int  p0, p1, p2; // indices of according triangle
	Vec3 normal;
	SBendTriangle(int _p0, int _p1, int _p2) : p0(_p0), p1(_p1), p2(_p2), normal(0) {}
	SBendTriangle() : p0(-1), p1(-1), p2(-1), normal(0) {}
	//std::string toString() { std::stringstream ss; ss << "Bend Triangle: " << p0 << ":" << p1 << ":" << p2; return ss.str(); }
};

struct SBendTrianglePair // a pair of triangles which share one edge and is used for bending forces by neighboring triangle angle
{
	int   p0, p1;     // shared edge
	int   p2;         // first triangle // oriented 0,1,2
	int   p3;         // second triangle // reverse oriented 1,0,3
	int   idxNormal0; // idx of BendTriangle for first triangle
	int   idxNormal1; // idx of BendTriangle for second triangle
	float phi0;       // initial angle
	SBendTrianglePair(int _e0, int _e1, int _p2, int _p3, int _idxNormal0, int _idxNormal1) :
		p0(_e0), p1(_e1), p2(_p2), p3(_p3), idxNormal0(_idxNormal0), idxNormal1(_idxNormal1), phi0(0) {}
	SBendTrianglePair() :
		p0(-1), p1(-1), p2(-1), p3(-1), idxNormal0(-1), idxNormal1(-1), phi0(0) {}
	//std::string toString() { std::stringstream ss; ss << "BendTrianglePair: Edge: " << p0 << ":" << p1 << " VtxTriangles: " << p2 << ":" << p3; return ss.str(); }
};

struct SLink
{
	int   i1, i2;
	float lenSqr, weight1, weight2;
	bool  skip;

	SLink() : i1(0), i2(0), lenSqr(0.0f), weight1(0.f), weight2(0.f), skip(false) {}
};

struct SAttachmentVClothPreProcessData
{
	// Nearest Neighbor Distance Constraints
	std::vector<AttachmentVClothPreProcessNndc> m_nndc;
	std::vector<int>                            m_nndcNotAttachedOrderedIdx;

	// Bending by triangle angles, not springs
	std::vector<SBendTrianglePair> m_listBendTrianglePairs; // triangle pairs sharing an edge
	std::vector<SBendTriangle>     m_listBendTriangles;     // triangles which are used for bending

	// Links
	std::vector<SLink> m_links[eVClothLink_COUNT];
};

struct STopology
{
	int iStartEdge, iEndEdge, iSorted;
	int bFullFan;

	STopology() : iStartEdge(0), iEndEdge(0), iSorted(0), bFullFan(0) {}
};

struct AttachmentVClothPreProcess
{
	bool                                              PreProcess(std::vector<Vec3> const& vtx, std::vector<int> const& idx, std::vector<bool> const& attached);

	std::vector<AttachmentVClothPreProcessNndc> const& GetNndc() const                     { return m_nndc; }
	std::vector<int> const&                           GetNndcNotAttachedOrderedIdx() const { return m_nndcNotAttachedOrderedIdx; }

	std::vector<SBendTrianglePair> const&             GetListBendTrianglePair() const      { return m_listBendTrianglePairs; }
	std::vector<SBendTriangle> const&                 GetListBendTriangle() const          { return m_listBendTriangles; }

	std::vector<SLink> const&                         GetLinksStretch() const              { return m_linksStretch; }
	std::vector<SLink> const&                         GetLinksShear() const                { return m_linksShear; }
	std::vector<SLink> const&                         GetLinksBend() const                 { return m_linksBend; }
	std::vector<SLink> const&                         GetLinks(int idx) const
	{
		switch (idx)
		{
		default:
		case 0:
			return GetLinksStretch();
		case 1:
			return GetLinksShear();
		case 2:
			return GetLinksBend();
		}
	}

	// helpers which are used in the RC as well as in the engine
	static bool IsAttached(float weight);
	static bool PruneWeldedVertices(std::vector<Vec3>& vtx, std::vector<int>& idx, std::vector<bool>& attached);
	static bool RemoveDegeneratedTriangles(std::vector<Vec3>& vtx, std::vector<int>& idx, std::vector<bool>& attached);
	static bool DetermineTriangleNormals(std::vector<Vec3> const& vtx, std::vector<SBendTriangle>& tri);
	static bool DetermineTriangleNormals(int nVertices, strided_pointer<Vec3> const& pVertices, std::vector<SBendTriangle>& tri);

private:

	bool NndcDijkstra(std::vector<Vec3> const& vtx, std::vector<int> const& idx, std::vector<bool> const& attached);
	bool BendByTriangleAngle(std::vector<Vec3> const& vtx, std::vector<int> const& idx, std::vector<bool> const& attached);
	// links
	bool CreateLinks(std::vector<Vec3> const& vtx, std::vector<int> const& idx);
	bool CalculateTopology(std::vector<Vec3> const& vtx, std::vector<int> const& idx, std::vector<VClothPreProcessUtils::STriInfo>& pTopology); // determine neighboring triangles

	// Nearest Neighbor Distance Constraints
	std::vector<AttachmentVClothPreProcessNndc> m_nndc;
	std::vector<int>                            m_nndcNotAttachedOrderedIdx;

	// Bending by triangle angles, not springs
	std::vector<SBendTrianglePair> m_listBendTrianglePairs; // triangle pairs sharing an edge
	std::vector<SBendTriangle>     m_listBendTriangles;     // triangles which are used for bending

	// Links
	std::vector<SLink> m_linksStretch;
	std::vector<SLink> m_linksShear;
	std::vector<SLink> m_linksBend;

};

inline bool AttachmentVClothPreProcess::IsAttached(float weight)
{
	const float attachedThresh = 0.99f; // weights with a value higher than this are handled as weight=1.0f -> attached
	return weight > attachedThresh;
}
