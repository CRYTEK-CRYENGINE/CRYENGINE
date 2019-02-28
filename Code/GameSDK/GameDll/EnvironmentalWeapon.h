// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ENVIRONMENTALWEAPON_H
#define __ENVIRONMENTALWEAPON_H

// Enable for debug visuals 
#define ALLOW_SWEEP_DEBUGGING 0

// Forward decls
class  CActor;
class  CPlayer;
struct EventPhysCollision; 
struct HitInfo;

#include "GameRulesModules/IGameRulesRevivedListener.h"
#include "IntersectionAssistanceUnit.h"

struct IEnvironmentalWeaponEventListener
{
	// Called when Rip anim is started
	virtual void OnRipStart()  = 0;

	// Called at the point within the rip action that the actual 'detach/Unroot' occurs
	virtual void OnRipDetach() = 0;

	// Called at the end of the rip anim
	virtual void OnRipEnd()    = 0;

	virtual ~IEnvironmentalWeaponEventListener() {};
};

class CEnvironmentalWeapon : public CGameObjectExtensionHelper<CEnvironmentalWeapon, IGameObjectExtension, 5>, 	public IGameRulesRevivedListener
{
public:
	static const NetworkAspectType ASPECT_HEALTH_STATUS	= eEA_GameServerA;

	CEnvironmentalWeapon();
	~CEnvironmentalWeapon();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId);
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId);
	virtual void Release();
	virtual void FullSerialize(TSerialize ser) {};
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo(TSerialize ser);
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime) {}
	virtual void PostRemoteSpawn() {}
	virtual void HandleEvent(const SGameObjectEvent& event);
	virtual uint64 GetEventMask() const;
	virtual void ProcessEvent(const SEntityEvent& event);
	virtual void SetChannelId(uint16 id) {}
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {};
	virtual bool GetEntityPoolSignature( TSerialize signature );
	// IGameObjectExtension

	void  RequestUse(EntityId requesterId);
	void	ForceDrop();
	void  SetOwner(EntityId ownerId);
	ILINE EntityId  GetOwner() const {return m_OwnerId;}
	ILINE bool ShouldShowShieldedHitIndicator() const {return m_bShowShieldedHitIndicator;}
	float GetCurrentHealth() const; 
	void OnObjectPhysicsPropertiesChanged(); 
	void RefreshObjectVelocityTracker(const bool bAllowFallbackHelper = false); 
	void SetVehicleDamageParams(float meleeDamage, float throwDamage, float throwMinVelocity, float throwMaxVelocity);
	bool IsRooted() const; 
	bool CanBeUsedByPlayer(CPlayer* pPlayer) const;
	void CheckAchievements(IActor* pKillerActor);
	void OnHostMigration(const Quat& rotation, const Vec3& position, const Vec3& velocity);

	const char* GetClassificationName() const;

	// Allow triggering of sync from lua
	void SvOnHealthChanged(float fPrevHealth, float fCurrentHealth); 

	// Listener interface
	void RegisterListener(IEnvironmentalWeaponEventListener* pListener);
	void UnregisterListener(IEnvironmentalWeaponEventListener* pListener);

	// Events
	void OnGrabStart(); 
	void OnGrabEnd(); 

	void OnAttached();
	
	void OnDetachedFrom( const CActor* pOwner, const bool chargedThrown );

	void CloakSyncWithEntity( const EntityId masterEntityId, const bool forceDecloak );

	// Melee system
	void OnStartMelee(const Vec3& hitDir, const float hitDirMinRatio, const float hitDirMaxRatio, const bool bPerformSweepTests); 
	void OnFinishMelee(); 

	// Animation events
	bool OnAnimationEvent(const AnimEventInstance &event);

	// Charged Throw system
	void OnStartChargedThrow();

	const Vec3& GetInitialThrowPos() const { return m_initialThrowPos; }

	//Network RMIs
	struct EnvironmentalWeaponRequestParams
	{
		void SerializeWith(const TSerialize& ser) {};
	};

	struct EnvironmentalWeaponParams
	{
		EnvironmentalWeaponParams() {};

		void SerializeWith(TSerialize ser)
		{
			ser.Value("requesterId", requesterId, 'eid');
		}

		EntityId requesterId;
	};

	struct InitWeaponStateParams
	{
		EntityId m_ownerId; 
		int8 m_weaponState; 

		void SerializeWith( TSerialize ser )
		{
			ser.Value("ownerId", m_ownerId, 'eid');
			ser.Value("weaponState", m_weaponState, 'ui2');
		}
	};

	struct NoParams
	{
		void SerializeWith( TSerialize ser )
		{
		}
	};


	DECLARE_SERVER_RMI_NOATTACH(SvRequestUseEnvironmentalWeapon, EnvironmentalWeaponRequestParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClEnvironmentalWeaponUsed, EnvironmentalWeaponParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClInitWeaponState, InitWeaponStateParams, eNRT_ReliableUnordered);

#ifndef _RELEASE
	void DebugSetThrowPath(const Vec3& start, const Vec3& end ) { m_throwOrigin = start; m_throwTarget = end; }
#endif //ifndef _RELEASE

	// IGameRulesRevivedListener
	virtual void EntityRevived(EntityId entityId);
	// ~IGameRulesRevivedListener

	ILINE void OnCollision(const EventPhysCollision& collision) {m_intersectionAssist.OnCollision(collision);}

	// Intersection Assist
	CIntersectionAssistanceUnit& GetIntersectionAssistUnit() {return m_intersectionAssist;}

private:

	static const char * kHealthScriptName;

	enum E_WeaponState 
	{
		EWS_Initial = 0,
		EWS_Held,
		EWS_OnGround,
		EWS_Count
	};


	// required collision data
	struct EntityHitRecord
	{
		EntityHitRecord(EntityId entityId, const Vec3& normal, int surfaceIdx, int partId ,const Vec3& wSpaceHitLoc)
		{
			m_entityId		     = entityId; 
			m_wSpaceHitLoc		 = wSpaceHitLoc; 
			m_normal			 = normal;
			m_surfaceIdx		 = surfaceIdx; 
			m_partId             = partId; 
		}

		EntityId m_entityId; 
		Vec3	 m_wSpaceHitLoc;	// Store the wSpace collision point
	
		Vec3     m_normal;
		int  	 m_surfaceIdx;
		int		 m_partId;   
	};

	// periodically track weapon points during swing
	struct SWeaponTrackSlice
	{
		SWeaponTrackSlice()
			: m_start(ZERO)
			, m_dir(ZERO)
			, m_invLength(0.f)
			, m_bDamagePhaseActive(false)
			, m_bVisible(false)
		{
			memset( &m_fSteps[0], 0, sizeof(m_fSteps[0])*k_numTrackPoints );
		}
		SWeaponTrackSlice( const Vec3& start, const Vec3& dir, const bool damagePhase, const bool visible )
			: m_start(start)
			, m_dir(dir)
			, m_invLength(dir.IsZero()?0.f:(1.f/dir.GetLength()))
			, m_bDamagePhaseActive(damagePhase)
			, m_bVisible(visible)
		{
			const float edgeScaled = k_fBeamRad*m_invLength;
			const float stepScaled = (1.f-(edgeScaled*2.f)) * k_fInvStepSize;
			float currStep = edgeScaled;
			for(int i=0; i<k_numTrackPoints; i++)
			{
				m_fSteps[i] = currStep;
				currStep += stepScaled;
			}
		}

		static const float k_fBeamRad;
		static const int k_numTrackPoints = 3;
		static const float k_fInvStepSize;

		Vec3 m_start;
		Vec3 m_dir;
		float m_invLength;
		float m_fSteps[k_numTrackPoints];
		bool m_bDamagePhaseActive; 
		bool m_bVisible;
	};

	bool DoDetachFromParent();
	bool Use(EntityId requesterId);
	void Reset();

	bool ProcessMeleeHit(const EntityHitRecord& hitRecord, IPhysicalEntity* pVictimPhysicalEntity);
	void GenerateHitInfo(EntityId attackerEntityId, float damage,  HitInfo& hitInfo, const EntityHitRecord& entityHitRecord, const Vec3& nImpactDir, int hitType ) const;
	Vec3 CalculateImpulse_Player(const HitInfo& hitInfo, CPlayer& rPlayer, const Vec3& nImpactDir, EntityId attackerId) const;
	Vec3 CalculateImpulse_Object(const Vec3& hitLoc, const Vec3& nImpactDir, EntityId attackerId, const float victimObjectMass) const;
	void ApplyImpulse(const HitInfo& hitInfo, const EntityId victimEntityId) const;
	void OnFinishChargedThrow();


	// Helpers
	bool  EntityPreviouslyHit(const EntityId queryId) const;
	void  ProcessHitPlayer( IActor* pVictimActor, const EntityHitRecord& entityHitRecord, const Vec3& nImpactDir, EntityId attackerId  );
	void  ProcessHitObject( const EntityHitRecord& entityHitRecord, const Vec3& impactVel,IPhysicalEntity* pVictimPhysicalEntity, EntityId attackerId) const;
	void  CleanUpThrowerPhysicsConstraints(); 
	void  TrackWeaponVelocity(const float dt); 
	void  UpdateCachedWeaponTrackerPos(); 
	void  AttemptToRegisterInteractiveEntity(); 

	// Util
	bool PassesHitzoneConeCheck(const EventPhysCollision	*pEventPhysCollision) const; 
	bool  LocationWithinFrontalCone(const Vec3& attackerLocation,const Vec3& victimLocation,const Vec3& attackerFacingdir, float attackerHitConeRads) const;
	float CalculateVictimMass(IPhysicalEntity* pVictimPhysicalEntity) const; 
	void GenerateMaterialEffects(const Vec3& pos, const int surfaceIndex);
	void GenerateArtificialCollision(IPhysicalEntity* pAttackerPhysEnt, IPhysicalEntity *pCollider, int colliderMatId, const Vec3 &position, const Vec3& normal, const Vec3 &vel, int partId, int surfaceIdx, int iPrim, const Vec3& impulse );
	
	// Event Handlers
	void HandleMeleeCollisionEvent(const EventPhysCollision	*pEventPhysCollision, const bool bAllowActors); 
	void HandleMeleeCollisionEvent_Actor(const EventPhysCollision *pEventPhysCollision, EntityId victimEntityId, CPlayer* pOwnerPlayer, int victimIdx, IPhysicalEntity* pVictimPhysicalEntity); 
	void HandleMeleeCollisionEvent_Geometry(const EventPhysCollision *pEventPhysCollision, CPlayer* pOwnerPlayer, int victimIdx);
	void HandleMeleeCollisionEvent_BreakableGeometry(const EventPhysCollision *pEventPhysCollision, CPlayer* pOwnerPlayer, int victimIdx, IPhysicalEntity* pVictimPhysicalEntity);
	void HandleMeleeCollisionEvent_Object(const EventPhysCollision *pEventPhysCollision, IEntity* pVictimEntity, CPlayer* pOwnerPlayer, int victimIdx, IPhysicalEntity* pVictimPhysicalEntity);
	bool HandleChargedThrowCollisionEvent(const EventPhysCollision	*pEventPhysCollision); 

	// Animation events
	void OnMeleeStartDamagePhaseEvent(); 
	void OnMeleeEndDamagePhaseEvent(); 

	// Damage zone
	bool IsMeleeCollisionWithinDamageZone(const Vec3& wSpaceCollisionPoint) const; 

	// Setup
	void DoVehicleAttach(); 
	void DoInitWeaponState(); 
	void InitWeaponState_Held(); 
	void InitWeaponState_OnGround();
	void DelegateAuthorityOnOwnershipChanged(EntityId prevOwnerId, EntityId newOwnerId);
	void InitDamageZoneHelper(); 

	// Swing test
	void AddSwingRequest(const Vec3& swingPos, const Vec3& endPoint, bool initialise=false);
	void ProcessSwingResults();
	void PerformWeaponSweepTracking(); 
	void PerformWeaponSweepTest(const SWeaponTrackSlice& prevSlice, SWeaponTrackSlice& currSlice, const Vec3& eyePos); 

	// Events
	void OnRipDetachEvent(); 
	void UnRoot();

	// Logic to ensure glass breaks when hit with a swinging env weapon
	void UpdateGlassBreakParams(); 

	// Misc
	void PerformGroundClearanceAdjust( const CActor* pOwner, const bool chargedThrown ); 
	void PerformRearClearanceAdjust( const CActor* pOwner, const bool chargedThrown ); 

	// Listeners
	const static int s_maxListeners = 12;
	typedef  CryFixedArray<IEnvironmentalWeaponEventListener*, s_maxListeners> TListenerList;
	TListenerList m_eventListeners;

	// Track the velocity of the 'track point' on the given weapon here. 
	Vec3 m_prevWeaponTrackerPos; 
	Vec3 m_weaponTrackerVelocity;
	Vec3 m_lastPos;

	Vec3 m_initialThrowPos;

	Vec3 m_overrideHitDir;
	float m_overrideHitDirMinRatio;
	float m_overrideHitDirMaxRatio;  
	
	float m_ragdollPostMeleeImpactSpeed;
	float m_ragdollPostThrowImpactSpeed;
	float m_mass; // Cache this instead of having to grab it every impact

	float m_vehicleMeleeDamage;
	float m_vehicleThrowDamage;
	float m_vehicleThrowMinVelocity;
	float m_vehicleThrowMaxVelocity;
	float	m_healthWhenGrabbed;
	float m_noRequiredKillVelocityTimer; 

	EntityId m_throwerId; 
	EntityId m_OwnerId;
	EntityId m_previousThrowerId; //Only updated when the next person picks it up. E.g. when you throw it you are not the last thrower until someone else picks it up
	E_WeaponState m_State;

	HSCRIPTFUNCTION m_OnClientHealthChangedFunc; 

	// Keep track of all entities (player && non player) we have hit during a single 'attack' so we dont
	const static int s_MaxEntitiesAllowedHitsPerSwing = 8; 
	typedef  CryFixedArray<EntityId, s_MaxEntitiesAllowedHitsPerSwing> TEntityHitList;
	TEntityHitList m_entityHitList;

	uint     m_livingConstraintID; 
	uint     m_articulatedConstraintID; 

	EntityId	m_parentVehicleId;
	IVehicle* m_pLinkedVehicle;

	typedef uint16 TLogicFlags;
	typedef uint8 TAttackState;

	// LogicState //
	enum ELogicFlag
	{
		ELogicFlag_doVehicleAttach												= BIT(0),
		ELogicFlag_removalScheduled												= BIT(1),
		ELogicFlag_damageZoneHelperInitialised						= BIT(2),
		ELogicFlag_geomCollisionDetectedThisAttack				= BIT(3),
		ELogicFlag_doInitWeaponState											= BIT(4),
		ELogicFlag_hasParentVehicle												= BIT(5),
		ELogicFlag_hasHitOverride													= BIT(6),
		ELogicFlag_processDamageAnimEvents								= BIT(7),
		ELogicFlag_beingRippedUp													= BIT(8),
		ELogicFlag_performSweepTests											= BIT(9), 
		ELogicFlag_damagePhaseActive											= BIT(10), 
		ELogicFlag_weaponSweepTestsValid									= BIT(11),
		ELogicFlag_registeredWithIEMonitor								= BIT(12),
		ELogicFlag_spawnHidden														= BIT(13)
	};
	TLogicFlags m_logicFlags;
	// ~State // 

	// Attack State
	enum EAttackStateType
	{
		EAttackStateType_None											 = BIT(0),
		EAttackStateType_EnactingPrimaryAttack	   = BIT(1),
		EAttackStateType_ChargedThrow	  				   = BIT(2),
		EAttackStateType_Max											 = BIT(3)		       
	};
	TAttackState  m_currentAttackState;

	bool   m_bCanInstantKill;
	bool   m_bShowShieldedHitIndicator; 
	float  m_rootedAngleDotMin;
	float  m_rootedAngleDotMax;
	bool	 m_bClientHasKilledWithThrow;

	// Damage zone helpers
	IStatObj::SSubObject* m_pDamageZoneHelper; 
	IStatObj::SSubObject* m_pVelocityTracker; 

	string m_archetypename;
	string m_mfxLibraryName; 

	SWeaponTrackSlice m_prevFrameWeaponTrackSlice;  

	CIntersectionAssistanceUnit m_intersectionAssist;

#if ALLOW_SWEEP_DEBUGGING
	std::vector<SWeaponTrackSlice> m_weaponSwingTrackedPoints; 
	void RenderDebugWeaponSweep() const; 
#endif //#if ALLOW_SWEEP_DEBUGGING

#ifndef _RELEASE
	void UpdateDebugOutput() const;
	void RenderDebugStats() const; 

	Vec3 m_throwDir; 
	Vec3 m_throwOrigin; 
	Vec3 m_throwTarget;

	Vec3 m_weaponTrackerPosAtImpact; 


	// Debug rendering
	struct debugCollisionPt
	{
		debugCollisionPt(const Vec3& pos, const ColorB& col, const Vec3& damageHelperPos, const Vec3& damageHelperDir) 
			:m_wCollisionPos(pos), m_collisionCol(col), m_damageHelperPos(damageHelperPos), m_damageHelperDir(damageHelperDir), m_showDamageZoneHelper(true) {}
		debugCollisionPt(const Vec3& pos, const ColorB& col) 
			:m_wCollisionPos(pos), m_collisionCol(col), m_damageHelperPos(ZERO), m_damageHelperDir(ZERO), m_showDamageZoneHelper(false){}
		Vec3   m_wCollisionPos;
		Vec3   m_damageHelperPos; 
		Vec3   m_damageHelperDir; 
		ColorB m_collisionCol;
		bool   m_showDamageZoneHelper; 
	};
	std::vector<debugCollisionPt> m_debugCollisionPts_Geom; 
#endif //#ifndef _RELEASE
};

#endif
