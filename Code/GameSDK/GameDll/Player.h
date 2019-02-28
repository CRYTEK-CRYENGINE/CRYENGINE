// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the player.
  
 -------------------------------------------------------------------------
  History:
  - 29:9:2004: Created by Filippo De Luca

*************************************************************************/
#ifndef __PLAYER_H__
#define __PLAYER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Actor.h"												
#include "StealthKill.h"
#include "SpectacularKill.h"									
#include "LargeObjectInteraction.h"																	
#include "PlayerPlugin_CurrentlyTargetting.h"					
#include "PlayerModifiableValues.h"								
#include "Effects/GameEffects/HitRecoilGameEffect.h"			
#include "Effects/GameEffects/PlayerHealthEffect.h"					
#include "HitDeathReactionsDefs.h"								
#include "LookAim_Helper.h"										
#include "DualCharacterProxy.h"									
#include "MountedGunController.h"								
#include "Item.h"												
#include "PlayerStateSwim_WaterTestProxy.h"						
#include "IPlayerInput.h"										
#include "EntityUtility/EntityEffectsHeat.h"								
#include "PlayerMovementDebug.h"								
#include "PlayerRotation.h"										
#include "ProceduralWeaponAnimation.h"
#include "IKTorsoAim_Helper.h"
#include "WeaponFPAiming.h"
#include "EquipmentLoadout.h"
#include "PlayerAI.h"
#include "State.h"
#include "IPlayerProfiles.h"
#include "ICameraMode.h"


struct IPlayerInput;
class CVehicleClient;
struct IInteractor;
class CHitDeathReactions;
struct IPlayerUpdateListener;
class CPlayerCamera;
class CPlayerPlugin_Interaction;
struct SInteractionInfo;
struct IPlayerEventListener;
class CPickAndThrowProxy;
class CLocalPlayerComponent;
struct SFollowCameraSettings;
class CPlayerPluginEventDistributor;
class CPlayerPlugin_InteractiveEntityMonitor;
class CSprintStamina;
class CMasterFader;

typedef ICameraMode::AnimationSettings PlayerCameraAnimationSettings;

//////////////////////////////////////////////////////////////////////////

class CPlayerEntityInteraction;
DECLARE_SHARED_POINTERS(CPickAndThrowProxy);

#if !defined(_RELEASE)
#define ENABLE_PLAYER_HEALTH_REDUCTION_POPUPS		(1)
#else
#define ENABLE_PLAYER_HEALTH_REDUCTION_POPUPS		(0)
#endif // #if !defined(_RELEASE)

union USpectatorModeData
{
	struct SFixed
	{
		EntityId  location;
	}
	fixed;
	struct SFree
	{
		uint8  _dummy_;
	}
	free;
	struct SFollow
	{
		EntityId  spectatorTarget;			// which player we are following
		float     invalidTargetTimer;		// how long have we been looking at an invalid spectator target
	}
	follow;
	struct SKiller
	{
		EntityId  spectatorTarget;		// which killer we are following
		float     startTime;					// Time that we enetered Killer view.
	}
	killer;

	void Reset()
	{
		memset(this, 0, sizeof(*this));
	}
};

struct SSpectatorInfo
{
	USpectatorModeData	dataU;
	float								yawSpeed;
	float								pitchSpeed;
	bool								rotateSpeedSingleFrame; //If true yawSpeed/pitchSpeed should be processed only once, otherwise process every frame until told otherwise
	uint8								mode;  // see CActor::EActorSpectatorMode enum
	uint8								state; // see CActor::EActorSpectatorState enum

	void Reset()
	{
		uint8  oldstate = state;
		memset(this, 0, sizeof(*this));
		state = oldstate;
		yawSpeed = 0.f;
		pitchSpeed = 0.f;
		rotateSpeedSingleFrame = false;
	}

	EntityId* GetOtherEntIdPtrForCurMode()
	{
		EntityId*  ptr = NULL;
		switch (mode)
		{
			case CActor::eASM_Fixed:	ptr = &dataU.fixed.location; break;
			case CActor::eASM_Follow:	ptr = &dataU.follow.spectatorTarget; break;
			case CActor::eASM_Killer:	ptr = &dataU.killer.spectatorTarget; break;
		}
		return ptr;
	}

	EntityId GetOtherEntIdForCurMode()
	{
		EntityId  e = 0;
		if (EntityId* ptr=GetOtherEntIdPtrForCurMode())
			e = (*ptr);
		return e;
	}

	void SetOtherEntIdForCurMode(EntityId e)
	{
		if (EntityId* ptr=GetOtherEntIdPtrForCurMode())
		{
			(*ptr) = e;
		}
	}
};

struct SMeleeHitParams
{
	SMeleeHitParams()
		: m_targetId(INVALID_ENTITYID)
		, m_hitOffset(ZERO)
		, m_hitNormal(ZERO)
		, m_surfaceIdx(0)
		, m_boostedMelee(false)
	{}

	EntityId m_targetId;
	Vec3 m_hitOffset;
	Vec3 m_hitNormal;
	int m_surfaceIdx;
	bool m_boostedMelee;
};

struct SPlayerStats : public SActorStats
{

	enum ECinematicFlags
	{
		eCinematicFlag_HolsterWeapon	= BIT(0),	//Holster weapon, a locks weapon selection
		eCinematicFlag_LowerWeapon		= BIT(1),	//Lowers weapon, and weapon functionality
		eCinematicFlag_LowerWeaponMP	= BIT(2),	//Lowers weapon, and weapon functionality - but leaves other suit abilities unaffected
		eCinematicFlag_RestrictMovement	= BIT(3),	//No sprint, no super-jump
		eCinematicFlag_WalkOnly			= BIT(4),	//Limits movement speed to 'walk' speed
	};

	float flashBangStunMult;
	float flashBangStunTimer;
	float flashBangStunLength;

	float zeroVelocityForTime;

	float inMovement;//for how long the player is moving


	float stuckTimeout;

	float partialCameraAnimFactor;
	float partialCameraAnimBlendRate;
	float partialCameraAnimTarget;

	// falling things
	float fallSpeed;
	float downwardsImpactVelocity;

	bool isThirdPerson;
	bool isInPickAndThrowMode;
	bool isScoped;
	bool bIsInSmoke;
	bool bIgnoreSprinting;
	bool bDisableTranslationPinning;

	enum EForceSTAP
	{
		eFS_None=0,
		eFS_Off,
		eFS_On,
	};
	EForceSTAP forceSTAP;

	EntityId animationControlledID;

	//cheating stuff
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	uint8 flyMode;//0 no fly, 1 fly mode, 2 fly mode + noclip
#endif
	uint8 cinematicFlags;
	uint8 isAnimatedSlave;	// Don't serialize. Local.
	
	CCoherentValue<uint8> followCharacterHead;

	EntityId pickAndThrowEntity; // entity that is being hold in PickAndThrowMode (0 when is not in pickandthrow mode )
	EntityId prevPickAndThrowEntity; // Last entity that was being held in PickAndThrowMode
	SSpectatorInfo spectatorInfo;

	EntityId recentKiller;
	EntityId lastAttacker;
	int killedByDamageType;

	float	fInSmokeTime;

	SPlayerStats()
	{
		memset(this,0,sizeof(SPlayerStats)); // This will zero everything, fine.
		new (this) SActorStats(); // But this will set certain Vec3 to QNAN, due to the new default constructors.

		flashBangStunMult = 1.f;
		flashBangStunTimer = 0.f;
		flashBangStunLength = 0.f;

		inAir = 0.0f;

		bIgnoreSprinting = false;
		forceSTAP = eFS_None;
		bDisableTranslationPinning = false;
		isInPickAndThrowMode = false;
		
		pickAndThrowEntity = 0;
		prevPickAndThrowEntity = 0;

		fallSpeed = 0.0f;
		downwardsImpactVelocity = 0.0f;

		isAnimatedSlave = 0;

		spectatorInfo.Reset();

		recentKiller = 0;
		lastAttacker = 0;
		killedByDamageType = CGameRules::EHitType::Invalid;

		fInSmokeTime = 0.0f;
		bIsInSmoke = false;

		feetHeight[0] = -10000.f;
		feetHeight[1] = -10000.f;
	}

	void Serialize( TSerialize ser, EEntityAspects aspects );
	void GetMemoryUsage( ICrySizer *pSizer ) const { /*nothing */}
};

struct SAimAccelerationParams
{
	SAimAccelerationParams();

	float angle_min;
	float angle_max;
};

struct SPlayerRotationParams
{
	enum EAimType
	{
		EAimType_NORMAL,
		EAimType_CROUCH,
		EAimType_SLIDING,
		EAimType_SPRINTING,
		EAimType_SWIM,
		EAimType_MOUNTED_GUN,
		EAimType_TOTAL
	};

	SAimAccelerationParams m_horizontalAims[2][EAimType_TOTAL];
	SAimAccelerationParams m_verticalAims[2][EAimType_TOTAL];

	void Reset(const IItemParamsNode* pRootNode);

	ILINE const SAimAccelerationParams& GetHorizontalAimParams(SPlayerRotationParams::EAimType aimType, bool firstPerson) const
	{
		CRY_ASSERT((aimType >= 0) && (aimType < EAimType_TOTAL));

		return m_horizontalAims[firstPerson ? 0 : 1][aimType];
	}

	ILINE const SAimAccelerationParams& GetVerticalAimParams(SPlayerRotationParams::EAimType aimType, bool firstPerson) const
	{
		CRY_ASSERT((aimType >= 0) && (aimType < EAimType_TOTAL));

		return m_verticalAims[firstPerson ? 0 : 1][aimType];
	}

private:
	void ReadAimParams(const IItemParamsNode* pRootNode, const char* aimTypeName, EAimType aimType, bool firstPerson);
	void ReadAccelerationParams(const IItemParamsNode* node, SAimAccelerationParams* output);
};

struct SFrameMovementModifiers
{
	SFrameMovementModifiers()
		: weaponSpeedModifier(1.0f)
		, weaponRotationModifier(1.0f)
		, totalSpeedModifier(1.0f)
	{

	}

	void SetFrameModfiers(const float _weaponSpeedMod, const float _weaponRotationMod, const float _totalSpeedMod)
	{
		weaponSpeedModifier = _weaponSpeedMod;
		weaponRotationModifier = _weaponRotationMod;
		totalSpeedModifier = _totalSpeedMod;
	}

	ILINE float GetWeaponSpeedModifier() const { return weaponSpeedModifier; }
	ILINE float GetWeaponRotationModifier() const { return weaponRotationModifier; }
	ILINE float GetTotalSpeedModifier() const { return totalSpeedModifier; }

private:

	float	weaponSpeedModifier;
	float	weaponRotationModifier;
	float	totalSpeedModifier;
};

struct SXPEvents
{
	struct SEvent
	{
		SEvent()
			: xpDelta(0)
			, xpReason(k_XPRsn_MatchBonus)
		{}
		int					xpDelta;
		EXPReason		xpReason;
	};

	SEvent				events[15];
	int						numEvents;			// sent as ui4 - hence max is 15

	SXPEvents()
		: numEvents(0)
	{}

	void SerializeWith(TSerialize ser);
};

struct SMicrowaveBeamParams
{
	SMicrowaveBeamParams()	{	}
	SMicrowaveBeamParams(const Vec3& _pos, const Vec3& _dir)
	{
		position = _pos;
		direction = _dir;
	}

	void SerializeWith(TSerialize ser);

	Vec3 position;
	Vec3 direction;
};

struct SNetPlayerProgression
{
private:
	struct SSerVals
	{
		uint16	xp;
		int8	rank;
		int8	reincarnations;
		int8	defaultMode;
		int8	stealth;
		int8	armour;
	};
public:
	void	Construct(CPlayer* player);
	void	Serialize(TSerialize ser, EEntityAspects aspect);
	void	GetValues(int* outXp, int* outRank, int* outDefault, int* outStealth, int* outArmour, int* outReincarnations);
	void  OwnClientConnected();
	
#if (USE_DEDICATED_INPUT)
	void	SetRandomValues();
#endif

private:
	void	SyncOnLocalPlayer(const bool serialized = true);
	void SetSerializedValues(int newXp, int newRank, int newDefault, int newStealth, int newArmour, int newReincarnations);
private:
	SSerVals  m_serVals;
	const CPlayer*  m_player;
};

struct SDeferredFootstepImpulse
{
	SDeferredFootstepImpulse()
		: m_queuedRayId(0) 
		,m_impulseAmount(ZERO)
	{

	}

	~SDeferredFootstepImpulse()
	{
		CancelPendingRay();
	}

	void DoCollisionTest(const Vec3 &startPos, const Vec3 &dir, float distance, float impulseAmount, IPhysicalEntity* pSkipEntity);
	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

private:

	void CancelPendingRay();

	QueuedRayID m_queuedRayId;
	Vec3		m_impulseAmount;
};

enum ELadderLeaveLocation
{
	eLLL_First = 0,
	eLLL_Top = eLLL_First,
	eLLL_Bottom,
	eLLL_Drop,
	eLLL_Count,
};

class CPlayer :
	public CActor, public IPlayerProfileListener
{
	friend class CPlayerStateMovement;
	friend class CPlayerStateAIMovement;
	friend class CPlayerStateGround;
	friend class CPlayerStateDead;
	friend class CPlayerStateJump;
	friend class CPlayerStateLedge;
	friend class CPlayerStateUtil;
	friend class CPlayerStateFly;
	friend class CPlayerStateSwim;
	friend class CPlayerStateLadder;
	friend class CPlayerStateLinked;
	friend class CPlayerStateAnimationControlled;
	friend class CSlideController;
	friend class CPlayerMovementController;
	friend class CPlayerRotation;
	friend class CPlayerInput;
	friend class CAIInput;
	friend class CNetPlayerInput;
	
	//ergh. Better way of doing this?
	friend class CLocalPlayerComponent;

private:
	typedef CActor inherited;

	DECLARE_STATE_MACHINE( CPlayer, Movement );

	void InitMannequinParams();
public:


	enum EPlayerSounds
	{
		ESound_Player_First,
		ESound_Jump,
		ESound_Fall_Drop,
		ESound_ChargeMelee,
		ESound_Breathing_UnderWater,
		ESound_Gear_Walk,
		ESound_Gear_Run,
		ESound_Gear_Jump,
		ESound_Gear_Land,
		ESound_Gear_HeavyLand,
		ESound_Gear_Water,
		ESound_FootStep_Boot,
		ESound_FootStep_Boot_Armor,
		ESound_DiveIn,
		ESound_DiveOut,
		ESound_WaterEnter,
		ESound_WaterExit,
		ESound_EnterMidHealth,
		ESound_ExitMidHealth,
		ESound_MedicalMonitorRegen,
		ESound_Player_Last
	};

	enum EClientSoundmoods
	{
		ESoundmood_Invalid = -1,
		ESoundmood_Alive = 0,
		ESoundmood_LowHealth,
		ESoundmood_Dead,
		ESoundmood_Killcam,
		ESoundmood_KillcamSlow,
		ESoundmood_Spectating,
		ESoundmood_PreGame,
		ESoundmood_PostGame,
		ESoundmood_EMPBlasted,
		ESoundmood_Last
	};

	enum EReactionOverlay
	{
		EReaction_None,
		EReaction_SmokeEnter,
		EReaction_SmokeLoop,
		EReaction_SmokeExit,
		EReaction_FlashEnter,
		EReaction_FlashLoop,
		EReaction_FlashExit,
		EReaction_Total
	};

	enum EClientPostEffect //first person screen effects for the client player
	{
		EEffect_ChromaShift = 1,
		EEffect_WaterDroplets
	};

	static const NetworkAspectType ASPECT_HEALTH				= eEA_GameServerStatic;

	static const NetworkAspectType ASPECT_PLAYERSTATS_SERVER								= eEA_GameServerA;
	static const NetworkAspectType ASPECT_SPECTATOR													= eEA_GameServerC;
	
	static const NetworkAspectType ASPECT_INPUT_CLIENT											= eEA_Aspect31;
	static const NetworkAspectType ASPECT_INPUT_CLIENT_AUGMENTED						= eEA_GameClientO;

	static const NetworkAspectType ASPECT_CURRENTLYTARGETTING_CLIENT				= eEA_GameClientB;
	static const NetworkAspectType ASPECT_BATTLECHATTER_CLIENT							= eEA_GameClientB;

	static const NetworkAspectType ASPECT_LEDGEGRAB_CLIENT									= eEA_GameClientC;	

	static const NetworkAspectType ASPECT_LAST_MELEE_HIT										= eEA_GameClientD;
	static const NetworkAspectType ASPECT_JUMPING_CLIENT										= eEA_GameClientE;

	static const NetworkAspectType ASPECT_FLASHBANG_SHOOTER_CLIENT					= eEA_GameClientG;

	static const NetworkAspectType ASPECT_SNAP_TARGET												= eEA_GameClientH;

	static const NetworkAspectType ASPECT_RANK_CLIENT												= eEA_GameClientJ;

	static const NetworkAspectType ASPECT_STEALTH_KILL											= eEA_GameServerE;
	static const NetworkAspectType ASPECT_INTERACTIVE_OBJECT								= eEA_GameClientK;
	
	static const NetworkAspectType ASPECT_VEHICLEVIEWDIR_CLIENT							= eEA_GameClientL;

	static const NetworkAspectType ASPECT_CURRENT_ITEM											= eEA_GameClientM;

	static const NetworkAspectType ASPECT_LADDER_SERVER											= eEA_GameClientP;


	static const int MAX_NETWORKED_LEDGE_COUNT = 1024;
	static const int JUMP_COUNTER_MAX  = 8;

	CPlayer();
	virtual ~CPlayer();

	virtual bool IsHeadUnderWater() const override;
	virtual bool IsSwimming() const override;
	virtual bool IsSprinting() const override;
	virtual bool CanFire() const override;
	bool CanMelee() const;
	bool CanToggleCamera() const;
	virtual bool Init( IGameObject * pGameObject ) override;
	virtual void PostInit( IGameObject * pGameObject ) override;
	void ReloadClientXmlData();
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) override;
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override;
	virtual void Update(SEntityUpdateContext& ctx, int updateSlot) override;
	virtual void SerializeSpawnInfo( TSerialize ser ) override;
	virtual ISerializableInfoPtr GetSpawnInfo() override;

	void UpdateAnimationState(const SActorFrameMovementParams &frameMovementParams);

	virtual void PrePhysicsUpdate();
	virtual void UpdateView(SViewParams &viewParams) override;
	virtual void PostUpdateView(SViewParams &viewParams) override;
	void OnFootStepImpulseAnimEvent(ICharacterInstance* pCharacter, const AnimEventInstance &event);
	virtual void GetMemoryUsage(ICrySizer * s) const override;
	void GetInternalMemoryUsage(ICrySizer * s) const;
	virtual void OnFootStepAnimEvent(ICharacterInstance* pCharacter, const char* boneName);
	virtual void OnFoleyAnimEvent(ICharacterInstance* pCharacter, const char* CustomParameter, const char* boneName);
	void OnSwimmingStrokeAnimEvent(ICharacterInstance* pCharacter);
	void ExecuteFootStep(ICharacterInstance* pCharacter, const float frameTime, const int32 nFootJointID);
	void ExecuteFoleySignal(ICharacterInstance* pCharacter, const float frameTime, const char* CustomParameter, const int32 nBoneJointID);
	bool ShouldUpdateNextFootStep() const;
	void ExecuteFootStepsAIStimulus(const float relativeSpeed, const float noiseSupression);
	void OnGroundEffectAnimEvent(ICharacterInstance* pCharacter, const AnimEventInstance &event);
	void ExecuteGroundEffectAnimEvent(ICharacterInstance* pCharacter, const float frameTime, const char* szEffectName, const int32 nJointID);
	void OnKillAnimEvent(const AnimEventInstance &event);
	void UpdateClient( const float frameTime );

	void OnIntroSequenceFinished(); 

	bool IsWeaponUnderWater() const;
	
	virtual bool CanBreakGlass() const override;
	virtual bool MustBreakGlass() const override;

	virtual void Physicalize(EStance stance=STANCE_NULL) override;

	virtual bool SetActorModel(const char* modelName = NULL) override;

	virtual void SetChannelId(uint16 id) override;

	virtual IEntity *LinkToVehicle(EntityId vehicleId) override;
	virtual IEntity *LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach=true) override;
	virtual void LinkToMountedWeapon(EntityId weaponId) override;
	
	virtual void StartInteractiveAction(EntityId entityId, int interactionIndex = 0) override;
	virtual void StartInteractiveActionByName(const char* interaction, bool bUpdateVisibility, float actionSpeed = 1.0f) override;
	virtual void EndInteractiveAction(EntityId entityId) override;
	virtual bool IsInteractiveActionDone() const;

	void AnimationControlled(bool activate, bool bUpdateVisibility=true);
	void PartialAnimationControlled( bool activate, const PlayerCameraAnimationSettings& cameraAnimationSettings );

	void OnReceivingLoadout();
	void AddAmmoToInventory(IInventory* pInventory, IEntityClass* pAmmoClass, IWeapon* pWeapon, int totalCount, int totalCapacity, int increase);
	void RefillAmmo();
	void RefillAmmoDone();
	virtual float GetReloadSpeedScale() const override;
	virtual float GetOverchargeDamageScale() const override;

	virtual void SetViewInVehicle(Quat viewRotation) override;
	virtual Vec3 GetVehicleViewDir() const { return m_vehicleViewDir; }

	ILINE CLocalPlayerComponent* GetLocalPlayerComponent() {return m_pPlayerTypeComponent; }
	ILINE void HoldScreenEffectsUntilNextSpawnRevive(){ m_bDontResetFXUntilNextSpawnRevive = true; } 

	virtual int GetPhysicalSkipEntities(IPhysicalEntity** pSkipList, const int maxSkipSize) const override;

	virtual void SupressViewBlending() override { m_viewBlending = false; }

	ILINE Vec3 GetLastRequestedVelocity() const { return m_lastRequestedVelocity; }
	ILINE bool IsMoving() const { return m_lastRequestedVelocity.GetLengthSquared() > 0.01f; }
	const QuatT& GetAnimationRelativeMovement(int slot = 0) const;
	void SetDeathTimer() { m_fDeathTime = gEnv->pTimer->GetFrameStartTime().GetSeconds(); }
	float GetDeathTime() const { return m_fDeathTime; }

	bool GetForcedLookDir(Vec3& vDir) const;
	void SetForcedLookDir(const Vec3& vDir);
	void ClearForcedLookDir();

	EntityId GetForcedLookObjectId() const;
	void SetForcedLookObjectId(EntityId entityId);
	void ClearForcedLookObjectId();

	bool CanMove() const;

	void SufferingHighLatency(bool highLatency);

	virtual const char* GetActorClassName() const override;

#ifdef STATE_DEBUG
	static void DebugStateMachineEntity( const char* pName );
#endif 

	static  ActorClass GetActorClassType() { return (ActorClass)eActorClass_Player; }
	virtual ActorClass GetActorClass() const override { return (ActorClass)eActorClass_Player; }

	virtual EntityId	GetGrabbedEntityId() const override { return GetPickAndThrowEntity(); }

	IInteractor* GetInteractor();
	void UnlockInteractor(EntityId unlockId);

	virtual void UpdateMountedGunController(bool forceIKUpdate) override;

	ILINE bool	ShouldPlayIntro() const				{ return m_bPlayIntro; }
	void				SetPlayIntro(bool playIntro);

	EntityId GetInteractingEntityId() const;
	ILINE int GetLastInteractionIndex() const { return static_cast<int>(m_lastCachedInteractionIndex); }
	ILINE void SetLastInteractionIndex(int interactionIndex) { m_lastCachedInteractionIndex = static_cast<int8>(interactionIndex); }

	void SetCinematicFlag(const SPlayerStats::ECinematicFlags flag);
	void ResetCinematicFlags();
	ILINE bool IsCinematicFlagActive(const SPlayerStats::ECinematicFlags flag) const
	{
		return ((m_stats.cinematicFlags & flag) != 0);
	}
	ILINE bool IsMovementRestricted() const
	{
		return
			IsCinematicFlagActive(SPlayerStats::eCinematicFlag_RestrictMovement) ||
			IsCinematicFlagActive(SPlayerStats::eCinematicFlag_WalkOnly);
	}
	ILINE bool IsInCinematicMode() const
	{
		return
			IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) &&
			IsCinematicFlagActive(SPlayerStats::eCinematicFlag_RestrictMovement);
	}

	void SetBackToNormalWeapon(const bool dropHeavyWeapon);
	void SetBackToNormalWeapon(CWeapon* pCurrentWeapon, const bool dropHeavyWeaopn);

	const SInteractionInfo& GetCurrentInteractionInfo() const;

	bool IsStealthKilling() const { return m_stealthKill.IsBusy(); }
	bool IsJumping() const;
	bool IsOnLadder() const;
	
  virtual void EnableStumbling(PlayerActor::Stumble::StumbleParameters* stumbleParameters) override;
  virtual void DisableStumbling() override;

	ILINE void BlockMovementInputsForTime(float time)
	{
		m_stats.zeroVelocityForTime = (float)__fsel(time - m_stats.zeroVelocityForTime, time, m_stats.zeroVelocityForTime);
	}
	
	ILINE bool GetBlockMovementInputs() const
	{
		return m_stats.zeroVelocityForTime > 0.f;
	}

	ILINE bool CanHandFire(int hand) const
	{ 
		return m_lookAim.CanHandFire(hand);
	}

	void SetAnimatedCharacterParams( const SAnimatedCharacterParams& params );

	void SwitchPlayerInput(IPlayerInput* pNewPlayerInput);

	virtual bool IsInteracting() const override;

protected:

	// CanFire helpers
	bool CanFireOrMelee(bool isMelee) const;
	bool CanFire_AI() const;
	bool CanFire_DedicatedClient() const;
	bool CanFire_Player(bool isMelee) const;

	void ResetInteractor();

	void UpdateEyeOffsets(CWeapon* pCurrentWeapon, float frameTime);

	virtual IActorMovementController * CreateMovementController() override;
	void SetIK( const SActorFrameMovementParams& );

	virtual void UpdatePlayerPlugins(const float dt);

	void LeaveAllPlayerPlugins();
	void UpdateSilentFeetSoundAdjustment();

	void RegisterOnHUD( void );

	bool DoSTAPAiming() const;
	void UpdateFPAiming();
	void UpdateFPIKTorso(float fFrameTime, IItem * pCurrentItem, const Vec3& cameraPosition);
	void UpdatePartialCameraAnim(float timeStep);
	
	const Vec3  GetFPCameraOffset() const;
	virtual void OnChangeTeam();
	void OnLocalPlayerChangeTeam();
	void UpdatePlayerCinematicStatus(uint8 oldFlags, uint8 newFlags);
	void ResetCinematicStatus(uint8 oldFlags);

	void CheckSendXPChanges();

	void UpdateSpectator(float frameTime);
	void PostRagdollPhysicalized(bool fallAndPlay);

	void OnBeginCutScene();
	void OnEndCutScene();

public:

	void BlendPartialCameraAnim(float target, float blendTime = 0.2f);
	const Vec3  GetFPCameraPosition(bool worldSpace) const;

	// Made public, so that a plug-in class can tell the player to enter or leave it
	void EnterPlayerPlugin(CPlayerPlugin * pluginClass);
	void LeavePlayerPlugin(CPlayerPlugin * pluginClass);

	struct EntityParams
	{
		
		EntityParams()
			: entityId(0)
		{};
		
		EntityParams(EntityId entId)
			: entityId(entId)
		{}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityId", entityId, 'eid');
		}

		EntityId entityId;
	};

	struct TwoEntityParams
	{

		TwoEntityParams()
			: entityA_Id(0)
			, entityB_Id(0)
		{};

		TwoEntityParams(EntityId entIdA, EntityId entIdB)
			: entityA_Id(entIdA)
			, entityB_Id(entIdB)
		{}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("entityA_Id", entityA_Id, 'eid');
			ser.Value("entityB_Id", entityB_Id, 'eid');
		}

		EntityId entityA_Id;
		EntityId entityB_Id;
	};

	struct SStealthKillRequestParams
	{
		SStealthKillRequestParams() {}

		SStealthKillRequestParams(EntityId _victimId,	uint _animIndex) 
			: victimId(_victimId)
			, animIndex(_animIndex)
			{}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("stealthKillTarget", victimId, 'eid');
			
#ifndef _RELEASE
			if(ser.IsWriting() && (animIndex > 3))
			{
				CryFatalError("Index used for stealth kill too high. Value %d, max 3 Change the compression policy of sk_animIdx to use more bits", animIndex);
			}
#endif

			ser.Value("sk_animIdx", animIndex, 'ui2');
		}

		EntityId victimId;
		uint animIndex;
	};

	struct SRequestUseLadderParams
	{
		SRequestUseLadderParams() : ladderId(0), initialHeightFrac(0) {}
		SRequestUseLadderParams(EntityId id, float initialHeightFrac) : ladderId(id), initialHeightFrac(initialHeightFrac) {}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("ladderId", ladderId, 'eid');
			ser.Value("initialHeightFrac", initialHeightFrac, 'unit');
		}

		EntityId ladderId;
		float initialHeightFrac;
	};

	struct SRequestLeaveLadderParams
	{
		SRequestLeaveLadderParams() : leaveLocation(eLLL_Drop) {}
		SRequestLeaveLadderParams(ELadderLeaveLocation leaveLocation) : leaveLocation(leaveLocation) {}

		void SerializeWith(TSerialize ser)
		{
			ser.EnumValue("leaveLocation", leaveLocation, eLLL_First, eLLL_Count);
		}

		ELadderLeaveLocation leaveLocation;
	};

	struct SPlayerMeleeImpulseParams
	{
		SPlayerMeleeImpulseParams() : dir(ZERO), strength(0.f) {};
		SPlayerMeleeImpulseParams(Vec3 impulseDir, float impulseStrength) : dir(impulseDir), strength(impulseStrength) {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("dir", dir, 'dir1');
			ser.Value("strength", strength, 'iii');
		}

		Vec3 dir;
		float strength;
	};

	struct SIntStatParams
	{
		SIntStatParams() : m_stat(-1) {}
		SIntStatParams(int stat) : m_stat(stat) {}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("stat", m_stat, 'mat');
		}

		int m_stat;
	};

	DECLARE_SERVER_RMI_NOATTACH(SvOnXPChanged, SXPEvents, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH(ClDelayedDetonation, EntityParams, eNRT_ReliableUnordered);
	
	DECLARE_SERVER_RMI_INDEPENDENT(SvRequestMicrowaveBeam, SMicrowaveBeamParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_INDEPENDENT(ClDeployMicrowaveBeam, SMicrowaveBeamParams, eNRT_ReliableUnordered);

	void RequestMicrowaveBeam(const SMicrowaveBeamParams& params);
	void DeployMicrowaveBeam(const SMicrowaveBeamParams& params);

	DECLARE_SERVER_RMI_NOATTACH(SvRequestStealthKill, SStealthKillRequestParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClAbortStealthKill, TwoEntityParams, eNRT_ReliableOrdered);

	DECLARE_CLIENT_RMI_NOATTACH(ClApplyMeleeImpulse, SPlayerMeleeImpulseParams, eNRT_ReliableUnordered);

	void ApplyMeleeImpulse(const Vec3& impulseDirection, float impulseStrength);

	DECLARE_CLIENT_RMI_NOATTACH(ClIncrementIntStat, SIntStatParams, eNRT_ReliableUnordered);

	DECLARE_SERVER_RMI_NOATTACH(SvRequestUseLadder, SRequestUseLadderParams, eNRT_ReliableOrdered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestLeaveFromLadder, SRequestLeaveLadderParams, eNRT_ReliableOrdered);
	DECLARE_CLIENT_RMI_NOATTACH(ClLeaveFromLadder, SRequestLeaveLadderParams, eNRT_ReliableOrdered);

#if ENABLE_RMI_BENCHMARK

	DECLARE_SERVER_RMI_INDEPENDENT( SvBenchmarkPing, SRMIBenchmarkParams, eNRT_ReliableUnordered );
	DECLARE_CLIENT_RMI_INDEPENDENT( ClBenchmarkPong, SRMIBenchmarkParams, eNRT_ReliableUnordered );
	DECLARE_SERVER_RMI_INDEPENDENT( SvBenchmarkPang, SRMIBenchmarkParams, eNRT_ReliableUnordered );
	DECLARE_CLIENT_RMI_INDEPENDENT( ClBenchmarkPeng, SRMIBenchmarkParams, eNRT_ReliableUnordered );

	static void RMIBenchmarkCallback( ERMIBenchmarkLogPoint point0, ERMIBenchmarkLogPoint point1, int64 delay, void* pUserData );

#endif
	//set/get actor status
	virtual void SetStats(SmartScriptTable &rTable) override;
	virtual void UpdateStats(float frameTime);
	void UpdateSwimStats(float frameTime, const Matrix34& EntityMtx);
	void UpdateBreathing(float frameTime);
	void UpdateStumble(float deltaTime);
	void UpdateFrameMovementModifiersAndWeaponStats(CWeapon* pWeapon, float currentTime);
		
	virtual void SetParamsFromLua(SmartScriptTable &rTable) override;

	virtual float CalculatePseudoSpeed(bool wantSprint, float speedOverride = -1.0f) const;

	// Accessed via function to allow game based modifiers to stance speed without multiplying the number of stances.
	virtual float GetStanceMaxSpeed(EStance stance) const;

	virtual void ToggleThirdPerson() override;
	void SetThirdPerson(bool thirdPersonEnabled, bool force = false);

	virtual int  IsGod() override;
	
	void RestartMannequin();
	virtual void Revive( EReasonForRevive reasonForRevive = kRFR_Spawn ) override;
	virtual void Kill() override;
	virtual void Reset(bool toGame) override;

	virtual void RequestFacialExpression(const char* pExpressionName /* = NULL */, f32* sequenceLength = NULL) override;

	//stances
	Vec3 GetStanceViewOffset(EStance stance, const float *pLeanAmt=NULL, bool withY = false, const bool useWhileLeanedOffsets = false) const;
	virtual void SetStance(EStance stance) override;
	virtual bool IsThirdPerson() const override;
	virtual void OnStanceChanged(EStance newStance, EStance oldStance) override;
	void SetStanceTag(EStance stance, CTagState& tagState);

	//reset function clearing state and animations for teleported actor
	virtual void OnTeleported() override;

	virtual void ResetAnimationState() override;

	virtual void AddHeatPulse(const float intensity, const float time) override;
	
	float GetSprintStaminaLevel() const;

	//IPlayerProfileListener
	virtual void SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason) override;
	virtual void LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason) override;
	//~IPlayerProfileListener

	virtual void OnReturnedToPool() override;
	virtual void OnAIProxyEnabled(bool enabled) override;

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	virtual void SetFlyMode(uint8 flyMode) override;
	virtual uint8 GetFlyMode() const override { return m_stats.flyMode; }
#else
	virtual uint8 GetFlyMode() const override { return 0; }
#endif

	virtual void SetSpectatorState(uint8 state) override;
	virtual EActorSpectatorState GetSpectatorState() const override { return (EActorSpectatorState)m_stats.spectatorInfo.state; }

	virtual void SetSpectatorModeAndOtherEntId(const uint8 _mode, const EntityId _othEntId, bool isSpawning=false) override;

	virtual uint8 GetSpectatorMode() const override { return m_stats.spectatorInfo.mode; }
	virtual void SetSpectatorTarget(EntityId targetId) override;
	virtual EntityId GetSpectatorTarget() const override { assert(m_stats.spectatorInfo.mode==eASM_Follow||m_stats.spectatorInfo.mode==eASM_Killer); return m_stats.spectatorInfo.mode==eASM_Follow?m_stats.spectatorInfo.dataU.follow.spectatorTarget:m_stats.spectatorInfo.dataU.killer.spectatorTarget; }
	virtual float GetSpectatorOrbitYawSpeed() const override;
	virtual void SetSpectatorOrbitYawSpeed(float yawSpeed, bool singleFrame) override;
	virtual bool CanSpectatorOrbitYaw() const override;
	virtual float GetSpectatorOrbitPitchSpeed() const override;
	virtual void SetSpectatorOrbitPitchSpeed(float pitchSpeed, bool singleFrame) override;
	virtual bool CanSpectatorOrbitPitch() const override;
	virtual void SetSpectatorFixedLocation(EntityId locId) override;
	virtual EntityId GetSpectatorFixedLocation() const override { assert(m_stats.spectatorInfo.mode==eASM_Fixed); return m_stats.spectatorInfo.dataU.fixed.location; }

	CPlayerHealthGameEffect& GetPlayerHealthGameEffect() { return m_playerHealthEffect; }

	//Cloak material
	virtual void SetCloakLayer(bool set, eFadeRules config = eAllowFades) override;

	void MoveToSpectatorTargetPosition();

	ILINE void SetRecentKiller(EntityId killerEid, int damageType) { m_stats.recentKiller = killerEid; m_stats.killedByDamageType = damageType; }

	EntityId GetLastAttacker() { return m_stats.lastAttacker; }

	virtual void SelectNextItem(int direction, bool keepHistory, int category) override;
	virtual void HolsterItem(bool holster, bool playSelect = true, float selectSpeedBias = 1.0f, bool hideLeftHandObject = true) override;
	void HolsterItem_NoNetwork(bool holster, bool playSelect = true, float selectSpeedBias = 1.0f, bool hideLeftHandObject = true);
	virtual void SelectLastItem(bool keepHistory, bool forceNext = false) override;
	virtual void SelectItemByName(const char *name, bool keepHistory, bool forceFastSelect=false) override;
	virtual void SelectItem(EntityId itemId, bool keepHistory, bool forceSelect) override;
	virtual bool ScheduleItemSwitch(EntityId itemId, bool keepHistory, int category = 0, bool forceFastSelect=false) override;
	virtual void NotifyCurrentItemChanged(IItem* newItem) override;

	virtual void RagDollize( bool fallAndPlay ) override;
	void UnRagdollize();
	virtual void HandleEvent( const SGameObjectEvent& event ) override;

	virtual void PostUpdate(float frameTime) override;

	virtual void AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event) override;
	
	virtual void SetViewRotation( const Quat &rotation ) override;
	void SetViewRotationAndKeepBaseOrientation( const Quat &rotation );
	void SetForceLookAt(const Vec3& lookAtDirection, const bool bForcedLookAtBlendingEnabled = true);
	virtual Quat GetViewRotation() const override;
	void AddViewAngles(const Ang3 &angles);
	virtual void EnableTimeDemo( bool bTimeDemo ) override;

	virtual void AddViewAngleOffsetForFrame(const Ang3 &offset) override;

	virtual bool SetAspectProfile(EEntityAspects aspect, uint8 profile ) override;

	virtual void FullSerialize( TSerialize ser ) override;
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) override;
	virtual void PostSerialize() override;
	virtual void  SerializeLevelToLevel( TSerialize &ser ) override;

	void SetInNetLimbo(bool yesNo) { m_inNetLimbo = yesNo; }
	bool InNetLimbo() const { return m_inNetLimbo; }

	//set/get actor params
	virtual void SetHealth( float health ) override;
	virtual SPlayerStats *GetActorStats() override { return &m_stats; }
	virtual const SPlayerStats *GetActorStats() const override { return &m_stats; }
	virtual void PostPhysicalize() override;
	virtual void CameraShake(float angle,float shift,float duration,float frequency,Vec3 pos,int ID,const char* source="") override;
	virtual bool CreateCodeEvent(SmartScriptTable &rTable) override;
	ILINE virtual Matrix34 GetViewMatrix() const { return m_clientViewMatrix; }
	virtual void AddAngularImpulse(const Ang3 &angular,float deceleration, float duration) override;
	virtual void SetAngles(const Ang3 &angles) override;
	virtual Ang3 GetAngles() override;
	virtual void PlayAction(const char *action,const char *extension, bool looping=false) override;

	void			CaughtInStealthKill(EntityId killerId);
	void			StoreDelayedKillingHitInfo(HitInfo delayedHit);
	HitInfo&	GetDelayedKillingHitInfo();
	bool			ShouldFilterOutHit( const HitInfo& hit );
	bool			ShouldFilterOutExplosion( const HitInfo& hitInfo );

	virtual bool	AllowLandingBob() override { return true; }

	void OnStartRecordingPlayback();
	void OnStopRecordingPlayback();
	void OnRecordingPlaybackBulletTime(bool bBulletTimeActive);

	bool IsPlayingSmartObjectAction() const;
	bool CanFall() const;
	virtual void KnockDown(float backwardsImpulse) override;

  virtual void SetLookAtTargetId(EntityId targetId, float interpolationTime=1.f) override;
  virtual void SetForceLookAtTargetId(EntityId targetId, float interpolationTime=1.f) override;

	virtual void DamageInfo(EntityId shooterID, EntityId weaponID, IEntityClass *pProjectileClass, float damage, int damageType, const Vec3 hitDirection) override;

	const Quat& GetBaseQuat() const { return m_pPlayerRotation->GetBaseQuat(); }
	const Quat& GetViewQuat() const { return m_pPlayerRotation->GetViewQuat(); }
	const Quat& GetViewQuatFinal() const { return m_pPlayerRotation->GetViewQuatFinal(); }
	virtual void ResetAnimations();
	IPlayerInput* GetPlayerInput() const {return m_pPlayerInput.get();}
	ILINE CPlayerCamera	* GetPlayerCamera() const { return m_playerCamera; }

	virtual void SwitchDemoModeSpectator(bool activate) override;
	bool IsTimeDemo() const { return m_timedemo; }

	void RegisterPlayerEventListener	(IPlayerEventListener *pPlayerEventListener);
	void UnregisterPlayerEventListener(IPlayerEventListener *pPlayerEventListener);
	void RegisterPlayerUpdateListener(IPlayerUpdateListener *pListener);
	void UnregisterPlayerUpdateListener(IPlayerUpdateListener *pListener);

	bool IsOnGround() const;

	bool CanTurnBody() const 
	{
		return m_bCanTurnBody;
	}
	void SetCanTurnBody(const bool canTurn)
	{
		m_bCanTurnBody = canTurn;
	}

	void SetAimLimit(const float aimLimit)
	{
		m_aimLimit = aimLimit;
		m_bHasAimLimit = true;
	}
	void ClearAimLimit()
	{
		m_bHasAimLimit = false;
	}
	bool GetAimLimit(float &aimLimit) const
	{
		aimLimit = m_aimLimit;
		return m_bHasAimLimit;
	}

	// Sliding
	bool IsSliding() const;
	bool IsExitingSlide() const;
	bool IsInAir() const;

	bool CanDoSlideKick() const;

	bool IsPlayerOkToAction() const;
	bool IsOnLedge() const;
	bool HasBeenOffLedgeSince( float fTimeSinceOnLedge ) const;

	bool CanSwitchItems() const;
	bool HasHeavyWeaponEquipped() const;
	//~Crysis2 

	virtual bool UseItem(EntityId itemId) override;
	virtual bool PickUpItem(EntityId itemId, bool sound, bool select) override;
	virtual bool DropItem(EntityId itemId, float impulseScale=1.0f, bool slectNext=true, bool byDeath=false) override;
	virtual void NetKill(const KillParams &killParams) override;

	ILINE const Vec3& GetEyeOffset() const { return m_eyeOffset; }
	ILINE const Vec3& GetWeaponOffset() const { return m_weaponOffset; }
	ILINE const Vec3& GetThirdPersonAimTarget() const { return m_thirdPersonAimTarget; }

	void PlaySound(EPlayerSounds sound, bool play = true, const char* paramName = NULL, float paramValue = 0.0f, const char* paramName2 = NULL, float paramValue2 = 0.0f, float volume = 1.0f, bool playOnRemote = false);

	ILINE SCharacterMoveRequest& GetMoveRequest() { return m_request; }

	void ResetScreenFX();
	void ResetFPView();

	void ForceRefreshStanceAndEyeOffsetNow();

	float GetLastDamageSeconds() const { return m_lastTimeDamaged.GetSeconds(); }
	float GetTimeEnteredLowHealth() const;

	const EntityId GetLastFlashbangShooterId() const { return m_lastFlashbangShooterId; };
	const float GetLastFlashbangTime() const { return m_lastFlashbangTime; }

	const float GetLastZoomedTime() const { return m_lastZoomedTime; }

	virtual EntityId GetCurrentTargetEntityId() const override
	{
		return m_currentlyTargettingPlugin.GetCurrentTargetEntityId();
	}

	virtual const float GetCurrentTargetTime() const
	{
		return m_currentlyTargettingPlugin.GetCurrentTargetTime();
	}

	const float GetCloakBlendSpeedScale() override;

	void SetLastReloadTime(float reloadTime) { m_lastReloadTime = reloadTime; };
	float GetLastReloadTime() const { return m_lastReloadTime; };

	void AddXPBonusMultiplier(int increment) { m_xpBonusMultiplier += increment; }
	int GetXPBonusModifiedXP(int baseXP);

	CPlayerPluginEventDistributor* GetPlayerPluginEventDistributor() const { return m_pPlayerPluginEventDistributor; }

	//Weapon movement modifiers
	ILINE float GetWeaponMovementFactor() const { return m_frameMovementModifiers.GetWeaponSpeedModifier(); };
	ILINE float GetWeaponRotationFactor() const { return m_frameMovementModifiers.GetWeaponRotationModifier(); };
	ILINE float GetTotalSpeedMultiplier() const { return m_frameMovementModifiers.GetTotalSpeedModifier(); };

	void RegisterKill(IActor *pKilledActor, int hit_type);

	void NetSetInStealthKill(bool inKill, EntityId targetId, uint8 animIndex);
	void StopStealthKillTargetMovement(EntityId playerId);
	void OnPickedUpPickableAmmo( IEntityClass* pAmmoType, int count );
	
	struct SStagingParams
	{
		SStagingParams() : 
			bActive(false), bLocked(false), vLimitDir(ZERO), vLimitRangeH(0.0f), vLimitRangeV(0.0f), stance(STANCE_NULL)
		{
		}

		bool  bActive;
		bool  bLocked;
		Vec3  vLimitDir;
		float vLimitRangeH;
		float vLimitRangeV;
		EStance stance;
		void Serialize(TSerialize ser)
		{
			assert( ser.GetSerializationTarget() != eST_Network );
			ser.BeginGroup("SStagingParams");
			ser.Value("bActive", bActive);
			ser.Value("bLocked", bLocked);
			ser.Value("vLimitDir", vLimitDir);
			ser.Value("vLimitRangeH", vLimitRangeH);
			ser.Value("vLimitRangeV", vLimitRangeV);
			ser.EnumValue("stance", stance, STANCE_NULL, STANCE_LAST);
			ser.EndGroup();
		}
	};

	virtual void SetViewLimits(Vec3 dir, float rangeH, float rangeV) override;
	void StagePlayer(bool bStage, SStagingParams* pStagingParams = 0); 

	void NotifyObjectGrabbed(bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded = false, float fThrowSpeed = 0.f);

	bool HasActiveNavPath() const;

	bool HasShadowCharacter() const;
	int  GetShadowCharacterSlot() const;
	ICharacterInstance *GetShadowCharacter() const;

	void UpdateVisibility();

	void RefreshVisibilityState();

	CPlayerEntityInteraction& GetPlayerEntityInteration();
	ILINE CPlayerModifiableValues& GetModifiableValues() { return m_modifiableValues; }
	ILINE const CPlayerModifiableValues& GetModifiableValues() const { return m_modifiableValues; }

	const SFollowCameraSettings& GetCurrentFollowCameraSettings() const;
	virtual void ChangeCurrentFollowCameraSettings(bool increment) override;
	bool SetCurrentFollowCameraSettings(uint32 crcName);

	void StartFlashbangEffects(const float time, const EntityId shooterId);
	void StopFlashbangEffects();
	void UpdateFlashbangEffect(float frameTime);
	
	void StartTinnitus();
	void UpdateTinnitus(float frameTime);
	void StopTinnitus();

	void AttemptStealthKill(EntityId enemyEntityId);
	
	void FailedStealthKill();
	void EnterLargeObjectInteraction(EntityId objectEntityId, const bool bSkipKickAnim = false) { m_largeObjectInteraction.Enter(objectEntityId, bSkipKickAnim);}
	void RequestEnterPickAndThrow( EntityId entityPicked );
	void EnterPickAndThrow( EntityId entityPicked, bool selectImmediately = true, bool forceSelect = false );
	void ExitPickAndThrow(bool forceInstantDrop = false);
	bool IsInPickAndThrowMode() const { return m_stats.isInPickAndThrowMode; }
	EntityId GetPickAndThrowEntity() const { return m_stats.pickAndThrowEntity; }
	EntityId GetPrevPickAndThrowEntity() const { return m_stats.prevPickAndThrowEntity; }

	CHitDeathReactionsPtr GetHitDeathReactions() { return m_pHitDeathReactions; } 
	CHitDeathReactionsConstPtr GetHitDeathReactions() const { return m_pHitDeathReactions; } 
	void InitHitDeathReactions();

	void GetPlayerProgressions(int* outXp, int* outRank, int* outDefault, int* outStealth, int* outArmour, int* outReincarnations );

	void TriggerMeleeReaction();

	void SpawnCorpse();

	ILINE const CSpectacularKill& GetSpectacularKill() const { return m_spectacularKill; }
	ILINE CSpectacularKill& GetSpectacularKill() { return m_spectacularKill; }
	ILINE const CStealthKill& GetStealthKill() const { return m_stealthKill; }
	ILINE const CLargeObjectInteraction& GetLargeObjectInteraction() const { return m_largeObjectInteraction; }

	ILINE CPickAndThrowProxyConstPtr GetPickAndThrowProxy() const { return m_pPickAndThrowProxy; }
	ILINE CPickAndThrowProxyPtr GetPickAndThrowProxy() { return m_pPickAndThrowProxy; }
	void ReloadPickAndThrowProxy();

	void SetRagdollPhysicsParams(IPhysicalEntity * pPhysEnt, bool fallAndPlay);

	void OnCollision(EventPhysCollision *physCollision);

	const QuatT	&GetLastSTAPCameraDelta() const;
	
	void SetClientSoundmood(EClientSoundmoods soundmood);
	EClientSoundmoods FindClientSoundmoodBestFit() const;

	void LogXPChangeToTelemetry(int inXPDelta, EXPReason inReason);

	void PostProcessAnimation(ICharacterInstance *pCharacter);

	void StealthKillInterrupted(EntityId interruptorId);
	void SetStealthKilledBy(EntityId shooterId)		{ m_stealthKilledById = shooterId; }
	EntityId GetStealthKilledBy() const						{ return m_stealthKilledById; }
	SPlayerRotationParams::EAimType GetCurrentAimType() const;
	const SPlayerRotationParams &GetPlayerRotationParams() const { return m_playerRotationParams; }

	void OnMeleeHit(const SMeleeHitParams &params);

	bool IsInFreeFallDeath() const;
	void CreateInputClass(bool client);

	void TriggerLoadoutGroupChange( CEquipmentLoadout::EEquipmentPackageGroup group, bool forOneLifeOnly );
	ILINE void SetLastTimeInLedge( float lastLedgeTime ) { m_lastLedgeTime = lastLedgeTime; }
	void SetTagByCRC( uint32 tagCRC, bool enable );

	virtual void BecomeRemotePlayer() override;

	void DeselectWeapon();

	// AI Specific
	CAIAnimationComponent* GetAIAnimationComponent() { return m_pAIAnimationComponent.get(); }
	const CAIAnimationComponent* GetAIAnimationComponent() const { return m_pAIAnimationComponent.get(); }

	virtual bool ShouldMuteWeaponSoundStimulus() const override;

	bool WasFriendlyWhenKilled(EntityId entityId) const;
	int GetTeamWhenKilled() const { return m_teamWhenKilled; }

	ILINE uint8 GetMPModelIndex() const {return m_mpModelIndex;}

	void  OnUseLadder(EntityId ladderId, float heightFrac);
	void  OnLeaveLadder(ELadderLeaveLocation leaveLocation);
	void  OnLadderPositionUpdated(float ladderFrac);
	void  InterpLadderPosition(float frameTime);

	void RegisterVehicleClient(CVehicleClient* vehicleClient) { m_pVehicleClient = vehicleClient; }

private:

	void NetSerialize_Health( TSerialize ser, bool bReading );
	void NetSerialize_CurrentItem( TSerialize ser, bool bReading );
	void NetSerialize_StealthKill( TSerialize ser, bool bReading );
	void NetSerialize_SnapTarget( TSerialize ser, bool bReading );
	void NetSerialize_InteractiveObject( TSerialize ser, bool bReading );
	void NetSerialize_InputClient( TSerialize ser, bool bReading );
	void NetSerialize_InputClient_Aug( TSerialize ser, bool bReading );
	void NetSerialize_Spectator( TSerialize ser, bool bReading );
	void NetSerialize_FlashBang( TSerialize ser, bool bReading );
	void NetSerialize_Jumping( TSerialize ser, bool bReading );
	void NetSerialize_LedgeGrab( TSerialize ser, bool bReading );

	void NetSerialize_Melee( TSerialize ser, bool bReading );
	void NetSerialize_Ladder( TSerialize ser, bool bReading );

	void DoMeleeMaterialEffect(const SMeleeHitParams& rHitParams);

	void CreateInputClass(IPlayerInput::EInputType inputType);

	bool ShouldUsePhysicsMovement();
	void InformHealthHasBeenReduced();
	void CommitKnockDown();
	void OnRagdollize();

	void OnLocalSpectatorStateSerialize(CActor::EActorSpectatorState newState, CActor::EActorSpectatorState curState);

	CAudioSignalPlayer m_flashbangSignal;

#if ENABLE_PLAYER_HEALTH_REDUCTION_POPUPS
	float	m_healthAtStartOfUpdate;
#endif

#ifdef STATE_DEBUG
	static EntityId s_StateMachineDebugEntityID;
#endif 

	typedef std::list<IPlayerEventListener *> TPlayerEventListeners;
	TPlayerEventListeners m_playerEventListeners;

	typedef std::list<IPlayerUpdateListener*> TPlayerUpdateListeners;
	TPlayerUpdateListeners m_playerUpdateListeners;

	//Health update
	float				m_timeOfLastHealthSync;
	CTimeValue  m_lastTimeDamaged;

	void UpdateHealthRegeneration(float fHealth, float frameTime);
	float GetRegenerationAmount(float frameTime);

	// Any MP specific functionality we want to handle in revive before we call down to CActor::Revive()
	void HandleMPPreRevive();

	virtual void PrepareLuaCache() override;

	// void Cover and lean
	void UpdateCrouchAndLeanReferencePoints();

	virtual void ReadDataFromXML(bool isReloading = false) override;

	virtual void InitLocalPlayer() override;

	static void StrikeTargetPosition(const int currentPoint, const int numberOfPoints, Vec3& targetPos);

	void UpdateThirdPersonState();

	void SetUpInventorySlotsAndCategories();

	// Network stuff
	void  HasJumped(const Vec3 &jumpVel);
	uint8 GetJumpCounter() const;
	void  SetJumpCounter(uint8 counter);
	void  HasClimbedLedge(const uint16 ledgeID, bool comingFromOnGround, bool comingFromSprint);
	uint8 GetLedgeCounter() const;
	void  SetLedgeCounter(uint8 counter);

	void SetupAimIKProperties();

	virtual void InitGameParams(const SActorGameParams &gameParams, const bool reloadCharacterSounds) override;

  bool  MountedGunControllerEnabled() const override { return m_mountedGunControllerEnabled; }
  void  MountedGunControllerEnabled(bool val) override { m_mountedGunControllerEnabled = val; }

	// Support for both AI and players
	void SelectMovementHierarchy();
	ILINE bool IsAIControlled() const { return m_isAIControlled; }; // Note this is NOT necessarily !IsPlayer().

	// AI Specific
	void UpdateAIAnimationState(const SActorFrameMovementParams &frameMovementParams, CWeapon* pWeapon, ICharacterInstance* pICharInst, IActionController* pActionController, IMannequin& mannequinSys);
	void UpdateAITagsFromAG(CTagState* pTagState, const struct SMannequinAIStateParams* pParams);

protected:
	//set the character's model names based on race and team
	void SetMultiplayerModelName();

	virtual float GetBaseHeat() const;
	virtual void SetModelIndex(uint8 modelIndex) override { m_mpModelIndex = modelIndex; }

	CPlayerRotation *m_pPlayerRotation;

	EntityEffects::CHeatController m_heatController;

	CPlayerCamera	* m_playerCamera;

	CPlayerPluginEventDistributor* m_pPlayerPluginEventDistributor;

	bool IsSoundPlaying(EPlayerSounds sound) const { return m_sounds[sound].audioSignalPlayer.IsPlaying( GetEntity()->GetId() ); }

	CSprintStamina *m_pSprintStamina;
	
	Matrix34 m_clientViewMatrix;

	Vec3		m_eyeOffset;	// View system - used to interpolate to goal eye offset
												//the offset from the entity origin to eyes, its not the real offset vector but its referenced to player view direction.

	Vec3		m_weaponOffset;

	Vec3		m_thirdPersonAimTarget;

	CMasterFader* m_pMasterFader;
	float    m_closeToWallFadeoutAmount;

	// updated by PlayerMovement for tracking time based acceleration
	Vec3		m_lastRequestedVelocity;
	Vec3    m_lastKnownPosition;
	Vec3	m_lastSyncedWorldPosition;

	Vec3		m_forcedLookDir;
	EntityId		m_forcedLookObjectId;
	
	EntityId		m_lastFlashbangShooterId;
	float				m_lastFlashbangTime;

	float				m_lastZoomedTime;

	SPlayerStats		m_stats;
	
	std::unique_ptr<IPlayerInput> m_pPlayerInput;

	// for foot/leg ik
	float m_fLastEffectFootStepTime;

	// compatibility with old code: which actions are set
	int m_actions;

	bool m_isAIControlled;
	bool m_viewBlending;
	bool m_jumpButtonIsPressed;
	bool m_timedemo;
	bool m_isInWater;
	bool m_isHeadUnderWater;
	float m_fOxygenLevel;

	// probably temporary, feel free to figure out better place
	float m_fDeathTime;

	bool		m_sufferingHighLatency;
	
	CVehicleClient* m_pVehicleClient;
	Vec3 m_vehicleViewDir;

	struct SSound
	{
		CAudioSignalPlayer audioSignalPlayer;
		bool isRepeated;
	
		SSound() : isRepeated(false)	{}
	};
	
	SSound m_sounds[ESound_Player_Last];
	int m_footstepCounter;

	void AddClientSoundmood(EClientSoundmoods soundmood);
	void RemoveClientSoundmood(EClientSoundmoods soundmood);


	IInteractor*         m_pInteractor;
	IEntityAudioComponent* m_pIEntityAudioComponent;
	CryAudio::ControlId m_waterEnter;
	CryAudio::ControlId m_waterExit;
	CryAudio::ControlId m_waterDiveIn;
	CryAudio::ControlId m_waterDiveOut;
	CryAudio::ControlId m_waterInOutSpeed;

	SStagingParams m_stagingParams;

	static const int									k_maxActivePlayerPlugIns = 6;

	CStealthKill											m_stealthKill;
	HitInfo														m_stealthKillDelayedHit;
	CSpectacularKill									m_spectacularKill;
	CLargeObjectInteraction						m_largeObjectInteraction;
	CPickAndThrowProxyPtr							m_pPickAndThrowProxy;
	int																m_numActivePlayerPlugins;
	int 															m_pendingLoadoutGroup;

	CPlayerPlugin*																m_activePlayerPlugins[k_maxActivePlayerPlugIns];
	CPlayerPlugin_CurrentlyTargetting							m_currentlyTargettingPlugin;
	CPlayerPlugin_Interaction*										m_pLocalPlayerInteractionPlugin;
	CPlayerPlugin_InteractiveEntityMonitor*				m_pInteractiveEntityMonitorPlugin;

	CPlayerModifiableValues                       m_modifiableValues;
	
	SCharacterMoveRequest		m_request;
	CMountedGunController		m_mountedGunController;
	SPlayerRotationParams m_playerRotationParams;
	CPlayerStateSwim_WaterTestProxy  m_playerStateSwim_WaterTestProxy;

	SFrameMovementModifiers m_frameMovementModifiers;

	float										m_ragdollTime;

	CHitRecoilGameEffect m_hitRecoilGameEffect;
	CPlayerHealthGameEffect m_playerHealthEffect;
	
	float m_thermalVisionBaseHeat;

	// Network
	bool  m_netFlashBangStun;
  bool  m_mountedGunControllerEnabled;
	uint8 m_jumpCounter;
	Vec3	m_jumpVel;
	uint8 m_mpModelIndex;
	uint8	m_ledgeCounter;
	uint16 m_ledgeID;

	EntityId m_ladderId;
	ELadderLeaveLocation m_lastLadderLeaveLoc;
	float m_ladderHeightFrac;
	float m_ladderHeightFracInterped;

	typedef uint8 ELedgeFlags;
	enum
	{
		eLF_NONE						= (0),
		eLF_FROM_ON_GROUND	= BIT(0),
		eLF_FROM_SPRINTING	= BIT(1),
	};
	ELedgeFlags m_ledgeFlags;

	CItem::TempAGInputName m_lastSuffix;
	CItem::TempAGInputName m_lastPose;
	CItem::TempAGInputName m_lastItemClass;

	SNetPlayerProgression m_netPlayerProgression;
	SXPEvents							m_netXPEvents;
	CTimeValue						m_netXPSendTime;

	CHitDeathReactionsPtr m_pHitDeathReactions;

	SSerializedPlayerInput	m_recvPlayerInput;

	SDeferredFootstepImpulse m_deferredFootstepImpulse;

	CLocalPlayerComponent * m_pPlayerTypeComponent;

private:

	void StatsPopulateAngVelAndMass( const IPhysicalEntity& physEnt );
	void StatsPopulateMovingAndRestTime( float frameTime );

	const char* GetFootstepEffectName() const;

	float m_lastLedgeTime;

	EntityId			m_netCloseCombatSnapTargetId;

	EntityId	m_stealthKilledById;
	EntityId	m_carryObjId;
	bool			m_pickingUpCarryObject;
	IAttachment *m_pIAttachmentGrab;

	void UpdateReactionOverlay(float frameTime);
	void SetReactionOverlay(EReactionOverlay overlay);

	EReactionOverlay m_reactionOverlay;
	float						 m_reactionTimer;
	float						 m_reactionFactor;
	int							 m_reactionOverlayAnimIDs[EReaction_Total];

	struct SReactionAnim
	{
		const char *name;
		int flags;
		float blend;
		int animID;
	};
	static SReactionAnim  m_reactionAnims[EReaction_Total];

	uint8 m_meleeHitCounter;

	int m_xpBonusMultiplier;

	float m_timeFirstSpawned;
	float m_lastReloadTime;

#if ENABLE_RMI_BENCHMARK
	int64                   m_RMIBenchmarkLast;
	uint8                   m_RMIBenchmarkSeq;
#endif

	typedef std::map<int32, CryAudio::AuxObjectId> TJointToAudioProxyLookup;
	TJointToAudioProxyLookup		m_cJointAudioProxies;

public:

	void OnRayCastBottomLevelDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

	float GetTimeFirstSpawned() const { return m_timeFirstSpawned; }

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
	void DebugGraph_AddValue(const char* id, float value) const;

	ILINE CPlayerDebugMovement& GetMovementDebug() { return m_movementDebug; }
	CPlayerDebugMovement m_movementDebug;
#endif

	virtual void SetTurnAnimationParams(const float turnThresholdAngle, const float turnThresholdTime) override;

	QuatT m_lastCameraLocation;

	CAnimationProxyDualCharacter m_animationProxy;
	CAnimationProxyDualCharacterUpper m_animationProxyUpper;

	CIKTorsoAim_Helper	m_torsoAimIK;
	CLookAim_Helper	m_lookAim;
	CWeaponFPAiming		m_weaponFPAiming;
	SParams_WeaponFPAiming m_weaponParams;

	SAnimActionAIMovementSettings m_animActionAIMovementSettings;

	float m_logPingTimer;
	float m_deferredKnockDownImpulse;

	int m_teamWhenKilled;

	float m_aimLimit;
	bool m_fpCompleteBodyVisible;
	bool m_deferredKnockDownPending : 1;
	bool m_registeredOnHUD : 1;
	bool m_dropCorpseOnDeath : 1;
	bool m_hideOnDeath : 1;
	bool m_usingSpectatorPhysics : 1;
	bool m_inNetLimbo : 1;
	bool m_bCanTurnBody : 1;
	bool m_isControllingCamera : 1;
	bool m_bDontResetFXUntilNextSpawnRevive : 1;
	bool m_bMakeVisibleOnNextSpawn: 1; 
	bool m_bHasAimLimit : 1;
	bool m_bPlayIntro : 1;

	int8 m_lastCachedInteractionIndex;

	std::unique_ptr<CAIAnimationComponent> m_pAIAnimationComponent;
};

void SetupPlayerCharacterVisibility(IEntity* playerEntity, bool isThirdPerson, int shadowCharacterSlot = -1, bool forceDontRenderNearest = false);

#endif //__PLAYER_H__
