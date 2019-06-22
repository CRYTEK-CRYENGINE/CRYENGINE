// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "State.h"

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include "Object.h"
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

#include <AK/SoundEngine/Common/AkSoundEngine.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void CState::Set(IObject* const pIObject)
{
	SetGlobally();

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	auto const pObject = static_cast<CObject*>(pIObject);
	Cry::Audio::Log(ELogType::Warning, R"(Wwise - state "%s: %s" was set on object "%s". Consider setting it globally.)",
	                m_stateGroupName.c_str(), m_stateName.c_str(), pObject->GetName());
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CState::SetGlobally()
{
	AK::SoundEngine::SetState(m_stateGroupId, m_stateId);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	g_debugStates[m_stateGroupName] = m_stateName;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
