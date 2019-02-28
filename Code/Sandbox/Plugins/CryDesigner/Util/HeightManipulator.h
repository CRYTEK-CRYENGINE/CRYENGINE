// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Plane.h"

namespace Designer
{
class HeightManipulator
{
public:
	HeightManipulator() :
		m_bValid(false),
		m_bDisplayPole(false),
		m_bStartedManipulation(false),
		m_Offset(0),
		m_bHighlightPole(false)
	{}

	void Invalidate()
	{
		m_bValid = false;
		m_bStartedManipulation = false;
		m_bDisplayPole = false;
		m_bHighlightPole = false;
	}

	void       Init(const BrushPlane& floorPlane, const BrushVec3& vPivot, bool bDisplayPole = false);
	BrushFloat Update(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray);
	void       Display(DisplayContext& dc);
	BrushVec3  GetDir() const        { return m_FloorPlane.Normal(); }
	BrushPlane GetFloorPlane() const { return m_FloorPlane; }

	bool       Start(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray);
	bool       IsCursorCloseToPole(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray);

	void       HighlightPole(bool bHighlightPole) { m_bHighlightPole = bHighlightPole; }

private:
	BrushPlane CalculateHelperPlane(const BrushMatrix34& worldTM, CViewport* view);
	bool       GetProjectedPosToPole(const BrushMatrix34& worldTM, CViewport* view, const BrushRay& ray, BrushVec3& outProjectedPos);

	BrushVec3  m_vPivot;
	BrushPlane m_FloorPlane;
	BrushFloat m_Offset;

	bool       m_bStartedManipulation;
	bool       m_bDisplayPole;
	bool       m_bValid;
	bool       m_bHighlightPole;
};

extern HeightManipulator s_HeightManipulator;
}

