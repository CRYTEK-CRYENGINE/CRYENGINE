// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Model.h"

namespace Designer
{
class BSPTree3DNode;
struct OutputPolygons
{
	PolygonList posList;
	PolygonList negList;
	PolygonList coSameList;
	PolygonList coDiffList;
};

class BSPTree3D : public _i_reference_target_t
{
public:
	BSPTree3D(PolygonList& polygonList);
	~BSPTree3D();

	void GetPartitions(PolygonPtr& pPolygon, OutputPolygons& outPolygons) const;
	bool IsInside(const BrushVec3& vPos) const;
	bool IsValidTree() const { return m_bValidTree; }

private:
	BSPTree3DNode* m_pRootNode;
	bool           m_bValidTree;
	BSPTree3DNode* BuildBSP(PolygonList& polygonList);
};
}

