// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "WidgetsGlobalActionRegistry.h"
#include "QtViewPane.h"

namespace Private_WidgetsGlobalActionRegistry
{
	CWidgetsGlobalActionRegistry* g_pRegistry;

	using RegFunc = CWidgetsGlobalActionRegistry::HandlerFunc;

	struct SActionHandlers
	{
		std::vector<std::pair<string, RegFunc>> list;
	};

	using WidgetsHandlers = std::pair<string, SActionHandlers>;
	std::vector<WidgetsHandlers> s_widgetActionHandlers;

	void AddActoinHandler(const string& actionId, SActionHandlers& handlers, RegFunc f)
	{
		handlers.list.emplace_back(std::move(actionId), std::move(f));
	}

	int FindWidgetHandlers(const string& widgetName)
	{
		auto it = std::find_if(s_widgetActionHandlers.cbegin(), s_widgetActionHandlers.cend(), [&widgetName](const WidgetsHandlers& handlers)
		{
			return handlers.first == widgetName;
		});
		return it == s_widgetActionHandlers.cend() ? -1 : std::distance(s_widgetActionHandlers.cbegin(), it);
	}

	void SaveActionHandler(const string& widgetName, const string& actionId, RegFunc f)
	{
		int index = FindWidgetHandlers(widgetName);
		if (index == -1)
		{
			s_widgetActionHandlers.emplace_back(widgetName, SActionHandlers());
			index = s_widgetActionHandlers.size() - 1;
		}
		AddActoinHandler(actionId, s_widgetActionHandlers[index].second, std::move(f));
	}

	void RemoveActionHandler(const string& widgetName, const string& actionId)
	{
		int index = FindWidgetHandlers(widgetName);
		CRY_ASSERT_MESSAGE(index != -1, "Widget %s has no action handlers", widgetName);
		if (index == -1)
		{
			return;
		}
		SActionHandlers& handlers = s_widgetActionHandlers[index].second;
		auto it = std::find_if(handlers.list.begin(), handlers.list.end(), [&actionId](const auto& pair)
		{
			return pair.first == actionId;
		});
		CRY_ASSERT_MESSAGE(it != handlers.list.end(), "Action handler %s for widget %s doesn't exist", actionId, widgetName);
		if (it == handlers.list.end())
		{
			return;
		}
		handlers.list.erase(it);
	}
}

CWidgetsGlobalActionRegistry& CWidgetsGlobalActionRegistry::GetInstance()
{
	return *Private_WidgetsGlobalActionRegistry::g_pRegistry;
}

CWidgetsGlobalActionRegistry::CWidgetsGlobalActionRegistry()
{
	using namespace Private_WidgetsGlobalActionRegistry;
	CRY_ASSERT_MESSAGE(!g_pRegistry, "WidgetCommandRegistry can be instantiated only once");
	g_pRegistry = this;
	IPane::s_signalPaneCreated.Connect(this, &CWidgetsGlobalActionRegistry::OnPaneCreated);
}

void CWidgetsGlobalActionRegistry::Unregister(const string& widgetName, const string& actionId, uintptr_t id)
{
	using namespace Private_WidgetsGlobalActionRegistry;
	for (CWidgetActionRegistry* pRegistry : GetAllWidgetRegistries(widgetName))
	{
		pRegistry->Unregister(actionId, id);
	}
	RemoveActionHandler(widgetName, actionId);
}

std::vector<CWidgetActionRegistry*> CWidgetsGlobalActionRegistry::GetAllWidgetRegistries(const string& widgetName)
{
	const auto& panes = GetIEditor()->FindAllDockables(widgetName);
	std::vector<CWidgetActionRegistry*> result;
	result.reserve(panes.size());
	for (IPane* pPane : panes)
	{
		CEditorWidget* pEditorWidget = qobject_cast<CEditorWidget*>(pPane->GetWidget());
		if (pEditorWidget)
		{
			result.push_back(&pEditorWidget->GetActionRegistry());
		}
	}
	return result;
}

void CWidgetsGlobalActionRegistry::OnPaneCreated(IPane* pPane)
{
	using namespace Private_WidgetsGlobalActionRegistry;
	auto it = std::find_if(s_widgetActionHandlers.cbegin(), s_widgetActionHandlers.cend(), [pPane](const auto& pair)
	{
		return pPane->GetPaneTitle() == pair.first;
	});
	if (it == s_widgetActionHandlers.cend())
	{
		return;
	}
	const SActionHandlers& handlers = it->second;
	CEditorWidget* pEditorWidget = qobject_cast<CEditorWidget*>(pPane->GetWidget());
	if (!pEditorWidget)
	{
		return;
	}
	CWidgetActionRegistry& registry = pEditorWidget->GetActionRegistry();
	for (const auto& handler : handlers.list)
	{
		handler.second(handler.first, &registry);
	}
}

void CWidgetsGlobalActionRegistry::SaveActionHandlerAndRegisterOnPanes(const string& widgetName, const string& actionId, HandlerFunc handlerFunc)
{
	using namespace Private_WidgetsGlobalActionRegistry;
	for (CWidgetActionRegistry* pRegistry : GetAllWidgetRegistries(widgetName))
	{
		handlerFunc(actionId, pRegistry);
	}
	SaveActionHandler(widgetName, actionId, std::move(handlerFunc));
}
