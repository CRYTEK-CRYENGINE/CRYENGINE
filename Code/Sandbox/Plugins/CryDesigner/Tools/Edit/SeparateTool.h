// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class DesignerObject;
class SeparateTool : public BaseTool
{
public:

	SeparateTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void                   Enter() override;
	bool                   IsManipulatorVisible() override { return false; }
	static DesignerObject* Separate(MainContext& mc);
};
}

