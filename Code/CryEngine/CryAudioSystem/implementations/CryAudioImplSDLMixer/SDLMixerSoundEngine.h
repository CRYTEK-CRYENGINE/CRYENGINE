// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SDLMixerSoundEngineTypes.h"

namespace SdlMixer
{
namespace SoundEngine
{
typedef void (* FnEventCallback)(AudioEventId);
typedef void (* FnStandaloneFileCallback)(AudioStandaloneFileId const, char const*);

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
CryAudio::Impl::SAtlTriggerImplData_sdlmixer* CreateEventData();
bool                                          ExecuteEvent(CryAudio::Impl::SAtlAudioObjectData_sdlmixer* const pAudioObject, CryAudio::Impl::SAtlTriggerImplData_sdlmixer const* const pEventStaticData, CryAudio::Impl::SAtlEventData_sdlmixer* const pEventInstance);
bool                                          PlayFile(CryAudio::Impl::SAtlAudioObjectData_sdlmixer* const pAudioObject, CryAudio::Impl::CAudioStandaloneFile_sdlmixer* const pEventInstance, const CryAudio::Impl::SAtlTriggerImplData_sdlmixer* const pUsedTrigger, const char* const szFilePath);
bool                                          StopFile(CryAudio::Impl::SAtlAudioObjectData_sdlmixer* const pAudioObject, AudioStandaloneFileId const fileInstanceID);

// stops an specific event instance
bool StopEvent(CryAudio::Impl::SAtlEventData_sdlmixer const* const pEventInstance);
// stops all the events associated with this trigger
bool StopTrigger(CryAudio::Impl::SAtlTriggerImplData_sdlmixer const* const pEventData);

// Listeners
bool SetListenerPosition(const ListenerId listenerId, const CAudioObjectTransformation& position);

// Audio Objects
bool RegisterAudioObject(CryAudio::Impl::SAtlAudioObjectData_sdlmixer* pAudioObjectData);
bool UnregisterAudioObject(CryAudio::Impl::SAtlAudioObjectData_sdlmixer* pAudioObjectData);
bool SetAudioObjectPosition(CryAudio::Impl::SAtlAudioObjectData_sdlmixer* pAudioObjectData, const CAudioObjectTransformation& position);

// Callbacks
void RegisterEventFinishedCallback(FnEventCallback pCallbackFunction);
void RegisterStandaloneFileFinishedCallback(FnStandaloneFileCallback pCallbackFunction);
}
}
