// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

static const float PERCEPTION_SUSPECT_THR = 0.1f;
static const float PERCEPTION_INTERESTING_THR = 0.3f;
static const float PERCEPTION_THREATENING_THR = 0.6f;
static const float PERCEPTION_AGGRESSIVE_THR = 0.9f;

class CPuppet;

//! Temp target selection priority.
enum ETempTargetPriority
{
	eTTP_Last = 0,
	eTTP_OverSounds,
	eTTP_Always,
};

//! Potential target definition.
struct SAIPotentialTarget
{
	CCountedRef<CAIObject> refDummyRepresentation;

	EAITargetType          type; //!< Type of the event.

	float                  priority;       //!< Priority of the event.
	float                  upPriority;     //!< Extra priority set from outside the system.
	float                  upPriorityTime; //!< Timeout for the extra priority.

	float                  soundTime;        //!< Current time of the sound event counting down from maxTime.
	float                  soundMaxTime;     //!< Maximum duration of the sound event.
	float                  soundThreatLevel; //!< Threat level of the sound.
	Vec3                   soundPos;         //!< Position of the sound event.

	enum EVisualType
	{
		VIS_NONE,
		VIS_VISIBLE,
		VIS_MEMORY,
		VIS_LAST //!< Only for serialization.
	};

	float           visualThreatLevel; //!< Threat level of the sight.
	float           visualTime;        //!< Current time of the sight event counting down from maxTime.
	float           visualMaxTime;     //!< Max duration of the sight event.
	int             visualFrameId;
	Vec3            visualPos;      //!< Last seen position.
	bool            indirectSight;  //!< Seeing something related to the target (i.e. flashlight), but there is no direct LOS.
	EVisualType     visualType;     //!< Type of the sight event.

	float           exposure;       //!< Target exposure level, triggers the threat level up.
	EAITargetThreat threat;         //!< Threat level of the target.
	float           threatTimeout;  //!< Remaining time of the threat level.
	float           threatTime;     //!< Combined threat+threatTimeout in range [0..1].
	EAITargetThreat exposureThreat; //!< Threat level of the target exposure.

	bool            bNeedsUpdating;

	//! Returns the total time until the perception will time out.
	inline float GetTimeout(const AgentPerceptionParameters& pp) const
	{
		float timeout = 0.0f;
		if (threat == AITHREAT_AGGRESSIVE)
		{
			timeout += pp.forgetfulnessMemory;
			timeout += pp.forgetfulnessSeek;
		}
		else if (threat == AITHREAT_THREATENING)
		{
			timeout += pp.forgetfulnessMemory;
		}
		timeout += threatTimeout;
		return timeout;
	}

	SAIPotentialTarget() :
		type(AITARGET_NONE),
		priority(0.0f),
		soundTime(0.0f),
		soundMaxTime(0.0f),
		soundThreatLevel(0.0f),
		soundPos(ZERO),
		visualTime(0.0f),
		visualMaxTime(0.0f),
		visualThreatLevel(0.0f),
		visualPos(ZERO),
		visualType(VIS_NONE),
		exposure(0.0),
		threat(AITHREAT_NONE),
		exposureThreat(AITHREAT_NONE),
		threatTimeout(0.0f),
		threatTime(0.0f),
		upPriority(0.0f),
		upPriorityTime(0.0f),
		indirectSight(false),
		visualFrameId(0),
		bNeedsUpdating(false)
	{
	}

	void Serialize(TSerialize ser);
};

typedef std::map<CWeakRef<CAIObject>, SAIPotentialTarget> PotentialTargetMap;

//! Perception extra data interface.
struct IPerceptionExtraData
{
	// <interfuscator:shuffle>
	virtual ~IPerceptionExtraData() {}
	virtual bool SetData(uint32 uDataId, ScriptAnyValue dataAny) = 0;
	// </interfuscator:shuffle>
};

//! Perception handler interface.
struct IPerceptionHandler
{
	//! Unique identifier given when registered.
	enum { TypeId = -1 }; // ePHT_INVALID.
	// <interfuscator:shuffle>
	virtual ~IPerceptionHandler() {}

	virtual int  GetType() const { return TypeId; }

	virtual void SetOwner(CWeakRef<CPuppet> refOwner) = 0;
	virtual bool Serialize(TSerialize ser) { return false; }
	virtual void PostSerialize()           {}

	// Extra data
	virtual IPerceptionExtraData* GetExtraData() { return NULL; }

	// Event management
	virtual SAIPotentialTarget* AddEvent(CWeakRef<CAIObject> refObject, SAIPotentialTarget& targetInfo) = 0;
	virtual bool                RemoveEvent(CWeakRef<CAIObject> refObject) = 0;
	virtual CAIObject*          GetEventOwner(CWeakRef<CAIObject> refOwner) const = 0;

	// Target management
	virtual void ClearPotentialTargets() = 0;
	virtual bool GetPotentialTargets(PotentialTargetMap& targetMap) const = 0;
	virtual void UpTargetPriority(CWeakRef<CAIObject> refTarget, float fPriorityIncrement) = 0;
	virtual bool AddAggressiveTarget(CWeakRef<CAIObject> refTarget) = 0;
	virtual bool DropTarget(CWeakRef<CAIObject> refTarget) = 0;
	virtual bool SetTempTargetPriority(ETempTargetPriority priority) = 0;
	virtual bool UpdateTempTarget(const Vec3& vPosition) = 0;
	virtual bool ClearTempTarget() = 0;

	virtual bool GetBestTarget(CWeakRef<CAIObject>& refBestTarget, SAIPotentialTarget*& pTargetInfo, bool& bCurrentTargetErased) = 0;

	// Event handling
	virtual void HandleSoundEvent(SAIEVENT* pEvent) = 0;
	virtual void HandleVisualStimulus(SAIEVENT* pEvent) = 0;
	virtual void HandleBulletRain(SAIEVENT* pEvent) = 0;
	// </interfuscator:shuffle>
};

//! \endcond