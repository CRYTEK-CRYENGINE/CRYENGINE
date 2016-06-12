// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioImpl.h"
#include "Common.h"
#include "AudioTrigger.h"
#include "AudioEvent.h"
#include "AudioObject.h"
#include "AudioImplCVars.h"
#include "ATLEntities.h"
#include <sndfile.hh>
#include <CrySystem/File/ICryPak.h>
#include <CryAudio/IAudioSystem.h>

using namespace CryAudio::Impl;
using namespace CryAudio::Impl::PortAudio;

char const* const CAudioImpl::s_szPortAudioEventTag = "PortAudioEvent";
char const* const CAudioImpl::s_szPortAudioEventNameAttribute = "portaudio_name";
char const* const CAudioImpl::s_szPortAudioEventTypeAttribute = "event_type";
char const* const CAudioImpl::s_szPortAudioEventNumLoopsAttribute = "num_loops";

///////////////////////////////////////////////////////////////////////////
CAudioImpl::CAudioImpl()
{
}

///////////////////////////////////////////////////////////////////////////
CAudioImpl::~CAudioImpl()
{
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::Update(float const deltaTime)
{
	for (auto pAudioObject : m_registeredAudioObjects)
	{
		pAudioObject->Update();
	}
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Init()
{
	string const gameFolder = PathUtil::GetGameFolder().c_str();

	if (gameFolder.empty())
	{
		CryFatalError("<Audio>: Needs a valid game folder to proceed!");
	}

	m_regularSoundBankFolder = gameFolder.c_str();
	m_regularSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
	m_regularSoundBankFolder += PORTAUDIO_IMPL_DATA_ROOT;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	PaError const err = Pa_Initialize();

	if (err != paNoError)
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
	}

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	m_fullImplString = Pa_GetVersionText();
	m_fullImplString += " (";
	m_fullImplString += gameFolder + CRY_NATIVE_PATH_SEPSTR PORTAUDIO_IMPL_DATA_ROOT ")";
#endif // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ShutDown()
{
	PaError const err = Pa_Terminate();

	if (err != paNoError)
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Failed to shut down PortAudio: %s", Pa_GetErrorText(err));
	}

	return (err == paNoError) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Release()
{
	POOL_FREE(this);

	// Freeing Memory Pool Memory again
	uint8* pMemSystem = g_audioImplMemoryPool.Data();
	g_audioImplMemoryPool.UnInitMem();

	if (pMemSystem)
	{
		delete[] (uint8*)(pMemSystem);
	}

	g_audioImplCVars.UnregisterVariables();

	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::OnLoseFocus()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::OnGetFocus()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::MuteAll()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnmuteAll()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopAllSounds()
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterAudioObject(IAudioObject* const pAudioObject)
{
	stl::push_back_unique(m_registeredAudioObjects, static_cast<CAudioObject*>(pAudioObject));
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterAudioObject(
  IAudioObject* const pAudioObject,
  char const* const szAudioObjectName)
{
	stl::push_back_unique(m_registeredAudioObjects, static_cast<CAudioObject*>(pAudioObject));
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnregisterAudioObject(IAudioObject* const pAudioObject)
{
	stl::find_and_erase(m_registeredAudioObjects, static_cast<CAudioObject*>(pAudioObject));
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ResetAudioObject(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UpdateAudioObject(IAudioObject* const pAudioObject)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PlayFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PrepareTriggerSync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnprepareTriggerSync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::PrepareTriggerAsync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnprepareTriggerAsync(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	return eAudioRequestStatus_Success;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ActivateTrigger(
  IAudioObject* const pAudioObject,
  IAudioTrigger const* const pAudioTrigger,
  IAudioEvent* const pAudioEvent)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioTrigger const* const pPAAudioTrigger = static_cast<CAudioTrigger const* const>(pAudioTrigger);
	CAudioEvent* const pPAAudioEvent = static_cast<CAudioEvent*>(pAudioEvent);

	if ((pPAAudioObject != nullptr) && (pPAAudioTrigger != nullptr) && (pPAAudioEvent != nullptr))
	{
		if (pPAAudioTrigger->eventType == ePortAudioEventType_Start)
		{
			requestResult = pPAAudioEvent->Execute(
			  pPAAudioTrigger->numLoops,
			  pPAAudioTrigger->sampleRate,
			  pPAAudioTrigger->filePath,
			  pPAAudioTrigger->streamParameters) ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;

			if (requestResult == eAudioRequestStatus_Success)
			{
				pPAAudioEvent->pPAAudioObject = pPAAudioObject;
				pPAAudioEvent->pathId = pPAAudioTrigger->pathId;
				pPAAudioObject->RegisterAudioEvent(pPAAudioEvent);
			}
		}
		else
		{
			pPAAudioObject->StopAudioEvent(pPAAudioTrigger->pathId);

			// Return failure here so the ATL does not keep track of this event.
			requestResult = eAudioRequestStatus_Failure;
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData, ATLTriggerData or EventData passed to the PortAudio implementation of ActivateTrigger.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopEvent(
  IAudioObject* const pAudioObject,
  IAudioEvent const* const pAudioEvent)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioEvent const* const pPAAudioEvent = static_cast<CAudioEvent const* const>(pAudioEvent);

	if (pPAAudioObject != nullptr && pPAAudioEvent != nullptr)
	{
		pPAAudioObject->StopAudioEvent(pPAAudioEvent->pathId);
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid EventData passed to the PortAudio implementation of StopEvent.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::StopAllEvents(IAudioObject* const pAudioObject)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (pPAAudioObject != nullptr)
	{
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the PortAudio implementation of StopAllEvents.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::Set3DAttributes(
  IAudioObject* const pAudioObject,
	CryAudio::Impl::SAudioObject3DAttributes const& attributes)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (pPAAudioObject != nullptr)
	{
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the PortAudio implementation of SetPosition.");
	}

	return requestResult;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetEnvironment(
  IAudioObject* const pAudioObject,
  IAudioEnvironment const* const pAudioEnvironment,
  float const amount)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioEnvironment const* const pPAAudioEnvironment = static_cast<CAudioEnvironment const* const>(pAudioEnvironment);

	if ((pPAAudioObject != nullptr) && (pPAAudioEnvironment != nullptr))
	{
		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData or EnvironmentData passed to the PortAudio implementation of SetEnvironment");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetRtpc(
  IAudioObject* const pAudioObject,
  IAudioRtpc const* const pAudioRtpc,
  float const value)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioParameter const* const pPAAudioParameter = static_cast<CAudioParameter const* const>(pAudioRtpc);

	if ((pPAAudioObject != nullptr) && (pPAAudioParameter != nullptr))
	{
		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData or RtpcData passed to the PortAudio implementation of SetRtpc");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetSwitchState(
  IAudioObject* const pAudioObject,
  IAudioSwitchState const* const pAudioSwitchState)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);
	CAudioSwitchState const* const pPAAudioSwitchState = static_cast<CAudioSwitchState const* const>(pAudioSwitchState);

	if ((pPAAudioObject != nullptr) && (pPAAudioSwitchState != nullptr))
	{
		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData or RtpcData passed to the PortAudio implementation of SetSwitchState");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetObstructionOcclusion(
  IAudioObject* const pAudioObject,
  float const obstruction,
  float const occlusion)
{
	EAudioRequestStatus result = eAudioRequestStatus_Failure;
	CAudioObject* const pPAAudioObject = static_cast<CAudioObject* const>(pAudioObject);

	if (pPAAudioObject != nullptr)
	{
		result = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioObjectData passed to the PortAudio implementation of SetObstructionOcclusion");
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::SetListener3DAttributes(
  IAudioListener* const pAudioListener,
	CryAudio::Impl::SAudioObject3DAttributes const& attributes)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;
	CAudioListener* const pPAAudioListener = static_cast<CAudioListener* const>(pAudioListener);

	if (pPAAudioListener != nullptr)
	{
		requestResult = eAudioRequestStatus_Success;
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Invalid ATLListenerData passed to the PortAudio implementation of SetListenerPosition");
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::RegisterInMemoryFile(SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;

	if (pFileEntryInfo != nullptr)
	{
		CAudioFileEntry* const pPAAudioFileEntry = static_cast<CAudioFileEntry*>(pFileEntryInfo->pImplData);

		if (pPAAudioFileEntry != nullptr)
		{
			requestResult = eAudioRequestStatus_Success;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the PortAudio implementation of RegisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::UnregisterInMemoryFile(SAudioFileEntryInfo* const pFileEntryInfo)
{
	EAudioRequestStatus requestResult = eAudioRequestStatus_Failure;

	if (pFileEntryInfo != nullptr)
	{
		CAudioFileEntry* const pPAAudioFileEntry = static_cast<CAudioFileEntry*>(pFileEntryInfo->pImplData);

		if (pPAAudioFileEntry != nullptr)
		{
			requestResult = eAudioRequestStatus_Success;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Invalid AudioFileEntryData passed to the PortAudio implementation of UnregisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CAudioImpl::ParseAudioFileEntry(
  XmlNodeRef const pAudioFileEntryNode,
  SAudioFileEntryInfo* const pFileEntryInfo)
{
	return eAudioRequestStatus_Failure;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry)
{
	POOL_FREE(pOldAudioFileEntry);
}

//////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl::GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo)
{
	char const* sResult = nullptr;

	if (pFileEntryInfo != nullptr)
	{
		sResult = pFileEntryInfo->bLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str();
	}

	return sResult;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl::NewGlobalAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(CAudioObject, pPAAudioObject);
	return pPAAudioObject;
}

///////////////////////////////////////////////////////////////////////////
IAudioObject* CAudioImpl::NewAudioObject(AudioObjectId const audioObjectID)
{
	POOL_NEW_CREATE(CAudioObject, pPAAudioObject);
	return pPAAudioObject;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioObject(IAudioObject const* const pOldAudioObject)
{
	POOL_FREE_CONST(pOldAudioObject);
}

///////////////////////////////////////////////////////////////////////////
CryAudio::Impl::IAudioListener* CAudioImpl::NewDefaultAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(CAudioListener, pAudioListener);
	return pAudioListener;
}

///////////////////////////////////////////////////////////////////////////
CryAudio::Impl::IAudioListener* CAudioImpl::NewAudioListener(AudioObjectId const audioObjectId)
{
	POOL_NEW_CREATE(CAudioListener, pAudioListener);
	return pAudioListener;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioListener(CryAudio::Impl::IAudioListener* const pOldAudioListener)
{
	POOL_FREE(pOldAudioListener);
}

//////////////////////////////////////////////////////////////////////////
IAudioEvent* CAudioImpl::NewAudioEvent(AudioEventId const audioEventID)
{
	POOL_NEW_CREATE(CAudioEvent, pPAAudioEvent)(audioEventID);
	return pPAAudioEvent;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent)
{
	POOL_FREE_CONST(pOldAudioEvent);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::ResetAudioEvent(IAudioEvent* const pAudioEvent)
{
	CAudioEvent* const pPAAudioEvent = static_cast<CAudioEvent*>(pAudioEvent);

	if (pPAAudioEvent != nullptr)
	{
		pPAAudioEvent->Reset();
		pPAAudioEvent->pPAAudioObject->UnregisterAudioEvent(pPAAudioEvent);
		pPAAudioEvent->pPAAudioObject = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
IAudioStandaloneFile* CAudioImpl::NewAudioStandaloneFile()
{
	POOL_NEW_CREATE(CAudioStandaloneFile, pAudioStandaloneFile);
	return pAudioStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioStandaloneFile(IAudioStandaloneFile const* const _pOldAudioStandaloneFile)
{
	POOL_FREE_CONST(_pOldAudioStandaloneFile);
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::ResetAudioStandaloneFile(IAudioStandaloneFile* const _pAudioStandaloneFile)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
IAudioTrigger const* CAudioImpl::NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info)
{
	CAudioTrigger* pAudioTrigger = nullptr;
	char const* const szTag = pAudioTriggerNode->getTag();

	if (_stricmp(szTag, s_szPortAudioEventTag) == 0)
	{
		stack_string path = "GameSDK/audio/portaudio/";
		path += pAudioTriggerNode->getAttr(s_szPortAudioEventNameAttribute);

		if (!path.empty())
		{
			SF_INFO sfInfo;
			ZeroStruct(sfInfo);

			SNDFILE* const pSndFile = sf_open(path.c_str(), SFM_READ, &sfInfo);

			if (pSndFile != nullptr)
			{
				CryFixedStringT<16> const eventTypeString(pAudioTriggerNode->getAttr(s_szPortAudioEventTypeAttribute));
				EPortAudioEventType const eventType = eventTypeString.compareNoCase("start") == 0 ? ePortAudioEventType_Start : ePortAudioEventType_Stop;

				// numLoops -1 == infinite, 0 == once, 1 == twice etc
				int numLoops = 0;
				pAudioTriggerNode->getAttr(s_szPortAudioEventNumLoopsAttribute, numLoops);
				PaStreamParameters streamParameters;
				streamParameters.device = Pa_GetDefaultOutputDevice();
				streamParameters.channelCount = sfInfo.channels;
				streamParameters.suggestedLatency = Pa_GetDeviceInfo(streamParameters.device)->defaultLowOutputLatency;
				streamParameters.hostApiSpecificStreamInfo = nullptr;
				int const subFormat = sfInfo.format & SF_FORMAT_SUBMASK;

				switch (subFormat)
				{
				case SF_FORMAT_PCM_16:
					streamParameters.sampleFormat = paInt16;
					break;
				case SF_FORMAT_PCM_32:
					streamParameters.sampleFormat = paInt32;
					break;
				case SF_FORMAT_FLOAT:
				case SF_FORMAT_VORBIS:
					streamParameters.sampleFormat = paFloat32;
					break;
				}

				POOL_NEW(CAudioTrigger, pAudioTrigger)(
				  AudioStringToId(path.c_str()),
				  numLoops,
				  static_cast<double>(sfInfo.samplerate),
				  eventType,
				  path.c_str(),
				  streamParameters);

				int const failure = sf_close(pSndFile);

				if (failure)
				{
					g_audioImplLogger.Log(eAudioLogType_Error, "Failed to close SNDFILE during CAudioImpl::NewAudioTrigger");
				}
			}
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown PortAudio tag: %s", szTag);
	}

	info.maxRadius = 0;
	return pAudioTrigger;
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger)
{
	POOL_FREE_CONST(pOldAudioTrigger);
}

///////////////////////////////////////////////////////////////////////////
IAudioRtpc const* CAudioImpl::NewAudioRtpc(XmlNodeRef const pAudioParameterNode)
{
	CAudioParameter* pPAAudioParameter = nullptr;
	POOL_NEW(CAudioParameter, pPAAudioParameter);
	return static_cast<IAudioRtpc*>(pPAAudioParameter);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioRtpc(IAudioRtpc const* const pOldAudioRtpc)
{
	POOL_FREE_CONST(pOldAudioRtpc);
}

///////////////////////////////////////////////////////////////////////////
IAudioSwitchState const* CAudioImpl::NewAudioSwitchState(XmlNodeRef const pAudioSwitchNode)
{
	CAudioSwitchState* pPAAudioSwitchState = nullptr;
	POOL_NEW(CAudioSwitchState, pPAAudioSwitchState);
	return static_cast<IAudioSwitchState*>(pPAAudioSwitchState);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState)
{
	POOL_FREE_CONST(pOldAudioSwitchState);
}

///////////////////////////////////////////////////////////////////////////
IAudioEnvironment const* CAudioImpl::NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode)
{
	CAudioEnvironment* pPAAudioEnvironment = nullptr;
	POOL_NEW(CAudioEnvironment, pPAAudioEnvironment);
	return static_cast<IAudioEnvironment*>(pPAAudioEnvironment);
}

///////////////////////////////////////////////////////////////////////////
void CAudioImpl::DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment)
{
	POOL_FREE_CONST(pOldAudioEnvironment);
}

///////////////////////////////////////////////////////////////////////////
char const* const CAudioImpl::GetImplementationNameString() const
{
#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	return m_fullImplString.c_str();
#endif // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::OnAudioSystemRefresh()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		m_localizedSoundBankFolder = PathUtil::GetGameFolder().c_str();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += PathUtil::GetLocalizationFolder();
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += szLanguage;
		m_localizedSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
		m_localizedSoundBankFolder += PORTAUDIO_IMPL_DATA_ROOT;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioImpl::GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const
{
}
