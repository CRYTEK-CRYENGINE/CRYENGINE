// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioInternalInterfaces.h"
#include "Common/Logger.h"

namespace CryAudio
{
#define REQUEST_CASE_BLOCK(CLASS, ENUM, P_SOURCE, P_RESULT)                        \
  case ENUM:                                                                       \
    {                                                                              \
      P_RESULT = new CLASS<ENUM>(static_cast<CLASS<ENUM> const* const>(P_SOURCE)); \
                                                                                   \
      break;                                                                       \
    }

#define AM_REQUEST_BLOCK(ENUM)  REQUEST_CASE_BLOCK(SAudioManagerRequestData, ENUM, pRequestData, pResult)
#define ACM_REQUEST_BLOCK(ENUM) REQUEST_CASE_BLOCK(SAudioCallbackManagerRequestData, ENUM, pRequestData, pResult)
#define AO_REQUEST_BLOCK(ENUM)  REQUEST_CASE_BLOCK(SAudioObjectRequestData, ENUM, pRequestData, pResult)
#define AL_REQUEST_BLOCK(ENUM)  REQUEST_CASE_BLOCK(SAudioListenerRequestData, ENUM, pRequestData, pResult)

////////////////////////////////////////////////////////////////////////////
SAudioRequestData* AllocateRequestData(SAudioRequestData const* const pRequestData)
{
	CRY_ASSERT(pRequestData != nullptr);
	SAudioRequestData* pResult = nullptr;
	EAudioRequestType const requestType = pRequestData->type;

	switch (requestType)
	{
	case EAudioRequestType::AudioManagerRequest:
		{
			SAudioManagerRequestDataBase const* const pBase = static_cast<SAudioManagerRequestDataBase const* const>(pRequestData);

			switch (pBase->type)
			{
				AM_REQUEST_BLOCK(EAudioManagerRequestType::SetAudioImpl)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ReleaseAudioImpl)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::RefreshAudioSystem)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::StopAllSounds)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ParseControlsData)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ParsePreloadsData)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ClearControlsData)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ClearPreloadsData)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::PreloadSingleRequest)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::UnloadSingleRequest)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::UnloadAFCMDataByScope)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::DrawDebugInfo)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::AddRequestListener)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::RemoveRequestListener)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ChangeLanguage)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::RetriggerAudioControls)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ReleasePendingRays)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::ReloadControlsData)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::GetAudioFileData)
				AM_REQUEST_BLOCK(EAudioManagerRequestType::GetImplInfo)
			default:
				{
					Cry::Audio::Log(ELogType::Error, "Unknown audio manager request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	case EAudioRequestType::AudioObjectRequest:
		{
			SAudioObjectRequestDataBase const* const pBase = static_cast<SAudioObjectRequestDataBase const* const>(pRequestData);

			switch (pBase->type)
			{
				AO_REQUEST_BLOCK(EAudioObjectRequestType::LoadTrigger)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::UnloadTrigger)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::PlayFile)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::StopFile)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::ExecuteTrigger)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::ExecuteTriggerEx)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::StopTrigger)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::StopAllTriggers)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::SetTransformation)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::SetParameter)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::SetSwitchState)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::SetCurrentEnvironments)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::SetEnvironment)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::ResetEnvironments)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::RegisterObject)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::ReleaseObject)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::ProcessPhysicsRay)
				AO_REQUEST_BLOCK(EAudioObjectRequestType::SetName)
			default:
				{
					Cry::Audio::Log(ELogType::Error, "Unknown audio object request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}
			break;
		}
	case EAudioRequestType::AudioListenerRequest:
		{
			SAudioListenerRequestDataBase const* const pBase = static_cast<SAudioListenerRequestDataBase const* const>(pRequestData);

			switch (pBase->type)
			{
				AL_REQUEST_BLOCK(EAudioListenerRequestType::SetTransformation)
				AL_REQUEST_BLOCK(EAudioListenerRequestType::RegisterListener)
				AL_REQUEST_BLOCK(EAudioListenerRequestType::ReleaseListener)
				AL_REQUEST_BLOCK(EAudioListenerRequestType::SetName)
			default:
				{
					Cry::Audio::Log(ELogType::Error, "Unknown audio listener request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	case EAudioRequestType::AudioCallbackManagerRequest:
		{
			SAudioCallbackManagerRequestDataBase const* const pBase = static_cast<SAudioCallbackManagerRequestDataBase const* const>(pRequestData);

			switch (pBase->type)
			{
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportStartedEvent)
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportFinishedEvent)
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportFinishedTriggerInstance)
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportStartedFile)
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportStoppedFile)
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportVirtualizedEvent)
				ACM_REQUEST_BLOCK(EAudioCallbackManagerRequestType::ReportPhysicalizedEvent)
			default:
				{
					Cry::Audio::Log(ELogType::Error, "Unknown audio callback manager request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	default:
		{
			Cry::Audio::Log(ELogType::Error, "Unknown audio request type (%u)", requestType);
			CRY_ASSERT(false);

			break;
		}
	}

	return pResult;
}
} // namespace CryAudio
