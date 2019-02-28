// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Polygon.h"

namespace Designer
{

//! The LoopPolygons instance contains separate loops of polygons. The loop is categorized into the outer loop and hole loops.
//! An usual polygon has a outer loop and several holes or no hole.
class LoopPolygons : public _i_reference_target_t
{
public:
	LoopPolygons(PolygonPtr polygon, bool bOptimizePolygon);

	const std::vector<PolygonPtr>& GetOuterPolygons() const { return m_OuterLoops; }
	const std::vector<PolygonPtr>& GetHolePolygons()  const { return m_Holes; }

	std::vector<PolygonPtr>        GetOuterClones() const;
	std::vector<PolygonPtr>        GetHoleClones()  const;

	const std::vector<PolygonPtr>& GetIslands() const;
	std::vector<PolygonPtr>        GetAllLoops() const;

	bool                           IsOptimized() const { return m_bOptimizedPolygon; }

private:
	void FindLoops(PolygonPtr polygon, std::vector<EdgeList>& outLoopList) const;
	bool CreateNewPolygonFromEdges(PolygonPtr polygon, const std::vector<IndexPair>& inputEdges, bool bOptimizePolygon, PolygonPtr& outPolygon) const;

	std::vector<PolygonPtr>         m_OuterLoops;
	std::vector<PolygonPtr>         m_Holes;
	mutable std::vector<PolygonPtr> m_Islands;
	bool                            m_bOptimizedPolygon;
};
}

