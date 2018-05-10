// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UICommon.h"

namespace Designer
{
template<class _Tool>
class PropertyTreePanel : public QPropertyTreeWidget, public IBasePanel
{
public:
	PropertyTreePanel(_Tool* pTool) : m_pTool(pTool)
	{
		CreatePropertyTree(m_pTool, [=](bool continuous){ m_pTool->OnChangeParameter(continuous); });
		LoadSettings(Serialization::SStruct(*m_pTool), m_pTool->ToolClass());
	}

	void Done() override
	{
		SaveSettings(Serialization::SStruct(*m_pTool), m_pTool->ToolClass());
	}

	void Update() override
	{
		m_pPropertyTree->revert();
	}

	QWidget* GetWidget() override
	{
		return this;
	}

private:

	_Tool* m_pTool;
};
}

