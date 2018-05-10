// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class ElementSet;
class SelectGrowTool : public BaseTool
{
public:
	SelectGrowTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	virtual void Enter() override;

	static void  GrowSelection(MainContext& mc);

protected:

	static bool  SelectAdjacentElements(MainContext& mc);
	static bool  SelectAdjacentEdgesToPolygonVertex(CBaseObject* pObject, ElementSet* pSelected, PolygonPtr pPolygon, int index);
};
}

