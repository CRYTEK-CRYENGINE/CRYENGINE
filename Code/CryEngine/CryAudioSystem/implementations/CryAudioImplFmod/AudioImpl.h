// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <IAudioImpl.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
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
	virtual void                Update() override;
	virtual ERequestStatus      Init(uint32 const objectPoolSize, uint32 const eventPoolSize) override;
	virtual ERequestStatus      OnBeforeShutDown() override;
	virtual ERequestStatus      ShutDown() override;
	virtual ERequestStatus      Release() override;
	virtual ERequestStatus      OnLoseFocus() override;
	virtual ERequestStatus      OnGetFocus() override;
	virtual ERequestStatus      MuteAll() override;
	virtual ERequestStatus      UnmuteAll() override;
	virtual ERequestStatus      PauseAll() override;
	virtual ERequestStatus      ResumeAll() override;
	virtual ERequestStatus      StopAllSounds() override;
	virtual ERequestStatus      RegisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus      UnregisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus      ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo) override;
	virtual void                DestructFile(IFile* const pIFile) override;
	virtual char const* const   GetFileLocation(SFileInfo* const pFileInfo) override;
	virtual void                GetInfo(SImplInfo& implInfo) const override;
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
	virtual void                OnRefresh() override;
	virtual void                SetLanguage(char const* const szLanguage) override;

	// Below data is only used when INCLUDE_FMOD_IMPL_PRODUCTION_CODE is defined!
	virtual void GetMemoryInfo(SMemoryInfo& memoryInfo) const override;
	virtual void GetFileData(char const* const szName, SFileData& fileData) const override;
	// ~CryAudio::Impl::IImpl

private:

	static char const* const s_szEventPrefix;
	static char const* const s_szSnapshotPrefix;
	static char const* const s_szBusPrefix;
	static char const* const s_szVcaPrefix;

	void        CreateVersionString(CryFixedStringT<MaxInfoStringLength>& stringOut) const;
	bool        LoadMasterBanks();
	void        UnloadMasterBanks();
	void        MuteMasterBus(bool const shouldMute);
	void        PauseMasterBus(bool const shouldPause);

	FMOD_RESULT LoadBankCustom(char const* const szFileName, FMOD::Studio::Bank** ppBank);

	bool                                  m_isMuted;

	Objects                               m_constructedObjects;

	CryFixedStringT<MaxFilePathLength>    m_regularSoundBankFolder;
	CryFixedStringT<MaxFilePathLength>    m_localizedSoundBankFolder;

	CryFixedStringT<MaxFilePathLength>    m_masterBankPath;
	CryFixedStringT<MaxFilePathLength>    m_masterAssetsBankPath;
	CryFixedStringT<MaxFilePathLength>    m_masterStreamsBankPath;
	CryFixedStringT<MaxFilePathLength>    m_masterStringsBankPath;

	FMOD::Studio::System*                 m_pSystem;
	FMOD::System*                         m_pLowLevelSystem;
	FMOD::Studio::Bank*                   m_pMasterBank;
	FMOD::Studio::Bank*                   m_pMasterAssetsBank;
	FMOD::Studio::Bank*                   m_pMasterStreamsBank;
	FMOD::Studio::Bank*                   m_pMasterStringsBank;
	CryFixedStringT<MaxControlNameLength> m_language;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	CryFixedStringT<MaxInfoStringLength> m_name;
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
