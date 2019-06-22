// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "WayTriangleData.h"
#include "OpenList.h"
#include "DangerousAreas.h"

namespace MNM
{

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

struct AStarNodesList
	: public AStarContention
{
	typedef AStarContention ContentionPolicy;

	struct Node
	{
		Node()
			: prevTriangle()
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

		std::pair<AStarNodesList::Nodes::iterator, bool> firstEntryIt = m_nodeLookUp.insert(std::make_pair(WayTriangleData(fromTriangleID, OffMeshLinkID()), AStarNodesList::Node(fromTriangleID, OffMeshLinkID(), startLocation, 0, dist_start_to_end, true)));
		m_openList.push_back(OpenNodeListElement(WayTriangleData(fromTriangleID, OffMeshLinkID()), &firstEntryIt.first->second, dist_start_to_end));

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

struct SWayQueryWorkingSet
{
	SWayQueryWorkingSet()
	{
		nextLinkedTriangles.reserve(32);
	}
	typedef std::vector<WayTriangleData> TNextLinkedTriangles;

	void Reset()
	{
		aStarNodesList.Reset();
		nextLinkedTriangles.clear();
		nextLinkedTriangles.reserve(32);
	}

	TNextLinkedTriangles nextLinkedTriangles;

	//! Contains open and closed nodes
	AStarNodesList       aStarNodesList;
};

struct SWayQueryResult
{
	SWayQueryResult(const size_t wayMaxSize = 512)
		: m_wayMaxSize(wayMaxSize)
		, m_waySize(0)
	{
		Reset();
	}

	SWayQueryResult(SWayQueryResult&&) = default;
	SWayQueryResult(const SWayQueryResult&) = delete;
	~SWayQueryResult() = default;

	void Reset()
	{
		m_pWayTriData.reset(new WayTriangleData[m_wayMaxSize]);
		m_waySize = 0;
	}

	ILINE WayTriangleData* GetWayData() const { return m_pWayTriData.get(); }
	ILINE size_t           GetWaySize() const { return m_waySize; }
	ILINE size_t           GetWayMaxSize() const { return m_wayMaxSize; }

	ILINE void             SetWaySize(const size_t waySize) { m_waySize = waySize; }

	ILINE void             Clear() { m_waySize = 0; }

	SWayQueryResult& operator=(SWayQueryResult&&) = default;
	SWayQueryResult& operator=(const SWayQueryResult&) = delete;

private:

	std::unique_ptr<WayTriangleData[]> m_pWayTriData;
	size_t                             m_wayMaxSize;
	size_t                             m_waySize;
};

} // namespace MNM