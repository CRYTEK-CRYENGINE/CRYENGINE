// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QtViewPane.h"
#include "IEditor.h"

class QWidget;

namespace Designer
{

class DesignerDockable : public CDockableWidget
{
public:
	DesignerDockable(QWidget* pParent = nullptr);
	~DesignerDockable();

	const char* GetPaneTitle() const { return "Designer Tool"; }

private:
	void SetupUI();
};

}


