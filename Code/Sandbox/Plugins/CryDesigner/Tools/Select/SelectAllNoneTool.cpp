// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectAllNoneTool.h"
#include "DesignerEditor.h"
#include "Tools/Select/SelectTool.h"
#include "DesignerSession.h"

namespace Designer
{
void SelectAllNoneTool::Enter()
{
	CUndo undo("Designer : All or None Selection");
	GetDesigner()->StoreSelectionUndo();

	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();

	bool bSelectedElementsExist = !pSelected->IsEmpty();
	pSelected->Clear();

	if (!bSelectedElementsExist)
	{
		EDesignerTool prevDesignerMode = GetDesigner()->GetPrevTool();
		EDesignerTool originalPrevDesignerMode = prevDesignerMode;
		if (!IsSelectElementMode(prevDesignerMode))
		{
			prevDesignerMode = GetDesigner()->GetPrevSelectTool();
			GetDesigner()->SetPrevTool(prevDesignerMode);
		}
		if (prevDesignerMode & eDesigner_Edge)
			SelectAllEdges(GetBaseObject(), GetModel());
		else if (prevDesignerMode & eDesigner_Vertex)
			SelectAllVertices(GetBaseObject(), GetModel());
		else
			SelectAllFaces(GetBaseObject(), GetModel());
		GetDesigner()->SetPrevTool(originalPrevDesignerMode);
	}

	UpdateDrawnEdges(GetMainContext());
	GetDesigner()->UpdateTMManipulatorBasedOnElements(pSelected);
	GetDesigner()->SwitchToPrevTool();
	pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
}

void SelectAllNoneTool::SelectAllVertices(CBaseObject* pObject, Model* pModel)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		for (int k = 0, iVertexCount(pPolygon->GetVertexCount()); k < iVertexCount; ++k)
			pSelected->Add(Element(pObject, pPolygon->GetPos(k)));
	}
}

void SelectAllNoneTool::SelectAllEdges(CBaseObject* pObject, Model* pModel)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		for (int k = 0, iEdgeCount(pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
		{
			BrushEdge3D e = pPolygon->GetEdge(k);
			pSelected->Add(Element(pObject, e));
		}
	}
}

void SelectAllNoneTool::SelectAllFaces(CBaseObject* pObject, Model* pModel)
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = pModel->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden))
			continue;

		pSelected->Add(Element(pObject, pPolygon));
	}
}

void SelectAllNoneTool::DeselectAllVertices()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Erase(ePF_Vertex);
}

void SelectAllNoneTool::DeselectAllEdges()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Erase(ePF_Edge);
}

void SelectAllNoneTool::DeselectAllFaces()
{
	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Erase(ePF_Polygon);
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_AllNone, eToolGroup_Selection, "AllNone", SelectAllNoneTool,
                                   allnoneselection, "runs All/None selection tool", "designer.allnoneselection")

