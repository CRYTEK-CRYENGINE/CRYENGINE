// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MergeTool.h"
#include "DesignerEditor.h"
#include "Objects/DesignerObject.h"
#include "Tools/Modify/MirrorTool.h"
#include "Tools/Select/SelectTool.h"
#include "Core/Helper.h"

namespace Designer
{
std::vector<PolygonPtr> GetPolygonList(ElementSet* pElements)
{
	std::vector<PolygonPtr> polygonList;
	for (int i = 0, iElementCount(pElements->GetCount()); i < iElementCount; ++i)
	{
		if ((*pElements)[i].IsPolygon())
			polygonList.push_back((*pElements)[i].m_pPolygon);
	}
	return polygonList;
}

void MergeTool::Enter()
{
	std::vector<MainContext> selections;
	GetSelectedObjectList(selections);

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	std::vector<PolygonPtr> selectedPolygons = GetPolygonList(pSelected);

	if (selections.size() < 2 && selectedPolygons.size() < 2)
	{
		MessageBox("Warning", "More than 2 designer objects or 2 polygons should be selected to be merged.");
		
		// Should actually exit. This is not a modal operator
		GetDesigner()->SwitchTool(eDesigner_Invalid);
		return;
	}

	CUndo undo("Designer : Object Merge");

	if (selections.size() >= 2)
		MergeObjects();
	else if (selectedPolygons.size() >= 2)
		MergePolygons();
}

void MergeTool::MergeObjects()
{
	std::vector<MainContext> selections;
	GetSelectedObjectList(selections);

	int nSelectionCount = selections.size();
	MainContext& lastSelection = selections[nSelectionCount - 1];

	lastSelection.pModel->RecordUndo("Designer : Merge", lastSelection.pObject);
	ResetXForm(lastSelection);

	for (int i = 0; i < nSelectionCount - 1; ++i)
	{
		MainContext& selection = selections[i];
		if (selection.pModel->CheckFlag(eModelFlag_Mirror))
		{
			MirrorTool::ReleaseMirrorMode(selection.pModel);
			MirrorTool::RemoveEdgesOnMirrorPlane(selection.pModel);
		}
		MergeTwoObjects(lastSelection, selection);
	}

	Designer::ApplyPostProcess(lastSelection);

	DesignerSession::GetInstance()->SetBaseObject(nullptr);
	for (int i = 0; i < nSelectionCount - 1; ++i)
		GetIEditor()->DeleteObject(selections[i].pObject);

	GetIEditor()->SelectObject(lastSelection.pObject);

	DesignerEditor* pDesignerTool = GetDesigner();
	if (pDesignerTool)
	{
		DesignerSession::GetInstance()->SetBaseObject(lastSelection.pObject);
		// again, non modal, should actually quit here
		pDesignerTool->SwitchTool(eDesigner_Invalid);
	}
}

void MergeTool::MergePolygons(MainContext& mc)
{
	std::vector<PolygonPtr> selectedPolygons = GetPolygonList(DesignerSession::GetInstance()->GetSelectedElements());
	int nSelectedPolygonCount = selectedPolygons.size();

	std::set<PolygonPtr> usedPolygons;

	while (usedPolygons.size() < nSelectedPolygonCount)
	{
		PolygonPtr pPolygon = NULL;
		for (int i = 0; i < nSelectedPolygonCount; ++i)
		{
			if (usedPolygons.find(selectedPolygons[i]) == usedPolygons.end())
			{
				pPolygon = selectedPolygons[i];
				break;
			}
		}

		if (pPolygon == NULL)
			break;

		bool bMerged = false;
		for (int i = 0; i < nSelectedPolygonCount; ++i)
		{
			if (pPolygon == selectedPolygons[i] || usedPolygons.find(selectedPolygons[i]) != usedPolygons.end())
				continue;

			if (eIT_None == Polygon::HasIntersection(pPolygon, selectedPolygons[i]) || pPolygon->GetSubMatID() != selectedPolygons[i]->GetSubMatID())
				continue;

			pPolygon->Union(selectedPolygons[i]);
			mc.pModel->RemovePolygon(selectedPolygons[i]);
			usedPolygons.insert(selectedPolygons[i]);
			bMerged = true;
		}

		if (!bMerged)
			usedPolygons.insert(pPolygon);
	}
}

void MergeTool::MergePolygons()
{
	GetModel()->RecordUndo("Designer : Merge Polygons", GetBaseObject());
	MergePolygons(MainContext(GetBaseObject(), GetCompiler(), GetModel()));
	ApplyPostProcess();
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Merge, eToolGroup_Edit, "Merge", MergeTool,
                                   merge, "runs merge tool", "designer.merge")

