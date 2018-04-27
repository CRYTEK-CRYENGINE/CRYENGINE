// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ShapeTool.h"

namespace Designer
{
enum EStairHeightCalculationWayMode
{
	eStairHeightCalculationWay_StepRise,
	eStairHeightCalculationWay_StepNumber
};

struct StairProfileParameter
{
	float m_StepRise;
	bool  m_bClosedProfile;

	StairProfileParameter() :
		m_StepRise(kDefaultStepRise),
		m_bClosedProfile(true)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(STEPRISE_RANGE(m_StepRise), "StepRise", "Step Rise");
		ar(m_bClosedProfile, "Closed Profile", "^^Closed Profile");
	}
};

enum ESideStairMode
{
	eSideStairMode_PlaceFirstPoint,
	eSideStairMode_DrawDiagonal,
	eSideStairMode_SelectDirection,
};

class StairProfileTool : public ShapeTool
{
public:

	StairProfileTool(EDesignerTool tool) :
		ShapeTool(tool),
		m_SideStairMode(eSideStairMode_PlaceFirstPoint),
		m_nSelectedCandidate(0)
	{
	}

	void Enter() override;

	bool OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	void Display(DisplayContext& dc) override;
	void Serialize(Serialization::IArchive& ar) override;
	bool IsPhaseFirstStepOnPrimitiveCreation() const override;

protected:

	void CreateCandidates();
	void DrawCandidateStair(DisplayContext& dc, int nIndex, const ColorB& color);
	Spot Convert2Spot(Model* pModel, const BrushPlane& plane, const BrushVec2& pos) const;

	ESideStairMode        m_SideStairMode;
	std::vector<Spot>     m_CandidateStairs[2];
	BrushLine             m_BorderLine;
	int                   m_nSelectedCandidate;
	Spot                  m_LastSpot;
	int                   m_nSelectedPolygonIndex;
	StairProfileParameter m_StairProfileParameter;
};
}

