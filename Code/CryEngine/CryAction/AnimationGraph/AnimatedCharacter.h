// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ANIMATEDCHARACTER_H__
#define __ANIMATEDCHARACTER_H__

#pragma once

#include "IAnimatedCharacter.h"
#include "IAnimationGraph.h"
#include "AnimationGraphCVars.h" // this includes animated character cvars too
#include <queue>
#include <CryCore/BitFiddling.h>
#include <CryAction/IDebugHistory.h>
#include "CryActionPhysicQueues.h"
#include "ICryMannequin.h"
#include "Mannequin/MannequinAGState.h"
#include "PersistantDebug.h"
#include "IMovementController.h" // for SExactPositioningTarget
#include "AnimatedCharacterEventProxies.h"
#include <CryCore/Containers/CryListenerSet.h>

//--------------------------------------------------------------------------------

#define ANIMCHAR_PROFILE_HEAVY

//--------------------------------------------------------------------------------

#define ANIMCHAR_PROFILE CRY_PROFILE_FUNCTION(PROFILE_ACTION)

#ifdef ANIMCHAR_PROFILE_HEAVY
	#define ANIMCHAR_PROFILE_DETAILED CRY_PROFILE_FUNCTION(PROFILE_ACTION)
#else
	#define ANIMCHAR_PROFILE_DETAILED {}
#endif

#ifdef ANIMCHAR_PROFILE_HEAVY
	#define ANIMCHAR_PROFILE_SCOPE(label) CRY_PROFILE_REGION(PROFILE_ACTION, label)
#else
	#define ANIMCHAR_PROFILE_SCOPE(label) {}
#endif

//--------------------------------------------------------------------------------

#if defined(_DEBUG)
	#define DEBUGHISTORY
#endif

//--------------------------------------------------------------------------------

struct IAnimationBlending;

//--------------------------------------------------------------------------------

#if 0
	#define ANIMCHAR_SIZED_VAR(type, name, bits) type name
#else
	#define ANIMCHAR_SIZED_VAR(type, name, bits) type name: bits
#endif

//--------------------------------------------------------------------------------

struct IVec
{
	Vec3 normal;
	Vec3 pos;
};

//--------------------------------------------------------------------------------

enum EDebugHistoryID
{
	eDH_Undefined,

	eDH_FrameTime,

	eDH_TurnSpeed,
	eDH_TravelSpeed,
	eDH_TravelDist,
	eDH_TravelDistScale,
	eDH_TravelDirX,
	eDH_TravelDirY,

	eDH_StateSelection_State,
	eDH_StateSelection_StartTravelSpeed,
	eDH_StateSelection_EndTravelSpeed,
	eDH_StateSelection_TravelDistance,
	eDH_StateSelection_StartTravelAngle,
	eDH_StateSelection_EndTravelAngle,
	eDH_StateSelection_EndBodyAngle,

	eDH_MovementControlMethodH,
	eDH_MovementControlMethodV,

	eDH_EntMovementErrorTransX,
	eDH_EntMovementErrorTransY,
	eDH_EntMovementErrorRotZ,
	eDH_EntTeleportMovementTransX,
	eDH_EntTeleportMovementTransY,
	eDH_EntTeleportMovementRotZ,
	eDH_ProceduralAnimMovementTransX,
	eDH_ProceduralAnimMovementTransY,
	eDH_ProceduralAnimMovementRotZ,
	eDH_AnimTargetCorrectionTransX,
	eDH_AnimTargetCorrectionTransY,
	eDH_AnimTargetCorrectionRotZ,
	eDH_AnimAssetTransX,
	eDH_AnimAssetTransY,
	eDH_AnimAssetTransZ,
	eDH_AnimAssetRotZ,

	eDH_EntLocationPosX,
	eDH_EntLocationPosY,
	eDH_EntLocationOriZ,

	eDH_AnimErrorDistance,
	eDH_AnimErrorAngle,

	eDH_AnimEntityOffsetTransX,
	eDH_AnimEntityOffsetTransY,
	eDH_AnimEntityOffsetRotZ,

	eDH_ReqEntMovementTransX,
	eDH_ReqEntMovementTransY,
	eDH_ReqEntMovementRotZ,

	eDH_CarryCorrectionDistance,
	eDH_CarryCorrectionAngle,

	eDH_TEMP00,
	eDH_TEMP01,
	eDH_TEMP02,
	eDH_TEMP03,

	/*
	   eDH_EntityMoveSpeed,
	   eDH_EntityPhysSpeed,
	   eDH_ACRequestSpeed,
	 */
};

//--------------------------------------------------------------------------------

enum EMCMComponent
{
	eMCMComponent_Horizontal = 0,
	eMCMComponent_Vertical,

	eMCMComponent_COUNT,
};

//--------------------------------------------------------------------------------

enum RequestedEntityMovementType
{
	RequestedEntMovementType_Undefined = -1,
	RequestedEntMovementType_Absolute  = 1,
	RequestedEntMovementType_Impulse   = 2, // Relative
};

//--------------------------------------------------------------------------------
struct IAnimatedCharacterListener
{
	virtual ~IAnimatedCharacterListener() {}
	virtual void OnCharacterChange() = 0; // called when switching character or first -> third person switch
};

//--------------------------------------------------------------------------------
// TODO: Shuffle variable members around to better align and pack things tightly and reduce padding.
class CAnimatedCharacter : public CGameObjectExtensionHelper<CAnimatedCharacter, IAnimatedCharacter>, public IAnimationGraphStateListener
{
public:
	CAnimatedCharacter();
	~CAnimatedCharacter();

	// IAnimatedCharacter
	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId)                                                       {};
	virtual void                 PostInit(IGameObject* pGameObject);
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual void                 Release();
	virtual void                 FullSerialize(TSerialize ser);
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return true; }
	virtual void                 ProcessEvent(const SEntityEvent& event);
	virtual uint64               GetEventMask() const;
	virtual void                 PostSerialize();
	virtual void                 SerializeSpawnInfo(TSerialize ser) {}
	virtual ISerializableInfoPtr GetSpawnInfo()                     { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int);
	virtual void                 HandleEvent(const SGameObjectEvent&);
	virtual void                 SetChannelId(uint16 id)     {}
	virtual void                 PostUpdate(float frameTime) { CRY_ASSERT(false); }
	virtual void                 PostRemoteSpawn()           {};
	virtual void                 GetMemoryUsage(ICrySizer* s) const;

	virtual IActionController*   GetActionController()
	{
		return m_pActionController;
	}

	virtual const IActionController* GetActionController() const
	{
		return m_pActionController;
	}

	virtual void                             SetShadowCharacterSlot(int id);

	virtual void                             SetAnimationPlayerProxy(CAnimationPlayerProxy* proxy, int layer);
	virtual CAnimationPlayerProxy*           GetAnimationPlayerProxy(int layer);

	virtual IAnimationGraphState*            GetAnimationGraphState()                  { return m_pMannequinAGState; }
	virtual void                             PushForcedState(const char* state)        {}
	virtual void                             ClearForcedStates()                       {}
	virtual void                             ChangeGraph(const char* graph, int layer) {}

	virtual void                             AddMovement(const SCharacterMoveRequest& request);

	virtual const SPredictedCharacterStates& GetOverriddenMotionParameters() const                                            { return m_moveRequest.prediction; }
	virtual void                             SetOverriddenMotionParameters(const SPredictedCharacterStates& motionParameters) { m_moveRequest.prediction = motionParameters; }
	virtual void                             SetBlendWeightParam(const EMotionParamID motionParamID, const float value, const uint8 targetFlags = eBWPT_All);

	virtual void                             SetEntityRotation(const Quat& rot) {}

	virtual const SAnimatedCharacterParams& GetParams()                        { return m_params; }
	virtual void                            SetParams(const SAnimatedCharacterParams& params);
	virtual int                             GetCurrentStance();
	virtual void                            RequestStance(int stanceID, const char* name);
	virtual bool                            InStanceTransition();

	virtual void                            SetFacialAlertnessLevel(int alertness) { m_facialAlertness = alertness; }
	virtual int                             GetFacialAlertnessLevel()              { return m_facialAlertness; }

	virtual void                            ResetState();
	virtual void                            ResetInertiaCache();

	virtual float                           FilterView(SViewParams& viewParams) const;

	virtual uint32                          MakeFace(const char* pExpressionName, bool usePreviewChannel = true, float lifeTime = 1.f);

	virtual void                            AllowLookIk(bool allow, int layer = -1);
	virtual bool                            IsLookIkAllowed() const { return (m_disallowLookIKFlags == 0); }

	virtual void                            AllowAimIk(bool allow);
	virtual bool                            IsAimIkAllowed() const { return m_allowAimIk; }

	virtual void                            TriggerRecoil(float duration, float kinematicImpact, float kickIn /*=0.8f*/, EAnimatedCharacterArms arms /*= eACA_BothArms*/);
	virtual void                            SetWeaponRaisedPose(EWeaponRaisedPose pose);

	virtual SGroundAlignmentParams&         GetGroundAlignmentParams();

	//! Override for the entity's desired movement -> replace it with animation movement components
	void                           UseAnimationMovementForEntity(bool xyMove, bool zMove, bool rotation);

	virtual EColliderMode          GetPhysicalColliderMode() const { return m_colliderMode; }
	virtual void                   ForceRefreshPhysicalColliderMode();
	virtual void                   RequestPhysicalColliderMode(EColliderMode mode, EColliderModeLayer layer, const char* tag = NULL);
	virtual void                   SetCharacterCollisionFlags(unsigned int flags) { m_characterCollisionFlags = flags; }
	virtual void                   SetMovementControlMethods(EMovementControlMethod horizontal, EMovementControlMethod vertical);
	virtual void                   SetMovementControlMethods(EMCMSlot slot, EMovementControlMethod horizontal, EMovementControlMethod vertical, const char* tag = NULL);

	virtual void                   SetDoMotionParams(bool doMotionParams) { m_doMotionParams = doMotionParams; }
	virtual void                   EnableLandBob(const SLandBobParams& landBobParams);
	virtual void                   DisableLandBob();

	virtual const QuatT&           GetAnimLocation() const                 { return m_entLocation; }
	virtual const float            GetEntitySpeed() const                  { return m_actualEntSpeed; }
	virtual const float            GetEntitySpeedHorizontal() const        { return m_actualEntSpeedHorizontal; }
	virtual const Vec2&            GetEntityMovementDirHorizontal() const  { return m_actualEntMovementDirHorizontal; }
	virtual const Vec2&            GetEntityVelocityHorizontal() const     { return m_actualEntVelocityHorizontal; }
	virtual const Vec2&            GetEntityAccelerationHorizontal() const { return m_actualEntAccelerationHorizontal; }
	virtual const float            GetEntityTangentialAcceleration() const { return m_actualEntTangentialAcceleration; }
	virtual const Vec3&            GetExpectedEntMovement() const          { return m_expectedEntMovement; }
	virtual float                  GetAngularSpeedHorizontal() const;

	virtual void                   SetNoMovementOverride(bool external);

	virtual EMovementControlMethod GetMCMH() const;
	virtual EMovementControlMethod GetMCMV() const;

	virtual void                   SetInGrabbedState(bool bEnable);

	// Returns the angle (in degrees) of the ground below the character.
	// Zero is flat ground (along facing direction), positive when character facing uphill, negative when character facing downhill.
	virtual float GetSlopeDegreeMoveDir() const { return m_fGroundSlopeMoveDirSmooth; }
	virtual float GetSlopeDegree() const        { return m_fGroundSlopeSmooth; }

	void          ForceMovement(const QuatT& relativeMovement);
	void          ForceOverrideRotation(const Quat& qWorldRotation);

#if DEBUG_VELOCITY()
	virtual void AddDebugVelocity(const QuatT& movement, const float frameTime, const char* szComment, const ColorF& colorF, const bool pastMovement = false) const;
	virtual bool DebugVelocitiesEnabled() const;
#endif
	// ~IAnimatedCharacter

	// IAnimationGraphStateListener
	virtual void SetOutput(const char* output, const char* value);
	virtual void QueryComplete(TAnimationGraphQueryID queryID, bool succeeded);
	virtual void DestroyedState(IAnimationGraphState*);
	// ~IAnimationGraphStateListener

	// TODO: Is this function really needed? It's only used in CAGFreeFall::CanLeaveState
	ILINE const QuatT& GetPrevEntityLocation() const { return m_prevEntLocation; }

	// TODO: Is this function really needed? It's only used in some special case in ExactPositioning.cpp
	ILINE const QuatT& GetRequestedEntityMovement() const { return m_requestedEntityMovement; }

	// Need to be accessible for the event proxies.
	void       PrepareAnimatedCharacterForUpdate();
	void       GenerateMovementRequest();
	void       PrepareAndStartAnimProc();

private:
	void PerformSimpleMovement();
	void DeleteActionController();
	void SetActionController(const char* filename);

	bool NoMovementOverride() const;

	void CalculateParamsForCurrentMotions();

	void PostAnimationUpdate();
	void PostProcessingUpdate();

	//void OnPhysicsPreStep( float frameTime );
	void       InitVars();
	void       ResetVars();

	void       DisableEntitySystemCharacterUpdate();

	void       InitShadowCharacter();

	void       UpdateTime();

	void       UpdateMCMs();
	void       UpdateMCMComponent(EMCMComponent component);
	void       PreventAnimTargetOvershooting(EMCMComponent component);
	QuatT      OverrideEntityMovement(const QuatT& ent, const QuatT& anim) const;
	QuatT      MergeMCM(const QuatT& ent, const QuatT& anim, bool flat) const;

	ILINE bool RecentQuickLoad() { return ((m_lastSerializeReadFrameID + 3) > m_curFrameID); }

	bool       EvaluateSimpleMovementConditions() const;
	void       UpdateSimpleMovementConditions();

	bool       InitializeMannequin();

	void       PreAnimationUpdate();

	void       UpdateGroundAlignment();

	void       UpdateSkeletonSettings();

	void       SetDesiredLocalLocation(ISkeletonAnim* pSkeletonAnim, const QuatT& desiredLocalLocation, float fDeltaTime);
	void       SetMotionParam(ISkeletonAnim* const pSkeletonAnim, const EMotionParamID motionParamID, const float value) const;

	void       GetCurrentEntityLocation();
	QuatT      CalculateDesiredAnimMovement() const;
	void       AcquireRequestedBehaviourMovement();
	QuatT      CalculateWantedEntityMovement(const QuatT& desiredAnimMovement) const;

	void       RefreshAnimTarget();

	ILINE bool RecentCollision() const { return ((m_collisionFrameID + 5) > m_curFrameID); }
	Vec3       RemovePenetratingComponent(const Vec3& v, const Vec3& n) const;

	ILINE bool InCutscene()
	{
		if ((m_pSkeletonAnim != NULL) && (m_pSkeletonAnim->GetTrackViewStatus() != 0))
			return true;

		return false;
	}

	QuatT CalculateDesiredLocation() const;

	void  CalculateAndRequestPhysicalEntityMovement();
	void  RequestPhysicalEntityMovement(const QuatT& wantedEntMovement);

	// PROTOTYPE: Limit entity velocity to animation velocity projected along entity velocity direction.
	QuatT      DoEntityClamping(const QuatT& wantedEntMovement) const;

	ILINE bool AllowEntityClamping() const
	{
		bool allowEntityClamp = (GetMCMH() == eMCM_ClampedEntity);

		/*
		   // NOTE: During AnimTarget MCM is forced to Entity (approaching/preparing) or Animation (active), so this is redundant.
		   const SAnimationTarget* pAnimTarget = m_animationGraphStates.GetAnimationTarget();
		   bool hasAnimTarget = (pAnimTarget != NULL) && (pAnimTarget->preparing || pAnimTarget->activated);
		 */

		/*
		   // NOTE: MCM is forced to Entity for all players, so this is redundant.
		   // NOTE: Actually, it's only entity in first person, not nessecarily in third person.
		   IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId());
		   bool isPlayer = pActor->IsPlayer();
		 */

#ifdef _DEBUG
		if (allowEntityClamp && (CAnimationGraphCVars::Get().m_entityAnimClamp == 0))
			allowEntityClamp = false;
#endif
		return allowEntityClamp;
	}

	void UpdatePhysicalColliderMode();
	void UpdatePhysicsInertia();

	void CreateExtraSolidCollider();
	void DestroyExtraSolidCollider();

	void EnableRigidCollider(float radius);
	void DisableRigidCollider();

	void UpdateCharacterPtrs();
	void ValidateCharacterPtrs();

	void SetBlendFromRagdollizeParams(const SGameObjectEvent& event);
	void SetRagdollizeParams(const SGameObjectEvent& event);
	void KickOffRagdoll();
	bool StartAnimationProcessing(const QuatT& entityLocation) const;
	void StartAnimationProcessing() const;
	void SetAnimationPostProcessParameters(const QuatT& entityLocation, const bool finishImmediate = false) const;
	void SetAnimationPostProcessParameters() const;

	void ComputeGroundSlope();

	// [Benito] Note:
	// Normal processing assumes that animation processing starts during PrePhysicsUpdate event (See PrePhysicsUpdate/Update functions)
	// This is not the case for the 'grabbed' state, where the final entity location is not known until the very end of the frame
	ILINE bool UseNormalAnimationProcessing() const { return !m_inGrabbedState; }

	// Consistency Tests
	void RunTests();

	// Debug rendering (in world) and text
	bool DebugFilter() const;
	bool DebugTextEnabled() const;
	void DebugRenderCurLocations() const;
	void DebugDisplayNewLocationsAndMovements(const QuatT& entLocation, const QuatT& entMovement,
	                                          const QuatT& animMovement,
	                                          float frameTime) const;

	void LayoutHelper(const char* id, const char* name, bool visible, float minout, float maxout, float minin, float maxin, float x, float y, float w = 1.0f, float h = 1.0f);

	// DebugHistory Graphs
	void SetupDebugHistories();

	//void DebugGraphMotionParams(const ISkeletonAnim* pSkeletonAnim);
	void DebugBlendWeights(const ISkeletonAnim* pSkeletonAnim);

	void DebugHistory_AddValue(const char* id, float x) const;
	void DebugGraphQT(const QuatT& m, const char* tx, const char* ty, const char* rz, const char* tz = 0) const;

public:
	virtual void RegisterListener(IAnimatedCharacterListener* eventListener, const char* name) { m_listeners.Add(eventListener, name); }
	virtual void UnregisterListener(IAnimatedCharacterListener* eventListener)                 { m_listeners.Remove(eventListener); }

private:

	CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate* m_pComponentPrepareCharForUpdate;

	//EventListeners
	typedef CListenerSet<IAnimatedCharacterListener*> TAnimCharListeners;
	TAnimCharListeners m_listeners;

	// Mannequin (not serialized)
	IActionController*              m_pActionController;
	SAnimationContext*              m_pAnimContext;
	const IAnimationDatabase*       m_pAnimDatabase1P;
	const IAnimationDatabase*       m_pAnimDatabase3P;
	const IAnimationDatabase*       m_pSoundDatabase;
	bool                            m_lastACFirstPerson;
	string                          m_AGScopeMask;

	MannequinAG::CMannequinAGState* m_pMannequinAGState;

	CAnimationPlayerProxy*          m_pAnimationPlayerProxies[eAnimationGraphLayer_COUNT];

	// Not serialized
	SAnimatedCharacterParams m_params; // TODO: Garbage collect unused members of this struct.

	// Not serialized. Cached values for updating inertia
	float m_fPrevInertia;
	float m_fPrevInertiaAccel;
	float m_fPrevTimeImpulseRecover;

	// Not serialized
	bool m_moveOverride_useAnimXY;
	bool m_moveOverride_useAnimZ;
	bool m_moveOverride_useAnimRot;

	// Not serialized
	int m_facialAlertness; // TODO: Make this an int8 instead. We never use more than a few unique values.

	// Serialized
	// TODO: Turn the stance enums into uint8 instead of int32.
	int m_currentStance;

	// Not serialized
	int                    m_requestedStance;
	TAnimationGraphQueryID m_stanceQuery;

	unsigned int           m_disallowLookIKFlags; // 1 bit for each animationgraph layer plus one for the game. LookIK is allowed if this is 0.
	ANIMCHAR_SIZED_VAR(bool, m_allowAimIk, 1);

	// Not serialized
	int         m_curFrameID;                   // Updated every frame in UpdateTime().
	int         m_lastResetFrameId;             // Initialized to the current frame id in ResetVars().
	int         m_updateGrabbedInputFrameID;    // This is updated in PostAnimationUpdate(), when checking grabbed AG input.
	int         m_moveRequestFrameID;           // NOTE: GetFrameID() returns an int and this is initialized to -1 in ResetVars().
	mutable int m_lastAnimProcFrameID;          // last time StartAnimationProcessing was called.
	int         m_lastSerializeReadFrameID;     //
	int         m_lastPostProcessUpdateFrameID; //

	int         m_shadowCharacterSlot;
	bool        m_hasShadowCharacter;

	// TODO: Pack as bits with other bools.
	bool              m_bSimpleMovementSetOnce;

	EWeaponRaisedPose m_curWeaponRaisedPose;

	// Not serialized
	SCharacterMoveRequest m_moveRequest;

	// Not serialized
	// TODO: Pack these as bits instead.
	bool m_isPlayer;

	// Not serialized
	CTimeValue m_curFrameStartTime;

	// Not serialized
	double m_curFrameTime;
	double m_prevFrameTime;
	double m_curFrameTimeOriginal;

	// Not serialized
	bool  m_hasForcedMovement;
	QuatT m_forcedMovementRelative;

	bool  m_hasForcedOverrideRotation;
	Quat  m_forcedOverrideRotationWorld;

	// Not serialized
	QuatT m_entLocation;     // Current entity location.
	QuatT m_prevEntLocation; // Previous entity location.
	Vec3  m_expectedEntMovement;
	QuatT m_actualEntMovement;   // This is the actual measured difference in location between two frames.
	QuatT m_entTeleportMovement; // Offset between entity locations between and after set pos/rot (teleportation).
	QuatT m_desiredLocalLocation;

	// PROCEDURAL LEANING STUFF (wip)
	// Not serialized
	bool  EnableProceduralLeaning();
	float GetProceduralLeaningScale();
	QuatT CalculateProceduralLeaning();
#define NumAxxSamples 32
	Vec2       m_reqLocalEntAxx[NumAxxSamples];
	Vec2       m_reqEntVelo[NumAxxSamples];
	CTimeValue m_reqEntTime[NumAxxSamples];
	int        m_reqLocalEntAxxNextIndex;
	Vec3       m_smoothedActualEntVelo;
	float      m_smoothedAmountAxx;
	Vec3       m_avgLocalEntAxx;

	bool       m_doMotionParams;

	// Not serialized
	// TODO: Pack these two as one uint8, 4 bits each.
	int8 m_requestedEntMoveDirLH4;
	int8 m_actualEntMoveDirLH4;

	// Serialized
	// TODO: Make sure EMovementControlMethod is not bigger than 8 bits.
	EMovementControlMethod m_movementControlMethod[eMCMComponent_COUNT][eMCMSlot_COUNT];
	const char*            m_movementControlMethodTags[eMCMSlot_COUNT];
	const char*            m_currentMovementControlMethodTags[eMCMComponent_COUNT];
	float                  m_elapsedTimeMCM[eMCMComponent_COUNT];

	// Serialized
	// TODO: Make sure EColliderMode is not bigger than 8 bits.
	const char*   m_colliderModeLayersTag[eColliderModeLayer_COUNT];
	EColliderMode m_colliderModeLayers[eColliderModeLayer_COUNT];
	EColliderMode m_colliderMode;

	// Not serialized
	// TODO: Try to pack these into bits instead. ANIMCHAR_SIZED_VAR(bool, name, 1);
	bool m_simplifiedAGSpeedInputsRequested;
	bool m_simplifiedAGSpeedInputsActual;

	// Not serialized
	QuatT                       m_requestedEntityMovement;
	RequestedEntityMovementType m_requestedEntityMovementType;
	int                         m_requestedIJump;          // TODO: Turn this into an int8 instead.
	bool                        m_bDisablePhysicalGravity; // TODO: Pack this into bits instead.

	// Not serialized
	bool  m_simplifyMovement;                 // TODO: Pack this into bits instead.
	bool  m_forceDisableSlidingContactEvents; // TODO: Pack this into bits instead.
	float m_noMovementTimer;                  // TODO: This does not have to be very accurate. Try packing it as an uint8 or uint16.
	Vec2  m_actualEntVelocityHorizontal;
	Vec2  m_actualEntMovementDirHorizontal;
	Vec2  m_actualEntAccelerationHorizontal;
	float m_actualEntTangentialAcceleration;
	float m_actualEntSpeed;

	// Serialized
	float m_actualEntSpeedHorizontal; // TODO: Look into how this is actually used. (at least it's converted from QuatT to float, rest was redundant)

	// Not serialized
	const SExactPositioningTarget* m_pAnimTarget;
	ICharacterInstance*            m_pCharacter;
	ISkeletonAnim*                 m_pSkeletonAnim;
	ISkeletonPose*                 m_pSkeletonPose;
	ICharacterInstance*            m_pShadowCharacter;
	ISkeletonAnim*                 m_pShadowSkeletonAnim;
	ISkeletonPose*                 m_pShadowSkeletonPose;

	// Not serialized
	// TODO: Pack these as bits instead.
	bool m_noMovementOverrideExternal;
	bool m_bAnimationGraphStatePaused;

	// Not serialized
	f32                     m_fJumpSmooth;
	f32                     m_fJumpSmoothRate;

	f32                     m_fGroundSlopeMoveDirSmooth;
	f32                     m_fGroundSlopeMoveDirRate;

	f32                     m_fGroundSlope;
	f32                     m_fGroundSlopeSmooth;
	f32                     m_fGroundSlopeRate;

	f32                     m_fRootHeightSmooth;
	f32                     m_fRootHeightRate;

	SLandBobParams          m_landBobParams;
	f32                     m_fallMaxHeight;
	f32                     m_landBobTime;
	f32                     m_totalLandBob;
	bool                    m_wasInAir;

	SGroundAlignmentParams  m_groundAlignmentParams;

	f32                     m_fDesiredMoveSpeedSmoothQTX;
	f32                     m_fDesiredMoveSpeedSmoothRateQTX;
	f32                     m_fDesiredTurnSpeedSmoothQTX;
	f32                     m_fDesiredTurnSpeedSmoothRateQTX;
	Vec2                    m_fDesiredStrafeSmoothQTX;
	Vec2                    m_fDesiredStrafeSmoothRateQTX;
	bool                    m_forcedRefreshColliderMode;

	bool                    m_bPendingRagdoll;
	SRagdollizeParams       m_ragdollParams;
	SBlendFromRagdollParams m_blendFromRagollizeParams;

	// Not serialized
	// Move these to pack better with other small variables.
	uint8 m_prevAnimPhaseHash;
	float m_prevAnimEntOffsetHash; // TODO: This could maybe be turned into an int8 and packed with some other 8 bitters.
	float m_prevMoveVeloHash;
	uint8 m_prevMoveJump;

	// Serialized

	int              m_collisionFrameID;
	int              m_collisionNormalCount;
	Vec3             m_collisionNormal[4];

	IPhysicalEntity* m_pFeetColliderPE;
	IPhysicalEntity* m_pRigidColliderPE;

	// Not serialized
	unsigned int m_characterCollisionFlags; // geom_colltype_player by default.

	// DebugHistory Graphs
	// TODO: Look at how this is managed.
	mutable IDebugHistoryManager* m_debugHistoryManager;

	IAnimationPoseAlignerPtr      m_pPoseAligner;

	mutable int                   m_lastAnimationUpdateFrameId;
	bool                          m_inGrabbedState;

	bool                          m_useMannequinAGState;

	bool                          m_proxiesInitialized;
};

//--------------------------------------------------------------------------------
#undef UNIQUE
#if defined(CRY_PLATFORM_64BIT)
	#define UNIQUE(s) (s + string().Format("%016llX", (uint64) this)).c_str()
#else
	#define UNIQUE(s) (s + string().Format("%08X", (uint32) this)).c_str()
#endif

//--------------------------------------------------------------------------------

extern float ApplyAntiOscilationFilter(float value, float filtersize);
extern void  ScaleQuatAngles(Quat& q, const Ang3& a);
extern float GetQuatAbsAngle(const Quat& q);
extern f32   GetYaw(const Vec3& v0, const Vec3& v1);
extern f32   GetYaw(const Vec2& v0, const Vec2& v1);
extern QuatT ApplyWorldOffset(const QuatT& origin, const QuatT& offset);
extern QuatT GetWorldOffset(const QuatT& origin, const QuatT& destination);
extern QuatT GetClampedOffset(const QuatT& offset, float maxDistance, float maxAngle, float& distance, float& angle);
extern QuatT ExtractHComponent(const QuatT& m);
extern QuatT ExtractVComponent(const QuatT& m);
extern QuatT CombineHVComponents2D(const QuatT& cmpH, const QuatT& cmpV);
extern QuatT CombineHVComponents3D(const QuatT& cmpH, const QuatT& cmpV);
extern void  DebugRenderAngleMeasure(CPersistantDebug* pPD, const Vec3& origin, const Quat& orientation, const Quat& offset, float fraction);
extern void  DebugRenderDistanceMeasure(CPersistantDebug* pPD, const Vec3& origin, const Vec3& offset, float fraction);
extern QuatT GetDebugEntityLocation(const char* name, const QuatT& _default);

//--------------------------------------------------------------------------------

#undef ANIMCHAR_SIZED_VAR

#endif
