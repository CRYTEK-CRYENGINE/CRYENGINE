// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OffsetTool.h"
#include "Viewport.h"
#include "Core/Model.h"
#include "Core/Helper.h"
#include "Core/LoopPolygons.h"
#include "DesignerEditor.h"
#include "Util/MFCUtil.h"

namespace Designer
{
void OffsetTool::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("BridgeEdges", " "))
	{
		ar(m_Params.bBridgeEdges, "BridgeEdges", "^^Bridge Edges");
		ar.closeBlock();
	}
	if (ar.openBlock("Multiple Offset", " "))
	{
		ar(m_Params.bMultipleOffset, "MultipleOffset", "^^Multiple Offset");
		ar.closeBlock();
	}
	DISPLAY_MESSAGE("ALT + LMB : Repeat the previous");

	if (ar.isEdit())
	{
		m_OffsetManipulators.clear();

		DesignerSession* pSession = DesignerSession::GetInstance();

		if (!m_Params.bMultipleOffset)
		{
			pSession->GetSelectedElements()->Clear();
			pSession->ClearPolygonSelections();
		}
	}
}

bool OffsetTool::IsManipulatorVisible() 
{ 
	DesignerSession* pSession = DesignerSession::GetInstance();
	return m_Params.bMultipleOffset && !pSession->GetSelectedElements()->IsEmpty();
}

void OffsetTool::Enter()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	ElementSet backupSelectedElements(*pSelected);

	__super::Enter();
	m_OffsetManipulators.clear();

	if (backupSelectedElements.GetCount() > 1)
	{
		pSelected->Set(backupSelectedElements);
		m_Params.bMultipleOffset = true;
		pSession->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
		pSession->UpdateSelectionMeshFromSelectedElements();
	}
}

bool OffsetTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Params.bMultipleOffset)
	{
		OnLButtonDownForMultipleOffset(view, nFlags, point);
		return true;
	}

	if (Deprecated::CheckVirtualKey(VK_MENU))
	{
		CUndo undo("Designer : Offset");
		GetModel()->RecordUndo("Offset", GetBaseObject());
		RepeatPrevAction(QueryOffsetPolygon(view, point));
		m_OffsetManipulators.clear();
		return true;
	}

	m_OffsetManipulators.clear();
	PolygonPtr polygon = QueryOffsetPolygon(view, point);
	if (!polygon)
		return true;

	m_MouseDownPos = point;
	OffsetManipulator* pOffsetManipulator = new OffsetManipulator;
	pOffsetManipulator->Init(polygon, GetModel());
	pOffsetManipulator->UpdateOffset(view, point);
	m_OffsetManipulators.push_back(pOffsetManipulator);

	DesignerSession::GetInstance()->ClearPolygonSelections();

	return true;
}

PolygonPtr OffsetTool::QueryOffsetPolygon(CViewport* view, CPoint point) const
{
	int nPolygonIndex = 0;
	if (GetModel()->QueryPolygon(GetDesigner()->GetRay(), nPolygonIndex))
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(nPolygonIndex);
		if (!pPolygon->IsOpen())
			return pPolygon;
	}
	return NULL;
}

bool OffsetTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Params.bMultipleOffset)
	{
		OnLButtonUpForMultipleOffset(view, nFlags, point);
		return true;
	}

	if (!GetOffsetManipulator())
		return true;

	CUndo undo("Designer : Offset");
	GetModel()->RecordUndo("Offset", GetBaseObject());
	RegisterScaledPolygons();
	InitializeSelection();

	return true;
}

bool OffsetTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	DesignerSession* pSession = DesignerSession::GetInstance();

	if (m_Params.bMultipleOffset)
	{
		OnMouseMoveForMultipleOffset(view, nFlags, point);
		return true;
	}

	if (!GetOffsetManipulator() || !(nFlags & MK_LBUTTON))
	{
		int nPolygonIndex(-1);
		pSession->UpdateSelectionMesh(NULL, GetCompiler(), GetBaseObject());
		if (GetModel()->QueryPolygon(GetDesigner()->GetRay(), nPolygonIndex))
		{
			PolygonPtr pPolygon = GetModel()->GetPolygon(nPolygonIndex);
			if (!pPolygon->CheckFlags(ePolyFlag_Mirrored) && !pPolygon->IsOpen())
				pSession->UpdateSelectionMesh(pPolygon, GetCompiler(), GetBaseObject());
		}
	}
	else if (!IsTwoPointsEquivalent(point, m_MouseDownPos))
	{
		UpdateAllOffsetManipulators(view, point);
	}

	return true;
}

void OffsetTool::ApplyOffset(
  Model* pModel,
  PolygonPtr pScaledPolygon,
  PolygonPtr pOriginalPolygon,
  const std::vector<PolygonPtr>& holes,
  bool bBridgeEdges)
{
	for (int i = 0, iHoleCount(holes.size()); i < iHoleCount; ++i)
		pScaledPolygon->Intersect(holes[i]);

	pModel->AddSplitPolygon(pScaledPolygon);

	if (!bBridgeEdges)
		return;

	std::map<int, BrushLine> correspondingLineMap;
	PolygonPtr pResizedPolygon = pOriginalPolygon->Clone();
	pResizedPolygon->Scale((BrushFloat)0.01);

	for (int i = 0, iCount(pResizedPolygon->GetVertexCount()); i < iCount; ++i)
	{
		int nCorrespondingIndex = 0;
		if (!pOriginalPolygon->GetNearestVertexIndex(pResizedPolygon->GetPos(i), nCorrespondingIndex))
		{
			DESIGNER_ASSERT(0);
			correspondingLineMap.clear();
			break;
		}
		BrushVec2 vFromSource = pOriginalPolygon->GetPlane().W2P(pOriginalPolygon->GetPos(nCorrespondingIndex));
		BrushVec2 vToResized = pOriginalPolygon->GetPlane().W2P(pResizedPolygon->GetPos(i));
		correspondingLineMap[nCorrespondingIndex] = BrushLine(vFromSource, vToResized);
	}

	for (int i = 0, iVertexCount(pScaledPolygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		BrushVec3 vInOffsetPolygon = pScaledPolygon->GetPos(i);

		auto ii = correspondingLineMap.begin();
		std::map<BrushFloat, BrushVec3> candidatedVertices;

		for (; ii != correspondingLineMap.end(); ++ii)
		{
			int nVertexIndexInSelectedPolygon = ii->first;
			const BrushLine& correspondingLine = ii->second;

			if (std::abs(correspondingLine.Distance(pOriginalPolygon->GetPlane().W2P(vInOffsetPolygon))) < kDesignerLooseEpsilon)
			{
				BrushVec3 vInSelectedPolygon = pOriginalPolygon->GetPos(nVertexIndexInSelectedPolygon);
				candidatedVertices[vInSelectedPolygon.GetDistance(vInOffsetPolygon)] = vInSelectedPolygon;
			}
		}

		if (!candidatedVertices.empty())
		{
			std::vector<BrushVec3> vList;
			vList.push_back(candidatedVertices.begin()->second);
			vList.push_back(vInOffsetPolygon);
			PolygonPtr pPolygon = new Polygon(
			  vList,
			  pOriginalPolygon->GetPlane(),
			  pOriginalPolygon->GetSubMatID(),
			  &(pOriginalPolygon->GetTexInfo()), false);
			if (pPolygon->IsValid())
				pModel->AddSplitPolygon(pPolygon);
		}
	}
}

void OffsetTool::AddScaledPolygon(
  PolygonPtr pScaledPolygon,
  PolygonPtr pOriginalPolygon,
  const std::vector<PolygonPtr>& holes,
  bool bBridgeEdges)
{
	ApplyOffset(GetModel(), pScaledPolygon, pOriginalPolygon, holes, bBridgeEdges);
	MirrorUtil::UpdateMirroredPartWithPlane(GetModel(), pScaledPolygon->GetPlane());
	UpdateSurfaceInfo();
	ApplyPostProcess(ePostProcess_ExceptMirror);
}

void OffsetTool::Display(DisplayContext& dc)
{
	if (m_Params.bMultipleOffset)
		__super::Display(dc);

	auto ii = m_OffsetManipulators.begin();
	for (; ii != m_OffsetManipulators.end(); ++ii)
		(*ii)->Display(dc, *ii == GetOffsetManipulator());
}

void OffsetTool::RepeatPrevAction(PolygonPtr polygon)
{
	if (!polygon || fabs(m_fPrevScale) < kDesignerEpsilon)
		return;
	PolygonPtr pOffsetPolygon = polygon->Clone();
	ApplyScaleCheckingBoundary(pOffsetPolygon, m_fPrevScale);
	std::vector<PolygonPtr> holes = polygon->GetLoops()->GetHoleClones();
	AddScaledPolygon(pOffsetPolygon, polygon, holes, m_Params.bBridgeEdges);
}

void OffsetTool::OnLButtonDownForMultipleOffset(CViewport* view, UINT nFlags, CPoint point)
{
	ElementSet* pSelect = DesignerSession::GetInstance()->GetSelectedElements();
	PolygonPtr polygon = QueryOffsetPolygon(view, point);
	if (polygon && pSelect->HasPolygonSelected(polygon))
	{
		if (Deprecated::CheckVirtualKey(VK_MENU))
		{
			CUndo undo("Designer : Offset");
			GetModel()->RecordUndo("Offset", GetBaseObject());
			std::vector<PolygonPtr> polygons = pSelect->GetAllPolygons();
			for (auto ii = polygons.begin(); ii != polygons.end(); ++ii)
				RepeatPrevAction(*ii);
			InitializeSelection();
			return;
		}

		for (int i = 0, iCount(pSelect->GetCount()); i < iCount; ++i)
		{
			const Element& element = pSelect->Get(i);
			if (!element.IsPolygon())
				continue;
			OffsetManipulator* pOffsetManipulator = new OffsetManipulator;
			pOffsetManipulator->Init(element.m_pPolygon, GetModel());
			if (element.m_pPolygon == polygon)
				m_OffsetManipulators.insert(m_OffsetManipulators.begin(), pOffsetManipulator);
			else
				m_OffsetManipulators.push_back(pOffsetManipulator);
		}

		m_MouseDownPos = point;
		UpdateAllOffsetManipulators(view, point);
	}
	else
	{
		m_OffsetManipulators.clear();
		__super::OnLButtonDown(view, nFlags, point);
	}
}

void OffsetTool::OnMouseMoveForMultipleOffset(CViewport* view, UINT nFlags, CPoint point)
{
	if (!m_OffsetManipulators.empty())
	{
		if (nFlags & MK_LBUTTON)
			UpdateAllOffsetManipulators(view, point);
	}
	else
	{
		__super::OnMouseMove(view, nFlags, point);
	}
}

void OffsetTool::OnLButtonUpForMultipleOffset(CViewport* view, UINT nFlags, CPoint point)
{
	if (!m_OffsetManipulators.empty())
	{
		if (!GetOffsetManipulator())
			return;
		CUndo undo("Designer : Offset");
		GetModel()->RecordUndo("Offset", GetBaseObject());
		RegisterScaledPolygons();
		InitializeSelection();
	}
	else
	{
		__super::OnLButtonUp(view, nFlags, point);
	}
}

OffsetManipulator* OffsetTool::GetOffsetManipulator() const
{
	if (m_OffsetManipulators.empty())
		return NULL;
	return m_OffsetManipulators[0];
}

void OffsetTool::UpdateAllOffsetManipulators(CViewport* view, CPoint point)
{
	if (!GetOffsetManipulator())
		return;

	GetOffsetManipulator()->UpdateOffset(view, point);
	BrushFloat scale = GetOffsetManipulator()->GetScale();
	for (auto ii = m_OffsetManipulators.begin(); ii != m_OffsetManipulators.end(); ++ii)
	{
		if (*ii == GetOffsetManipulator())
			continue;
		(*ii)->UpdateOffset(scale);
	}
}

void OffsetTool::InitializeSelection()
{
	DesignerSession* pSession = DesignerSession::GetInstance();

	pSession->GetSelectedElements()->Clear();
	pSession->ClearPolygonSelections();
	m_OffsetManipulators.clear();
}

void OffsetTool::RegisterScaledPolygons()
{
	m_fPrevScale = GetOffsetManipulator()->GetScale();
	for (auto ii = m_OffsetManipulators.begin(); ii != m_OffsetManipulators.end(); ++ii)
	{
		AddScaledPolygon(
		  (*ii)->GetScaledPolygon(),
		  (*ii)->GetPolygon(),
		  (*ii)->GetHoles(),
		  m_Params.bBridgeEdges);
	}
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Offset, eToolGroup_Edit, "Offset", OffsetTool,
                                                           offset, "runs offset tool", "designer.offset")

