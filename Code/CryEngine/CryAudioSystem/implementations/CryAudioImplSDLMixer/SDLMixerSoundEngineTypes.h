// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioSystemImplementation.h>
#include <STLSoundAllocator.h>

namespace SdlMixer
{
typedef uint SampleId;
typedef uint ListenerId;
}

namespace CryAudio
{
namespace Impl
{

typedef std::vector<int, STLSoundAllocator<int>> ChannelList;

struct SAtlTriggerImplData_sdlmixer final : public IAudioTrigger
{
	explicit SAtlTriggerImplData_sdlmixer()
		: sampleId(0)
		, attenuationMinDistance(0.0f)
		, attenuationMaxDistance(100.0f)
		, volume(128)
		, loopCount(1)
		, bPanningEnabled(true)
		, bStartEvent(true)
	{}

	SdlMixer::SampleId sampleId;
	float              attenuationMinDistance;
	float              attenuationMaxDistance;
	int                volume;
	int                loopCount;
	bool               bPanningEnabled;
	bool               bStartEvent;
};

struct SAtlRtpcImplData_sdlmixer final : public IAudioRtpc
{
	// Empty implementation so that the engine has something
	// to refer to since RTPCs are not currently supported by
	// the SDL Mixer implementation
};

struct SAtlSwitchStateImplData_sdlmixer final : public IAudioSwitchState
{
	// Empty implementation so that the engine has something
	// to refer to since switches are not currently supported by
	// the SDL Mixer implementation
};

struct SAtlEnvironmentImplData_sdlmixer final : public IAudioEnvironment
{
	// Empty implementation so that the engine has something
	// to refer to since environments are not currently supported by
	// the SDL Mixer implementation
};

struct SAtlEventData_sdlmixer final : public IAudioEvent
{
	explicit SAtlEventData_sdlmixer(AudioEventId const passedId)
		: eventId(passedId)
		, pStaticData(nullptr)
	{}

	void Reset()
	{
		channels.clear();
		pStaticData = nullptr;
	}
	const AudioEventId                  eventId;
	ChannelList                         channels;
	const SAtlTriggerImplData_sdlmixer* pStaticData;
};

class CAudioStandaloneFile_sdlmixer final : public IAudioStandaloneFile
{
public:
	CAudioStandaloneFile_sdlmixer() : fileId(INVALID_AUDIO_STANDALONE_FILE_ID), fileInstanceId(INVALID_AUDIO_STANDALONE_FILE_ID) {}

	void Reset()
	{
		fileId = INVALID_AUDIO_STANDALONE_FILE_ID;
		fileInstanceId = INVALID_AUDIO_STANDALONE_FILE_ID;
		channels.clear();
		fileName.clear();
	}

	AudioStandaloneFileId                       fileId;         // ID unique to the file, only needed for the 'finished' request
	AudioStandaloneFileId                       fileInstanceId; // ID unique to the file instance, only needed for the 'finished' request
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> fileName;
	ChannelList channels;
};

typedef std::vector<SAtlEventData_sdlmixer*, STLSoundAllocator<SAtlEventData_sdlmixer*>>               EventInstanceList;
typedef std::vector<CAudioStandaloneFile_sdlmixer*, STLSoundAllocator<CAudioStandaloneFile_sdlmixer*>> StandAloneFileInstanceList;

struct SAtlAudioObjectData_sdlmixer final : public IAudioObject
{
	SAtlAudioObjectData_sdlmixer(AudioObjectId id, bool bIsGlobal)
		: audioObjectId(id)
		, bGlobal(bIsGlobal)
		, bPositionChanged(false) {}

	const AudioObjectId        audioObjectId;
	CAudioObjectTransformation position;
	EventInstanceList          events;
	StandAloneFileInstanceList standaloneFiles;
	bool                       bGlobal;
	bool                       bPositionChanged;
};

struct SAtlListenerData_sdlmixer final : public IAudioListener
{
	explicit SAtlListenerData_sdlmixer(const SdlMixer::ListenerId id)
		: listenerId(id)
	{}

	const SdlMixer::ListenerId listenerId;
};

struct SAtlAudioFileEntryData_sdlmixer final : public IAudioFileEntry
{
	SAtlAudioFileEntryData_sdlmixer()
	{}

	SdlMixer::SampleId sampleId;
};

struct SAtlAudioStandaloneFile_sdlmixer final : public IAudioStandaloneFile
{
	// Empty implementation so that the engine has something
	// to refer to since support for standalone audio files
	// is currently not implemented to the SDL Mixer implementation
};

}
}
