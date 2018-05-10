// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectTool.h"
#include "../UVMappingEditor.h"
#include "Core/UVIslandManager.h"
#include "../UVUndo.h"
#include "Util/ElementSet.h"
#include "DesignerEditor.h"

namespace Designer {
namespace UVMapping
{
SelectTool::SelectTool(EUVMappingTool tool) : BaseTool(tool)
{
	if (tool == eUVMappingTool_Vertex)
		m_SelectionFlag = eSelectionFlag_Vertex;

	if (tool == eUVMappingTool_Edge)
		m_SelectionFlag = eSelectionFlag_Edge;

	if (tool == eUVMappingTool_Polygon)
		m_SelectionFlag = eSelectionFlag_Polygon;

	if (tool == eUVMappingTool_Island)
		m_SelectionFlag = eSelectionFlag_Island;
}

void SelectTool::InitSelection()
{
	m_pRectangleSelectionContext = NULL;
	m_bUVCursorSelected = false;
}

void SelectTool::ChooseSelectionType(const SelectionContext& sc)
{
	bool bFound = sc.uvElement.IsUVIsland() && sc.uvElement.m_pUVIsland || !sc.uvElement.m_UVVertices.empty();
	bool bSelectExisting = bFound && sc.elementSet->FindElement(sc.uvElement) != NULL;

	if (!sc.multipleSelection && !bSelectExisting)
	{
		GetUVEditor()->ClearSharedSelection();
		sc.elementSet->Clear();
	}

	if (bFound)
	{
		if (sc.elementSet->HasElement(sc.uvElement))
		{
			if (sc.multipleSelection)
			{
				sc.elementSet->EraseElement(sc.uvElement);
			}
			else
			{
				sc.elementSet->Clear();
				sc.elementSet->AddElement(sc.uvElement);
			}
		}
		else
		{
			sc.elementSet->AddElement(sc.uvElement);
		}
		GetUVEditor()->UpdateGizmoPos();
		SyncSelection();
	}
	else
	{
		*(sc.rectSelectionContext) = new RectangleSelectionContext(sc.multipleSelection, SearchUtil::FindHitPointToUVPlane(sc.mouseEvent->viewport, sc.mouseEvent->x, sc.mouseEvent->y));
	}
}

void SelectTool::OnLButtonDown(const SMouseEvent& me)
{
	InitSelection();

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	if (pUVIslandMgr == NULL)
		return;

	Ray ray;
	if (!me.viewport->ScreenToWorldRay(&ray, me.x, me.y))
		return;

	UVElementSetPtr pElements = GetUVEditor()->GetElementSet();
	UVCursorPtr pUVCursor = GetUVEditor()->GetUVCursor();

	if (!me.control && pUVCursor->IsPicked(ray))
	{
		m_bUVCursorSelected = true;
		return;
	}

	CUndo undo("UVMapping Editor : Selection");
	CUndo::Record(new UVSelectionUndo);

	bool bEmptyBefore = pElements->IsEmpty();

	if (m_SelectionFlag == eSelectionFlag_Island)
	{
		std::vector<UVIslandPtr> uvIslands;
		SearchUtil::FindUVIslands(ray, uvIslands);

		SelectionContext sc;
		sc.elementSet = pElements;
		sc.multipleSelection = me.control;
		sc.rectSelectionContext = &m_pRectangleSelectionContext;
		sc.uvElement = !uvIslands.empty() ? UVElement(uvIslands[0]) : UVElement();
		sc.mouseEvent = &me;
		ChooseSelectionType(sc);
	}
	else if (m_SelectionFlag == eSelectionFlag_Polygon)
	{
		std::vector<UVPolygon> uvPolygons;
		SearchUtil::FindPolygons(ray, uvPolygons);

		SelectionContext sc;
		sc.elementSet = pElements;
		sc.multipleSelection = me.control;
		sc.rectSelectionContext = &m_pRectangleSelectionContext;
		sc.uvElement = !uvPolygons.empty() ? UVElement(uvPolygons[0]) : UVElement();
		sc.mouseEvent = &me;
		ChooseSelectionType(sc);
	}
	else if (m_SelectionFlag == eSelectionFlag_Edge)
	{
		std::vector<UVEdge> uvEdges;
		SearchUtil::FindEdges(ray, uvEdges);

		SelectionContext sc;
		sc.elementSet = pElements;
		sc.multipleSelection = me.control;
		sc.rectSelectionContext = &m_pRectangleSelectionContext;
		sc.uvElement = !uvEdges.empty() ? UVElement(uvEdges[0]) : UVElement();
		sc.mouseEvent = &me;
		ChooseSelectionType(sc);
	}
	else if (m_SelectionFlag == eSelectionFlag_Vertex)
	{
		std::vector<UVVertex> uvVertices;
		SearchUtil::FindVertices(ray, uvVertices);

		SelectionContext sc;
		sc.elementSet = pElements;
		sc.multipleSelection = me.control;
		sc.rectSelectionContext = &m_pRectangleSelectionContext;
		sc.uvElement = !uvVertices.empty() ? UVElement(uvVertices[0]) : UVElement();
		sc.mouseEvent = &me;
		ChooseSelectionType(sc);
	}
}

void SelectTool::OnLButtonUp(const SMouseEvent& me)
{
	InitSelection();
}

void SelectTool::OnMouseMove(const SMouseEvent& me)
{
	if (m_pRectangleSelectionContext)
	{
		m_pRectangleSelectionContext->end = SearchUtil::FindHitPointToUVPlane(me.viewport, me.x, me.y);
		m_pRectangleSelectionContext->Select(m_SelectionFlag, me);
		GetUVEditor()->UpdateGizmoPos();
		SyncSelection();
	}
}

void SelectTool::Display(DisplayContext& dc)
{
	if (m_pRectangleSelectionContext)
		m_pRectangleSelectionContext->Draw(dc);
}

RectangleSelectionContext::RectangleSelectionContext(bool multipleselection, const Vec3& _start) : start(_start), end(_start)
{
	m_pSelectedElements = multipleselection ? GetUVEditor()->GetElementSet()->Clone() : new UVElementSet;
}

void RectangleSelectionContext::Draw(DisplayContext& dc)
{
	float z = 0.001f;

	Vec3 v[] = {
		Vec3(start.x, start.y, z),
		Vec3(end.x,   start.y, z),
		Vec3(end.x,   end.y,   z),
		Vec3(start.x, end.y,   z)
	};

	dc.SetColor(ColorB(255, 255, 255));
	dc.DrawPolyLine(v, sizeof(v) / sizeof(*v), true);
}

void RectangleSelectionContext::Select(ESelectionFlag flag, const SMouseEvent& me)
{
	AABB aabb;
	aabb.Reset();
	aabb.Add(start);
	aabb.Add(end);
	aabb.Expand(Vec3(0.001f));

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	UVElementSetPtr pElements = GetUVEditor()->GetElementSet();

	*pElements = *m_pSelectedElements;

	if (flag == eSelectionFlag_Island)
	{
		std::vector<UVIslandPtr> uvIslands;
		SearchUtil::FindUVIslands(aabb, uvIslands);
		for (int i = 0, iCount(uvIslands.size()); i < iCount; ++i)
			pElements->AddElement(UVElement(uvIslands[i]));
	}
	else if (flag == eSelectionFlag_Polygon)
	{
		std::vector<UVPolygon> uvPolygons;
		SearchUtil::FindPolygons(aabb, uvPolygons);
		for (int i = 0, iCount(uvPolygons.size()); i < iCount; ++i)
			pElements->AddElement(UVElement(uvPolygons[i]));
	}
	else if (flag == eSelectionFlag_Edge)
	{
		std::vector<UVEdge> uvEdges;
		SearchUtil::FindEdges(aabb, uvEdges);
		for (int i = 0, iCount(uvEdges.size()); i < iCount; ++i)
			pElements->AddElement(UVElement(uvEdges[i]));
	}
	else if (flag == eSelectionFlag_Vertex)
	{
		std::vector<UVVertex> uvVertices;
		SearchUtil::FindVertices(aabb, uvVertices);
		for (int i = 0, iCount(uvVertices.size()); i < iCount; ++i)
			pElements->AddElement(UVElement(uvVertices[i]));
	}
}
}
}

