// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/PriorityCommandRegistry.h"

class QCommandAction;
class CEditorWidget;

//! This enum is used to define permission of a certain action handler.
enum class EActionHandlerPermission
{
	Default,
	Exclusive
};

constexpr int DEF_HANDLER_PRIORITY = 0;

//! Command registry that deals with handlers permissions and priorities.
//! All commands with exclusive permission are scheduled first.
//! Handlers with the same permission are sorted by priority (highest first).
struct SPrioritizedRegistry : SPriorityCommandRegistry<EActionHandlerPermission, int>
{
	SPrioritizedRegistry();
};

//! A common interface for widget's context type.
//! it's used to make sure that given specific context belongs to this hierarchy.
//! Every widget context (like IAssetBrowserContext) should extend this interface
//! and add widget's specific methods that will be used by action handlers.
struct IEditorContext
{
	virtual const char* GetEditorName() const = 0;
};

//! Registry class that allow registration of action in a particular editor from any other object
//! and being able to handle the action in context of the current widget.
//! So, whenever an action on a specific widget (like AssetBrowser) needs to be registered 
//! a handler (or function to register) needs to accept as the first parameter pointer to the 
//! type of current widget's context (IAssetBrowserContext*). 
class EDITOR_COMMON_API CWidgetActionRegistry
{
public:
	CWidgetActionRegistry(CEditorWidget& widget)
		: m_widget(widget)
	{}

	template<class T>
	using EnableIfEditorContext = std::enable_if_t<std::is_base_of<IEditorContext,
		std::decay_t<std::remove_pointer_t<T>>>::value && std::is_pointer<T>::value>;

	//! Registers an action in the registry of specific widget.
	//! /param actionId id of the action to register.
	//! /param actionHandler The handler the will be called when the action is triggered on the widget.
	//!     The first parameter to the handler must be a pointer to widget's context object.
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /see SPrioritizedRegistry
	template<class Ret, class Context, class... Params, class = EnableIfEditorContext<Context>>
	QCommandAction* Register(const string& actionId, std::function<Ret(Context, Params...)> actionHandler, uintptr_t id,
		EActionHandlerPermission perm = EActionHandlerPermission::Default, int priority = DEF_HANDLER_PRIORITY)
	{
		auto func = [this, actionHandler = std::move(actionHandler)](Params&&... params)
		{
			actionHandler(static_cast<Context>(m_widget.GetContextObject()), std::forward<Params>(params)...);
		};

		return m_widget.RegisterAction(actionId, std::move(func), perm, priority, id);
	}

	//! Registers a member function an action in the registry of specific widget.
	//! /param actionId id of the action to register.
	//! /param actionHandler The handler the will be called when the action is triggered on the widget.
	//!     The first parameter to the handler must be a pointer to widget's context object.
	//! /param pObj the object instance on which the member function should be called.
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//!     If id == 0 the address of the pObj will be used as id.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /see SPrioritizedRegistry
	template<class Func, class T>
	QCommandAction* Register(const string& actionid, Func&& actionHandler, T* pObj,
		EActionHandlerPermission perm = EActionHandlerPermission::Default, int priority = DEF_HANDLER_PRIORITY, uintptr_t id = 0)
	{
		return Register(actionid, ToStdFunction(std::forward<Func>(actionHandler), pObj)
			, id ? id : reinterpret_cast<uintptr_t>(pObj), perm, priority);
	}

	//! Unregister an action from the registry of specific widget.
	void Unregister(const string& actionId, uintptr_t id);

	//! Unregister an action from the registry of specific widget.
	void Unregister(const string& actionId, void* pObj, uintptr_t id = 0);

private:
	CEditorWidget& m_widget;
};
