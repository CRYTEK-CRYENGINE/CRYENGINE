// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"

namespace Designer
{
class Model;

class OffsetManipulator : public _i_reference_target_t
{
public:
	OffsetManipulator() : m_bValid(false) {}
	~OffsetManipulator() {}

	bool       IsValid() const { return m_bValid; }
	void       Invalidate()    { m_bValid = false; }

	void       Init(PolygonPtr pPolygon, Model* pModel, bool bIncludeHoles = false);
	void       UpdateOffset(CViewport* view, CPoint point);
	void       UpdateOffset(BrushFloat scale);

	BrushFloat GetScale() const
	{
		if (IsValid())
			return m_Scale;
		return 0;
	}
	PolygonPtr                     GetPolygon() const       { return m_pPolygon; }
	PolygonPtr                     GetScaledPolygon() const { return m_pScaledPolygon; }
	const std::vector<PolygonPtr>& GetHoles() const         { return m_Holes; }

	void                           Display(DisplayContext& dc, bool bDisplayDistanceLine = true);

private:
	bool                    m_bValid;
	PolygonPtr              m_pScaledPolygon;
	PolygonPtr              m_pPolygon;
	std::vector<PolygonPtr> m_Holes;
	BrushPlane              m_Plane;
	BrushFloat              m_Scale;
	BrushVec3               m_Origin;
	BrushVec3               m_HitPos;
};

extern OffsetManipulator s_OffsetManipulator;
}

