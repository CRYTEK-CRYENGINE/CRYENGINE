// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"
#include "Util/OffsetManipulator.h"

namespace Designer
{
struct OffsetParam
{
	OffsetParam() :
		bBridgeEdges(false),
		bMultipleOffset(false)
	{
	}
	bool bBridgeEdges;
	bool bMultipleOffset;
};

class OffsetTool : public SelectTool
{
public:
	OffsetTool(EDesignerTool tool) :
		SelectTool(tool),
		m_fPrevScale(0)
	{
		m_nPickFlag = ePF_Polygon;
	}

	void        Enter() override;

	bool        OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool        OnLButtonUp(CViewport* view, UINT nFlags, CPoint point) override;
	bool        OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	void        Display(DisplayContext& dc) override;
	void        Serialize(Serialization::IArchive& ar);
	bool        IsManipulatorVisible() override;

	static void ApplyOffset(
	  Model* pModel,
	  PolygonPtr pScaledPolygon,
	  PolygonPtr pOriginalPolygon,
	  const std::vector<PolygonPtr>& holes,
	  bool bBridgeEdges);

private:
	void OnLButtonDownForMultipleOffset(CViewport* view, UINT nFlags, CPoint point);
	void OnLButtonUpForMultipleOffset(CViewport* view, UINT nFlags, CPoint point);
	void OnMouseMoveForMultipleOffset(CViewport* view, UINT nFlags, CPoint point);

private:
	void       RepeatPrevAction(PolygonPtr polygon);
	PolygonPtr QueryOffsetPolygon(CViewport* view, CPoint point) const;
	void       AddScaledPolygon(
	  PolygonPtr pScaledPolygon,
	  PolygonPtr pOriginalPolygon,
	  const std::vector<PolygonPtr>& holes,
	  bool bBridgeEdges);
	OffsetManipulator* GetOffsetManipulator() const;
	void               UpdateAllOffsetManipulators(CViewport* view, CPoint point);
	void               InitializeSelection();
	void               RegisterScaledPolygons();
	bool               IsTwoPointsEquivalent(const CPoint& p0, const CPoint& p1) const { return std::abs(p0.x - p1.x) > 2 || std::abs(p0.y - p1.y) > 2 ? false : true; }

	BrushFloat  m_fPrevScale;
	OffsetParam m_Params;
	CPoint      m_MouseDownPos;
	std::vector<_smart_ptr<OffsetManipulator>> m_OffsetManipulators;
};
}

