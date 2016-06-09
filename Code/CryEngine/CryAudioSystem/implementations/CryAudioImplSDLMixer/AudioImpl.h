// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioImpl.h>
#include "SoundEngine.h"

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CAudioImpl final : public IAudioImpl
{
public:

	CAudioImpl();
	virtual ~CAudioImpl();

	// IAudioImpl
	virtual void                     Update(float const deltaTime) override;
	virtual EAudioRequestStatus      Init() override;
	virtual EAudioRequestStatus      ShutDown() override;
	virtual EAudioRequestStatus      Release() override;
	virtual void                     OnAudioSystemRefresh() override;
	virtual EAudioRequestStatus      OnLoseFocus() override;
	virtual EAudioRequestStatus      OnGetFocus() override;
	virtual EAudioRequestStatus      MuteAll() override;
	virtual EAudioRequestStatus      UnmuteAll() override;
	virtual EAudioRequestStatus      StopAllSounds() override;
	virtual EAudioRequestStatus      RegisterAudioObject(IAudioObject* const pAudioObject, char const* const szAudioObjectName) override;
	virtual EAudioRequestStatus      RegisterAudioObject(IAudioObject* const pAudioObject) override;
	virtual EAudioRequestStatus      UnregisterAudioObject(IAudioObject* const pAudioObject) override;
	virtual EAudioRequestStatus      ResetAudioObject(IAudioObject* const pAudioObject) override;
	virtual EAudioRequestStatus      UpdateAudioObject(IAudioObject* const pAudioObject) override;
	virtual EAudioRequestStatus      PlayFile(SAudioStandaloneFileInfo* const pAudioStandaloneFileInfo) override;
	virtual EAudioRequestStatus      StopFile(SAudioStandaloneFileInfo* const pAudioStandaloneFileInfo) override;
	virtual EAudioRequestStatus      PrepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger) override;
	virtual EAudioRequestStatus      UnprepareTriggerSync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger) override;
	virtual EAudioRequestStatus      PrepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent) override;
	virtual EAudioRequestStatus      UnprepareTriggerAsync(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent) override;
	virtual EAudioRequestStatus      ActivateTrigger(IAudioObject* const pAudioObject, IAudioTrigger const* const pAudioTrigger, IAudioEvent* const pAudioEvent) override;
	virtual EAudioRequestStatus      StopEvent(IAudioObject* const pAudioObject, IAudioEvent const* const pAudioEvent) override;
	virtual EAudioRequestStatus      StopAllEvents(IAudioObject* const pAudioObject) override;
	virtual EAudioRequestStatus      Set3DAttributes(IAudioObject* const pAudioObject, SAudioObject3DAttributes const& attributes) override;
	virtual EAudioRequestStatus      SetEnvironment(IAudioObject* const pAudioObject, IAudioEnvironment const* const pAudioEnvironment, float const amount) override;
	virtual EAudioRequestStatus      SetRtpc(IAudioObject* const pAudioObject, IAudioRtpc const* const pAudioRtpc, float const value) override;
	virtual EAudioRequestStatus      SetSwitchState(IAudioObject* const pAudioObject, IAudioSwitchState const* const pAudioSwitchState) override;
	virtual EAudioRequestStatus      SetObstructionOcclusion(IAudioObject* const pAudioObject, float const obstruction, float const occlusion) override;
	virtual EAudioRequestStatus      SetListener3DAttributes(IAudioListener* const pAudioListener, SAudioObject3DAttributes const& attributes) override;
	virtual EAudioRequestStatus      RegisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry) override;
	virtual EAudioRequestStatus      UnregisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry) override;
	virtual EAudioRequestStatus      ParseAudioFileEntry(XmlNodeRef const pAudioFileEntryNode, SAudioFileEntryInfo* const pFileEntryInfo) override;
	virtual void                     DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry) override;
	virtual char const* const        GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo) override;
	virtual IAudioTrigger const*     NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info) override;
	virtual void                     DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger) override;
	virtual IAudioRtpc const*        NewAudioRtpc(XmlNodeRef const pAudioRtpcNode) override;
	virtual void                     DeleteAudioRtpc(IAudioRtpc const* const pOldAudioRtpc) override;
	virtual IAudioSwitchState const* NewAudioSwitchState(XmlNodeRef const pAudioSwitchStateImplNode) override;
	virtual void                     DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState) override;
	virtual IAudioEnvironment const* NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode) override;
	virtual void                     DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment) override;
	virtual IAudioObject*            NewGlobalAudioObject(AudioObjectId const audioObjectID) override;
	virtual IAudioObject*            NewAudioObject(AudioObjectId const audioObjectID) override;
	virtual void                     DeleteAudioObject(IAudioObject const* const pOldAudioObject) override;
	virtual IAudioListener*          NewDefaultAudioListener(AudioObjectId const audioObjectId) override;
	virtual IAudioListener*          NewAudioListener(AudioObjectId const audioObjectId) override;
	virtual void                     DeleteAudioListener(IAudioListener* const pOldListenerData) override;
	virtual IAudioEvent*             NewAudioEvent(AudioEventId const audioEventID) override;
	virtual void                     DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent) override;
	virtual void                     ResetAudioEvent(IAudioEvent* const pAudioEvent) override;
	virtual IAudioStandaloneFile*    NewAudioStandaloneFile() override;
	virtual void                     DeleteAudioStandaloneFile(IAudioStandaloneFile const* const pOldAudioStandaloneFile) override;
	virtual void                     ResetAudioStandaloneFile(IAudioStandaloneFile* const pAudioStandaloneFile) override;
	virtual void                     GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID) override;
	virtual void                     GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID) override;
	virtual void                     SetLanguage(char const* const szLanguage) override;

	// Below data is only used when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	virtual char const* const GetImplementationNameString() const override;
	virtual void              GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const override;
	virtual void              GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const override;
	// ~IAudioImpl

private:

	static char const* const s_szSDLFileTag;
	static char const* const s_szSDLEventTag;
	static char const* const s_szSDLCommonAttribute;
	static char const* const s_szSDLSoundLibraryPath;
	static char const* const s_szSDLEventTypeTag;
	static char const* const s_szSDLEventPanningEnabledTag;
	static char const* const s_szSDLEventAttenuationEnabledTag;
	static char const* const s_szSDLEventAttenuationMinDistanceTag;
	static char const* const s_szSDLEventAttenuationMaxDistanceTag;
	static char const* const s_szSDLEventVolumeTag;
	static char const* const s_szSDLEventLoopCountTag;
	static char const* const s_szSDLEventIdTag;

	string                   m_gameFolder;
	size_t                   m_memoryAlignment;
	string                   m_language;

	ICVar*                   m_pCVarFileExtension;

#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	std::map<AudioObjectId, string>             m_idToName;
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> m_fullImplString;
#endif  // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE

};
}
}
}
