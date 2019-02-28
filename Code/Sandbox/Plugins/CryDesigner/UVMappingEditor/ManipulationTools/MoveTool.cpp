// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MoveTool.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "Core/UVIslandManager.h"

namespace Designer {
namespace UVMapping
{
MoveTool::MoveTool(EUVMappingTool tool) : SelectTool(tool), m_VertexCluster(true)
{
}

void MoveTool::RecordUndo()
{
	if (!GetIEditor()->GetIUndoManager()->IsUndoRecording())
	{
		GetIEditor()->GetIUndoManager()->Begin();
		CUndo::Record(new UVMoveUndo);
	}
}

void MoveTool::EndUndoRecord()
{
	if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
		GetIEditor()->GetIUndoManager()->Accept("UVMapping Editor : Move UVs");
}

void MoveTool::OnLButtonDown(const SMouseEvent& me)
{
	__super::OnLButtonDown(me);
	m_VertexCluster.Invalidate();
	m_DraggingContext.Start(me.x, me.y);
	m_PrevHitPos = SearchUtil::FindHitPointToUVPlane(me.viewport, me.x, me.y);
}

void MoveTool::OnLButtonUp(const SMouseEvent& me)
{
	__super::OnLButtonUp(me);
	EndUndoRecord();
	m_VertexCluster.Unshelve();
}

void MoveTool::OnMouseMove(const SMouseEvent& me)
{
	__super::OnMouseMove(me);

	if (IsRectSelection())
		return;

	if (me.button == SMouseEvent::BUTTON_LEFT)
	{
		if (m_DraggingContext.IsDraggedEnough(me.x, me.y))
		{
			RecordUndo();

			Vec3 vHit = SearchUtil::FindHitPointToUVPlane(me.viewport, me.x, me.y);
			Matrix33 tm = Matrix33::CreateIdentity();
			tm.SetColumn(2, vHit - m_PrevHitPos);

			if (IsUVCursorSelected())
				GetUVEditor()->GetUVCursor()->Transform(tm);
			else
				m_VertexCluster.Transform(tm);

			GetUVEditor()->UpdateGizmoPos();
			m_PrevHitPos = vHit;
		}
	}
}

void MoveTool::OnGizmoLMBDown(int mode)
{
	RecordUndo();
	m_VertexCluster.Invalidate();
}

void MoveTool::OnGizmoLMBUp(int mode)
{
	EndUndoRecord();
	m_VertexCluster.Unshelve();
}

void MoveTool::OnTransformGizmo(int mode, const Vec3& offset)
{
	Matrix33 tm = Matrix33::CreateIdentity();

	if (mode == CAxisHelper::MOVE_MODE)
	{
		tm.SetColumn(2, offset);
		m_VertexCluster.Transform(tm);
		if (GetUVEditor()->GetPivotType() == ePivotType_Cursor)
			GetUVEditor()->GetUVCursor()->Transform(tm);
	}
	else if (mode == CAxisHelper::ROTATE_CIRCLE_MODE || mode == CAxisHelper::SCALE_MODE)
	{
		if (mode == CAxisHelper::ROTATE_CIRCLE_MODE)
			tm.SetRotationZ(offset.x);
		else
			tm.SetScale(offset);

		Vec3 pivot = GetUVEditor()->GetPivotType() == ePivotType_Selection ?
		             GetUVEditor()->GetGizmo()->GetPos() : GetUVEditor()->GetUVCursor()->GetPos();

		Matrix33 pivotTM = Matrix33::CreateIdentity();
		pivotTM.SetColumn(2, Vec3(-pivot.x, -pivot.y, 0));
		m_VertexCluster.Transform(pivotTM, false);
		m_VertexCluster.Transform(tm, false);
		pivotTM.SetColumn(2, Vec3(pivot.x, pivot.y, 0));
		m_VertexCluster.Transform(pivotTM);
	}
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Island, eUVMappingToolGroup_Manipulation, "Island", MoveIslandTool,
                                    island, "runs island tool", "uvmapping.island")
REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Polygon, eUVMappingToolGroup_Manipulation, "Polygon", MovePolygonTool,
                                    polygon, "runs polygon tool", "uvmapping.polygon")
REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Edge, eUVMappingToolGroup_Manipulation, "Edge", MoveEdgeTool,
                                    edge, "runs edge tool", "uvmapping.edge")
REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Vertex, eUVMappingToolGroup_Manipulation, "Vertex", MoveVertexTool,
                                    vertex, "runs vertex tool", "uvmapping.vertex")

