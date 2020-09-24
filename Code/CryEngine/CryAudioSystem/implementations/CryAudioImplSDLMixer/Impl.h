// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IImpl.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CEvent;
class CObject;
class CEventInstance;

class CImpl final : public IImpl
{
public:

	CImpl(CImpl const&) = delete;
	CImpl(CImpl&&) = delete;
	CImpl& operator=(CImpl const&) = delete;
	CImpl& operator=(CImpl&&) = delete;

	CImpl();
	virtual ~CImpl() override = default;

	// CryAudio::Impl::IImpl
	virtual void                    Update() override;
	virtual bool                    Init(uint16 const objectPoolSize) override;
	virtual void                    ShutDown() override;
	virtual void                    OnBeforeRelease() override {}
	virtual void                    Release() override;
	virtual void                    OnRefresh() override;
	virtual void                    SetLibraryData(XmlNodeRef const& node, ContextId const contextId) override;
	virtual void                    OnBeforeLibraryDataChanged() override;
	virtual void                    OnAfterLibraryDataChanged(int const poolAllocationMode) override;
	virtual void                    OnLoseFocus() override;
	virtual void                    OnGetFocus() override;
	virtual void                    MuteAll() override;
	virtual void                    UnmuteAll() override;
	virtual void                    PauseAll() override;
	virtual void                    ResumeAll() override;
	virtual void                    StopAllSounds() override;
	virtual void                    RegisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual void                    UnregisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual bool                    ConstructFile(XmlNodeRef const& rootNode, SFileInfo* const pFileInfo) override;
	virtual void                    DestructFile(IFile* const pIFile) override;
	virtual char const*             GetFileLocation(IFile* const pIFile) const override;
	virtual void                    GetInfo(SImplInfo& implInfo) const override;
	virtual ITriggerConnection*     ConstructTriggerConnection(XmlNodeRef const& rootNode, float& radius) override;
	virtual ITriggerConnection*     ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo) override;
	virtual void                    DestructTriggerConnection(ITriggerConnection* const pITriggerConnection) override;
	virtual IParameterConnection*   ConstructParameterConnection(XmlNodeRef const& rootNode) override;
	virtual void                    DestructParameterConnection(IParameterConnection const* const pIParameterConnection) override;
	virtual ISwitchStateConnection* ConstructSwitchStateConnection(XmlNodeRef const& rootNode) override;
	virtual void                    DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection) override;
	virtual IEnvironmentConnection* ConstructEnvironmentConnection(XmlNodeRef const& rootNode) override;
	virtual void                    DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection) override;
	virtual ISettingConnection*     ConstructSettingConnection(XmlNodeRef const& rootNode) override;
	virtual void                    DestructSettingConnection(ISettingConnection const* const pISettingConnection) override;
	virtual IObject*                ConstructObject(CTransformation const& transformation, IListeners const& listeners, char const* const szName = nullptr) override;
	virtual void                    DestructObject(IObject const* const pIObject) override;
	virtual IListener*              ConstructListener(CTransformation const& transformation, char const* const szName) override;
	virtual void                    DestructListener(IListener* const pIListener) override;
	virtual void                    GamepadConnected(DeviceId const deviceUniqueID) override;
	virtual void                    GamepadDisconnected(DeviceId const deviceUniqueID) override;
	virtual void                    SetLanguage(char const* const szLanguage) override;

	// Below data is only used when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	virtual void DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const drawDetailedInfo) override;
	virtual void DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, Vec3 const& camPos, float const debugDistance, char const* const szTextFilter) const override;
	// ~CryAudio::Impl::IImpl

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CEventInstance* ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event, CObject const& object);
#else
	CEventInstance* ConstructEventInstance(TriggerInstanceId const triggerInstanceId, CEvent& event);
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	void DestructEventInstance(CEventInstance const* const pEventInstance);

private:

	size_t m_memoryAlignment;
	string m_language;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CryFixedStringT<MaxInfoStringLength> const m_name;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
