// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class MergeTool : public BaseTool
{
public:
	MergeTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void        Enter() override;

	static void MergePolygons(MainContext& mc);

private:
	void MergeObjects();
	void MergePolygons();
};
}

