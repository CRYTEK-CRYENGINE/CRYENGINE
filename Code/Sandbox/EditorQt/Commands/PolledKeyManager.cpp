// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PolledKeyManager.h"

#include <QCoreApplication>
#include <QKeyEvent>

#include "PolledKey.h"
#include "Commands/QCommandAction.h"
#include "CommandManager.h"

CPolledKeyManager::CPolledKeyManager()
{
}

CPolledKeyManager::~CPolledKeyManager()
{
	GetIEditorImpl()->GetCommandManager()->signalChanged.DisconnectObject(this);
	qApp->removeEventFilter(this);
}

void CPolledKeyManager::Init()
{
	qApp->installEventFilter(this);
	GetIEditorImpl()->GetCommandManager()->signalChanged.Connect(this, &CPolledKeyManager::Update);
	Update();
}

bool CPolledKeyManager::IsKeyDown(const char* commandName) const
{
	CCommand* command = GetIEditorImpl()->GetCommandManager()->GetCommand(commandName);
	assert(command->IsPolledKey());
	return static_cast<CPolledKeyCommand*>(command)->GetKeyDown();
}

void CPolledKeyManager::Update()
{
	std::vector<CCommand*> cmds;
	GetIEditorImpl()->GetCommandManager()->GetCommandList(cmds);
	m_polledKeyState.clear();

	for (CCommand* cmd : cmds)
	{
		if (cmd->IsPolledKey())
		{
			CPolledKeyCommand* uiCmd = static_cast<CPolledKeyCommand*>(cmd);

			QCommandAction* action = static_cast<QCommandAction*>(uiCmd->GetUiInfo());
			for (const QKeySequence& seq : action->shortcuts())
			{
				if (seq.count() == 1)//discard all combination key sequences for polled keys
					m_polledKeyState.push_back(CmdAndKey(uiCmd, seq));
			}
		}
	}
}

bool CPolledKeyManager::eventFilter(QObject* object, QEvent* event)
{
	bool press = false;
	bool soverride = false;
	switch (event->type())
	{
	case QEvent::KeyPress:
		press = true;
	//Intentional fallthrough
	case QEvent::ShortcutOverride:
		// override is actually a release event, so that key does not stay down during the shortcut operation
		soverride = true;
	//Intentional fallthrough
	case QEvent::KeyRelease:
		{
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

			for (CmdAndKey& cmdAndKey : m_polledKeyState)
			{
				// catch all keys, regardless of modifier (only exception is if key belongs to keypad)...
				if (QKeySequence(keyEvent->key() | (keyEvent->modifiers() & Qt::KeypadModifier)) == cmdAndKey.second)
				{
					cmdAndKey.first->SetKeyDown(press);

					// but only emit a key (by accepting a shortcut override) if modifiers exactly match
					if (soverride && QKeySequence(keyEvent->key() | keyEvent->modifiers()) == cmdAndKey.second)
						event->accept();

					return false;
				}
			}

			break;
		}

	// When any window is deactivated, reset the key down state for any commands
	case QEvent::WindowDeactivate:
		for (CmdAndKey& cmdAndKey : m_polledKeyState)
		{
			cmdAndKey.first->SetKeyDown(false);
		}
	}

	return false;
}

