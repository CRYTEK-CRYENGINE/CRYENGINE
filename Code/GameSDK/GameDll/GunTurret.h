// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: GunTurret Implementation

-------------------------------------------------------------------------
History:
- 24:05:2006   14:00 : Created by Michael Rauh

*************************************************************************/
#ifndef __GunTurret_H__
#define __GunTurret_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include <CryAISystem/IFactionMap.h>
#include "Weapon.h"
#include "Single.h"



class CGunTurret : public CWeapon, public IWeaponFiringLocator
{
	static const NetworkAspectType ASPECT_GOALORIENTATION = eEA_GameServerDynamic;
	static const NetworkAspectType ASPECT_STATEBITS = eEA_GameServerStatic;

protected:
	typedef struct SGunTurretParams
	{
		SGunTurretParams()
			: surveillance(false)
			, vehicles_only(false)
			, mg_range(20.0f)
			, rocket_range(20.0f)
			, tac_range(20.0f)
			, aim_tolerance(20.0f)
			, prediction(1.f)
			, turn_speed(1.5f)
			, min_pitch(-30.f)
			, max_pitch(75.f)
			, yaw_range(90.f)
			, factionID(IFactionMap::InvalidFactionID)
			, team(0)
			, enabled(true)
			, searching(false)
			, search_only(false)		
			, search_speed(1.0f)
			, update_target_time(1.5f)
			, abandon_target_time(0.5f)
			, TAC_check_time(0.5f)
			, burst_time(0.f)
			, burst_pause(0.f)
			, sweep_time(0.f)
			, light_fov(0.f)
			, explosive_damage_multiplier(0.0f)
			, find_cloaked(true)
		{ 						
		}

		bool Reset(IScriptTable* params)
		{
			SmartScriptTable turret;
			if (!params->GetValue("GunTurret", turret))
				return false;

			turret->GetValue("bSurveillance", surveillance);
			turret->GetValue("bVehiclesOnly", vehicles_only);
			turret->GetValue("bEnabled", enabled);
			turret->GetValue("bSearching", searching);
      turret->GetValue("bSearchOnly", search_only);
			turret->GetValue("TurnSpeed", turn_speed);
			turret->GetValue("MinPitch", min_pitch);
			turret->GetValue("MaxPitch", max_pitch);
			turret->GetValue("YawRange", yaw_range);
			turret->GetValue("MGRange", mg_range);
			turret->GetValue("RocketRange", rocket_range);
			turret->GetValue("TACDetectRange",tac_range);
      turret->GetValue("AimTolerance", aim_tolerance);
      turret->GetValue("Prediction", prediction);
			turret->GetValue("SearchSpeed",search_speed);
			turret->GetValue("UpdateTargetTime",update_target_time);
			turret->GetValue("AbandonTargetTime",abandon_target_time);
			turret->GetValue("TACCheckTime",TAC_check_time);
      turret->GetValue("BurstTime", burst_time);
      turret->GetValue("BurstPause", burst_pause);
      turret->GetValue("SweepTime", sweep_time);
      turret->GetValue("LightFOV", light_fov);
      turret->GetValue("ExplosiveDamageMultiplier", explosive_damage_multiplier);
			turret->GetValue("bFindCloaked", find_cloaked);


			const char* faction = 0;
			if (gEnv->pAISystem && params->GetValue("esFaction", faction))
				factionID = gEnv->pAISystem->GetFactionMap().GetFactionID(faction);

      Limit(min_pitch, -90.f, 90.f);
      Limit(max_pitch, -90.f, 90.f);
      Limit(yaw_range, 0.f, 360.f);
      Limit(prediction, 0.f, 1.f);

			return true;
		}

		bool		surveillance;
		bool		vehicles_only;
		float		mg_range;    
		float		rocket_range;    
		float   tac_range;
    float   aim_tolerance;
    float   prediction; 
		float		turn_speed;
		float		min_pitch;
		float		max_pitch;
		float		yaw_range;
		uint8		factionID;
		int			team;
		bool        enabled;
		bool        searching;
    bool    search_only;
		float       search_speed;
		float       update_target_time;
		float       abandon_target_time;
		float       TAC_check_time;
    float   burst_time;
    float   burst_pause;
    float   sweep_time;
    float   light_fov;
		float   explosive_damage_multiplier;
    bool    find_cloaked;
	} SGunTurretParams;

 	enum ETargetClass//the higher the value the more the priority
	{
		eTC_NotATarget,
		eTC_Player,
		eTC_Vehicle,
		eTC_TACProjectile
	};

  template<typename T> struct TRandomVal
  {
    TRandomVal() { Range(0); }    
    
    ILINE void Range(T r) { range = r; New(); }
    ILINE void New() { val = cry_random(-range, range); }
    ILINE T Val() { return val; }
    
    T val;    
    T range; 
  };

  enum ERandomVals
  {
    eRV_UpdateTarget = 0,
    eRV_AbandonTarget,
    eRV_BurstTime,
    eRV_BurstPause,
    eRV_Last
  };

public:

	static bool QueryDeployment(const Vec3 &pos, const Vec3 &direction, float radius, Vec3 &hitPos);

	CGunTurret();
	virtual ~CGunTurret();

	void SetTeamNum(int num) { m_turretparams.team = num; }

	// IGameObjectExtension
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual NetworkAspectType GetNetSerializeAspects();
	//~ IGameObjectExtension

	// IItem
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void Update(SEntityUpdateContext& ctx, int update);
	virtual void OnReset();
	virtual void OnHit(float damage, int hitType);
	virtual void OnDestroyed();
	virtual void OnRepaired();
	virtual void DestroyedGeometry(bool use);
  virtual void SetCharacterAttachmentLocalTM(int slot, const char *name, const Matrix34 &tm);
  virtual bool SetAspectProfile( EEntityAspects aspect, uint8 profile );
  virtual void HandleEvent(const SGameObjectEvent&);
	virtual void PostInit(IGameObject * pGameObject);
	virtual bool CanPickUp(EntityId userId) const { return false; };
	virtual bool IsPickable() const	{	return false;	}
	virtual void SetOwnerId(EntityId ownerId);
  // ~IItem

	// IWeapon
	virtual Vec3 GetFiringPos(const Vec3 &probableHit) const;

	virtual void SetDestinationEntity(EntityId targetId){ m_destinationId = targetId; }
	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		GetInternalMemoryUsage(s);
	}
	void GetInternalMemoryUsage(ICrySizer * s) const
	{
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}
	// ~IWeapon

  // IWeaponFiringLocator
  virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit) { return false; }
  virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos) { return false; }
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
  virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir) { return false; }
  virtual void WeaponReleased() {}
// IWeaponFiringLocator

	// Network
	virtual bool NetAllowUpdate(bool requireActor);
	// ~Network

	void SetFaction(const uint8 factionID);

protected:
	virtual bool Init(IGameObject * pGameObject);
	virtual void InitItemFromParams();

	void InternalStartFire(bool sec);
	void InternalStopFire(bool sec);
	bool InternalIsFiring(bool sec);

	ILINE const SGunTurretParams & GetTurretParams() const { return m_turretparams; }
	virtual IEntity *GetClosestTarget() const;
	IEntity *GetClosestTACShell();
	virtual void UpdateAiming(IEntity * pCurrentTarget, SEntityUpdateContext& ctx);

	ILINE void SetGoalPitchAndYaw(float goalPitch, float goalYaw)
	{
		m_goalPitch = goalPitch;
		m_goalYaw = goalYaw;
		CHANGED_NETWORK_STATE(this, ASPECT_GOALORIENTATION);
	}

#if 0
private:	// For the time being, let subclasses (there's only 1 at time of writing) access ALL internal functions and data [TF]
#endif

	bool IsOperational();
	Vec3 GetTargetPos(IEntity* pEntity) const;
	Vec3 PredictTargetPos(IEntity* pTarget, bool sec);//sec - weapon to use
  Vec3 GetSweepPos(IEntity* pTarget, const Vec3& shootPos);
	bool GetTargetAngles(const Vec3& targetPos, float& z, float& x) const;

	bool IsTargetDead(IActor* pTarget) const;
	bool IsTargetHostile(IActor *pTarget) const;
	bool IsTargetSpectating(IActor* pTarget) const;
	bool IsTACBullet(IEntity* pTarget) const;
	ETargetClass GetTargetClass(IEntity* pTarget)const;

	virtual void OverrideShouldShoot(IEntity* pTarget, bool & shouldShoot) const {};

	bool IsInRange(const Vec3& pos, ETargetClass cl)const;
	bool IsTargetAimable(float angleYaw, float anglePitch) const;
	bool IsTargetShootable(IEntity* pTarget) const;
  bool RayCheck(IEntity* pTarget, const Vec3& pos, const Vec3& dir) const;
  bool IsTargetCloaked(IActor* pTarget) const;

	bool IsTargetRocketable(const Vec3 &pos) const;
	bool IsTargetMGable(const Vec3 &pos) const;
	bool IsAiming(const Vec3& pos, float treshold) const;
	IEntity *ResolveTarget(IEntity *pTarget) const;

	void    OnTargetLocked(IEntity* pTarget);
	void    Activate(bool active);
	void    ServerUpdate(SEntityUpdateContext& ctx, int update);
	void		UpdateEntityProperties();
	void    UpdateGoal(IEntity* pTarget, float deltaTime);
	void    UpdateOrientation(float deltaTime);
	void    UpdateSearchingGoal(float deltaTime);
	void    UpdatePhysics();
	void    ChangeTargetTo(IEntity* pTarget);
  bool    UpdateBurst(float deltaTime);
  void    UpdateDeviation(float deltaTime, const Vec3& shootPos);

  void    ReadProperties(IScriptTable *pScriptTable);

	ILINE Vec3 GetWeaponPos() const;
  ILINE Vec3 GetWeaponDir() const;
  ILINE float GetYaw() const;
  ILINE float GetPitch() const;
  ILINE float GetBurstTime() const;

  void UpdateHover(SEntityUpdateContext &ctx);

  void DrawDebug();
	float GetScaledDamage(float damage, int hitType) const;

	void CreateAIRepresentation();

	SGunTurretParams m_turretparams;
  TRandomVal<float> m_randoms[eRV_Last];
	
	IFireMode *m_fm2;

	EntityId m_targetId;
	EntityId m_destinationId;

	float m_checkTACTimer;
	float	m_updateTargetTimer;
  float m_abandonTargetTimer;
  float m_burstTimer;
  float m_pauseTimer;
  float m_rayTimer;

  Vec3  m_deviationPos;
	float m_goalYaw;
	float	m_goalPitch;
	
  uint8 m_searchHint;
  uint8 m_fireHint;

	EntityEffects::TAttachedEffectId m_lightId;  

  Matrix34 m_barrelRotation;

	Vec3 m_radarHelperPos;
	Vec3 m_tripodHelperPos;
	Vec3 m_barrelHelperPos;
	Vec3 m_fireHelperPos;
	Vec3 m_rocketHelperPos;
	Vec3 m_turretHelperPos;

	bool   m_destroyed;
  bool   m_canShoot;

  float m_vertVel;
};

#endif // __GunTurret_H__
