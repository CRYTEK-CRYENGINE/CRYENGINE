// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExtrudeEdgeTool.h"
#include "Util/ElementSet.h"
#include "Util/ExcludedEdgeManager.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "Serialization/Decorators/EditorActionButton.h"

using Serialization::ActionButton;

namespace Designer
{
void ExtrudeEdgeTool::Enter()
{
	__super::Enter();
	m_bManipulatingGizmo = false;
	;
	m_bHitGizmo = false;
	m_CreatedPolygons.clear();

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelection = pSession->GetSelectedElements();

	pSession->GetExcludedEdgeManager()->Clear();
	for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
	{
		const Element& element = pSelection->Get(i);
		if (element.IsEdge())
			pSession->GetExcludedEdgeManager()->Add(element.GetEdge());
	}

	GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelection);
}

void ExtrudeEdgeTool::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("FlipPolygons", " "))
	{
		ar(ActionButton([this] { FlipPolygons();
		                }), "FlipPolygons", "^^Flip Polygons");
		ar.closeBlock();
	}

	DISPLAY_MESSAGE("CTRL + LMB or Dragging : Select several");
}

bool ExtrudeEdgeTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnLButtonUp(view, nFlags, point);
	FinishExtrusion();
	AcceptUndo();
	return true;
}

bool ExtrudeEdgeTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnMouseMove(view, nFlags, point);

	if (nFlags & MK_LBUTTON)
	{
		if (m_SelectionType == eST_JustAboutToTransformSelectedElements)
		{
			if (std::abs(m_MouseDownPos.x - point.x) > 10 || std::abs(m_MouseDownPos.y - point.y) > 10)
			{
				if (DesignerSession::GetInstance()->GetSelectedElements()->IsEmpty())
					return true;
				StartUndo();
				InitalizePlaneAlignedWithView(view);
				PrepareExtrusion();
				m_SelectionType = eST_TransformSelectedElements;
			}
		}
		else if (m_SelectionType == eST_TransformSelectedElements)
		{
			Extrude(GetOffsetTMOnAlignedPlane(view, m_PlaneAlignedWithView, m_MouseDownPos, point));
		}
	}

	return true;
}

void ExtrudeEdgeTool::OnManipulatorEnd(
	IDisplayViewport* pView,
	ITransformManipulator* pManipulator)
{
	m_bHitGizmo = false;
	if (m_bManipulatingGizmo)
	{
		m_bManipulatingGizmo = false;
		FinishExtrusion();
		AcceptUndo();
	}
}


void ExtrudeEdgeTool::OnManipulatorBegin(
  IDisplayViewport* pView,
  ITransformManipulator* pManipulator,
  CPoint& point,
  int flags)
{
	m_bHitGizmo = true;

	if (!m_bManipulatingGizmo)
	{
		m_bManipulatingGizmo = true;
		StartUndo();
		PrepareExtrusion();
	}
}

void ExtrudeEdgeTool::OnManipulatorDrag(
  IDisplayViewport* pView,
  ITransformManipulator* pManipulator,
  CPoint& p0,
  BrushVec3 value,
  int nFlags)
{
	BrushMatrix34 offsetTM = GetOffsetTM(pManipulator, value, GetWorldTM());
	Extrude(offsetTM);
}

void ExtrudeEdgeTool::PrepareExtrusion()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	m_EdgePairs.clear();

	for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
	{
		const Element& e = pSelected->Get(i);
		if (e.IsEdge())
		{
			std::vector<PolygonPtr> polygons;
			GetModel()->QueryPolygonsHavingEdge(e.GetEdge(), polygons);
			auto ii = polygons.begin();
			for (; ii != polygons.end(); ++ii)
			{
				if ((*ii)->IsOpen())
					GetModel()->EraseEdge(e.GetEdge());
			}
			m_EdgePairs.push_back(std::pair<BrushEdge3D, BrushEdge3D>(e.GetEdge(), e.GetEdge()));
		}
	}
}

void ExtrudeEdgeTool::Extrude(const Matrix34& offsetTM)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	pSelected->Clear();

	pSession->GetExcludedEdgeManager()->Clear();

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	for (int i = 0, iCount(m_EdgePairs.size()); i < iCount; ++i)
	{
		m_EdgePairs[i].second.m_v[0] = offsetTM.TransformPoint(m_EdgePairs[i].first.m_v[0]);
		m_EdgePairs[i].second.m_v[1] = offsetTM.TransformPoint(m_EdgePairs[i].first.m_v[1]);

		std::vector<BrushVec3> vList;
		vList.push_back(m_EdgePairs[i].first.m_v[1]);
		vList.push_back(m_EdgePairs[i].first.m_v[0]);
		vList.push_back(m_EdgePairs[i].second.m_v[0]);
		vList.push_back(m_EdgePairs[i].second.m_v[1]);
		PolygonPtr polygon = new Polygon(vList);
		GetModel()->AddPolygon(polygon);

		pSelected->Add(Element(GetBaseObject(), m_EdgePairs[i].second));
		pSession->GetExcludedEdgeManager()->Add(m_EdgePairs[i].second);
	}

	ApplyPostProcess(ePostProcess_Mirror);
	GetCompiler()->Compile(GetBaseObject(), GetModel(), eShelf_Construction);
	GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
}

void ExtrudeEdgeTool::FinishExtrusion()
{
	m_EdgePairs.clear();

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
		m_CreatedPolygons.push_back(GetModel()->GetPolygon(i));

	GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);
	ApplyPostProcess();
}

void ExtrudeEdgeTool::StartUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
	GetDesigner()->StoreSelectionUndo(false);
	GetModel()->RecordUndo("Designer : Edge Extrusion", GetBaseObject());
}

void ExtrudeEdgeTool::AcceptUndo()
{
	GetIEditor()->GetIUndoManager()->Accept("Designer : Edge Extrusion");
}

void ExtrudeEdgeTool::FlipPolygons()
{
	ElementSet* pSelection = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
	{
		Element element = pSelection->Get(i);
		if (!element.IsEdge())
			continue;
		BrushEdge3D edge = element.GetEdge();
		std::swap(edge.m_v[0], edge.m_v[1]);
		pSelection->Set(i, Element(GetBaseObject(), edge));
	}

	auto ii = m_CreatedPolygons.begin();
	for (; ii != m_CreatedPolygons.end(); ++ii)
		(*ii)->Flip();

	ApplyPostProcess();
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_ExtrudeEdge, eToolGroup_Edit, "Extrude Edge", ExtrudeEdgeTool,
                                                           extrudeedge, "runs extrude edge tool", "designer.extrudeedge")

