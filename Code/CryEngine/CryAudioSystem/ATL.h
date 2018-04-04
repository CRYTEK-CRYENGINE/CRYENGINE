// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "InternalEntities.h"
#include "AudioListenerManager.h"
#include "AudioEventListenerManager.h"
#include "AudioStandaloneFileManager.h"
#include "AudioEventManager.h"
#include "AudioObjectManager.h"
#include "AudioXMLProcessor.h"
#include <CryInput/IInput.h>

namespace CryAudio
{
enum class EInternalStates : EnumFlagsType
{
	None                        = 0,
	IsMuted                     = BIT(0),
	AudioMiddlewareShuttingDown = BIT(1),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EInternalStates);

class CSystem;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
class CProfileData;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

class CAudioTranslationLayer final : public IInputEventListener
{
public:

	CAudioTranslationLayer();
	virtual ~CAudioTranslationLayer() override;

	CAudioTranslationLayer(CAudioTranslationLayer const&) = delete;
	CAudioTranslationLayer(CAudioTranslationLayer&&) = delete;
	CAudioTranslationLayer& operator=(CAudioTranslationLayer const&) = delete;
	CAudioTranslationLayer& operator=(CAudioTranslationLayer&&) = delete;

	// IInputEventListener
	virtual bool OnInputEvent(SInputEvent const& event) override;
	// ~IInputEventListener

	bool           Initialize(CSystem* const pAudioSystem);
	bool           ShutDown();
	void           ProcessRequest(CAudioRequest& request);
	void           Update(float const deltaTime);

	bool           CanProcessRequests() const { return (m_flags& EInternalStates::AudioMiddlewareShuttingDown) == 0; }

	ERequestStatus ParseControlsData(char const* const szFolderPath, EDataScope const dataScope);
	ERequestStatus ClearControlsData(EDataScope const dataScope);
	ERequestStatus ParsePreloadsData(char const* const szFolderPath, EDataScope const dataScope);
	ERequestStatus ClearPreloadsData(EDataScope const dataScope);

	void           NotifyListener(CAudioRequest const& request);

	void           IncrementGlobalObjectSyncCallbackCounter();
	void           DecrementGlobalObjectSyncCallbackCounter();

private:

	ERequestStatus ProcessAudioManagerRequest(CAudioRequest const& request);
	ERequestStatus ProcessAudioCallbackManagerRequest(CAudioRequest& request);
	ERequestStatus ProcessAudioObjectRequest(CAudioRequest const& request);
	ERequestStatus ProcessAudioListenerRequest(SAudioRequestData const* const pPassedRequestData);
	ERequestStatus SetImpl(Impl::IImpl* const pIImpl);
	void           ReleaseImpl();

	ERequestStatus RefreshAudioSystem(char const* const szLevelName);
	void           SetImplLanguage();
	void           InitInternalControls();
	void           SetCurrentEnvironmentsOnObject(CATLAudioObject* const pObject, EntityId const entityToIgnore, Vec3 const& position);

	// ATLObject containers
	AudioTriggerLookup        m_triggers;
	AudioParameterLookup      m_parameters;
	AudioSwitchLookup         m_switches;
	AudioPreloadRequestLookup m_preloadRequests;
	AudioEnvironmentLookup    m_environments;

	CATLAudioObject*          m_pGlobalAudioObject = nullptr;

	// Components
	CAudioStandaloneFileManager m_audioStandaloneFileMgr;
	CAudioEventManager          m_audioEventMgr;
	CAudioObjectManager         m_audioObjectMgr;
	CAudioListenerManager       m_audioListenerMgr;
	CFileCacheManager           m_fileCacheMgr;
	CAudioEventListenerManager  m_audioEventListenerMgr;
	CAudioXMLProcessor          m_xmlProcessor;

	SInternalControls           m_internalControls;

	// Utility members
	uint32          m_lastMainThreadFrameId = 0;
	EInternalStates m_flags = EInternalStates::None;
	Impl::IImpl*    m_pIImpl = nullptr;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void          DrawAudioSystemDebugInfo();
	void          GetAudioTriggerData(ControlId const audioTriggerId, STriggerData& audioTriggerData) const;
	CProfileData* GetProfileData() const;

private:

	void DrawAudioObjectDebugInfo(IRenderAuxGeom& auxGeom);
	void DrawATLComponentDebugInfo(IRenderAuxGeom& auxGeom, float posX, float const posY);
	void RetriggerAudioControls();

	CProfileData* m_pProfileData = nullptr;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
