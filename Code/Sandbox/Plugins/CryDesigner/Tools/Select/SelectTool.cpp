// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Tools/Select/SelectTool.h"
#include "Viewport.h"
#include "DesignerEditor.h"
#include "Util/Undo.h"
#include "Core/Model.h"
#include "Tools/Edit/RemoveTool.h"
#include "Core/PolygonDecomposer.h"
#include "Core/Helper.h"
#include "ToolFactory.h"
#include "Util/Display.h"
#include "Util/ExcludedEdgeManager.h"
#include "DesignerSession.h"

namespace Designer
{
void SelectTool::Enter()
{
	__super::Enter();
	if (GetModel())
		GetModel()->ResetDB(eDBRF_ALL);

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	pSelected->RemoveInvalidElements();
	pSession->UpdateSelectionMeshFromSelectedElements();
}

bool SelectTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	int nPolygonIndex(0);

	if (m_bAllowSelectionUndo)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		GetDesigner()->StoreSelectionUndo();
	}

	bool bOnlyUseSelectionCube = GetKeyState(VK_SPACE) & (1 << 15);
	bool bRemoveSelectedElements = GetKeyState(VK_MENU) & (1 << 15);

	ElementSet pickedElements;
	if (!bRemoveSelectedElements)
	{
		pickedElements.PickFromModel(GetBaseObject(), GetModel(), view, point, m_nPickFlag, bOnlyUseSelectionCube, &m_PickedPosAsLMBDown);
	}

	bool bPicked = pickedElements.IsEmpty() ? false : true;

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	if (nFlags & MK_CONTROL)
	{
		m_InitialSelectionElementsInRectangleSel = *pSelected;

		if (!bPicked)
		{
			m_SelectionType = eST_RectangleSelection;
			view->SetSelectionRectangle(point, point);
			m_MouseDownPos = point;
		}
		else
		{
			m_SelectionType = eST_NormalSelection;
			if (!pSelected->Erase(pickedElements))
				pSelected->Add(pickedElements);
		}
	}
	else if (bRemoveSelectedElements)
	{
		if (!pSelected->IsEmpty())
		{
			m_InitialSelectionElementsInRectangleSel = *pSelected;
			m_SelectionType = eST_EraseSelectionInRectangle;
			view->SetSelectionRectangle(point, point);
			m_MouseDownPos = point;
		}
	}
	else
	{
		pSelected->Clear();
		if (!bPicked)
		{
			m_InitialSelectionElementsInRectangleSel.Clear();
			m_SelectionType = eST_RectangleSelection;
			view->SetSelectionRectangle(point, point);
		}
		else
		{
			m_SelectionType = eST_JustAboutToTransformSelectedElements;
		}
		m_MouseDownPos = point;
		pSelected->Add(pickedElements);
	}

	SetSubMatIDFromLastElement(*pSelected);

	GetDesigner()->UpdateTMManipulatorBasedOnElements(DesignerSession::GetInstance()->GetSelectedElements());
	pSession->UpdateSelectionMeshFromSelectedElements();
	pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
	UpdateDrawnEdges(GetMainContext());

	return true;
}

bool SelectTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_bAllowSelectionUndo)
		GetIEditor()->GetIUndoManager()->Accept("Designer : Selection");

	if (m_SelectionType == eST_RectangleSelection || m_SelectionType == eST_EraseSelectionInRectangle)
	{
		view->SetSelectionRectangle(CPoint(0, 0), CPoint(0, 0));
		DesignerSession* pSession = DesignerSession::GetInstance();
		GetDesigner()->UpdateTMManipulatorBasedOnElements(pSession->GetSelectedElements());
		pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
	}

	m_SelectionType = eST_Nothing;

	return true;
}

bool SelectTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	bool bOnlyUseSelectionCube = GetKeyState(VK_SPACE) & (1 << 15);
	ElementSet pickedElements;
	bool bPicked = pickedElements.PickFromModel(GetBaseObject(), GetModel(), view, point, m_nPickFlag, bOnlyUseSelectionCube, NULL);

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	if (!m_bHitGizmo)
		UpdateCursor(view, bPicked);

	if (m_SelectionType == eST_RectangleSelection)
	{
		SelectElementsInRectangle(view, point, bOnlyUseSelectionCube);
	}
	else if (m_SelectionType == eST_EraseSelectionInRectangle)
	{
		EraseElementsInRectangle(view, point, bOnlyUseSelectionCube);
	}
	else if ((nFlags & MK_CONTROL) && (nFlags & MK_LBUTTON))
	{
		if (bPicked)
		{
			pSelected->Add(pickedElements);
			pSession->UpdateSelectionMeshFromSelectedElements();
			UpdateDrawnEdges(GetMainContext());
		}
	}

	return true;
}

void SelectTool::SelectElementsInRectangle(CViewport* view, CPoint point, bool bOnlyUseSelectionCube)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	CRect rc(m_MouseDownPos, point);
	view->SetSelectionRectangle(rc.TopLeft(), rc.BottomRight());
	if (std::abs(rc.Width()) < 2 || std::abs(rc.Height()) < 2)
		return;

	pSession->GetExcludedEdgeManager()->Clear();
	*pSelected = m_InitialSelectionElementsInRectangleSel;

	CRect selectionRect = view->GetSelectionRectangle();
	PolygonPtr pRectPolygon = MakePolygonFromRectangle(selectionRect);

	if (selectionRect.top != selectionRect.bottom && selectionRect.left != selectionRect.right)
	{
		ElementSet selectionList;

		if (m_nPickFlag & ePF_Vertex)
		{
			ModelDB::QueryResult qResult;
			GetModel()->GetDB()->QueryAsRectangle(view, GetWorldTM(), selectionRect, qResult);
			for (int i = 0, iCount(qResult.size()); i < iCount; ++i)
				selectionList.Add(Element(GetBaseObject(), qResult[i].m_Pos));
		}

		if (m_nPickFlag & ePF_Edge)
		{
			std::vector<std::pair<BrushEdge3D, BrushVec3>> edges;
			GetModel()->QueryIntersectionEdgesWith2DRect(view, GetWorldTM(), pRectPolygon, false, edges);
			for (int i = 0, iCount(edges.size()); i < iCount; ++i)
			{
				selectionList.Add(Element(GetBaseObject(), edges[i].first));
				pSession->GetExcludedEdgeManager()->Add(edges[i].first);
			}
		}

		if (m_nPickFlag & ePF_Polygon)
		{
			std::vector<PolygonPtr> polygonList;
			if (!bOnlyUseSelectionCube)
				GetModel()->QueryIntersectionPolygonsWith2DRect(view, GetWorldTM(), pRectPolygon, false, polygonList);

			if (gDesignerSettings.bHighlightElements)
			{
				for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
				{
					PolygonPtr pPolygon = GetModel()->GetPolygon(i);
					if (DoesSelectionBoxIntersectRect(view, GetWorldTM(), selectionRect, pPolygon))
						polygonList.push_back(pPolygon);
				}
			}

			for (int i = 0, iPolygonCount(polygonList.size()); i < iPolygonCount; ++i)
				selectionList.Add(Element(GetBaseObject(), polygonList[i]));
		}

		pSelected->Add(selectionList);
	}

	pSession->UpdateSelectionMeshFromSelectedElements();
}

void SelectTool::EraseElementsInRectangle(CViewport* view, CPoint point, bool bOnlyUseSelectionCube)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	CRect rc(m_MouseDownPos, point);
	view->SetSelectionRectangle(rc.TopLeft(), rc.BottomRight());
	if (std::abs(rc.Width()) < 2 || std::abs(rc.Height()) < 2)
		return;

	pSession->GetExcludedEdgeManager()->Clear();
	*pSelected = m_InitialSelectionElementsInRectangleSel;

	CRect selectionRect = view->GetSelectionRectangle();

	if (selectionRect.top != selectionRect.bottom && selectionRect.left != selectionRect.right)
	{
		std::vector<Element> elementsInRect;
		pSelected->FindElementsInRect(selectionRect, view, GetWorldTM(), bOnlyUseSelectionCube, elementsInRect);

		for (int i = 0, iCount(elementsInRect.size()); i < iCount; ++i)
			pSelected->Erase(elementsInRect[i]);

		for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
		{
			if ((*pSelected)[i].IsEdge())
				pSession->GetExcludedEdgeManager()->Add((*pSelected)[i].GetEdge());
		}
	}

	pSession->UpdateSelectionMeshFromSelectedElements();
}

bool SelectTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	if (nChar == Qt::Key_Escape)
	{
		pSession->GetExcludedEdgeManager()->Clear();
		// return false, this gets handled by editor and quits the tool
		return false;
	}
	return true;
}

void SelectTool::SetSubMatIDFromLastElement(const ElementSet& elements)
{
	if (elements.IsEmpty())
		return;

	const Element& lastElement = elements.Get(elements.GetCount() - 1);
	if (!lastElement.IsPolygon() || !lastElement.m_pPolygon)
		return;

	DesignerSession::GetInstance()->SetCurrentSubMatID(lastElement.m_pPolygon->GetSubMatID());
}

void SelectTool::UpdateCursor(CViewport* view, bool bPickingElements)
{
	if (!bPickingElements)
	{
		view->SetCurrentCursor(STD_CURSOR_DEFAULT, "");
		return;
	}
	else if (GetIEditor()->GetEditMode() == eEditModeMove)
		view->SetCurrentCursor(STD_CURSOR_MOVE, "");
	else if (GetIEditor()->GetEditMode() == eEditModeRotate)
		view->SetCurrentCursor(STD_CURSOR_ROTATE, "");
	else if (GetIEditor()->GetEditMode() == eEditModeScale)
		view->SetCurrentCursor(STD_CURSOR_SCALE, "");
}

void SelectTool::Display(DisplayContext& dc)
{
	__super::Display(dc);

	if (gDesignerSettings.bHighlightElements)
	{
		DesignerSession* pSession = DesignerSession::GetInstance();
		ElementSet* pSelected = pSession->GetSelectedElements();
		Display::DisplayHighlightedElements(dc, GetMainContext(), m_nPickFlag, pSession->GetExcludedEdgeManager());
	}
}

void SelectTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected =pSession->GetSelectedElements();

	switch (event)
	{
	case eNotify_OnEndUndoRedo:
		for (int i = 0, iElementSize(pSelected->GetCount()); i < iElementSize; ++i)
		{
			Element element = pSelected->Get(i);

			if (!element.IsPolygon())
				continue;

			PolygonPtr pEquivalentPolygon = GetModel()->QueryEquivalentPolygon(element.m_pPolygon);
			if (pEquivalentPolygon)
			{
				element.m_pPolygon = pEquivalentPolygon;
				pSelected->Set(i, element);
			}
		}
		pSession->UpdateSelectionMeshFromSelectedElements();
		break;
	}
}

SelectTool::OrganizedQueryResults SelectTool::CreateOrganizedResultsAroundPolygonFromQueryResults(const ModelDB::QueryResult& queryResult)
{
	SelectTool::OrganizedQueryResults organizedQueryResults;
	int iQueryResultSize(queryResult.size());
	for (int i = 0; i < iQueryResultSize; ++i)
	{
		const ModelDB::Vertex& v = queryResult[i];
		for (int k = 0, iMarkListSize(v.m_MarkList.size()); k < iMarkListSize; ++k)
		{
			PolygonPtr pPolygon = v.m_MarkList[k].m_pPolygon;
			DESIGNER_ASSERT(pPolygon);
			if (!pPolygon)
				continue;

			if (pPolygon->CheckFlags(ePolyFlag_Mirrored))
				continue;

			organizedQueryResults[v.m_MarkList[k].m_pPolygon].push_back(QueryInput(i, k));
		}
	}
	return organizedQueryResults;
}

void SelectTool::SelectAllElements()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	bool bSelectedElementsExist = !pSelected->IsEmpty();
	pSelected->Clear();

	if (!bSelectedElementsExist)
	{
		ElementSet elements;
		for (int k = 0, iPolygonCount(GetModel()->GetPolygonCount()); k < iPolygonCount; ++k)
		{
			PolygonPtr pPolygon = GetModel()->GetPolygon(k);
			Element element;
			for (int i = 0, iVertexCount(pPolygon->GetVertexCount()); i < iVertexCount; ++i)
				element.m_Vertices.push_back(pPolygon->GetPos(i));
			element.m_pPolygon = pPolygon;
			element.m_pObject = GetBaseObject();
			elements.Add(element);
		}
		pSelected->Add(elements);
	}

	pSession->UpdateSelectionMeshFromSelectedElements();
	UpdateDrawnEdges(GetMainContext());
	GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
}

void SelectTool::InitalizePlaneAlignedWithView(CViewport* pView)
{
	m_PlaneAlignedWithView = BrushPlane(m_PickedPosAsLMBDown, m_PickedPosAsLMBDown + pView->GetViewTM().GetColumn0(), m_PickedPosAsLMBDown + pView->GetViewTM().GetColumn2());
}

BrushMatrix34 SelectTool::GetOffsetTMOnAlignedPlane(CViewport* pView, const BrushPlane& planeAlighedWithView, CPoint prevPos, CPoint currentPos)
{
	BrushMatrix34 offsetTM;
	offsetTM.SetIdentity();

	BrushRay localPrevRay;
	GetLocalViewRay(GetBaseObject()->GetWorldTM(), pView, prevPos, localPrevRay);

	BrushRay localCurrentRay;
	GetLocalViewRay(GetBaseObject()->GetWorldTM(), pView, currentPos, localCurrentRay);

	BrushVec3 prevHitPos;
	m_PlaneAlignedWithView.HitTest(localPrevRay, NULL, &prevHitPos);

	BrushVec3 currentHitPos;
	m_PlaneAlignedWithView.HitTest(localCurrentRay, NULL, &currentHitPos);

	offsetTM.SetTranslation(currentHitPos - prevHitPos);

	return offsetTM;
}
}

