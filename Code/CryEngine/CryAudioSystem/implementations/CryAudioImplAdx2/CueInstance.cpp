// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CueInstance.h"
#include "Common.h"
#include "Object.h"
#include "Cue.h"

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include <CrySystem/ITimer.h>
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
bool CCueInstance::PrepareForPlayback(CObject& object)
{
	bool isPlaying = false;

	if (criAtomExPlayback_GetStatus(m_playbackId) == CRIATOMEXPLAYBACK_STATUS_PLAYING)
	{
		CriAtomExAcbHn const pAcbHandle = m_cue.GetAcbHandle();
		auto const cueName = static_cast<CriChar8 const*>(m_cue.GetName());
		CriAtomExCueInfo cueInfo;

		if (criAtomExAcb_GetCueInfoByName(pAcbHandle, cueName, &cueInfo) == CRI_TRUE)
		{
			if (cueInfo.pos3d_info.doppler_factor > 0.0f)
			{
				SetFlag(ECueInstanceFlags::HasDoppler);
			}
		}

		if (criAtomExAcb_IsUsingAisacControlByName(pAcbHandle, cueName, g_szAbsoluteVelocityAisacName) == CRI_TRUE)
		{
			SetFlag(ECueInstanceFlags::HasAbsoluteVelocity);
		}

		RemoveFlag(ECueInstanceFlags::IsPending);
		object.UpdateVelocityTracking();

		isPlaying = true;
	}

	return isPlaying;
}

//////////////////////////////////////////////////////////////////////////
void CCueInstance::Stop()
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	StartFadeOut();
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	criAtomExPlayback_Stop(m_playbackId);
}

//////////////////////////////////////////////////////////////////////////
void CCueInstance::Pause()
{
	criAtomExPlayback_Pause(m_playbackId, CRI_TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CCueInstance::Resume()
{
	criAtomExPlayback_Pause(m_playbackId, CRI_FALSE);
}

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CCueInstance::StartFadeOut()
{
	if ((m_cue.GetFadeOutTime() > 0.0f) && ((m_flags& ECueInstanceFlags::IsStopping) == 0))
	{
		SetFlag(ECueInstanceFlags::IsStopping);
		m_timeFadeOutStarted = gEnv->pTimer->GetAsyncTime().GetSeconds();
	}
}
#endif // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}      // namespace Adx2
}      // namespace Impl
}      // namespace CryAudio
