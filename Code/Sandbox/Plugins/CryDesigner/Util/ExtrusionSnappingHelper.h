// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"
#include "Core/Model.h"
#include "Tools/ToolCommon.h"

class CViewport;

namespace Designer
{
class ExtrusionSnappingHelper
{
public:

	void Init(Model* pModel)
	{
		m_pModel = pModel;
	}

	void       SearchForOppositePolygons(PolygonPtr pCapPolygon);
	bool       IsOverOppositePolygon(const PolygonPtr& pCapPolygon, EPushPull pushpull, bool bIgnoreSimilarDirection = false);
	void       ApplyOppositePolygons(PolygonPtr pCapPolygon, EPushPull pushpull, bool bHandleTouch = false);
	BrushFloat GetNearestDistanceToOpposite(EPushPull pushpull) const;
	PolygonPtr FindAlignedPolygon(PolygonPtr pCapPolygon, const BrushMatrix34& worldTM, CViewport* view, const CPoint& point) const;

	struct Opposite
	{
		Opposite()
		{
			Init();
		}
		void Init()
		{
			polygon = NULL;
			resultOfFindingOppositePolygon = eFindOppositePolygon_None;
			nearestDistanceToPolygon = 0;
			bTouchAdjacent = false;
		}
		PolygonPtr                      polygon;
		EResultOfFindingOppositePolygon resultOfFindingOppositePolygon;
		BrushFloat                      nearestDistanceToPolygon;
		bool                            bTouchAdjacent;
	};

private:

	Model* GetModel() const { return m_pModel; }

	_smart_ptr<Model> m_pModel;
	Opposite          m_Opposites[2];
};

extern ExtrusionSnappingHelper s_SnappingHelper;
}

