// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a seat action to control vehicle weapons

   -------------------------------------------------------------------------
   History:
   - 06:12:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLESEATACTIONWEAPONS_H__
#define __VEHICLESEATACTIONWEAPONS_H__

struct IWeapon;

struct SWeaponAction
{
	std::vector<string> animations;

	void                GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(animations);
		for (size_t i = 0; i < animations.size(); i++)
			s->Add(animations[i]);
	}
};
typedef std::map<string, SWeaponAction> TWeaponActionMap;

typedef std::vector<IVehicleHelper*>    TVehicleHelperVector;

struct SVehicleWeapon
{
	TWeaponActionMap     actions;
	TVehicleHelperVector helpers;
	IEntityClass*        pEntityClass;
	EntityId             weaponEntityId;
	IVehiclePart*        pPart;
	bool                 inheritVelocity;
	float                m_respawnTime;

	TagID                contextID;

	//Network dependency parent and child ID's
	EntityId networkChildId;
	EntityId networkParentId;

	SVehicleWeapon()
	{
		pEntityClass = NULL;
		weaponEntityId = 0;
		pPart = 0;
		inheritVelocity = true;
		m_respawnTime = 0.f;
		networkChildId = 0;
		networkParentId = 0;
		contextID = TAG_ID_INVALID;
	}

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(actions);
		s->AddObject(helpers);
	}
};

class CVehicleSeatActionWeapons
	:
	  public IVehicleSeatAction,
	  public IWeaponFiringLocator,
	  public IWeaponEventListener
{
	IMPLEMENT_VEHICLEOBJECT

	enum EAttackInput
	{
		eAI_Primary = 0,
		eAI_Secondary,
	};

public:

	CVehicleSeatActionWeapons();
	virtual ~CVehicleSeatActionWeapons();

	// IVehicleSeatAction
	virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void OnSpawnComplete() override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void StartUsing(EntityId passengerId) override;
	void         StartUsing();

	virtual void ForceUsage() override;
	virtual void StopUsing() override;
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override;

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void PostSerialize() override;
	virtual void Update(const float deltaTime) override;
	virtual void UpdateFromPassenger(const float frameTime, EntityId playerId) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override;
	// ~IVehicleSeatAction

	// IWeaponFiringLocator
	virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit) override;
	virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos) override;
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos) override;
	virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos) override;
	virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir) override;
	virtual void WeaponReleased() override {}
	// ~IWeaponFiringLocator

	// IWeaponEventListener
	virtual void OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
	                     const Vec3& pos, const Vec3& dir, const Vec3& vel) override;
	virtual void OnStartFire(IWeapon* pWeapon, EntityId shooterId) override                            {}
	virtual void OnStopFire(IWeapon* pWeapon, EntityId shooterId) override                             {}
	virtual void OnFireModeChanged(IWeapon* pWeapon, int currentFireMode) override                     {}
	virtual void OnStartReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType) override {}
	virtual void OnEndReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType) override   {}
	virtual void OnSetAmmoCount(IWeapon* pWeapon, EntityId shooterId) override                         {}
	virtual void OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType) override;
	virtual void OnReadyToFire(IWeapon* pWeapon) override                                              {}
	virtual void OnPickedUp(IWeapon* pWeapon, EntityId actorId, bool destroyed) override               {}
	virtual void OnDropped(IWeapon* pWeapon, EntityId actorId) override                                {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId) override                                {}
	virtual void OnStartTargetting(IWeapon* pWeapon) override                                          {}
	virtual void OnStopTargetting(IWeapon* pWeapon) override                                           {}
	virtual void OnSelected(IWeapon* pWeapon, bool select) override                                    {}
	virtual void OnEndBurst(IWeapon* pWeapon, EntityId shooterId) override                             {}
	// ~IWeaponEventListener

	void            BindWeaponToNetwork(EntityId weaponId);
	Vec3            GetAverageFiringPos();

	virtual void    OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	void            StartFire();
	void            StopFire();
	bool            IsFiring() const                  { return m_isShooting; }
	void            SetFireTarget(const Vec3& target) { m_fireTarget = target; }

	bool            IsSecondary() const               { return m_isSecondary; }

	bool            CanFireWeapon() const;

	unsigned int    GetWeaponCount() const;
	unsigned int    GetCurrentWeapon() const { return m_weaponIndex; }

	EntityId        GetCurrentWeaponEntityId() const;
	EntityId        GetWeaponEntityId(unsigned int index) const;
	void            ClSetupWeapon(unsigned int index, EntityId weaponId);

	IItem*          GetIItem(unsigned int index);
	IWeapon*        GetIWeapon(unsigned int index);

	IItem*          GetIItem(const SVehicleWeapon& vehicleWeapon);
	IWeapon*        GetIWeapon(const SVehicleWeapon& vehicleWeapon);

	SVehicleWeapon* GetVehicleWeapon(EntityId weaponId);
	bool            ForcedUsage() const { return m_Forced; }

	ILINE bool      IsMounted() const
	{
		return m_isMounted;
	}

	void OnWeaponRespawned(int weaponIndex, EntityId newWeaponEntityId);
	void OnVehicleActionControllerDeleted();

protected:

	IEntity*        SpawnWeapon(SVehicleWeapon& weapon, IEntity* pVehicleEntity, const string& partName, int weaponIndex);
	void            SetupWeapon(SVehicleWeapon& weapon);

	SVehicleWeapon& GetVehicleWeapon();
	SVehicleWeapon& GetWeaponInfo(int weaponIndex);

	IVehicleHelper* GetHelper(SVehicleWeapon& vehicleWeapon);
	IActor*         GetUserActor();

protected:

	int          GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip);

	void         DoUpdate(const float deltaTime);

	virtual void UpdateWeaponTM(SVehicleWeapon& weapon);
	IEntity*     GetEntity(const SVehicleWeapon& weapon);

	bool         CanTriggerActionFire(const TVehicleActionId actionId) const;
	bool         CanTriggerActionFiremode(const TVehicleActionId actionId) const;
	bool         CanTriggerActionZoom(const TVehicleActionId actionId) const;

	IVehicle*     m_pVehicle;
	string        m_partName;
	IVehicleSeat* m_pSeat;

	EntityId      m_passengerId;

	typedef std::vector<SVehicleWeapon> TVehicleWeaponVector;
	TVehicleWeaponVector m_weapons;
	TVehicleWeaponVector m_weaponsCopy;
	Vec3                 m_fireTarget;

	int                  m_lastActionActivationMode;
	int                  m_weaponIndex;

	float                m_shotDelay;
	float                m_nextShotTimer;
	float                m_respawnTimer;

	EAttackInput         m_attackInput;

	bool                 m_isUsingShootingByUpdate;
	bool                 m_isSecondary;
	bool                 m_isShootingToCrosshair;
	bool                 m_isShooting;
	bool                 m_Forced;
	bool                 m_isMounted;
};

#endif
