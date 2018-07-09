// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SphereUnwrappingTool.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "Util/UVUnwrapUtil.h"

namespace Designer {
namespace UVMapping
{
void SphereUnwrappingTool::Enter()
{
	ElementSet* pSelected = GetUVEditor()->GetSelectedElements();
	if (pSelected == NULL)
		return;

	_smart_ptr<Designer::Model> pModel = pSelected->CreateModel();
	if (pModel == NULL)
		return;

	BrushVec3 origin = ToBrushVec3(pModel->GetBoundBox().GetCenter());

	CUndo undo("UV Mapping : Sphere Projection");
	CUndo::Record(new UVIslandUndo);
	CUndo::Record(new UVProjectionUndo);

	AssignMatIDToSelectedPolygons(GetUVEditor()->GetSubMatID());

	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();
	UVIslandPtr pUVIsland = new UVIsland;

	float radius = UnwrapUtil::CalculateSphericalRadius(pModel);
	for (int i = 0, iCount(pModel->GetPolygonCount()); i < iCount; ++i)
	{
		PolygonPtr polygon = pModel->GetPolygon(i);
		UnwrapUtil::ApplySphereProjection(origin, radius, polygon);
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

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Sphere, eUVMappingToolGroup_Unwrapping, "Sphere", SphereUnwrappingTool,
                                    sphere_unwrapping, "runs sphere unwrapping tool", "uvmapping.sphere_unwrapping");

