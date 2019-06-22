// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Snapshot.h"
#include "Common.h"

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#include <Logger.h>
#endif // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
ETriggerResult CSnapshot::Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId)
{
	ETriggerResult result = ETriggerResult::Failure;

	switch (m_actionType)
	{
	case EActionType::Start:
		{
			if (g_activeSnapshots.find(m_id) == g_activeSnapshots.end())
			{
				FMOD::Studio::EventInstance* pFmodEventInstance = nullptr;
				FMOD_RESULT fmodResult = m_pEventDescription->createInstance(&pFmodEventInstance);
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
				fmodResult = pFmodEventInstance->start();
				CRY_AUDIO_IMPL_FMOD_ASSERT_OK;

				g_activeSnapshots[m_id] = pFmodEventInstance;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
				g_activeSnapshotNames.push_back(m_name);
#endif        // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
			}
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Warning, "Snapshot %s is already active during %s", m_name.c_str(), __FUNCTION__);
			}
#endif        // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

			result = ETriggerResult::DoNotTrack;

			break;
		}
	case EActionType::Stop:
		{
			Stop(nullptr);
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
void CSnapshot::Stop(IObject* const pIObject)
{
	for (auto const& snapshotPair : g_activeSnapshots)
	{
		if (snapshotPair.first == m_id)
		{
			snapshotPair.second->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
			// snaphotInstancePair.second->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			g_activeSnapshots.erase(m_id);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
			g_activeSnapshotNames.erase(std::remove(g_activeSnapshotNames.begin(), g_activeSnapshotNames.end(), m_name), g_activeSnapshotNames.end());
#endif      // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

			break;
		}
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
