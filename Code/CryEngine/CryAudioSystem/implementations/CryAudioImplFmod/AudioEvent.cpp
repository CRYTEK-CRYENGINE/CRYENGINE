// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include "AudioImplCVars.h"

using namespace CryAudio::Impl::Fmod;

//////////////////////////////////////////////////////////////////////////
bool CAudioEvent::PrepareForOcclusion()
{
	m_pMasterTrack = nullptr;
	FMOD_RESULT fmodResult = m_pInstance->getChannelGroup(&m_pMasterTrack);
	ASSERT_FMOD_OK_OR_NOT_LOADED;

	if (m_pMasterTrack != nullptr)
	{
		m_pOcclusionParameter = nullptr;
		fmodResult = m_pInstance->getParameter("occlusion", &m_pOcclusionParameter);
		ASSERT_FMOD_OK_OR_EVENT_NOT_FOUND;

		if (m_pOcclusionParameter == nullptr)
		{
			m_pLowpass = nullptr;
			int numDSPs = 0;
			fmodResult = m_pMasterTrack->getNumDSPs(&numDSPs);
			ASSERT_FMOD_OK;

			for (int i = 0; i < numDSPs; ++i)
			{
				fmodResult = m_pMasterTrack->getDSP(i, &m_pLowpass);
				ASSERT_FMOD_OK;

				if (m_pLowpass != nullptr)
				{
					FMOD_DSP_TYPE dspType;
					fmodResult = m_pLowpass->getType(&dspType);
					ASSERT_FMOD_OK;

					if (dspType == FMOD_DSP_TYPE_LOWPASS_SIMPLE || dspType == FMOD_DSP_TYPE_LOWPASS)
					{
						FMOD_DSP_PARAMETER_DESC* pParameterDesc = nullptr;
						fmodResult = m_pLowpass->getParameterInfo(FMOD_DSP_LOWPASS_CUTOFF, &pParameterDesc);
						ASSERT_FMOD_OK;

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
	}

	return m_pMasterTrack != nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEvent::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	if (m_pOcclusionParameter != nullptr)
	{
		FMOD_RESULT const fmodResult = m_pOcclusionParameter->setValue(occlusion);
		ASSERT_FMOD_OK;
	}
	else if (m_pLowpass != nullptr)
	{
		float const range = m_lowpassFrequencyMax - std::max(m_lowpassFrequencyMin, g_audioImplCVars.m_lowpassMinCutoffFrequency);
		float const value = m_lowpassFrequencyMax - (occlusion * range);
		FMOD_RESULT const fmodResult = m_pLowpass->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, value);
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioEvent::Reset()
{
	if (m_pInstance != nullptr)
	{
		// Will be destroyed after the next update if the handle is valid.
		// It's fine if not though as in that case it's been already released.
		FMOD_RESULT const fmodResult = m_pInstance->release();
		ASSERT_FMOD_OK_OR_INVALID_HANDLE;
		m_pInstance = nullptr;
	}

	if (m_pAudioObject != nullptr)
	{
		m_pAudioObject->RemoveAudioEvent(this);
		m_pAudioObject = nullptr;
	}

	m_eventPathId = AUDIO_INVALID_CRC32;
	m_pMasterTrack = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEvent::TrySetEnvironment(CAudioEnvironment const* const pEnvironment, float const value)
{
	if (m_pInstance != nullptr && m_pMasterTrack != nullptr)
	{
		FMOD::ChannelGroup* pChannelGroup = nullptr;
		FMOD_RESULT fmodResult = pEnvironment->pBus->getChannelGroup(&pChannelGroup);
		ASSERT_FMOD_OK;

		if (pChannelGroup != nullptr)
		{
			FMOD::DSP* pReturn = nullptr;
			fmodResult = pChannelGroup->getDSP(FMOD_CHANNELCONTROL_DSP_TAIL, &pReturn);
			ASSERT_FMOD_OK;

			if (pReturn != nullptr)
			{
				int returnId1 = FMOD_IMPL_INVALID_INDEX;
				fmodResult = pReturn->getParameterInt(FMOD_DSP_RETURN_ID, &returnId1, nullptr, 0);
				ASSERT_FMOD_OK;

				int numDSPs = 0;
				fmodResult = m_pMasterTrack->getNumDSPs(&numDSPs);
				ASSERT_FMOD_OK;

				for (int i = 0; i < numDSPs; ++i)
				{
					FMOD::DSP* pSend = nullptr;
					fmodResult = m_pMasterTrack->getDSP(i, &pSend);
					ASSERT_FMOD_OK;

					if (pSend != nullptr)
					{
						FMOD_DSP_TYPE dspType;
						fmodResult = pSend->getType(&dspType);
						ASSERT_FMOD_OK;

						if (dspType == FMOD_DSP_TYPE_SEND)
						{
							int returnId2 = FMOD_IMPL_INVALID_INDEX;
							fmodResult = pSend->getParameterInt(FMOD_DSP_RETURN_ID, &returnId2, nullptr, 0);
							ASSERT_FMOD_OK;

							if (returnId1 == returnId2)
							{
								fmodResult = pSend->setParameterFloat(FMOD_DSP_SEND_LEVEL, value);
								ASSERT_FMOD_OK;
								break;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// Must exist at this point.
		CRY_ASSERT(false);
	}
}
