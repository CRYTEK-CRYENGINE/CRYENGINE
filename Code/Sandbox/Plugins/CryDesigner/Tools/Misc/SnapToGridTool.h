// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class SnapToGridTool : public BaseTool
{
public:

	SnapToGridTool(EDesignerTool tool);

	void      Enter() override;

	BrushVec3 SnapVertexToGrid(const BrushVec3& vPos);
};
}

