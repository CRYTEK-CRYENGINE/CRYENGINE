// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SoundEngineTypes.h"

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
namespace SoundEngine
{
typedef void (*FnEventCallback)(AudioEventId);
typedef void (*FnStandaloneFileCallback)(AudioStandaloneFileId const, char const*);

// Global events
bool Init();
void Release();
void Refresh();
void Update();

void Pause();
void Resume();

void Mute();
void UnMute();
void Stop();

// Load / Unload samples
const SampleId LoadSample(const string& sampleFilePath);
const SampleId LoadSampleFromMemory(void* pMemory, const size_t size, const string& samplePath, const SampleId id = 0);
void           UnloadSample(const SampleId id);

// Events
SAudioTrigger* CreateEventData();
bool           ExecuteEvent(SAudioObject* const pAudioObject, SAudioTrigger const* const pEventStaticData, SAudioEvent* const pEventInstance);
bool           PlayFile(SAudioObject* const pAudioObject, CAudioStandaloneFile* const pEventInstance, const SAudioTrigger* const pUsedTrigger, const char* const szFilePath);
bool           StopFile(SAudioObject* const pAudioObject, AudioStandaloneFileId const fileInstanceID);

// stops an specific event instance
bool StopEvent(SAudioEvent const* const pEventInstance);
// stops all the events associated with this trigger
bool StopTrigger(SAudioTrigger const* const pEventData);

// Listeners
bool SetListenerPosition(const ListenerId listenerId, const CAudioObjectTransformation& position);

// Audio Objects
bool RegisterAudioObject(SAudioObject* pAudioObjectData);
bool UnregisterAudioObject(SAudioObject* pAudioObjectData);
bool SetAudioObjectPosition(SAudioObject* pAudioObjectData, const CAudioObjectTransformation& position);

// Callbacks
void RegisterEventFinishedCallback(FnEventCallback pCallbackFunction);
void RegisterStandaloneFileFinishedCallback(FnStandaloneFileCallback pCallbackFunction);
}
}
}
}
