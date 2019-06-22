// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "EditorWidget.h"

#include "Commands/ICommandManager.h"
#include "Commands/QCommandAction.h"
#include "Events.h"

#include <IEditor.h>

#include <QKeyEvent>

QCommandAction* CEditorWidget::GetAction_Deprecated(const string& actionId) const
{
	QCommandAction* pAction = GetIEditor()->GetICommandManager()->GetCommandAction(actionId.c_str());

	if (!pAction)
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR_DBGBRK, "Command not found");

	return pAction;
}

bool CEditorWidget::event(QEvent *pEvent)
{
	// All editor widgets must handle shortcut override events since Qt doesn't support shortcuts to collide in the widget hierarchy.
	// The primary cause for shortcuts to be ambiguous is having the same command as separate QActions in more than one place of the widget hierarchy.
	// For example: if we have "general.new" in both an editor window and one of the editor's sub-panels, both are mapped to Ctrl+N. Qt will fail to
	// execute the command because both actions are in fact different QActions to be able to track separate state (icon, checked state, disabled, etc).
	// In turn Qt will not be able to determine that we're referring to the same editor command. If we don't override the shortcut handling, Qt will fail
	// to handle the shortcut stating that the context is ambiguous
	if (pEvent->type() == QEvent::ShortcutOverride)
	{
		// Shortcut override events are QKeyEvents
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		const int key = pKeyEvent->key();

		// Make sure the key is valid
		if (key != Qt::Key_unknown && !(key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta))
		{
			// Create a key sequence out of key and modifiers
			QKeySequence keySequence(key | pKeyEvent->modifiers());

			// Try and find if there's any command that maps to this shortcut
			auto ite = std::find_if(m_actions.cbegin(), m_actions.cend(), [&keySequence](const StringMap<QCommandAction*>::value_type& value)
			{
				return value.second->shortcut() == keySequence;
			});

			// If we found a match for the shortcut, then trigger the action and mark the event as handled
			if (ite != m_actions.cend())
			{
				QCommandAction* pCommandAction = qobject_cast<QCommandAction*>(ite->second);
				pCommandAction->trigger();
				pEvent->setAccepted(true);
				return true;
			}
		}
	}

	return QWidget::event(pEvent);
}

void CEditorWidget::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);
		pEvent->setAccepted(m_commandRegistry.ExecuteCommand(pCommandEvent->GetCommand()));
		if (pEvent->isAccepted())
		{
			return;
		}
	}

	QWidget::customEvent(pEvent);
}

QCommandAction* CEditorWidget::RegisterCommandAction(const string& actionId)
{
	const auto it = m_actions.find(actionId);
	if (it != m_actions.cend())
	{
		return it->second;
	}

	QCommandAction* pAction = GetIEditor()->GetICommandManager()->CreateNewAction(actionId);
	CRY_ASSERT_MESSAGE(pAction, "Error occurred while creating action %s", actionId);
	m_actions[actionId] = pAction;
	addAction(pAction);

	CCommand* pCommand = GetIEditor()->GetICommandManager()->GetCommand(actionId.c_str());
	if (pCommand)
	{
		m_commands.push_back(pCommand);
	}
	return pAction;
}

void CEditorWidget::UnregisterCommandAction(const string& actionId)
{
	auto it = m_actions.find(actionId);
	if (it == m_actions.cend())
	{
		CRY_ASSERT_MESSAGE(false, "Command %s not found. Unregistering failed", actionId.c_str());
		return;
	}
	removeAction(it->second);
	m_actions.erase(it);
	string moduleName = actionId.substr(0, actionId.find('.'));
	string commandName = actionId.substr(moduleName.size() + 1);
	m_commands.erase(std::remove_if(m_commands.begin(), m_commands.end(), [&moduleName, &commandName](CCommand* pCommand)
	{
		return pCommand->GetModule() == moduleName && pCommand->GetName() == commandName;
	}), m_commands.end());
}

QCommandAction* CEditorWidget::GetAction(const string& actionId) const
{
	const auto& it = m_actions.find(actionId);
	if (it != m_actions.cend())
	{
		return it->second;
	}
	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR_DBGBRK, string().Format("Command %s not found", actionId));
	return nullptr;
}

void CEditorWidget::UnregisterAction(const string& actionId, uintptr_t id /*= 0*/)
{
	m_commandRegistry.UnregisterCommand(actionId, id ? id : reinterpret_cast<uintptr_t>(this));
	UnregisterCommandAction(actionId);
}

QCommandAction* CEditorWidget::SetActionEnabled(const string& actionId, bool enable)
{
	QCommandAction* pAction = GetAction(actionId);
	if (!pAction)
	{
		return nullptr;
	}
	pAction->setEnabled(enable);
	return pAction;
}

QCommandAction* CEditorWidget::SetActionChecked(const string& actionId, bool check)
{
	QCommandAction* pAction = GetAction(actionId);
	if (!pAction)
	{
		return nullptr;
	}
	pAction->setChecked(check);
	return pAction;
}

QCommandAction* CEditorWidget::SetActionText(const string& actionId, const QString& text)
{
	QCommandAction* pAction = GetAction(actionId);
	if (!pAction)
	{
		return nullptr;
	}
	pAction->setText(text);
	return pAction;
}
