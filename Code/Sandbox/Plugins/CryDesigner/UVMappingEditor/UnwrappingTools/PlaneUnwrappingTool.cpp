// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlaneUnwrappingTool.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "Util/UVUnwrapUtil.h"

namespace Designer {
namespace UVMapping
{
void PlaneUnwrappingTool::Enter()
{
	ElementSet* pSelected = GetUVEditor()->GetSelectedElements();
	if (!pSelected)
		return;

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	CUndo undo("UV Mapping : Planar Projection");
	CUndo::Record(new UVIslandUndo);
	CUndo::Record(new UVProjectionUndo);

	std::vector<UVIslandPtr> UVIslands;
	UnwrapUtil::MakeUVIslandsFromSelectedElements(UVIslands);
	UnwrapUtil::PackUVIslands(pUVIslandMgr, UVIslands, GetUVEditor()->GetPlaneAxis());

	GetUVEditor()->SelectUVIslands(UVIslands);
	GetUVEditor()->UpdateGizmoPos();
	GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
	GetUVEditor()->SetTool(eUVMappingTool_Island);
	GetUVEditor()->ApplyPostProcess();
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Plane, eUVMappingToolGroup_Unwrapping, "Plane", PlaneUnwrappingTool,
                                    plane_unwrapping, "runs plane unwrapping tool", "uvmapping.plane_unwrapping")

