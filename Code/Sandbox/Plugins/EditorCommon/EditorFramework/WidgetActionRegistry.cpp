// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "WidgetActionRegistry.h"
#include "EditorWidget.h"

void CWidgetActionRegistry::Unregister(const string& actionId, uintptr_t id)
{
	m_widget.UnregisterAction(actionId, id);
}

//! Unregister an action from the registry of specific widget.
void CWidgetActionRegistry::Unregister(const string& actionId, void* pObj, uintptr_t id /*= 0*/)
{
	Unregister(actionId, id ? id : reinterpret_cast<uintptr_t>(pObj));
}

SPrioritizedRegistry::SPrioritizedRegistry()
{
	// Sort commands with exclusive permission first.
	// Handlers with the same permission are sorted by priority (highest first).
	comparePriorityFunc = [](const auto& left, const auto& right)
	{
		return std::get<0>(left) == std::get<0>(right) ? std::get<1>(left) > std::get<1>(right)
			: std::get<0>(left) == EActionHandlerPermission::Exclusive;
	};

	// Stop further execution is current handler is exclusive
	evalPriorityFunc = [](size_t ind, const std::vector<std::tuple<EActionHandlerPermission, int>>& priorities)
	{
		return std::get<0>(priorities[ind]) != EActionHandlerPermission::Exclusive;
	};

	defaults = { EActionHandlerPermission::Default , DEF_HANDLER_PRIORITY };
}
