// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class ElementSet;

class CopyTool : public BaseTool
{
public:

	CopyTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void        Enter() override;

	static void Copy(MainContext& mc, ElementSet* pOutCopiedElements);
};
}

