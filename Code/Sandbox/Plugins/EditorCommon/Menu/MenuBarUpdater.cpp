// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MenuBarUpdater.h"
#include "AbstractMenu.h"
#include "MenuWidgetBuilders.h"

#include <QMenuBar>

CMenuBarUpdater::CMenuBarUpdater(CAbstractMenu* pAbstractMenu, QMenuBar* pMenuBar)
	: m_pAbstractMenu(pAbstractMenu)
{
	auto rebuild = [pAbstractMenu, pMenuBar]()
	{
		pMenuBar->clear();
		pAbstractMenu->Build(MenuWidgetBuilders::CMenuBarBuilder(pMenuBar));
	};

	pAbstractMenu->signalActionAdded.Connect(rebuild, GetId());
	pAbstractMenu->signalMenuAdded.Connect([rebuild](CAbstractMenu*) { rebuild(); }, GetId());
}

CMenuBarUpdater::~CMenuBarUpdater()
{
	m_pAbstractMenu->signalActionAdded.DisconnectById(GetId());
}

