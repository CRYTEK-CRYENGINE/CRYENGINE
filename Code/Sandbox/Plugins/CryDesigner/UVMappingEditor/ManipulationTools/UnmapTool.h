// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"
#include "Core/UVIsland.h"

namespace Designer {
namespace UVMapping
{
class UnmapTool : public BaseTool
{
public:
	UnmapTool(EUVMappingTool tool) : BaseTool(tool)
	{
	}

	void        Enter() override;
	static void UnmapSelectedElements();
};
}
}

