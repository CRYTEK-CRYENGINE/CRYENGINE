// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"

namespace ACE
{
class CListener;

class CListenerManager final
{
public:

	CListenerManager(CListenerManager const&) = delete;
	CListenerManager(CListenerManager&&) = delete;
	CListenerManager& operator=(CListenerManager const&) = delete;
	CListenerManager& operator=(CListenerManager&&) = delete;

	CListenerManager() = default;

	void          Initialize();
	size_t        GetNumListeners() const                   { return m_listeners.size(); }
	string const& GetListenerName(size_t const index) const { return m_listeners[index]; }

private:

	void GenerateUniqueListenerName(string& name);

	AssetNames m_listeners;
};
} // namespace ACE
