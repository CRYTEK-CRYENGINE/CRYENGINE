// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
class BooleanTool : public BaseTool
{
public:

	BooleanTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void Enter() override;

	void BooleanOperation(EBooleanOperationEnum booleanType);
	void Serialize(Serialization::IArchive& ar);

	void Union()        { BooleanOperation(eBOE_Union); }
	void Subtract()     { BooleanOperation(eBOE_Difference); }
	void Intersection() { BooleanOperation(eBOE_Intersection); }
};
}

