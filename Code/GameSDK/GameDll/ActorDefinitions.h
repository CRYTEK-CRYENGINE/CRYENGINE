// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Common Actor helper definitions
  
 -------------------------------------------------------------------------
  History:
  - 01:07:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __ACTORDEFINITIONS_H__
#define __ACTORDEFINITIONS_H__

#include <CryGame/CoherentValue.h>
#include "IAnimationGraph.h"
#include "IAnimatedCharacter.h"
#include "IMovementController.h"
#include <CryCore/CryFlags.h>

struct IVehicle;
struct IInventory;
struct IInteractor;

//FIXME:not sure to put this here
static const int ZEROG_AREA_ID = PHYS_FOREIGN_ID_USER+1;

//represent the key status of the actor
#define ACTION_JUMP						(1<<0)
#define ACTION_CROUCH					(1<<1)
#define ACTION_SPRINT					(1<<2)
#define ACTION_RELAXED					(1<<3)
#define ACTION_STEALTH					(1<<4)
#define ACTION_MOVE						(1<<5)
#define ACTION_USE						(1<<6)
#define ACTION_FIRE						(1<<7)
#define ACTION_PRONE					(1<<8)
#define ACTION_FORCE_CROUCH		(1<<9) //Users without crouch toggle on can have this flag set to force crouching even when the ACTION_CROUCH flag is not set

#if !defined(_RELEASE)
#define WATCH_ACTOR_STATE(format, ...) \
	do {                                 \
		g_pGameCVars && g_pGameCVars->g_displayDbgText_actorState && CryWatch("[ACTOR STATE] <%s> %s '%s' " format, IsClient() ? "Client" : "Non-client", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), __VA_ARGS__); \
	} while(0)
#define WATCH_ACTOR_STATE_FOR(pIActor, format, ...) \
	do {                                 \
		g_pGameCVars && g_pGameCVars->g_displayDbgText_actorState && CryWatch("[ACTOR STATE] <%s> %s '%s' " format, pIActor->IsClient() ? "Client" : "Non-client", pIActor->GetEntity()->GetClass()->GetName(), pIActor->GetEntity()->GetName(), __VA_ARGS__); \
	} while(0)
#else
#define WATCH_ACTOR_STATE(...) (void)(0)
#define WATCH_ACTOR_STATE_FOR(...) (void)(0)
#endif

#define ReasonForReviveList(f)    \
	f(kRFR_FromInit)                \
	f(kRFR_StartSpectating)         \
	f(kRFR_Spawn)                   \
	f(kRFR_ScriptBind)              \

#define ActorBoneList(f) \
	f(BONE_BIP01)    \
	f(BONE_SPINE)    \
	f(BONE_SPINE2)   \
	f(BONE_SPINE3)   \
	f(BONE_HEAD)     \
	f(BONE_EYE_R)    \
	f(BONE_EYE_L)    \
	f(BONE_WEAPON)   \
	f(BONE_WEAPON2)  \
	f(BONE_FOOT_R)   \
	f(BONE_FOOT_L)   \
	f(BONE_ARM_R)    \
	f(BONE_ARM_L)    \
	f(BONE_CALF_R)   \
	f(BONE_CALF_L)   \
	f(BONE_CAMERA)   \
	f(BONE_HUD)   

AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(EBonesID, ActorBoneList, BONE_ID_NUM);
extern const char *s_BONE_ID_NAME[BONE_ID_NUM];

struct SActorFrameMovementParams
{
	SActorFrameMovementParams() : 
		desiredVelocity(ZERO), 
		desiredLean(0.0f),
		desiredPeekOver(0.0f),
		deltaAngles(ZERO),
		lookTarget(ZERO),
		aimTarget(ZERO),
		lookIK(false), 
		aimIK(false), 
		jump(false),
		sprint(0.0f),
		stance(STANCE_NULL),
		mannequinClearTags(TAG_STATE_EMPTY),
		mannequinSetTags(TAG_STATE_EMPTY),
		allowStrafe(false)
	{
	}

	// desired velocity for this frame (meters/second) (in local space)
	Vec3 desiredVelocity;
	// desired lean
	float desiredLean;
	float desiredPeekOver;
	// desired turn
	Ang3 deltaAngles;
	Vec3 lookTarget;
	Vec3 aimTarget;
	// prediction
	SPredictedCharacterStates prediction;
	// look target
	bool lookIK;
	// aim target
	bool aimIK;
	bool jump;
	float sprint;
	// stance
	EStance stance;
	// mannequin tag state
	TagState mannequinClearTags;
	TagState mannequinSetTags;
	// cover request
	SAICoverRequest coverRequest;

	bool allowStrafe;
};

struct IActorMovementController : public IMovementController
{
	virtual void Reset() = 0;
	virtual void Update( float frameTime, SActorFrameMovementParams& params ) = 0;
	virtual void PostUpdate( float frameTime ) = 0;
	virtual void Release() = 0;
	virtual void Serialize(TSerialize &ser) = 0;
	virtual void ApplyControllerAcceleration( float& rotX, float& rotZ, float dt ) {}

	virtual void GetMemoryUsage(ICrySizer *pSizer ) const  = 0;
};

struct SViewLimitParams
{

	SViewLimitParams() { ClearViewLimit(eVLS_None, true); }
	
	enum EViewLimitState
	{
		eVLS_None,
		eVLS_Ladder,
		eVLS_Slide,
		eVLS_Item,
		eVLS_Ledge,
		eVLS_Stage
	};
	
	void SetViewLimit(Vec3 direction, float rangeH, float rangeV, float rangeUp, float rangeDown, EViewLimitState limitType)
	{
		vLimitDir = direction;
		vLimitRangeH = rangeH;
		vLimitRangeV = rangeV;
		vLimitRangeVUp = rangeUp;
		vLimitRangeVDown = rangeDown;
		vLimitType = limitType;
	}

	void ClearViewLimit(EViewLimitState limitType, bool force = false)
	{
		if(limitType == vLimitType || force)
		{
			vLimitDir.zero();
			vLimitRangeH = 0.f;
			vLimitRangeV = 0.f;
			vLimitRangeVUp = 0.f;
			vLimitRangeVDown = 0.f;
			vLimitType = eVLS_None;
		}
	}

	ILINE Vec3 GetViewLimitDir() const					{ return vLimitDir; }
	ILINE float GetViewLimitRangeH() const			{ return vLimitRangeH; }
	ILINE float GetViewLimitRangeV() const			{ return vLimitRangeV; }
	ILINE float GetViewLimitRangeVUp() const		{ return vLimitRangeVUp; }
	ILINE float GetViewLimitRangeVDown() const	{ return vLimitRangeVDown; }
	ILINE EViewLimitState GetViewLimitState() const	{ return vLimitType; }

private:
	Vec3 vLimitDir;
	float vLimitRangeH;
	float vLimitRangeV;
	float vLimitRangeVUp;
	float vLimitRangeVDown;
	EViewLimitState vLimitType;
};


struct SAITurnParams
{
	SAITurnParams()
		: minimumAngle(DEG2RAD(5.0f))
		, minimumAngleForTurnWithoutDelay(DEG2RAD(30.0f))
		, maximumDelay(1.0f)
	{}

	float minimumAngle;                    // (radians; >= 0) Angle deviation that is needed before turning is even considered
	float minimumAngleForTurnWithoutDelay; // (radians; >= minimumAngle) Angle deviation that is needed before turning happens immediately, without delay.
	float maximumDelay;                    // (seconds; >= 0) Maximum delay before we turn. This delay is used when the deviation is the minimumAngle and is scaled down to zero when the deviation is minimumAngleForTurnWithoutDelay.
};


struct SActorParams
{
	enum ESpeedMultiplierReason
	{
		eSMR_Internal,
		eSMR_GameRules,
		eSMR_Item,
		eSMR_SuitDisruptor,

		eSMR_COUNT,
	};

	SViewLimitParams viewLimits;

	Vec3 mountedWeaponCameraTarget;

	float viewFoVScale;

	float internalSpeedMult;
	float speedMultiplier[eSMR_COUNT];
	float meeleHitRagdollImpulseScale;

	float lookFOVRadians;
	float lookInVehicleFOVRadians;
	float aimFOVRadians;
	float maxLookAimAngleRadians;
	bool allowLookAimStrafing; // whether or not the character should turn his body a bit towards the look/aim target when moving
	int cornerSmoother; // default:1; 0 = none, 1 = C2 method: SmoothCD of movement angle, 2 = C3 method: spline prediction

	float fallNPlayStiffness_scale;

	float sprintMultiplier;
	float crouchMultiplier;
	float strafeMultiplier;
	float backwardMultiplier;

	float jumpHeight;

	float leanShift;
	float leanAngle;

	float aimIKFadeDuration;

	float proceduralLeaningFactor;

	char animationAppendix[32];
	string footstepEffectName;
	string remoteFootstepEffectName;
	string foleyEffectName;

	string footstepIndGearAudioSignal_Walk;
	string footstepIndGearAudioSignal_Run;
	bool footstepGearEffect;

	bool canUseComplexLookIK;
	string lookAtSimpleHeadBoneName;
	uint32 aimIKLayer; // [0..15] default: 1
	uint32 lookIKLayer; // [0..15] default: 15

	struct SDynamicAimPose
	{
		string leftArmAimPose;
		string rightArmAimPose;
		string bothArmsAimPose;
	};
	SDynamicAimPose idleDynamicAimPose;
	SDynamicAimPose runDynamicAimPose;
	float bothArmsAimHalfFOV;			// half the FOV in radians for using both arms, in radians, at pitch 0
	float bothArmsAimPitchFactor;	// increase of the half-FOV (in radians) per radian of pitch
	bool useDynamicAimPoses;

	SAITurnParams AITurnParams;

	float stepThresholdDistance;	// threshold distance, that is needed before stepping is even considered
	float stepThresholdTime;			// the current distance deviation needs to be over the stepThresholdDistance for longer than this time before the character steps

	float maxDeltaAngleRateNormal;
	float maxDeltaAngleRateAnimTarget;
	float maxDeltaAngleMultiplayer;
	float maxDeltaAngleRateJukeTurn;
	EStance defaultStance;

	bool smoothedZTurning;


	SActorParams()
		: mountedWeaponCameraTarget(ZERO)
		, viewFoVScale(1.0f)
		, lookFOVRadians(gf_PI)
		, lookInVehicleFOVRadians(gf_PI)
		, aimFOVRadians(gf_PI)
		, maxLookAimAngleRadians(gf_PI)
		, allowLookAimStrafing(true)
		, internalSpeedMult(1.0f)
		, meeleHitRagdollImpulseScale(1.0f)
		, fallNPlayStiffness_scale(1200)
		, sprintMultiplier(1.25f)
		, crouchMultiplier(1.0f)
		, strafeMultiplier(0.9f)
		, backwardMultiplier(0.9f)
		, jumpHeight(1.0f)
		, leanShift(0.5f)
		, leanAngle(11.0f)
		, aimIKFadeDuration(0.6f)
		, footstepGearEffect(false)
		, canUseComplexLookIK(true)
		, aimIKLayer(1)
		, lookIKLayer(15)
		, cornerSmoother(1) // C2 method
		, proceduralLeaningFactor(0.8f) // Crysis1 value
		, bothArmsAimHalfFOV(DEG2RAD(10.0f))
		, bothArmsAimPitchFactor(1.0f)
		, useDynamicAimPoses(false)
		, stepThresholdDistance(0.15f) // Crysis1 value
		, stepThresholdTime(1.0f) // Crysis1 value
		, maxDeltaAngleRateNormal(DEG2RAD(180.f))
		, maxDeltaAngleRateAnimTarget(DEG2RAD(360.f))
		, maxDeltaAngleMultiplayer(DEG2RAD(3600.f))
		, maxDeltaAngleRateJukeTurn(DEG2RAD(720.f))
		, defaultStance(STANCE_STAND)
		, smoothedZTurning(false)
	{
		for (int smr = 0; smr < eSMR_COUNT; ++smr)
			speedMultiplier[smr] = 1.0f;

		memset(animationAppendix, 0, sizeof(animationAppendix));
		cry_strcpy(animationAppendix,"nw");
	}
};

struct SActorPhysics
{
	SActorPhysics()
		: angVelocity(ZERO)
		, velocity(ZERO)
		, velocityDelta(ZERO)
		, velocityUnconstrained(ZERO)
		, velocityUnconstrainedLast(ZERO)
		, gravity(ZERO)
		, groundNormal(0,0,1)
		, speed(0)
		, groundHeight(0.0f)
		, mass(80.0f)
		, lastFrameUpdate(0)
		, groundMaterialIdx(-1)
		, groundColliderId(0)
	{}

	enum EActorPhysicsFlags
	{
		EActorPhysicsFlags_Flying			= BIT(0),
		EActorPhysicsFlags_WasFlying	= BIT(1),
		EActorPhysicsFlags_Stuck			= BIT(2)
	};

	void Serialize(TSerialize ser, EEntityAspects aspects);

	Vec3 angVelocity;
	Vec3 velocity;
	Vec3 velocityDelta;
	Vec3 velocityUnconstrained;
	Vec3 velocityUnconstrainedLast;
	Vec3 gravity;
	Vec3 groundNormal;

	float	speed;
	float groundHeight;
	float mass;

	int lastFrameUpdate;
	int groundMaterialIdx;
	EntityId groundColliderId;

	CCryFlags<uint32> flags;
};

typedef TBitfield TActorStatusFlags;

#define ActorStatusFlagList(func)                \
	func(kActorStatus_onLadder)                    \
	func(kActorStatus_stealthKilling)              \
	func(kActorStatus_stealthKilled)               \
	func(kActorStatus_attemptingStealthKill)       \
	func(kActorStatus_pickAndThrow)                \
	func(kActorStatus_vaultLedge)                  \
	func(kActorStatus_swimming)                    \
	func(kActorStatus_linkedToVehicle)             \

AUTOENUM_BUILDFLAGS_WITHZERO(ActorStatusFlagList, kActorStatus_none);

struct SActorStats
{
	struct SItemExchangeStats
	{
		EntityId switchingToItemID;
		bool keepHistory;
		bool switchThisFrame; // used only when the character is using an action controller

		SItemExchangeStats() : switchingToItemID(0), keepHistory(false), switchThisFrame(false) {}
	};

	CCoherentValue<float> inAir;		//double purpose, tells if the actor is in air and for how long
	CCoherentValue<float> onGround;	//double purpose, tells if the actor is on ground and for how long
	CCoherentValue<float> speedFlat;

	int movementDir;
	float inFiring;
	float maxAirSpeed; //The maximum speed the actor has reached over the duration of their current jump/fall. Set to 0 when on the ground.

	bool isGrabbed;
	bool isRagDoll;
	bool isInBlendRagdoll; // To signal when the actor is in the FallNPlay Animation Graph state
	bool wasHit;		// to signal if the player is in air due to an impulse

	bool bStealthKilling;
	bool bStealthKilled;
	bool bAttemptingStealthKill;

	EntityId spectacularKillPartner;

	CCoherentValue<bool> isHidden;

	EntityId mountedWeaponID;

	SItemExchangeStats exchangeItemStats;

	float feetHeight[2];

	void EnableStatusFlags(TActorStatusFlags reason) { m_actorStatusFlags |= reason; }
	void DisableStatusFlags(TActorStatusFlags reason) { m_actorStatusFlags &= ~reason; }
	bool HasAnyStatusFlags(TActorStatusFlags reasons = ~0) const { return (m_actorStatusFlags & reasons) != kActorStatus_none; }
	bool HasAllStatusFlags(TActorStatusFlags reasons) const { return (m_actorStatusFlags & reasons) == reasons; }
	TActorStatusFlags GetStatusFlags() const { return m_actorStatusFlags; }

	SActorStats()
	{
		memset( this,0,sizeof(SActorStats) );
	}

private:
	TActorStatusFlags m_actorStatusFlags;
};

struct SStanceInfo
{
	//dimensions
	float heightCollider;
	float heightPivot;
	float groundContactEps;

	bool useCapsule;

	Vec3 size;

	//view
	Vec3 viewOffset;
	Vec3 leanLeftViewOffset;
	Vec3 leanRightViewOffset;
	Vec3 whileLeanedLeftViewOffset;
	Vec3 whileLeanedRightViewOffset;
	float viewDownYMod;

	Vec3 peekOverViewOffset;
	Vec3 peekOverWeaponOffset;

	//weapon
	Vec3 weaponOffset;
	Vec3 leanLeftWeaponOffset;
	Vec3 leanRightWeaponOffset;
	Vec3 whileLeanedLeftWeaponOffset;
	Vec3 whileLeanedRightWeaponOffset;

	//movement
	float normalSpeed;
	float maxSpeed;

	//misc
	char name[32];

	Vec3 modelOffset;

	inline Vec3	GetViewOffsetWithLean(float lean, float peekOver, bool useWhileLeanedOffsets = false) const
	{
		float peek = clamp_tpl(peekOver, 0.0f, 1.0f);
		Vec3 peekOffset = peek * (peekOverViewOffset - viewOffset);

		if (lean < -0.01f)
		{
			float a = clamp_tpl(-lean, 0.0f, 1.0f);
			return viewOffset + a * ((useWhileLeanedOffsets ? whileLeanedLeftViewOffset : leanLeftViewOffset) - viewOffset) + peekOffset;
		}
		else if (lean > 0.01f)
		{
			float a = clamp_tpl(lean, 0.0f, 1.0f);
			return viewOffset + a * ((useWhileLeanedOffsets ? whileLeanedRightViewOffset : leanRightViewOffset) - viewOffset) + peekOffset;
		}
		return viewOffset + peekOffset;
	}

	inline Vec3	GetWeaponOffsetWithLean(float lean, float peekOver, bool useWhileLeanedOffsets = false) const
	{
		float peek = clamp_tpl(peekOver, 0.0f, 1.0f);
		Vec3 peekOffset = peek * (peekOverWeaponOffset - weaponOffset);

		if (lean < -0.01f)
		{
			float a = clamp_tpl(-lean, 0.0f, 1.0f);
			return weaponOffset + a * ((useWhileLeanedOffsets ? whileLeanedLeftWeaponOffset : leanLeftWeaponOffset) - weaponOffset) + peekOffset;
		}
		else if (lean > 0.01f)
		{
			float a = clamp_tpl(lean, 0.0f, 1.0f);
			return weaponOffset + a * ((useWhileLeanedOffsets ? whileLeanedRightWeaponOffset : leanRightWeaponOffset) - weaponOffset) + peekOffset;
		}
		return weaponOffset + peekOffset;
	}

	static inline Vec3 GetOffsetWithLean(float lean, float peekOver, const Vec3& sOffset, const Vec3& sLeftLean, const Vec3& sRightLean, const Vec3& sPeekOffset)
	{
		float peek = clamp_tpl(peekOver, 0.0f, 1.0f);
		Vec3 peekOffset = peek * (sPeekOffset - sOffset);

		if (lean < -0.01f)
		{
			float a = clamp_tpl(-lean, 0.0f, 1.0f);
			return sOffset + a * (sLeftLean - sOffset) + peekOffset;
		}
		else if (lean > 0.01f)
		{
			float a = clamp_tpl(lean, 0.0f, 1.0f);
			return sOffset + a * (sRightLean - sOffset) + peekOffset;
		}
		return sOffset + peekOffset;
	}


	// Returns the size of the stance including the collider and the ground location.
	ILINE AABB	GetStanceBounds() const
	{
		AABB	aabb(-size, size);
		// Compensate for capsules.
		if(useCapsule)
		{
			aabb.min.z -= max(size.x, size.y);
			aabb.max.z += max(size.x, size.y);
		}
		// Lift from ground.
		aabb.min.z += heightCollider;
		aabb.max.z += heightCollider;
		// Include ground position
		aabb.Add(Vec3(0,0,0));

		// Make relative to the entity
		aabb.min.z -= heightPivot;
		aabb.max.z -= heightPivot;
		return aabb;
	}

	// Returns the size of the collider of the stance.
	inline AABB	GetColliderBounds() const
	{
		AABB	aabb(-size, size);
		// Compensate for capsules.
		if(useCapsule)
		{
			aabb.min.z -= max(size.x, size.y);
			aabb.max.z += max(size.x, size.y);
		}
		// Lift from ground.
		aabb.min.z += heightCollider;
		aabb.max.z += heightCollider;
		// Make relative to the entity
		aabb.min.z -= heightPivot;
		aabb.max.z -= heightPivot;
		return aabb;
	}

	SStanceInfo()
		: heightCollider(0.0f)
		, heightPivot(0.0f)
		, groundContactEps(0.025f)
		, useCapsule(false)
		, size(0.5f,0.5f,0.5f)
		, viewOffset(ZERO)
		, leanLeftViewOffset(ZERO)
		, leanRightViewOffset(ZERO)
		, whileLeanedLeftViewOffset(ZERO)
		, whileLeanedRightViewOffset(ZERO)
		, viewDownYMod(0.0f)
		, peekOverViewOffset(ZERO)
		, peekOverWeaponOffset(ZERO)
		, weaponOffset(ZERO)
		, leanLeftWeaponOffset(ZERO)
		, leanRightWeaponOffset(ZERO)
		, whileLeanedLeftWeaponOffset(ZERO)
		, whileLeanedRightWeaponOffset(ZERO)
		, normalSpeed(0.0f)
		, maxSpeed(0.0f)
		, modelOffset(ZERO)
	{
		cry_strcpy(name, "null");
	}
};

#define IKLIMB_RIGHTHAND (1<<0)
#define IKLIMB_LEFTHAND (1<<1)

struct SActorIKLimbInfo
{
	string sLimbName;
	string sRootBone;
	string sMidBone;
	string sEndBone;
	int characterSlot;
	int flags;

	SActorIKLimbInfo() : characterSlot(-1), flags(0) {}
	SActorIKLimbInfo(int slot, const char *limbName, const char *rootBone, const char *midBone, const char *endBone, int _flags) 
		: sLimbName(limbName), sRootBone(rootBone), sMidBone(midBone), sEndBone(endBone), characterSlot(slot), flags(_flags) {}
};

struct SIKLimb
{
	int flags;

	int rootBoneID;
	int endBoneID;
	int middleBoneID;//optional for custom IK

	Vec3 goalWPos;
	Vec3 currentWPos;

	Vec3 goalNormal;

	//limb local position in the animation
	Vec3 lAnimPos;

	char name[64];

	float recoverTime;//the time to go back to the animation pose
	float recoverTimeMax;

	float blendTime;
	float invBlendTimeMax;

	int blendID;

	int characterSlot;

	bool keepGoalPos;

	SIKLimb()
		: flags(0)
		, rootBoneID(0)
		, endBoneID(0)
		, middleBoneID(0)
		, goalWPos(ZERO)
		, currentWPos(ZERO)
		, goalNormal(ZERO)
		, lAnimPos(ZERO)
		, recoverTime(0.0f)
		, recoverTimeMax(0.0f)
		, blendTime(0.0f)
		, invBlendTimeMax(0.0f)
		, blendID(0)
		, characterSlot(0)
		, keepGoalPos(false)
	{
		cry_strcpy(name, "");
	}

	void SetLimb(int slot,const char *limbName,int rootID,int midID,int endID,int iFlags);
	void SetWPos(IEntity *pOwner,const Vec3 &pos,const Vec3 &normal,float blend,float recover,int requestID);
	void Update(IEntity *pOwner,float frameTime);

	void GetMemoryUsage(ICrySizer *pSizer) const{}
};

struct SLinkStats
{
	enum EFlags
	{
		LINKED_FREELOOK = (1<<0),
		LINKED_VEHICLE = (1<<1),
	};

	//which entity are we linked to?
	EntityId linkID;
	//
	uint32 flags;

	//
	SLinkStats() : linkID(0), flags(0)
	{
	}

	SLinkStats( EntityId eID, uint32 flgs = 0) : linkID(eID), flags(flgs)
	{
	}

	ILINE void UnLink()
	{
		linkID = 0;
		flags = 0;
	}

	ILINE bool CanMove()
	{
		return (!linkID);
	}

	ILINE bool CanDoIK()
	{
		return (!linkID);
	}

	ILINE bool CanMoveCharacter()
	{
		return (!linkID);
	}

	ILINE IEntity *GetLinked()
	{
		if (!linkID)
			return NULL;
		else
		{
			IEntity *pEnt = gEnv->pEntitySystem->GetEntity(linkID);
			//if the entity doesnt exist anymore forget about it.
			if (!pEnt)
				UnLink();

			return pEnt;
		}
	}

	IVehicle *GetLinkedVehicle();

	void Serialize( TSerialize ser );
};

struct SActorGameParams
{
	SActorParams actorParams;
	SStanceInfo stances[STANCE_LAST];
	string boneNames[BONE_ID_NUM];
	std::vector<string>	animationDBAs;
};

struct SActorFileModelInfo
{
	string sFileName;
	string sClientFileName;
	string sShadowFileName;
	int nModelVariations;
	bool bUseFacialFrameRateLimiting;
	
	typedef std::vector<SActorIKLimbInfo> TIKLimbInfoVec;
	TIKLimbInfoVec IKLimbInfo;

	SActorFileModelInfo() : nModelVariations(0), bUseFacialFrameRateLimiting(true) {}
};

struct SActorAnimationEvents
{
	SActorAnimationEvents()
		: m_initialized(false)
	{

	}

	void Init();

	uint32 m_soundId;
	uint32 m_plugginTriggerId;
	uint32 m_footstepSignalId;
	uint32 m_foleySignalId;
	uint32 m_groundEffectId;
	uint32 m_swimmingStrokeId;
	uint32 m_footStepImpulseId;
	uint32 m_forceFeedbackId;
	uint32 m_grabObjectId;
	uint32 m_stowId;
	uint32 m_weaponLeftHandId;
	uint32 m_weaponRightHandId;
	uint32 m_deathReactionEndId;
	uint32 m_reactionOnCollision;
	uint32 m_forbidReactionsId;
	uint32 m_ragdollStartId;
	uint32 m_killId;
	uint32 m_deathBlow;
	uint32 m_startFire;
	uint32 m_stopFire;
	uint32 m_shootGrenade;
	uint32 m_meleeHitId;
	uint32 m_meleeStartDamagePhase;
	uint32 m_meleeEndDamagePhase;
	uint32 m_endReboundAnim;
	uint32 m_detachEnvironmentalWeapon;
	uint32 m_stealthMeleeDeath;

private:
	bool   m_initialized;
};

#endif //__ACTORDEFINITIONS_H__
