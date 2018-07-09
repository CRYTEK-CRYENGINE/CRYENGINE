// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
not really a "weapon": is just the hands grabbing objects (barrels etc) and throwing them
*************************************************************************/

#pragma once

#ifndef _PICKANDTHROWWEAPON_H_
#define _PICKANDTHROWWEAPON_H_

#include "Weapon.h"
#include "WeaponSharedParams.h"
#include "MeleeCollisionHelper.h"
#include "IPlayerEventListener.h"
#include "QuatTBlender.h"
#include "PickAndThrowUtilities.h"

class CPlayer;
class Agent;
class CEnvironmentalWeapon;
struct SAutoaimTarget; 

#if !defined(_RELEASE) && !defined(PERFORMANCE_BUILD)
#define PICKANDTHROWWEAPON_DEBUGINFO
#endif


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CPickAndThrowWeapon :	public CWeapon, public IMeleeCollisionHelperListener
{
	struct FinishSimpleMelee;
	struct FinishComplexMelee;
	struct DoSimpleMelee;
	struct DelayedMeleeImpulse;
	struct DelayedNPCRagdollThrow;
	struct SInfiniteViewDistEntity;
	
	struct HelperInfo
	{
		HelperInfo()
			: m_pSubObject( NULL )
			, m_slot( -1 )
			, m_grabTypeIdx( -1 )
		{

		}

		ILINE bool Valid() const { return (m_pSubObject != NULL); }

		IStatObj::SSubObject* m_pSubObject;
		int       m_slot;
		int       m_grabTypeIdx;
	};

public:

	CPickAndThrowWeapon();
	virtual ~CPickAndThrowWeapon();

	static const char* GetWeaponComponentType();
	virtual const char* GetType() const;
	virtual void Select(bool select);
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void UpdateFPView( float frameTime );
	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}
	virtual bool CanModify() const;
	virtual void OnSelected( bool selected );
	virtual bool ShouldPlaySelectAction() const;
	virtual void Update(SEntityUpdateContext& ctx, int val );
	virtual bool Init(IGameObject * pGameObject);	
	virtual bool UpdateAimAnims( SParams_WeaponFPAiming &aimAnimParams);
	virtual void UpdateTags(const class IActionController *pActionController, class CTagState &tagState, bool selected) const;
	virtual bool GetAnimGraphInputs(CItem::TempAGInputName &pose, CItem::TempAGInputName &suffix, CItem::TempAGInputName &itemClass);
	virtual void Reset();
	virtual float GetMeleeRange() const;
	
	virtual void SetViewMode(int mode);

	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual NetworkAspectType GetNetSerializeAspects();
	virtual void FullSerialize( TSerialize ser );
	virtual void PostSerialize();
	
	virtual void OnBeginCutScene();
	virtual void OnEndCutScene();

	virtual void NetStartMeleeAttack(bool boostedAttack, int8 attackIndex = -1);

	virtual bool ShouldAttachWhenSelected () {return false;}

	virtual void SerializeSpawnInfo( TSerialize ser );
	virtual ISerializableInfoPtr GetSpawnInfo();

	//IMeleeCollisionHelperListener
	virtual void OnSuccesfulHit(const ray_hit& hitResult);
	virtual void OnFailedHit(); 
	//~IMeleeCollisionHelperListener

	bool HasObjectGrabHelper(EntityId entityId) const;
	bool ShouldFilterOutHitOnOwner( const HitInfo& hit );
	bool ShouldFilterOutExplosionOnOwner( const HitInfo& hit );
	
	// Shields Player?
	bool ShieldsPlayer() { return m_objectId!=0 && GetGrabTypeParams().shieldsPlayer; }
	
	#ifdef PICKANDTHROWWEAPON_DEBUGINFO
	void DebugDraw();
	#endif

	// Simple melee (uses delayed raycast system)
	void OnDoSimpleMelee();
	void OnSimpleMeleeFinished();

	// Complex melee (uses physical object to detect hits)
	void OnDoComplexMelee(const bool bScheduleFinishComplexMelee);
	void OnComplexMeleeFinished();
	void OnDoChargedThrow(); 

	// Animation events
	bool OnAnimationEvent(const AnimEventInstance &event);

	ILINE bool IsIdling() const { return (m_state == eST_IDLE) && !IsNPC(); }

	EInteractionType GetHeldEntityInteractionType();

	void SetObjectId(EntityId objectId);		// Recording system

	void QuickAttach(); //Skip all the animations etc and put the item straight in the hands of the owner (MP)
	void DecideGrabType();

	void OnEndMannequinGrabAction();

	virtual void GetAngleLimits(EStance stance, float& minAngle, float& maxAngle);

	struct SvRequestThrowParams
	{
		SvRequestThrowParams() : m_RequestedThrowState(0){}
		SvRequestThrowParams(int requestedThrowState) : m_RequestedThrowState(requestedThrowState){}

		void SerializeWith(TSerialize ser)
		{
			ser.Value("requestedThrowState", m_RequestedThrowState, 'ui5');
		};

		int    m_RequestedThrowState;
	};

	struct NoParams
	{
		void SerializeWith(const TSerialize& ser) {};
	};

	DECLARE_SERVER_RMI_NOATTACH(SvRequestThrow, SvRequestThrowParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestChargedThrowPreRelease, NoParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestChargedThrowPostRelease, NoParams, eNRT_ReliableUnordered);

	// Item Events
	
	void Start_Drop( bool withAnimation = true );
	void OnObjectPhysicsPropertiesChanged();

	// Useful object helpers
	HelperInfo GlobalFindHelperObject( const EntityId objectId ) const;

	void MeleeFinished()
	{
		m_logicFlags |= ELogicFlag_MeleeActionFinished;
	}

	const SPickAndThrowParams::SGrabTypeParams& GetGrabTypeParams() const;
	virtual float GetMovementModifier() const;

	void OnHostMigration();

protected:
	virtual bool ShouldDoPostSerializeReset() const;

private:

	typedef uint8 TLogicFlags;
	enum eLogicFlag
	{
		ELogicFlag_None															= 0,
		ELogicFlag_OrientationCorrectionInitialised	= BIT(0),
		ELogicFlag_RequestedCombo 									= BIT(1),
		ELogicFlag_ReleaseChargedThrowTriggered 		= BIT(2),
		ELogicFlag_ProcessEndComplexMeleeState 			= BIT(3),
		ELogicFlag_MeleeActionFinished							= BIT(4),
	};	
		
	enum EAction
	{
		eAC_Grab,
		eAC_Throw,
		eAC_Drop
	};
	
	typedef CWeapon inherited;
 
	void SetUpCollisionPropertiesAndMassInfo( const bool pickedUp ); 
	void DrawNearObject( const bool drawNear );

	float CalculateThrowSpeed( const CPlayer& ownerPlayer, const SPickAndThrowParams::SThrowParams& throwParams ) const;
	void ThrowObject();
	
	void DropObject();
	bool CheckObjectIsStillThere();	
	void CheckObjectHiddenByLayerSwitch();
	IEntity* GetEntityObject();
	const IEntity* GetEntityObject() const;
	const CActor* GetNPCGrabbedActor() const;
	CActor* GetNPCGrabbedActor() { return const_cast<CActor*>(static_cast<const CPickAndThrowWeapon*>(this)->GetNPCGrabbedActor()); }
	void EnslaveTarget(bool enslave);


	CPlayer* GetNPCGrabbedPlayer();
	CPlayer* GetOwnerPlayer();
	const CPlayer* GetOwnerPlayer() const;
	const char* GetGrabHelperName() const;

	bool OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value);	
	bool OnActionMelee(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	bool OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionChargedThrow(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	void PerformMelee(int activationMode, const bool bForcePerformMelee = false);

	void CalcObjectHelperInverseMat( Matrix34& objectHelperInvMat );

	/// Object attachment
	//////////////////////////////////////////////////////////////////////////
	void AttachObject(const bool bIsRefresh = false);
	void DetachObject(const bool bIsRefresh = false);
	//////////////////////////////////////////////////////////////////////////

	IAttachment* GetOwnerAttachment(const int slotIndex = 0) const;

	const SPickAndThrowParams::SThrowParams& GetThrowParams()		const;
	
	void PrepareNPCToBeThrown();
	void PrepareNPCToBeDropped(bool hasNPCBeenThrown = false);
	bool IsNPC() const { return m_isNPC; }
	int	 DoSimpleMeleeHit( const ray_hit& hitResult, EntityId collidedEntityId);
	void DoSimpleMeleeImpulse( const ray_hit& hitResult, EntityId collidedEntityId, int hitTypeID );

	void SendMortalHitTo( CActor* pActor );
	void PlayCommunication( const char* pName, const char* pChannelName );
		
	void ResetInternal();
	bool IsGrabStillValid() const;
	bool IsBusy() const { return IsNPC() || CWeapon::IsBusy(); }
	bool CanSprint() const { return gEnv->bMultiplayer ? !IsBusy() : true; }

	void PlaySimpleGrabAction();
	void PlayActionLocal( const ItemString& actionName, bool _loop = false, uint32 _flags = eIPAF_Default, const float speedOverride = -1.0f);

	void PowerThrowPrepareSound(bool enable);
	
	// Called when the AI sees the object
	static void OnThrownObjectSeen(const Agent& agent, EntityId objectId, void *pVPickAndThrowWeapon);

	void UpdateInfiniteViewEntities ( float frameTime );
	void RestoreEntityViewDist( const SInfiniteViewDistEntity& info ) const;
	void ResetTrackingOfExternalEntities();
	void RestoreObjectPhysics( EntityId entityId, bool isNPC );
	
	// Orientation correction
	bool UpdateCorrectOrientation(const float dt); 
	void UpdateComplexMelee(const float dt); 
	void PerformCameraShake(const SPickAndThrowParams::SCameraShakeParams& camShake, const CPlayer* pPlayer);
	void UpdateComplexMeleeLerp(const CPlayer* pOwner); 
	void UpdateTargetViewLock(const Vec3& toTargetVec, const SPickAndThrowParams::SGrabTypeParams &grabParams);
	void InitViewPitchAdjust(QuatTBlender& blender, const float desiredPitchOffsetFromHorizontal, const float blendTime) const; 

	// State Info
	float DetermineGoingToPickStateDuration() const; 
	float DeterminePickingStateDuration()	  const; 
	float CalculateAnimTimeScaling(const ItemString& anim, const float desiredAnimDuration); 
	void  ScaleTimeValue(float& timeValueToScale, const float totalDuration, const float desiredDuration) const;

	void OnStartGrabAction( const bool isGrabbingNpc ); 
	void OnEndGrabAction();
	void OnNpcAttached();

	enum EState 
	{                       
		eST_UNKNOWN,
		eST_GOINGTOPICK,				// moving hands towards the object/npc, but it is not grabbed yet
		eST_PICKING,						// object/npc grabbed, hands bringing it to us
		eST_GRABBINGNPC,				 
		eST_IDLE,								// holding the object/npc
		eST_PUSHINGAWAY,				// hands moving ahead to throw the object/npc, but it is still stick to them
		eST_PUSHINGAWAY_POWER,
		eST_PUSHINGAWAY_WEAK,		// this is used automatically when the NPC is killed while staying grabbed. It is about the same than eST_PUSHINGAWAY,  but with different (usually much less) throwing speed
		eST_CHARGED_THROW_PRE_RELEASE,	  // this is used when starting a charged throw (e.g. priming/holding)
		eST_CHARGED_THROW_POST_RELEASE,		// this is used when ending a charged throw (e.g. priming/holding)
		eST_THROWING,						// object/npc is start moving freely away while the hands finish the throwing animation
		eST_STARTDROPPING,			// hands starts moving to drop the object/npc, but it still stick to them
		eST_DROPPING,						// object/npc starts dropping freely to the floor, while the hands finish the dropping animation
		eST_MELEE,							// use object to melee
		eST_COMPLEX_MELEE,			// using object to melee, but not using raycasts damage method.. instead using new complex physical object collision system
		eST_KILLING_GRABBEDNPC,  // killing the NPC that is being grabbed
		eST_KILLED_GRABBEDNPC,   // the NPC is killed, but we are in this state until we are ready to drop the corpse
		eST_LAST
	};

	///////////////////
	// Charged throw //

	enum EChargedThrowState 
	{ 
		eCTS_INACTIVE,
		eCTS_PRIMING,
		eCTS_CHARGING,
		eCTS_LAST
	};
	EChargedThrowState m_chargedThrowState;

	// Local
	void Start_Throw( EState throwState );
	void Start_Charged_Throw_PreRelease();

	// Requests
	void RequestStartThrow( EState throwState );
	void RequestChargedThrowPreRelease(); 

	// Logic 
	void UpdateChargedThrowPreRelease(const float dt); 
	Vec3 CalculateChargedThrowDir(const Matrix34& attackerTransform, const Vec3& throwPos) const;
	IEntity* CalculateBestChargedThrowAutoAimTarget(const Matrix34& attackerTransform) const;

	// Anim
	void UpdateReboundAnim( const float dt ); 
	void UpdateReboundOrStickLogic(const float dt); 

	// ~Charged throw //
	///////////////////

	// NOTE: Getting plenty of bools in here now.. perhaps time to flag up. 
	CMeleeCollisionHelper			m_collisionHelper;
	int												m_constraintId;
	int												m_constraintId2;
	float											m_timePassed; // used for different timmings, depending on state
	float											m_reboundOrStickTimer; // Used to control state exit when rebounding/sticking
	float                     m_animDuration;  // used for different animations, depending on state
	float											m_objectMass;	
	float											m_meleeRange;
	float											m_postMeleeActionCooldown;  // Cooldown after a primary melee attack before another can be commenced
	float											m_reboundEndMarker; 
	float											m_attackTimeSmoothRate; 
	float											m_attackTime;
	EState										m_state;
	QuatT											m_attachmentOldRelativeLoc;
	uint											m_framesToCheckConstraint;  // need to give the physics some time before can really start checking for the constraint
	Vec3											m_objectScale;
	EntityId									m_objectId;  // the object being pickedup
	EntityId									m_netInitObjectId;
	EntityId									m_currentMeleeAutoTargetId;	
	int												m_grabType; // index into m_weaponsharedparams->pPickAndThrowParams->grabTypesParams
	int												m_hitsTakenByNPC;
	int												m_objectHelperSlot;  // in which slot is the helper that is being used to grab the object. Not always is the actual active slot
	int												m_currentMeleeAnimationId; 
	bool											m_objectCollisionsLimited;  // need a separate flag because at some points the object is not attached to the player anymore, but the collisions are still not allowed
	bool											m_objectGrabbed;
	bool											m_bSerializeObjectGrabbed; //serialization helper
	bool											m_isNPC;
	EInteractionType					m_heldEntityInteractionType; // The interaction type (used for hud prompts) for the currently held weapon (typically 'throw')
	EState										m_RequestedThrowState;
	uint8											m_throwCounter; 

	static const uint8				kThrowCounterMax = 4;

	TLogicFlags  									m_logicFlags;
	QuatTBlender							m_orientationCorrector; // Used to correct orientation of player so grab anim for rooted objects is aligned. 

										 

	
	struct SCorrectOrientationParams
	{
		typedef uint8 TCorrectOrientationFlags;
		SCorrectOrientationParams()
		{
			Reset(); 
		}
		~SCorrectOrientationParams(){}

		void Reset()
		{
			m_logicFlags = ELogicFlag_SetTranslation | ELogicFlag_SetOrientation; 
		}

		enum ELogicFlag
		{
			ELogicFlag_SetTranslation 					= 1 << 0,
			ELogicFlag_SetOrientation 					= 1 << 1,
			ELogicFlag_AllowVerticalPitchCorrection		= 1 << 2,
		};

		TCorrectOrientationFlags m_logicFlags;
	};
	SCorrectOrientationParams m_correctOrientationParams; 

	// used to keep track of the entities that are kept at infinite view distance to avoid the disappearing of small objects when they are thrown away
	struct SInfiniteViewDistEntity
	{
		SInfiniteViewDistEntity() : valid(false), entityId(0), timeLimit(0), timeStopped(0), oldViewDistRatio(0) {}
		bool			valid;
		EntityId	entityId;
		float			timeLimit;   // absolute time to stop tracking the entity
		float			timeStopped; // how much time the entity has been stopped (we dont restore view of the entity until after it was stopped for some time)
		int				oldViewDistRatio;
	};
	
	std::vector<SInfiniteViewDistEntity>	m_infiniteViewEntities;
	
	
	// to keep track of flying/falling actor. need to do this because the no-colliding-with-the-player constraint is not removed from the actor until after a while after pickandthrow has been deactivated
	struct SFlyingActorInfo
	{
		SFlyingActorInfo()
			: valid( false )
			, entityId( 0 )
			, timeToEndTracking( 0.f )
		{}
	
		bool			valid;
		EntityId	entityId;
		float			timeToEndTracking;  // a countdown timer
	}						m_flyingActorInfo;
	
	
	PickAndThrow::CObstructionCheck		m_obstructionChecker;
	
	static const char* const	m_pHelperName;
};

#endif // _PICKANDTHROWWEAPON_H_
