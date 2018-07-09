// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ConeTool.h"
#include "Core/Model.h"
#include "Util/PrimitiveShape.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "DesignerEditor.h"

namespace Designer
{
void ConeTool::UpdateShape(float fHeight)
{
	if (m_Phase == eDiscPhase_SetCenter)
		return;

	DESIGNER_ASSERT(m_pBasePolygon);
	if (!m_pBasePolygon)
		return;

	std::vector<PolygonPtr> polygonList;
	PrimitiveShape bp;
	bp.CreateCone(m_pBasePolygon, fHeight, &polygonList);

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();
	for (int i = 0, iPolygonCount(polygonList.size()); i < iPolygonCount; ++i)
	{
		if (GetStartSpot().m_pPolygon)
			polygonList[i]->SetTexInfo(GetStartSpot().m_pPolygon->GetTexInfo());
		AddPolygonWithSubMatID(polygonList[i]);
		GetModel()->AddPolygon(polygonList[i]);
	}

	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Cone, eToolGroup_Shape, "Cone", ConeTool,
                                                           cone, "runs cone tool", "designer.cone")

