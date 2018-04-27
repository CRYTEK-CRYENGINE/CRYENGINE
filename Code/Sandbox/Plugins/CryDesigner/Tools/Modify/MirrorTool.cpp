// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MirrorTool.h"
#include "Core/Model.h"
#include "Tools/Select/SelectTool.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include "Core/SmoothingGroupManager.h"
#include "Core/UVIslandManager.h"
#include "Core/EdgesSharpnessManager.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "DesignerSession.h"

namespace Designer
{
void MirrorTool::ApplyMirror()
{
	CUndo undo("Designer : Apply Mirror");
	GetModel()->RecordUndo("Designer : Apply Mirror", GetBaseObject());

	_smart_ptr<Model> frontPart;
	_smart_ptr<Model> backPart;
	if (GetModel()->Clip(m_SlicePlane, false, frontPart, backPart) == eCPR_FAILED_CLIP || backPart == NULL)
		return;

	backPart->GetSmoothingGroupMgr()->CopyFromModel(backPart, GetModel());
	*backPart->GetEdgeSharpnessMgr() = *GetModel()->GetEdgeSharpnessMgr();
	*backPart->GetUVIslandMgr() = *GetModel()->GetUVIslandMgr();

	*GetModel() = *backPart;

	for (int i = 0, iPolygonSize(backPart->GetPolygonCount()); i < iPolygonSize; ++i)
		GetModel()->AddPolygon(backPart->GetPolygon(i)->Clone()->Mirror(m_SlicePlane), false);

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	pSelected->Clear();

	GetModel()->SetMirrorPlane(m_SlicePlane);
	GetModel()->SetFlag(GetModel()->GetFlag() | eModelFlag_Mirror);

	UpdateSurfaceInfo();
	ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab | ePostProcess_GameResource | ePostProcess_SmoothingGroup);

	pSession->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	UpdateGizmo();
}

void MirrorTool::FreezeModel()
{
	CUndo undo("Designer : Release Mirror");
	GetModel()->RecordUndo("Designer : Freeze Designer", GetBaseObject());
	ReleaseMirrorMode(GetModel());
	RemoveEdgesOnMirrorPlane(GetModel());
	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	UpdateSurfaceInfo();
	ApplyPostProcess();
	UpdateGizmo();
}

void MirrorTool::ReleaseMirrorMode(Model* pModel)
{
	pModel->SetFlag(pModel->GetFlag() & (~eModelFlag_Mirror));
	for (int i = 0, iPolygonSize(pModel->GetPolygonCount()); i < iPolygonSize; ++i)
		pModel->GetPolygon(i)->RemoveFlags(ePolyFlag_Mirrored);
}

void MirrorTool::RemoveEdgesOnMirrorPlane(Model* pModel)
{
	BrushPlane mirrorPlane = pModel->GetMirrorPlane();

	std::vector<BrushEdge3D> borderEdges;
	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		DESIGNER_ASSERT(pPolygon);
		if (!pPolygon)
			continue;
		for (int k = 0, iEdgeCount(pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
		{
			BrushEdge3D edge = pPolygon->GetEdge(k);
			if (std::abs(mirrorPlane.Distance(edge.m_v[0])) < kDesignerEpsilon && std::abs(mirrorPlane.Distance(edge.m_v[1])) < kDesignerEpsilon)
				borderEdges.push_back(edge);
		}
	}

	for (int i = 0, iBorderEdgeCount(borderEdges.size()); i < iBorderEdgeCount; ++i)
	{
		std::vector<PolygonPtr> borderPolygons;
		pModel->QueryPolygonsHavingEdge(borderEdges[i], borderPolygons);
		if (borderPolygons.size() == 2 && borderPolygons[0]->GetPlane().IsEquivalent(borderPolygons[1]->GetPlane()))
			pModel->EraseEdge(borderEdges[i]);
	}
}

void MirrorTool::Display(DisplayContext& dc)
{
	if (GetModel()->CheckFlag(eModelFlag_Mirror))
		return;
	__super::Display(dc);
}

void MirrorTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		// Firing notifications as response to notifications is not so nice...
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
		break;
	}
}

void MirrorTool::OnManipulatorEnd(IDisplayViewport* pView, ITransformManipulator* pManipulator)
{
	if (GetModel()->CheckFlag(eModelFlag_Mirror))
	{
		ApplyPostProcess(ePostProcess_DataBase);
		GetIEditor()->GetIUndoManager()->Accept("Designer : Mirror.Move Pivot");
	}
}

void MirrorTool::OnManipulatorBegin(IDisplayViewport* pView, ITransformManipulator* pManipulator, CPoint& point, int flags)
{
	if (GetModel()->CheckFlag(eModelFlag_Mirror))
	{
		GetIEditor()->GetIUndoManager()->Begin();
		GetModel()->RecordUndo("Designer : Mirror.Move Pivot", GetBaseObject());
	}
	m_PrevGizmoPos = BrushVec3(0, 0, 0);
}

void MirrorTool::UpdateGizmo()
{
	if (GetModel()->CheckFlag(eModelFlag_Mirror))
	{
		BrushVec3 vAveragePos(0, 0, 0);
		int nCount(0);
		for (int i = 0, iPolygonSize(GetModel()->GetPolygonCount()); i < iPolygonSize; ++i)
		{
			PolygonPtr pPolygon = GetModel()->GetPolygon(i);
			if (pPolygon->CheckFlags(ePolyFlag_Mirrored))
				continue;
			for (int k = 0, iVertexSize(pPolygon->GetVertexCount()); k < iVertexSize; ++k)
			{
				vAveragePos += pPolygon->GetPos(k);
				++nCount;
			}
		}
		vAveragePos /= nCount;
		m_GizmoPos = vAveragePos;
	}
	else
	{
		m_GizmoPos = m_CursorPos;
	}
	GetDesigner()->UpdateTMManipulator(m_GizmoPos, BrushVec3(0, 0, 1));
}

bool MirrorTool::UpdateManipulatorInMirrorMode(const BrushMatrix34& offsetTM)
{
	for (int i = 0, iPolygonSize(GetModel()->GetPolygonCount()); i < iPolygonSize; ++i)
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Mirrored))
			continue;
		pPolygon->Transform(offsetTM);
	}

	ApplyPostProcess(ePostProcess_Mirror | ePostProcess_Mesh | ePostProcess_SmoothingGroup);

	return true;
}

void MirrorTool::Serialize(Serialization::IArchive& ar)
{
	using Serialization::ActionButton;

	bool bMirrored = GetModel()->CheckFlag(eModelFlag_Mirror);

	if (bMirrored)
	{
		ar(ActionButton([this] { FreezeModel();
		                }), "Freeze", "^Freeze");
	}
	else
	{
		ar(ActionButton([this] { ApplyMirror();
		                }), "Apply", "Apply");
		ar(ActionButton([this] { InvertSlicePlane();
		                }), "Invert", "Invert");
		ar(ActionButton([this] { CenterPivot();
		                }), "Center Pivot", "Center Pivot");

		if (ar.openBlock("Alignment", "Alignment"))
		{
			ar(ActionButton([this] { AlignSlicePlane(BrushVec3(1, 0, 0));
			                }), "X", "^X");
			ar(ActionButton([this] { AlignSlicePlane(BrushVec3(0, 1, 0));
			                }), "Y", "^Y");
			ar(ActionButton([this] { AlignSlicePlane(BrushVec3(0, 0, 1));
			                }), "X", "^Z");
			ar.closeBlock();
		}
	}
}
}

#define MirrorPanel PropertyTreePanel < Designer::MirrorTool >
REGISTER_DESIGNER_TOOL_WITH_PANEL_AND_COMMAND(eDesigner_Mirror, eToolGroup_Modify, "Mirror", MirrorTool, MirrorPanel,
                                              mirror, "runs mirror tool", "designer.mirror")

