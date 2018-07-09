// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"

namespace Designer {
namespace UVMapping
{
class SelectSharedTool : public BaseTool
{
public:
	SelectSharedTool(EUVMappingTool tool) : BaseTool(tool) {}
	void        Enter() override;

	static void SelectShared();
};
}
}

