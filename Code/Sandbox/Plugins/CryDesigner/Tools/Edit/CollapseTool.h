// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class CollapseTool : public BaseTool
{
public:

	CollapseTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void Enter() override;
};
}

