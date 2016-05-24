// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>

class CAudioProxy final : public IAudioProxy
{
public:

	CAudioProxy();
	virtual ~CAudioProxy();

	//////////////////////////////////////////////////////////
	// IAudioProxy implementation
	virtual void          Initialize(char const* const szObjectName, bool const bInitAsync = true) override;
	virtual void          Release() override;
	virtual void          Reset() override;
	virtual void          PlayFile(SAudioPlayFileInfo const& playFileInfo, SAudioCallBackInfo const& callBackInfo = SAudioCallBackInfo::GetEmptyObject()) override;
	virtual void          StopFile(char const* const szFile) override;
	virtual void          ExecuteTrigger(AudioControlId const audioTriggerId, SAudioCallBackInfo const& callBackInfo = SAudioCallBackInfo::GetEmptyObject()) override;
	virtual void          StopTrigger(AudioControlId const audioTriggerId) override;
	virtual void          SetSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioStateId) override;
	virtual void          SetRtpcValue(AudioControlId const audioRtpcId, float const value) override;
	virtual void          SetOcclusionType(EAudioOcclusionType const occlusionType) override;
	virtual void          SetTransformation(Matrix34 const& transformation) override;
	virtual void          SetPosition(Vec3 const& position) override;
	virtual void          SetEnvironmentAmount(AudioEnvironmentId const audioEnvironmentId, float const amount) override;
	virtual void          SetCurrentEnvironments(EntityId const entityToIgnore = 0) override;
	virtual AudioObjectId GetAudioObjectId() const override { return m_audioObjectId; }
	//////////////////////////////////////////////////////////

	void ClearEnvironments();
	void ExecuteQueuedCommands();

	static AudioControlId     s_occlusionTypeSwitchId;
	static AudioSwitchStateId s_occlusionTypeStateIds[eAudioOcclusionType_Count];

private:

	void SetTransformationInternal(Matrix34 const& transformation);

	static size_t const        s_maxAreas = 10;

	AudioObjectId              m_audioObjectId;
	CAudioObjectTransformation m_transformation;
	AudioEnumFlagsType         m_flags;

	enum EQueuedAudioCommandType : AudioEnumFlagsType
	{
		eQueuedAudioCommandType_None                   = 0,
		eQueuedAudioCommandType_PlayFile               = 1,
		eQueuedAudioCommandType_StopFile               = 2,
		eQueuedAudioCommandType_ExecuteTrigger         = 3,
		eQueuedAudioCommandType_StopTrigger            = 4,
		eQueuedAudioCommandType_SetSwitchState         = 5,
		eQueuedAudioCommandType_SetRtpcValue           = 6,
		eQueuedAudioCommandType_SetPosition            = 7,
		eQueuedAudioCommandType_SetEnvironmentAmount   = 8,
		eQueuedAudioCommandType_SetCurrentEnvironments = 9,
		eQueuedAudioCommandType_ClearEnvironments      = 10,
		eQueuedAudioCommandType_Reset                  = 11,
		eQueuedAudioCommandType_Release                = 12,
		eQueuedAudioCommandType_Initialize             = 13,
	};

	struct SQueuedAudioCommand
	{
		explicit SQueuedAudioCommand(EQueuedAudioCommandType const _type)
			: type(_type)
			, audioTriggerId(INVALID_AUDIO_CONTROL_ID)
			, audioSwitchId(INVALID_AUDIO_CONTROL_ID)
			, audioSwitchStateId(INVALID_AUDIO_SWITCH_STATE_ID)
			, audioRtpcId(INVALID_AUDIO_CONTROL_ID)
			, floatValue(0.0f)
			, audioEnvironmentId(INVALID_AUDIO_ENVIRONMENT_ID)
			, entityId(((EntityId)(0)))
			, pOwnerOverride(nullptr)
			, pUserData(nullptr)
			, pUserDataOwner(nullptr)
			, requestFlags(eAudioRequestFlags_None)
			, transformation(IDENTITY)
		{}

		SQueuedAudioCommand& operator=(SQueuedAudioCommand const& other)
		{
			const_cast<EQueuedAudioCommandType&>(type) = other.type;
			audioTriggerId = other.audioTriggerId;
			audioSwitchId = other.audioSwitchId;
			audioSwitchStateId = other.audioSwitchStateId;
			audioRtpcId = other.audioRtpcId;
			floatValue = other.floatValue;
			audioEnvironmentId = other.audioEnvironmentId;
			entityId = other.entityId;
			pOwnerOverride = other.pOwnerOverride;
			pUserData = other.pUserData;
			pUserDataOwner = other.pUserDataOwner;
			requestFlags = other.requestFlags;
			stringValue = other.stringValue;
			transformation = other.transformation;

			return *this;
		}

		EQueuedAudioCommandType const type;
		AudioControlId                audioTriggerId;
		AudioControlId                audioSwitchId;
		AudioSwitchStateId            audioSwitchStateId;
		AudioControlId                audioRtpcId;
		float                         floatValue;
		AudioEnvironmentId            audioEnvironmentId;
		EntityId                      entityId;
		void*                         pOwnerOverride;
		void*                         pUserData;
		void*                         pUserDataOwner;
		AudioEnumFlagsType            requestFlags;
		string                        stringValue;
		Matrix34                      transformation;

		DELETE_DEFAULT_CONSTRUCTOR(SQueuedAudioCommand);
	};

	enum EAudioProxyFlags : AudioEnumFlagsType
	{
		eAudioProxyFlags_None         = 0,
		eAudioProxyFlags_WaitingForId = BIT(0),
	};

	typedef std::deque<SQueuedAudioCommand, STLSoundAllocator<SQueuedAudioCommand>> TQueuedAudioCommands;
	TQueuedAudioCommands m_queuedAudioCommands;

	void TryAddQueuedCommand(SQueuedAudioCommand const& command);

	//////////////////////////////////////////////////////////////////////////
	struct SFindSetSwitchState
	{
		explicit SFindSetSwitchState(AudioControlId const _audioSwitchId, AudioSwitchStateId const _audioSwitchStateId)
			: audioSwitchId(_audioSwitchId)
			, audioSwitchStateId(_audioSwitchStateId)
		{}

		inline bool operator()(SQueuedAudioCommand& command)
		{
			bool bFound = false;

			if (command.type == eQueuedAudioCommandType_SetSwitchState && command.audioSwitchId == audioSwitchId)
			{
				// Command for this switch exists, just update the state.
				command.audioSwitchStateId = audioSwitchStateId;
				bFound = true;
			}

			return bFound;
		}

	private:

		AudioControlId const     audioSwitchId;
		AudioSwitchStateId const audioSwitchStateId;

		DELETE_DEFAULT_CONSTRUCTOR(SFindSetSwitchState);
	};

	//////////////////////////////////////////////////////////////////////////
	struct SFindSetRtpcValue
	{
		explicit SFindSetRtpcValue(AudioControlId const _audioRtpcId, float const _audioRtpcValue)
			: audioRtpcId(_audioRtpcId)
			, audioRtpcValue(_audioRtpcValue)
		{}

		inline bool operator()(SQueuedAudioCommand& command)
		{
			bool bFound = false;

			if (command.type == eQueuedAudioCommandType_SetRtpcValue && command.audioRtpcId == audioRtpcId)
			{
				// Command for this RTPC exists, just update the value.
				command.floatValue = audioRtpcValue;
				bFound = true;
			}

			return bFound;
		}

	private:

		AudioControlId const audioRtpcId;
		float const          audioRtpcValue;

		DELETE_DEFAULT_CONSTRUCTOR(SFindSetRtpcValue);
	};

	//////////////////////////////////////////////////////////////////////////
	struct SFindSetPosition
	{
		explicit SFindSetPosition(Matrix34 const& _position)
			: position(_position)
		{}

		inline bool operator()(SQueuedAudioCommand& command)
		{
			bool bFound = false;

			if (command.type == eQueuedAudioCommandType_SetPosition)
			{
				// Command exists already, just update the position.
				command.transformation = position;
				bFound = true;
			}

			return bFound;
		}

	private:

		Matrix34 const& position;

		DELETE_DEFAULT_CONSTRUCTOR(SFindSetPosition);
	};

	//////////////////////////////////////////////////////////////////////////
	struct SFindSetEnvironmentAmount
	{
		explicit SFindSetEnvironmentAmount(AudioEnvironmentId const _audioEnvironmentId, float const _audioEnvironmentValue)
			: audioEnvironmentId(_audioEnvironmentId)
			, audioEnvironmentValue(_audioEnvironmentValue)
		{}

		inline bool operator()(SQueuedAudioCommand& command)
		{
			bool bFound = false;

			if (command.type == eQueuedAudioCommandType_SetEnvironmentAmount && command.audioEnvironmentId == audioEnvironmentId)
			{
				// Command for this environment exists, just update the value.
				command.floatValue = audioEnvironmentValue;
				bFound = true;
			}

			return bFound;
		}

	private:

		AudioEnvironmentId const audioEnvironmentId;
		float const              audioEnvironmentValue;

		DELETE_DEFAULT_CONSTRUCTOR(SFindSetEnvironmentAmount);
	};

	//////////////////////////////////////////////////////////////////////////
	struct SFindCommand
	{
		explicit SFindCommand(EQueuedAudioCommandType const _audioQueuedCommandType)
			: audioQueuedCommandType(_audioQueuedCommandType)
		{}

		inline bool operator()(SQueuedAudioCommand const& command)
		{
			return command.type == audioQueuedCommandType;
		}

	private:

		EQueuedAudioCommandType const audioQueuedCommandType;

		DELETE_DEFAULT_CONSTRUCTOR(SFindCommand);
	};
};
