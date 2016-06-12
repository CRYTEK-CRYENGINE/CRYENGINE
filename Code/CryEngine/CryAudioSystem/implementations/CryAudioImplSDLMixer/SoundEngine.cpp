// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SoundEngine.h"
#include "SoundEngineUtil.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>

#include <SDL.h>
#include <SDL_mixer.h>

#define SDL_MIXER_NUM_CHANNELS 512
#define SDL_MIXER_PROJECT_PATH AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer" CRY_NATIVE_PATH_SEPSTR

using namespace CryAudio::Impl::SDL_mixer;

static const int g_nSupportedFormats = MIX_INIT_OGG | MIX_INIT_MP3;
static const int g_nNumMixChannels = SDL_MIXER_NUM_CHANNELS;

static const SampleId SDL_MIXER_INVALID_SAMPLE_ID = 0;
const int g_sampleRate = 48000;
const int g_bufferSize = 4096;

// Samples
string g_sampleDataRootDir;
typedef std::unordered_map<SampleId, Mix_Chunk*> SampleDataMap;
SampleDataMap g_sampleData;

typedef std::unordered_map<SampleId, string> SampleNameMap;
SampleNameMap g_samplePaths;

typedef std::unordered_map<SampleId, int> SampleIdUsageCounterMap;
SampleIdUsageCounterMap g_usageCounters;

// Channels
struct SChannelData
{
	SAudioObject* pAudioObject;
};

SChannelData g_channels[SDL_MIXER_NUM_CHANNELS];

typedef std::queue<int> TChannelQueue;
TChannelQueue g_freeChannels;

enum EChannelFinishedRequestQueueId
{
	eChannelFinishedRequestQueueId_One = 0,
	eChannelFinishedRequestQueueId_Two = 1,
	eChannelFinishedRequestQueueId_Count
};

typedef std::deque<int, STLSoundAllocator<int>> ChannelFinishedRequests;
ChannelFinishedRequests g_channelFinishedRequests[eChannelFinishedRequestQueueId_Count];
CryCriticalSection g_channelFinishedCriticalSection;

// Audio Objects
typedef std::vector<SAudioObject*, STLSoundAllocator<SAudioObject*>> AudioObjectList;
AudioObjectList g_audioObjects;

// Listeners
CAudioObjectTransformation g_listenerPosition;
bool g_bListenerPosChanged;
bool g_bMuted;

SoundEngine::FnEventCallback g_fnEventFinishedCallback;
SoundEngine::FnStandaloneFileCallback g_fnStandaloneFileFinishedCallback;

void SoundEngine::RegisterEventFinishedCallback(FnEventCallback pCallbackFunction)
{
	g_fnEventFinishedCallback = pCallbackFunction;
}

void SoundEngine::RegisterStandaloneFileFinishedCallback(FnStandaloneFileCallback pCallbackFunction)
{
	g_fnStandaloneFileFinishedCallback = pCallbackFunction;
}

void EventFinishedPlaying(AudioEventId nEventID)
{
	if (g_fnEventFinishedCallback)
	{
		g_fnEventFinishedCallback(nEventID);
	}
}

void StandaloneFileFinishedPlaying(AudioStandaloneFileId const fileInstanceId, char const* szFileName)
{
	if (g_fnStandaloneFileFinishedCallback)
	{
		g_fnStandaloneFileFinishedCallback(fileInstanceId, szFileName);
	}
}

void SoundEngine::UnloadSample(const SampleId nID)
{
	Mix_Chunk* pSample = stl::find_in_map(g_sampleData, nID, nullptr);
	if (pSample != nullptr)
	{
		Mix_FreeChunk(pSample);
		stl::member_find_and_erase(g_sampleData, nID);
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Could not find sample with id %d", nID);
	}
}

void ProcessChannelFinishedRequests(ChannelFinishedRequests& queue)
{
	if (!queue.empty())
	{
		for (const int finishedChannelId : queue)
		{
			SAudioObject* pAudioObject = g_channels[finishedChannelId].pAudioObject;
			if (pAudioObject)
			{
				EventInstanceList::iterator eventsEnd = pAudioObject->events.end();
				for (EventInstanceList::iterator eventsIt = pAudioObject->events.begin(); eventsIt != eventsEnd; )
				{
					SAudioEvent* pEventInstance = *eventsIt;
					EventInstanceList::iterator eventsCurrent = eventsIt;
					++eventsIt;
					if (pEventInstance)
					{
						const ChannelList::iterator channelsEnd = pEventInstance->channels.end();
						for (ChannelList::iterator channelIt = pEventInstance->channels.begin(); channelIt != channelsEnd; ++channelIt)
						{
							if (*channelIt == finishedChannelId)
							{
								pEventInstance->channels.erase(channelIt);
								if (pEventInstance->channels.empty())
								{
									eventsIt = pAudioObject->events.erase(eventsCurrent);
									eventsEnd = pAudioObject->events.end();
									EventFinishedPlaying(pEventInstance->eventId);
								}
								break;
							}
						}
					}
				}
				StandAloneFileInstanceList::iterator standaloneFilesEnd = pAudioObject->standaloneFiles.end();
				for (StandAloneFileInstanceList::iterator standaloneFilesIt = pAudioObject->standaloneFiles.begin(); standaloneFilesIt != standaloneFilesEnd; )
				{
					CAudioStandaloneFile* pStandaloneFileInstance = *standaloneFilesIt;
					StandAloneFileInstanceList::iterator standaloneFilesCurrent = standaloneFilesIt;
					++standaloneFilesIt;
					if (pStandaloneFileInstance)
					{
						const ChannelList::iterator channelsEnd = pStandaloneFileInstance->channels.end();
						for (ChannelList::iterator channelIt = pStandaloneFileInstance->channels.begin(); channelIt != channelsEnd; ++channelIt)
						{
							if (*channelIt == finishedChannelId)
							{
								pStandaloneFileInstance->channels.erase(channelIt);
								if (pStandaloneFileInstance->channels.empty())
								{
									standaloneFilesIt = pAudioObject->standaloneFiles.erase(standaloneFilesCurrent);
									standaloneFilesEnd = pAudioObject->standaloneFiles.end();
									StandaloneFileFinishedPlaying(pStandaloneFileInstance->fileInstanceId, pStandaloneFileInstance->fileName.c_str());

									SampleIdUsageCounterMap::iterator it = g_usageCounters.find(pStandaloneFileInstance->fileId);
									CRY_ASSERT(it != g_usageCounters.end() && it->second > 0);
									if (--it->second == 0)
									{
										SoundEngine::UnloadSample(pStandaloneFileInstance->fileInstanceId);
									}
								}
								break;
							}
						}
					}
				}
				g_channels[finishedChannelId].pAudioObject = nullptr;
			}
			g_freeChannels.push(finishedChannelId);
		}
		queue.clear();
	}
}

void ChannelFinishedPlaying(int nChannel)
{
	if (nChannel >= 0 && nChannel < g_nNumMixChannels)
	{
		CryAutoLock<CryCriticalSection> autoLock(g_channelFinishedCriticalSection);
		g_channelFinishedRequests[eChannelFinishedRequestQueueId_One].push_back(nChannel);
	}
}

void LoadMetadata(const string& sdlMixerAssetPath)
{
	g_sampleDataRootDir = PathUtil::GetPath(sdlMixerAssetPath);
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(sdlMixerAssetPath + "*.*", &fd);
	if (handle != -1)
	{
		do
		{
			const string name = fd.name;
			if (name != "." && name != ".." && !name.empty())
			{
				if (name.find(".wav") != string::npos ||
				    name.find(".ogg") != string::npos ||
				    name.find(".mp3") != string::npos)
				{
					// For now there's a 1 to 1 mapping between sample files and events
					g_samplePaths[GetIDFromFilePath(name)] = g_sampleDataRootDir + name;
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);
		pCryPak->FindClose(handle);
	}
}

bool SoundEngine::Init()
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "SDL::SDL_Init() returned: %s", SDL_GetError());
		return false;
	}

	int loadedFormats = Mix_Init(g_nSupportedFormats);
	if ((loadedFormats & g_nSupportedFormats) != g_nSupportedFormats)
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "SDLMixer::Mix_Init() failed to init support for format flags %d with error \"%s\"", g_nSupportedFormats, Mix_GetError());
		return false;
	}

	if (Mix_OpenAudio(g_sampleRate, MIX_DEFAULT_FORMAT, 2, g_bufferSize) < 0)
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "SDLMixer::Mix_OpenAudio() failed to init the SDL Mixer API with error \"%s\"", Mix_GetError());
		return false;
	}

	Mix_AllocateChannels(g_nNumMixChannels);

	g_bMuted = false;
	Mix_Volume(-1, SDL_MIX_MAXVOLUME);
	for (int i = 0; i < SDL_MIXER_NUM_CHANNELS; ++i)
	{
		g_freeChannels.push(i);
	}

	Mix_ChannelFinished(ChannelFinishedPlaying);

	LoadMetadata(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR SDL_MIXER_PROJECT_PATH);
	g_bListenerPosChanged = false;

	// need to reinit as the global variable might have been initialized with wrong values
	g_listenerPosition = CAudioObjectTransformation();

	return true;
}

void FreeAllSampleData()
{
	Mix_HaltChannel(-1);
	SampleDataMap::const_iterator it = g_sampleData.begin();
	SampleDataMap::const_iterator end = g_sampleData.end();
	for (; it != end; ++it)
	{
		Mix_FreeChunk(it->second);
	}
	g_sampleData.clear();
}

void SoundEngine::Release()
{
	FreeAllSampleData();
	g_audioObjects.clear();
	Mix_Quit();
	Mix_CloseAudio();
	SDL_Quit();
}

void SoundEngine::Refresh()
{
	FreeAllSampleData();
	LoadMetadata(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR SDL_MIXER_PROJECT_PATH);
}

const SampleId SoundEngine::LoadSampleFromMemory(void* pMemory, const size_t size, const string& samplePath, const SampleId overrideId)
{
	const SampleId id = (overrideId != 0) ? overrideId : GetIDFromFilePath(samplePath);
	Mix_Chunk* pSample = stl::find_in_map(g_sampleData, id, nullptr);
	if (pSample != nullptr)
	{
		Mix_FreeChunk(pSample);
		g_audioImplLogger.Log(eAudioLogType_Warning, "Loading sample %s which had already been loaded", samplePath.c_str());
	}
	SDL_RWops* pData = SDL_RWFromMem(pMemory, size);
	if (pData)
	{
		pSample = Mix_LoadWAV_RW(pData, 0);
		if (pSample != nullptr)
		{
			g_sampleData[id] = pSample;
			g_samplePaths[id] = samplePath;
			return id;
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "SDL Mixer failed to load sample. Error: \"%s\"", Mix_GetError());
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "SDL Mixer failed to transform the audio data. Error: \"%s\"", SDL_GetError());
	}
	return SDL_MIXER_INVALID_SAMPLE_ID;
}

bool LoadSampleImpl(const SampleId id, const string& samplePath)
{
	bool bSuccess = true;
	Mix_Chunk* pSample = Mix_LoadWAV(samplePath.c_str());
	if (pSample != nullptr)
	{
#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
		SampleNameMap::const_iterator it = g_samplePaths.find(id);
		if (it != g_samplePaths.end() && it->second != samplePath)
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Loaded a Sample with the already existing ID %u, but from a different path source path '%s' <-> '%s'.", static_cast<uint>(id), it->second.c_str(), samplePath.c_str());
		}
		if (stl::find_in_map(g_sampleData, id, nullptr) != nullptr)
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Loading sample '%s' which had already been loaded", samplePath.c_str());
		}
#endif
		g_sampleData[id] = pSample;
		g_samplePaths[id] = samplePath;
	}
	else
	{
		// Sample could be inside a pak file so we need to open it manually and load it from the raw file
		const size_t nFileSize = gEnv->pCryPak->FGetSize(samplePath);
		FILE* const pFile = gEnv->pCryPak->FOpen(samplePath, "rbx", ICryPak::FOPEN_HINT_DIRECT_OPERATION);
		if (pFile && nFileSize > 0)
		{
			void* const pData = AUDIO_ALLOCATOR_MEMORY_POOL.AllocateRaw(nFileSize, AUDIO_MEMORY_ALIGNMENT, "SDLMixerSample");
			gEnv->pCryPak->FReadRawAll(pData, nFileSize, pFile);
			const SampleId nNewID = SoundEngine::LoadSampleFromMemory(pData, nFileSize, samplePath, id);
			if (nNewID == SDL_MIXER_INVALID_SAMPLE_ID)
			{
				g_audioImplLogger.Log(eAudioLogType_Error, "SDL Mixer failed to load sample %s. Error: \"%s\"", samplePath.c_str(), Mix_GetError());
				bSuccess = false;
			}
			AUDIO_ALLOCATOR_MEMORY_POOL.Free(pData);
		}
	}
	return bSuccess;
}

const SampleId SoundEngine::LoadSample(const string& sampleFilePath, bool bOnlyMetadata)
{
	const SampleId id = GetIDFromFilePath(sampleFilePath);
	if (stl::find_in_map(g_sampleData, id, nullptr) == nullptr)
	{
		if (bOnlyMetadata)
		{
			g_samplePaths[id] = g_sampleDataRootDir + sampleFilePath;
		}
		else if (!LoadSampleImpl(id, sampleFilePath))
		{
			return SDL_MIXER_INVALID_SAMPLE_ID;
		}
	}
	return id;
}

void SoundEngine::Pause()
{
	Mix_Pause(-1);
	Mix_PauseMusic();
}

void SoundEngine::Resume()
{
	Mix_Resume(-1);
	Mix_ResumeMusic();
}

void SoundEngine::Stop()
{
	Mix_HaltChannel(-1);
}

void SoundEngine::Mute()
{
	Mix_Volume(-1, 0);
	g_bMuted = true;
}

void SoundEngine::UnMute()
{
	AudioObjectList::const_iterator audioObjectIt = g_audioObjects.begin();
	const AudioObjectList::const_iterator audioObjectEnd = g_audioObjects.end();
	for (; audioObjectIt != audioObjectEnd; ++audioObjectIt)
	{
		SAudioObject* pAudioObject = *audioObjectIt;
		if (pAudioObject)
		{
			EventInstanceList::const_iterator eventIt = pAudioObject->events.begin();
			const EventInstanceList::const_iterator eventEnd = pAudioObject->events.end();
			for (; eventIt != eventEnd; ++eventIt)
			{
				SAudioEvent* pEventInstance = *eventIt;
				if (pEventInstance)
				{
					ChannelList::const_iterator channelIt = pEventInstance->channels.begin();
					ChannelList::const_iterator channelEnd = pEventInstance->channels.end();
					for (; channelIt != channelEnd; ++channelIt)
					{
						Mix_Volume(*channelIt, pEventInstance->pStaticData->volume);
					}
				}
			}
		}
	}
	g_bMuted = false;
}

void SetChannelPosition(const SAudioTrigger* pStaticData, const int channelID, const float distance, const float angle)
{
	static const uint8 sdlMaxDistance = 255;
	const float min = pStaticData->attenuationMinDistance;
	const float max = pStaticData->attenuationMaxDistance;
	if (min <= max)
	{
		uint8 nDistance = 0;
		if (max >= 0.0f && distance > min)
		{
			if (min != max)
			{
				const float finalDistance = distance - min;
				const float range = max - min;
				nDistance = static_cast<uint8>((std::min((finalDistance / range), 1.0f) * sdlMaxDistance) + 0.5f);
			}
			else
			{
				nDistance = sdlMaxDistance;
			}
		}
		//Temp code, to be reviewed during the SetChannelPosition rewrite:
		Mix_SetDistance(channelID, nDistance);

		if (pStaticData->bPanningEnabled)
		{
			//Temp code, to be reviewed during the SetChannelPosition rewrite:
			float const absAngle = fabs(angle);
			float const frontAngle = (angle > 0.0f ? 1.0f : -1.0f) * (absAngle > 90.0f ? 180.f - absAngle : absAngle);
			float const rightVolume = (frontAngle + 90.0f) / 180.0f;
			float const leftVolume = 1.0f - rightVolume;
			Mix_SetPanning(channelID,
			               static_cast<uint8>(255.0f * leftVolume),
			               static_cast<uint8>(255.0f * rightVolume));
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "The minimum attenuation distance value is higher than the maximum");
	}
}

bool SoundEngine::StopEvent(SAudioEvent const* const pEventInstance)
{
	if (pEventInstance)
	{
		// need to make a copy because the callback
		// registered with Mix_ChannelFinished can edit the list
		ChannelList channels = pEventInstance->channels;
		for (int channel : channels)
		{
			Mix_HaltChannel(channel);
		}
		return true;
	}
	return false;
}

bool SoundEngine::ExecuteEvent(SAudioObject* const pAudioObject, SAudioTrigger const* const pEventStaticData, SAudioEvent* const pEventInstance)
{
	bool bSuccess = false;

	if (pAudioObject && pEventStaticData && pEventInstance)
	{
		if (pEventStaticData->bStartEvent)
		{
			// start playing samples
			pEventInstance->pStaticData = pEventStaticData;

			Mix_Chunk* pSample = stl::find_in_map(g_sampleData, pEventStaticData->sampleId, nullptr);
			if (pSample == nullptr)
			{
				// Trying to play sample that hasn't been loaded yet, load it in place
				// NOTE: This should be avoided as it can cause lag in audio playback
				const string& samplePath = g_samplePaths[pEventStaticData->sampleId];
				if (LoadSampleImpl(pEventStaticData->sampleId, samplePath))
				{
					pSample = stl::find_in_map(g_sampleData, pEventStaticData->sampleId, nullptr);
				}
				if (pSample == nullptr)
				{
					return false;
				}
			}

			int loopCount = pEventStaticData->loopCount;
			if (loopCount > 0)
			{
				// For SDL Mixer 0 loops means play only once, 1 loop play twice, etc ...
				--loopCount;
			}

			if (!g_freeChannels.empty())
			{
				int channelID = Mix_PlayChannel(g_freeChannels.front(), pSample, loopCount);
				if (channelID >= 0)
				{
					g_freeChannels.pop();
					Mix_Volume(channelID, g_bMuted ? 0 : pEventStaticData->volume);

					// Get distance and angle from the listener to the audio object
					float distance = 0.0f;
					float angle = 0.0f;
					GetDistanceAngleToObject(g_listenerPosition, pAudioObject->position, distance, angle);
					SetChannelPosition(pEventInstance->pStaticData, channelID, distance, angle);

					g_channels[channelID].pAudioObject = pAudioObject;
					pEventInstance->channels.push_back(channelID);
				}
				else
				{
					g_audioImplLogger.Log(eAudioLogType_Error, "Could not play sample. Error: %s", Mix_GetError());
				}
			}
			else
			{
				g_audioImplLogger.Log(eAudioLogType_Error, "Ran out of free audio channels. Are you trying to play more than %d samples?", SDL_MIXER_NUM_CHANNELS);
			}

			if (!pEventInstance->channels.empty())
			{
				// if any sample was added then add the event to the audio object
				pAudioObject->events.push_back(pEventInstance);
				bSuccess = true;
			}
		}
		else
		{
			// stop event in audio object
			const SampleId id = pEventStaticData->sampleId;
			for (SAudioEvent* pEvent : pAudioObject->events)
			{
				if (pEvent && (id == pEvent->pStaticData->sampleId))
				{
					SoundEngine::StopEvent(pEvent);
				}
			}
		}
	}

	return bSuccess;
}

bool SoundEngine::PlayFile(
	SAudioObject* const pAudioObject,
	CAudioStandaloneFile* const pEventInstance,
	const SAudioTrigger* const pUsedTrigger,
	const char* const szFilePath)
{
	if (!pUsedTrigger)
	{
		return false;
	}

	SampleId idForThisFile = pEventInstance->fileId;
	Mix_Chunk* pSample = stl::find_in_map(g_sampleData, idForThisFile, nullptr);

	if (!pSample)
	{
		if (LoadSampleImpl(idForThisFile, szFilePath))
		{
			pSample = stl::find_in_map(g_sampleData, idForThisFile, nullptr);
		}
		if (!pSample)
		{
			return false;
		}
	}

	SampleIdUsageCounterMap::iterator it = g_usageCounters.find(idForThisFile);
	if (it != g_usageCounters.end())
	{
		++it->second;
	}
	else
	{
		g_usageCounters[idForThisFile] = 1;
	}

	if (!g_freeChannels.empty())
	{
		int loopCount = pUsedTrigger->loopCount;
		if (loopCount > 0)
		{
			// For SDL Mixer 0 loops means play only once, 1 loop play twice, etc ...
			--loopCount;
		}

		int channelId = Mix_PlayChannel(g_freeChannels.front(), pSample, loopCount);
		if (channelId >= 0)
		{
			g_freeChannels.pop();
			Mix_Volume(channelId, g_bMuted ? 0 : pUsedTrigger->volume);

			// Get distance and angle from the listener to the audio object
			float distance = 0.0f;
			float angle = 0.0f;
			GetDistanceAngleToObject(g_listenerPosition, pAudioObject->position, distance, angle);
			SetChannelPosition(pUsedTrigger, channelId, distance, angle);

			g_channels[channelId].pAudioObject = pAudioObject;
			pEventInstance->channels.push_back(channelId);
		}
		else
		{
			g_audioImplLogger.Log(eAudioLogType_Error, "Could not play sample. Error: %s", Mix_GetError());
		}
	}
	else
	{
		g_audioImplLogger.Log(eAudioLogType_Error, "Ran out of free audio channels. Are you trying to play more than %d samples?", SDL_MIXER_NUM_CHANNELS);
	}

	if (!pEventInstance->channels.empty())
	{
		// if any sample was added then add the event to the audio object
		pAudioObject->standaloneFiles.push_back(pEventInstance);
	}

	return true;
}

bool SoundEngine::StopFile(SAudioObject* const pAudioObject, AudioStandaloneFileId const fileInstanceID)
{
	bool bResult = false;
	if (pAudioObject)
	{
		for (CAudioStandaloneFile* const standaloneFileEvent : pAudioObject->standaloneFiles)
		{
			if (standaloneFileEvent->fileInstanceId = fileInstanceID)
			{
				// need to make a copy because the callback
				// registered with Mix_ChannelFinished can edit the list
				ChannelList channels = standaloneFileEvent->channels;
				for (int channel : channels)
				{
					Mix_HaltChannel(channel);
				}
				bResult = true;
			}
		}
	}
	return bResult;
}

bool SoundEngine::SetListenerPosition(const ListenerId listenerId, const CAudioObjectTransformation& position)
{
	g_listenerPosition = position;
	g_bListenerPosChanged = true;
	return true;
}

bool SoundEngine::RegisterAudioObject(SAudioObject* pAudioObjectData)
{
	if (pAudioObjectData)
	{
		g_audioObjects.push_back(pAudioObjectData);
		return true;
	}
	return false;
}

bool SoundEngine::UnregisterAudioObject(SAudioObject* pAudioObjectData)
{
	if (pAudioObjectData)
	{
		stl::find_and_erase(g_audioObjects, pAudioObjectData);
		return true;
	}
	return false;
}

bool SoundEngine::SetAudioObjectPosition(SAudioObject* pAudioObjectData, const CAudioObjectTransformation& position)
{
	if (pAudioObjectData)
	{
		pAudioObjectData->position = position;
		pAudioObjectData->bPositionChanged = true;
		return true;
	}
	return false;
}

bool SoundEngine::StopTrigger(SAudioTrigger const* const pEventData)
{
	bool bResult = false;
	AudioObjectList::const_iterator audioObjectIt = g_audioObjects.begin();
	const AudioObjectList::const_iterator audioObjectEnd = g_audioObjects.end();
	for (; audioObjectIt != audioObjectEnd; ++audioObjectIt)
	{
		SAudioObject* pAudioObject = *audioObjectIt;
		if (pAudioObject)
		{
			EventInstanceList::const_iterator eventIt = pAudioObject->events.begin();
			const EventInstanceList::const_iterator eventEnd = pAudioObject->events.end();
			for (; eventIt != eventEnd; ++eventIt)
			{
				SAudioEvent* pEventInstance = *eventIt;
				if (pEventInstance && pEventInstance->pStaticData == pEventData)
				{
					SoundEngine::StopEvent(pEventInstance);
					bResult = true;
				}
			}
		}
	}
	return bResult;
}

SAudioTrigger* SoundEngine::CreateEventData()
{
	SAudioTrigger* pNewTriggerImpl = nullptr;
	POOL_NEW(SAudioTrigger, pNewTriggerImpl)();
	return pNewTriggerImpl;
}

void SoundEngine::Update()
{
	ProcessChannelFinishedRequests(g_channelFinishedRequests[eChannelFinishedRequestQueueId_Two]);
	{
		CryAutoLock<CryCriticalSection> oAutoLock(g_channelFinishedCriticalSection);
		g_channelFinishedRequests[eChannelFinishedRequestQueueId_One].swap(g_channelFinishedRequests[eChannelFinishedRequestQueueId_Two]);
	}

	AudioObjectList::const_iterator audioObjectIt = g_audioObjects.begin();
	const AudioObjectList::const_iterator audioObjectEnd = g_audioObjects.end();
	for (; audioObjectIt != audioObjectEnd; ++audioObjectIt)
	{
		SAudioObject* pAudioObject = *audioObjectIt;
		if (pAudioObject && (pAudioObject->bPositionChanged || g_bListenerPosChanged))
		{
			// Get distance and angle from the listener to the audio object
			float distance = 0.0f;
			float angle = 0.0f;
			GetDistanceAngleToObject(g_listenerPosition, pAudioObject->position, distance, angle);

			EventInstanceList::const_iterator eventIt = pAudioObject->events.begin();
			const EventInstanceList::const_iterator eventEnd = pAudioObject->events.end();
			for (; eventIt != eventEnd; ++eventIt)
			{
				SAudioEvent* pEventInstance = *eventIt;
				if (pEventInstance && pEventInstance->pStaticData)
				{
					for (int channelIndex : pEventInstance->channels)
					{
						SetChannelPosition(pEventInstance->pStaticData, channelIndex, distance, angle);
					}
				}
			}
			pAudioObject->bPositionChanged = false;
		}
	}
	g_bListenerPosChanged = false;
}
