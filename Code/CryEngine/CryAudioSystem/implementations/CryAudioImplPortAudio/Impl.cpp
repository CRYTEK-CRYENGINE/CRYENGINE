// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "Common.h"
#include "Event.h"
#include "EventInstance.h"
#include "Object.h"
#include "CVars.h"
#include "Listener.h"
#include "GlobalData.h"

#include <IEnvironmentConnection.h>
#include <IFile.h>
#include <IParameterConnection.h>
#include <ISettingConnection.h>
#include <ISwitchStateConnection.h>
#include <FileInfo.h>
#include <sndfile.hh>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	#include <Logger.h>
	#include <DebugStyle.h>
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
std::vector<CObject*> g_constructedObjects;
std::vector<CEvent*> g_events;

uint16 g_eventsPoolSize = 0;
std::map<ContextId, uint16> g_contextEventPoolSizes;

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
uint16 g_debugEventrPoolSize = 0;
uint16 g_objectPoolSize = 0;
uint16 g_eventInstancePoolSize = 0;
EventInstances g_constructedEventInstances;
std::vector<CListener*> g_constructedListeners;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CountPoolSizes(XmlNodeRef const& node, uint16& poolSizes)
{
	uint16 numEvents = 0;
	node->getAttr(g_szEventsAttribute, numEvents);
	poolSizes += numEvents;
}

//////////////////////////////////////////////////////////////////////////
void AllocateMemoryPools(uint16 const objectPoolSize, uint16 const eventPoolSize)
{
	CObject::CreateAllocator(objectPoolSize);
	CEventInstance::CreateAllocator(eventPoolSize);
	CEvent::CreateAllocator(g_eventsPoolSize);
}

//////////////////////////////////////////////////////////////////////////
void FreeMemoryPools()
{
	CObject::FreeMemoryPool();
	CEventInstance::FreeMemoryPool();
	CEvent::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
bool GetSoundInfo(char const* const szPath, SF_INFO& sfInfo, PaStreamParameters& streamParameters)
{
	bool success = false;

	ZeroStruct(sfInfo);
	SNDFILE* const pSndFile = sf_open(szPath, SFM_READ, &sfInfo);

	if (pSndFile != nullptr)
	{
		streamParameters.device = Pa_GetDefaultOutputDevice();
		streamParameters.channelCount = sfInfo.channels;
		streamParameters.suggestedLatency = Pa_GetDeviceInfo(streamParameters.device)->defaultLowOutputLatency;
		streamParameters.hostApiSpecificStreamInfo = nullptr;
		int const subFormat = sfInfo.format & SF_FORMAT_SUBMASK;

		switch (subFormat)
		{
		case SF_FORMAT_PCM_16: // Intentional fall-through.
		case SF_FORMAT_PCM_24:
			{
				streamParameters.sampleFormat = paInt16;
				break;
			}
		case SF_FORMAT_PCM_32:
			{
				streamParameters.sampleFormat = paInt32;
				break;
			}
		case SF_FORMAT_FLOAT: // Intentional fall-through.
		case SF_FORMAT_VORBIS:
			{
				streamParameters.sampleFormat = paFloat32;
				break;
			}
		default:
			{
				break;
			}
		}

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
		if (sf_close(pSndFile) != 0)
		{
			Cry::Audio::Log(ELogType::Error, "Failed to close sound file %s", szPath);
		}
#else
		sf_close(pSndFile);
#endif    // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

		success = true;
	}
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Error, "Failed to open sound file %s", szPath);
	}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	return success;
}

//////////////////////////////////////////////////////////////////////////
CImpl::CImpl()
{
	g_pImpl = this;
}

///////////////////////////////////////////////////////////////////////////
bool CImpl::Init(uint16 const objectPoolSize)
{
	if (g_cvars.m_eventPoolSize < 1)
	{
		g_cvars.m_eventPoolSize = 1;

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, R"(Event pool size must be at least 1. Forcing the cvar "s_PortAudioEventPoolSize" to 1!)");
#endif    // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
	}

	g_constructedObjects.reserve(static_cast<size_t>(objectPoolSize));
	AllocateMemoryPools(objectPoolSize, static_cast<uint16>(g_cvars.m_eventPoolSize));

	char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	g_objectPoolSize = objectPoolSize;
	g_eventInstancePoolSize = static_cast<uint16>(g_cvars.m_eventPoolSize);

	g_constructedEventInstances.reserve(static_cast<size_t>(g_cvars.m_eventPoolSize));

	if (strlen(szAssetDirectory) == 0)
	{
		Cry::Audio::Log(ELogType::Error, "<Audio - PortAudio>: No asset folder set!");
		szAssetDirectory = "no-asset-folder-set";
	}

	m_name = Pa_GetVersionText();
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	m_regularSoundBankFolder = szAssetDirectory;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += CRY_AUDIO_DATA_ROOT;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szImplFolderName;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += g_szAssetsFolderName;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	if (ICVar* const pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		SetLanguage(pCVar->GetString());
	}

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	PaError const err = Pa_Initialize();

	if (err != paNoError)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
	}
#else
	Pa_Initialize();
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	return true;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ShutDown()
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	PaError const err = Pa_Terminate();

	if (err != paNoError)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to shut down PortAudio: %s", Pa_GetErrorText(err));
	}
#else
	Pa_Terminate();
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::Release()
{
	delete this;
	g_pImpl = nullptr;
	g_cvars.UnregisterVariables();

	FreeMemoryPools();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLibraryData(XmlNodeRef const& node, ContextId const contextId)
{
	if (contextId == GlobalContextId)
	{
		CountPoolSizes(node, g_eventsPoolSize);
	}
	else
	{
		CountPoolSizes(node, g_contextEventPoolSizes[contextId]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeLibraryDataChanged()
{
	g_eventsPoolSize = 0;
	g_contextEventPoolSizes.clear();

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	g_debugEventrPoolSize = 0;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterLibraryDataChanged(int const poolAllocationMode)
{
	if (!g_contextEventPoolSizes.empty())
	{
		if (poolAllocationMode <= 0)
		{
			for (auto const& poolSizePair : g_contextEventPoolSizes)
			{
				g_eventsPoolSize += poolSizePair.second;
			}
		}
		else
		{
			uint16 maxContextPoolsize = 0;

			for (auto const& poolSizePair : g_contextEventPoolSizes)
			{
				maxContextPoolsize = std::max(maxContextPoolsize, poolSizePair.second);
			}

			g_eventsPoolSize += maxContextPoolsize;
		}
	}

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	// Used to hide pools without allocations in debug draw.
	g_debugEventrPoolSize = g_eventsPoolSize;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	g_eventsPoolSize = std::max<uint16>(1, g_eventsPoolSize);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnLoseFocus()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::OnGetFocus()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::MuteAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::UnmuteAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::PauseAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ResumeAll()
{
}

///////////////////////////////////////////////////////////////////////////
void CImpl::StopAllSounds()
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->StopAllTriggers();
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
}

//////////////////////////////////////////////////////////////////////////
bool CImpl::ConstructFile(XmlNodeRef const& rootNode, SFileInfo* const pFileInfo)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

///////////////////////////////////////////////////////////////////////////
char const* CImpl::GetFileLocation(IFile* const pIFile) const
{
	return m_localizedSoundBankFolder.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	cry_strcpy(implInfo.name, m_name.c_str());
#else
	cry_fixed_size_strcpy(implInfo.name, g_implNameInRelease);
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
	cry_strcpy(implInfo.folderName, g_szImplFolderName, strlen(g_szImplFolderName));
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(CTransformation const& transformation, IListeners const& listeners, char const* const szName /*= nullptr*/)
{
	auto const pListener = static_cast<CListener*>(listeners[0]);

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CObject");
	auto const pObject = new CObject(pListener);

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	pObject->SetName(szName);

	if (listeners.size() > 1)
	{
		Cry::Audio::Log(ELogType::Warning, R"(More than one listener is passed in %s. Port Audio supports only one listener per object. Listener "%s" will be used for object "%s".)",
		                __FUNCTION__, pListener->GetName(), pObject->GetName());
	}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	stl::push_back_unique(g_constructedObjects, pObject);

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	auto const pObject = static_cast<CObject const*>(pIObject);
	stl::find_and_erase(g_constructedObjects, pObject);
	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(CTransformation const& transformation, char const* const szName)
{
	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CListener");
	auto const pListener = new CListener(transformation);

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	pListener->SetName(szName);
	g_constructedListeners.push_back(pListener);
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	return static_cast<IListener*>(pListener);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	auto const pListener = static_cast<CListener*>(pIListener);

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	auto iter(g_constructedListeners.begin());
	auto const iterEnd(g_constructedListeners.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pListener)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = g_constructedListeners.back();
			}

			g_constructedListeners.pop_back();
			break;
		}
	}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	delete pListener;
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
ITriggerConnection* CImpl::ConstructTriggerConnection(XmlNodeRef const& rootNode, float& radius)
{
	ITriggerConnection* pITriggerConnection = nullptr;

	char const* const szTag = rootNode->getTag();

	if (_stricmp(szTag, g_szEventTag) == 0)
	{
		char const* const szLocalized = rootNode->getAttr(g_szLocalizedAttribute);
		bool const isLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, g_szTrueValue) == 0);

		stack_string path = (isLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str());
		path += "/";
		stack_string const folderName = rootNode->getAttr(g_szPathAttribute);

		if (!folderName.empty())
		{
			path += folderName.c_str();
			path += "/";
		}

		stack_string const name = rootNode->getAttr(g_szNameAttribute);
		path += name.c_str();

		SF_INFO sfInfo;
		PaStreamParameters streamParameters;

		if (GetSoundInfo(path.c_str(), sfInfo, streamParameters))
		{
			CryFixedStringT<16> const eventTypeString(rootNode->getAttr(g_szTypeAttribute));
			CEvent::EActionType const actionType = eventTypeString.compareNoCase(g_szStartValue) == 0 ? CEvent::EActionType::Start : CEvent::EActionType::Stop;
			EEventFlags const flags = isLocalized ? EEventFlags::IsLocalized : EEventFlags::None;

			int numLoops = 0;
			rootNode->getAttr(g_szLoopCountAttribute, numLoops);
			// --numLoops because -1: play infinite, 0: play once, 1: play twice, etc...
			--numLoops;
			// Max to -1 to stay backwards compatible.
			numLoops = std::max(-1, numLoops);

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CEvent");
			auto const pEvent = new CEvent(
				flags,
				actionType,
				StringToId(path.c_str()),
				numLoops,
				static_cast<double>(sfInfo.samplerate),
				streamParameters,
				path.c_str(),
				folderName.c_str(),
				name.c_str());

			g_events.push_back(pEvent);

			pITriggerConnection = static_cast<ITriggerConnection*>(pEvent);
		}
	}
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown PortAudio tag: %s", szTag);
	}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	return pITriggerConnection;
}

//////////////////////////////////////////////////////////////////////////
ITriggerConnection* CImpl::ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo)
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	ITriggerConnection* pITriggerConnection = nullptr;
	auto const pTriggerInfo = static_cast<STriggerInfo const*>(pITriggerInfo);

	if (pTriggerInfo != nullptr)
	{
		stack_string path = (pTriggerInfo->isLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str());
		path += "/";
		stack_string const folderName(pTriggerInfo->path);

		if (!folderName.empty())
		{
			path += folderName.c_str();
			path += "/";
		}

		stack_string const name(pTriggerInfo->name);
		path += name.c_str();

		SF_INFO sfInfo;
		PaStreamParameters streamParameters;

		if (GetSoundInfo(path.c_str(), sfInfo, streamParameters))
		{
			EEventFlags const flags = pTriggerInfo->isLocalized ? EEventFlags::IsLocalized : EEventFlags::None;

			MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CEvent");
			pITriggerConnection = static_cast<ITriggerConnection*>(
				new CEvent(
					flags,
					CEvent::EActionType::Start,
					StringToId(path.c_str()),
					0,
					static_cast<double>(sfInfo.samplerate),
					streamParameters,
					path.c_str(),
					folderName.c_str(),
					name.c_str()));
		}
	}

	return pITriggerConnection;
#else
	return nullptr;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTriggerConnection(ITriggerConnection* const pITriggerConnection)
{
	auto const pEvent = static_cast<CEvent*>(pITriggerConnection);
	pEvent->SetFlag(EEventFlags::ToBeDestructed);

	if (pEvent->CanBeDestructed())
	{
		delete pEvent;
	}
}

///////////////////////////////////////////////////////////////////////////
IParameterConnection* CImpl::ConstructParameterConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameterConnection(IParameterConnection const* const pIParameterConnection)
{
	delete pIParameterConnection;
}

///////////////////////////////////////////////////////////////////////////
ISwitchStateConnection* CImpl::ConstructSwitchStateConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection)
{
	delete pISwitchStateConnection;
}

///////////////////////////////////////////////////////////////////////////
IEnvironmentConnection* CImpl::ConstructEnvironmentConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection)
{
	delete pIEnvironmentConnection;
}

//////////////////////////////////////////////////////////////////////////
ISettingConnection* CImpl::ConstructSettingConnection(XmlNodeRef const& rootNode)
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructSettingConnection(ISettingConnection const* const pISettingConnection)
{
	delete pISettingConnection;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
	g_events.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		bool const shouldReload = !m_language.IsEmpty() && (m_language.compareNoCase(szLanguage) != 0);

		m_language = szLanguage;
		char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();
		m_localizedSoundBankFolder = szAssetDirectory;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += PathUtil::GetLocalizationFolder().c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += szLanguage;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += CRY_AUDIO_DATA_ROOT;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szImplFolderName;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += g_szAssetsFolderName;

		if (shouldReload)
		{
			UpdateLocalizedEvents();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UpdateLocalizedEvents()
{
	for (auto const pEvent : g_events)
	{
		if ((pEvent->GetFlags() & EEventFlags::IsLocalized) != EEventFlags::None)
		{
			stack_string path = m_localizedSoundBankFolder.c_str();
			path += "/";

			char const* const szFolderName = pEvent->GetFolder();

			if ((szFolderName != nullptr) && (szFolderName[0] != '\0'))
			{
				path += szFolderName;
				path += "/";
			}

			path += pEvent->GetName();

			SF_INFO sfInfo;
			PaStreamParameters streamParameters;

			GetSoundInfo(path.c_str(), sfInfo, streamParameters);
			pEvent->SetSampleRate(static_cast<double>(sfInfo.samplerate));
			pEvent->SetStreamParameters(streamParameters);
			pEvent->SetFilePath(path.c_str());
		}
	}
}

#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
CEventInstance* CImpl::ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object)
{
	event.IncrementNumInstances();
	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CEventInstance");

	auto const pEventInstance = new CEventInstance(triggerInstanceId, event, object);
	g_constructedEventInstances.push_back(pEventInstance);

	return pEventInstance;
}
#else
//////////////////////////////////////////////////////////////////////////
CEventInstance* CImpl::ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event)
{
	event.IncrementNumInstances();

	MEMSTAT_CONTEXT(EMemStatContextType::AudioImpl, "CryAudio::Impl::PortAudio::CEventInstance");
	return new CEventInstance(triggerInstanceId, event);
}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructEventInstance(CEventInstance const* const pEventInstance)
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	CRY_ASSERT_MESSAGE(pEventInstance != nullptr, "pEventInstance is nullpter during %s", __FUNCTION__);

	auto iter(g_constructedEventInstances.begin());
	auto const iterEnd(g_constructedEventInstances.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pEventInstance)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = g_constructedEventInstances.back();
			}

			g_constructedEventInstances.pop_back();
			break;
		}
	}

#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE

	CEvent* const pEvent = &pEventInstance->GetEvent();
	delete pEventInstance;

	pEvent->DecrementNumInstances();

	if (pEvent->CanBeDestructed())
	{
		delete pEvent;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const drawDetailedInfo)
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	float const headerPosY = posY;
	posY += Debug::g_systemHeaderLineSpacerHeight;

	size_t totalPoolSize = 0;

	{
		auto& allocator = CObject::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Objects", g_objectPoolSize);
		}
	}

	{
		auto& allocator = CEventInstance::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Event Instances", g_eventInstancePoolSize);
		}
	}

	if (g_debugEventrPoolSize > 0)
	{
		auto& allocator = CEvent::GetAllocator();
		size_t const memAlloc = allocator.GetTotalMemory().nAlloc;
		totalPoolSize += memAlloc;

		if (drawDetailedInfo)
		{
			Debug::DrawMemoryPoolInfo(auxGeom, posX, posY, memAlloc, allocator.GetCounts(), "Events", g_eventsPoolSize);
		}
	}

	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);
	CryGetMemoryInfoForModule(&memInfo);

	CryFixedStringT<Debug::MaxMemInfoStringLength> memAllocSizeString;
	auto const memAllocSize = static_cast<size_t>(memInfo.allocated - memInfo.freed);
	Debug::FormatMemoryString(memAllocSizeString, memAllocSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalPoolSizeString;
	Debug::FormatMemoryString(totalPoolSizeString, totalPoolSize);

	CryFixedStringT<Debug::MaxMemInfoStringLength> totalMemSizeString;
	size_t const totalMemSize = memAllocSize + totalPoolSize;
	Debug::FormatMemoryString(totalMemSizeString, totalMemSize);

	auxGeom.Draw2dLabel(posX, headerPosY, Debug::g_systemHeaderFontSize, Debug::s_globalColorHeader, false, "%s (System: %s | Pools: %s | Total: %s)",
	                    m_name.c_str(), memAllocSizeString.c_str(), totalPoolSizeString.c_str(), totalMemSizeString.c_str());

	size_t const numEvents = g_constructedEventInstances.size();
	size_t const numListeners = g_constructedListeners.size();

	posY += Debug::g_systemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorTextSecondary, false, "Active Events: %3" PRISIZE_T " | Listeners: %" PRISIZE_T, numEvents, numListeners);

	for (auto const pListener : g_constructedListeners)
	{
		Vec3 const& listenerPosition = pListener->GetTransformation().GetPosition();
		Vec3 const& listenerDirection = pListener->GetTransformation().GetForward();
		char const* const szName = pListener->GetName();

		posY += Debug::g_systemLineHeight;
		auxGeom.Draw2dLabel(posX, posY, Debug::g_systemFontSize, Debug::s_systemColorListenerActive, false, "Listener: %s | PosXYZ: %.2f %.2f %.2f | FwdXYZ: %.2f %.2f %.2f",
		                    szName, listenerPosition.x, listenerPosition.y, listenerPosition.z, listenerDirection.x, listenerDirection.y, listenerDirection.z);
	}
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
}

/////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, Vec3 const& camPos, float const debugDistance, char const* const szTextFilter) const
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(szTextFilter);
	lowerCaseSearchString.MakeLower();

	auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "Port Audio Events [%" PRISIZE_T "]", g_constructedEventInstances.size());
	posY += Debug::g_listHeaderLineHeight;

	for (auto const pEventInstance : g_constructedEventInstances)
	{
		Vec3 const& position = pEventInstance->GetObject().GetTransformation().GetPosition();
		float const distance = position.GetDistance(camPos);

		if ((debugDistance <= 0.0f) || ((debugDistance > 0.0f) && (distance < debugDistance)))
		{
			char const* const szEventName = pEventInstance->GetEvent().GetName();
			CryFixedStringT<MaxControlNameLength> lowerCaseEventName(szEventName);
			lowerCaseEventName.MakeLower();
			bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseEventName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

			if (draw)
			{
				char const* const szListenerName = (pEventInstance->GetObject().GetListener() != nullptr) ? pEventInstance->GetObject().GetListener()->GetName() : "No Listener!";

				auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, Debug::s_listColorItemActive, false, "%s on %s (%s)",
				                    szEventName, pEventInstance->GetObject().GetName(), szListenerName);

				posY += Debug::g_listLineHeight;
			}
		}
	}

	posX += 600.0f;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
