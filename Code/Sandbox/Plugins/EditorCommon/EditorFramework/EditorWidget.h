// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/WidgetActionRegistry.h"

#include <CryString/CryString.h>

#include <QWidget>

class QCommandAction;
class CCommand;
class CEditorWidget;

//! The widget that encapsulates registration and handling of actions.
class EDITOR_COMMON_API CEditorWidget : public QWidget
{
	Q_OBJECT
public:
	CEditorWidget(QWidget* pParent = nullptr)
		: QWidget(pParent)
		, m_widgetActionRegistry(*this)
	{}

	const std::vector<CCommand*>& GetCommands() const { return m_commands; }

	QCommandAction* GetAction_Deprecated(const string& actionId) const;

	//! Returns registered action.
	QCommandAction* GetAction(const string& actionId) const;

	CWidgetActionRegistry& GetActionRegistry() { return m_widgetActionRegistry; }

protected:
	virtual bool event(QEvent* pEvent) override;
	virtual void customEvent(QEvent* pEvent) override;

	//! Registers an action on the widget.
	//! /param actionId id of the action to register.
	//! /param actionHandler The handler the will be called when the action is triggered on the widget.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//! /see SPrioritizedRegistry
	template<class Func>
	QCommandAction* RegisterAction(const string& actionId, Func&& actionHandler, 
		EActionHandlerPermission perm, int priority = DEF_HANDLER_PRIORITY, uintptr_t id = 0)
	{
		m_commandRegistry.RegisterCommand(actionId, std::forward<Func>(actionHandler), id ? id : reinterpret_cast<uintptr_t>(this),
			perm, priority);
		return RegisterCommandAction(actionId);
	}

	//! Registers an action on the widget.
	//! /param actionId id of the action to register.
	//! /param actionHandler The handler the will be called when the action is triggered on the widget.
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	template<class Func>
	QCommandAction* RegisterAction(const string& actionId, Func&& actionHandler, uintptr_t id = 0)
	{
		return RegisterAction(actionId, std::forward<Func>(actionHandler), EActionHandlerPermission::Default, DEF_HANDLER_PRIORITY, id);
	}

	//! Registers a member function as an action on the widget.
	//! /param actionId id of the action to register.
	//! /param actionHandler The handler the will be called when the action is triggered on the widget.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//! /see SPrioritizedRegistry
	template<class T, class Ret, class... Params, typename std::enable_if_t<std::is_base_of<CEditorWidget, T>::value, bool> = true>
	QCommandAction* RegisterAction(const string& actionId, Ret(T::*actionHandler)(Params...), 
		EActionHandlerPermission perm = EActionHandlerPermission::Default, int priority = DEF_HANDLER_PRIORITY, uintptr_t id = 0)
	{
		m_commandRegistry.RegisterCommand(actionId, actionHandler, static_cast<T*>(this), id, perm, priority);
		return RegisterCommandAction(actionId);
	}

	void UnregisterAction(const string& actionId, uintptr_t id = 0);

	QCommandAction* SetActionEnabled(const string& actionId, bool enable);
	QCommandAction* SetActionChecked(const string& actionId, bool check);
	QCommandAction* SetActionText(const string& actionId, const QString& text);

	virtual const IEditorContext* GetContextObject() const { return nullptr; }

private:
	friend class CWidgetActionRegistry;

	QCommandAction* RegisterCommandAction(const string& actionId);
	void            UnregisterCommandAction(const string& actionId);

	SPrioritizedRegistry       m_commandRegistry;
	CWidgetActionRegistry      m_widgetActionRegistry;
	std::vector<CCommand*>     m_commands;
	StringMap<QCommandAction*> m_actions;
};
