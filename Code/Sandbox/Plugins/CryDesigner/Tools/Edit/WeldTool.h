// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class WeldTool : public BaseTool
{
public:

	WeldTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	virtual void Enter() override;
	static void  Weld(MainContext& mc, const BrushVec3& vSrc, const BrushVec3& vTarget);
};
}

