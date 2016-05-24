// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <atomic>

#include <ATLEntityData.h>
#include "AudioEventData_fmod.h"

namespace CryAudio
{
namespace Impl
{
class CAudioListener_fmod final : public IAudioListener
{
public:

	CAudioListener_fmod(int const _id)
		: m_id(_id)
	{
		ZeroStruct(m_attributes);
	}

	virtual ~CAudioListener_fmod() {}

	ILINE int                 GetId() const                                               { return m_id; }
	ILINE FMOD_3D_ATTRIBUTES& Get3DAttributes()                                           { return m_attributes; }
	ILINE void                SetListenerAttributes(FMOD_3D_ATTRIBUTES const& attributes) { m_attributes = attributes; }

private:

	int                m_id;
	FMOD_3D_ATTRIBUTES m_attributes;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioListener_fmod);
	PREVENT_OBJECT_COPY(CAudioListener_fmod);
};

enum EFmodEventType : AudioEnumFlagsType
{
	eFmodEventType_None,
	eFmodEventType_Start,
	eFmodEventType_Stop,
};

class CAudioTrigger_fmod final : public IAudioTrigger
{
public:

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	explicit CAudioTrigger_fmod(
	  uint32 const _eventPathId,
	  AudioEnumFlagsType const _eventType,
	  FMOD::Studio::EventDescription* const _pEventDescription,
	  FMOD_GUID const _guid,
	  char const* const _szEventPath)
		: m_eventPathId(_eventPathId)
		, m_eventType(_eventType)
		, m_pEventDescription(_pEventDescription)
		, m_guid(_guid)
		, m_eventPath(_szEventPath)
	{}
#else
	explicit CAudioTrigger_fmod(
	  uint32 const _eventPathId,
	  AudioEnumFlagsType const _eventType,
	  FMOD::Studio::EventDescription* const _pEventDescription,
	  FMOD_GUID const _guid)
		: m_eventPathId(_eventPathId)
		, m_eventType(_eventType)
		, m_pEventDescription(_pEventDescription)
		, m_guid(_guid)
	{}
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	virtual ~CAudioTrigger_fmod() {}

	uint32 const                          m_eventPathId;
	AudioEnumFlagsType const              m_eventType;
	FMOD::Studio::EventDescription* const m_pEventDescription;
	FMOD_GUID const                       m_guid;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	CryFixedStringT<512> const m_eventPath;
#endif // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	DELETE_DEFAULT_CONSTRUCTOR(CAudioTrigger_fmod);
	PREVENT_OBJECT_COPY(CAudioTrigger_fmod);
};

class CAudioParameter_fmod final : public IAudioRtpc
{
public:

	explicit CAudioParameter_fmod(
	  uint32 const _eventPathId,
	  float const _multiplier,
	  float const _shift,
	  char const* const _szName)
		: m_eventPathId(_eventPathId)
		, m_multiplier(_multiplier)
		, m_shift(_shift)
		, m_name(_szName)
	{}

	virtual ~CAudioParameter_fmod() {}

	uint32                                               GetEventPathId() const     { return m_eventPathId; }
	float                                                GetValueMultiplier() const { return m_multiplier; }
	float                                                GetValueShift() const      { return m_shift; }
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> const& GetName() const            { return m_name; }

private:

	uint32 const m_eventPathId;
	float const  m_multiplier;
	float const  m_shift;
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> const m_name;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioParameter_fmod);
	PREVENT_OBJECT_COPY(CAudioParameter_fmod);
};

class CAudioSwitchState_fmod final : public IAudioSwitchState
{
public:

	explicit CAudioSwitchState_fmod(
	  uint32 const _eventPathId,
	  float const _value,
	  char const* const _szName)
		: eventPathId(_eventPathId)
		, value(_value)
		, name(_szName)
	{}

	virtual ~CAudioSwitchState_fmod() {}

	uint32 const eventPathId;
	float const  value;
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> const name;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioSwitchState_fmod);
	PREVENT_OBJECT_COPY(CAudioSwitchState_fmod);
};

class CAudioEnvironment_fmod final : public IAudioEnvironment
{
public:

	explicit CAudioEnvironment_fmod(
	  FMOD::Studio::EventDescription* const _pEventDescription,
	  FMOD::Studio::Bus* const _pBus)
		: pEventDescription(_pEventDescription)
		, pBus(_pBus)
	{}

	FMOD::Studio::EventDescription* const pEventDescription;
	FMOD::Studio::Bus* const              pBus;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioEnvironment_fmod);
	PREVENT_OBJECT_COPY(CAudioEnvironment_fmod);
};

class CAudioFileEntry_fmod final : public IAudioFileEntry
{
public:

	CAudioFileEntry_fmod()
		: pBank(nullptr)
	{}

	virtual ~CAudioFileEntry_fmod() {}

	FMOD::Studio::Bank* pBank;

	PREVENT_OBJECT_COPY(CAudioFileEntry_fmod);
};

class CAudioStandaloneFile_fmod final : public IAudioStandaloneFile
{
public:
	CAudioStandaloneFile_fmod()
		: fileId(INVALID_AUDIO_STANDALONE_FILE_ID)
		, fileInstanceId(INVALID_AUDIO_STANDALONE_FILE_ID)
		, programmerSoundEvent(INVALID_AUDIO_EVENT_ID)
		, pLowLevelSystem(nullptr)
		, pLowLevelSound(nullptr)
		, bWaitingForData(false)
		, bHasFinished(false)
		, bShouldBeStreamed(false)
	{}
	~CAudioStandaloneFile_fmod()
	{}

	void Reset()
	{
		fileId = INVALID_AUDIO_STANDALONE_FILE_ID;
		fileInstanceId = INVALID_AUDIO_STANDALONE_FILE_ID;
		fileName.clear();
		programmerSoundEvent.Reset();
		pLowLevelSound = nullptr;
		bWaitingForData = false;
		bHasFinished = false;
		bShouldBeStreamed = false;
	}

	AudioStandaloneFileId                       fileId;               // ID unique to the file, only needed for the 'finished' request
	AudioStandaloneFileId                       fileInstanceId;       // ID unique to the file instance, only needed for the 'finished' request
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> fileName;
	CAudioEvent_fmod                            programmerSoundEvent; //the fmod event containing the programmer sound that is used for playing the file
	FMOD::Sound*                                pLowLevelSound;
	FMOD::System*                               pLowLevelSystem;
	std::atomic<bool>                           bWaitingForData;
	std::atomic<bool>                           bHasFinished;
	bool bShouldBeStreamed;
};

class CAudioObject_fmod;

typedef std::vector<CAudioObject_fmod*, STLSoundAllocator<CAudioObject_fmod*>>                                                                                                     AudioObjects;
typedef std::vector<CAudioEvent_fmod*, STLSoundAllocator<CAudioEvent_fmod*>>                                                                                                       AudioEvents;
typedef std::vector<CAudioStandaloneFile_fmod*, STLSoundAllocator<CAudioStandaloneFile_fmod*>>                                                                                     StandaloneFiles;

typedef std::map<CAudioParameter_fmod const* const, int, std::less<CAudioParameter_fmod const* const>, STLSoundAllocator<std::pair<CAudioParameter_fmod const* const, int>>>       AudioParameterToIndexMap;
typedef std::map<CAudioSwitchState_fmod const* const, int, std::less<CAudioSwitchState_fmod const* const>, STLSoundAllocator<std::pair<CAudioSwitchState_fmod const* const, int>>> FmodSwitchToIndexMap;
}
}
