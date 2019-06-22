// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ContextManager.h"

#include "AssetsManager.h"
#include "Context.h"
#include "ContextWidget.h"

#include <PathUtils.h>

namespace ACE
{
constexpr uint16 g_contextPoolSize = 32;

//////////////////////////////////////////////////////////////////////////
CContextManager::CContextManager()
{
	g_contexts.reserve(static_cast<size_t>(g_contextPoolSize));
}

//////////////////////////////////////////////////////////////////////////
CContextManager::~CContextManager()
{
	ClearAll();
	CContext::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::Initialize()
{
	CContext::CreateAllocator(g_contextPoolSize);
	AddGlobalContext();
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::Clear()
{
	ClearAll();
	AddGlobalContext();
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::ClearAll()
{
	SignalOnBeforeClear();

	for (auto const pContext : g_contexts)
	{
		delete pContext;
	}

	g_contexts.clear();

	SignalOnAfterClear();
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::AddGlobalContext()
{
	auto const pContext = new CContext(CryAudio::GlobalContextId, CryAudio::g_szGlobalContextName, true, true);

	SignalOnBeforeContextAdded(pContext);
	g_contexts.push_back(pContext);
	SignalOnAfterContextAdded(pContext);
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::TryCreateContext(string const& name, bool const isRegistered)
{
	CryAudio::ContextId const contextId = GenerateContextId(name);

	if (!ContextExists(contextId))
	{
		bool const isActive = std::find(g_activeUserDefinedContexts.begin(), g_activeUserDefinedContexts.end(), contextId) != g_activeUserDefinedContexts.end();
		auto const pContext = new CContext(contextId, name, isActive, isRegistered);

		SignalOnBeforeContextAdded(pContext);
		g_contexts.push_back(pContext);
		SignalOnAfterContextAdded(pContext);
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::TryRegisterContexts(ContextIds const& ids)
{
	for (auto const pContext : g_contexts)
	{
		CryAudio::ContextId const contextId = pContext->GetId();

		for (auto const id : ids)
		{
			if (contextId == id)
			{
				pContext->SetRegistered(true);
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CContextManager::ContextExists(CryAudio::ContextId const id) const
{
	bool contextExists = false;

	for (auto const pContext : g_contexts)
	{
		if (pContext->GetId() == id)
		{
			contextExists = true;
			break;
		}
	}

	return contextExists;
}

//////////////////////////////////////////////////////////////////////////
CryAudio::ContextId CContextManager::GenerateContextId(string const& name) const
{
	return CryAudio::StringToId(name.c_str());
}

//////////////////////////////////////////////////////////////////////////
string CContextManager::GetContextName(CryAudio::ContextId const contextId) const
{
	// Initialized to g_szGlobalContextName to avoid crash in property tree if no valid context id is found.
	string name = CryAudio::g_szGlobalContextName;

	for (auto const pContext : g_contexts)
	{
		if (pContext->GetId() == contextId)
		{
			name = pContext->GetName();
			break;
		}
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::SetContextActive(CryAudio::ContextId const contextId)
{
	g_activeUserDefinedContexts.emplace_back(contextId);

	for (auto const pContext : g_contexts)
	{
		if (pContext->GetId() == contextId)
		{
			pContext->SetActive(true);
			break;
		}
	}

	if (g_pContextWidget != nullptr)
	{
		g_pContextWidget->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::SetContextInactive(CryAudio::ContextId const contextId)
{
	// GlobalContextId is used to notify that all contexts have been deactivated.

	if (contextId == CryAudio::GlobalContextId)
	{
		g_activeUserDefinedContexts.clear();

		for (auto const pContext : g_contexts)
		{
			if (pContext->GetId() != CryAudio::GlobalContextId)
			{
				pContext->SetActive(false);
			}
		}
	}
	else
	{
		g_activeUserDefinedContexts.erase(std::remove(g_activeUserDefinedContexts.begin(), g_activeUserDefinedContexts.end(), contextId), g_activeUserDefinedContexts.end());

		for (auto const pContext : g_contexts)
		{
			if (pContext->GetId() == contextId)
			{
				pContext->SetActive(false);
				break;
			}
		}
	}

	if (g_pContextWidget != nullptr)
	{
		g_pContextWidget->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CContextManager::RenameContext(CContext* const pContext, string const& newName)
{
	bool wasRenamed = false;

	string const& oldName = pContext->GetName();

	if (!newName.empty() && (newName.compareNoCase(oldName) != 0))
	{
		CryAudio::ContextId const oldContextId = pContext->GetId();

		if (pContext->IsActive())
		{
			gEnv->pAudioSystem->DeactivateContext(oldContextId);
		}

		pContext->SetName(GenerateUniqueContextName(newName));
		pContext->SetActive(false);
		pContext->SetRegistered(false);

		CryAudio::ContextId const newContextId = pContext->GetId();
		g_assetsManager.ChangeContext(oldContextId, newContextId);

		wasRenamed = true;
		SignalContextRenamed();
	}

	return wasRenamed;
}

//////////////////////////////////////////////////////////////////////////
void CContextManager::DeleteContext(CContext const* const pContext)
{
	if (pContext->IsActive())
	{
		gEnv->pAudioSystem->DeactivateContext(pContext->GetId());
	}

	SignalOnBeforeContextRemoved(pContext);
	g_contexts.erase(std::remove(g_contexts.begin(), g_contexts.end(), pContext), g_contexts.end());

	delete pContext;

	SignalOnAfterContextRemoved();
}

//////////////////////////////////////////////////////////////////////////
string CContextManager::GenerateUniqueContextName(string const& name) const
{
	AssetNames names;
	names.reserve(g_contexts.size());

	for (auto const pContext : g_contexts)
	{
		names.emplace_back(pContext->GetName());
	}

	return PathUtil::GetUniqueName(name, names);
}
} // namespace ACE
