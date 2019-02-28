// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class RingSelectionTool : public BaseTool
{
public:

	RingSelectionTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void        Enter() override;
	static void RingSelection(MainContext& mc);

private:

	static void SelectFaceRing(MainContext& mc);
	static void SelectRing(MainContext& mc, const BrushEdge3D& inputEdge);
};
}

