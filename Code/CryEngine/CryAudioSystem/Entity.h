// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
template<typename IDType>
class CEntity
{
public:

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CEntity(IDType const id, ContextId const contextId, char const* const szName)
		: m_id(id)
		, m_contextId(contextId)
		, m_name(szName)
	{}
#else
	explicit CEntity(IDType const id, ContextId const contextId)
		: m_id(id)
		, m_contextId(contextId)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	CEntity() = delete;
	CEntity(CEntity const&) = delete;
	CEntity(CEntity&&) = delete;
	CEntity&  operator=(CEntity const&) = delete;
	CEntity&  operator=(CEntity&&) = delete;

	IDType    GetId() const        { return m_id; }
	ContextId GetContextId() const { return m_contextId; }

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif // CRY_AUDIO_USE_DEBUG_CODE

protected:

	~CEntity() = default;

	IDType const    m_id;
	ContextId const m_contextId;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};

using Control = CEntity<ControlId>;
} // namespace CryAudio
