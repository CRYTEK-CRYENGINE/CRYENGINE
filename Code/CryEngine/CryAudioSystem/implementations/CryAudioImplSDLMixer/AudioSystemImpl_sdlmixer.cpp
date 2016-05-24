// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioSystemImpl_sdlmixer.h"
#include "AudioSystemImplCVars_sdlmixer.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include "SDLMixerSoundEngineUtil.h"
#include <CryAudio/IAudioSystem.h>
#include "SDLMixerSoundEngineTypes.h"

// SDL Mixer
#include <SDL_mixer.h>

namespace CryAudio
{
namespace Impl
{
char const* const CAudioSystemImpl_sdlmixer::s_szSDLFileTag = "SDLMixerSample";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLCommonAttribute = "sdl_name";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventIdTag = "event_id";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventTag = "SDLMixerEvent";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLSoundLibraryPath = CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer" CRY_NATIVE_PATH_SEPSTR;
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventTypeTag = "event_type";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventPanningEnabledTag = "enable_panning";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventAttenuationEnabledTag = "enable_distance_attenuation";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventAttenuationMinDistanceTag = "attenuation_dist_min";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventAttenuationMaxDistanceTag = "attenuation_dist_max";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventVolumeTag = "volume";
char const* const CAudioSystemImpl_sdlmixer::s_szSDLEventLoopCountTag = "loop_count";

void OnEventFinished(AudioEventId eventId)
{
	SAudioRequest request;
	SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportFinishedEvent> requestData(eventId, true);
	request.flags = eAudioRequestFlags_ThreadSafePush;
	request.pData = &requestData;
	gEnv->pAudioSystem->PushRequest(request);
}

void OnStandaloneFileFinished(AudioStandaloneFileId const filesInstanceId, const char* szFile)
{
	SAudioRequest request;
	SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStoppedFile> requestData(filesInstanceId, szFile);
	request.flags = eAudioRequestFlags_ThreadSafePush;
	request.pData = &requestData;
	gEnv->pAudioSystem->PushRequest(request);
}
CAudioSystemImpl_sdlmixer::CAudioSystemImpl_sdlmixer() : m_pCVarFileExtension(nullptr)
{
	m_gameFolder = PathUtil::GetGameFolder();
	if (m_gameFolder.empty())
	{
		CryFatalError("<Audio - SDLMixer>: Needs a valid game folder to proceed!");
	}

#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	m_fullImplString = "SDL Mixer 2.0.1 (";
	m_fullImplString += m_gameFolder + PathUtil::RemoveSlash(s_szSDLSoundLibraryPath) + ")";
#endif      // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE

#if CRY_PLATFORM_WINDOWS
	m_memoryAlignment = 16;
#elif CRY_PLATFORM_MAC
	m_memoryAlignment = 16;
#elif CRY_PLATFORM_LINUX
	m_memoryAlignment = 16;
#elif CRY_PLATFORM_IOS
	m_memoryAlignment = 16;
#elif CRY_PLATFORM_ANDROID
	m_memoryAlignment = 16;
#else
	#error "Undefined platform."
#endif
}

CAudioSystemImpl_sdlmixer::~CAudioSystemImpl_sdlmixer()
{}

void CAudioSystemImpl_sdlmixer::Update(float const deltaTime)
{
	SdlMixer::SoundEngine::Update();
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::Init()
{
	m_pCVarFileExtension = REGISTER_STRING("s_SDLMixerStandaloneFileExtension", ".mp3", 0, "the expected file extension for standalone files, played via the sdl_mixer");

	if (SdlMixer::SoundEngine::Init())
	{
		SdlMixer::SoundEngine::RegisterEventFinishedCallback(OnEventFinished);
		SdlMixer::SoundEngine::RegisterStandaloneFileFinishedCallback(OnStandaloneFileFinished);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::ShutDown()
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::Release()
{
	if (m_pCVarFileExtension)
	{
		m_pCVarFileExtension->Release();
		m_pCVarFileExtension = nullptr;
	}

	SdlMixer::SoundEngine::Release();
	POOL_FREE(this);

	// Freeing Memory Pool Memory again
	uint8* pMemSystem = g_audioImplMemoryPool_sdlmixer.Data();
	g_audioImplMemoryPool_sdlmixer.UnInitMem();

	if (pMemSystem)
	{
		delete[](uint8*)(pMemSystem);
	}

	g_audioImplCVars_sdlmixer.UnregisterVariables();
	return eAudioRequestStatus_Success;
}

void CAudioSystemImpl_sdlmixer::OnAudioSystemRefresh()
{
	SdlMixer::SoundEngine::Refresh();
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::OnLoseFocus()
{
	SdlMixer::SoundEngine::Pause();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::OnGetFocus()
{
	SdlMixer::SoundEngine::Resume();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::MuteAll()
{
	SdlMixer::SoundEngine::Mute();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::UnmuteAll()
{
	SdlMixer::SoundEngine::UnMute();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::StopAllSounds()
{
	SdlMixer::SoundEngine::Stop();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::RegisterAudioObject(IAudioObject* const pAudioObject, char const* const szAudioObjectName)
{
	SAtlAudioObjectData_sdlmixer* const pSdlMixerObject = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioObject);

	if (pSdlMixerObject)
	{
		SdlMixer::SoundEngine::RegisterAudioObject(pSdlMixerObject);
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
		m_idToName[pSdlMixerObject->audioObjectId] = szAudioObjectName;
#endif
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::RegisterAudioObject(IAudioObject* const pAudioObject)
{
	SAtlAudioObjectData_sdlmixer* const pSdlMixerObject = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioObject);
	if (pSdlMixerObject)
	{
		SdlMixer::SoundEngine::RegisterAudioObject(pSdlMixerObject);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::UnregisterAudioObject(IAudioObject* const pAudioObject)
{
	SAtlAudioObjectData_sdlmixer* const pSdlMixerObject = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioObject);
	if (pSdlMixerObject)
	{
		SdlMixer::SoundEngine::UnregisterAudioObject(pSdlMixerObject);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::ResetAudioObject(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::UpdateAudioObject(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::PlayFile(SAudioStandaloneFileInfo* const pAudioStandaloneFileInfo)
{
	bool bSuccess = false;

	SAtlAudioObjectData_sdlmixer* const pSDLAudioObjectData = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioStandaloneFileInfo->pAudioObject);

	if (pSDLAudioObjectData)
	{
		CAudioStandaloneFile_sdlmixer* const pPlayStandaloneEvent = static_cast<CAudioStandaloneFile_sdlmixer*>(pAudioStandaloneFileInfo->pImplData);
		pPlayStandaloneEvent->fileName = pAudioStandaloneFileInfo->szFileName;
		pPlayStandaloneEvent->fileId = pAudioStandaloneFileInfo->fileId;
		pPlayStandaloneEvent->fileInstanceId = pAudioStandaloneFileInfo->fileInstanceId;
		const SAtlTriggerImplData_sdlmixer* pUsedTrigger = static_cast<const SAtlTriggerImplData_sdlmixer*>(pAudioStandaloneFileInfo->pUsedAudioTrigger);
		static string s_localizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + m_language + CRY_NATIVE_PATH_SEPSTR;
		static string s_nonLocalizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR;
		static string filePath;

		if (pAudioStandaloneFileInfo->bLocalized)
		{
			filePath = s_localizedfilesFolder + pAudioStandaloneFileInfo->szFileName + m_pCVarFileExtension->GetString();
		}
		else
		{
			filePath = s_nonLocalizedfilesFolder + pAudioStandaloneFileInfo->szFileName + m_pCVarFileExtension->GetString();
		}

		bSuccess = SdlMixer::SoundEngine::PlayFile(pSDLAudioObjectData, pPlayStandaloneEvent, pUsedTrigger, filePath.c_str());
	}

	SAudioRequest request;
	SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStartedFile> requestData(pAudioStandaloneFileInfo->fileInstanceId, pAudioStandaloneFileInfo->szFileName, bSuccess);
	request.pData = &requestData;
	request.flags = eAudioRequestFlags_ThreadSafePush;
	gEnv->pAudioSystem->PushRequest(request);

	return (bSuccess) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::StopFile(SAudioStandaloneFileInfo* const pAudioStandaloneFileInfo)
{
	SAtlAudioObjectData_sdlmixer* const pSDLAudioObjectData = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioStandaloneFileInfo->pAudioObject);

	if (pSDLAudioObjectData)
	{
		if (SdlMixer::SoundEngine::StopFile(pSDLAudioObjectData, pAudioStandaloneFileInfo->fileInstanceId))
		{
			return eAudioRequestStatus_Pending;
		}
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::PrepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::UnprepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::PrepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::UnprepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::ActivateTrigger(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent)
{
	if ((pAudioObject != nullptr) && (pAudioTrigger != nullptr) && (pAudioEvent != nullptr))
	{
		SAtlAudioObjectData_sdlmixer* const pSDLAudioObjectData = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioObject);
		SAtlTriggerImplData_sdlmixer const* const pSDLEventStaticData = static_cast<SAtlTriggerImplData_sdlmixer const* const>(pAudioTrigger);
		SAtlEventData_sdlmixer* const pSDLEventInstanceData = static_cast<SAtlEventData_sdlmixer* const>(pAudioEvent);

		if (SdlMixer::SoundEngine::ExecuteEvent(pSDLAudioObjectData, pSDLEventStaticData, pSDLEventInstanceData))
		{
			return eAudioRequestStatus_Success;
		}
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::StopEvent(IAudioObject* const pAudioObject, IAudioEvent const* const pAudioEvent)
{
	SAtlEventData_sdlmixer const* const pSDLEventInstanceData = static_cast<SAtlEventData_sdlmixer const* const>(pAudioEvent);

	if (pSDLEventInstanceData != nullptr)
	{
		if (SdlMixer::SoundEngine::StopEvent(pSDLEventInstanceData))
		{
			return eAudioRequestStatus_Pending;
		}
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::StopAllEvents(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::Set3DAttributes(IAudioObject* const pAudioObject, SAudioObject3DAttributes const& attributes)
{
	SAtlAudioObjectData_sdlmixer* const pSdlMixerObject = static_cast<SAtlAudioObjectData_sdlmixer* const>(pAudioObject);
	if (pSdlMixerObject)
	{
		SdlMixer::SoundEngine::SetAudioObjectPosition(pSdlMixerObject, attributes.transformation);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::SetRtpc(IAudioObject* const pAudioObject, IAudioRtpc const* const pAudioRtpc, float const value)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::SetSwitchState(IAudioObject* const pAudioObject, IAudioSwitchState const* const pAudioSwitchState)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::SetObstructionOcclusion(IAudioObject* const pAudioObject, float const obstruction, float const occlusion)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::SetEnvironment(IAudioObject* const pAudioObject, IAudioEnvironment const* const pAudioEnvironment, float const amount)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::SetListener3DAttributes(IAudioListener* const pAudioListener, SAudioObject3DAttributes const& attributes)
{
	SAtlListenerData_sdlmixer* const pListener = static_cast<SAtlListenerData_sdlmixer* const>(pAudioListener);
	if (pListener)
	{
		SdlMixer::SoundEngine::SetListenerPosition(pListener->listenerId, attributes.transformation);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::RegisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	EAudioRequestStatus eResult = eAudioRequestStatus_Failure;

	if (pAudioFileEntry != nullptr)
	{
		SAtlAudioFileEntryData_sdlmixer* const pFileData = static_cast<SAtlAudioFileEntryData_sdlmixer*>(pAudioFileEntry->pImplData);

		if (pFileData != nullptr)
		{
			pFileData->sampleId = SdlMixer::SoundEngine::LoadSampleFromMemory(pAudioFileEntry->pFileData, pAudioFileEntry->size, pAudioFileEntry->szFileName);
			eResult = eAudioRequestStatus_Success;
		}
		else
		{
			g_audioImplLogger_sdlmixer.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the SDL Mixer implementation of RegisterInMemoryFile");
		}
	}
	return eResult;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::UnregisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	EAudioRequestStatus eResult = eAudioRequestStatus_Failure;

	if (pAudioFileEntry != nullptr)
	{
		SAtlAudioFileEntryData_sdlmixer* const pFileData = static_cast<SAtlAudioFileEntryData_sdlmixer*>(pAudioFileEntry->pImplData);

		if (pFileData != nullptr)
		{
			SdlMixer::SoundEngine::UnloadSample(pFileData->sampleId);
			eResult = eAudioRequestStatus_Success;
		}
		else
		{
			g_audioImplLogger_sdlmixer.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the SDL Mixer implementation of UnregisterInMemoryFile");
		}
	}
	return eResult;
}

EAudioRequestStatus CAudioSystemImpl_sdlmixer::ParseAudioFileEntry(XmlNodeRef const pAudioFileEntryNode, SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus eResult = eAudioRequestStatus_Failure;

	if ((_stricmp(pAudioFileEntryNode->getTag(), s_szSDLFileTag) == 0) && (pFileEntryInfo != nullptr))
	{
		char const* const szFileName = pAudioFileEntryNode->getAttr(s_szSDLCommonAttribute);

		// Currently the SDLMixer Implementation does not support localized files.
		pFileEntryInfo->bLocalized = false;

		if (szFileName != nullptr && szFileName[0] != '\0')
		{
			pFileEntryInfo->szFileName = szFileName;
			pFileEntryInfo->memoryBlockAlignment = m_memoryAlignment;
			POOL_NEW(SAtlAudioFileEntryData_sdlmixer, pFileEntryInfo->pImplData);
			eResult = eAudioRequestStatus_Success;
		}
		else
		{
			pFileEntryInfo->szFileName = nullptr;
			pFileEntryInfo->memoryBlockAlignment = 0;
			pFileEntryInfo->pImplData = nullptr;
		}
	}
	return eResult;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
	POOL_FREE(pOldAudioFileEntry);
}

char const* const CAudioSystemImpl_sdlmixer::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	static CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> s_path;
	s_path = m_gameFolder.c_str();
	s_path += s_szSDLSoundLibraryPath;

	return s_path.c_str();
}

IAudioTrigger const* CAudioSystemImpl_sdlmixer::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	SAtlTriggerImplData_sdlmixer* pNewTriggerImpl = nullptr;
	if (_stricmp(pAudioTriggerNode->getTag(), s_szSDLEventTag) == 0)
	{
		pNewTriggerImpl = SdlMixer::SoundEngine::CreateEventData();
		if (pNewTriggerImpl)
		{
			char const* const szFileName = pAudioTriggerNode->getAttr(s_szSDLCommonAttribute);
			pNewTriggerImpl->sampleId = SdlMixer::GetIDFromString(szFileName);

			pNewTriggerImpl->bStartEvent = (_stricmp(pAudioTriggerNode->getAttr(s_szSDLEventTypeTag), "stop") != 0);
			if (pNewTriggerImpl->bStartEvent)
			{
				pNewTriggerImpl->bPanningEnabled = (_stricmp(pAudioTriggerNode->getAttr(s_szSDLEventPanningEnabledTag), "true") == 0);
				bool bAttenuationEnabled = (_stricmp(pAudioTriggerNode->getAttr(s_szSDLEventAttenuationEnabledTag), "true") == 0);
				if (bAttenuationEnabled)
				{
					pAudioTriggerNode->getAttr(s_szSDLEventAttenuationMinDistanceTag, pNewTriggerImpl->attenuationMinDistance);
					pAudioTriggerNode->getAttr(s_szSDLEventAttenuationMaxDistanceTag, pNewTriggerImpl->attenuationMaxDistance);
				}
				else
				{
					pNewTriggerImpl->attenuationMinDistance = -1.0f;
					pNewTriggerImpl->attenuationMaxDistance = -1.0f;
				}

				info.maxRadius = pNewTriggerImpl->attenuationMaxDistance;

				// Translate decibel to normalized value.
				static const int maxVolume = 128;
				float volume = 0.0f;
				pAudioTriggerNode->getAttr(s_szSDLEventVolumeTag, volume);
				pNewTriggerImpl->volume = static_cast<int>(pow_tpl(10.0f, volume / 20.0f) * maxVolume);

				pAudioTriggerNode->getAttr(s_szSDLEventLoopCountTag, pNewTriggerImpl->loopCount);
			}
		}
	}
	return pNewTriggerImpl;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger)
{
	SAtlTriggerImplData_sdlmixer const* const pSDLEventStaticData = static_cast<SAtlTriggerImplData_sdlmixer const* const>(pOldAudioTrigger);
	if (pSDLEventStaticData)
	{
		SdlMixer::SoundEngine::StopTrigger(pSDLEventStaticData);
	}
	POOL_FREE_CONST(pOldAudioTrigger);
}

IAudioRtpc const* CAudioSystemImpl_sdlmixer::NewAudioRtpc(XmlNodeRef const pAudioRtpcNode)
{
	SAtlRtpcImplData_sdlmixer* pNewRtpcImpl = nullptr;
	POOL_NEW(SAtlRtpcImplData_sdlmixer, pNewRtpcImpl)();
	return pNewRtpcImpl;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioRtpc(IAudioRtpc const* const pOldAudioRtpc)
{
	POOL_FREE_CONST(pOldAudioRtpc);
}

IAudioSwitchState const* CAudioSystemImpl_sdlmixer::NewAudioSwitchState(XmlNodeRef const pAudioSwitchStateNode)
{
	SAtlSwitchStateImplData_sdlmixer* pNewSwitchImpl = nullptr;
	POOL_NEW(SAtlSwitchStateImplData_sdlmixer, pNewSwitchImpl)();
	return pNewSwitchImpl;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState)
{
	POOL_FREE_CONST(pOldAudioSwitchState);
}

IAudioEnvironment const* CAudioSystemImpl_sdlmixer::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	SAtlEnvironmentImplData_sdlmixer* pNewEnvironmentImpl = nullptr;
	POOL_NEW(SAtlEnvironmentImplData_sdlmixer, pNewEnvironmentImpl)();
	return pNewEnvironmentImpl;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment)
{
	POOL_FREE_CONST(pOldAudioEnvironment);
}

IAudioObject* CAudioSystemImpl_sdlmixer::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(SAtlAudioObjectData_sdlmixer, pNewObject)(audioObjectID, true);
	return pNewObject;
}

IAudioObject* CAudioSystemImpl_sdlmixer::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(SAtlAudioObjectData_sdlmixer, pNewObject)(audioObjectID, false);
	return pNewObject;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioObject(IAudioObject const* const pOldObjectData)
{
	POOL_FREE_CONST(pOldObjectData);
}

IAudioListener* CAudioSystemImpl_sdlmixer::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(SAtlListenerData_sdlmixer, pNewObject)(0);
	return pNewObject;
}

IAudioListener* CAudioSystemImpl_sdlmixer::NewAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(SAtlListenerData_sdlmixer, pNewObject)(audioObjectId);
	return pNewObject;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioListener(IAudioListener* const pOldAudioListener)
{
	POOL_FREE(pOldAudioListener);
}

IAudioEvent* CAudioSystemImpl_sdlmixer::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(SAtlEventData_sdlmixer, pNewEvent)(audioEventID);
	return pNewEvent;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent)
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	SAtlEventData_sdlmixer const* const pEventInstanceData = static_cast<SAtlEventData_sdlmixer const* const>(pOldAudioEvent);
	CRY_ASSERT_MESSAGE(pEventInstanceData->channels.size() == 0, "Events always have to be stopped/finished before they get deleted");
#endif
	POOL_FREE_CONST(pOldAudioEvent);
}

void CAudioSystemImpl_sdlmixer::ResetAudioEvent(IAudioEvent* const pAudioEvent)
{
	if (pAudioEvent != nullptr)
	{
		SAtlEventData_sdlmixer* const pEventInstanceData = static_cast<SAtlEventData_sdlmixer*>(pAudioEvent);
		pEventInstanceData->Reset();
	}
}

IAudioStandaloneFile* CAudioSystemImpl_sdlmixer::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(CAudioStandaloneFile_sdlmixer, pNewEvent)();
	return pNewEvent;
}

void CAudioSystemImpl_sdlmixer::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const pOldAudioStandaloneFile)
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	const CAudioStandaloneFile_sdlmixer* const pStandaloneEvent = static_cast<const CAudioStandaloneFile_sdlmixer*>(pOldAudioStandaloneFile);
	CRY_ASSERT_MESSAGE(pStandaloneEvent->channels.size() == 0, "Events always have to be stopped/finished before they get deleted");
#endif
	POOL_FREE_CONST(pOldAudioStandaloneFile);
}

void CAudioSystemImpl_sdlmixer::ResetAudioStandaloneFile(IAudioStandaloneFile* const pAudioStandaloneFile)
{
	if (pAudioStandaloneFile != nullptr)
	{
		CAudioStandaloneFile_sdlmixer* const pStandaloneEvent = static_cast<CAudioStandaloneFile_sdlmixer*>(pAudioStandaloneFile);
		pStandaloneEvent->Reset();
	}
}

void CAudioSystemImpl_sdlmixer::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{}

void CAudioSystemImpl_sdlmixer::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{}

void CAudioSystemImpl_sdlmixer::SetLanguage(char const* const szLanguage)
{
	m_language = szLanguage;
}

char const* const CAudioSystemImpl_sdlmixer::GetImplementationNameString() const
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	return m_fullImplString.c_str();
#endif      // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE
	return nullptr;
}

void CAudioSystemImpl_sdlmixer::GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const
{
	memoryInfo.primaryPoolSize = g_audioImplMemoryPool_sdlmixer.MemSize();
	memoryInfo.primaryPoolUsedSize = memoryInfo.primaryPoolSize - g_audioImplMemoryPool_sdlmixer.MemFree();
	memoryInfo.primaryPoolAllocations = g_audioImplMemoryPool_sdlmixer.FragmentCount();
	memoryInfo.bucketUsedSize = g_audioImplMemoryPool_sdlmixer.GetSmallAllocsSize();
	memoryInfo.bucketAllocations = g_audioImplMemoryPool_sdlmixer.GetSmallAllocsCount();
	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;
}

void CAudioSystemImpl_sdlmixer::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{}

} //CryAudio
} //Impl
