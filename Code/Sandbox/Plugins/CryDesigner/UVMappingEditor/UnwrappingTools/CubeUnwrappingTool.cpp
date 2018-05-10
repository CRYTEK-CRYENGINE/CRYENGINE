// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CubeUnwrappingTool.h"
#include "Util/ElementSet.h"
#include "Core/UVIslandManager.h"
#include "../UVMappingEditor.h"
#include "../UVUndo.h"
#include "Util/UVUnwrapUtil.h"

namespace Designer {
namespace UVMapping
{
void CubeUnwrappingTool::Enter()
{
	if (!GetUVEditor()->GetModel())
		return;

	Vec3 axises[] = { Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, -1, 0), Vec3(0, 0, -1) };
	constexpr int numberOfAxises = CRY_ARRAY_COUNT(axises);
	std::vector<PolygonPtr> polygonSets[numberOfAxises];
	ElementSet* pSelected = GetUVEditor()->GetSelectedElements();
	if (pSelected == NULL)
		return;
	UVIslandManager* pUVIslandMgr = GetUVEditor()->GetUVIslandMgr();

	CUndo undo("UV Mapping : Cube Projection");
	CUndo::Record(new UVIslandUndo);
	CUndo::Record(new UVProjectionUndo);

	AssignMatIDToSelectedPolygons(GetUVEditor()->GetSubMatID());

	for (int k = 0, iCount(pSelected->GetCount()); k < iCount; ++k)
	{
		const Element& element = pSelected->Get(k);
		if (!element.IsPolygon())
			continue;

		float fBiggestCos = -1.5f;
		int axisIndex = -1;

		for (int i = 0; i < numberOfAxises; ++i)
		{
			const Vec3& axis = axises[i];
			float fCos = axis.Dot(ToVec3(element.m_pPolygon->GetPlane().Normal()));

			if (fCos > fBiggestCos)
			{
				fBiggestCos = fCos;
				axisIndex = i;
			}
		}

		DESIGNER_ASSERT(axisIndex != -1);
		if (axisIndex == -1)
			continue;

		UnwrapUtil::ApplyPlanarProjection(BrushPlane(ToVec3(axises[axisIndex]), 0), element.m_pPolygon);
		polygonSets[axisIndex].push_back(element.m_pPolygon);
	}

	std::vector<UVIslandPtr> UVIslands;
	for (int i = 0; i < numberOfAxises; ++i)
	{
		UVIslandPtr pUVIsland = new UVIsland;
		for (int k = 0, iCount(polygonSets[i].size()); k < iCount; ++k)
			pUVIsland->AddPolygon(polygonSets[i][k]);
		UVIslands.push_back(pUVIsland);
	}

	UnwrapUtil::PackUVIslands(pUVIslandMgr, UVIslands, ePlaneAxis_Average);

	GetUVEditor()->SelectUVIslands(UVIslands);
	GetUVEditor()->UpdateGizmoPos();
	GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
	GetUVEditor()->SetTool(eUVMappingTool_Island);
	GetUVEditor()->ApplyPostProcess();
}

}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Cube, eUVMappingToolGroup_Unwrapping, "Cube", CubeUnwrappingTool,
                                    cube_unwrmapping, "runs cube unwrapping tool", "uvmapping.cube_unwrapping")

