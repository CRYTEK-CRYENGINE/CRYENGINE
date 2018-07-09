// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef PUPPET_H
#define PUPPET_H

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "PipeUser.h"
#include "PathObstacles.h"
#include <CryAISystem/ValueHistory.h>
#include <CryAISystem/IPerceptionHandler.h>
#include <CryAISystem/IVisionMap.h>
#include "PostureManager.h"

// forward declarations
class CPersonalInterestManager;
class CFireCommandGrenade;
struct PendingDeathReaction;

// Quick prototype of a signal/state container
class CSignalState
{
public:
	bool bState;
	bool bLastUpdatedState;
	CSignalState() : bState(false), bLastUpdatedState(false) {}
	bool CheckUpdate()
	{
		bool bRet = bState != bLastUpdatedState;
		bLastUpdatedState = bState;
		return bRet;
	}
};

struct CSpeedControl
{
	float      fPrevDistance;
	CTimeValue fLastTime;
	Vec3       vLastPos;
	//	CAIPIDController PIDController;
	//	static const float	m_CMaxDist;

	CSpeedControl() : vLastPos(0.0f, 0.0f, 0.0f), fPrevDistance(0.0f){}
	void Reset(const Vec3 vPos, CTimeValue fTime)
	{
		fPrevDistance = 0;
		fLastTime = fTime;
		vLastPos = vPos;
	}
};

typedef std::multimap<float, SHideSpot>                    MultimapRangeHideSpots;
typedef std::map<CWeakRef<CAIObject>, float>               DevaluedMap;
typedef std::map<CWeakRef<CAIObject>, CWeakRef<CAIObject>> ObjectObjectMap;

struct SShootingStatus
{
	bool      triggerPressed;             // True if the weapon trigger is being pressed.
	float     timeSinceTriggerPressed;    // Time in seconds since last time trigger is pressed.
	bool      friendOnWay;                // True if the fire if blocked by friendly unit.
	float     friendOnWayElapsedTime;     // Time in seconds the line of fire is blocked by friendly unit.
	EFireMode fireMode;                   // Fire mode status, see EFireMode.
};

struct SSoundPerceptionDescriptor
{
	float fMinDist;         // Minimum distance from sound origin to recognize it
	float fRadiusScale;     // Scalar value applied to radius to amplify or reduce the effect of the sound
	float fSoundTime;       // How long to remember the sound
	float fBaseThreat;      // Base threat value
	float fLinStepMin;      // Linear step ranges used based on ratio of distance to sound and sound radius
	float fLinStepMax;

	SSoundPerceptionDescriptor(float _fMinDist = 0.0f, float _fRadiusScale = 1.0f, float _fSoundTime = 0.0f, float _fBaseThreat = 0.0f, float _fLinStepMin = 0.0f, float _fLinStepMax = 1.0f)
	{
		Set(_fMinDist, _fRadiusScale, _fSoundTime, _fBaseThreat, _fLinStepMin, _fLinStepMax);
	}
	void Set(float _fMinDist = 0.0f, float _fRadiusScale = 1.0f, float _fSoundTime = 0.0f, float _fBaseThreat = 0.0f, float _fLinStepMin = 0.0f, float _fLinStepMax = 1.0f)
	{
		fMinDist = _fMinDist;
		fRadiusScale = _fRadiusScale;
		fSoundTime = _fSoundTime;
		fBaseThreat = _fBaseThreat;
		fLinStepMin = _fLinStepMin;
		fLinStepMax = _fLinStepMax;
	}
};

struct SSortedHideSpot
{
	SSortedHideSpot(float weight, SHideSpot* pHideSpot) : weight(weight), pHideSpot(pHideSpot) {}
	inline bool operator<(const SSortedHideSpot& rhs) const { return weight > rhs.weight; } // highest weight first.
	float      weight;
	SHideSpot* pHideSpot;
};

class CPuppet
	: public CPipeUser
	  , public IPuppet
{
	friend class CAISystem;
public:
	CPuppet();
	virtual ~CPuppet();

	static void                  ClearStaticData();

	virtual const IPuppet*       CastToIPuppet() const { return this; }
	virtual IPuppet*             CastToIPuppet()       { return this; }

	inline PostureManager&       GetPostureManager()   { return m_postureManager; };

	virtual void                 ResetPerception();

	virtual bool                 CanDamageTarget(IAIObject* target = 0) const;
	virtual bool                 CanDamageTargetWithMelee() const;

	void                         AdjustSpeed(CAIObject* pNavTarget, float distance = 0);
	void                         ResetSpeedControl();
	virtual bool                 GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const;
	virtual bool                 GetTeleportPosition(Vec3& teleportPos) const;
	virtual bool                 GetPosAlongPath(float dist, bool extrapolateBeyond, Vec3& retPos) const;
	virtual IFireCommandHandler* GetFirecommandHandler() const                 { return m_pFireCmdHandler; }

	void                         SetAllowedToHitTarget(bool state)             { m_allowedToHitTarget = state; }
	bool                         IsAllowedToHitTarget() const                  { return m_allowedToHitTarget; }

	void                         SetAllowedToUseExpensiveAccessory(bool state) { m_allowedToUseExpensiveAccessory = state; }
	bool                         IsAllowedToUseExpensiveAccessory() const      { return m_allowedToUseExpensiveAccessory; }

	// Returns the AI object to shoot at.
	CAIObject* GetFireTargetObject() const;

	// Returns the reaction time of between allowed to fire and allowed to damage target.
	float GetFiringReactionTime(const Vec3& targetPos) const;

	// Returns true if the reaction time pas passed.
	float GetCurrentFiringReactionTime() const { return m_firingReactionTime; }
	bool  HasFiringReactionTimePassed() const  { return m_firingReactionTimePassed; }

	// Returns the current status shooting related parameters.
	void GetShootingStatus(SShootingStatus& ss);

	// Sets the allowed start and end of the path strafing distance.
	void SetAllowedStrafeDistances(float start, float end, bool whileMoving);

	// Sets the allowed start and end of the path strafing distance.
	void SetAdaptiveMovementUrgency(float minUrgency, float maxUrgency, float scaleDownPathlen);

	// Sets the stance that will be used when the character starts to move.
	void SetDelayedStance(int stance);

	void AdjustMovementUrgency(float& urgency, float pathLength, float* maxPathLen = 0);

	/// Returns all the path obstacles that might affect paths. If allowRecalc = false then the cached obstacles
	/// will not be updated
	const CPathObstacles& GetPathAdjustmentObstacles(bool allowRecalc = true) { if (allowRecalc) CalculatePathObstacles(); return m_pathAdjustmentObstacles; }
	const CPathObstacles& GetLastPathAdjustmentObstacles() const              { return m_pathAdjustmentObstacles; }

	// current accuracy (with soft cover and motion correction, distance/stance, ....)
	float GetAccuracy(const CAIObject* pTarget) const;

	/// inherited
	virtual DamagePartVector* GetDamageParts();

	// Returns the memory usage of the object.
	size_t MemStats();

	// Inherited from IPuppet
	virtual void       UpTargetPriority(const IAIObject* pTarget, float fPriorityIncrement);
	virtual void       UpdateBeacon();
	virtual IAIObject* MakeMeLeader();
	virtual bool       CheckFriendsInLineOfFire(const Vec3& fireDir, bool cheapTest);

	// Modify perception descriptions for visual and sound stimulus
	bool               GetSoundPerceptionDescriptor(EAISoundStimType eType, SSoundPerceptionDescriptor& sDescriptor) const;
	bool               SetSoundPerceptionDescriptor(EAISoundStimType eType, const SSoundPerceptionDescriptor& sDescriptor);

	virtual IAIObject* GetEventOwner(IAIObject* pObject) const;
	virtual CAIObject* GetEventOwner(CWeakRef<CAIObject> refOwned) const;

	// trace from weapon pos to target body/eye,
	// returns true if good to shoot
	// in:	pTarget
	//			lowDamage	- low accuracy, should hit arms/legs if possible
	// out: vTargetPos, vTargetDir, distance
	bool         CheckAndGetFireTarget_Deprecated(IAIObject* pTarget, bool lowDamage, Vec3& vTargetPos, Vec3& vTargetDir) const;
	virtual Vec3 ChooseMissPoint_Deprecated(const Vec3& targetPos) const;

	// Inherited from IPipeUser
	virtual void Update(EUpdateType type);
	virtual void UpdateProxy(EUpdateType type);
	virtual void Devalue(IAIObject* pObject, bool bDevaluePuppets, float fAmount = 20.f);
	virtual bool IsDevalued(IAIObject* pObject);
	virtual void ClearDevalued();

	// Inherited from IAIObject
	virtual void SetParameters(const AgentParameters& sParams);
	virtual void Event(unsigned short eType, SAIEVENT* pEvent);
	virtual void ParseParameters(const AIObjectParams& params, bool bParseMovementParams = true);
	virtual bool CreateFormation(const char* szName, Vec3 vTargetPos);
	virtual void Serialize(TSerialize ser);
	virtual void PostSerialize();

	// Inherited from CAIObject
	virtual void OnObjectRemoved(CAIObject* pObject);
	virtual void Reset(EObjectResetType type);

	// <title GetDistanceAlongPath>
	// Description: returns the distance of a point along the puppet's path
	// Arguments:
	// Vec3& pos: point
	// bool bInit: if true, the current path is stored in a internal container; if false, the distance is computed
	//		along the stored path, no matter if the current path is different
	// Returns:
	// distance of point along Puppets path; distance value would be negative if the point is ahead along the path
	virtual float GetDistanceAlongPath(const Vec3& pos, bool bInit);

	bool          GetPotentialTargets(PotentialTargetMap& targetMap) const;

	uint32        GetBestTargets(tAIObjectID* targets, uint32 maxCount) const;

	bool          AddAggressiveTarget(IAIObject* pTarget);
	bool          SetTempTargetPriority(ETempTargetPriority priority);
	bool          UpdateTempTarget(const Vec3& vPosition);
	bool          ClearTempTarget();
	bool          DropTarget(IAIObject* pTarget);

	virtual void  SetRODHandler(IAIRateOfDeathHandler* pHandler);
	virtual void  ClearRODHandler();

	virtual bool  GetPerceivedTargetPos(IAIObject* pTarget, Vec3& vPos) const;

	virtual void  UpdateLookTarget(CAIObject* pTarget);
	virtual void  UpdateLookTarget3D(CAIObject* pTarget);
	virtual bool  NavigateAroundObjects(const Vec3& targetPos, bool fullUpdate);

	void          SetForcedNavigation(const Vec3& vDirection, float fSpeed);
	void          ClearForcedNavigation();

	void          SetVehicleStickTarget(EntityId targetId) { m_vehicleStickTarget = targetId; }
	EntityId      GetVehicleStickTarget() const            { return m_vehicleStickTarget; }

	/// Like NavigateAroundObjects but navigates around an individual AI obstacle (implemented by the puppet/vehicle).
	/// If steer is true then navigation should be done by steering. If false then navigation should be
	/// done by slowing down/stopping.
	virtual bool NavigateAroundAIObject(const Vec3& targetPos, const CAIObject* obstacle, const Vec3& predMyPos, const Vec3& predObjectPos, bool steer, bool in3D);

	// Change flag so this puppet can be shoot or not
	virtual void SetCanBeShot(bool bCanBeShot);
	virtual bool GetCanBeShot() const;

	// Change flag so this puppet can fire at its memory target always
	virtual void            SetMemoryFireType(EMemoryFireType eType);
	virtual EMemoryFireType GetMemoryFireType() const;
	virtual bool            CanMemoryFire() const;

	// Check if can fire at my target from this given stance
	bool CanFireInStance(EStance stance, float fDistanceRatio = 0.9f) const;

	// Request secondary grenade fire (no interruption to fire mode)
	// iRegType - Who to throw at (See AI_REG_*)
	void RequestThrowGrenade(ERequestedGrenadeType eGrenadeType, int iRegType);

	// Returns the current alertness level of the puppet.
	inline int                       GetAlertness() const { return m_Alertness; }

	virtual float                    AdjustTargetVisibleRange(const CAIActor& observer, float fVisibleRange) const;

	inline EPuppetUpdatePriority     GetUpdatePriority() const                    { return m_updatePriority; }
	inline void                      SetUpdatePriority(EPuppetUpdatePriority pri) { m_updatePriority = pri; }

	const AIWeaponDescriptor&        QueryCurrentWeaponDescriptor(bool bIsSecondaryFire = false, ERequestedGrenadeType prefGrenadeType = eRGT_ANY);
	virtual const AIWeaponDescriptor& GetCurrentWeaponDescriptor() const { return m_CurrentWeaponDescriptor; }

	// Returns true if it is possible to aim at the target without colliding with a wall etc in front of the puppet.
	bool         CanAimWithoutObstruction(const Vec3& vTargetPos);
	bool         CheckLineOfFire(const Vec3& vTargetPos, float fDistance, float fSoftDistance, EStance stance = STANCE_NULL) const;

	float        GetTimeToNextShot() const;

	virtual void MakeIgnorant(bool bIgnorant) { m_bCanReceiveSignals = !bIgnorant; }

	// Perception handler modifiers
	// [AlexMcC|26.04.10]: These won't work with CTargetTrackManager
	bool AddPerceptionHandlerModifier(IPerceptionHandlerModifier* pModifier);
	bool RemovePerceptionHandlerModifier(IPerceptionHandlerModifier* pModifier);
	bool GetPerceptionHandlerModifiers(TPerceptionHandlerModifiersVector& outModifiers);
	// Debug draw for perception handler modifiers
	void DebugDrawPerceptionHandlerModifiers();

	IFireCommandHandler* m_pFireCmdHandler;
	CFireCommandGrenade* m_pFireCmdGrenade;

	float                m_targetApproach;
	float                m_targetFlee;
	bool                 m_targetApproaching;
	bool                 m_targetFleeing;
	bool                 m_lastTargetValid;
	Vec3                 m_lastTargetPos;
	float                m_lastTargetSpeed;
	CSignalState         m_attTargetOutOfTerritory;

	bool                 m_bCanReceiveSignals;

protected:
	typedef enum
	{
		PA_NONE = 0,
		PA_LOOKING,
		PA_STICKING
	} TPlayerActionType;

	struct STargetSelectionInfo
	{
		CWeakRef<CAIObject> bestTarget;
		EAITargetThreat     targetThreat;
		EAITargetType       targetType;
		SAIPotentialTarget* pTargetInfo;
		bool                bCurrentTargetErased;
		bool                bIsGroupTarget;

		STargetSelectionInfo() : targetThreat(AITHREAT_NONE), targetType(AITARGET_NONE), pTargetInfo(),
			bCurrentTargetErased(false), bIsGroupTarget(false) {}
	};

	void                CheckAwarenessPlayer(); // check if the player is sticking or looking at me

	SAIPotentialTarget* AddEvent(CWeakRef<CAIObject> refObject, SAIPotentialTarget& ed);
	//	float CalculatePriority(CAIObject * pTarget);

	void UpdatePuppetInternalState();
	bool UpdateTargetSelection(STargetSelectionInfo& targetSelectionInfo);
	bool GetTargetTrackBestTarget(CWeakRef<CAIObject>& refBestTarget, SAIPotentialTarget*& pTargetInfo,
	                              bool& bCurrentTargetErased) const;

	void         HandleSoundEvent(SAIEVENT* pEvent);
	void         HandlePathDecision(MNMPathRequestResult& result);
	void         HandleVisualStimulus(SAIEVENT* pEvent);
	virtual void HandleBulletRain(SAIEVENT* pEvent);

	//	float GetEventPriority(const CAIObject *pObj,const EventData &ed) const;
	void UpdateAlertness();
	void ResetAlertness();

	// Steers the puppet around the object and makes it avoid the immediate obstacles. Returns true if steering is
	// being done
	bool            SteerAroundVehicle(const Vec3& targetPos, const CAIObject* object, const Vec3& predMyPos, const Vec3& predObjectPos);
	bool            SteerAroundPuppet(const Vec3& targetPos, const CAIObject* object, const Vec3& predMyPos, const Vec3& predObjectPos);
	bool            SteerAround3D(const Vec3& targetPos, const CAIObject* object, const Vec3& predMyPos, const Vec3& predObjectPos);
	/// Helper for NavigateAroundObjects
	bool            NavigateAroundObjectsInternal(const Vec3& targetPos, const Vec3& myPos, bool in3D, const CAIObject* object);
	/// Helper for NavigateAroundObjects
	ENavInteraction NavigateAroundObjectsBasicCheck(const CAIObject* object) const;
	bool            NavigateAroundObjectsBasicCheck(const Vec3& targetPos, const Vec3& myPos, bool in3D, const CAIObject* object, float extraDist);

	// helper to judge what type of fire command we are using
	bool IsSecondaryFireCommand() const { return m_fireMode == FIREMODE_SECONDARY || m_fireMode == FIREMODE_SECONDARY_SMOKE; }
	bool IsMeleeFireCommand() const     { return m_fireMode == FIREMODE_MELEE || m_fireMode == FIREMODE_MELEE_FORCED; }

	// decides whether to fire or not
	void FireCommand(float updateTime);
	// process secondary fire
	void FireSecondary(CAIObject* pTarget, ERequestedGrenadeType prefGrenadeType = eRGT_ANY);
	// process melee fire
	void FireMelee(CAIObject* pTarget);

	bool CheckTargetInRange(Vec3& vTargetPos);

	// Evaluates whether the chosen navigation point will expose us too much to the target
	bool Compromising(const Vec3& pos, const Vec3& dir, const Vec3& hideFrom, const Vec3& objectPos, const Vec3& searchPos, bool bIndoor, bool bCheckVisibility) const;

	void UpdateTargetMovementState();

	bool IsFriendInLineOfFire(CAIObject* pFriend, const Vec3& firePos, const Vec3& fireDirection, bool cheapTest);

	void AdjustWithPrediction(CAIObject* pTarget, Vec3& posOut);

	// Returns true if it pActor is obstructing aim.
	bool                      ActorObstructingAim(const CAIActor* pActor, const Vec3& firePos, const Vec3& dir, const Ray& fireRay) const;

	CPersonalInterestManager* GetPersonalInterestManager();

	CTimeValue        m_fLastUpdateTime;

	bool              m_bDryUpdate;

	DevaluedMap       m_mapDevaluedPoints;

	int               m_Alertness;

	float             m_fLastTimeAwareOfPlayer;
	TPlayerActionType m_playerAwarenessType;

	bool              m_allowedToHitTarget;
	bool              m_allowedToUseExpensiveAccessory;
	bool              m_firingReactionTimePassed;
	float             m_firingReactionTime;
	float             m_outOfAmmoTimeOut;

	bool              m_bWarningTargetDistance;

	CSpeedControl     m_SpeedControl;

	// for smoothing the output of AdjustSpeed
	float                m_chaseSpeed;
	float                m_chaseSpeedRate;
	int                  m_lastChaseUrgencyDist;
	int                  m_lastChaseUrgencySpeed;

	CWeakRef<CAIVehicle> m_refAvoidedVehicle;
	CTimeValue           m_vehicleAvoidingTime;

	Vec3                 m_vForcedNavigation;
	float                m_fForcedNavigationSpeed;

	EntityId             m_vehicleStickTarget;

	// Current actor weapon descriptor
	EntityId           m_currentWeaponId;
	AIWeaponDescriptor m_CurrentWeaponDescriptor;

	//request throw grenade data
	bool                  m_bGrenadeThrowRequested;
	ERequestedGrenadeType m_eGrenadeThrowRequestType;
	int                   m_iGrenadeThrowTargetType;

	float                 m_allowedStrafeDistanceStart; // Allowed strafe distance at the start of the path
	float                 m_allowedStrafeDistanceEnd;   // Allowed strafe distance at the end of the path
	bool                  m_allowStrafeLookWhileMoving; // Allow strafing when moving along path.
	bool                  m_closeRangeStrafing;

	float                 m_strafeStartDistance;

	float                 m_adaptiveUrgencyMin;
	float                 m_adaptiveUrgencyMax;
	float                 m_adaptiveUrgencyScaleDownPathLen;
	float                 m_adaptiveUrgencyMaxPathLen;

	int                   m_delayedStance;
	int                   m_delayedStanceMovementCounter;

	enum ESeeTargetFrom
	{
		ST_HEAD,
		ST_WEAPON,
		ST_OFSETTED_LEFT,
		ST_OFSETTED_RIGHT,
	};
	float                  m_lastTimeSeeFromHead;

	float                  m_timeSinceTriggerPressed;
	float                  m_friendOnWayElapsedTime;

	bool                   m_bCoverFireEnabled;

	bool                   m_bCanBeShot;

	EMemoryFireType        m_eMemoryFireType;

	DamagePartVector       m_damageParts;

	IPerceptionHandler*    m_pPerceptionHandler;
	IAIRateOfDeathHandler* m_pRODHandler;

	typedef std::vector<Vec3> TPointList;
	TPointList m_InitialPath;

	void UpdateHealthTracking();

public:

	void ResetTargetTracking();
	Vec3 UpdateTargetTracking(CWeakRef<CAIObject> refTarget, const Vec3& vTargetPos);
	void UpdateFireReactionTimer(const Vec3& vTargetPos);
	void UpdateTargetZone(CWeakRef<CAIObject> refTarget);

protected:

	struct LineOfFireState
	{
		LineOfFireState()
			: asyncState(AsyncReady)
			, rayID(0)
			, result(false)
			, softDistance(0.0f)
		{
		}

		~LineOfFireState()
		{
			if (rayID)
				gAIEnv.pRayCaster->Cancel(rayID);
		}

		void Swap(LineOfFireState& other)
		{
			std::swap(asyncState, other.asyncState);
			std::swap(rayID, other.rayID);
			std::swap(softDistance, other.softDistance);
			std::swap(result, other.result);
		}

		AsyncState  asyncState;
		QueuedRayID rayID;
		float       softDistance;
		bool        result;
	};

	mutable LineOfFireState m_lineOfFireState;
	void LineOfFireRayComplete(const QueuedRayID& rayID, const RayCastResult& result);

	struct ValidTargetState
	{
		ValidTargetState()
			: asyncState(AsyncReady)
			, rayID(0)
			, latestHitDist(FLT_MAX)
		{
		}

		~ValidTargetState()
		{
			if (rayID)
				gAIEnv.pRayCaster->Cancel(rayID);
		}

		void Swap(ValidTargetState& other)
		{
			std::swap(asyncState, other.asyncState);
			std::swap(rayID, other.rayID);
			std::swap(latestHitDist, other.latestHitDist);
		}

		AsyncState  asyncState;
		QueuedRayID rayID;
		float       latestHitDist;
	};

	ValidTargetState m_validTargetState;
	void FireTargetValidRayComplete(const QueuedRayID& rayID, const RayCastResult& result);
	void QueueFireTargetValidRay(const CAIObject* targetObj, const Vec3& firePos, const Vec3& fireDir);

public:

	bool AdjustFireTarget(CAIObject* targetObject, const Vec3& target, bool hit, float missExtraOffset,
	                      float clampAngle, Vec3* posOut);
	bool CalculateHitPointOnTarget(CAIObject* targetObject, const Vec3& target, float clampAngle, Vec3* posOut);
	bool CalculateMissPointOutsideTargetSilhouette(CAIObject* targetObject, const Vec3& target, float missExtraOffset,
	                                               Vec3* posOut);

	bool  IsFireTargetValid(const Vec3& pos, const CAIObject* pTargetObject);
	float GetCoverFireTime() const;
	float GetBurstFireDistanceScale() const;

	float GetTargetAliveTime();

	Vec3  InterpolateLookOrAimTargetPos(const Vec3& current, const Vec3& target, float maxRate);

	void  HandleBurstFireInit();
	void  HandleWeaponEffectBurstDrawFire(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void  HandleWeaponEffectBurstSnipe(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void  HandleWeaponEffectPanicSpread(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void  HandleWeaponEffectAimSweep(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);

	struct STargetSilhouette
	{
		STargetSilhouette()
		{
			Reset();
			baseMtx.SetIdentity();
		}

		bool              valid;
		std::vector<Vec3> points;
		Matrix33          baseMtx;
		Vec3              center;

		inline Vec3       ProjectVectorOnSilhouette(const Vec3& vec)
		{
			return Vec3(baseMtx.GetColumn0().Dot(vec), baseMtx.GetColumn2().Dot(vec), 0.0f);
		}

		inline Vec3 IntersectSilhouettePlane(const Vec3& from, const Vec3& to)
		{
			if (!valid)
				return to;

			// Intersect (infinite) segment with the silhouette plane.
			Vec3 pn = baseMtx.GetColumn1();
			float pd = -pn.Dot(center);
			Vec3 dir = to - from;

			float d = pn.Dot(dir);
			if (fabsf(d) < 1e-6)
				return to;

			float n = pn.Dot(from) + pd;
			return from + dir * (-n / d);
		}

		void Reset()
		{
			valid = false;
			points.clear();
			center.zero();
		}
	};

	STargetSilhouette     m_targetSilhouette;
	Vec3                  m_targetLastMissPoint;
	Vec3                  m_targetPosOnSilhouettePlane;
	Vec3                  m_targetBiasDirection;
	float                 m_targetFocus;
	EAITargetZone         m_targetZone;
	float                 m_targetDistanceToSilhouette;
	float                 m_targetEscapeLastMiss;
	float                 m_targetSeenTime;
	float                 m_targetLostTime;
	float                 m_targetDazzlingTime;
	float                 m_burstEffectTime;
	int                   m_burstEffectState;

	size_t                m_lastMissShotsCount;
	size_t                m_lastHitShotsCount;
	size_t                m_lastTargetPart;
	float                 m_targetDamageHealthThr;

	bool                  m_lastAimObstructionResult;
	EPuppetUpdatePriority m_updatePriority;

	PostureManager        m_postureManager;

	CTimeValue            m_lastTimeUpdatedBestTarget;

	/// It's quite expensive to walk through all AI objects and find which ones to steer
	/// around so they get cached here and only set on full updates
	std::vector<const CAIObject*> m_steeringObjects;
	CTimeValue                    m_lastSteerTime;

	CAIRadialOccypancy            m_steeringOccupancy;
	float                         m_steeringOccupancyBias;
	bool                          m_steeringEnabled;
	float                         m_steeringAdjustTime;
	float                         m_fLastNavTest;

	CValueHistory<float>*         m_targetDamageHealthThrHistory;

	CWeakRef<CAIVehicle> GetAvoidedVehicle() { return m_refAvoidedVehicle; }
	inline int64         GetVehicleAvoidingTime()
	{
		return (GetAISystem()->GetFrameStartTime() - m_vehicleAvoidingTime).GetMilliSecondsAsInt64();
	}

	virtual void EnableFire(bool enable);
	virtual bool IsFireEnabled() const;

	inline void  EnableCoverFire(bool enable) { m_bCoverFireEnabled = enable; }
	inline bool  IsCoverFireEnabled() const   { return m_bCoverFireEnabled; }

	inline void  SetAvoidedVehicle(CWeakRef<CAIVehicle> refVehicle)
	{
		if (!refVehicle.IsValid())
		{
			CCCPOINT(CPuppet_SetAvoidedVehicle_Reset);
			refVehicle.Reset();
			m_vehicleAvoidingTime = 0.0f;
		}
		else
		{
			CCCPOINT(CPuppet_SetAvoidedVehicle_Set)
			m_refAvoidedVehicle = refVehicle;
			m_vehicleAvoidingTime = GetAISystem()->GetFrameStartTime();
		}
	}

	// Updates the strafing logic.
	void UpdateStrafing();

	// The fire target cache caches raycast results used for
	// testing if a
	class SAIFireTargetCache
	{
	public:
		// cppcheck-suppress uninitMemberVar
		SAIFireTargetCache() : m_size(0), m_head(0), m_queries(0), m_hits(0) {}

		float QueryCachedResult(const Vec3& pos, const Vec3& dir, float reqDist)
		{
			m_queries++;
			const float distThr = sqr(0.15f);
			const float angleThr = cosf(DEG2RAD(6.0f));
			int j = m_head;
			for (int i = 0; i < m_size; ++i)
			{
				j--;
				if (j < 0) j = CACHE_SIZE - 1;

				if ((m_cacheReqDist[j] - 0.5f) >= reqDist &&
				    Distance::Point_PointSq(pos, m_cachePos[j]) < distThr &&
				    m_cacheDir[j].Dot(dir) > angleThr)
				{
					m_hits++;
					return m_cacheDist[j];
				}
			}
			// Could not find.
			return -1.0f;
		}

		void Insert(const Vec3& pos, const Vec3& dir, float reqDist, float dist)
		{
			m_cachePos[m_head] = pos;
			m_cacheDir[m_head] = dir;
			m_cacheDist[m_head] = dist;
			m_cacheReqDist[m_head] = reqDist;
			if (m_size < CACHE_SIZE)
				m_size++;
			m_head++;
			if (m_head >= CACHE_SIZE)
				m_head = 0;
		}

		int  GetQueries() const { return m_queries; }
		int  GetHits() const    { return m_hits; }

		void Reset()
		{
			m_size = 0;
			m_head = 0;
			m_hits = 0;
			m_queries = 0;
		}

	private:
		static const int CACHE_SIZE = 8;
		Vec3             m_cachePos[CACHE_SIZE];
		Vec3             m_cacheDir[CACHE_SIZE];
		float            m_cacheReqDist[CACHE_SIZE];
		float            m_cacheDist[CACHE_SIZE];
		int              m_size, m_head;
		int              m_hits;
		int              m_queries;
	};

	virtual void SetAlarmed()
	{
		AgentPerceptionParameters& perceptionParameters = m_Parameters.m_PerceptionParams;
		m_alarmedTime = perceptionParameters.forgetfulnessTarget + perceptionParameters.forgetfulnessMemory;

		// reset perceptionScale to 1.0f when alerted
		AgentPerceptionParameters::SPerceptionScale& perceptionScale = perceptionParameters.perceptionScale;
		perceptionScale.visual = 1.0f;
		perceptionScale.audio = 1.0f;

		gEnv->pAISystem->DisableGlobalPerceptionScaling();
	}
	virtual bool  IsAlarmed() const               { return m_alarmedTime > 0.01f; }
	virtual float GetPerceptionAlarmLevel() const { return max(GetParameters().m_PerceptionParams.minAlarmLevel, m_alarmedLevel); }

	float m_alarmedTime;
	float m_alarmedLevel;
	bool  m_damagePartsUpdated;

	uint8 m_fireDisabled;

	// Perception descriptors
	DynArray<SSoundPerceptionDescriptor>         m_SoundPerceptionDescriptor;
	static SSoundPerceptionDescriptor            s_DefaultSoundPerceptionDescriptor[AISOUND_LAST];

	static std::vector<Vec3>                     s_projectedPoints;
	static std::vector<std::pair<float, size_t>> s_weights;
};

ILINE const CPuppet* CastToCPuppetSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCPuppet() : 0; }
ILINE CPuppet*       CastToCPuppetSafe(IAIObject* pAI)       { return pAI ? pAI->CastToCPuppet() : 0; }
float                SquaredDistance(const CPuppet& puppetA, const CPuppet& puppetB);

#endif
