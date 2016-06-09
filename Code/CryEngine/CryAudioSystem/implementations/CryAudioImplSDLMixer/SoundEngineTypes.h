// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioImpl.h>
#include <STLSoundAllocator.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
typedef uint                                     SampleId;
typedef uint                                     ListenerId;
typedef std::vector<int, STLSoundAllocator<int>> ChannelList;

struct SAudioTrigger final : public IAudioTrigger
{
	explicit SAudioTrigger()
		: sampleId(0)
		, attenuationMinDistance(0.0f)
		, attenuationMaxDistance(100.0f)
		, volume(128)
		, loopCount(1)
		, bPanningEnabled(true)
		, bStartEvent(true)
	{}

	SampleId sampleId;
	float              attenuationMinDistance;
	float              attenuationMaxDistance;
	int                volume;
	int                loopCount;
	bool               bPanningEnabled;
	bool               bStartEvent;
};

struct SAudioParameter final : public IAudioRtpc
{
	// Empty implementation so that the engine has something
	// to refer to since RTPCs are not currently supported by
	// the SDL Mixer implementation
};

struct SAudioSwitchState final : public IAudioSwitchState
{
	// Empty implementation so that the engine has something
	// to refer to since switches are not currently supported by
	// the SDL Mixer implementation
};

struct SAudioEnvironment final : public IAudioEnvironment
{
	// Empty implementation so that the engine has something
	// to refer to since environments are not currently supported by
	// the SDL Mixer implementation
};

struct SAudioEvent final : public IAudioEvent
{
	explicit SAudioEvent(AudioEventId const passedId)
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
	const SAudioTrigger* pStaticData;
};

class CAudioStandaloneFile final : public IAudioStandaloneFile
{
public:
	CAudioStandaloneFile() : fileId(INVALID_AUDIO_STANDALONE_FILE_ID), fileInstanceId(INVALID_AUDIO_STANDALONE_FILE_ID) {}

	void Reset()
	{
		fileId = INVALID_AUDIO_STANDALONE_FILE_ID;
		fileInstanceId = INVALID_AUDIO_STANDALONE_FILE_ID;
		channels.clear();
		fileName.clear();
	}

	AudioStandaloneFileId                       fileId;               // ID unique to the file, only needed for the 'finished' request
	AudioStandaloneFileId                       fileInstanceId;       // ID unique to the file instance, only needed for the 'finished' request
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> fileName;
	ChannelList channels;
};

typedef std::vector<SAudioEvent*, STLSoundAllocator<SAudioEvent*>>               EventInstanceList;
typedef std::vector<CAudioStandaloneFile*, STLSoundAllocator<CAudioStandaloneFile*>> StandAloneFileInstanceList;

struct SAudioObject final : public IAudioObject
{
	SAudioObject(AudioObjectId id, bool bIsGlobal)
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

struct SAudioListener final : public IAudioListener
{
	explicit SAudioListener(const ListenerId id)
		: listenerId(id)
	{}

	const ListenerId listenerId;
};

struct SAudioFileEntry final : public IAudioFileEntry
{
	SAudioFileEntry()
	{}

	SampleId sampleId;
};

}
}
}
