// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Switch.h"
#include "Object.h"

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
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
void CSwitch::Set(IObject* const pIObject)
{
	auto const pObject = static_cast<CObject const*>(pIObject);

	AK::SoundEngine::SetSwitch(m_switchGroupId, m_switchId, pObject->GetId());
}

//////////////////////////////////////////////////////////////////////////
void CSwitch::SetGlobally()
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	Cry::Audio::Log(ELogType::Warning, "Wwise - Switches cannot get set globally! Tried to set \"%s: %s\"", m_switchGroupName.c_str(), m_switchName.c_str());
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
