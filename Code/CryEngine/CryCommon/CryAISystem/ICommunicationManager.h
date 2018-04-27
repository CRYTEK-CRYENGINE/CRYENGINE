// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ICommunicationManager_h__
#define __ICommunicationManager_h__

#pragma  once

enum ECommTags
{
	CommConfigTag = 0,
	CommChannelTag,
	CommTag,
	CommPlayTag,
};

template<ECommTags T>
struct CommHandle
{
	explicit CommHandle(uint32 id = 0) : id(id) {}

	CommHandle& operator=(const CommHandle& other) { id = other.id; return *this; }

	operator bool() const { return id != 0; }
	bool operator==(const CommHandle& other) const { return id == other.id; }
	bool operator!=(const CommHandle& other) const { return id != other.id; }
	bool operator<(const CommHandle& other) const  { return id < other.id; }

	void Serialize(TSerialize ser)
	{
		ser.Value("id", id);
	}

	uint32 id;
};

typedef CommHandle<CommConfigTag>  CommConfigID;
typedef CommHandle<CommChannelTag> CommChannelID;
typedef CommHandle<CommTag>        CommID;
typedef CommHandle<CommPlayTag>    CommPlayID;

struct SRestrictedActorParams;
struct SCommunicationRequest;

struct ICommunicationPopulateCallBack
{
	// <interfuscator:shuffle>
	//! Callback function to retrieve all communication names
	//! Use it in conjunction with ICommunicationManager::EnumerateActions / ICommunicationManager::GetCommunicationsCount.
	//! \param pName Name of one of the communications retrieved.
	virtual void AddCommunicationName(const char* const pName) = 0;
	virtual ~ICommunicationPopulateCallBack(){}
	// </interfuscator:shuffle>
};

struct ICommunicationManager
{
	struct WWiseConfiguration
	{
		WWiseConfiguration()
			: prefixForPlayTrigger("")
			, prefixForStopTrigger("")
			, switchNameForCharacterVoice("")
			, switchNameForCharacterType("")
		{
			prefixForPlayTrigger.reserve(16);
			prefixForStopTrigger.reserve(16);
			switchNameForCharacterVoice.reserve(64);
			switchNameForCharacterType.reserve(64);
		}

		string prefixForPlayTrigger;
		string prefixForStopTrigger;
		string switchNameForCharacterVoice;
		string switchNameForCharacterType;
	};

	enum ECommunicationEvent
	{
		CommunicationQueued = 0,
		CommunicationExpired,
		CommunicationStarted,
		CommunicationCancelled,
		CommunicationFinished,
	};

	struct ICommInstanceListener
	{
		// <interfuscator:shuffle>
		virtual ~ICommInstanceListener(){}
		virtual void OnCommunicationEvent(ECommunicationEvent event, EntityId actorID, const CommPlayID& playID) = 0;
		// </interfuscator:shuffle>
	};

	struct ICommGlobalListener
	{
		// <interfuscator:shuffle>
		virtual ~ICommGlobalListener(){}
		virtual void OnCommunicationEvent(ECommunicationEvent event, EntityId actorID, const CommID& commID) = 0;
		// </interfuscator:shuffle>
	};

	// <interfuscator:shuffle>
	virtual ~ICommunicationManager(){}
	virtual uint32        GetConfigCount() const = 0;
	virtual const char*   GetConfigName(uint32 index) const = 0;
	virtual CommConfigID  GetConfigIDByIndex(uint32 index) const = 0;

	virtual CommChannelID GetChannelID(const char* name) const = 0;
	virtual CommConfigID  GetConfigID(const char* name) const = 0;
	virtual CommID        GetCommunicationID(const char* name) const = 0;
	virtual const char*   GetCommunicationName(const CommID& communicationID) const = 0;

	virtual bool          CanCommunicationPlay(const SCommunicationRequest& request, float* estimatedWaitTime = 0) = 0;
	virtual CommPlayID    PlayCommunication(SCommunicationRequest& request) = 0;
	virtual void          StopCommunication(const CommPlayID& playID) = 0;
	virtual bool          IsPlaying(const CommPlayID& playID, float* timeRemaining = 0) const = 0;
	virtual bool          IsQueued(const CommPlayID& playID, float* estimatedWaitTime = 0) const = 0;

	virtual void          RegisterListener(ICommGlobalListener* eventListener, const char* name = NULL) = 0;
	virtual void          UnregisterListener(ICommGlobalListener* eventListener) = 0;
	virtual void          RemoveInstanceListener(const CommPlayID& playID) = 0;

	virtual uint32        GetConfigCommunicationCount(const CommConfigID& configID) const = 0;
	virtual const char*   GetConfigCommunicationNameByIndex(const CommConfigID& configID, uint32 index) const = 0;

	//! Sets silence duration for actors, exluding them from communication sounds/animations for the length of the duration.
	virtual void SetRestrictedDuration(EntityId actorId, float voiceDuration, float animDuration) = 0;

	//! Adds restriction on actor, excluding them from communiction sounds/animations until explicitly removed.
	virtual void AddActorRestriction(EntityId actorId, bool restrictVoice, bool restrictAnimation) = 0;

	//! Removes restriction on actor, if no more restrictions are present actor will be available for communication sounds/animations.
	virtual void                      RemoveActorRestriction(EntityId actorId, bool unrestrictVoice, bool unrestrictAnimation) = 0;

	virtual const WWiseConfiguration& GetWiseConfiguration() const = 0;

	virtual void                      SetVariableValue(const char* variableName, const bool newVariableValue) = 0;
	virtual void                      GetVariablesNames(const char** variableNames, const size_t maxSize, size_t& actualSize) const = 0;
	// </interfuscator:shuffle>
};

struct SRestrictedActorParams
{
	SRestrictedActorParams() : m_animRestrictedTime(0.0f), m_voiceRestrictedTime(0.0f), m_animRestricted(false), m_voiceRestricted(false){}

	bool IsRestricted() const          { return (IsVoiceRestricted() || IsAnimationRestricted()); }
	bool IsVoiceRestricted() const     { return (m_voiceRestrictedTime > 0) || m_voiceRestricted; }
	bool IsAnimationRestricted() const { return (m_animRestrictedTime > 0) || m_animRestricted; }

	void Update(float deltaTime)
	{
		if (m_animRestrictedTime > 0.0f)
			m_animRestrictedTime -= deltaTime;
		if (m_voiceRestrictedTime > 0.0f)
			m_voiceRestrictedTime -= deltaTime;
	}

	float m_animRestrictedTime;
	float m_voiceRestrictedTime;

	int   m_animRestricted;
	int   m_voiceRestricted;
};

struct SCommunicationRequest
{
	SCommunicationRequest()
		: configID(0)
		, channelID(0)
		, commID(0)
		, actorID(0)
		, targetID(0)
		, target(ZERO)
		, ordering(Unordered)
		, contextExpiry(0.0f)
		, minSilence(-1.0f)
		, eventListener(0)
		, skipCommSound(false)
		, skipCommAnimation(false)
	{
	}

	enum EOrdering
	{
		Ordered   = 0,
		Unordered = 1,
	};

	CommConfigID  configID;
	CommChannelID channelID;
	CommID        commID;

	EntityId      actorID;

	EntityId      targetID;
	Vec3          target;

	EOrdering     ordering;
	float         contextExpiry;

	float         minSilence;

	bool          skipCommSound;
	bool          skipCommAnimation;

	ICommunicationManager::ICommInstanceListener* eventListener;
};

#endif
