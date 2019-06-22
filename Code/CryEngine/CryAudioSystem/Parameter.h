// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CParameter final : public Control, public CPoolObject<CParameter, stl::PSyncNone>
{
public:

	CParameter() = delete;
	CParameter(CParameter const&) = delete;
	CParameter(CParameter&&) = delete;
	CParameter& operator=(CParameter const&) = delete;
	CParameter& operator=(CParameter&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CParameter(
		ControlId const id,
		ContextId const contextId,
		ParameterConnections const& connections,
		char const* const szName)
		: Control(id, contextId, szName)
		, m_connections(connections)
	{}
#else
	explicit CParameter(
		ControlId const id,
		ContextId const contextId,
		ParameterConnections const& connections)
		: Control(id, contextId)
		, m_connections(connections)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	~CParameter();

	void Set(float const value) const;
	void SetGlobally(float const value) const;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	void Set(CObject const& object, float const value) const;
#else
	void Set(Impl::IObject* const pIObject, float const value) const;
#endif // CRY_AUDIO_USE_DEBUG_CODE

private:

	ParameterConnections const m_connections;
};
} // namespace CryAudio
