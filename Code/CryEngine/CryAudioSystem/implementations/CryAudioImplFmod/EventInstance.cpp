// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EventInstance.h"
#include "Object.h"
#include "CVars.h"
#include "Return.h"
#include "Event.h"
#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CEventInstance::~CEventInstance()
{
	if (m_pInstance != nullptr)
	{
		FMOD_RESULT const fmodResult = m_pInstance->release();
		CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_INVALID_HANDLE;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEventInstance::PrepareForOcclusion()
{
	m_pMasterTrack = nullptr;
	FMOD_RESULT const fmodResult = m_pInstance->getChannelGroup(&m_pMasterTrack);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_NOT_LOADED;

	if ((m_pMasterTrack != nullptr) && ((m_event.GetFlags() & EEventFlags::HasOcclusionParameter) == 0))
	{
		m_pLowpass = nullptr;
		int numDSPs = 0;
		CRY_VERIFY(m_pMasterTrack->getNumDSPs(&numDSPs) == FMOD_OK);

		for (int i = 0; i < numDSPs; ++i)
		{
			CRY_VERIFY(m_pMasterTrack->getDSP(i, &m_pLowpass) == FMOD_OK);

			if (m_pLowpass != nullptr)
			{
				FMOD_DSP_TYPE dspType;
				CRY_VERIFY(m_pLowpass->getType(&dspType) == FMOD_OK);

				if (dspType == FMOD_DSP_TYPE_LOWPASS_SIMPLE || dspType == FMOD_DSP_TYPE_LOWPASS)
				{
					FMOD_DSP_PARAMETER_DESC* pParameterDesc = nullptr;
					CRY_VERIFY(m_pLowpass->getParameterInfo(FMOD_DSP_LOWPASS_CUTOFF, &pParameterDesc) == FMOD_OK);

					m_lowpassFrequencyMin = pParameterDesc->floatdesc.min;
					m_lowpassFrequencyMax = pParameterDesc->floatdesc.max;
					break;
				}
				else
				{
					m_pLowpass = nullptr;
				}
			}
		}
	}

	return m_pMasterTrack != nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::SetOcclusion(float const occlusion)
{
	if (((m_event.GetFlags() & EEventFlags::HasOcclusionParameter) == 0) && (m_pLowpass != nullptr))
	{
		float const range = m_lowpassFrequencyMax - std::max(m_lowpassFrequencyMin, g_cvars.m_lowpassMinCutoffFrequency);
		float const value = m_lowpassFrequencyMax - (occlusion * range);
		CRY_VERIFY(m_pLowpass->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, value) == FMOD_OK);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::SetReturnSend(CReturn const* const pReturn, float const value)
{
	if (m_pMasterTrack != nullptr)
	{
		FMOD::ChannelGroup* pChannelGroup = nullptr;
		CRY_VERIFY(pReturn->GetBus()->getChannelGroup(&pChannelGroup) == FMOD_OK);

		if (pChannelGroup != nullptr)
		{
			FMOD::DSP* pDsp = nullptr;
			CRY_VERIFY(pChannelGroup->getDSP(FMOD_CHANNELCONTROL_DSP_TAIL, &pDsp) == FMOD_OK);

			if (pDsp != nullptr)
			{
				int returnId1 = CRY_AUDIO_IMPL_FMOD_INVALID_INDEX;
				CRY_VERIFY(pDsp->getParameterInt(FMOD_DSP_RETURN_ID, &returnId1, nullptr, 0) == FMOD_OK);

				int numDSPs = 0;
				CRY_VERIFY(m_pMasterTrack->getNumDSPs(&numDSPs) == FMOD_OK);

				for (int i = 0; i < numDSPs; ++i)
				{
					FMOD::DSP* pSend = nullptr;
					CRY_VERIFY(m_pMasterTrack->getDSP(i, &pSend) == FMOD_OK);

					if (pSend != nullptr)
					{
						FMOD_DSP_TYPE dspType;
						CRY_VERIFY(pSend->getType(&dspType) == FMOD_OK);

						if (dspType == FMOD_DSP_TYPE_SEND)
						{
							int returnId2 = CRY_AUDIO_IMPL_FMOD_INVALID_INDEX;
							CRY_VERIFY(pSend->getParameterInt(FMOD_DSP_RETURN_ID, &returnId2, nullptr, 0) == FMOD_OK);

							if (returnId1 == returnId2)
							{
								CRY_VERIFY(pSend->setParameterFloat(FMOD_DSP_SEND_LEVEL, value) == FMOD_OK);
								break;
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::UpdateVirtualState()
{
	// Workaround until Fmod has callbacks for virtual/physical states.
	if (m_pMasterTrack != nullptr)
	{
		float audibility = 0.0f;
		m_pMasterTrack->getAudibility(&audibility);

		m_state = (audibility < 0.01f) ? EEventState::Virtual : EEventState::Playing;
	}
	else
	{
		m_state = EEventState::None;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::StopAllowFadeOut()
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	m_isFadingOut = true;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	CRY_VERIFY(m_pInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT) == FMOD_OK);
}

//////////////////////////////////////////////////////////////////////////
void CEventInstance::StopImmediate()
{
	CRY_VERIFY(m_pInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE) == FMOD_OK);
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
