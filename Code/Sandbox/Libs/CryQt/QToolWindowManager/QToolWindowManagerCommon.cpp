// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowManagerCommon.h"

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

QMainWindow* mainWindow = nullptr;

void registerMainWindow(QMainWindow* w)
{
	mainWindow = w;
}

QMainWindow* getMainWindow()
{
	return mainWindow;
}

QWidget* windowBelow(QWidget* w)
{
#if defined(WIN32) || defined(WIN64)
	while (w && !w->isWindow())
	{
		w = w->parentWidget();
	}
	if (!w)
		return nullptr;

	HWND h = (HWND)w->winId();
	DWORD thisProc, otherProc;
	GetWindowThreadProcessId(h, &thisProc);
	while (h = GetNextWindow(h, GW_HWNDNEXT))
	{
		GetWindowThreadProcessId(h, &otherProc);
		if (thisProc == otherProc)
		{
			foreach(QWidget* w, qApp->topLevelWidgets())
			{
				if (w->isWindow() && w->isVisible() && (HWND)w->winId() == h && !w->windowState().testFlag(Qt::WindowMinimized))
					if (w->geometry().contains(QCursor::pos()))
						return w;
			}
		}
	}
#endif
	return nullptr;
}

QIcon getIcon(const QVariantMap& config, QString key, QIcon fallback)
{
	QVariant v = config.value(key, fallback);
	return v.value<QIcon>();
}
