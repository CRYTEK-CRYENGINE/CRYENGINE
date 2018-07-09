// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include <QAction>
#include <QSet>
#include <QKeySequence>

class QWidget;
class CPolledKeyCommand;
class CUiCommand;
class CCustomCommand;

//////////////////////////////////////////////////////////////////////////
class QCommandAction : public QAction, public CUiCommand::UiInfo
{
	Q_OBJECT
public:
	QCommandAction(const QString& actionName, const QString& actionText, QObject* parent, const char* command = 0);
	QCommandAction(const QString& actionName, const char* command, QObject* parent);
	QCommandAction(const CUiCommand& cmd);
	QCommandAction(const CCustomCommand& cmd);
	QCommandAction(const CPolledKeyCommand& cmd);
	~QCommandAction() {}

	QString              GetCommand();
	void                 SetCommand(const char* command);

	void                 SetDefaultShortcuts(const QList<QKeySequence>& shortcuts) { m_defaults = shortcuts; }
	QList<QKeySequence>& GetDefaultShortcuts()                                     { return m_defaults; }

	virtual void         operator=(const UiInfo& info) override;
protected:
	void                 OnTriggered();

private:
	QList<QKeySequence> m_defaults;
};

