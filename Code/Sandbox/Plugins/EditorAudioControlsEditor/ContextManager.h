// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySandbox/CrySignal.h>

namespace ACE
{
class CContext;

class CContextManager final
{
public:

	CContextManager(CContextManager const&) = delete;
	CContextManager(CContextManager&&) = delete;
	CContextManager& operator=(CContextManager const&) = delete;
	CContextManager& operator=(CContextManager&&) = delete;

	CContextManager();
	~CContextManager();

	void                Initialize();
	void                Clear();

	void                TryCreateContext(string const& name, bool const isRegistered);
	void                TryRegisterContexts(ContextIds const& ids);

	CryAudio::ContextId GenerateContextId(string const& name) const;
	string              GetContextName(CryAudio::ContextId const contextId) const;
	string              GenerateUniqueContextName(string const& name) const;

	void                SetContextActive(CryAudio::ContextId const contextId);
	void                SetContextInactive(CryAudio::ContextId const contextId);

	bool                RenameContext(CContext* const pContext, string const& newName);
	void                DeleteContext(CContext const* const pContext);

	CCrySignal<void(CContext*)>       SignalOnBeforeContextAdded;
	CCrySignal<void(CContext*)>       SignalOnAfterContextAdded;
	CCrySignal<void(CContext const*)> SignalOnBeforeContextRemoved;
	CCrySignal<void()>                SignalOnAfterContextRemoved;
	CCrySignal<void()>                SignalContextRenamed;
	CCrySignal<void()>                SignalOnBeforeClear;
	CCrySignal<void()>                SignalOnAfterClear;

private:

	void ClearAll();
	void AddGlobalContext();
	bool ContextExists(CryAudio::ContextId const id) const;
};
} // namespace ACE
