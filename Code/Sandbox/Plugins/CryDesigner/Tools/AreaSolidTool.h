// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DesignerEditor.h"

namespace Designer
{
class AreaSolidTool : public DesignerEditor
{
public:
	DECLARE_DYNCREATE(AreaSolidTool)
	AreaSolidTool();
	virtual string      GetDisplayName() const override { return "Area Solid"; }
	virtual bool        OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
};

class CreateAreaSolidTool final : public AreaSolidTool
{
public:

	DECLARE_DYNCREATE(CreateAreaSolidTool)
	CreateAreaSolidTool();
};

} // namespace Designer

