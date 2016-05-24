// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLUtils.h"
#include "AudioInternalInterfaces.h"
#include <SharedAudioData.h>
#include <CrySystem/IStreamEngine.h>
#include <CrySystem/TimeValue.h>

struct SATLXMLTags
{
	static char const* const szPlatform;

	static char const* const szRootNodeTag;
	static char const* const szEditorDataTag;
	static char const* const szTriggersNodeTag;
	static char const* const szRtpcsNodeTag;
	static char const* const szSwitchesNodeTag;
	static char const* const szPreloadsNodeTag;
	static char const* const szEnvironmentsNodeTag;

	static char const* const szATLTriggerTag;
	static char const* const szATLSwitchTag;
	static char const* const szATLRtpcTag;
	static char const* const szATLSwitchStateTag;
	static char const* const szATLEnvironmentTag;
	static char const* const szATLPlatformsTag;
	static char const* const szATLConfigGroupTag;

	static char const* const szATLTriggerRequestTag;
	static char const* const szATLSwitchRequestTag;
	static char const* const szATLValueTag;
	static char const* const szATLRtpcRequestTag;
	static char const* const szATLPreloadRequestTag;
	static char const* const szATLEnvironmentRequestTag;

	static char const* const szATLVersionAttribute;
	static char const* const szATLNameAttribute;
	static char const* const szATLInternalNameAttribute;
	static char const* const szATLTypeAttribute;
	static char const* const szATLConfigGroupAttribute;

	static char const* const szATLDataLoadType;
};

struct SATLInternalControlIDs
{
	static AudioControlId        obstructionOcclusionCalcSwitchId;
	static AudioControlId        objectDopplerTrackingSwitchId;
	static AudioControlId        objectVelocityTrackingSwitchId;
	static AudioControlId        loseFocusTriggerId;
	static AudioControlId        getFocusTriggerId;
	static AudioControlId        muteAllTriggerId;
	static AudioControlId        unmuteAllTriggerId;
	static AudioControlId        objectDopplerRtpcId;
	static AudioControlId        objectVelocityRtpcId;
	static AudioSwitchStateId    ignoreStateId;
	static AudioSwitchStateId    singleRayStateId;
	static AudioSwitchStateId    multiRayStateId;
	static AudioSwitchStateId    onStateId;
	static AudioSwitchStateId    offStateId;
	static AudioPreloadRequestId globalPreloadRequestId;
};

void InitATLControlIDs(); // initializes the values in SATLInternalControlIDs

namespace CryAudio
{
namespace Impl
{
struct IAudioObject;
struct IAudioListener;
struct IAudioTrigger;
struct IAudioRtpc;
struct IAudioSwitchState;
struct IAudioEnvironment;
struct IAudioEvent;
struct IAudioFileEntry;
struct IAudioImpl;
}
}

enum EAudioObjectFlags : AudioEnumFlagsType
{
	eAudioObjectFlags_None                = 0,
	eAudioObjectFlags_TrackDoppler        = BIT(0),
	eAudioObjectFlags_TrackVelocity       = BIT(1),
	eAudioObjectFlags_NeedsDopplerUpdate  = BIT(2),
	eAudioObjectFlags_NeedsVelocityUpdate = BIT(3),
	eAudioObjectFlags_DoNotRelease        = BIT(4),
	eAudioObjectFlags_Virtual             = BIT(5),
};

enum EAudioSubsystem : AudioEnumFlagsType
{
	eAudioSubsystem_None          = 0,
	eAudioSubsystem_AudioImpl     = 1,
	eAudioSubsystem_AudioInternal = 2,
};

enum EAudioFileFlags : AudioEnumFlagsType
{
	eAudioFileFlags_Cached                    = BIT(0),
	eAudioFileFlags_NotCached                 = BIT(1),
	eAudioFileFlags_NotFound                  = BIT(2),
	eAudioFileFlags_MemAllocFail              = BIT(3),
	eAudioFileFlags_Removable                 = BIT(4),
	eAudioFileFlags_Loading                   = BIT(5),
	eAudioFileFlags_UseCounted                = BIT(6),
	eAudioFileFlags_NeedsResetToManualLoading = BIT(7),
	eAudioFileFlags_Localized                 = BIT(8),
};

template<typename IDType>
class CATLEntity
{
public:

	explicit CATLEntity(IDType const id, EAudioDataScope const dataScope)
		: m_id(id)
		, m_dataScope(dataScope)
	{}

	virtual ~CATLEntity() {}

	virtual IDType          GetId() const        { return m_id; }
	virtual EAudioDataScope GetDataScope() const { return m_dataScope; }

protected:

	EAudioDataScope m_dataScope;

private:

	IDType const m_id;
};

typedef CATLEntity<AudioControlId> TATLControl;
typedef CATLEntity<AudioObjectId>  TATLObject;

struct SATLSoundPropagationData
{
	SATLSoundPropagationData()
		: obstruction(0.0f)
		, occlusion(0.0f)
	{}

	~SATLSoundPropagationData() {}

	float obstruction;
	float occlusion;
};

class CATLListenerObject final : public TATLObject
{
public:

	explicit CATLListenerObject(AudioObjectId const id, CryAudio::Impl::IAudioListener* const pImplData = nullptr)
		: TATLObject(id, eAudioDataScope_None)
		, m_pImplData(pImplData)
		, m_bNeedsFinalSetPosition(false)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		, m_velocity(0.0f)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	{}

	virtual ~CATLListenerObject() {}

	void Update(CryAudio::Impl::IAudioImpl* const pImpl)
	{
		// Exponential decay towards zero.
		if (m_attributes.velocity.GetLengthSquared() > 0.0f)
		{
			float const deltaTime = (g_lastMainThreadFrameStartTime - m_previousTime).GetSeconds();
			float const decay = std::max(1.0f - deltaTime / 0.125f, 0.0f);
			m_attributes.velocity *= decay;
		}
		else if (m_bNeedsFinalSetPosition)
		{
			m_attributes.velocity = ZERO;
			pImpl->SetListener3DAttributes(m_pImplData, m_attributes);
			m_bNeedsFinalSetPosition = false;
		}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		m_velocity = m_attributes.velocity.GetLength();
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	float GetVelocity() const { return m_velocity; }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	CryAudio::Impl::SAudioObject3DAttributes const& Get3DAttributes() const { return m_attributes; }
	void                                            SetTransformation(CAudioObjectTransformation const& transformation)
	{
		float const deltaTime = (g_lastMainThreadFrameStartTime - m_previousTime).GetSeconds();

		if (deltaTime > 0.0f)
		{
			m_attributes.transformation = transformation;
			m_attributes.velocity = (m_attributes.transformation.GetPosition() - m_previousAttributes.transformation.GetPosition()) / deltaTime;
			m_previousTime = g_lastMainThreadFrameStartTime;
			m_previousAttributes = m_attributes;
			m_bNeedsFinalSetPosition = m_attributes.velocity.GetLengthSquared() > 0.0f;
		}
	}

	CryAudio::Impl::IAudioListener* const m_pImplData;

private:

	bool m_bNeedsFinalSetPosition;
	CryAudio::Impl::SAudioObject3DAttributes m_attributes;
	CryAudio::Impl::SAudioObject3DAttributes m_previousAttributes;
	CTimeValue                               m_previousTime;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	float m_velocity;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	PREVENT_OBJECT_COPY(CATLListenerObject);
	DELETE_DEFAULT_CONSTRUCTOR(CATLListenerObject);
};

class CATLControlImpl
{
public:

	CATLControlImpl()
		: m_audioSubsystem(eAudioSubsystem_None)
	{}

	explicit CATLControlImpl(EAudioSubsystem const audioSubsystem)
		: m_audioSubsystem(audioSubsystem)
	{}

	virtual ~CATLControlImpl() {}

	EAudioSubsystem GetReceiver() const { return m_audioSubsystem; }

protected:

	EAudioSubsystem m_audioSubsystem;
};

class CATLTriggerImpl : public CATLControlImpl
{
public:

	explicit CATLTriggerImpl(
	  AudioTriggerImplId const audioTriggerImplId,
	  AudioControlId const audioTriggerId,
	  EAudioSubsystem const audioSubsystem,
	  CryAudio::Impl::IAudioTrigger const* const pImplData = nullptr)
		: CATLControlImpl(audioSubsystem)
		, m_audioTriggerImplId(audioTriggerImplId)
		, m_audioTriggerId(audioTriggerId)
		, m_pImplData(pImplData)
	{}

	virtual ~CATLTriggerImpl() {}

	AudioTriggerImplId const                   m_audioTriggerImplId;
	AudioControlId const                       m_audioTriggerId;
	CryAudio::Impl::IAudioTrigger const* const m_pImplData;
};

class CATLTrigger : public TATLControl
{
public:

	typedef std::vector<CATLTriggerImpl const*, STLSoundAllocator<CATLTriggerImpl const*>> ImplPtrVec;

	explicit CATLTrigger(AudioControlId const audioTriggerId, EAudioDataScope const dataScope, ImplPtrVec const& implPtrs, float const maxRadius)
		: TATLControl(audioTriggerId, dataScope)
		, m_implPtrs(implPtrs)
		, m_maxRadius(maxRadius)
	{}

	virtual ~CATLTrigger() {}

	ImplPtrVec const m_implPtrs;
	float const      m_maxRadius;
};

class CATLRtpcImpl : public CATLControlImpl
{
public:

	explicit CATLRtpcImpl(EAudioSubsystem const audioSubsystem, CryAudio::Impl::IAudioRtpc const* const pImplData = nullptr)
		: CATLControlImpl(audioSubsystem)
		, m_pImplData(pImplData)
	{}

	virtual ~CATLRtpcImpl() {}

	CryAudio::Impl::IAudioRtpc const* const m_pImplData;
};

class CATLRtpc : public TATLControl
{
public:

	typedef std::vector<CATLRtpcImpl const*, STLSoundAllocator<CATLRtpcImpl const*>> ImplPtrVec;

	explicit CATLRtpc(AudioControlId const audioRtpcId, EAudioDataScope const dataScope, ImplPtrVec const& cImplPtrs)
		: TATLControl(audioRtpcId, dataScope)
		, m_implPtrs(cImplPtrs)
	{}

	virtual ~CATLRtpc() {}

	ImplPtrVec const m_implPtrs;
};

class CATLSwitchStateImpl : public CATLControlImpl
{
public:

	explicit CATLSwitchStateImpl(EAudioSubsystem const audioSubsystem, CryAudio::Impl::IAudioSwitchState const* const pImplData = nullptr)
		: CATLControlImpl(audioSubsystem)
		, m_pImplData(pImplData)
	{}

	virtual ~CATLSwitchStateImpl() {}

	CryAudio::Impl::IAudioSwitchState const* const m_pImplData;
};

class CATLSwitchState
{
public:

	typedef std::vector<CATLSwitchStateImpl const*, STLSoundAllocator<CATLSwitchStateImpl const*>> ImplPtrVec;

	explicit CATLSwitchState(
	  AudioControlId const audioSwitchId,
	  AudioSwitchStateId const audioSwitchStateId,
	  ImplPtrVec const& implPtrs)
		: m_audioSwitchStateId(audioSwitchStateId)
		, m_audioSwitchId(audioSwitchId)
		, m_implPtrs(implPtrs)
	{}

	virtual ~CATLSwitchState() {}

	virtual AudioSwitchStateId GetId() const       { return m_audioSwitchStateId; }
	virtual AudioSwitchStateId GetParentId() const { return m_audioSwitchId; }

	ImplPtrVec const m_implPtrs;

private:

	AudioSwitchStateId const m_audioSwitchStateId;
	AudioControlId const     m_audioSwitchId;
};

class CATLSwitch : public TATLControl
{
public:

	explicit CATLSwitch(AudioControlId const audioSwitchId, EAudioDataScope const dataScope)
		: TATLControl(audioSwitchId, dataScope)
	{}

	virtual ~CATLSwitch() {}

	typedef std::map<AudioSwitchStateId, CATLSwitchState const*, std::less<AudioSwitchStateId>, STLSoundAllocator<std::pair<AudioSwitchStateId, CATLSwitchState const*>>> AudioStates;
	AudioStates audioSwitchStates;
};

class CATLEnvironmentImpl : public CATLControlImpl
{
public:

	explicit CATLEnvironmentImpl(EAudioSubsystem const audioSubsystem, CryAudio::Impl::IAudioEnvironment const* const pImplData = nullptr)
		: CATLControlImpl(audioSubsystem)
		, m_pImplData(pImplData)
	{}

	virtual ~CATLEnvironmentImpl() {}

	CryAudio::Impl::IAudioEnvironment const* const m_pImplData;
};

class CATLAudioEnvironment : public CATLEntity<AudioEnvironmentId>
{
public:

	typedef std::vector<CATLEnvironmentImpl const*, STLSoundAllocator<CATLEnvironmentImpl const*>> ImplPtrVec;

	explicit CATLAudioEnvironment(AudioEnvironmentId const audioEnvironmentId, EAudioDataScope const dataScope, ImplPtrVec const& implPtrs)
		: CATLEntity<AudioEnvironmentId>(audioEnvironmentId, dataScope)
		, m_implPtrs(implPtrs)
	{}

	virtual ~CATLAudioEnvironment() {}

	ImplPtrVec const m_implPtrs;
};

class CATLStandaloneFile : public CATLEntity<AudioStandaloneFileId>
{
public:

	explicit CATLStandaloneFile(AudioStandaloneFileId const instanceId, EAudioDataScope const dataScope)
		: CATLEntity<AudioStandaloneFileId>(instanceId, dataScope)
		, m_id(INVALID_AUDIO_STANDALONE_FILE_ID)
		, m_audioObjectId(INVALID_AUDIO_OBJECT_ID)
		, m_pImplData(nullptr)
		, m_state(eAudioStandaloneFileState_None)
	{}

	virtual ~CATLStandaloneFile() {}

	bool         IsPlaying() const { return (m_state == eAudioStandaloneFileState_Playing) || (m_state == eAudioStandaloneFileState_Stopping); }

	virtual void Clear()
	{
		m_id = INVALID_AUDIO_STANDALONE_FILE_ID;
		m_audioObjectId = INVALID_AUDIO_OBJECT_ID;
		m_state = eAudioStandaloneFileState_None;
	}

	AudioStandaloneFileId                 m_id;
	AudioObjectId                         m_audioObjectId;
	CryAudio::Impl::IAudioStandaloneFile* m_pImplData;
	EAudioStandaloneFileState             m_state;

	DELETE_DEFAULT_CONSTRUCTOR(CATLStandaloneFile);
	PREVENT_OBJECT_COPY(CATLStandaloneFile);
};

class CATLEvent : public CATLEntity<AudioEventId>
{
public:

	explicit CATLEvent(AudioEventId const audioEventId, EAudioSubsystem const audioSubsystem)
		: CATLEntity<AudioEventId>(audioEventId, eAudioDataScope_None)
		, m_audioObjectId(INVALID_AUDIO_OBJECT_ID)
		, m_pTrigger(nullptr)
		, m_audioTriggerImplId(INVALID_AUDIO_TRIGGER_IMPL_ID)
		, m_audioTriggerInstanceId(INVALID_AUDIO_TRIGGER_INSTANCE_ID)
		, m_audioEventState(eAudioEventState_None)
		, m_audioSubsystem(audioSubsystem)
		, m_pImplData(nullptr)
	{}

	virtual ~CATLEvent() {}

	void         SetDataScope(EAudioDataScope const dataScope) { m_dataScope = dataScope; }
	bool         IsPlaying() const                             { return m_audioEventState == eAudioEventState_Playing || m_audioEventState == eAudioEventState_PlayingDelayed; }

	virtual void Clear()
	{
		m_dataScope = eAudioDataScope_None;
		m_audioObjectId = INVALID_AUDIO_OBJECT_ID;
		m_pTrigger = nullptr;
		m_audioTriggerImplId = INVALID_AUDIO_TRIGGER_IMPL_ID;
		m_audioTriggerInstanceId = INVALID_AUDIO_TRIGGER_INSTANCE_ID;
		m_audioEventState = eAudioEventState_None;
	}

	AudioObjectId                m_audioObjectId;
	CATLTrigger const*           m_pTrigger;
	AudioTriggerImplId           m_audioTriggerImplId;
	AudioTriggerInstanceId       m_audioTriggerInstanceId;
	EAudioEventState             m_audioEventState;
	EAudioSubsystem const        m_audioSubsystem;
	CryAudio::Impl::IAudioEvent* m_pImplData;

	DELETE_DEFAULT_CONSTRUCTOR(CATLEvent);
	PREVENT_OBJECT_COPY(CATLEvent);
};

class CATLAudioFileEntry
{
public:

	explicit CATLAudioFileEntry(char const* const szPath = nullptr, CryAudio::Impl::IAudioFileEntry* const pImplData = nullptr)
		: m_path(szPath)
		, m_size(0)
		, m_useCount(0)
		, m_memoryBlockAlignment(AUDIO_MEMORY_ALIGNMENT)
		, m_flags(eAudioFileFlags_NotFound)
		, m_dataScope(eAudioDataScope_All)
		, m_streamTaskType(eStreamTaskTypeCount)
		, m_pMemoryBlock(nullptr)
		, m_pReadStream(nullptr)
		, m_pImplData(pImplData)
	{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		m_timeCached.SetValue(0);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}

	virtual ~CATLAudioFileEntry() {}

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> m_path;
	size_t                           m_size;
	size_t                           m_useCount;
	size_t                           m_memoryBlockAlignment;
	AudioEnumFlagsType               m_flags;
	EAudioDataScope                  m_dataScope;
	EStreamTaskType                  m_streamTaskType;
	_smart_ptr<ICustomMemoryBlock>   m_pMemoryBlock;
	IReadStreamPtr                   m_pReadStream;
	CryAudio::Impl::IAudioFileEntry* m_pImplData;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CTimeValue m_timeCached;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	DELETE_DEFAULT_CONSTRUCTOR(CATLAudioFileEntry);
	PREVENT_OBJECT_COPY(CATLAudioFileEntry);
};

class CATLPreloadRequest : public CATLEntity<AudioPreloadRequestId>
{
public:

	typedef std::vector<AudioFileEntryId, STLSoundAllocator<AudioFileEntryId>> FileEntryIds;

	explicit CATLPreloadRequest(
	  AudioPreloadRequestId const audioPreloadRequestId,
	  EAudioDataScope const dataScope,
	  bool const bAutoLoad,
	  FileEntryIds const& fileEntryIds)
		: CATLEntity<AudioPreloadRequestId>(audioPreloadRequestId, dataScope)
		, m_bAutoLoad(bAutoLoad)
		, m_fileEntryIds(fileEntryIds)
	{}

	virtual ~CATLPreloadRequest() {}

	bool const   m_bAutoLoad;
	FileEntryIds m_fileEntryIds;

	DELETE_DEFAULT_CONSTRUCTOR(CATLPreloadRequest);
	PREVENT_OBJECT_COPY(CATLPreloadRequest);
};

//-------------------- ATLObject container typedefs --------------------------
typedef std::map<AudioControlId, CATLTrigger const*, std::less<AudioControlId>, STLSoundAllocator<std::pair<AudioControlId, CATLTrigger const*>>>                               AudioTriggerLookup;
typedef std::map<AudioControlId, CATLRtpc const*, std::less<AudioControlId>, STLSoundAllocator<std::pair<AudioControlId, CATLRtpc const*>>>                                     AudioRtpcLookup;
typedef std::map<AudioControlId, CATLSwitch const*, std::less<AudioControlId>, STLSoundAllocator<std::pair<AudioControlId, CATLSwitch const*>>>                                 AudioSwitchLookup;
typedef std::map<AudioPreloadRequestId, CATLPreloadRequest*, std::less<AudioPreloadRequestId>, STLSoundAllocator<std::pair<AudioPreloadRequestId, CATLPreloadRequest*>>>        AudioPreloadRequestLookup;
typedef std::map<AudioEnvironmentId, CATLAudioEnvironment const*, std::less<AudioEnvironmentId>, STLSoundAllocator<std::pair<AudioEnvironmentId, CATLAudioEnvironment const*>>> AudioEnvironmentLookup;

//------------------------ ATLInternal Entity Data ----------------------------
struct SATLSwitchStateImplData_internal : public CryAudio::Impl::IAudioSwitchState
{
	explicit SATLSwitchStateImplData_internal(AudioControlId const _internalAudioSwitchId, AudioSwitchStateId const _internalAudioSwitchStateId)
		: internalAudioSwitchId(_internalAudioSwitchId)
		, internalAudioSwitchStateId(_internalAudioSwitchStateId)
	{}

	virtual ~SATLSwitchStateImplData_internal() {}

	AudioControlId const     internalAudioSwitchId;
	AudioSwitchStateId const internalAudioSwitchStateId;

	DELETE_DEFAULT_CONSTRUCTOR(SATLSwitchStateImplData_internal);
	PREVENT_OBJECT_COPY(SATLSwitchStateImplData_internal);
};

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
class CATLDebugNameStore
{
public:

	CATLDebugNameStore();
	~CATLDebugNameStore();

	void        SyncChanges(CATLDebugNameStore const& otherNameStore);

	void        AddAudioObject(AudioObjectId const audioObjectId, char const* const szName);
	void        AddAudioTrigger(AudioControlId const audioTriggerId, char const* const szName);
	void        AddAudioRtpc(AudioControlId const audioRtpcId, char const* const szName);
	void        AddAudioSwitch(AudioControlId const audioSwitchId, char const* const szName);
	void        AddAudioSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioSwitchStateId, char const* const szName);
	void        AddAudioPreloadRequest(AudioPreloadRequestId const audioPreloadRequestId, char const* const szName);
	void        AddAudioEnvironment(AudioEnvironmentId const audioEnvironmentId, char const* const szName);
	void        AddAudioStandaloneFile(AudioStandaloneFileId const audioStandaloneFileId, char const* const szName);

	void        RemoveAudioObject(AudioObjectId const audioObjectId);
	void        RemoveAudioTrigger(AudioControlId const audioTriggerId);
	void        RemoveAudioRtpc(AudioControlId const audioRtpcId);
	void        RemoveAudioSwitch(AudioControlId const audioSwitchId);
	void        RemoveAudioPreloadRequest(AudioPreloadRequestId const audioPreloadRequestId);
	void        RemoveAudioEnvironment(AudioEnvironmentId const audioEnvironmentId);
	void        RemoveAudioStandaloneFile(AudioStandaloneFileId const audioStandaloneFileId);

	bool        AudioObjectsChanged() const       { return m_bAudioObjectsChanged; }
	bool        AudioTriggersChanged() const      { return m_bAudioTriggersChanged; }
	bool        AudioRtpcsChanged() const         { return m_bAudioRtpcsChanged; }
	bool        AudioSwitchesChanged() const      { return m_bAudioSwitchesChanged; }
	bool        AudioPreloadsChanged() const      { return m_bAudioPreloadsChanged; }
	bool        AudioEnvironamentsChanged() const { return m_bAudioEnvironmentsChanged; }

	char const* LookupAudioObjectName(AudioObjectId const audioObjectId) const;
	char const* LookupAudioTriggerName(AudioControlId const audioTriggerId) const;
	char const* LookupAudioRtpcName(AudioControlId const audioRtpcId) const;
	char const* LookupAudioSwitchName(AudioControlId const audioSwitchId) const;
	char const* LookupAudioSwitchStateName(AudioControlId const audioSwitchId, AudioSwitchStateId const audioSwitchStateId) const;
	char const* LookupAudioPreloadRequestName(AudioPreloadRequestId const audioPreloadRequestId) const;
	char const* LookupAudioEnvironmentName(AudioEnvironmentId const audioEnvironmentId) const;
	char const* LookupAudioStandaloneFileName(AudioStandaloneFileId const audioStandaloneFileId) const;

	void        GetAudioDebugData(SAudioDebugData& audioDebugData) const;

private:

	typedef std::map<AudioObjectId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, std::less<AudioObjectId>, STLSoundAllocator<std::pair<AudioObjectId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>>>>                                                                    AudioObjectMap;
	typedef std::map<AudioControlId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, std::less<AudioControlId>, STLSoundAllocator<std::pair<AudioControlId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>>>>                                                                 AudioControlMap;
	typedef std::map<AudioSwitchStateId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, std::less<AudioSwitchStateId>, STLSoundAllocator<std::pair<AudioSwitchStateId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>>>>                                                     AudioSwitchStateMap;
	typedef std::map<AudioControlId, std::pair<CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, AudioSwitchStateMap>, std::less<AudioControlId>, STLSoundAllocator<std::pair<AudioControlId, std::pair<CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, AudioSwitchStateMap>>>> AudioSwitchMap;
	typedef std::map<AudioPreloadRequestId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, std::less<AudioPreloadRequestId>, STLSoundAllocator<std::pair<AudioPreloadRequestId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>>>>                                            AudioPreloadRequestsMap;
	typedef std::map<AudioEnvironmentId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, std::less<AudioEnvironmentId>, STLSoundAllocator<std::pair<AudioEnvironmentId, CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>>>>                                                     AudioEnvironmentMap;
	typedef std::map<AudioStandaloneFileId, std::pair<CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, size_t>, std::less<AudioStandaloneFileId>, STLSoundAllocator<std::pair<AudioStandaloneFileId, std::pair<CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH>, size_t>>>>      AudioStandaloneFileMap;

	AudioObjectMap          m_audioObjectNames;
	AudioControlMap         m_audioTriggerNames;
	AudioControlMap         m_audioRtpcNames;
	AudioSwitchMap          m_audioSwitchNames;
	AudioPreloadRequestsMap m_audioPreloadRequestNames;
	AudioEnvironmentMap     m_audioEnvironmentNames;
	AudioStandaloneFileMap  m_audioStandaloneFileNames;

	mutable bool            m_bAudioObjectsChanged;
	mutable bool            m_bAudioTriggersChanged;
	mutable bool            m_bAudioRtpcsChanged;
	mutable bool            m_bAudioSwitchesChanged;
	mutable bool            m_bAudioPreloadsChanged;
	mutable bool            m_bAudioEnvironmentsChanged;
	mutable bool            m_bAudioStandaloneFilesChanged;

	PREVENT_OBJECT_COPY(CATLDebugNameStore);
};
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
