// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ViewUnwrappingTool.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "Util/UVUnwrapUtil.h"

namespace Designer {
namespace UVMapping
{
void ViewUnwrappingTool::Enter()
{
	ElementSet* pSelected = GetUVEditor()->GetSelectedElements();
	if (!pSelected)
		return;

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	CUndo undo("UV Mapping : View Projection");
	CUndo::Record(new UVIslandUndo);
	CUndo::Record(new UVProjectionUndo);

	std::vector<UVIslandPtr> UVIslands;
	UnwrapUtil::MakeUVIslandsFromSelectedElements(UVIslands);

	for (int i = 0, iIslandCount(UVIslands.size()); i < iIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = UVIslands[i];
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
			UnwrapUtil::ApplyCameraProjection(*GetUVEditor()->Get3DViewportCamera(), GetUVEditor()->GetBaseObject()->GetWorldTM(), pUVIsland->GetPolygon(k));
		pUVIslandMgr->AddUVIsland(UVIslands[i]);
	}

	GetUVEditor()->SelectUVIslands(UVIslands);
	GetUVEditor()->UpdateGizmoPos();
	GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
	GetUVEditor()->SetTool(eUVMappingTool_Island);
	GetUVEditor()->ApplyPostProcess();
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_View, eUVMappingToolGroup_Unwrapping, "View", ViewUnwrappingTool,
                                    view_unwrapping, "runs view unwrapping tool", "uvmapping.view_unwrapping")

