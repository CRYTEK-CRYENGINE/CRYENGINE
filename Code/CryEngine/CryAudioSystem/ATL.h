// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include "ATLComponents.h"
#include "FileCacheManager.h"
#include <CryInput/IInput.h>

class CAudioTranslationLayer : public IInputEventListener
{
public:

	CAudioTranslationLayer();
	virtual ~CAudioTranslationLayer();

	// IInputEventListener
	virtual bool OnInputEvent(SInputEvent const& event);
	// ~IInputEventListener

	bool                Initialize();
	bool                ShutDown();
	void                ProcessRequest(CAudioRequestInternal& request);
	void                Update(float const deltaTime);
	bool                GetAudioTriggerId(char const* const szAudioTriggerName, AudioControlId& audioTriggerId) const;
	bool                GetAudioRtpcId(char const* const szAudioRtpcName, AudioControlId& audioRtpcId) const;
	bool                GetAudioSwitchId(char const* const szAudioSwitchName, AudioControlId& audioSwitchId) const;
	bool                GetAudioSwitchStateId(AudioControlId const switchId, char const* const szAudioSwitchStateName, AudioSwitchStateId& audioSwitchStateId) const;
	bool                GetAudioPreloadRequestId(char const* const szAudioPreloadRequestName, AudioPreloadRequestId& audioPreloadRequestId) const;
	bool                GetAudioEnvironmentId(char const* const szAudioEnvironmentName, AudioEnvironmentId& audioEnvironmentId) const;
	bool                ReserveAudioObjectId(AudioObjectId& audioObjectId);
	bool                ReleaseAudioObjectId(AudioObjectId const audioObjectId);
	bool                ReserveAudioListenerId(AudioObjectId& audioObjectId);
	bool                ReleaseAudioListenerId(AudioObjectId const audioObjectId);
	bool                CanProcessRequests() const { return (m_flags & eAudioInternalStates_AudioMiddlewareShuttingDown) == 0; }

	EAudioRequestStatus ParseControlsData(char const* const szFolderPath, EAudioDataScope const dataScope);
	EAudioRequestStatus ClearControlsData(EAudioDataScope const dataScope);
	EAudioRequestStatus ParsePreloadsData(char const* const szFolderPath, EAudioDataScope const dataScope);
	EAudioRequestStatus ClearPreloadsData(EAudioDataScope const dataScope);

	void                NotifyListener(CAudioRequestInternal const& request);

private:

	EAudioRequestStatus ProcessAudioManagerRequest(CAudioRequestInternal const& request);
	EAudioRequestStatus ProcessAudioCallbackManagerRequest(CAudioRequestInternal& request);
	EAudioRequestStatus ProcessAudioObjectRequest(CAudioRequestInternal const& request);
	EAudioRequestStatus ProcessAudioListenertRequest(CATLListenerObject* const pListener, SAudioRequestDataInternal const* const pPassedRequestData);
	EAudioRequestStatus SetImpl(CryAudio::Impl::IAudioImpl* const pImpl);
	void                ReleaseImpl();
	EAudioRequestStatus PrepUnprepTriggerAsync(
	  CATLAudioObject* const pAudioObject,
	  CATLTrigger const* const pTrigger,
	  bool const bPrepare);
	EAudioRequestStatus PlayFile(
	  CATLAudioObject* const _pAudioObject,
	  char const* const _szFile,
	  AudioControlId const _triggerId,
	  bool const _bLocalized,
	  void* const _pOwner,
	  void* const _pUserData,
	  void* const _pUserDataOwner);

	EAudioRequestStatus StopFile(CATLAudioObject* const pAudioObject, char const* const szFile);
	EAudioRequestStatus ActivateTrigger(
	  CATLAudioObject* const pAudioObject,
	  CATLTrigger const* const pTrigger,
	  float const timeUntilRemovalMS,
	  void* const pOwner = nullptr,
	  void* const pUserData = nullptr,
	  void* const pUserDataOwner = nullptr,
	  AudioEnumFlagsType const flags = INVALID_AUDIO_ENUM_FLAG_TYPE);
	EAudioRequestStatus StopTrigger(CATLAudioObject* const pAudioObject, CATLTrigger const* const pTrigger);
	EAudioRequestStatus StopAllTriggers(CATLAudioObject* const pAudioObject);
	EAudioRequestStatus SetSwitchState(CATLAudioObject* const pAudioObject, CATLSwitchState const* const pState);
	EAudioRequestStatus SetRtpc(
	  CATLAudioObject* const pAudioObject,
	  CATLRtpc const* const pRtpc,
	  float const value);
	EAudioRequestStatus SetEnvironment(
	  CATLAudioObject* const pAudioObject,
	  CATLAudioEnvironment const* const pEnvironment,
	  float const amount);
	EAudioRequestStatus ResetEnvironments(CATLAudioObject* const pAudioObject);
	EAudioRequestStatus ActivateInternalTrigger(
	  CATLAudioObject* const pAudioObject,
	  CryAudio::Impl::IAudioTrigger const* const pAudioTrigger,
	  CryAudio::Impl::IAudioEvent* const pAudioEvent);
	EAudioRequestStatus StopInternalEvent(CATLAudioObject* const pAudioObject, CryAudio::Impl::IAudioEvent const* const pAudioEvent);
	EAudioRequestStatus StopAllInternalEvents(CATLAudioObject* const pAudioObject);
	EAudioRequestStatus SetInternalRtpc(
	  CATLAudioObject* const pAudioObject,
	  CryAudio::Impl::IAudioRtpc const* const pAudioRtpc,
	  float const value);
	EAudioRequestStatus SetInternalSwitchState(CATLAudioObject* const pAudioObject, CryAudio::Impl::IAudioSwitchState const* const pAudioSwitchState);
	EAudioRequestStatus SetInternalEnvironment(
	  CATLAudioObject* const pAudioObject,
	  CryAudio::Impl::IAudioEnvironment const* const pAudioEnvironment,
	  float const amount);
	EAudioRequestStatus RefreshAudioSystem(char const* const szLevelName);
	void                SetImplLanguage();

	enum EAudioInternalStates : AudioEnumFlagsType
	{
		eAudioInternalStates_None                        = 0,
		eAudioInternalStates_IsMuted                     = BIT(0),
		eAudioInternalStates_AudioMiddlewareShuttingDown = BIT(1),
	};

	// ATLObject containers
	AudioTriggerLookup        m_triggers;
	AudioRtpcLookup           m_rtpcs;
	AudioSwitchLookup         m_switches;
	AudioPreloadRequestLookup m_preloadRequests;
	AudioEnvironmentLookup    m_environments;

	CATLAudioObject*          m_pGlobalAudioObject;
	AudioObjectId const       m_globalAudioObjectId;

	AudioTriggerInstanceId    m_triggerInstanceIDCounter;

	CATLTrigger const*        m_pDefaultStandaloneFileTrigger;

	// Components
	CAudioStandaloneFileManager m_audioStandaloneFileMgr;
	CAudioEventManager          m_audioEventMgr;
	CAudioObjectManager         m_audioObjectMgr;
	CAudioListenerManager       m_audioListenerMgr;
	CFileCacheManager           m_fileCacheMgr;
	CAudioEventListenerManager  m_audioEventListenerMgr;
	CATLXMLProcessor            m_xmlProcessor;

	// Utility members
	uint32                      m_lastMainThreadFramId;
	volatile AudioEnumFlagsType m_flags;
	CryAudio::Impl::IAudioImpl* m_pImpl;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	bool                      ReserveAudioObjectId(AudioObjectId& audioObjectId, char const* const szAudioObjectName);
	void                      DrawAudioSystemDebugInfo();
	CATLDebugNameStore const& GetDebugStore() const { return m_debugNameStore; }

private:

	void DrawAudioObjectDebugInfo(IRenderAuxGeom& auxGeom);
	void DrawATLComponentDebugInfo(IRenderAuxGeom& auxGeom, float posX, float const posY);
	void RetriggerAudioControls();

	CATLDebugNameStore                            m_debugNameStore;
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> m_implNameString;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	PREVENT_OBJECT_COPY(CAudioTranslationLayer);
};
