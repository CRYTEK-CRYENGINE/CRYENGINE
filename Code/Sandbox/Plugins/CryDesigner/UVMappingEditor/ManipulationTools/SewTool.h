// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"
#include "../UVCluster.h"

namespace Designer {
namespace UVMapping
{
struct SortedEdges
{
	UVIslandPtr           pUVIsland;
	std::vector<UVVertex> uvs;
	AABB                  aabb;
	bool                  bReverse;
	SortedEdges()
	{
		Reset();
	}
	void Reset()
	{
		pUVIsland = NULL;
		uvs.clear();
		bReverse = false;
		aabb.Reset();
	}
	void AssignUVs(const std::vector<UVEdge>& edges)
	{
		for (int i = 0, iCount(edges.size()); i < iCount; ++i)
			uvs.push_back(edges[i].uv0);
		uvs.push_back(edges[edges.size() - 1].uv1);
	}
	void UpdateAABB()
	{
		aabb.Reset();
		for (int i = 0, iCount(uvs.size()); i < iCount; ++i)
			aabb.Add(uvs[i].GetUV());
	}
	bool HasUV(const Vec2& uv) const
	{
		for (auto ii = uvs.begin(); ii < uvs.end(); ++ii)
		{
			if (Comparison::IsEquivalent((*ii).GetUV(), uv))
				return true;
		}
		return false;
	}
};

class SewTool : public BaseTool
{
public:
	SewTool(EUVMappingTool tool) : BaseTool(tool)
	{
	}
	void Enter() override;

protected:
	void SewEdges();
	void SewEdges(SortedEdges& se0, SortedEdges& se1);
	void SewVertices();
	bool SortEdges(SortedEdges& outSortedEdges0, SortedEdges& outSortedEdges1);
	bool FindSerialEdges(const std::vector<UVEdge>& inputEdges, std::vector<std::vector<UVEdge>>& outEdges) const;
};

class MoveAndSewTool : public SewTool
{
public:
	MoveAndSewTool(EUVMappingTool tool) : SewTool(tool) {}

	void Enter() override;

protected:
	void MoveAndSew();
	void MoveUVIsland(SortedEdges& se0, SortedEdges& se1);
	void ScaleUVIsland(SortedEdges& se0, SortedEdges& se1);
};

class SmartSewTool : public MoveAndSewTool
{
public:
	SmartSewTool(EUVMappingTool tool) : MoveAndSewTool(tool) {}
	void Enter() override;
};
}
}

