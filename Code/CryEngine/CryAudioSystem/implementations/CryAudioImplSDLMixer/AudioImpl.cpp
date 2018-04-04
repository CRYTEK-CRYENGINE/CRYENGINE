// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "AudioImpl.h"
#include "AudioImplCVars.h"
#include "SoundEngineUtil.h"
#include "SoundEngineTypes.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include <CryAudio/IAudioSystem.h>
#include <CrySystem/IProjectManager.h>

// SDL Mixer
#include <SDL_mixer.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
char const* const CImpl::s_szSDLFileTag = "SDLMixerSample";
char const* const CImpl::s_szSDLCommonAttribute = "sdl_name";
char const* const CImpl::s_szSDLPathAttribute = "sdl_path";
char const* const CImpl::s_szSDLEventIdTag = "event_id";
char const* const CImpl::s_szSDLEventTag = "SDLMixerEvent";
char const* const CImpl::s_szSDLSoundLibraryPath = CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer" CRY_NATIVE_PATH_SEPSTR;
char const* const CImpl::s_szSDLEventTypeTag = "event_type";
char const* const CImpl::s_szSDLEventPanningEnabledTag = "enable_panning";
char const* const CImpl::s_szSDLEventAttenuationEnabledTag = "enable_distance_attenuation";
char const* const CImpl::s_szSDLEventAttenuationMinDistanceTag = "attenuation_dist_min";
char const* const CImpl::s_szSDLEventAttenuationMaxDistanceTag = "attenuation_dist_max";
char const* const CImpl::s_szSDLEventVolumeTag = "volume";
char const* const CImpl::s_szSDLEventLoopCountTag = "loop_count";

///////////////////////////////////////////////////////////////////////////
void OnEventFinished(CATLEvent& audioEvent)
{
	gEnv->pAudioSystem->ReportFinishedEvent(audioEvent, true);
}

///////////////////////////////////////////////////////////////////////////
void OnStandaloneFileFinished(CATLStandaloneFile& standaloneFile, const char* szFile)
{
	gEnv->pAudioSystem->ReportStoppedFile(standaloneFile);
}

///////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
	: m_pCVarFileExtension(nullptr)
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();

	if (strlen(szAssetDirectory) == 0)
	{
		g_implLogger.Log(ELogType::Error, "<Audio - SDL_mixer>: No asset folder set!");
		szAssetDirectory = "no-asset-folder-set";
	}

	m_name = "SDL Mixer 2.0.1 (";
	m_name += szAssetDirectory + PathUtil::RemoveSlash(s_szSDLSoundLibraryPath) + ")";
#endif  // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE

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

///////////////////////////////////////////////////////////////////////////
void CImpl::Update(float const deltaTime)
{
	SoundEngine::Update();
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Init(uint32 const objectPoolSize, uint32 const eventPoolSize)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "SDL Mixer Object Pool");
	CObject::CreateAllocator(objectPoolSize);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "SDL Mixer Event Pool");
	CEvent::CreateAllocator(eventPoolSize);

	m_pCVarFileExtension = REGISTER_STRING("s_SDLMixerStandaloneFileExtension", ".mp3", 0, "the expected file extension for standalone files, played via the sdl_mixer");

	if (SoundEngine::Init())
	{
		SoundEngine::RegisterEventFinishedCallback(OnEventFinished);
		SoundEngine::RegisterStandaloneFileFinishedCallback(OnStandaloneFileFinished);
		return ERequestStatus::Success;
	}
	return ERequestStatus::Failure;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnBeforeShutDown()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ShutDown()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Release()
{
	if (m_pCVarFileExtension)
	{
		m_pCVarFileExtension->Release();
		m_pCVarFileExtension = nullptr;
	}

	SoundEngine::Release();

	delete this;
	g_cvars.UnregisterVariables();

	CObject::FreeMemoryPool();
	CEvent::FreeMemoryPool();

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
	SoundEngine::Refresh();
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnLoseFocus()
{
	SoundEngine::Pause();
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnGetFocus()
{
	SoundEngine::Resume();
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteAll()
{
	SoundEngine::Mute();
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnmuteAll()
{
	SoundEngine::UnMute();
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::StopAllSounds()
{
	SoundEngine::Stop();
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus result = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		SFile* const pFileData = static_cast<SFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			pFileData->sampleId = SoundEngine::LoadSampleFromMemory(pFileInfo->pFileData, pFileInfo->size, pFileInfo->szFileName);
			result = ERequestStatus::Success;
		}
		else
		{
			g_implLogger.Log(ELogType::Error, "Invalid AudioFileEntryData passed to the SDL Mixer implementation of RegisterInMemoryFile");
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus result = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		SFile* const pFileData = static_cast<SFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			SoundEngine::UnloadSample(pFileData->sampleId);
			result = ERequestStatus::Success;
		}
		else
		{
			g_implLogger.Log(ELogType::Error, "Invalid AudioFileEntryData passed to the SDL Mixer implementation of UnregisterInMemoryFile");
		}
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo)
{
	ERequestStatus result = ERequestStatus::Failure;

	if ((_stricmp(pRootNode->getTag(), s_szSDLFileTag) == 0) && (pFileInfo != nullptr))
	{
		char const* const szFileName = pRootNode->getAttr(s_szSDLCommonAttribute);
		char const* const szPath = pRootNode->getAttr(s_szSDLPathAttribute);
		CryFixedStringT<MaxFilePathLength> fullFilePath;
		if (szPath)
		{
			fullFilePath = szPath;
			fullFilePath += CRY_NATIVE_PATH_SEPSTR;
			fullFilePath += szFileName;
		}
		else
		{
			fullFilePath = szFileName;
		}

		// Currently the SDLMixer Implementation does not support localized files.
		pFileInfo->bLocalized = false;

		if (!fullFilePath.empty())
		{
			pFileInfo->szFileName = fullFilePath.c_str();
			pFileInfo->memoryBlockAlignment = m_memoryAlignment;
			pFileInfo->pImplData = new SFile;
			result = ERequestStatus::Success;
		}
		else
		{
			pFileInfo->szFileName = nullptr;
			pFileInfo->memoryBlockAlignment = 0;
			pFileInfo->pImplData = nullptr;
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

///////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetFileLocation(SFileInfo* const pFileInfo)
{
	static CryFixedStringT<MaxFilePathLength> s_path;
	s_path = PathUtil::GetGameFolder().c_str();
	s_path += s_szSDLSoundLibraryPath;

	return s_path.c_str();
}

///////////////////////////////////////////////////////////////////////////
ITrigger const* CImpl::ConstructTrigger(XmlNodeRef const pRootNode)
{
	CTrigger* pTrigger = nullptr;

	if (_stricmp(pRootNode->getTag(), s_szSDLEventTag) == 0)
	{
		pTrigger = SoundEngine::CreateTrigger();

		if (pTrigger != nullptr)
		{
			char const* const szFileName = pRootNode->getAttr(s_szSDLCommonAttribute);
			char const* const szPath = pRootNode->getAttr(s_szSDLPathAttribute);
			string fullFilePath;

			if (szPath != nullptr && szPath[0] != '\0')
			{
				fullFilePath = szPath;
				fullFilePath += CRY_NATIVE_PATH_SEPSTR;
				fullFilePath += szFileName;
			}
			else
			{
				fullFilePath = szFileName;
			}

			pTrigger->m_sampleId = SoundEngine::LoadSample(fullFilePath, true);
			pTrigger->m_bStartEvent = (_stricmp(pRootNode->getAttr(s_szSDLEventTypeTag), "stop") != 0);

			if (pTrigger->m_bStartEvent)
			{
				pTrigger->m_bPanningEnabled = (_stricmp(pRootNode->getAttr(s_szSDLEventPanningEnabledTag), "true") == 0);
				bool bAttenuationEnabled = (_stricmp(pRootNode->getAttr(s_szSDLEventAttenuationEnabledTag), "true") == 0);
				if (bAttenuationEnabled)
				{
					pRootNode->getAttr(s_szSDLEventAttenuationMinDistanceTag, pTrigger->m_attenuationMinDistance);
					pRootNode->getAttr(s_szSDLEventAttenuationMaxDistanceTag, pTrigger->m_attenuationMaxDistance);
				}
				else
				{
					pTrigger->m_attenuationMinDistance = -1.0f;
					pTrigger->m_attenuationMaxDistance = -1.0f;
				}

				// Translate decibel to normalized value.
				static const int maxVolume = 128;
				float volume = 0.0f;
				pRootNode->getAttr(s_szSDLEventVolumeTag, volume);
				pTrigger->m_volume = static_cast<int>(pow_tpl(10.0f, volume / 20.0f) * maxVolume);

				pRootNode->getAttr(s_szSDLEventLoopCountTag, pTrigger->m_numLoops);
			}
		}
	}

	return static_cast<ITrigger*>(pTrigger);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTrigger(ITrigger const* const pITrigger)
{
	if (pITrigger != nullptr)
	{
		CTrigger const* const pTrigger = static_cast<CTrigger const* const>(pITrigger);
		SoundEngine::StopTrigger(pTrigger);
		delete pTrigger;
	}
}

///////////////////////////////////////////////////////////////////////////
IParameter const* CImpl::ConstructParameter(XmlNodeRef const pRootNode)
{
	return static_cast<IParameter*>(new SParameter);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameter(IParameter const* const pIParameter)
{
	delete pIParameter;
}

///////////////////////////////////////////////////////////////////////////
ISwitchState const* CImpl::ConstructSwitchState(XmlNodeRef const pRootNode)
{
	return static_cast<ISwitchState*>(new SSwitchState);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchState(ISwitchState const* const pISwitchState)
{
	delete pISwitchState;
}

///////////////////////////////////////////////////////////////////////////
IEnvironment const* CImpl::ConstructEnvironment(XmlNodeRef const pRootNode)
{
	return static_cast<IEnvironment*>(new SEnvironment);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironment(IEnvironment const* const pIEnvironment)
{
	delete pIEnvironment;
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructGlobalObject()
{
	return static_cast<IObject*>(new CObject(0));
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(char const* const szName /*= nullptr*/)
{
	static uint32 id = 1;
	CObject* pObject = new CObject(id++);

	SoundEngine::RegisterObject(pObject);

#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	m_idToName[pObject->m_id] = szName;
#endif  // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	if (pIObject != nullptr)
	{
		CObject const* pObject = static_cast<CObject const*>(pIObject);
		SoundEngine::UnregisterObject(pObject);
		delete pObject;
	}
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(char const* const szName /*= nullptr*/)
{
	static ListenerId id = 0;
	return static_cast<IListener*>(new CListener(id++));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	delete pIListener;
}

///////////////////////////////////////////////////////////////////////////
IEvent* CImpl::ConstructEvent(CATLEvent& event)
{
	return static_cast<IEvent*>(new CEvent(event));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEvent(IEvent const* const pIEvent)
{
	delete pIEvent;
}

///////////////////////////////////////////////////////////////////////////
IStandaloneFile* CImpl::ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger /*= nullptr*/)
{
	static string s_localizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + m_language + CRY_NATIVE_PATH_SEPSTR;
	static string s_nonLocalizedfilesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR;
	static string filePath;

	if (bLocalized)
	{
		filePath = s_localizedfilesFolder + szFile + m_pCVarFileExtension->GetString();
	}
	else
	{
		filePath = s_nonLocalizedfilesFolder + szFile + m_pCVarFileExtension->GetString();
	}

	return static_cast<IStandaloneFile*>(new CStandaloneFile(filePath, standaloneFile));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile)
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	const CStandaloneFile* const pStandaloneEvent = static_cast<const CStandaloneFile*>(pIStandaloneFile);
	CRY_ASSERT_MESSAGE(pStandaloneEvent->m_channels.size() == 0, "Events always have to be stopped/finished before they get deleted");
#endif  // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE
	delete pIStandaloneFile;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{}

///////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{}

///////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
	m_language = szLanguage;
}

///////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetName() const
{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	return m_name.c_str();
#endif  // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE
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

///////////////////////////////////////////////////////////////////////////
void CImpl::GetFileData(char const* const szName, SFileData& fileData) const
{}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
