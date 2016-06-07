// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>
#include <IAudioSystemImplementation.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
typedef std::vector<AkUniqueID, STLSoundAllocator<AkUniqueID>> TAKUniqueIDVector;

struct SAudioObject final : public IAudioObject
{
	typedef std::map<AkAuxBusID, float, std::less<AkAuxBusID>,
	                 STLSoundAllocator<std::pair<AkAuxBusID, float>>> TEnvironmentImplMap;

	explicit SAudioObject(AkGameObjectID const _id, bool const _bHasPosition)
		: id(_id)
		, bHasPosition(_bHasPosition)
		, bNeedsToUpdateEnvironments(false)
	{}

	virtual ~SAudioObject() {}

	AkGameObjectID const id;
	bool const           bHasPosition;
	bool                 bNeedsToUpdateEnvironments;
	TEnvironmentImplMap  cEnvironemntImplAmounts;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioObject);
	PREVENT_OBJECT_COPY(SAudioObject);
};

struct SAudioListener final : public IAudioListener
{
	explicit SAudioListener(AkUniqueID const _id)
		: id(_id)
	{}

	virtual ~SAudioListener() {}

	AkUniqueID const id;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioListener);
	PREVENT_OBJECT_COPY(SAudioListener);
};

struct SAudioTrigger final : public IAudioTrigger
{
	explicit SAudioTrigger(AkUniqueID const _id)
		: id(_id)
	{}

	virtual ~SAudioTrigger() {}

	AkUniqueID const id;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioTrigger);
	PREVENT_OBJECT_COPY(SAudioTrigger);
};

struct SAudioRtpc final : public IAudioRtpc
{
	explicit SAudioRtpc(AkRtpcID const _id, float const _mult, float const _shift)
		: mult(_mult)
		, shift(_shift)
		, id(_id)
	{}

	virtual ~SAudioRtpc() {}

	float const    mult;
	float const    shift;
	AkRtpcID const id;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioRtpc);
	PREVENT_OBJECT_COPY(SAudioRtpc);
};

enum EWwiseSwitchType : AudioEnumFlagsType
{
	eWwiseSwitchType_None = 0,
	eWwiseSwitchType_Switch,
	eWwiseSwitchType_State,
	eWwiseSwitchType_Rtpc,
};

struct SAudioSwitchState final : public IAudioSwitchState
{
	explicit SAudioSwitchState(
	  EWwiseSwitchType const _type,
	  AkUInt32 const _switchId,
	  AkUInt32 const _stateId,
	  float const rtpcValue = 0.0f)
		: type(_type)
		, switchId(_switchId)
		, stateId(_stateId)
		, rtpcValue(rtpcValue)
	{}

	virtual ~SAudioSwitchState() {}

	EWwiseSwitchType const type;
	AkUInt32 const         switchId;
	AkUInt32 const         stateId;
	float const            rtpcValue;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioSwitchState);
	PREVENT_OBJECT_COPY(SAudioSwitchState);
};

enum EWwiseAudioEnvironmentType : AudioEnumFlagsType
{
	eWwiseAudioEnvironmentType_None = 0,
	eWwiseAudioEnvironmentType_AuxBus,
	eWwiseAudioEnvironmentType_Rtpc,
};

struct SAudioEnvironment final : public IAudioEnvironment
{
	explicit SAudioEnvironment(EWwiseAudioEnvironmentType const _type)
		: type(_type)
	{}

	explicit SAudioEnvironment(EWwiseAudioEnvironmentType const _type, AkAuxBusID const _busId)
		: type(_type)
		, busId(_busId)
	{
		CRY_ASSERT(_type == eWwiseAudioEnvironmentType_AuxBus);
	}

	explicit SAudioEnvironment(
	  EWwiseAudioEnvironmentType const _type,
	  AkRtpcID const _rtpcId,
	  float const _multiplier,
	  float const _shift)
		: type(_type)
		, rtpcId(_rtpcId)
		, multiplier(_multiplier)
		, shift(_shift)
	{
		CRY_ASSERT(_type == eWwiseAudioEnvironmentType_Rtpc);
	}

	virtual ~SAudioEnvironment() {}

	EWwiseAudioEnvironmentType const type;

	union
	{
		// Aux Bus implementation
		struct
		{
			AkAuxBusID busId;
		};

		// Rtpc implementation
		struct
		{
			AkRtpcID rtpcId;
			float    multiplier;
			float    shift;
		};
	};

	DELETE_DEFAULT_CONSTRUCTOR(SAudioEnvironment);
	PREVENT_OBJECT_COPY(SAudioEnvironment);
};

struct SAudioEvent final : public IAudioEvent
{
	explicit SAudioEvent(AudioEventId const _id)
		: audioEventState(eAudioEventState_None)
		, id(AK_INVALID_UNIQUE_ID)
		, audioEventId(_id)
	{}

	virtual ~SAudioEvent() {}

	EAudioEventState   audioEventState;
	AkUniqueID         id;
	AudioEventId const audioEventId;

	DELETE_DEFAULT_CONSTRUCTOR(SAudioEvent);
	PREVENT_OBJECT_COPY(SAudioEvent);
};

struct SAudioFileEntry final : public IAudioFileEntry
{
	SAudioFileEntry()
		: bankId(AK_INVALID_BANK_ID)
	{}

	virtual ~SAudioFileEntry() {}

	AkBankID bankId;

	PREVENT_OBJECT_COPY(SAudioFileEntry);
};

struct SAudioStandaloneFile final : public IAudioStandaloneFile
{
};
}
}
}
