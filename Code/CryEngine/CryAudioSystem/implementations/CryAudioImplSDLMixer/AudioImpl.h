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
class CImpl final : public IImpl
{
public:

	CImpl();
	CImpl(CImpl const&) = delete;
	CImpl(CImpl&&) = delete;
	CImpl& operator=(CImpl const&) = delete;
	CImpl& operator=(CImpl&&) = delete;

	// CryAudio::Impl::IImpl
	virtual void                Update(float const deltaTime) override;
	virtual ERequestStatus      Init(uint32 const objectPoolSize, uint32 const eventPoolSize) override;
	virtual ERequestStatus      OnBeforeShutDown() override;
	virtual ERequestStatus      ShutDown() override;
	virtual ERequestStatus      Release() override;
	virtual void                OnRefresh() override;
	virtual ERequestStatus      OnLoseFocus() override;
	virtual ERequestStatus      OnGetFocus() override;
	virtual ERequestStatus      MuteAll() override;
	virtual ERequestStatus      UnmuteAll() override;
	virtual ERequestStatus      StopAllSounds() override;
	virtual ERequestStatus      RegisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus      UnregisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus      ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo) override;
	virtual void                DestructFile(IFile* const pIFile) override;
	virtual char const* const   GetFileLocation(SFileInfo* const pFileInfo) override;
	virtual ITrigger const*     ConstructTrigger(XmlNodeRef const pRootNode) override;
	virtual void                DestructTrigger(ITrigger const* const pITrigger) override;
	virtual IParameter const*   ConstructParameter(XmlNodeRef const pRootNode) override;
	virtual void                DestructParameter(IParameter const* const pIParameter) override;
	virtual ISwitchState const* ConstructSwitchState(XmlNodeRef const pRootNode) override;
	virtual void                DestructSwitchState(ISwitchState const* const pISwitchState) override;
	virtual IEnvironment const* ConstructEnvironment(XmlNodeRef const pRootNode) override;
	virtual void                DestructEnvironment(IEnvironment const* const pIEnvironment) override;
	virtual IObject*            ConstructGlobalObject() override;
	virtual IObject*            ConstructObject(char const* const szName = nullptr) override;
	virtual void                DestructObject(IObject const* const pIObject) override;
	virtual IListener*          ConstructListener(char const* const szName = nullptr) override;
	virtual void                DestructListener(IListener* const pIListener) override;
	virtual IEvent*             ConstructEvent(CATLEvent& event) override;
	virtual void                DestructEvent(IEvent const* const pIEvent) override;
	virtual IStandaloneFile*    ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger = nullptr) override;
	virtual void                DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile) override;
	virtual void                GamepadConnected(DeviceId const deviceUniqueID) override;
	virtual void                GamepadDisconnected(DeviceId const deviceUniqueID) override;
	virtual void                SetLanguage(char const* const szLanguage) override;

	// Below data is only used when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	virtual char const* const GetName() const override;
	virtual void              GetMemoryInfo(SMemoryInfo& memoryInfo) const override;
	virtual void              GetFileData(char const* const szName, SFileData& fileData) const override;
	// ~CryAudio::Impl::IImpl

private:

	static char const* const s_szSDLFileTag;
	static char const* const s_szSDLEventTag;
	static char const* const s_szSDLCommonAttribute;
	static char const* const s_szSDLPathAttribute;
	static char const* const s_szSDLSoundLibraryPath;
	static char const* const s_szSDLEventTypeTag;
	static char const* const s_szSDLEventPanningEnabledTag;
	static char const* const s_szSDLEventAttenuationEnabledTag;
	static char const* const s_szSDLEventAttenuationMinDistanceTag;
	static char const* const s_szSDLEventAttenuationMaxDistanceTag;
	static char const* const s_szSDLEventVolumeTag;
	static char const* const s_szSDLEventLoopCountTag;
	static char const* const s_szSDLEventIdTag;

	size_t                   m_memoryAlignment;
	string                   m_language;

	ICVar*                   m_pCVarFileExtension;

#if defined(INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE)
	std::map<uint32, string>           m_idToName;
	CryFixedStringT<MaxFilePathLength> m_name;
#endif  // INCLUDE_SDLMIXER_IMPL_PRODUCTION_CODE

};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
