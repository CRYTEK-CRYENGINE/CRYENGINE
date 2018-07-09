// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_H
#define __MNM_H

#pragma once

#include <CryAISystem/IMNM.h>
#include <CryAISystem/NavigationSystem/MNMTile.h>
#include "MNM_Type_info.h"
#include "OpenList.h"

#if !defined(_RELEASE) && CRY_PLATFORM_WINDOWS
	#define DEBUG_MNM_ENABLED 1
#else
	#define DEBUG_MNM_ENABLED 0
#endif

// If you want to debug the data consistency just set this define to 1
#define DEBUG_MNM_DATA_CONSISTENCY_ENABLED 0

// Log additional information about OffMesh link operation (add, remove, ...)
#define DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS 0

#if CRY_PLATFORM_WINDOWS
	#define MNM_USE_EXPORT_INFORMATION 1
#else
	#define MNM_USE_EXPORT_INFORMATION 0
#endif

namespace MNM
{

struct WayTriangleData
{
	WayTriangleData()
		: triangleID(0)
		, offMeshLinkID(0)
		, costMultiplier(1.0f)
		, incidentEdge((unsigned int)MNM::Constants::InvalidEdgeIndex)
	{

	}

	WayTriangleData(const TriangleID _triangleID, const OffMeshLinkID _offMeshLinkID)
		: triangleID(_triangleID)
		, offMeshLinkID(_offMeshLinkID)
		, costMultiplier(1.f)
		, incidentEdge((unsigned int)MNM::Constants::InvalidEdgeIndex)
	{

	}

	operator bool() const
	{
		return (triangleID != 0);
	}

	bool operator<(const WayTriangleData& other) const
	{
		return triangleID == other.triangleID ? offMeshLinkID < other.offMeshLinkID : triangleID < other.triangleID;
	}

	bool operator==(const WayTriangleData& other) const
	{
		return ((triangleID == other.triangleID) && (offMeshLinkID == other.offMeshLinkID));
	}

	TriangleID    triangleID;
	OffMeshLinkID offMeshLinkID;
	float         costMultiplier;
	unsigned int  incidentEdge;
};

inline bool IsTriangleAlreadyInWay(const TriangleID triangleID, const TriangleID* const way, const size_t wayTriCount)
{
	assert(way);
	return std::find(way, way + wayTriCount, triangleID) != (way + wayTriCount);
}

struct AStarContention
{
	void SetFrameTimeQuota(float frameTimeQuota)
	{
		m_frameTimeQuota.SetSeconds(frameTimeQuota);
	}

	struct ContentionStats
	{
		float  frameTimeQuota;

		uint32 averageSearchSteps;
		uint32 peakSearchSteps;

		float  averageSearchTime;
		float  peakSearchTime;
	};

	ContentionStats GetContentionStats()
	{
		ContentionStats stats;
		stats.frameTimeQuota = m_frameTimeQuota.GetMilliSeconds();

		stats.averageSearchSteps = m_totalSearchCount ? m_totalSearchSteps / m_totalSearchCount : 0;
		stats.peakSearchSteps = m_peakSearchSteps;

		stats.averageSearchTime = m_totalSearchCount ? m_totalComputationTime / (float)m_totalSearchCount : 0.0f;
		stats.peakSearchTime = m_peakSearchTime;

		return stats;
	}

	void ResetConsumedTimeDuringCurrentFrame()
	{
		m_consumedFrameTime.SetValue(0);
	}

protected:
	AStarContention(float frameTimeQuota = 0.001f)
	{
		m_frameTimeQuota.SetSeconds(frameTimeQuota);

		m_consumedFrameTime.SetValue(0);
		m_currentStepStartTime.SetValue(0);
		m_currentSearchTime.SetValue(0);
		m_currentSearchSteps = 0;

		ResetContentionStats();
	}

	inline void StartSearch()
	{
		m_currentSearchSteps = 0;
		m_currentSearchTime.SetValue(0);
	}

	inline void EndSearch()
	{
		m_totalSearchCount++;

		m_totalSearchSteps += m_currentSearchSteps;
		m_peakSearchSteps = max(m_peakSearchSteps, m_currentSearchSteps);

		const float lastSearchTime = m_currentSearchTime.GetMilliSeconds();
		m_totalComputationTime += lastSearchTime;
		m_peakSearchTime = max(m_peakSearchTime, lastSearchTime);
	}

	inline void StartStep()
	{
		m_currentStepStartTime = gEnv->pTimer->GetAsyncTime();
		m_currentSearchSteps++;
	}

	inline void EndStep()
	{
		const CTimeValue stepTime = (gEnv->pTimer->GetAsyncTime() - m_currentStepStartTime);
		m_consumedFrameTime += stepTime;
		m_currentSearchTime += stepTime;
	}

	inline bool FrameQuotaReached() const
	{
		return (m_frameTimeQuota > CTimeValue()) ? (m_consumedFrameTime >= m_frameTimeQuota) : false;
	}

	void ResetContentionStats()
	{
		m_totalSearchCount = 0;

		m_totalSearchSteps = 0;
		m_peakSearchSteps = 0;

		m_totalComputationTime = 0.0f;
		m_peakSearchTime = 0.0f;
	}

protected:
	CTimeValue m_frameTimeQuota;
	CTimeValue m_consumedFrameTime;
	CTimeValue m_currentStepStartTime;
	CTimeValue m_currentSearchTime;

	uint32     m_totalSearchCount;

	uint32     m_currentSearchSteps;
	uint32     m_totalSearchSteps;
	uint32     m_peakSearchSteps;

	float      m_totalComputationTime;
	float      m_peakSearchTime;
};

struct AStarOpenList
	: public AStarContention
{
	typedef AStarContention ContentionPolicy;

	struct Node
	{
		Node()
			: prevTriangle(0, 0)
			, open(false)
		{
		}

		Node(TriangleID _prevTriangleID, OffMeshLinkID _prevOffMeshLinkID, const vector3_t& _location, const real_t _cost = 0,
		     bool _open = false)
			: prevTriangle(_prevTriangleID, _prevOffMeshLinkID)
			, location(_location)
			, cost(_cost)
			, estimatedTotalCost(FLT_MAX)
			, open(_open)
		{
		}

		Node(TriangleID _prevTriangleID, OffMeshLinkID _prevOffMeshLinkID, const vector3_t& _location, const real_t _cost = 0,
		     const real_t _estitmatedTotalCost = real_t::max(), bool _open = false)
			: prevTriangle(_prevTriangleID, _prevOffMeshLinkID)
			, location(_location)
			, cost(_cost)
			, estimatedTotalCost(_estitmatedTotalCost)
			, open(_open)
		{
		}

		bool operator<(const Node& other) const
		{
			return estimatedTotalCost < other.estimatedTotalCost;
		}

		WayTriangleData prevTriangle;
		vector3_t       location;

		real_t          cost;
		real_t          estimatedTotalCost;

		bool            open : 1;
	};

	struct OpenNodeListElement
	{
		OpenNodeListElement(WayTriangleData _triData, Node* _pNode, real_t _fCost)
			: triData(_triData)
			, pNode(_pNode)
			, fCost(_fCost)
		{

		}

		WayTriangleData triData;
		Node*           pNode;
		real_t          fCost;

		bool operator<(const OpenNodeListElement& rOther) const
		{
			return fCost < rOther.fCost;
		}
	};

	// to get the size of the map entry, use typedef to provide us with value_type
	typedef std::map<WayTriangleData, Node>                                                NodesSizeHelper;
	typedef stl::STLPoolAllocator<NodesSizeHelper::value_type, stl::PSyncMultiThread>      OpenListAllocator;
	typedef std::map<WayTriangleData, Node, std::less<WayTriangleData>, OpenListAllocator> Nodes;

	void SetUpForPathSolving(const uint32 triangleCount, const TriangleID fromTriangleID, const vector3_t& startLocation, const real_t dist_start_to_end)
	{
		ContentionPolicy::StartSearch();

		const size_t EstimatedNodeCount = triangleCount + 64;
		const size_t MinOpenListSize = NextPowerOfTwo(EstimatedNodeCount);

		m_openList.clear();
		m_openList.reserve(MinOpenListSize);
		m_nodeLookUp.clear();

		std::pair<AStarOpenList::Nodes::iterator, bool> firstEntryIt = m_nodeLookUp.insert(std::make_pair(WayTriangleData(fromTriangleID, 0), AStarOpenList::Node(fromTriangleID, 0, startLocation, 0, dist_start_to_end, true)));
		m_openList.push_back(OpenNodeListElement(WayTriangleData(fromTriangleID, 0), &firstEntryIt.first->second, dist_start_to_end));

	}

	void PathSolvingDone()
	{
		ContentionPolicy::EndSearch();
	}

	inline OpenNodeListElement PopBestNode()
	{
		ContentionPolicy::StartStep();

		OpenList::iterator it = std::min_element(m_openList.begin(), m_openList.end());

		//Switch the smallest element with the last one and pop the last element
		OpenNodeListElement element = *it;
		*it = m_openList.back();
		m_openList.pop_back();

		return element;
	}

	inline bool InsertNode(const WayTriangleData& triangle, Node** pNextNode)
	{
		std::pair<Nodes::iterator, bool> itInsert = m_nodeLookUp.insert(std::make_pair(triangle, Node()));
		(*pNextNode) = &itInsert.first->second;

		assert(pNextNode);

		return itInsert.second;
	}

	inline const Node* FindNode(const WayTriangleData& triangle) const
	{
		Nodes::const_iterator it = m_nodeLookUp.find(triangle);

		return (it != m_nodeLookUp.end()) ? &it->second : NULL;
	}

	inline void AddToOpenList(const WayTriangleData& triangle, Node* pNode, real_t cost)
	{
		assert(pNode);
		m_openList.push_back(OpenNodeListElement(triangle, pNode, cost));
	}

	inline bool CanDoStep() const
	{
		return (!m_openList.empty() && !ContentionPolicy::FrameQuotaReached());
	}

	inline void StepDone()
	{
		ContentionPolicy::EndStep();
	}

	inline bool Empty() const
	{
		return m_openList.empty();
	}

	void Reset()
	{
		m_consumedFrameTime.SetValue(0);
		m_currentStepStartTime.SetValue(0);
		m_currentSearchTime.SetValue(0);

		m_currentSearchSteps = 0;

		stl::free_container(m_openList);
		stl::free_container(m_nodeLookUp);
	}

	bool TileWasVisited(const TileID tileID) const
	{
		Nodes::const_iterator nodesEnd = m_nodeLookUp.end();

		for (Nodes::const_iterator nodeIt = m_nodeLookUp.begin(); nodeIt != nodesEnd; ++nodeIt)
		{
			if (ComputeTileID(nodeIt->first.triangleID) != tileID)
				continue;

			return true;
		}

		return false;
	}

private:

	size_t NextPowerOfTwo(size_t n)
	{
		n = n - 1;
		n = n | (n >> 1);
		n = n | (n >> 2);
		n = n | (n >> 4);
		n = n | (n >> 8);
		n = n | (n >> 16);
		n = n + 1;

		return n;
	}

	typedef std::vector<OpenNodeListElement> OpenList;
	OpenList m_openList;
	Nodes    m_nodeLookUp;
};

template<typename Ty>
inline bool maximize(Ty& val, const Ty& x)
{
	if (val < x)
	{
		val = x;
		return true;
	}

	return false;
}

template<typename Ty>
inline bool minimize(Ty& val, const Ty& x)
{
	if (val > x)
	{
		val = x;
		return true;
	}

	return false;
}

template<typename Ty>
inline void sort2(Ty& x, Ty& y)
{
	if (x > y)
		std::swap(x, y);
}

template<typename Ty>
inline void rsort2(Ty& x, Ty& y)
{
	if (x < y)
		std::swap(x, y);
}

template<typename Ty>
inline Ty clamp(const Ty& x, const Ty& min, const Ty& max)
{
	if (x > max)
		return max;
	if (x < min)
		return min;
	return x;
}

template<typename Ty>
inline Ty clampunit(const Ty& x)
{
	if (x > Ty(1))
		return Ty(1);
	if (x < Ty(0))
		return Ty(0);
	return x;
}

template<typename Ty>
inline Ty next_mod3(const Ty& x)
{
	return (x + 1) % 3;
}

template<typename VecType>
inline real_t ProjectionPointLineSeg(const VecType& p, const VecType& a0, const VecType& a1)
{
	const VecType a = a1 - a0;
	const real_t lenSq = a.lenSq();

	if (lenSq > 0)
	{
		const VecType ap = p - a0;
		const real_t dot = a.dot(ap);
		return dot / lenSq;
	}

	return 0;
}

template<typename VecType>
inline real_t DistPointLineSegSq(const VecType& p, const VecType& a0, const VecType& a1)
{
	const VecType a = a1 - a0;
	const VecType ap = p - a0;
	const VecType bp = p - a1;

	const real_t e = ap.dot(a);
	if (e <= 0)
		return ap.dot(ap);

	const real_t f = a.dot(a);
	if (e >= f)
		return bp.dot(bp);

	return ap.dot(ap) - ((e * e) / f);
}

template<typename VecType>
inline VecType ClosestPtPointTriangle(const VecType& p, const VecType& a, const VecType& b, const VecType& c)
{
	const VecType ab = b - a;
	const VecType ac = c - a;
	const VecType ap = p - a;

	const real_t d1 = ab.dot(ap);
	const real_t d2 = ac.dot(ap);

	if ((d1 <= 0) && (d2 <= 0))
		return a;

	const VecType bp = p - b;
	const real_t d3 = ab.dot(bp);
	const real_t d4 = ac.dot(bp);

	if ((d3 >= 0) && (d4 <= d3))
		return b;

	const real_t vc = d1 * d4 - d3 * d2;
	if ((vc <= 0) && (d1 >= 0) && (d3 < 0))
	{
		const real_t v = d1 / (d1 - d3);
		return a + (ab * v);
	}

	const VecType cp = p - c;
	const real_t d5 = ab.dot(cp);
	const real_t d6 = ac.dot(cp);

	if ((d6 >= 0) && (d5 <= d6))
		return c;

	const real_t vb = d5 * d2 - d1 * d6;
	if ((vb <= 0) && (d2 >= 0) && (d6 < 0))
	{
		const real_t w = d2 / (d2 - d6);
		return a + (ac * w);
	}

	const real_t va = d3 * d6 - d5 * d4;
	if ((va <= 0) && ((d4 - d3) >= 0) && ((d5 - d6) > 0))
	{
		const real_t w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return b + ((c - b) * w);
	}

	// do not factorize the divisions : Fixed point precision requires it
	const real_t denom = va + vb + vc;
	const real_t v = vb / denom;
	const real_t w = vc / denom;

	return a + ab * v + ac * w;
}

inline bool PointInTriangle(const vector2_t& p, const vector2_t& a, const vector2_t& b, const vector2_t& c)
{
	const bool e0 = (p - a).cross(a - b) >= 0;
	const bool e1 = (p - b).cross(b - c) >= 0;
	const bool e2 = (p - c).cross(c - a) >= 0;

	return (e0 == e1) && (e0 == e2);
}

template<typename VecType>
inline real_t ClosestPtSegmentSegment(const VecType& a0, const VecType& a1, const VecType& b0, const VecType& b1,
                                      real_t& s, real_t& t, VecType& closesta, VecType& closestb)
{
	const VecType da = a1 - a0;
	const VecType db = b1 - b0;
	const VecType r = a0 - b0;
	const real_t a = da.lenSq();
	const real_t e = db.lenSq();
	const real_t f = db.dot(r);

	if ((a == 0) && (e == 0))
	{
		s = t = 0;
		closesta = a0;
		closestb = b0;

		return (closesta - closestb).lenSq();
	}

	if (a == 0)
	{
		s = 0;
		t = f / e;
		t = clampunit(t);
	}
	else
	{
		const real_t c = da.dot(r);

		if (e == 0)
		{
			t = 0;
			s = clampunit(-c / a);
		}
		else
		{
			const real_t b = da.dot(db);
			const real_t denom = (a * e) - (b * b);

			if (denom != 0)
				s = clampunit(((b * f) - (c * e)) / denom);
			else
				s = 0;

			const real_t tnom = (b * s) + f;

			if (tnom < 0)
			{
				t = 0;
				s = clampunit(-c / a);
			}
			else if (tnom > e)
			{
				t = 1;
				s = clampunit((b - c) / a);
			}
			else
				t = tnom / e;
		}
	}

	closesta = a0 + da * s;
	closestb = b0 + db * t;

	return (closesta - closestb).lenSq();
}

template<typename VecType>
inline vector2_t ClosestPtPointSegment(const VecType& p, const VecType& a0, const VecType& a1, real_t& t)
{
	const VecType a = a1 - a0;

	t = (p - a0).dot(a);

	if (t <= 0)
	{
		t = 0;
		return a0;
	}
	else
	{
		const real_t denom = a.lenSq();

		if (t >= denom)
		{
			t = 1;
			return a1;
		}
		else
		{
			t = t / denom;
			return a0 + (a * t);
		}
	}
}

// Intersection between two segments in 2D. The two parametric values are set to between 0 and 1
// if intersection occurs. If intersection does not occur their values will indicate the
// parametric values for intersection of the lines extended beyond the segment lengths.
// Parallel lines will result in a negative result.
enum EIntersectionResult
{
	eIR_NoIntersection,
	eIR_Intersection,
	eIR_ParallelOrCollinearSegments,
};

template<typename VecType>
inline EIntersectionResult DetailedIntersectSegmentSegment(const VecType& a0, const VecType& a1, const VecType& b0, const VecType& b1,
                                                           real_t& s, real_t& t)
{
	/*
	          a0
	          |
	          s
	   b0-----t----------b1
	          |
	          |
	          a1

	   Assuming
	   da = a1 - a0
	   db = b1 - b0
	   we can define the equations of the two segments as
	   a0 + s * da = 0    with s = [0...1]
	   b0 + t * db = 0    with t = [0...1]
	   We have an intersection if

	   a0 + s * da = b0 + t * db

	   We then cross product both side for db ( to calculate s) obtaining

	   (a0 - b0) x db + s * (da x db) = t * (db x db)
	   (a0 - b0) x db + s * (da x db) = 0

	   we call e = (a0 - b0)

	   s = ( e x db ) / ( da x db )

	   We can now calculate t with the same procedure.
	 */

	const VecType e = b0 - a0;
	const VecType da = a1 - a0;
	const VecType db = b1 - b0;
	// | da.x   da.y |
	// | db.x   db.y |
	// If det is zero then the two vectors are dependent, meaning
	// they are parallel or colinear.
	const real_t det = da.x * db.y - da.y * db.x;
	const real_t tolerance = real_t::epsilon();
	const real_t signAdjustment = det >= real_t(0.0f) ? real_t(1.0f) : real_t(-1.0f);
	const real_t minAllowedValue = real_t(.0f) - tolerance;
	const real_t maxAllowedValue = (real_t(1.0f) * det * signAdjustment) + tolerance;
	if (det != 0)
	{
		s = (e.x * db.y - e.y * db.x) * signAdjustment;

		if ((s < minAllowedValue) || (s > maxAllowedValue))
			return eIR_NoIntersection;

		t = (e.x * da.y - e.y * da.x) * signAdjustment;
		if ((t < minAllowedValue) || (t > maxAllowedValue))
			return eIR_NoIntersection;

		s = clampunit(s / det * signAdjustment);
		t = clampunit(t / det * signAdjustment);
		return eIR_Intersection;
	}

	return eIR_ParallelOrCollinearSegments;
}

template<typename VecType>
inline bool IntersectSegmentSegment(const VecType& a0, const VecType& a1, const VecType& b0, const VecType& b1,
                                    real_t& s, real_t& t)
{
	return DetailedIntersectSegmentSegment(a0, a1, b0, b1, s, t) == eIR_Intersection;
}
}

namespace MNMUtils
{
inline const MNM::real_t CalculateMinHorizontalRange(const uint16 radiusVoxelUnits, const float voxelHSize)
{
	assert(voxelHSize > 0.0f);

	// The horizontal x-y size for the voxels it's recommended to be the same.
	return MNM::real_t(max(radiusVoxelUnits * voxelHSize * 2.0f, 1.0f));
}

inline const MNM::real_t CalculateMinVerticalRange(const uint16 agentHeightVoxelUnits, const float voxelVSize)
{
	assert(voxelVSize > 0.0f);

	return MNM::real_t(max(agentHeightVoxelUnits * voxelVSize, 1.0f));
}
}

#endif  // #ifndef __MNM_H
