// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryName.h
//  Version:     v1.00
//  Created:     Craig Tiller.
//  Compilers:   Visual Studio.NET
//  Description: Handles locomotion/physics integration for objects
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IANIMATEDCHARACTER_H__
#define __IANIMATEDCHARACTER_H__

#pragma once

#include "IGameObject.h"
#include "IAnimationGraph.h"
#include <CryAnimation/IFacialAnimation.h>
#include <CryNetwork/SerializeFwd.h>

#include <CryAnimation/CryCharAnimationParams.h>

//--------------------------------------------------------------------------------

#if defined(_RELEASE)
	#define DEBUG_VELOCITY() 0
#else
	#define DEBUG_VELOCITY() 1
#endif

//--------------------------------------------------------------------------------

class IActionController;

//--------------------------------------------------------------------------------

enum EAnimationGraphLayerID
{
	eAnimationGraphLayer_FullBody = 0,
	eAnimationGraphLayer_UpperBody,

	eAnimationGraphLayer_COUNT
};

//--------------------------------------------------------------------------------

enum ECharacterMoveType
{
	eCMT_None = 0,
	eCMT_Normal,
	eCMT_Fly,
	eCMT_Swim,
	eCMT_ZeroG,

	eCMT_Impulse,
	eCMT_JumpInstant,
	eCMT_JumpAccumulate

};

//--------------------------------------------------------------------------------

enum EWeaponRaisedPose
{
	eWeaponRaisedPose_None       = 0x00,
	eWeaponRaisedPose_Fists      = 0x10,
	eWeaponRaisedPose_Rifle      = 0x20,
	eWeaponRaisedPose_Pistol     = 0x30,
	eWeaponRaisedPose_Rocket     = 0x40,
	eWeaponRaisedPose_MG         = 0x50,

	eWeaponRaisedPose_DualLft    = 0x01,
	eWeaponRaisedPose_DualRgt    = 0x02,

	eWeaponRaisedPose_PistolLft  = eWeaponRaisedPose_Pistol | eWeaponRaisedPose_DualLft,
	eWeaponRaisedPose_PistolRgt  = eWeaponRaisedPose_Pistol | eWeaponRaisedPose_DualRgt,
	eWeaponRaisedPose_PistolBoth = eWeaponRaisedPose_Pistol | eWeaponRaisedPose_DualLft | eWeaponRaisedPose_DualRgt,
};

//--------------------------------------------------------------------------------

enum EAnimatedCharacterArms
{
	eACA_RightArm = 1,
	eACA_LeftArm,
	eACA_BothArms,
};

//--------------------------------------------------------------------------------

enum EGroundAlignment
{
	eGA_Enable                     = BIT(0),
	eGA_AllowWithNoCollision       = BIT(1),
	eGA_AllowWhenHasGroundCollider = BIT(2),
	eGA_PoseAlignerUseRootOffset   = BIT(3),

	eGA_Default                    = eGA_Enable,
};

enum EMCMSlot
{
	eMCMSlot_Game = 0,
	eMCMSlot_Animation,
	eMCMSlot_Cur,
	eMCMSlot_Prev,
	eMCMSlot_Debug,

	eMCMSlot_COUNT,
};

enum EMCMSlotStack
{
	eMCMSlotStack_Begin = eMCMSlot_Game,
	eMCMSlotStack_End   = eMCMSlot_Animation + 1
};

//--------------------------------------------------------------------------------

struct SCharacterMoveRequest
{
	SCharacterMoveRequest()
		: type(eCMT_None)
		, velocity(ZERO)
		, rotation(IDENTITY)
		, allowStrafe(false)
		, proceduralLeaning(0.0f)
		, jumping(false)
	{}

	ECharacterMoveType        type;

	Vec3                      velocity; // meters per second (world space)
	Quat                      rotation; // relative, but immediate (per frame, not per second)

	SPredictedCharacterStates prediction;

	bool                      allowStrafe;
	float                     proceduralLeaning;
	bool                      jumping;

	// TODO: pass left/right turn intent
};

//--------------------------------------------------------------------------------

struct SAnimationBlendingParams
{
	SAnimationBlendingParams()
	{
		m_yawAngle = 0;
		m_speed = 0;
		m_strafeParam = 0;
		m_StrafeVector = ZERO;
		m_turnAngle = 0;
		m_diffBodyMove = 0;
		m_fBlendedDesiredSpeed = 0;
		m_fUpDownParam = 0;
	}
	f32  m_yawAngle;
	f32  m_speed;
	f32  m_strafeParam;
	Vec3 m_StrafeVector;
	f32  m_turnAngle;
	f32  m_diffBodyMove;
	f32  m_fBlendedDesiredSpeed;
	f32  m_fUpDownParam;
};

//--------------------------------------------------------------------------------

struct SLandBobParams
{
	SLandBobParams(float _maxTime = -1.0f, float _maxBob = -1.0f, float _maxFallDist = -1.0f) : maxTime(_maxTime), maxBob(_maxBob), maxFallDist(_maxFallDist)
	{
	};

	void Invalidate()
	{
		maxTime = -1.0f;
		maxBob = -1.0f;
		maxFallDist = -1.0f;
	}

	inline bool IsValid() const
	{
		return (maxTime > 0.0f) && (maxFallDist > 0.0f);
	}

	float maxTime;
	float maxBob;
	float maxFallDist;
};

//--------------------------------------------------------------------------------

struct IAnimationBlending
{
	virtual ~IAnimationBlending(){}
	//
	virtual SAnimationBlendingParams* Update(IEntity* pIEntity, Vec3 DesiredBodyDirection, Vec3 DesiredMoveDirection, f32 fDesiredMovingSpeed) = 0;
	virtual void                      GetMemoryStatistics(ICrySizer* s) = 0;
};

//--------------------------------------------------------------------------------

struct SAnimatedCharacterParams
{
	SAnimatedCharacterParams()
	{
		Reset();
	}

	void Reset()
	{
		inertia = 0.0f;
		inertiaAccel = 0.0f;
		timeImpulseRecover = 0.0f;
		pAnimationBlending = 0;
	}

	void SetInertia(float i, float ia)
	{
		inertia = i;
		inertiaAccel = ia;
	}

	float               inertia;
	float               inertiaAccel;
	float               timeImpulseRecover;

	IAnimationBlending* pAnimationBlending;
};

//--------------------------------------------------------------------------------

struct SRagdollizeParams
{
	SRagdollizeParams(float _mass = 0.0f, float _stiffness = 0.0f, bool _sleep = false)
		: mass(_mass)
		, stiffness(_stiffness)
		, sleep(_sleep)
	{}

	float mass;
	float stiffness;
	bool  sleep;
};

//--------------------------------------------------------------------------------

struct SBlendFromRagdollParams
{
	SBlendFromRagdollParams()
		: m_bPendingBlend(false)
		, m_animID(0)
	{}
	int  m_animID;
	bool m_bPendingBlend;
};
//--------------------------------------------------------------------------------

struct SGroundAlignmentParams
{
	SGroundAlignmentParams()
	{
		InitVars();
	}

	void InitVars()
	{
		ikDisableDistanceSqr = sqr(35.f);
		flags = eGA_Default;
	}

	ILINE void SetFlag(const EGroundAlignment flag, const bool set) { set ? flags |= ((uint8)flag) : flags &= ~((uint8)flag); }
	ILINE bool IsFlag(const EGroundAlignment flag) const            { return (flags & flag) != 0; }

	float      ikDisableDistanceSqr;
	uint8      flags;
};

//--------------------------------------------------------------------------------

struct IAnimatedCharacter : public IGameObjectExtension
{
	virtual IAnimationGraphState*    GetAnimationGraphState() = 0;
	virtual void                     PushForcedState(const char* state) = 0;
	virtual void                     ClearForcedStates() = 0;
	virtual void                     ChangeGraph(const char* graph, int layer) = 0;
	virtual void                     ResetState() = 0;
	virtual void                     ResetInertiaCache() = 0;

	virtual IActionController*       GetActionController() = 0;
	virtual const IActionController* GetActionController() const = 0;

	virtual void                     SetShadowCharacterSlot(int id) = 0;

	virtual void                     SetAnimationPlayerProxy(CAnimationPlayerProxy* proxy, int layer) = 0;
	virtual CAnimationPlayerProxy*   GetAnimationPlayerProxy(int layer) = 0;

	virtual void                     UpdateCharacterPtrs() = 0;

	// movement related - apply some physical impulse/movement request to an object
	// and make sure it syncs correctly with animation
	virtual void                             AddMovement(const SCharacterMoveRequest&) = 0;
	virtual void                             SetEntityRotation(const Quat& rot) = 0;
	virtual const SPredictedCharacterStates& GetOverriddenMotionParameters() const = 0;
	virtual void                             SetOverriddenMotionParameters(const SPredictedCharacterStates& motionParameters) = 0;

	enum EBlendWeightParamTargets
	{
		eBWPT_None                = 0,
		eBWPT_FirstPersonSkeleton = BIT(0),
		eBWPT_ShadowSkeleton      = BIT(1),
		eBWPT_All                 = 3
	};
	virtual void SetBlendWeightParam(const EMotionParamID motionParamID, const float value, const uint8 targetFlags = eBWPT_All) = 0;

	//! Override for the entity's desired movement -> replace it with animation movement components
	virtual void                            UseAnimationMovementForEntity(bool xyMove, bool zMove, bool rotation) = 0;

	virtual const QuatT&                    GetAnimLocation() const = 0;
	virtual const float                     GetEntitySpeedHorizontal() const = 0;
	virtual const float                     GetEntitySpeed() const = 0;
	virtual const Vec2&                     GetEntityMovementDirHorizontal() const = 0;
	virtual const Vec2&                     GetEntityVelocityHorizontal() const = 0;
	virtual const Vec2&                     GetEntityAccelerationHorizontal() const = 0;
	virtual const float                     GetEntityTangentialAcceleration() const = 0;
	virtual const Vec3&                     GetExpectedEntMovement() const = 0;
	virtual float                           GetAngularSpeedHorizontal() const = 0;

	virtual const SAnimatedCharacterParams& GetParams() = 0;
	virtual void                            SetParams(const SAnimatedCharacterParams& params) = 0;

	virtual SGroundAlignmentParams& GetGroundAlignmentParams() = 0;

	virtual void                    SetDoMotionParams(bool doMotionParams) = 0;

	// stance helper functions
	virtual int  GetCurrentStance() = 0;
	virtual bool InStanceTransition() = 0;
	virtual void RequestStance(int stanceID, const char* name) = 0;

	//
	virtual float                  FilterView(SViewParams& viewParams) const = 0;

	virtual EColliderMode          GetPhysicalColliderMode() const = 0;
	virtual void                   ForceRefreshPhysicalColliderMode() = 0;
	virtual void                   RequestPhysicalColliderMode(EColliderMode mode, EColliderModeLayer layer, const char* tag = NULL) = 0;
	virtual void                   SetCharacterCollisionFlags(unsigned int flags) = 0;
	virtual void                   SetMovementControlMethods(EMovementControlMethod horizontal, EMovementControlMethod vertical) = 0; // Deprecated, please use the version of this function that requires the slot parameter.
	virtual void                   SetMovementControlMethods(EMCMSlot slot, EMovementControlMethod horizontal, EMovementControlMethod vertical, const char* tag = NULL) = 0;

	virtual void                   EnableRigidCollider(float radius) = 0;
	virtual void                   DisableRigidCollider() = 0;

	virtual EMovementControlMethod GetMCMH() const = 0;
	virtual EMovementControlMethod GetMCMV() const = 0;

	// Procedural landing bounce effect that works in conjunction with the leg IK
	virtual void EnableLandBob(const SLandBobParams& landBobParams) = 0;
	virtual void DisableLandBob() = 0;

	//virtual void MakePushable(bool enable) = 0;

	virtual void SetFacialAlertnessLevel(int alertness) = 0;
	virtual int  GetFacialAlertnessLevel() = 0;

	// look IK is allowed by default. use this to disallow it
	virtual void AllowLookIk(bool allow, int layer = -1) = 0;
	virtual bool IsLookIkAllowed() const = 0;

	// aim IK is allowed by default. use this to disallow it
	virtual void AllowAimIk(bool allow) = 0;
	virtual bool IsAimIkAllowed() const = 0;

	virtual void TriggerRecoil(float duration, float kinematicImpact, float kickIn = 0.8f, EAnimatedCharacterArms arms = eACA_BothArms) = 0;
	virtual void SetWeaponRaisedPose(EWeaponRaisedPose pose) = 0; // deprecated

	virtual void SetNoMovementOverride(bool external) = 0;

	// Returns the angle (in degrees) of the ground below the character.
	// Zero is flat ground (along facing direction), positive when character facing uphill, negative when character facing downhill.
	virtual float GetSlopeDegreeMoveDir() const = 0;
	virtual float GetSlopeDegree() const = 0;

	// Set Animated Character in a special state (grabbed)
	// This state requires special handling regarding animation processing order
	virtual void SetInGrabbedState(bool bEnable) = 0;

	// Add movement to the animated character - used to force movement when the character is in animation controlled movement
	// Multiple calls will accumulate the forced movement until it is applied
	virtual void ForceMovement(const QuatT& relativeMovement) = 0;

	// Override rotation the animated character will apply - used to force rotation when the character is in animation controlled movement
	virtual void ForceOverrideRotation(const Quat& qWorldRotation) = 0;

#if DEBUG_VELOCITY()
	virtual void AddDebugVelocity(const QuatT& movement, const float frameTime, const char* szComment, const ColorF& colorF, const bool pastMovement = false) const = 0;
	virtual bool DebugVelocitiesEnabled() const = 0;
#endif
};

//--------------------------------------------------------------------------------

namespace animatedcharacter
{
void Preload(struct IScriptTable* pEntityScript);
}

struct IAnimationPoseModifierTorsoAim : public IAnimationPoseModifier
{
	CRYINTERFACE_DECLARE_GUID(IAnimationPoseModifierTorsoAim, "388374ea-ddf8-49ba-b8b7-5dfd824b2c3a"_cry_guid);
};

#endif
