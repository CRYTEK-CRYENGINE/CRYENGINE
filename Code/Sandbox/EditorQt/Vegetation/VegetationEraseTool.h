// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "VegetationPaintTool.h"

class CVegetationEraseTool : public CVegetationPaintTool
{
	DECLARE_DYNCREATE(CVegetationEraseTool)
public:
	CVegetationEraseTool();

	virtual string                           GetDisplayName() const override { return "Erase Vegetation"; }
	static QEditToolButtonPanel::SButtonInfo CreateEraseToolButtonInfo();

protected:
	virtual ~CVegetationEraseTool() {}
	// Delete itself.
	virtual void DeleteThis() override { delete this; }
};

