// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

namespace ACE
{
class CImplManager final
{
public:

	CImplManager(CImplManager const&) = delete;
	CImplManager(CImplManager&&) = delete;
	CImplManager& operator=(CImplManager const&) = delete;
	CImplManager& operator=(CImplManager&&) = delete;

	CImplManager() = default;
	~CImplManager();

	bool LoadImpl();
	void Release();

	CCrySignal<void()> SignalOnBeforeImplChange;
	CCrySignal<void()> SignalOnAfterImplChange;

private:

	HMODULE m_hMiddlewarePlugin = nullptr;
};
} // namespace ACE
