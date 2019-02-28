// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectConnectedTool.h"
#include "DesignerEditor.h"
#include "Util/ElementSet.h"
#include "DesignerSession.h"

namespace Designer
{
void SelectConnectedTool::Enter()
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	ElementSet* pSelected = pSession->GetSelectedElements();
	if (pSelected->IsEmpty())
	{
		MessageBox("Warning", "At least one element should be selected to use this tool.");
		GetDesigner()->SwitchToPrevTool();
		return;
	}
	CUndo undo("Designer : Selection of connected elements");
	GetDesigner()->StoreSelectionUndo();
	SelectConnectedPolygons(GetMainContext());
	GetDesigner()->SwitchToPrevTool();
	pSession->signalDesignerEvent(eDesignerNotify_Select, nullptr);
}

void SelectConnectedTool::SelectConnectedPolygons(MainContext& mc)
{
	while (SelectAdjacentElements(mc))
	{
	}
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Connected, eToolGroup_Selection, "Connected", SelectConnectedTool,
                                   connectedselection, "runs connected selection tool", "designer.connectedselection")

