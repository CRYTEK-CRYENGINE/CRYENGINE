// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include "AudioImplCVars.h"
#include "ATLEntities.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
extern TriggerToParameterIndexes g_triggerToParameterIndexes;

//////////////////////////////////////////////////////////////////////////
CEvent::~CEvent()
{
	if (m_pInstance != nullptr)
	{
		FMOD_RESULT const fmodResult = m_pInstance->release();
		ASSERT_FMOD_OK_OR_INVALID_HANDLE;
	}

	if (m_pObject != nullptr)
	{
		m_pObject->RemoveEvent(this);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEvent::PrepareForOcclusion()
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
void CEvent::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	if (m_pOcclusionParameter != nullptr)
	{
		FMOD_RESULT const fmodResult = m_pOcclusionParameter->setValue(occlusion);
		ASSERT_FMOD_OK;
	}
	else if (m_pLowpass != nullptr)
	{
		float const range = m_lowpassFrequencyMax - std::max(m_lowpassFrequencyMin, g_cvars.m_lowpassMinCutoffFrequency);
		float const value = m_lowpassFrequencyMax - (occlusion * range);
		FMOD_RESULT const fmodResult = m_pLowpass->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, value);
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEvent::TrySetEnvironment(CEnvironment const* const pEnvironment, float const value)
{
	if (m_pInstance != nullptr && m_pMasterTrack != nullptr)
	{
		auto const type = pEnvironment->GetType();

		if (type == EEnvironmentType::Bus)
		{
			CEnvironmentBus const* const pEnvBus = static_cast<CEnvironmentBus const* const>(pEnvironment);

			FMOD::ChannelGroup* pChannelGroup = nullptr;
			FMOD_RESULT fmodResult = pEnvBus->GetBus()->getChannelGroup(&pChannelGroup);
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
		else if (type == EEnvironmentType::Parameter)
		{
			CEnvironmentParameter const* const pEnvParam = static_cast<CEnvironmentParameter const* const>(pEnvironment);
			uint32 const parameterId = pEnvParam->GetId();
			FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

			FMOD::Studio::EventInstance* const pEventInstance = GetInstance();
			CRY_ASSERT_MESSAGE(pEventInstance != nullptr, "Event instance doesn't exist.");
			CTrigger const* const pTrigger = GetTrigger();
			CRY_ASSERT_MESSAGE(pTrigger != nullptr, "Trigger doesn't exist.");

			FMOD::Studio::EventDescription* pEventDescription = nullptr;
			fmodResult = pEventInstance->getDescription(&pEventDescription);
			ASSERT_FMOD_OK;

			if (g_triggerToParameterIndexes.find(pTrigger) != g_triggerToParameterIndexes.end())
			{
				ParameterIdToIndex& parameters = g_triggerToParameterIndexes[pTrigger];

				if (parameters.find(parameterId) != parameters.end())
				{
					fmodResult = pEventInstance->setParameterValueByIndex(parameters[parameterId], pEnvParam->GetValueMultiplier() * value + pEnvParam->GetValueShift());
					ASSERT_FMOD_OK;
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pEventInstance->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
						ASSERT_FMOD_OK;

						if (parameterId == StringToId(parameterDescription.name))
						{
							parameters.emplace(parameterId, index);
							fmodResult = pEventInstance->setParameterValueByIndex(index, pEnvParam->GetValueMultiplier() * value + pEnvParam->GetValueShift());
							ASSERT_FMOD_OK;
							break;
						}
					}
				}
			}
			else
			{
				int parameterCount = 0;
				fmodResult = pEventInstance->getParameterCount(&parameterCount);
				ASSERT_FMOD_OK;

				for (int index = 0; index < parameterCount; ++index)
				{
					FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
					fmodResult = pEventDescription->getParameterByIndex(index, &parameterDescription);
					ASSERT_FMOD_OK;

					if (parameterId == StringToId(parameterDescription.name))
					{
						g_triggerToParameterIndexes[pTrigger].emplace(std::make_pair(parameterId, index));
						fmodResult = pEventInstance->setParameterValueByIndex(index, pEnvParam->GetValueMultiplier() * value + pEnvParam->GetValueShift());
						ASSERT_FMOD_OK;
						break;
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

//////////////////////////////////////////////////////////////////////////
ERequestStatus CEvent::Stop()
{
	FMOD_RESULT const fmodResult = m_pInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
	ASSERT_FMOD_OK;
	return ERequestStatus::Success;
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
