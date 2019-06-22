// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CSetting final : public Control, public CPoolObject<CSetting, stl::PSyncNone>
{
public:

	CSetting() = delete;
	CSetting(CSetting const&) = delete;
	CSetting(CSetting&&) = delete;
	CSetting& operator=(CSetting const&) = delete;
	CSetting& operator=(CSetting&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CSetting(
		ControlId const id,
		ContextId const contextId,
		bool const isAutoLoad,
		SettingConnections const& connections,
		char const* const szName)
		: Control(id, contextId, szName)
		, m_isAutoLoad(isAutoLoad)
		, m_connections(connections)
	{}
#else
	explicit CSetting(
		ControlId const id,
		ContextId const contextId,
		bool const isAutoLoad,
		SettingConnections const& connections)
		: Control(id, contextId)
		, m_isAutoLoad(isAutoLoad)
		, m_connections(connections)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CSetting();

	bool IsAutoLoad() const { return m_isAutoLoad; }

	void Load() const;
	void Unload() const;

private:

	bool const               m_isAutoLoad;
	SettingConnections const m_connections;
};
} // namespace CryAudio
