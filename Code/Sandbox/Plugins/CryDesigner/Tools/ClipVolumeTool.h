// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DesignerEditor.h"

namespace Designer
{
class ClipVolumeTool : public DesignerEditor
{
public:

	DECLARE_DYNCREATE(ClipVolumeTool)
	ClipVolumeTool();
	virtual string      GetDisplayName() const override { return "Clip Volume"; }
	bool                OnKeyDown(CViewport* pView, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
};

class CreateClipVolumeTool : public ClipVolumeTool
{
public:
	DECLARE_DYNCREATE(CreateClipVolumeTool)
	CreateClipVolumeTool();
};
}

