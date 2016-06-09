// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioImplCVars.h"
#include "SoundEngineUtil.h"
#include "SoundEngineTypes.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include <CryAudio/IAudioSystem.h>

// SDL Mixer
#include <SDL_mixer.h>

using namespace CryAudio::Impl;
using namespace CryAudio::Impl::SDL_mixer;

char const* const CAudioImpl::s_szSDLFileTag = "SDLMixerSample";
char const* const CAudioImpl::s_szSDLCommonAttribute = "sdl_name";
char const* const CAudioImpl::s_szSDLEventIdTag = "event_id";
char const* const CAudioImpl::s_szSDLEventTag = "SDLMixerEvent";
char const* const CAudioImpl::s_szSDLSoundLibraryPath = CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer" CRY_NATIVE_PATH_SEPSTR;
char const* const CAudioImpl::s_szSDLEventTypeTag = "event_type";
char const* const CAudioImpl::s_szSDLEventPanningEnabledTag = "enable_panning";
char const* const CAudioImpl::s_szSDLEventAttenuationEnabledTag = "enable_distance_attenuation";
char const* const CAudioImpl::s_szSDLEventAttenuationMinDistanceTag = "attenuation_dist_min";
char const* const CAudioImpl::s_szSDLEventAttenuationMaxDistanceTag = "attenuation_dist_max";
char const* const CAudioImpl::s_szSDLEventVolumeTag = "volume";
char const* const CAudioImpl::s_szSDLEventLoopCountTag = "loop_count";

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

CAudioImpl::CAudioImpl()
	: m_pCVarFileExtension(nullptr)
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

CAudioImpl::~CAudioImpl()
{}

void CAudioImpl::Update(float const deltaTime)
{
	SoundEngine::Update();
}

EAudioRequestStatus CAudioImpl::Init()
{
	m_pCVarFileExtension = REGISTER_STRING("s_SDLMixerStandaloneFileExtension", ".mp3", 0, "the expected file extension for standalone files, played via the sdl_mixer");

	if (SoundEngine::Init())
	{
		SoundEngine::RegisterEventFinishedCallback(OnEventFinished);
		SoundEngine::RegisterStandaloneFileFinishedCallback(OnStandaloneFileFinished);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::ShutDown()
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::Release()
{
	if (m_pCVarFileExtension)
	{
		m_pCVarFileExtension->Release();
		m_pCVarFileExtension = nullptr;
	}

	SoundEngine::Release();
	POOL_FREE(this);

	// Freeing Memory Pool Memory again
	uint8* pMemSystem = g_audioImplMemoryPool.Data();
	g_audioImplMemoryPool.UnInitMem();

	if (pMemSystem)
	{
		delete[](uint8*)(pMemSystem);
	}

	g_audioImplCVars.UnregisterVariables();
	return eAudioRequestStatus_Success;
}

void CAudioImpl::OnAudioSystemRefresh()
{
	SoundEngine::Refresh();
}

EAudioRequestStatus CAudioImpl::OnLoseFocus()
{
	SoundEngine::Pause();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::OnGetFocus()
{
	SoundEngine::Resume();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::MuteAll()
{
	SoundEngine::Mute();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::UnmuteAll()
{
	SoundEngine::UnMute();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::StopAllSounds()
{
	SoundEngine::Stop();
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::RegisterAudioObject(IAudioObject* const pAudioObject, char const* const szAudioObjectName)
{
	SAudioObject* const pSdlMixerObject = static_cast<SAudioObject* const>(pAudioObject);

	if (pSdlMixerObject)
	{
		SoundEngine::RegisterAudioObject(pSdlMixerObject);
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
		m_idToName[pSdlMixerObject->audioObjectId] = szAudioObjectName;
#endif
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::RegisterAudioObject(IAudioObject* const pAudioObject)
{
	SAudioObject* const pSdlMixerObject = static_cast<SAudioObject* const>(pAudioObject);
	if (pSdlMixerObject)
	{
		SoundEngine::RegisterAudioObject(pSdlMixerObject);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::UnregisterAudioObject(IAudioObject* const pAudioObject)
{
	SAudioObject* const pSdlMixerObject = static_cast<SAudioObject* const>(pAudioObject);
	if (pSdlMixerObject)
	{
		SoundEngine::UnregisterAudioObject(pSdlMixerObject);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::ResetAudioObject(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::UpdateAudioObject(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::PlayFile(SAudioStandaloneFileInfo* const pAudioStandaloneFileInfo)
{
	bool bSuccess = false;

	SAudioObject* const pSDLAudioObjectData = static_cast<SAudioObject* const>(pAudioStandaloneFileInfo->pAudioObject);

	if (pSDLAudioObjectData)
	{
		CAudioStandaloneFile* const pPlayStandaloneEvent = static_cast<CAudioStandaloneFile*>(pAudioStandaloneFileInfo->pImplData);
		pPlayStandaloneEvent->fileName = pAudioStandaloneFileInfo->szFileName;
		pPlayStandaloneEvent->fileId = pAudioStandaloneFileInfo->fileId;
		pPlayStandaloneEvent->fileInstanceId = pAudioStandaloneFileInfo->fileInstanceId;
		const SAudioTrigger* pUsedTrigger = static_cast<const SAudioTrigger*>(pAudioStandaloneFileInfo->pUsedAudioTrigger);
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

		bSuccess = SoundEngine::PlayFile(pSDLAudioObjectData, pPlayStandaloneEvent, pUsedTrigger, filePath.c_str());
	}

	SAudioRequest request;
	SAudioCallbackManagerRequestData<eAudioCallbackManagerRequestType_ReportStartedFile> requestData(pAudioStandaloneFileInfo->fileInstanceId, pAudioStandaloneFileInfo->szFileName, bSuccess);
	request.pData = &requestData;
	request.flags = eAudioRequestFlags_ThreadSafePush;
	gEnv->pAudioSystem->PushRequest(request);

	return (bSuccess) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::StopFile(SAudioStandaloneFileInfo* const pAudioStandaloneFileInfo)
{
	SAudioObject* const pSDLAudioObjectData = static_cast<SAudioObject* const>(pAudioStandaloneFileInfo->pAudioObject);

	if (pSDLAudioObjectData)
	{
		if (SoundEngine::StopFile(pSDLAudioObjectData, pAudioStandaloneFileInfo->fileInstanceId))
		{
			return eAudioRequestStatus_Pending;
		}
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::PrepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::UnprepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::PrepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::UnprepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::ActivateTrigger(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent)
{
	if ((pAudioObject != nullptr) && (pAudioTrigger != nullptr) && (pAudioEvent != nullptr))
	{
		SAudioObject* const pSDLAudioObjectData = static_cast<SAudioObject* const>(pAudioObject);
		SAudioTrigger const* const pSDLEventStaticData = static_cast<SAudioTrigger const* const>(pAudioTrigger);
		SAudioEvent* const pSDLEventInstanceData = static_cast<SAudioEvent* const>(pAudioEvent);

		if (SoundEngine::ExecuteEvent(pSDLAudioObjectData, pSDLEventStaticData, pSDLEventInstanceData))
		{
			return eAudioRequestStatus_Success;
		}
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::StopEvent(IAudioObject* const pAudioObject, IAudioEvent const* const pAudioEvent)
{
	SAudioEvent const* const pSDLEventInstanceData = static_cast<SAudioEvent const* const>(pAudioEvent);

	if (pSDLEventInstanceData != nullptr)
	{
		if (SoundEngine::StopEvent(pSDLEventInstanceData))
		{
			return eAudioRequestStatus_Pending;
		}
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::StopAllEvents(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::Set3DAttributes(IAudioObject* const pAudioObject, SAudioObject3DAttributes const& attributes)
{
	SAudioObject* const pSdlMixerObject = static_cast<SAudioObject* const>(pAudioObject);
	if (pSdlMixerObject)
	{
		SoundEngine::SetAudioObjectPosition(pSdlMixerObject, attributes.transformation);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::SetRtpc(IAudioObject* const pAudioObject, IAudioRtpc const* const pAudioRtpc, float const value)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::SetSwitchState(IAudioObject* const pAudioObject, IAudioSwitchState const* const pAudioSwitchState)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::SetObstructionOcclusion(IAudioObject* const pAudioObject, float const obstruction, float const occlusion)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::SetEnvironment(IAudioObject* const pAudioObject, IAudioEnvironment const* const pAudioEnvironment, float const amount)
{
	return eAudioRequestStatus_Success;
}

EAudioRequestStatus CAudioImpl::SetListener3DAttributes(IAudioListener* const pAudioListener, SAudioObject3DAttributes const& attributes)
{
	SAudioListener* const pListener = static_cast<SAudioListener* const>(pAudioListener);
	if (pListener)
	{
		SoundEngine::SetListenerPosition(pListener->listenerId, attributes.transformation);
		return eAudioRequestStatus_Success;
	}
	return eAudioRequestStatus_Failure;
}

EAudioRequestStatus CAudioImpl::RegisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	EAudioRequestStatus eResult = eAudioRequestStatus_Failure;

	if (pAudioFileEntry != nullptr)
	{
		SAudioFileEntry* const pFileData = static_cast<SAudioFileEntry*>(pAudioFileEntry->pImplData);

		if (pFileData != nullptr)
		{
			pFileData->sampleId = SoundEngine::LoadSampleFromMemory(pAudioFileEntry->pFileData, pAudioFileEntry->size, pAudioFileEntry->szFileName);
			eResult = eAudioRequestStatus_Success;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the SDL Mixer implementation of RegisterInMemoryFile");
		}
	}
	return eResult;
}

EAudioRequestStatus CAudioImpl::UnregisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry)
{
	EAudioRequestStatus eResult = eAudioRequestStatus_Failure;

	if (pAudioFileEntry != nullptr)
	{
		SAudioFileEntry* const pFileData = static_cast<SAudioFileEntry*>(pAudioFileEntry->pImplData);

		if (pFileData != nullptr)
		{
			SoundEngine::UnloadSample(pFileData->sampleId);
			eResult = eAudioRequestStatus_Success;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the SDL Mixer implementation of UnregisterInMemoryFile");
		}
	}
	return eResult;
}

EAudioRequestStatus CAudioImpl::ParseAudioFileEntry(XmlNodeRef const pAudioFileEntryNode, SAudioFileEntryInfo* const pFileEntryInfo)
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
			POOL_NEW(SAudioFileEntry, pFileEntryInfo->pImplData);
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

void CAudioImpl::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
	POOL_FREE(pOldAudioFileEntry);
}

char const* const CAudioImpl::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	static CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> s_path;
	s_path = m_gameFolder.c_str();
	s_path += s_szSDLSoundLibraryPath;

	return s_path.c_str();
}

IAudioTrigger const* CAudioImpl::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	SAudioTrigger* pNewTriggerImpl = nullptr;
	if (_stricmp(pAudioTriggerNode->getTag(), s_szSDLEventTag) == 0)
	{
		pNewTriggerImpl = SoundEngine::CreateEventData();
		if (pNewTriggerImpl)
		{
			char const* const szFileName = pAudioTriggerNode->getAttr(s_szSDLCommonAttribute);
			pNewTriggerImpl->sampleId = GetIDFromString(szFileName);

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

void CAudioImpl::DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger)
{
	SAudioTrigger const* const pSDLEventStaticData = static_cast<SAudioTrigger const* const>(pOldAudioTrigger);
	if (pSDLEventStaticData)
	{
		SoundEngine::StopTrigger(pSDLEventStaticData);
	}
	POOL_FREE_CONST(pOldAudioTrigger);
}

IAudioRtpc const* CAudioImpl::NewAudioRtpc(XmlNodeRef const pAudioRtpcNode)
{
	SAudioParameter* pNewRtpcImpl = nullptr;
	POOL_NEW(SAudioParameter, pNewRtpcImpl)();
	return pNewRtpcImpl;
}

void CAudioImpl::DeleteAudioRtpc(IAudioRtpc const* const pOldAudioRtpc)
{
	POOL_FREE_CONST(pOldAudioRtpc);
}

IAudioSwitchState const* CAudioImpl::NewAudioSwitchState(XmlNodeRef const pAudioSwitchStateNode)
{
	SAudioSwitchState* pNewSwitchImpl = nullptr;
	POOL_NEW(SAudioSwitchState, pNewSwitchImpl)();
	return pNewSwitchImpl;
}

void CAudioImpl::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState)
{
	POOL_FREE_CONST(pOldAudioSwitchState);
}

IAudioEnvironment const* CAudioImpl::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	SAudioEnvironment* pNewEnvironmentImpl = nullptr;
	POOL_NEW(SAudioEnvironment, pNewEnvironmentImpl)();
	return pNewEnvironmentImpl;
}

void CAudioImpl::DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment)
{
	POOL_FREE_CONST(pOldAudioEnvironment);
}

IAudioObject* CAudioImpl::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(SAudioObject, pNewObject)(audioObjectID, true);
	return pNewObject;
}

IAudioObject* CAudioImpl::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(SAudioObject, pNewObject)(audioObjectID, false);
	return pNewObject;
}

void CAudioImpl::DeleteAudioObject(IAudioObject const* const pOldObjectData)
{
	POOL_FREE_CONST(pOldObjectData);
}

IAudioListener* CAudioImpl::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(SAudioListener, pNewObject)(0);
	return pNewObject;
}

IAudioListener* CAudioImpl::NewAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(SAudioListener, pNewObject)(audioObjectId);
	return pNewObject;
}

void CAudioImpl::DeleteAudioListener(IAudioListener* const pOldAudioListener)
{
	POOL_FREE(pOldAudioListener);
}

IAudioEvent* CAudioImpl::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(SAudioEvent, pNewEvent)(audioEventID);
	return pNewEvent;
}

void CAudioImpl::DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent)
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	SAudioEvent const* const pEventInstanceData = static_cast<SAudioEvent const* const>(pOldAudioEvent);
	CRY_ASSERT_MESSAGE(pEventInstanceData->channels.size() == 0, "Events always have to be stopped/finished before they get deleted");
#endif
	POOL_FREE_CONST(pOldAudioEvent);
}

void CAudioImpl::ResetAudioEvent(IAudioEvent* const pAudioEvent)
{
	if (pAudioEvent != nullptr)
	{
		SAudioEvent* const pEventInstanceData = static_cast<SAudioEvent*>(pAudioEvent);
		pEventInstanceData->Reset();
	}
}

IAudioStandaloneFile* CAudioImpl::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(CAudioStandaloneFile, pNewEvent)();
	return pNewEvent;
}

void CAudioImpl::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const pOldAudioStandaloneFile)
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	const CAudioStandaloneFile* const pStandaloneEvent = static_cast<const CAudioStandaloneFile*>(pOldAudioStandaloneFile);
	CRY_ASSERT_MESSAGE(pStandaloneEvent->channels.size() == 0, "Events always have to be stopped/finished before they get deleted");
#endif
	POOL_FREE_CONST(pOldAudioStandaloneFile);
}

void CAudioImpl::ResetAudioStandaloneFile(IAudioStandaloneFile* const pAudioStandaloneFile)
{
	if (pAudioStandaloneFile != nullptr)
	{
		CAudioStandaloneFile* const pStandaloneEvent = static_cast<CAudioStandaloneFile*>(pAudioStandaloneFile);
		pStandaloneEvent->Reset();
	}
}

void CAudioImpl::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{}

void CAudioImpl::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{}

void CAudioImpl::SetLanguage(char const* const szLanguage)
{
	m_language = szLanguage;
}

char const* const CAudioImpl::GetImplementationNameString() const
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	return m_fullImplString.c_str();
#endif      // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE
	return nullptr;
}

void CAudioImpl::GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const
{
	memoryInfo.primaryPoolSize = g_audioImplMemoryPool.MemSize();
	memoryInfo.primaryPoolUsedSize = memoryInfo.primaryPoolSize - g_audioImplMemoryPool.MemFree();
	memoryInfo.primaryPoolAllocations = g_audioImplMemoryPool.FragmentCount();
	memoryInfo.bucketUsedSize = g_audioImplMemoryPool.GetSmallAllocsSize();
	memoryInfo.bucketAllocations = g_audioImplMemoryPool.GetSmallAllocsCount();
	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;
}

void CAudioImpl::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{}
