// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CEnvironment final : public CEntity<EnvironmentId>, public CPoolObject<CEnvironment, stl::PSyncNone>
{
public:

	CEnvironment() = delete;
	CEnvironment(CEnvironment const&) = delete;
	CEnvironment(CEnvironment&&) = delete;
	CEnvironment& operator=(CEnvironment const&) = delete;
	CEnvironment& operator=(CEnvironment&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CEnvironment(
		EnvironmentId const id,
		ContextId const contextId,
		EnvironmentConnections const& connections,
		char const* const szName)
		: CEntity<EnvironmentId>(id, contextId, szName)
		, m_connections(connections)
	{}
#else
	explicit CEnvironment(
		EnvironmentId const id,
		ContextId const contextId,
		EnvironmentConnections const& connections)
		: CEntity<EnvironmentId>(id, contextId)
		, m_connections(connections)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CEnvironment();

	void Set(CObject const& object, float const value) const;

private:

	EnvironmentConnections const m_connections;
};
} // namespace CryAudio
