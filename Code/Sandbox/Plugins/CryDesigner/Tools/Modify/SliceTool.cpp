// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SliceTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "ViewManager.h"
#include "Gizmos/ITransformManipulator.h"
#include "Objects/DesignerObject.h"
#include "Objects/AreaSolidObject.h"
#include "Objects/DisplayContext.h"
#include "Serialization/Decorators/EditorActionButton.h"

namespace Designer
{

void SliceTool::Enter()
{
	__super::Enter();
	AABB aabb;
	GetBaseObject()->GetLocalBounds(aabb);
	m_SlicePlane = BrushPlane(BrushVec3(0, 0, 1), -aabb.GetCenter().z);
	GenerateLoop(m_SlicePlane, m_MainTraverseLines);
	UpdateGizmo();
	CenterPivot();
}

void SliceTool::Display(DisplayContext& dc)
{
	dc.SetDrawInFrontMode(true);

	DrawOutlines(dc);

	Matrix34 worldTM = dc.GetMatrix();
	Vec3 vScale = worldTM.GetInverted().TransformVector(Vec3(4.0f, 4.0f, 4.0f));
	float fScale = (vScale.x + vScale.y + vScale.z) / 3.0f;

	dc.DepthTestOff();
	dc.DrawArrow(m_CursorPos, m_CursorPos + m_SlicePlane.Normal() * fScale, 2.0f);
	dc.DepthTestOn();

	dc.SetDrawInFrontMode(false);
}

void SliceTool::DrawOutlines(DisplayContext& dc)
{
	float oldLineWidth = dc.GetLineWidth();
	dc.SetLineWidth(3);

	dc.SetColor(ColorB(233, 233, 33, 255));
	for (int i = 0, iSize(m_RestTraverseLineSet.size()); i < iSize; ++i)
		DrawOutline(dc, m_RestTraverseLineSet[i]);

	dc.SetColor(ColorB(50, 200, 50, 255));
	DrawOutline(dc, m_MainTraverseLines);

	dc.SetLineWidth(oldLineWidth);
}

void SliceTool::DrawOutline(DisplayContext& dc, TraverseLineList& lineList)
{
	for (int i = 0, iSize(lineList.size()); i < iSize; ++i)
		dc.DrawLine(lineList[i].m_Edge.m_v[0], lineList[i].m_Edge.m_v[1]);
}

void SliceTool::GenerateLoop(const BrushPlane& SlicePlane, TraverseLineList& outLineList) const
{
	outLineList.clear();

	for (int i = 0, iPolygonSize(GetModel()->GetPolygonCount()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);

		std::vector<BrushEdge3D> sortedIntersectedEdges;
		if (!pPolygon->QueryIntersectionsByPlane(SlicePlane, sortedIntersectedEdges))
			continue;

		for (int i = 0, iEdgeSize(sortedIntersectedEdges.size()); i < iEdgeSize; ++i)
			outLineList.push_back(ETraverseLineInfo(pPolygon, sortedIntersectedEdges[i], SlicePlane));
	}
}

BrushVec3 SliceTool::GetLoopPivotPoint() const
{
	AABB aabb;
	aabb.Reset();
	for (int i = 0, iSize(m_MainTraverseLines.size()); i < iSize; ++i)
	{
		aabb.Add(m_MainTraverseLines[i].m_Edge.m_v[0]);
		aabb.Add(m_MainTraverseLines[i].m_Edge.m_v[1]);
	}
	return aabb.GetCenter();
}

void SliceTool::SliceFrontPart()
{
	_smart_ptr<Model> frontPart;
	_smart_ptr<Model> backPart;

	CUndo undo("Designer : Slice Front");

	if (GetModel()->Clip(m_SlicePlane, true, frontPart, backPart) == eCPR_FAILED_CLIP)
		return;

	GetModel()->RecordUndo("Designer : Slice Front", GetBaseObject());

	if (backPart)
	{
		(*GetModel()) = *backPart;
		UpdateSurfaceInfo();
		ApplyPostProcess();
	}
}

void SliceTool::SliceBackPart()
{
	_smart_ptr<Model> frontPart;
	_smart_ptr<Model> backPart;

	CUndo undo("Designer : Slice Back");

	if (GetModel()->Clip(m_SlicePlane, true, frontPart, backPart) == eCPR_FAILED_CLIP)
		return;

	GetModel()->RecordUndo("Designer : Slice Back", GetBaseObject());

	if (frontPart)
	{
		(*GetModel()) = *frontPart;
		UpdateSurfaceInfo();
		ApplyPostProcess();
	}
}

void SliceTool::Divide()
{
	TraverseLineLists traverseLineListSet;
	if (m_RestTraverseLineSet.empty())
		traverseLineListSet.push_back(m_MainTraverseLines);
	else
		traverseLineListSet = m_RestTraverseLineSet;

	CUndo undo("Designer : Divide");
	GetModel()->RecordUndo("Designer : Divide", GetBaseObject());

	for (int i = 0, iSize(traverseLineListSet.size()); i < iSize; ++i)
	{
		const TraverseLineList& lineList = traverseLineListSet[i];
		for (int k = 0, iLineListSize(lineList.size()); k < iLineListSize; ++k)
		{
			PolygonPtr pSlicePolygon = new Polygon;
			pSlicePolygon->SetPlane(lineList[k].m_pPolygon->GetPlane());
			pSlicePolygon->AddEdge(lineList[k].m_Edge);
			GetModel()->AddSplitPolygon(pSlicePolygon);
			GetModel()->RemovePolygon(lineList[k].m_pPolygon);
		}
	}
	UpdateSurfaceInfo();
	ApplyPostProcess();
}

void SliceTool::Clip()
{
	_smart_ptr<Model> frontPart;
	_smart_ptr<Model> backPart;

	CUndo undo("Designer : Slip");

	if (GetModel()->Clip(m_SlicePlane, true, frontPart, backPart) == eCPR_FAILED_CLIP)
		return;

	GetModel()->RecordUndo("Designer : Slip", GetBaseObject());

	if (!frontPart || !backPart)
	{
		if (frontPart)
			(*GetModel()) = *frontPart;
		else if (backPart)
			(*GetModel()) = *backPart;
	}
	else
	{
		(*GetModel()) = *frontPart;

		CBaseObject* pClonedObject = GetIEditor()->GetObjectManager()->CloneObject(GetBaseObject());
		if (pClonedObject->IsKindOf(RUNTIME_CLASS(DesignerObject)))
		{
			DesignerObject* pBackObj = ((DesignerObject*)pClonedObject);
			pBackObj->SetModel(backPart);
			pBackObj->GetCompiler()->Compile(pBackObj, pBackObj->GetModel());
		}
		else if (pClonedObject->IsKindOf(RUNTIME_CLASS(AreaSolidObject)))
		{
			AreaSolidObject* pBackObj = ((AreaSolidObject*)pClonedObject);
			pBackObj->SetModel(backPart);
			pBackObj->GetCompiler()->Compile(pBackObj, pBackObj->GetModel());
		}
		else
		{
			DESIGNER_ASSERT(0);
		}
	}

	UpdateSurfaceInfo();
	ApplyPostProcess();
}

void SliceTool::UpdateSlicePlane()
{
	GenerateLoop(m_SlicePlane, m_MainTraverseLines);
}

void SliceTool::AlignSlicePlane(const BrushVec3& normal)
{
	BrushPlane plane(normal, 0);
	m_SlicePlane = BrushPlane(normal, -plane.Distance(m_CursorPos));
	GenerateLoop(m_SlicePlane, m_MainTraverseLines);
}

void SliceTool::InvertSlicePlane()
{
	m_SlicePlane.Invert();
	GenerateLoop(m_SlicePlane, m_MainTraverseLines);
}

void SliceTool::OnManipulatorDrag(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& p0, BrushVec3 value, int  nFlags)
{
	if (GetIEditor()->GetEditMode() == eEditModeScale)
		return;

	BrushVec3 vDelta = value - m_PrevGizmoPos;
	if (Comparison::IsEquivalent(vDelta, BrushVec3(0, 0, 0)))
		return;

	BrushMatrix34 offsetTM = GetOffsetTM(pManipulator, vDelta, GetWorldTM());
	bool bUpdatedManipulator = false;
	bool bDesignerMirrorMode = GetModel()->CheckFlag(eModelFlag_Mirror);

	if (bDesignerMirrorMode)
		bUpdatedManipulator = UpdateManipulatorInMirrorMode(offsetTM);

	if (!bUpdatedManipulator)
	{
		if (GetIEditor()->GetEditMode() == eEditModeMove)
		{
			m_GizmoPos += vDelta;
			m_CursorPos = m_GizmoPos;
			AlignSlicePlane(m_SlicePlane.Normal());
		}
		else if (!bDesignerMirrorMode && GetIEditor()->GetEditMode() == eEditModeRotate)
		{
			AlignSlicePlane(offsetTM.TransformVector(m_SlicePlane.Normal()));
		}
	}

	GetDesigner()->UpdateTMManipulator(m_GizmoPos, BrushVec3(0, 0, 1));
	m_PrevGizmoPos = value;
}

void SliceTool::OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags)
{
	m_PrevGizmoPos = BrushVec3(0, 0, 0);
}

void SliceTool::UpdateGizmo()
{
	m_GizmoPos = m_CursorPos;
	GetDesigner()->UpdateTMManipulator(m_GizmoPos, BrushVec3(0, 0, 1));
}

void SliceTool::CenterPivot()
{
	AABB bbox;
	if (!GetBaseObject())
		return;
	GetBaseObject()->GetLocalBounds(bbox);
	BrushVec3 vCenter = bbox.GetCenter();
	m_CursorPos = vCenter;
	AlignSlicePlane(m_SlicePlane.Normal());
	UpdateGizmo();
}

void SliceTool::Serialize(Serialization::IArchive& ar)
{
	using Serialization::ActionButton;

	if (ar.openBlock("Takeaway", "Take away"))
	{
		ar(ActionButton([this] { SliceFrontPart();
		                }), "Front", "^Front");
		ar(ActionButton([this] { SliceBackPart();
		                }), "Back", "^Back");
		ar.closeBlock();
	}

	if (ar.openBlock("Split", "Split"))
	{
		ar(ActionButton([this] { Clip();
		                }), "Clip", "^Clip");
		ar(ActionButton([this] { Divide();
		                }), "Divide", "^Divide");
		ar.closeBlock();
	}

	if (ar.openBlock("Alignment", "Alignment"))
	{
		ar(ActionButton([this] { AlignSlicePlane(BrushVec3(1, 0, 0));
		                }), "X", "^X");
		ar(ActionButton([this] { AlignSlicePlane(BrushVec3(0, 1, 0));
		                }), "Y", "^Y");
		ar(ActionButton([this] { AlignSlicePlane(BrushVec3(0, 0, 1));
		                }), "Z", "^Z");
		ar.closeBlock();
	}

	ar(ActionButton([this] { InvertSlicePlane();
	                }), "Invert", "Invert");
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Slice, eToolGroup_Modify, "Slice", SliceTool,
                                                           slice, "runs slice tool", "designer.slice");

