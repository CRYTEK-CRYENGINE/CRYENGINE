// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"

namespace Designer {
namespace UVMapping
{
class AlignTool : public BaseTool
{
public:
	AlignTool(EUVMappingTool tool) : BaseTool(tool) {}

	void Enter() override;

private:

	void AlignSelectedVertices();
};
}
}

