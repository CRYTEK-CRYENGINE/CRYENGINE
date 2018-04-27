// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HidePolygonTool.h"
#include "Tools/Select/SelectTool.h"
#include "DesignerEditor.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "DesignerSession.h"

using Serialization::ActionButton;

namespace Designer
{
void HidePolygonTool::Enter()
{
	__super::Enter();

	if (HideSelectedPolygons())
		GetDesigner()->SwitchTool(eDesigner_Polygon);
}

bool HidePolygonTool::HideSelectedPolygons()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	std::vector<PolygonPtr> selectedPolygons;

	for (int i = 0, iCount(pSelected->GetCount()); i < iCount; ++i)
	{
		if (!(*pSelected)[i].IsPolygon() || (*pSelected)[i].m_pPolygon == NULL)
			continue;
		selectedPolygons.push_back((*pSelected)[i].m_pPolygon);
	}

	if (selectedPolygons.empty())
		return false;

	CUndo undo("Designer : Hide Polygon(s)");
	GetModel()->RecordUndo("Designer : Hide Polygon(s)", GetBaseObject());
	for (int i = 0, iPolygonCount(selectedPolygons.size()); i < iPolygonCount; ++i)
		selectedPolygons[i]->AddFlags(ePolyFlag_Hidden);
	pSelected->Clear();
	pSession->ReleaseSelectionMesh();
	ApplyPostProcess();
	pSession->signalDesignerEvent(eDesignerNotify_PolygonsModified, nullptr);

	return true;
}

void HidePolygonTool::UnhideAll()
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Base);

	ElementSet hiddenElements;
	for (int i = 0, iCount(GetModel()->GetPolygonCount()); i < iCount; ++i)
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);
		if (pPolygon->CheckFlags(ePolyFlag_Hidden) && !pPolygon->CheckFlags(ePolyFlag_Mirrored))
			hiddenElements.Add(Element(GetBaseObject(), pPolygon));
	}

	if (!hiddenElements.IsEmpty())
	{
		CUndo undo("Designer : Unhide Polygon(s)");
		GetModel()->RecordUndo("Designer : Unhide Polygon(s)", GetBaseObject());

		for (int i = 0, iElementCount(hiddenElements.GetCount()); i < iElementCount; ++i)
			hiddenElements[i].m_pPolygon->RemoveFlags(ePolyFlag_Hidden);

		ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
		pSelected->Set(hiddenElements);
		ApplyPostProcess();
		GetDesigner()->SwitchTool(eDesigner_Polygon);
	}
	else
	{
		GetDesigner()->SwitchToPrevTool();
	}
}

void HidePolygonTool::Serialize(Serialization::IArchive& ar)
{
	ar(ActionButton([this] { HideSelectedPolygons();
	                }), "Apply", "Apply");
	ar(ActionButton([this] { UnhideAll();
	                }), "UnhideAllPolygons", "Unhide All");
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_HidePolygon, eToolGroup_Misc, "Hide Polygon", HidePolygonTool,
                                                           hide, "runs hide tool", "designer.hide")

