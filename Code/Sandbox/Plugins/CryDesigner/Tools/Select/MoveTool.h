// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"
#include "Core/Model.h"
#include "Tools/Select/SelectTool.h"

namespace Designer
{
class MovePipeline;
class Model;

class MoveTool : public SelectTool
{
public:
	MoveTool(EDesignerTool tool);
	~MoveTool();

	void        Enter() override;
	bool        OnLButtonDown(CViewport* pView, UINT nFlags, CPoint point) override;
	bool        OnLButtonUp(CViewport* pView, UINT nFlags, CPoint point) override;
	bool        OnMouseMove(CViewport* pView, UINT nFlags, CPoint point) override;
	void        OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int flags) override;
	void        OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags) override;
	void        OnManipulatorEnd(IDisplayViewport* pView, ITransformManipulator* pManipulator) override;

	void        OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void        Serialize(Serialization::IArchive& ar);

	static void Transform(MainContext& mc, const BrushMatrix34& tm, bool bMoveTogether);

private:

	void        StartTransformation(bool bIsoloated);
	void        TransformSelections(const BrushMatrix34& offsetTM);
	void        EndTransformation();

	static void TransformSelections(MainContext& mc, MovePipeline& pipeline, const BrushMatrix34& offsetTM);
	void        InitializeMovementOnViewport(CViewport* pView, UINT nMouseFlags);

	std::unique_ptr<MovePipeline> m_Pipeline;
	bool                          m_bManipulatingGizmo;
	BrushVec3                     m_SelectedElementNormal;
};

GENERATE_MOVETOOL_CLASS(Vertex, EDesignerTool)
GENERATE_MOVETOOL_CLASS(Edge, EDesignerTool)
GENERATE_MOVETOOL_CLASS(Polygon, EDesignerTool)
GENERATE_MOVETOOL_CLASS(VertexEdge, EDesignerTool)
GENERATE_MOVETOOL_CLASS(VertexPolygon, EDesignerTool)
GENERATE_MOVETOOL_CLASS(EdgePolygon, EDesignerTool)
GENERATE_MOVETOOL_CLASS(VertexEdgePolygon, EDesignerTool)
}

