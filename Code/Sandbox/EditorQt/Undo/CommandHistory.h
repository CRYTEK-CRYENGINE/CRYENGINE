// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once

#include "QtViewPane.h"

class CHistoryPanel : public CDockableWidget
{
public:
	CHistoryPanel();
	~CHistoryPanel();

	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_RIGHT; }
	virtual const char*                       GetPaneTitle() const override        { return "Undo History"; };
	virtual QRect                             GetPaneRect() override               { return QRect(0, 0, 800, 500); }
};

