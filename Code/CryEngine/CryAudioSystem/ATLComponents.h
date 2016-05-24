// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FileCacheManager.h"
#include "ATLUtils.h"
#include "ATLEntities.h"
#include "ATLAudioObject.h"
#include <CryAudio/IAudioSystem.h>

//---------------- ATL implementation internal classes ------------------------
class CAudioStandaloneFileManager
{
public:

	CAudioStandaloneFileManager();
	virtual ~CAudioStandaloneFileManager();

	void                Init(CryAudio::Impl::IAudioImpl* const pImpl);
	void                Release();

	CATLStandaloneFile* GetStandaloneFile(char const* const szFile);
	CATLStandaloneFile* LookupId(AudioStandaloneFileId const instanceId) const;
	void                ReleaseStandaloneFile(CATLStandaloneFile* const pStandaloneFile);

private:

	typedef std::map<AudioStandaloneFileId, CATLStandaloneFile*, std::less<AudioStandaloneFileId>, STLSoundAllocator<std::pair<AudioStandaloneFileId, CATLStandaloneFile*>>>
	  ActiveStandaloneFilesMap;

	ActiveStandaloneFilesMap m_activeAudioStandaloneFiles;

	typedef CInstanceManager<CATLStandaloneFile, AudioStandaloneFileId> AudioStandaloneFilePool;
	AudioStandaloneFilePool     m_audioStandaloneFilePool;

	CryAudio::Impl::IAudioImpl* m_pImpl;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore);
	void DrawDebugInfo(IRenderAuxGeom& auxGeom, float posX, float posY) const;

private:

	CATLDebugNameStore* m_pDebugNameStore;
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

	PREVENT_OBJECT_COPY(CAudioStandaloneFileManager);
};

class CAudioEventManager
{
public:

	CAudioEventManager();
	virtual ~CAudioEventManager();

	void       Init(CryAudio::Impl::IAudioImpl* const pImpl);
	void       Release();
	void       Update(float const deltaTime);

	CATLEvent* GetEvent(EAudioSubsystem const audioSubsystem);
	CATLEvent* LookupId(AudioEventId const audioEventId) const;
	void       ReleaseEvent(CATLEvent* const pEvent);

	size_t     GetNumActive() const;

private:

	CATLEvent* GetImplInstance();
	void       ReleaseEventInternal(CATLEvent* const pEvent);
	void       ReleaseImplInstance(CATLEvent* const pOldEvent);
	CATLEvent* GetInternalInstance();
	void       ReleaseInternalInstance(CATLEvent* const pOldEvent);

	typedef std::map<AudioEventId, CATLEvent*, std::less<AudioEventId>, STLSoundAllocator<std::pair<AudioEventId, CATLEvent*>>>
	  TActiveEventMap;

	TActiveEventMap m_activeAudioEvents;

	typedef CInstanceManager<CATLEvent, AudioEventId> TAudioEventPool;
	TAudioEventPool             m_audioEventPool;

	CryAudio::Impl::IAudioImpl* m_pImpl;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void SetDebugNameStore(CATLDebugNameStore const* const pDebugNameStore);
	void DrawDebugInfo(IRenderAuxGeom& auxGeom, float posX, float posY) const;

private:

	CATLDebugNameStore const* m_pDebugNameStore;
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

	PREVENT_OBJECT_COPY(CAudioEventManager);
};

class CAudioObjectManager
{
public:
	typedef std::unordered_map<AudioObjectId const, CATLAudioObject* const, std::hash<AudioObjectId>, std::equal_to<AudioObjectId>, STLSoundAllocator<std::pair<AudioObjectId const, CATLAudioObject* const>>>
		RegisteredAudioObjectsMap;

	explicit CAudioObjectManager(CAudioEventManager& _audioEventMgr, CAudioStandaloneFileManager& _audioStandaloneFileMgr);
	virtual ~CAudioObjectManager();

	void             Init(CryAudio::Impl::IAudioImpl* const pImpl);
	void             Release();
	void             Update(float const deltaTime, CryAudio::Impl::SAudioObject3DAttributes const& listenerAttributes);
	bool             ReserveId(AudioObjectId& audioObjectId);
	bool             ReserveThisId(AudioObjectId const audioObjectId);
	bool             ReleaseId(AudioObjectId const audioObjectId);
	CATLAudioObject* LookupId(AudioObjectId const audioObjectId) const;

	void             ReportStartedEvent(CATLEvent* const pEvent);
	void             ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess);
	void             GetStartedStandaloneFileRequestData(CATLStandaloneFile* const _pStandaloneFile, CAudioRequestInternal& _request);
	void             ReportFinishedStandaloneFile(CATLStandaloneFile* const _pStandaloneFile);
	void             ReportObstructionRay(AudioObjectId const audioObjectId, size_t const rayId);
	void             ReleasePendingRays();
	bool             IsActive(CATLAudioObject const* const pAudioObject) const;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)

	bool                             ReserveId(AudioObjectId& audioObjectId, char const* const szAudioObjectName);
	void                             SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore);
	size_t                           GetNumAudioObjects() const;
	size_t                           GetNumActiveAudioObjects() const;
	RegisteredAudioObjectsMap const& GetRegisteredAudioObjects() const { return m_registeredAudioObjects; }
	void                             DrawPerObjectDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPos) const;
	void                             DrawDebugInfo(IRenderAuxGeom& auxGeom, float posX, float posY) const;

private:
	CATLDebugNameStore* m_pDebugNameStore;
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

private:

	static float s_controlsUpdateInterval;

	CATLAudioObject* GetInstance();
	bool             ReleaseInstance(CATLAudioObject* const pOldObject);
	bool             HasActiveData(CATLAudioObject const* const pAudioObject) const;

	RegisteredAudioObjectsMap m_registeredAudioObjects;

	typedef CInstanceManager<CATLAudioObject, AudioObjectId> TAudioObjectPool;
	TAudioObjectPool             m_audioObjectPool;
	CryAudio::Impl::IAudioImpl*  m_pImpl;
	float                        m_timeSinceLastControlsUpdate;

	static AudioObjectId const   s_minAudioObjectId;

	CAudioEventManager&          m_audioEventMgr;
	CAudioStandaloneFileManager& m_audioStandaloneFileMgr;

	PREVENT_OBJECT_COPY(CAudioObjectManager);
};

class CAudioListenerManager
{
public:

	CAudioListenerManager();
	~CAudioListenerManager();

	void                                            Init(CryAudio::Impl::IAudioImpl* const pImpl);
	void                                            Release();
	void                                            Update(float const deltaTime);
	bool                                            ReserveId(AudioObjectId& audioObjectId);
	bool                                            ReleaseId(AudioObjectId const audioObjectId);
	CATLListenerObject*                             LookupId(AudioObjectId const audioObjectId) const;
	size_t                                          GetNumActive() const;
	CryAudio::Impl::SAudioObject3DAttributes const& GetDefaultListenerAttributes() const;
	AudioObjectId                                   GetDefaultListenerId() const { return m_defaultListenerId; }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	float GetDefaultListenerVelocity() const;
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

private:

	typedef std::vector<CATLListenerObject*, STLSoundAllocator<CATLListenerObject*>> TListenerPtrContainer;
	typedef std::map<AudioObjectId, CATLListenerObject*, std::less<AudioObjectId>, STLSoundAllocator<std::pair<AudioObjectId, CATLListenerObject*>>>
	  TActiveListenerMap;

	TActiveListenerMap          m_activeListeners;
	TListenerPtrContainer       m_listenerPool;
	CATLListenerObject*         m_pDefaultListenerObject;
	AudioObjectId const         m_defaultListenerId;
	AudioObjectId               m_numListeners;
	CryAudio::Impl::IAudioImpl* m_pImpl;

	PREVENT_OBJECT_COPY(CAudioListenerManager);
};

class CAudioEventListenerManager
{
public:

	CAudioEventListenerManager();
	~CAudioEventListenerManager();

	EAudioRequestStatus AddRequestListener(SAudioManagerRequestDataInternal<eAudioManagerRequestType_AddRequestListener> const* const pRequestData);
	EAudioRequestStatus RemoveRequestListener(void (* func)(SAudioRequestInfo const* const), void const* const pObjectToListenTo);
	void                NotifyListener(SAudioRequestInfo const* const pRequestInfo);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	size_t GetNumEventListeners() const { return m_listeners.size(); }
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

private:

	typedef std::vector<SAudioEventListener, STLSoundAllocator<SAudioEventListener>> TListenerArray;
	TListenerArray m_listeners;

	PREVENT_OBJECT_COPY(CAudioEventListenerManager);
};

class CATLXMLProcessor
{
public:

	CATLXMLProcessor(
	  AudioTriggerLookup& triggers,
	  AudioRtpcLookup& rtpcs,
	  AudioSwitchLookup& switches,
	  AudioEnvironmentLookup& environments,
	  AudioPreloadRequestLookup& preloadRequests,
	  CFileCacheManager& fileCacheMgr);

	~CATLXMLProcessor();

	void Init(CryAudio::Impl::IAudioImpl* const pImpl);
	void Release();

	void ParseControlsData(char const* const szFolderPath, EAudioDataScope const dataScope);
	void ClearControlsData(EAudioDataScope const dataScope);
	void ParsePreloadsData(char const* const szFolderPath, EAudioDataScope const dataScope);
	void ClearPreloadsData(EAudioDataScope const dataScope);

private:

	void                                     ParseAudioTriggers(XmlNodeRef const pXMLTriggerRoot, EAudioDataScope const dataScope);
	void                                     ParseAudioSwitches(XmlNodeRef const pXMLSwitchRoot, EAudioDataScope const dataScope);
	void                                     ParseAudioRtpcs(XmlNodeRef const pXMLRtpcRoot, EAudioDataScope const dataScope);
	void                                     ParseAudioPreloads(XmlNodeRef const pPreloadDataRoot, EAudioDataScope const dataScope, char const* const szFolderName, uint const version);
	void                                     ParseAudioEnvironments(XmlNodeRef const pAudioEnvironmentRoot, EAudioDataScope const dataScope);

	CryAudio::Impl::IAudioTrigger const*     NewInternalAudioTrigger(XmlNodeRef const pXMLTriggerRoot);
	CryAudio::Impl::IAudioRtpc const*        NewInternalAudioRtpc(XmlNodeRef const pXMLRtpcRoot);
	CryAudio::Impl::IAudioSwitchState const* NewInternalAudioSwitchState(XmlNodeRef const pXMLSwitchRoot);
	CryAudio::Impl::IAudioEnvironment const* NewInternalAudioEnvironment(XmlNodeRef const pXMLEnvironmentRoot);

	void                                     DeleteAudioTrigger(CATLTrigger const* const pOldTrigger);
	void                                     DeleteAudioRtpc(CATLRtpc const* const pOldRtpc);
	void                                     DeleteAudioSwitch(CATLSwitch const* const pOldSwitch);
	void                                     DeleteAudioPreloadRequest(CATLPreloadRequest const* const pOldPreloadRequest);
	void                                     DeleteAudioEnvironment(CATLAudioEnvironment const* const pOldEnvironment);

	AudioTriggerLookup&         m_triggers;
	AudioRtpcLookup&            m_rtpcs;
	AudioSwitchLookup&          m_switches;
	AudioEnvironmentLookup&     m_environments;
	AudioPreloadRequestLookup&  m_preloadRequests;
	AudioTriggerImplId          m_triggerImplIdCounter;
	CFileCacheManager&          m_fileCacheMgr;
	CryAudio::Impl::IAudioImpl* m_pImpl;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore);

private:

	CATLDebugNameStore* m_pDebugNameStore;
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

	DELETE_DEFAULT_CONSTRUCTOR(CATLXMLProcessor);
	PREVENT_OBJECT_COPY(CATLXMLProcessor);
};
