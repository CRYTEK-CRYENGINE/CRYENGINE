// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "QCommandAction.h"
#include "Util/BoostPythonHelpers.h"

#include <QAction>
#include <QStatusBar>

#include "Qt/QtMainFrame.h"
#include "Command.h"
#include "Commands/CustomCommand.h"
#include "PolledKey.h"
#include "QtUtil.h"
#include <QT/QtUtil.h>
#include <CryIcon.h>

QCommandAction::QCommandAction(const QString& actionName, const QString& actionText, QObject* parent, const char* command /*=0*/)
	: QAction(actionName, parent)
{
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
	setObjectName(actionName);
	if (command)
	{
		setProperty("QCommandAction", QVariant(QString(command)));
	}
	connect(this, &QCommandAction::triggered, this, &QCommandAction::OnTriggered);
}

QCommandAction::QCommandAction(const CUiCommand& cmd)
	: QAction(cmd.GetDescription().c_str(), 0)
{
	setObjectName(cmd.GetName().c_str());
	setProperty("QCommandAction", QVariant(QString(cmd.GetCommandString())));
	connect(this, &QCommandAction::triggered, this, &QCommandAction::OnTriggered);

	CEditorMainFrame* mainFrame = CEditorMainFrame::GetInstance();
	if (mainFrame)
	{
		mainFrame->addAction(this);
	}
}

QCommandAction::QCommandAction(const CCustomCommand& cmd)
	: QAction(cmd.GetName().c_str(), 0)
{
	setObjectName(tr("Custom Command"));
	setProperty("QCommandAction", QVariant(QString(cmd.GetCommandString())));
	connect(this, &QCommandAction::triggered, [&]()
	{
		SetCommand(cmd.GetCommandString());
		OnTriggered();
	});

	CEditorMainFrame* mainFrame = CEditorMainFrame::GetInstance();
	if (mainFrame)
	{
		mainFrame->addAction(this);
	}
}

QCommandAction::QCommandAction(const CPolledKeyCommand& cmd)
	: QAction(cmd.GetDescription().c_str(), 0)
{
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

	GetIEditorImpl()->ExecuteCommand(cmd);
}

void QCommandAction::operator=(const UiInfo& info)
{
	UiInfo::operator=(info);

	QList<QKeySequence> shortcuts = info.key.ToKeySequence();

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

