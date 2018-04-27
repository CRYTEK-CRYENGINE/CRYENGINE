// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <AttachmentVClothPreProcessDijkstra.h>
#include <CryMath/Cry_Math.h>
typedef unsigned int uint32;

template<class T>
struct OutputResult
{
	uint32 idx;
	bool isTarget;
	bool isIsland;
	uint32 nextNodeIdOnShortestPath;
	T weightToNextNodeOnShortestPath;
	T weightTotal;
};

template<class T>
void TestGraph(const GraphDijkstra<T>& g, OutputResult<T> expected[])
{
	for (uint32 i = 0; i < g.Results().size(); ++i)
	{
		REQUIRE(expected[i].idx == i);
		REQUIRE(g.IsTargetNode(i) == expected[i].isTarget);
		REQUIRE(g.IsIslandNode(i) == expected[i].isIsland);
		if (!g.IsTargetNode(i) && !g.IsIslandNode(i))
		{
			REQUIRE(g.Results(i).nextNodeIdOnShortestPath       == expected[i].nextNodeIdOnShortestPath);
			REQUIRE(g.Results(i).weightToNextNodeOnShortestPath == expected[i].weightToNextNodeOnShortestPath);
			REQUIRE(IsEquivalent(g.Results(i).weightTotal, expected[i].weightTotal, 1.0e-6f));
		}
	}
}

TEST(GraphDijkstraTest, All)
{
	// create graph
	const int nNodes = 11;
	GraphDijkstra<float> g(nNodes);

	g.AddEdge(0, 1, 4.2f); // test: two edges 0->1, different weight
	g.AddEdge(0, 1, 5.1f); // test: two edges 0->1, different weight
	g.AddEdge(0, 6, 8.1f);
	g.AddEdge(1, 3, 8.1f);
	g.AddEdge(1, 6, 11.1f);
	g.AddEdge(2, 4, 7.1f);
	g.AddEdge(2, 7, 2.1f);
	g.AddEdge(2, 6, 4.1f);
	g.AddEdge(3, 5, 9.1f);
	g.AddEdge(3, 4, 14.1f);
	g.AddEdge(4, 6, 10.1f);
	g.AddEdge(5, 7, 2.1f);
	g.AddEdge(6, 8, 1.1f);
	g.AddEdge(6, 5, 6.1f);
	g.AddEdge(7, 8, 7.1f);
	g.AddEdge(9, 10, 7.0f); // these two nodes are not connected to any other nodes

	bool bUseMultipleTargets = true;

	if (bUseMultipleTargets)
	{
		std::vector<uint32> listTargetIds;
		listTargetIds.push_back(2);
		listTargetIds.push_back(4);
		listTargetIds.push_back(8);
		g.FindShortestPaths(listTargetIds);
	}
	else
		g.FindShortestPaths(0);

	OutputResult<float> expected[] = 
	{
		/*idx  target  island  next  wnext  wtotal*/
		{ 0,   false,  false,  6,    8.1f,  9.2f  },
		{ 1,   false,  false,  6,    11.1f, 12.2f },
		{ 2,   true,   false,  0,    0,     0     },
		{ 3,   false,  false,  5,    9.1f,  13.3f },
		{ 4,   true,   false,  0,    0,     0     },
		{ 5,   false,  false,  7,    2.1f,  4.2f  },
		{ 6,   false,  false,  8,    1.1f,  1.1f  },
		{ 7,   false,  false,  2,    2.1f,  2.1f  },
		{ 8,   true,   false,  0,    0,     0     },
		{ 9,   false,  true,   0,    0,     0     },
		{ 10,  false,  true,   0,    0,     0     }
	};
	TestGraph(g, expected);
}

