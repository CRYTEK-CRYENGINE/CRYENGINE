// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PivotTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "ViewManager.h"
#include "Core/ModelDB.h"
#include "Core/Helper.h"
#include "ToolFactory.h"
#include "Objects/DisplayContext.h"

namespace Designer
{
SERIALIZATION_ENUM_BEGIN(EPivotSelectionType, "Pivot Selection Type")
SERIALIZATION_ENUM(ePST_BoundBox, "BoundBox", "BoundBox")
SERIALIZATION_ENUM(ePST_Mesh, "Mesh", "Mesh")
SERIALIZATION_ENUM_END()

void PivotTool::Serialize(Serialization::IArchive& ar)
{
	ar(m_PivotSelectionType, "PivotType", "Pivot Type");
	if (ar.isEdit())
		SetSelectionType(m_PivotSelectionType, true);
}

void PivotTool::Confirm()
{
	CUndo undo("Designer : Pivot Tool");
	GetModel()->RecordUndo("Designer : Pivot", GetBaseObject());
	PivotToPos(GetBaseObject(), GetModel(), m_PivotPos);
	GetModel()->ResetDB(eDBRF_Vertex);
	SetSelectionType(m_PivotSelectionType, true);
	ApplyPostProcess(ePostProcess_ExceptCenterPivot);
	//Confirm applies new pivot, however during this time the edit tool is also being deleted
	//Maybe this InitializeManipulator is no longer necessary, maybe this will create a bug
	//InitializeManipulator();
}

void PivotTool::Enter()
{
	m_nSelectedCandidate = -1;
	m_nPivotIndex = -1;
	GetModel()->ResetDB(eDBRF_Vertex);

	__super::Enter();

	InitializeManipulator();
}

void PivotTool::Leave()
{
	__super::Leave();
	Confirm();
}

void PivotTool::InitializeManipulator()
{
	m_StartingDragManipulatorPos = m_PivotPos = BrushVec3(0, 0, 0);
	GetIEditor()->SetEditMode(eEditModeMove);
	if(GetDesigner())
		GetDesigner()->UpdateTMManipulator(m_PivotPos, BrushVec3(0, 0, 1));
}

void PivotTool::SetSelectionType(EPivotSelectionType selectionType, bool bForce)
{
	if (m_nSelectedCandidate == selectionType && !bForce)
		return;

	m_nPivotIndex = -1;
	m_CandidateVertices.clear();

	if (selectionType == ePST_BoundBox)
	{
		AABB aabb;
		GetBaseObject()->GetLocalBounds(aabb);
		BrushVec3 step = ToBrushVec3((aabb.max - aabb.min) * 0.5f);
		for (int i = 0; i <= 2; ++i)
			for (int j = 0; j <= 2; ++j)
				for (int k = 0; k <= 2; ++k)
					m_CandidateVertices.push_back(aabb.min + Vec3(i * step.x, j * step.y, k * step.z));
	}
	else if (selectionType == ePST_Mesh)
	{
		ModelDB* pDB = GetModel()->GetDB();
		pDB->GetVertexList(m_CandidateVertices);
	}
}

void PivotTool::Display(DisplayContext& dc)
{
	dc.SetColor(0xAAAAAAFF);
	dc.DepthTestOff();
	dc.DepthWriteOff();

	dc.PopMatrix();

	for (int i = 0, iCount(m_CandidateVertices.size()); i < iCount; ++i)
	{
		if (m_nSelectedCandidate == i)
			continue;
		BrushVec3 vWorldVertexPos = GetWorldTM().TransformPoint(m_CandidateVertices[i]);
		BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldVertexPos);
		dc.DrawSolidBox(ToVec3(vWorldVertexPos - vBoxSize), ToVec3(vWorldVertexPos + vBoxSize));
	}
	if (m_nSelectedCandidate != -1)
	{
		dc.SetColor(RGB(100, 100, 255));
		BrushVec3 vWorldVertexPos = GetWorldTM().TransformPoint(m_CandidateVertices[m_nSelectedCandidate]);
		BrushVec3 vBoxSize = GetElementBoxSize(dc.view, dc.flags & DISPLAY_2D, vWorldVertexPos);
		dc.DrawSolidBox(ToVec3(vWorldVertexPos - vBoxSize), ToVec3(vWorldVertexPos + vBoxSize));
	}

	dc.PushMatrix(GetWorldTM());

	dc.DepthWriteOn();
	dc.DepthTestOn();
}

bool PivotTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_nSelectedCandidate != -1)
	{
		GetDesigner()->UpdateTMManipulator(m_CandidateVertices[m_nSelectedCandidate], BrushVec3(0, 0, 1));
		m_nPivotIndex = m_nSelectedCandidate;
		m_PivotPos = m_CandidateVertices[m_nSelectedCandidate];
	}

	return true;
}

bool PivotTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	Vec3 raySrc, rayDir;
	view->ViewToWorldRay(point, raySrc, rayDir);

	BrushFloat leastT = (BrushFloat)3e10;
	m_nSelectedCandidate = -1;

	for (int i = 0, iCount(m_CandidateVertices.size()); i < iCount; ++i)
	{
		BrushFloat t = 0;
		BrushVec3 vWorldPos = GetWorldTM().TransformPoint(m_CandidateVertices[i]);
		BrushVec3 vBoxSize = GetElementBoxSize(view, view->GetType() != ET_ViewportCamera, vWorldPos);
		if (!GetIntersectionOfRayAndAABB(raySrc, rayDir, AABB(vWorldPos - vBoxSize, vWorldPos + vBoxSize), &t))
			continue;
		if (t < leastT && t > 0)
		{
			m_nSelectedCandidate = i;
			leastT = t;
		}
	}

	return true;
}

bool PivotTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
		return GetDesigner()->EndCurrentDesignerTool();
	return true;
}

void PivotTool::OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int flags)
{
	BrushMatrix34 moveTM = GetOffsetTM(pManipulator, value, GetWorldTM());
	m_PivotPos = moveTM.TransformPoint(m_StartingDragManipulatorPos);
	GetDesigner()->UpdateTMManipulator(m_PivotPos, BrushVec3(0, 0, 1));
}

void PivotTool::OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags)
{
	m_StartingDragManipulatorPos = m_PivotPos;
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Pivot, eToolGroup_BasicSelection, "Pivot", PivotTool,
                                                           pivotselection, "runs pivot selection tool", "designer.pivotselection")

