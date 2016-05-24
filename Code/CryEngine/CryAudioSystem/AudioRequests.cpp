// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioInternalInterfaces.h"

#define REQUEST_CASE_BLOCK(CLASS, ENUM, P_SOURCE, P_RESULT)                                         \
  case ENUM:                                                                                        \
    {                                                                                               \
      POOL_NEW(CLASS ## Internal<ENUM>, P_RESULT)(static_cast<CLASS<ENUM> const* const>(P_SOURCE)); \
                                                                                                    \
      break;                                                                                        \
    }

#define AM_REQUEST_BLOCK(ENUM)  REQUEST_CASE_BLOCK(SAudioManagerRequestData, ENUM, pExternalData, pResult)
#define ACM_REQUEST_BLOCK(ENUM) REQUEST_CASE_BLOCK(SAudioCallbackManagerRequestData, ENUM, pExternalData, pResult)
#define AO_REQUEST_BLOCK(ENUM)  REQUEST_CASE_BLOCK(SAudioObjectRequestData, ENUM, pExternalData, pResult)
#define AL_REQUEST_BLOCK(ENUM)  REQUEST_CASE_BLOCK(SAudioListenerRequestData, ENUM, pExternalData, pResult)

#define AM_REQUEST_BLOCK_INTERNAL(ENUM)                                                                                                                  \
  case ENUM:                                                                                                                                             \
    {                                                                                                                                                    \
      POOL_NEW(SAudioManagerRequestDataInternal<ENUM>, pResult)(static_cast<SAudioManagerRequestDataInternal<ENUM> const* const>(pRequestDataInternal)); \
                                                                                                                                                         \
      break;                                                                                                                                             \
    }

///////////////////////////////////////////////////////////////////////////
SAudioRequestDataInternal* ConvertToInternal(SAudioRequestDataBase const* const pExternalData)
{
	CRY_ASSERT(pExternalData != nullptr);
	SAudioRequestDataInternal* pResult = nullptr;
	EAudioRequestType const requestType = pExternalData->type;

	switch (requestType)
	{
	case eAudioRequestType_AudioManagerRequest:
		{
			SAudioManagerRequestDataBase const* const pBase = static_cast<SAudioManagerRequestDataBase const* const>(pExternalData);

			switch (pBase->type)
			{
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ReserveAudioObjectId)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_AddRequestListener)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_RemoveRequestListener)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ParseControlsData)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ParsePreloadsData)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ClearControlsData)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ClearPreloadsData)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_PreloadSingleRequest)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_UnloadSingleRequest)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_UnloadAFCMDataByScope)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_SetAudioImpl)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_RefreshAudioSystem)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_LoseFocus)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_GetFocus)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_MuteAll)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_UnmuteAll)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_StopAllSounds)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ChangeLanguage)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_RetriggerAudioControls)
				AM_REQUEST_BLOCK(eAudioManagerRequestType_ReloadControlsData)
			default:
				{
					g_audioLogger.Log(eAudioLogType_Error, "Unknown audio manager request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	case eAudioRequestType_AudioCallbackManagerRequest:
		{
			SAudioCallbackManagerRequestDataBase const* const pBase = static_cast<SAudioCallbackManagerRequestDataBase const* const>(pExternalData);

			switch (pBase->type)
			{
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportStartedEvent)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportFinishedEvent)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportStartedFile)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportStoppedFile)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportProcessedObstructionRay)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportVirtualizedEvent)
				ACM_REQUEST_BLOCK(eAudioCallbackManagerRequestType_ReportPhysicalizedEvent)
			default:
				{
					g_audioLogger.Log(eAudioLogType_Error, "Unknown audio callback manager request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	case eAudioRequestType_AudioObjectRequest:
		{
			SAudioObjectRequestDataBase const* const pBase = static_cast<SAudioObjectRequestDataBase const* const>(pExternalData);

			switch (pBase->type)
			{
				AO_REQUEST_BLOCK(eAudioObjectRequestType_PrepareTrigger)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_UnprepareTrigger)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_PlayFile)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_StopFile)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_ExecuteTrigger)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_StopTrigger)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_StopAllTriggers)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_SetTransformation)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_SetRtpcValue)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_SetSwitchState)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_SetVolume)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_SetEnvironmentAmount)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_ResetEnvironments)
				AO_REQUEST_BLOCK(eAudioObjectRequestType_ReleaseObject)
			default:
				{
					g_audioLogger.Log(eAudioLogType_Error, "Unknown audio object request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	case eAudioRequestType_AudioListenerRequest:
		{
			SAudioListenerRequestDataBase const* const pBase = static_cast<SAudioListenerRequestDataBase const* const>(pExternalData);

			switch (pBase->type)
			{
				AL_REQUEST_BLOCK(eAudioListenerRequestType_SetTransformation)
			default:
				{
					g_audioLogger.Log(eAudioLogType_Error, "Unknown audio listener request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	default:
		{
			g_audioLogger.Log(eAudioLogType_Error, "Unknown audio request type (%u)", requestType);
			CRY_ASSERT(false);

			break;
		}
	}

	return pResult;
}

////////////////////////////////////////////////////////////////////////////
SAudioRequestDataInternal* AllocateForInternal(SAudioRequestDataInternal const* const pRequestDataInternal)
{
	CRY_ASSERT(pRequestDataInternal);
	SAudioRequestDataInternal* pResult = nullptr;
	EAudioRequestType const requestType = pRequestDataInternal->type;

	switch (requestType)
	{
	case eAudioRequestType_AudioManagerRequest:
		{
			SAudioManagerRequestDataInternalBase const* const pBase = static_cast<SAudioManagerRequestDataInternalBase const* const>(pRequestDataInternal);

			switch (pBase->type)
			{
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_AddRequestListener)
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_RemoveRequestListener)
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_DrawDebugInfo)
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_ReleasePendingRays)
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_ReleaseAudioImpl)
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_UnloadAFCMDataByScope)
				AM_REQUEST_BLOCK_INTERNAL(eAudioManagerRequestType_GetAudioFileData)
			default:
				{
					g_audioLogger.Log(eAudioLogType_Error, "Unknown internal audio manager request type (%u)", pBase->type);
					CRY_ASSERT(false);

					break;
				}
			}

			break;
		}
	default:
		{
			g_audioLogger.Log(eAudioLogType_Error, "Unknown audio request type (%u)", requestType);
			CRY_ASSERT(false);

			break;
		}
	}

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void SAudioRequestDataInternal::Release()
{
	int const nCount = CryInterlockedDecrement(&m_nRefCounter);
	CRY_ASSERT(nCount >= 0);

	if (nCount == 0)
	{
		POOL_FREE(this);
	}
	else if (nCount < 0)
	{
		CryFatalError("Deleting Reference Counted Object Twice");
	}
}
