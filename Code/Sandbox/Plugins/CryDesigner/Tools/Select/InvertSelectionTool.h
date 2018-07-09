// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class InvertSelectionTool : public BaseTool
{
public:

	InvertSelectionTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void        Enter() override;

	static void InvertSelection(MainContext& mc);
};
}

