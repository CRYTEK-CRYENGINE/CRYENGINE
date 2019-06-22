// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "QCommandAction.h"

#include "CustomCommand.h"
#include "CryIcon.h"
#include "PolledKey.h"
#include "QtUtil.h"

#include <QAction>
#include <QStatusBar>

QCommandAction::QCommandAction(const QString& actionName, const QString& actionText, QObject* parent, const char* command /*=0*/)
	: QAction(actionName, parent)
{
	setShortcutContext(Qt::WidgetWithChildrenShortcut);
	setShortcutVisibleInContextMenu(true);
	setText(actionText);
	if (command)
	{
		setProperty("QCommandAction", QVariant(QString(command)));
	}
	connect(this, &QCommandAction::triggered, this, &QCommandAction::OnTriggered);
}

QCommandAction::QCommandAction(const QString& actionName, const char* command, QObject* parent)
	: QAction(actionName, parent)
{
	setShortcutContext(Qt::WidgetWithChildrenShortcut);
	setShortcutVisibleInContextMenu(true);
	setObjectName(actionName);
	if (command)
	{
		setProperty("QCommandAction", QVariant(QString(command)));
	}
	connect(this, &QCommandAction::triggered, this, &QCommandAction::OnTriggered);
}

QCommandAction::QCommandAction(const QCommandAction& action)
	: QAction(action.text(), nullptr)
	, UiInfo(action.UiInfo::buttonText, action.UiInfo::icon, action.key, action.UiInfo::isCheckable)
{
	setShortcutContext(Qt::WidgetWithChildrenShortcut);
	setShortcutVisibleInContextMenu(true);
	setIcon(action.QAction::icon());
	setCheckable(action.QAction::isCheckable());
	setObjectName(action.objectName());
	setProperty("QCommandAction", action.property("QCommandAction"));
	setShortcuts(action.QAction::shortcuts());

	connect(this, &QCommandAction::triggered, this, &QCommandAction::OnTriggered);
}

QCommandAction::QCommandAction(const CUiCommand& cmd)
	: QAction(cmd.GetDescription().c_str(), nullptr)
{
	setShortcutContext(Qt::WidgetWithChildrenShortcut);
	setShortcutVisibleInContextMenu(true);
	setObjectName(cmd.GetName().c_str());
	setProperty("QCommandAction", QVariant(QString(cmd.GetCommandString())));
	connect(this, &QCommandAction::triggered, this, &QCommandAction::OnTriggered);
}

QCommandAction::QCommandAction(const CCustomCommand& cmd)
	: QAction(cmd.GetName().c_str(), nullptr)
{
	setShortcutContext(Qt::WidgetWithChildrenShortcut);
	setShortcutVisibleInContextMenu(true);
	setObjectName(tr("Custom Command"));
	setProperty("QCommandAction", QVariant(QString(cmd.GetCommandString())));
	connect(this, &QCommandAction::triggered, [&]()
	{
		SetCommand(cmd.GetCommandString());
		OnTriggered();
	});
}

QCommandAction::QCommandAction(const CPolledKeyCommand& cmd)
	: QAction(cmd.GetDescription().c_str(), 0)
{
	setShortcutContext(Qt::WidgetWithChildrenShortcut);
	setShortcutVisibleInContextMenu(true);
	setObjectName(tr("Polled Key Command"));
	//triggered() is intentionally not connected
}

QString QCommandAction::GetCommand()
{
	return property("QCommandAction").toString();
}

void QCommandAction::SetCommand(const char* command)
{
	setProperty("QCommandAction", QVariant(command));
}

void QCommandAction::OnTriggered()
{
	string cmd;

	QVariant commandProp = property("QCommandAction");
	if (commandProp.isValid())
	{
		QString qstr = commandProp.toString();
		cmd = qstr.toStdString().c_str();
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Invalid QCommandAction %s", objectName().toStdString().c_str());
	}

	GetIEditor()->ExecuteCommand(cmd);
}

void QCommandAction::operator=(const UiInfo& info)
{
	UiInfo::operator=(info);

	QList<QKeySequence> shortcuts = info.key.ToKeySequence();
	setCheckable(info.isCheckable);

	if (!shortcuts.empty())
	{
		setShortcuts(shortcuts);
		SetDefaultShortcuts(shortcuts);
	}

	if (!info.buttonText || *info.buttonText)
		setText(QtUtil::ToQString(info.buttonText));

	if (!info.icon || *info.icon)
		setIcon(CryIcon(QtUtil::ToQString(info.icon)));
}
