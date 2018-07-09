// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UVMappingEditor.h"
#include "Core/SmoothingGroupManager.h"

namespace Designer {
namespace UVMapping
{
struct less_UVVertex
{
	bool operator()(const UVVertex& v0, const UVVertex& v1) const
	{
		if (v0.pPolygon == v1.pPolygon)
			return v0.vIndex < v1.vIndex;

		return v0.pPolygon.get() < v1.pPolygon.get();
	}
};

struct UVVertexSet
{
	Vec2                              uv;
	std::set<UVVertex, less_UVVertex> uvSet;
};

class UVCluster
{
public:

	UVCluster(bool bShelf = false) : m_bShelf(bShelf)
	{
	}

	void Invalidate()
	{
		m_uvCluster.clear();
		m_uvIslandCluster.clear();
		m_AffectedSmoothingGroups.clear();
	}
	bool IsEmpty() const
	{
		return m_uvCluster.empty() && m_uvIslandCluster.empty();
	}

	void Transform(const Matrix33& tm, bool bUpdateShelf = true);
	void AlignToAxis(EPrincipleAxis axis, float value);
	void SetElementSetPtr(UVElementSetPtr pElementSet) { m_pElementSet = pElementSet; }
	void RemoveUVs();
	void Flip(const Vec2& pivot, const Vec2& normal);
	void GetPolygons(std::set<PolygonPtr>& outPolygons);

	void Unshelve();

private:
	void Update();
	void Shelve();
	void InvalidateSmoothingGroups();
	void FindSmoothingGroupAffected(std::set<SmoothingGroupPtr>& outAffectedSmoothingGroups);

	bool                                             m_bShelf;
	UVElementSetPtr                                  m_pElementSet;
	std::map<Vec2, UVVertexSet, Comparison::less_UV> m_uvCluster;
	std::set<UVIslandPtr>                            m_uvIslandCluster;
	std::set<SmoothingGroupPtr>                      m_AffectedSmoothingGroups;
};
}
}

