// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"

namespace Designer
{
struct ResetXFormParams
{
	ResetXFormParams() :
		bResetPosition(true),
		bResetRotation(true),
		bResetScale(true)
	{
	}

	bool bResetPosition;
	bool bResetRotation;
	bool bResetScale;
};

class ResetXFormTool : public BaseTool
{
public:
	ResetXFormTool(EDesignerTool tool) : BaseTool(tool)
	{
	}

	void        FreezeXForm(int nResetFlag);
	void        Serialize(Serialization::IArchive& ar);

	static void FreezeXForm(MainContext& mc, int nResetFlag);

private:

	void OnResetXForm();

	ResetXFormParams m_Params;
};
}

