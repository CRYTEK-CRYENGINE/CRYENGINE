// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../BaseTool.h"

namespace Designer {
namespace UVMapping
{
class SeparateTool : public BaseTool
{
public:
	SeparateTool(EUVMappingTool tool);

	void Enter() override;
	void Leave() override;

private:

	void SeparatePolygons();
};
}
}

