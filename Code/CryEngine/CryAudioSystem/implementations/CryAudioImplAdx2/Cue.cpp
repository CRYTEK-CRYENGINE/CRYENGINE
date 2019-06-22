// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Cue.h"
#include "Object.h"
#include "CueInstance.h"
#include "Impl.h"
#include "Listener.h"

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include <Logger.h>
#endif // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
ETriggerResult CCue::Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId)
{
	ETriggerResult result = ETriggerResult::Failure;

	auto const pObject = static_cast<CObject*>(pIObject);

	switch (m_actionType)
	{
	case EActionType::Start:
		{
			CriAtomExPlayerHn const pPlayer = pObject->GetPlayer();
			auto const iter = g_acbHandles.find(m_cueSheetId);

			if (iter != g_acbHandles.end())
			{
				m_pAcbHandle = iter->second;
				auto const cueName = static_cast<CriChar8 const*>(m_name.c_str());

				criAtomExPlayer_SetCueName(pPlayer, m_pAcbHandle, cueName);
				CriAtomExPlaybackId const playbackId = criAtomExPlayer_Start(pPlayer);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
				CCueInstance* const pCueInstance = g_pImpl->ConstructCueInstance(triggerInstanceId, playbackId, *this, *pObject);
#else
				CCueInstance* const pCueInstance = g_pImpl->ConstructCueInstance(triggerInstanceId, playbackId, *this);
#endif                // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

				pObject->AddCueInstance(pCueInstance);

				if (pCueInstance->PrepareForPlayback(*pObject))
				{
					result = ((pCueInstance->GetFlags() & ECueInstanceFlags::IsVirtual) != 0) ? ETriggerResult::Virtual : ETriggerResult::Playing;
				}
				else
				{
					result = ETriggerResult::Pending;
				}
			}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Warning, R"(Cue "%s" failed to play because ACB file "%s" was not loaded)",
				                static_cast<char const*>(m_name), static_cast<char const*>(m_cueSheetName));
			}
#endif        // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

			break;
		}
	case EActionType::Stop:
		{
			pObject->StopCue(m_id);
			result = ETriggerResult::DoNotTrack;

			break;
		}
	case EActionType::Pause:
		{
			pObject->PauseCue(m_id);
			result = ETriggerResult::DoNotTrack;
			break;
		}
	case EActionType::Resume:
		{
			pObject->ResumeCue(m_id);
			result = ETriggerResult::DoNotTrack;

			break;
		}
	default:
		{
			break;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CCue::Stop(IObject* const pIObject)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->StopCue(m_id);
}

//////////////////////////////////////////////////////////////////////////
void CCue::DecrementNumInstances()
{
	CRY_ASSERT_MESSAGE(m_numInstances > 0, "Number of cue instances must be at least 1 during %s", __FUNCTION__);
	--m_numInstances;
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
