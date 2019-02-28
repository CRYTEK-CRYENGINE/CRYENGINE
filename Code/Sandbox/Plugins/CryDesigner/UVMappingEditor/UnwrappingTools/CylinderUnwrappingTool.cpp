// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CylinderUnwrappingTool.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "Util/UVUnwrapUtil.h"

namespace Designer {
namespace UVMapping
{
void CylinderUnwrappingTool::Enter()
{
	ElementSet* pSelected = GetUVEditor()->GetSelectedElements();
	if (!pSelected)
		return;

	_smart_ptr<Designer::Model> pModel = pSelected->CreateModel();
	if (pModel == NULL)
		return;

	BrushVec3 origin = ToBrushVec3(pModel->GetBoundBox().GetCenter());
	origin.z = pModel->GetBoundBox().min.z;

	CUndo undo("UV Mapping : Cylinder Projection");
	CUndo::Record(new UVIslandUndo);
	CUndo::Record(new UVProjectionUndo);

	AssignMatIDToSelectedPolygons(GetUVEditor()->GetSubMatID());

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	UVIslandPtr pUVIsland = new UVIsland;

	float radius = 0;
	float height = 0;
	UnwrapUtil::CalculateCylinderRadiusHeight(pModel, radius, height);

	for (int i = 0, iCount(pModel->GetPolygonCount()); i < iCount; ++i)
	{
		PolygonPtr polygon = pModel->GetPolygon(i);
		UnwrapUtil::ApplyCylinderProjection(origin, radius, height, polygon);
		pUVIsland->AddPolygon(polygon);
	}

	pUVIslandMgr->AddUVIsland(pUVIsland);
	std::vector<UVIslandPtr> UVIslands;
	UVIslands.push_back(pUVIsland);

	GetUVEditor()->SelectUVIslands(UVIslands);
	GetUVEditor()->UpdateGizmoPos();
	GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
	GetUVEditor()->SetTool(eUVMappingTool_Island);
	GetUVEditor()->ApplyPostProcess();
}

}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Cylinder, eUVMappingToolGroup_Unwrapping, "Cylinder", CylinderUnwrappingTool,
                                    cylinder_unwrapping, "runs cylinder unwrapping tool", "uvmapping.cylinder_unwrapping")

