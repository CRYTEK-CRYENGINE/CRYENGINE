// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/EditorWidget.h"

#include <functional>

struct IPane;

//! This global widget registry allow registration/unregistration of commands to widgets from any object.
//! These operations are handled for all instances of certain widget type. And whenever a new instance 
//! of the widget type is created, all commands that are registered for this type will be registered on the widget.
//! Example:
//!
//!     void OnAssetBrowserRefresh(const IAssetBrowserContext* pContext)
//!     {
//!			// use pContext to get any information about AssetBrowser's state
//!     }
//!     
//!     auto& registry = CWidgetsGlobalActionRegistry::GetInstance();
//!     registry.Register("Asset Browser", "version_control_system.refresh", ToStdFunction(&OnAssetBrowserRefresh), id);
class EDITOR_COMMON_API CWidgetsGlobalActionRegistry
{
public:
	using HandlerFunc = std::function<void(const string&, CWidgetActionRegistry*)>;

	static CWidgetsGlobalActionRegistry& GetInstance();

	CWidgetsGlobalActionRegistry();

	//! Register an action handler for a specific widget type.
	//! /param widgetName name of the widget type (like "Asset Browser") to register action handler for.
	//! /param actionId name of the command to register on all widget instances.
	//! /param actionHandler handler to trigger whenever the action needs to be executed.
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /see SPrioritizedRegistry
	template<class Ret, class... Params>
	void Register(const string& widgetName, const string& actionid, std::function<Ret(Params...)> actionHandler, uintptr_t id,
		EActionHandlerPermission perm = EActionHandlerPermission::Default, int priority = DEF_HANDLER_PRIORITY)
	{
		auto regFunc = [actionHandler = std::move(actionHandler), id, perm, priority](const string& actionId, CWidgetActionRegistry* pRegistry)
		{
			pRegistry->Register(actionId, actionHandler, id, perm, priority);
		};
		SaveActionHandlerAndRegisterOnPanes(widgetName, actionid, std::move(regFunc));
	}

	//! Register an action handler for a specific widget type.
	//! /param widgetName name of the widget type (like "Asset Browser") to register action handler for.
	//! /param actionId name of the command to register on all widget instances.
	//! /param actionHandler handler to trigger whenever the action needs to be executed.
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /see SPrioritizedRegistry
	template<class Func>
	void Register(const string& widgetName, const string& actionid, Func&& actionHandler, uintptr_t id,
		EActionHandlerPermission perm = EActionHandlerPermission::Default, int priority = DEF_HANDLER_PRIORITY)
	{
		Register(widgetName, actionid, ToStdFunction(std::forward<Func>(actionHandler)), id, perm, priority);
	}

	//! Register a member function as an action handler for a specific widget type.
	//! /param widgetName name of the widget type (like "Asset Browser") to register action handler for.
	//! /param actionId name of the command to register on all widget instances.
	//! /param actionHandler member function to trigger whenever the action needs to be executed.
	//! /param pObj the object instance on which the member function should be called.
	//! /param perm handler's permission
	//! /param priority handler's priority
	//! /param id of the handler. It needs to be used in order to unregister certain handler.
	//!     If id == 0 the address of the pObj will be used as id.
	//! /see SPrioritizedRegistry
	template<class Func, class T>
	void Register(const string& widgetName, const string& actionid, Func&& actionHandler, T* pObj,
		EActionHandlerPermission perm = EActionHandlerPermission::Default, int priority = DEF_HANDLER_PRIORITY, uintptr_t id = 0)
	{
		Register(widgetName, actionid, ToStdFunction(std::forward<Func>(actionHandler), pObj)
			, id ? id : reinterpret_cast<uintptr_t>(pObj), perm, priority);
	}

	//! Unregister an action handler from a specific widget type.
	//! /param widgetName name of the widget type (like "Asset Browser") to unregister action handler from.
	//! /param actionId name of the command to unregister from all widget instances.
	//! /param id of the handler.
	void Unregister(const string& widgetName, const string& actionId, uintptr_t id);

	//! Unregister a member function as an action handler from a specific widget type.
	//! /param widgetName name of the widget type (like "Asset Browser") to register action handler for.
	//! /param actionId name of the command to unregister from all widget instances.
	//! /param pObj the object instance on which the member function should be called.
	void Unregister(const string& widgetName, const string& actionId, void* pObj, uintptr_t id = 0)
	{
		Unregister(widgetName, actionId, id ? id : reinterpret_cast<uintptr_t>(pObj));
	}

private:
	std::vector<CWidgetActionRegistry*> GetAllWidgetRegistries(const string& widgetName);

	void OnPaneCreated(IPane* pPane);

	void SaveActionHandlerAndRegisterOnPanes(const string& widgetName, const string& actionId, HandlerFunc);
};