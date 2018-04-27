// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectSharedTool.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"

namespace Designer {
namespace UVMapping
{
void SelectSharedTool::Enter()
{
	CUndo undo("UVMapping Editor : Selection");
	CUndo::Record(new UVSelectionUndo);

	SelectShared();
	SyncSelection();
	GetUVEditor()->UpdateGizmoPos();

	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}

void SelectSharedTool::SelectShared()
{
	UVElementSetPtr pSharedUVElementSet = GetUVEditor()->GetSharedElementSet();
	UVElementSetPtr pUVElementSet = GetUVEditor()->GetElementSet();
	pUVElementSet->Join(pSharedUVElementSet);
	pSharedUVElementSet->Clear();
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_SharedSelect, eUVMappingToolGroup_Manipulation, "Select Shared", SelectSharedTool,
                                    shared_select, "runs shared select tool", "uvmapping.shared_select");

