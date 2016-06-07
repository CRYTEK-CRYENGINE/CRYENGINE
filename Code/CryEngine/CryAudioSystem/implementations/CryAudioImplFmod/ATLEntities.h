// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AudioEvent.h"
#include <ATLEntityData.h>
#include <atomic>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CAudioListener final : public IAudioListener
{
public:

	CAudioListener(int const _id)
		: m_id(_id)
	{
		ZeroStruct(m_attributes);
	}

	virtual ~CAudioListener() {}

	ILINE int                 GetId() const                                               { return m_id; }
	ILINE FMOD_3D_ATTRIBUTES& Get3DAttributes()                                           { return m_attributes; }
	ILINE void                SetListenerAttributes(FMOD_3D_ATTRIBUTES const& attributes) { m_attributes = attributes; }

private:

	int                m_id;
	FMOD_3D_ATTRIBUTES m_attributes;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioListener);
	PREVENT_OBJECT_COPY(CAudioListener);
};

enum EFmodEventType : AudioEnumFlagsType
{
	eFmodEventType_None,
	eFmodEventType_Start,
	eFmodEventType_Stop,
};

class CAudioTrigger final : public IAudioTrigger
{
public:

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	explicit CAudioTrigger(
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
	explicit CAudioTrigger(
	  uint32 const _eventPathId,
	  AudioEnumFlagsType const _eventType,
	  FMOD::Studio::EventDescription* const _pEventDescription,
	  FMOD_GUID const _guid)
		: m_eventPathId(_eventPathId)
		, m_eventType(_eventType)
		, m_pEventDescription(_pEventDescription)
		, m_guid(_guid)
	{}
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	virtual ~CAudioTrigger() {}

	uint32 const                          m_eventPathId;
	AudioEnumFlagsType const              m_eventType;
	FMOD::Studio::EventDescription* const m_pEventDescription;
	FMOD_GUID const                       m_guid;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	CryFixedStringT<512> const m_eventPath;
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	DELETE_DEFAULT_CONSTRUCTOR(CAudioTrigger);
	PREVENT_OBJECT_COPY(CAudioTrigger);
};

class CAudioParameter final : public IAudioRtpc
{
public:

	explicit CAudioParameter(
	  uint32 const _eventPathId,
	  float const _multiplier,
	  float const _shift,
	  char const* const _szName)
		: m_eventPathId(_eventPathId)
		, m_multiplier(_multiplier)
		, m_shift(_shift)
		, m_name(_szName)
	{}

	virtual ~CAudioParameter() {}

	uint32                                               GetEventPathId() const     { return m_eventPathId; }
	float                                                GetValueMultiplier() const { return m_multiplier; }
	float                                                GetValueShift() const      { return m_shift; }
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> const& GetName() const            { return m_name; }

private:

	uint32 const m_eventPathId;
	float const  m_multiplier;
	float const  m_shift;
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> const m_name;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioParameter);
	PREVENT_OBJECT_COPY(CAudioParameter);
};

class CAudioSwitchState final : public IAudioSwitchState
{
public:

	explicit CAudioSwitchState(
	  uint32 const _eventPathId,
	  float const _value,
	  char const* const _szName)
		: eventPathId(_eventPathId)
		, value(_value)
		, name(_szName)
	{}

	virtual ~CAudioSwitchState() {}

	uint32 const eventPathId;
	float const  value;
	CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> const name;
	DELETE_DEFAULT_CONSTRUCTOR(CAudioSwitchState);
	PREVENT_OBJECT_COPY(CAudioSwitchState);
};

class CAudioEnvironment final : public IAudioEnvironment
{
public:

	explicit CAudioEnvironment(
	  FMOD::Studio::EventDescription* const _pEventDescription,
	  FMOD::Studio::Bus* const _pBus)
		: pEventDescription(_pEventDescription)
		, pBus(_pBus)
	{}

	FMOD::Studio::EventDescription* const pEventDescription;
	FMOD::Studio::Bus* const              pBus;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioEnvironment);
	PREVENT_OBJECT_COPY(CAudioEnvironment);
};

class CAudioFileEntry final : public IAudioFileEntry
{
public:

	CAudioFileEntry()
		: pBank(nullptr)
	{}

	virtual ~CAudioFileEntry() {}

	FMOD::Studio::Bank* pBank;

	PREVENT_OBJECT_COPY(CAudioFileEntry);
};

class CAudioStandaloneFile final : public IAudioStandaloneFile
{
public:
	CAudioStandaloneFile()
		: fileId(INVALID_AUDIO_STANDALONE_FILE_ID)
		, fileInstanceId(INVALID_AUDIO_STANDALONE_FILE_ID)
		, programmerSoundEvent(INVALID_AUDIO_EVENT_ID)
		, pLowLevelSystem(nullptr)
		, pLowLevelSound(nullptr)
		, bWaitingForData(false)
		, bHasFinished(false)
		, bShouldBeStreamed(false)
	{}

	~CAudioStandaloneFile()
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

	AudioStandaloneFileId                       fileId;                     // ID unique to the file, only needed for the 'finished' request
	AudioStandaloneFileId                       fileInstanceId;             // ID unique to the file instance, only needed for the 'finished' request
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> fileName;
	CAudioEvent programmerSoundEvent;                                  //the fmod event containing the programmer sound that is used for playing the file
	FMOD::Sound*                                pLowLevelSound;
	FMOD::System*                               pLowLevelSystem;
	std::atomic<bool>                           bWaitingForData;
	std::atomic<bool>                           bHasFinished;
	bool bShouldBeStreamed;
};

class CAudioObject;

typedef std::vector<CAudioObject*, STLSoundAllocator<CAudioObject*>>                                                                                                AudioObjects;
typedef std::vector<CAudioEvent*, STLSoundAllocator<CAudioEvent*>>                                                                                                  AudioEvents;
typedef std::vector<CAudioStandaloneFile*, STLSoundAllocator<CAudioStandaloneFile*>>                                                                                StandaloneFiles;

typedef std::map<CAudioParameter const* const, int, std::less<CAudioParameter const* const>, STLSoundAllocator<std::pair<CAudioParameter const* const, int>>>       AudioParameterToIndexMap;
typedef std::map<CAudioSwitchState const* const, int, std::less<CAudioSwitchState const* const>, STLSoundAllocator<std::pair<CAudioSwitchState const* const, int>>> FmodSwitchToIndexMap;
}
}
}
