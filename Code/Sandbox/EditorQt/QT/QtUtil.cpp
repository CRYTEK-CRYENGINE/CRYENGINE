// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QtUtil.h"

#include <QAction>
#include <QApplication>

#include "KeyboardShortcut.h"

namespace QtUtil
{

void AssignMenuRole(QAction* action, const QString& roleString)
{
	if (roleString == STRINGIFY(QuitRole))
		action->setMenuRole(QAction::QuitRole);
	else if (roleString == STRINGIFY(PreferencesRole))
		action->setMenuRole(QAction::PreferencesRole);
	else if (roleString == STRINGIFY(AboutRole))
		action->setMenuRole(QAction::AboutRole);
}

}

