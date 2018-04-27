// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../UVMappingEditor.h"
#include "SeparateTool.h"
#include "Core/UVIslandManager.h"

namespace Designer {
namespace UVMapping
{
SeparateTool::SeparateTool(EUVMappingTool tool) : BaseTool(tool)
{
}

void SeparateTool::Enter()
{
	if (GetUVEditor()->GetUVIslandMgr())
		SeparatePolygons();
	GetUVEditor()->SetTool(GetUVEditor()->GetPrevTool());
}

void SeparateTool::Leave()
{
}

void SeparateTool::SeparatePolygons()
{
	UVElementSetPtr pElementSet = GetUVEditor()->GetElementSet();
	UVIslandPtr pUVIsland = new UVIsland;

	for (int i = 0, iCount(pElementSet->GetCount()); i < iCount; ++i)
	{
		UVElement& element = pElementSet->GetElement(i);
		if (!element.IsPolygon())
			return;
		pUVIsland->AddPolygon(element.m_UVVertices[0].pPolygon);
	}

	GetUVEditor()->GetUVIslandMgr()->AddUVIsland(pUVIsland);
	pElementSet->Clear();
	pElementSet->AddElement(UVElement(pUVIsland));
	GetUVEditor()->GetUVIslandMgr()->ConvertIsolatedAreasToIslands();
}
}
}

REGISTER_UVMAPPING_TOOL_AND_COMMAND(eUVMappingTool_Separate, eUVMappingToolGroup_Manipulation, "Separate", SeparateTool,
                                    separate, "runs separate tool", "uvmapping.separate")

