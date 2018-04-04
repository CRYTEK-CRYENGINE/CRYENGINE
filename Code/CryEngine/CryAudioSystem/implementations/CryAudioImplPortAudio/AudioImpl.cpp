// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>
#include <CryString/CryPath.h>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
char const* const CImpl::s_szPortAudioEventTag = "PortAudioEvent";
char const* const CImpl::s_szPortAudioEventNameAttribute = "portaudio_name";
char const* const CImpl::s_szPortAudioEventTypeAttribute = "event_type";
char const* const CImpl::s_szPortAudioEventNumLoopsAttribute = "num_loops";

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Init(uint32 const objectPoolSize, uint32 const eventPoolSize)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Port Audio Object Pool");
	CObject::CreateAllocator(objectPoolSize);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Port Audio Event Pool");
	CEvent::CreateAllocator(eventPoolSize);

	char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();

	if (strlen(szAssetDirectory) == 0)
	{
		g_implLogger.Log(ELogType::Error, "<Audio - PortAudio>: No asset folder set!");
		szAssetDirectory = "no-asset-folder-set";
	}

	m_regularSoundBankFolder = szAssetDirectory;
	m_regularSoundBankFolder += CRY_NATIVE_PATH_SEPSTR;
	m_regularSoundBankFolder += PORTAUDIO_IMPL_DATA_ROOT;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	PaError const err = Pa_Initialize();

	if (err != paNoError)
	{
		g_implLogger.Log(ELogType::Error, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
	}

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	m_name = Pa_GetVersionText();
	m_name += " (";
	m_name += szAssetDirectory;
	m_name += CRY_NATIVE_PATH_SEPSTR PORTAUDIO_IMPL_DATA_ROOT ")";
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnBeforeShutDown()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ShutDown()
{
	PaError const err = Pa_Terminate();

	if (err != paNoError)
	{
		g_implLogger.Log(ELogType::Error, "Failed to shut down PortAudio: %s", Pa_GetErrorText(err));
	}

	return (err == paNoError) ? ERequestStatus::Success : ERequestStatus::Failure;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Release()
{
	delete this;
	g_cvars.UnregisterVariables();

	CObject::FreeMemoryPool();
	CEvent::FreeMemoryPool();

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnLoseFocus()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnGetFocus()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnmuteAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::StopAllSounds()
{
	return ERequestStatus::Success;
}
//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus requestResult = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		CFile* const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			requestResult = ERequestStatus::Success;
		}
		else
		{
			g_implLogger.Log(ELogType::Error, "Invalid AudioFileEntryData passed to the PortAudio implementation of RegisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus requestResult = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		CFile* const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			requestResult = ERequestStatus::Success;
		}
		else
		{
			g_implLogger.Log(ELogType::Error, "Invalid AudioFileEntryData passed to the PortAudio implementation of UnregisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo)
{
	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

//////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetFileLocation(SFileInfo* const pFileInfo)
{
	char const* szResult = nullptr;

	if (pFileInfo != nullptr)
	{
		szResult = pFileInfo->bLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str();
	}

	return szResult;
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructGlobalObject()
{
	return new CObject();
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(char const* const szName /*= nullptr*/)
{
	CObject* pObject = new CObject();
	stl::push_back_unique(m_constructedObjects, pObject);

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	CObject const* const pObject = static_cast<CObject const*>(pIObject);
	stl::find_and_erase(m_constructedObjects, pObject);
	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(char const* const szName /*= nullptr*/)
{
	return static_cast<IListener*>(new CListener);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	delete pIListener;
}

//////////////////////////////////////////////////////////////////////////
IEvent* CImpl::ConstructEvent(CATLEvent& event)
{
	return static_cast<IEvent*>(new CEvent(event));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEvent(IEvent const* const pIEvent)
{
	delete pIEvent;
}

//////////////////////////////////////////////////////////////////////////
IStandaloneFile* CImpl::ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger /*= nullptr*/)
{
	return static_cast<IStandaloneFile*>(new CStandaloneFile);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile)
{
	delete pIStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
ITrigger const* CImpl::ConstructTrigger(XmlNodeRef const pRootNode)
{
	CTrigger* pTrigger = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szPortAudioEventTag) == 0)
	{
		stack_string path = m_regularSoundBankFolder.c_str();
		path += CRY_NATIVE_PATH_SEPSTR;
		path += pRootNode->getAttr(s_szPortAudioEventNameAttribute);

		if (!path.empty())
		{
			SF_INFO sfInfo;
			ZeroStruct(sfInfo);

			SNDFILE* const pSndFile = sf_open(path.c_str(), SFM_READ, &sfInfo);

			if (pSndFile != nullptr)
			{
				CryFixedStringT<16> const eventTypeString(pRootNode->getAttr(s_szPortAudioEventTypeAttribute));
				EEventType const eventType = eventTypeString.compareNoCase("start") == 0 ? EEventType::Start : EEventType::Stop;

				// numLoops -1 == infinite, 0 == once, 1 == twice etc
				int numLoops = 0;
				pRootNode->getAttr(s_szPortAudioEventNumLoopsAttribute, numLoops);
				PaStreamParameters streamParameters;
				streamParameters.device = Pa_GetDefaultOutputDevice();
				streamParameters.channelCount = sfInfo.channels;
				streamParameters.suggestedLatency = Pa_GetDeviceInfo(streamParameters.device)->defaultLowOutputLatency;
				streamParameters.hostApiSpecificStreamInfo = nullptr;
				int const subFormat = sfInfo.format & SF_FORMAT_SUBMASK;

				switch (subFormat)
				{
				case SF_FORMAT_PCM_16:
				case SF_FORMAT_PCM_24:
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

				pTrigger = new CTrigger(
				  StringToId(path.c_str()),
				  numLoops,
				  static_cast<double>(sfInfo.samplerate),
				  eventType,
				  path.c_str(),
				  streamParameters);

				int const failure = sf_close(pSndFile);

				if (failure)
				{
					g_implLogger.Log(ELogType::Error, "Failed to close SNDFILE during CImpl::NewAudioTrigger");
				}
			}
		}
	}
	else
	{
		g_implLogger.Log(ELogType::Warning, "Unknown PortAudio tag: %s", szTag);
	}

	return static_cast<ITrigger*>(pTrigger);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTrigger(ITrigger const* const pITrigger)
{
	delete pITrigger;
}

///////////////////////////////////////////////////////////////////////////
IParameter const* CImpl::ConstructParameter(XmlNodeRef const pRootNode)
{
	return static_cast<IParameter*>(new CParameter);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameter(IParameter const* const pIParameter)
{
	delete pIParameter;
}

///////////////////////////////////////////////////////////////////////////
ISwitchState const* CImpl::ConstructSwitchState(XmlNodeRef const pRootNode)
{
	return static_cast<ISwitchState*>(new CSwitchState);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchState(ISwitchState const* const pISwitchState)
{
	delete pISwitchState;
}

///////////////////////////////////////////////////////////////////////////
IEnvironment const* CImpl::ConstructEnvironment(XmlNodeRef const pRootNode)
{
	return static_cast<IEnvironment*>(new CEnvironment);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironment(IEnvironment const* const pIEnvironment)
{
	delete pIEnvironment;
}

///////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetName() const
{
#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	return m_name.c_str();
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::GetMemoryInfo(SMemoryInfo& memoryInfo) const
{
	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);
	CryGetMemoryInfoForModule(&memInfo);

	memoryInfo.totalMemory = static_cast<size_t>(memInfo.allocated - memInfo.freed);

	memoryInfo.secondaryPoolSize = 0;
	memoryInfo.secondaryPoolUsedSize = 0;
	memoryInfo.secondaryPoolAllocations = 0;

	{
		auto& allocator = CObject::GetAllocator();
		auto mem = allocator.GetTotalMemory();
		auto pool = allocator.GetCounts();
		memoryInfo.poolUsedObjects = pool.nUsed;
		memoryInfo.poolConstructedObjects = pool.nAlloc;
		memoryInfo.poolUsedMemory = mem.nUsed;
		memoryInfo.poolAllocatedMemory = mem.nAlloc;
	}

	{
		auto& allocator = CEvent::GetAllocator();
		auto mem = allocator.GetTotalMemory();
		auto pool = allocator.GetCounts();
		memoryInfo.poolUsedObjects += pool.nUsed;
		memoryInfo.poolConstructedObjects += pool.nAlloc;
		memoryInfo.poolUsedMemory += mem.nUsed;
		memoryInfo.poolAllocatedMemory += mem.nAlloc;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
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
void CImpl::GetFileData(char const* const szName, SFileData& fileData) const
{
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
