// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MoveTool.h"
#include "ViewManager.h"
#include "Core/Model.h"
#include "Core/Polygon.h"
#include "Gizmos/ITransformManipulator.h"
#include "DesignerEditor.h"
#include "MovePipeline.h"
#include "Core/Helper.h"
#include "Util/ExcludedEdgeManager.h"

namespace Designer
{
static const BrushFloat kQueryEpsilon = 0.005f;

void MoveTool::Serialize(Serialization::IArchive& ar)
{
	DISPLAY_MESSAGE("SHIFT + Dragging on selected elements : Detach polygons");
	DISPLAY_MESSAGE("CTRL + LMB or Dragging : Select several");
}

MoveTool::MoveTool(EDesignerTool tool) :
	SelectTool(tool),
	m_Pipeline(new MovePipeline),
	m_bManipulatingGizmo(false)
{
}

MoveTool::~MoveTool()
{
}

void MoveTool::Enter()
{
	__super::Enter();
	GetDesigner()->UpdateTMManipulatorBasedOnElements(DesignerSession::GetInstance()->GetSelectedElements());
	UpdateDrawnEdges(GetMainContext());
	m_bManipulatingGizmo = false;
}

void MoveTool::InitializeMovementOnViewport(CViewport* pView, UINT nMouseFlags)
{
	InitalizePlaneAlignedWithView(pView);
	bool bIsolatedMovement = (nMouseFlags & MK_SHIFT) || DesignerSession::GetInstance()->GetSelectedElements()->IsIsolated();
	StartTransformation(bIsolatedMovement);
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	ApplyPostProcess(ePostProcess_Mirror);
}

bool MoveTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_bManipulatingGizmo)
		return true;

	__super::OnLButtonDown(view, nFlags, point);
	GetDesigner()->UpdateTMManipulatorBasedOnElements(DesignerSession::GetInstance()->GetSelectedElements());

	return true;
}

bool MoveTool::OnLButtonUp(CViewport* pView, UINT nFlags, CPoint point)
{
	if (m_SelectionType == eST_TransformSelectedElements)
		EndTransformation();

	return __super::OnLButtonUp(pView, nFlags, point);
}

bool MoveTool::OnMouseMove(CViewport* pView, UINT nFlags, CPoint point)
{
	if (m_bManipulatingGizmo)
		return true;

	if (m_SelectionType == eST_NormalSelection && !(nFlags & MK_CONTROL))
	{
		m_SelectionType = eST_JustAboutToTransformSelectedElements;
		m_MouseDownPos = point;
	}

	if (m_SelectionType == eST_JustAboutToTransformSelectedElements && (nFlags & MK_LBUTTON))
	{
		if (std::abs(m_MouseDownPos.x - point.x) > 10 || std::abs(m_MouseDownPos.y - point.y) > 10)
		{
			InitializeMovementOnViewport(pView, nFlags);
			m_SelectionType = eST_TransformSelectedElements;
		}
	}

	if (m_SelectionType == eST_TransformSelectedElements)
		TransformSelections(GetOffsetTMOnAlignedPlane(pView, m_PlaneAlignedWithView, m_MouseDownPos, point));
	else
		__super::OnMouseMove(pView, nFlags, point);

	if ((nFlags & MK_CONTROL) || m_SelectionType == eST_TransformSelectedElements)
		GetDesigner()->UpdateTMManipulatorBasedOnElements(DesignerSession::GetInstance()->GetSelectedElements());

	return true;
}

void MoveTool::TransformSelections(const BrushMatrix34& offsetTM)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	MainContext mc(GetMainContext());

	m_Pipeline->TransformSelections(mc, offsetTM);
	pSession->GetExcludedEdgeManager()->Clear();

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	ApplyPostProcess(ePostProcess_Mirror | ePostProcess_SmoothingGroup);
	GetCompiler()->Compile(GetBaseObject(), GetModel(), eShelf_Construction);

	UpdateDrawnEdges(GetMainContext());

	pSession->UpdateSelectionMeshFromSelectedElements();
}

void MoveTool::OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int flags)
{
	if (!m_bManipulatingGizmo)
		return;

	TransformSelections(GetOffsetTM(pManipulator, value, GetWorldTM()));
}

void MoveTool::StartTransformation(bool bIsoloated)
{
	GetIEditor()->GetIUndoManager()->Begin();
	GetDesigner()->StoreSelectionUndo();
	GetModel()->RecordUndo("Move", GetBaseObject());

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	m_SelectedElementNormal = pSelected->GetNormal(GetModel());
	m_Pipeline->SetModel(GetModel());
	if (bIsoloated)
		m_Pipeline->InitializeIsolated(*pSelected);
	else
		m_Pipeline->Initialize(*pSelected);

	ApplyPostProcess(ePostProcess_Mesh | ePostProcess_Mirror | ePostProcess_SmoothingGroup);
}

void MoveTool::EndTransformation()
{
	m_Pipeline->End();

	UpdateSurfaceInfo();
	ApplyPostProcess(ePostProcess_CenterPivot | ePostProcess_SyncPrefab | ePostProcess_Mesh | ePostProcess_DataBase | ePostProcess_SmoothingGroup);

	BrushVec3 vOffset(GetOffsetToAnother(GetBaseObject(), GetWorldBottomCenter(GetBaseObject())));
	if (gDesignerSettings.bKeepPivotCentered)
	{
		ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
		pSelected->ApplyOffset(vOffset);
	}

	MainContext mc(GetMainContext());
	if (mc.pSelected)
	{
		GetDesigner()->UpdateTMManipulatorBasedOnElements(mc.pSelected);
	}
	GetIEditor()->GetIUndoManager()->Accept("Designer Move");
}

void MoveTool::OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (pSelected->IsEmpty())
		return;

	m_bHitGizmo = true;

	if (!m_bManipulatingGizmo)
	{
		bool bIsolated = (flags & MK_SHIFT) || pSelected->IsIsolated();
		StartTransformation(bIsolated);
		m_bManipulatingGizmo = true;
	}
}

void MoveTool::OnManipulatorEnd(IDisplayViewport* pView, ITransformManipulator* pManipulator)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (pSelected->IsEmpty())
		return;

	m_bHitGizmo = false;

	if (m_bManipulatingGizmo)
	{
		EndTransformation();
		m_bManipulatingGizmo = false;
	}
}

void MoveTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (GetBaseObject() == NULL)
		return;
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	if (pSelected->QueryFromElements(GetModel()).empty())
		return;
	__super::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		{
			GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
			m_Pipeline->SetModel(GetModel());
			m_Pipeline->SetQueryResultsFromSelectedElements(*pSelected);
		}
		break;
	}
}

void MoveTool::Transform(MainContext& mc, const BrushMatrix34& tm, bool bMoveTogether)
{
	MovePipeline pipeline;

	pipeline.SetModel(mc.pModel);

	if (bMoveTogether)
		pipeline.Initialize(*mc.pSelected);
	else
		pipeline.InitializeIsolated(*mc.pSelected);

	pipeline.TransformSelections(mc, tm);

	MODEL_SHELF_RECONSTRUCTOR(mc.pModel);
	mc.pModel->SetShelf(eShelf_Construction);
	mc.pModel->ResetDB(eDBRF_ALL, eShelf_Construction);

	pipeline.End();
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Vertex, eToolGroup_BasicSelection, "Vertex", MoveVertexTool,
                                                           vertex, "runs vertex tool", "designer.vertex")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Edge, eToolGroup_BasicSelection, "Edge", MoveEdgeTool,
                                                           edge, "runs edge tool", "designer.edge")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Polygon, eToolGroup_BasicSelection, "Polygon", MovePolygonTool,
                                                           polygon, "runs polygon tool", "designer.polygon")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_VertexEdge, eToolGroup_BasicSelectionCombination, "Vertex Edge", MoveVertexEdgeTool,
                                                           vertexedge, "runs vertex and edge tool", "designer.vertexedge")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_VertexPolygon, eToolGroup_BasicSelectionCombination, "Vertex Polygon", MoveVertexPolygonTool,
                                                           vertexpolygon, "runs vertex and polygon tool", "designer.vertexpolygon")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_EdgePolygon, eToolGroup_BasicSelectionCombination, "Edge Polygon", MoveEdgePolygonTool,
                                                           edgepolygon, "runs edge and polygon tool", "designer.edgepolygon")
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_VertexEdgePolygon, eToolGroup_BasicSelectionCombination, "Vertex Edge Polygon", MoveVertexEdgePolygonTool,
                                                           vertexedgepolygon, "runs vertex, edge and polygon tool", "designer.vertexedgepolygon")

