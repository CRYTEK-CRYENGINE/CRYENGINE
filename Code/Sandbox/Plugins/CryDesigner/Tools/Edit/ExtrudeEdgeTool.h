// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/Select/SelectTool.h"

namespace Designer
{
class ExtrudeEdgeTool : public SelectTool
{
public:
	ExtrudeEdgeTool(EDesignerTool tool) :
		SelectTool(tool),
		m_bManipulatingGizmo(false),
		m_bHitGizmo(false)
	{
		m_nPickFlag = ePF_Edge;
		m_bAllowSelectionUndo = false;
	}

	void Enter() override;
	void Serialize(Serialization::IArchive& ar);

	bool OnLButtonUp(CViewport* view, UINT nFlags, CPoint point) override;
	bool OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;

	void OnManipulatorDrag(
	  IDisplayViewport* pView,
	  ITransformManipulator* pManipulator,
	  CPoint& p0,
	  BrushVec3 value,
	  int nFlags) override;

	void OnManipulatorBegin(
	  IDisplayViewport* pView,
	  ITransformManipulator* pManipulator,
	  CPoint& point,
	  int flags) override;

	void OnManipulatorEnd(
		IDisplayViewport* pView,
		ITransformManipulator* pManipulator) override;

private:
	void PrepareExtrusion();
	void Extrude(const Matrix34& offsetTM);
	void FinishExtrusion();
	void FlipPolygons();

	void StartUndo();
	void AcceptUndo();

	std::vector<std::pair<BrushEdge3D, BrushEdge3D>> m_EdgePairs;
	std::vector<PolygonPtr>                          m_CreatedPolygons;
	bool m_bManipulatingGizmo;
	bool m_bHitGizmo;
};
}

