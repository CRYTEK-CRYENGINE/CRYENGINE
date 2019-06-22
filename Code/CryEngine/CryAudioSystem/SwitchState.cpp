// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SwitchState.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include "Common/ISwitchStateConnection.h"

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Object.h"
	#include "Common/Logger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CSwitchState::~CSwitchState()
{
	CRY_ASSERT_MESSAGE(g_pIImpl != nullptr, "g_pIImpl mustn't be nullptr during %s", __FUNCTION__);

	for (auto const pConnection : m_connections)
	{
		g_pIImpl->DestructSwitchStateConnection(pConnection);
	}
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CSwitchState::Set(CObject const& object) const
{
	Impl::IObject* const pIObject = object.GetImplData();

	for (auto const pConnection : m_connections)
	{
		pConnection->Set(pIObject);
	}

	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(SwitchState "%s" set on object "%s" without connections)", GetName(), object.GetName());
	}

	const_cast<CObject&>(object).StoreSwitchValue(m_switchId, m_switchStateId);
}
#else
//////////////////////////////////////////////////////////////////////////
void CSwitchState::Set(Impl::IObject* const pIObject) const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->Set(pIObject);
	}
}
#endif   // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CSwitchState::Set() const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->Set(g_pIObject);
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(SwitchState "%s" set without connections)", GetName());
	}

	g_switchStates[m_switchId] = m_switchStateId;
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSwitchState::SetGlobally() const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->SetGlobally();
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(SwitchState "%s" set globally without connections)", GetName());
	}

	g_switchStatesGlobally[m_switchId] = m_switchStateId;
#endif // CRY_AUDIO_USE_DEBUG_CODE
}
} // namespace CryAudio
