// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/IAgent.h>
#include "IActionMapManager.h"

struct IWeapon;
struct IActor;

struct SSpreadModParams;
struct SRecoilModParams;
class CProjectile;
struct IItem;

enum EPhysicalizationType
{
	ePT_None = 0,
	ePT_Particle,
	ePT_Rigid,
	ePT_Static,
};

enum EZoomState
{
	eZS_ZoomedOut = 0,
	eZS_ZoomingIn,
	eZS_ZoomedIn,
	eZS_ZoomingOut,
};

struct SProjectileLaunchParams
{
	float    fSpeedScale;
	EntityId trackingId;
	Vec3     vShootTargetPos;

	SProjectileLaunchParams() : fSpeedScale(1.0f), trackingId(0), vShootTargetPos(ZERO) {}
};

struct IFireMode
{
	virtual ~IFireMode(){}

	virtual void PostInit() = 0;
	virtual void Update(float frameTime, uint32 frameId) = 0;
	virtual void PostUpdate(float frameTime) = 0;
	virtual void UpdateFPView(float frameTime) = 0;
	virtual void Release() = 0;
	virtual void GetMemoryUsage(ICrySizer* s) const = 0;

	virtual void Activate(bool activate) = 0;

	virtual int  GetAmmoCount() const = 0;
	virtual int  GetClipSize() const = 0;

	virtual bool OutOfAmmo() const = 0;
	virtual bool LowAmmo(float thresholdPerCent) const = 0;
	virtual bool CanReload() const = 0;
	virtual void Reload(int zoomed) = 0;
	virtual bool IsReloading(bool includePending = true) = 0;
	virtual void CancelReload() = 0;
	virtual bool CanCancelReload() = 0;
	//virtual void FinishReload() = 0;

	virtual float         GetRecoil() const = 0;
	virtual float         GetSpread() const = 0;
	virtual float         GetSpreadForHUD() const = 0;
	virtual float         GetMinSpread() const = 0;
	virtual float         GetMaxSpread() const = 0;
	virtual float         GetHeat() const = 0;
	virtual const char*   GetCrosshair() const { return ""; };
	virtual bool          CanOverheat() const = 0;
	virtual Vec3          ApplySpread(const Vec3& dir, float spread, int quadrant = -1) const = 0;
	virtual void          ApplyAutoAim(Vec3& rDir, const Vec3& pos) const = 0;

	virtual bool          CanFire(bool considerAmmo = true) const = 0;
	virtual void          StartFire() = 0;
	virtual void          StopFire() = 0;
	virtual bool          IsFiring() const = 0;
	virtual bool          IsSilenced() const = 0;
	virtual bool          AllowZoom() const = 0;
	virtual void          Cancel() = 0;
	virtual void          SetProjectileLaunchParams(const SProjectileLaunchParams& launchParams) { CRY_ASSERT_MESSAGE(0, "Firemode does not handle launch params"); }

	virtual void          NetShoot(const Vec3& hit, int predictionHandle) = 0;
	virtual void          NetShootEx(const Vec3& pos, const Vec3& dir, const Vec3& vel, const Vec3& hit, float extra, int predictionHandle) = 0;
	virtual void          NetEndReload() = 0;
	virtual void          ReplayShoot() {};

	virtual void          NetStartFire() = 0;
	virtual void          NetStopFire() = 0;

	virtual EntityId      GetProjectileId() const = 0;
	virtual void          SetProjectileId(EntityId id) = 0;
	virtual EntityId      RemoveProjectileId() = 0;

	virtual IEntityClass* GetAmmoType() const = 0;
	virtual int           GetDamage() const = 0;

	virtual float         GetSpinUpTime() const = 0;
	virtual float         GetNextShotTime() const = 0;
	virtual void          SetNextShotTime(float time) = 0;
	virtual float         GetFireRate() const = 0;

	virtual Vec3          GetFiringPos(const Vec3& probableHit) const = 0;
	virtual Vec3          GetFiringDir(const Vec3& probableHit, const Vec3& firingPos) const = 0;

	virtual void          Enable(bool enable) = 0;
	virtual bool          IsEnabled() const = 0;

	virtual void          SetName(const char* name) = 0;
	virtual const char*   GetName() const = 0;

	virtual bool          HasFireHelper() const = 0;
	virtual Vec3          GetFireHelperPos() const = 0;
	virtual Vec3          GetFireHelperDir() const = 0;

	virtual int           GetCurrentBarrel() const = 0;

	virtual void          Serialize(TSerialize ser) = 0;
	virtual void          PostSerialize() = 0;

	virtual void          PatchSpreadMod(const SSpreadModParams& sSMP, float modMultiplier) = 0;
	virtual void          ResetSpreadMod() = 0;

	virtual void          PatchRecoilMod(const SRecoilModParams& sRMP, float modMultiplier) = 0;
	virtual void          ResetRecoilMod() = 0;

	virtual void          OnZoomStateChanged() = 0;
};

struct IZoomMode
{
	virtual ~IZoomMode(){}

	virtual void       Update(float frameTime, uint32 frameId) = 0;
	virtual void       Release() = 0;
	virtual void       GetMemoryUsage(ICrySizer* s) const = 0;

	virtual void       Activate(bool activate) = 0;

	virtual bool       CanZoom() const = 0;
	virtual bool       StartZoom(bool stayZoomed = false, bool fullZoomOut = true, int zoomStep = 1) = 0;
	virtual void       StopZoom() = 0;
	virtual void       ExitZoom(bool force = false) = 0;

	virtual int        GetCurrentStep() const = 0;
	virtual float      GetZoomFoVScale(int step) const = 0;
	virtual float      GetZoomInTime() const = 0;
	virtual float      GetZoomTransition() const = 0;

	virtual void       ZoomIn() = 0;
	virtual bool       ZoomOut() = 0;

	virtual bool       IsZoomed() const = 0;
	virtual bool       IsZoomingInOrOut() const = 0;
	virtual bool       IsZoomingIn() const = 0;
	virtual EZoomState GetZoomState() const = 0;
	virtual bool       AllowsZoomSnap() const = 0;

	virtual void       Enable(bool enable) = 0;
	virtual bool       IsEnabled() const = 0;

	virtual void       Serialize(TSerialize ser) = 0;

	virtual void       GetFPOffset(QuatT& offset) const = 0;

	virtual void       UpdateFPView(float frameTime) = 0;
	virtual void       FilterView(struct SViewParams& viewParams) = 0;
	virtual void       PostFilterView(struct SViewParams& viewParams) = 0;

	virtual int        GetMaxZoomSteps() const = 0;

	virtual void       ApplyZoomMod(IFireMode* pFM, float modMultiplier) = 0;
	virtual void       ResetZoomMod(IFireMode* pFM) = 0;

	//! zoom mode is activated by toggling
	virtual bool IsToggle() = 0;

	virtual void StartStabilize() = 0;
	virtual void EndStabilize() = 0;
	virtual bool IsStable() = 0;
};

struct IWeaponFiringLocator
{
	virtual ~IWeaponFiringLocator(){}
	virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit) = 0;                                                     // probable hit position
	virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos) = 0;                                                       // position of the projectile
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos) = 0;       // this is target pos - firing pos
	virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos) = 0; // this is acutal weapon direction
	virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir) = 0;                           // velocity to add up to the projectile (usually host velocity)
	virtual void WeaponReleased() = 0;                                                                                                             // called when weapon is deleted
};

struct IWeaponEventListener
{
	virtual ~IWeaponEventListener(){}
	virtual void OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
	                     const Vec3& pos, const Vec3& dir, const Vec3& vel) = 0;
	virtual void OnStartFire(IWeapon* pWeapon, EntityId shooterId) = 0;
	virtual void OnStopFire(IWeapon* pWeapon, EntityId shooterId) = 0;
	virtual void OnFireModeChanged(IWeapon* pWeapon, int currentFireMode) = 0;
	virtual void OnStartReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType) = 0;
	virtual void OnEndReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType) = 0;
	virtual void OnSetAmmoCount(IWeapon* pWeapon, EntityId shooterId) = 0;
	virtual void OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType) = 0;
	virtual void OnReadyToFire(IWeapon* pWeapon) = 0;

	virtual void OnPickedUp(IWeapon* pWeapon, EntityId actorId, bool destroyed) = 0;
	virtual void OnDropped(IWeapon* pWeapon, EntityId actorId) = 0;

	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId) = 0;

	virtual void OnStartTargetting(IWeapon* pWeapon) = 0;
	virtual void OnStopTargetting(IWeapon* pWeapon) = 0;

	virtual void OnSelected(IWeapon* pWeapon, bool selected) = 0;
	virtual void OnEndBurst(IWeapon* pWeapon, EntityId shooterId) = 0;

	// empty implementation to be compatible with other game dlls
	virtual void OnZoomChanged(IWeapon* pWeapon, bool zoomed, int idx) {}
};

struct AIWeaponDescriptor;
struct HazardWeaponDescriptor;

// Summary
// Queries that can be done throw IWeapon::Query function to handle
// generic or game specific functionality
enum EWeaponQuery
{
	eWQ_Has_Accessory_Laser = 0,
	eWQ_Has_Accessory_Flashlight,
	eWQ_Is_Laser_Activated,
	eWQ_Is_Flashlight_Activated,
	eWQ_Activate_Flashlight,
	eWQ_Activate_Laser,
	eWQ_Raise_Weapon,
	eWQ_Last
};

// Summary
//   Interface to implement a Weapon class
struct IWeapon
{
	virtual ~IWeapon(){}
	// Summary
	//   Receives an action from the client
	// Parameters
	//   actorId - EntityId of the actor who sent the action
	//   actionId - 'name' of the action performed
	//   activationMode - one of the activation mode defined in EActionActivationMode
	//   value - value which quantified the action performed
	virtual void                  OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value) = 0;

	virtual void                  SetFiringLocator(IWeaponFiringLocator* pLocator) = 0;
	virtual IWeaponFiringLocator* GetFiringLocator() const = 0;

	virtual void                  AddEventListener(IWeaponEventListener* pListener, const char* who) = 0;
	virtual void                  RemoveEventListener(IWeaponEventListener* pListener) = 0;

	// Summary
	//   Sets the position of the destination target
	// Parameters
	//   pos - position in world space
	virtual void SetDestination(const Vec3& pos) = 0;

	// Summary
	//   Sets an entity as the destination target
	// Parameters
	//   pos - position in world space
	virtual void SetDestinationEntity(EntityId targetId) = 0;

	// Summary
	//   Retrieves the destination position
	virtual const Vec3& GetDestination() = 0;

	// Summary
	//   Retrieves the firing position
	// See Also
	//   GetFiringDir
	virtual Vec3 GetFiringPos(const Vec3& probableHit) const = 0;

	// Summary
	//   Retrieves the firing direction
	// Remarks
	//   It's important to pass the firing position in order to get an accurate
	//   firing direction.
	// Parameters
	//   firingPos - the firing position
	// See Also
	//   GetFiringPos
	virtual Vec3 GetFiringDir(const Vec3& probableHit, const Vec3& firingPos) const = 0;

	// Summary
	//   Requests the weapon to start firing
	// See Also
	//   CanFire
	virtual void StartFire() = 0;

	// Summary
	//   Requests the weapon to start firing, using specific projectile launch params
	// See Also
	//   CanFire
	virtual void StartFire(const SProjectileLaunchParams& launchParams) = 0;

	// Summary
	//   Requests the weapon to stop firing
	virtual void StopFire() = 0;

	// Summary
	//   Determines if the weapon can shoot
	virtual bool CanFire() const = 0;

	// Summary
	//   Query to know if it's allowed to stop firing
	virtual bool CanStopFire() const = 0;

	// Summary
	//   Requests the weapon to start its zoom mode
	// Parameters
	//   shooterId - EntityId of the actor who uses this weapon.
	// See Also
	//   CanZoom
	virtual void StartZoom(EntityId shooterId, int zoomed = 1) = 0;

	// Summary
	//   Requests the weapon to stops its zoom mode
	// Parameters
	//   shooterId - EntityId of the actor who uses this weapon.
	virtual void StopZoom(EntityId shooterId) = 0;

	// Summary
	//   Determines if the weapon can zoom
	virtual bool CanZoom() const = 0;

	// Summary
	//   Requests the weapon to immediately stops its zoom mode
	virtual void ExitZoom(bool force = false) = 0;

	// Summary
	//   Performs reloading of the weapon
	// Parameters
	//   force - when enabled, this will force the weapon to reload even if some
	//           conditions could normally block this from happening
	// See Also
	//   CanReload
	virtual void Reload(bool force = false) = 0;

	// Summary
	//   Determines if the weapon can reload
	virtual bool CanReload() const = 0;

	// Summary
	//   Determines if the weapon has no more ammunition
	// Parameters
	//   allFireModes - Unless this option is enabled, the ammunition status
	//                  will only be applicable for the current firemode
	virtual bool OutOfAmmo(bool allFireModes) const = 0;

	// Summary
	//   Determines if the weapon has no more ammunition, including all ammo types
	virtual bool OutOfAmmoTypes() const = 0;

	// Summary
	//   Determines if the weapon is low on ammunition based on the passed threshold
	// Parameters
	//   thresholdPerCent - Percentage of ammo under which low ammo is true
	virtual bool LowAmmo(float thresholdPerCent) const = 0;

	// Summary
	//   Retrieves the ammunition count
	// Parameters
	//   pAmmoType - EntityClass pointer of the ammo type
	// Returns
	//   The ammo count if the type was properly specified, or 0 if the
	//   specified ammo type wasn't found
	// See Also
	//   SetAmmoCount
	virtual int GetAmmoCount(IEntityClass* pAmmoType) const = 0;

	// Summary
	//   Sets the ammunition count
	// Parameters
	//   pAmmoType - EntityClass pointer of the ammo type
	//   count - Specifies the desired ammo count
	// See Also
	//   GetAmmoCount
	virtual void SetAmmoCount(IEntityClass* pAmmoType, int count) = 0;

	// Summary
	//   Retrieves the fire mode count
	virtual int GetNumOfFireModes() const = 0;

	// Summary
	//   Retrieves a specified fire mode, by index
	// Parameters
	//   idx - index of the specified fire mode
	// See Also
	//   GetNumOfFireModes, GetFireModeIdx
	virtual IFireMode* GetFireMode(int idx) const = 0;

	// Summary
	//   Retrieves a specified fire mode, by name
	// Parameters
	//   name - name of the specified fire mode
	virtual IFireMode* GetFireMode(const char* name) const = 0;

	// Summary
	//   Retrieves the index for a specified fire mode
	// Parameters
	//   name - name of the specified fire mode
	virtual int GetFireModeIdx(const char* name) const = 0;

	// Summary
	//   Retrieves the index of the current fire mode
	virtual int GetCurrentFireMode() const = 0;

	// Summary
	//   Sets the fire mode, by index
	// Parameters
	//   idx - index of the requested fire mode
	virtual void SetCurrentFireMode(int idx) = 0;

	// Summary
	//   Sets the fire mode, by name
	// Parameters
	//   name - name of the requested fire mode
	virtual void SetCurrentFireMode(const char* name) = 0;

	// Summary
	//   Toggles to the next fire mode
	virtual void ChangeFireMode() = 0;

	// Summary
	//   Retrieves a specified zoom mode, by index
	// Parameters
	//   idx - index of the specified zoom mode
	// See Also
	//   GetNumOfZoomModes, GetZoomModeIdx
	virtual IZoomMode* GetZoomMode(int idx) const = 0;

	// Summary
	//   Retrieves a specified zoom mode, by name
	// Parameters
	//   name - name of the specified zoom mode
	virtual IZoomMode* GetZoomMode(const char* name) const = 0;

	// Summary
	//   Retrieves the index for a specified zoom mode
	// Parameters
	//   name - name of the specified zoom mode
	//virtual int GetZoomModeIdx(const char *name) const = 0;

	// Summary
	//   Retrieves the index of the current zoom mode
	virtual int GetCurrentZoomMode() const = 0;

	// Summary
	//   Sets the zoom mode, by index
	// Parameters
	//   idx - index of the requested zoom mode
	virtual void SetCurrentZoomMode(int idx) = 0;

	// Summary
	//   Sets the zoom mode, by name
	// Parameters
	//   name - name of the requested zoom mode
	virtual void SetCurrentZoomMode(const char* name) = 0;

	// Summary
	//   Toggles to the next zoom mode
	//virtual void ChangeZoomMode() = 0;

	// Summary
	//   Sets the host of the weapon
	// Description
	//   The host of the weapon can be any entity which the weapon is child of.
	//   For example, the cannon of the tank has the tank as an host.
	// Parameters
	//   hostId - EntityId of the host
	// See Also
	//   GetHostId
	virtual void SetHostId(EntityId hostId) = 0;

	// Summary
	//   Retrieves the EntityId of the host
	virtual EntityId GetHostId() const = 0;

	// Summary
	//   Performs a melee attack
	// See Also
	//   CanMeleeAttack
	// Parameters
	//   bShort - "short" melee, whatever that means (e.g. melee while standing, rather than moving towards the enemy)
	virtual void MeleeAttack(bool bShort = false) = 0;

	// Summary
	//   Determines if the weapon can perform a melee attack
	// See Also
	//   MeleeAttack
	virtual bool CanMeleeAttack() const = 0;

	virtual void SaveWeaponPosition() {}// FIX ME: = 0;

	virtual bool IsValidAssistTarget(IEntity* pEntity, IEntity* pSelf, bool includeVehicles = false) = 0;

	virtual bool PredictProjectileHit(IPhysicalEntity* pShooter, const Vec3& pos, const Vec3& dir,
	                                  const Vec3& velocity, float speed, Vec3& predictedPosOut, float& projectileSpeedOut,
	                                  Vec3* pTrajectoryPositions = 0, unsigned int* trajectorySizeInOut = 0, float timeStep = 0.24f,
	                                  Vec3* pTrajectoryVelocities = 0, const bool predictionForAI = false) const = 0;

	virtual bool                      IsZoomed() const = 0;
	virtual bool                      IsZoomingInOrOut() const = 0;
	virtual bool                      IsReloading(bool includePending = true) const = 0;
	virtual EZoomState                GetZoomState() const = 0;

	virtual bool                      GetScopePosition(Vec3& pos) = 0;

	virtual bool                      HasAttachmentAtHelper(const char* helper) = 0;
	virtual void                      GetAttachmentsAtHelper(const char* helper, CCryFixedStringListT<5, 30>& rAttachments) = 0;

	virtual bool                      IsServerSpawn(IEntityClass* pAmmoType) const = 0;

	virtual const AIWeaponDescriptor& GetAIWeaponDescriptor() const = 0;

	// Summary
	//	Query some weapon actions, info
	// Patameters
	//	query - Query Id
	//	param - In/Out param, specific for each query
	virtual bool Query(EWeaponQuery query, const void* param = NULL) = 0;

	/*
	   virtual bool		IsLamAttached() = 0;
	   virtual bool    IsFlashlightAttached() = 0;
	   virtual void    ActivateLamLaser(bool activate, bool aiRequest = true) = 0;
	   virtual void		ActivateLamLight(bool activate, bool aiRequest = true) = 0;
	   virtual bool	  IsLamLaserActivated() = 0;
	   virtual bool		IsLamLightActivated() = 0;
	 */

	virtual void       RequestShoot(IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel, const Vec3& hit, float extra, int predictionHandle, bool forceExtended) = 0;

	virtual IFireMode* GetMeleeFireMode() const = 0;

	/*
	   virtual	void		RaiseWeapon(bool raise, bool faster = false) = 0;
	   virtual void		LowerWeapon(bool lower)  = 0;
	   virtual	bool		IsWeaponRaised() const = 0;
	   virtual bool		IsWeaponLowered() = 0;
	 */

	virtual bool IsSwitchingFireMode() = 0;
	virtual void StartChangeFireMode() = 0;
	virtual void EndChangeFireMode() = 0;

	//virtual void ExitViewmodes() = 0;

	//virtual IItem*  GetIItem() = 0;

	virtual void         SendEndReload() = 0;
	virtual bool         IsTargetOn() = 0;
	virtual void         OnReadyToFire() = 0;
	virtual void         OnStartReload(EntityId shooterId, IEntityClass* pAmmoType) = 0;
	virtual void         OnEndReload(EntityId shooterId, IEntityClass* pAmmoType) = 0;
	virtual void         OnMelee(EntityId shooterId) = 0;
	virtual void         OnZoomIn() = 0;
	virtual void         OnZoomOut() = 0;
	virtual void         OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel) = 0;
	virtual void         OnStartTargetting(IWeapon* pWeapon) = 0;
	virtual void         OnStopTargetting(IWeapon* pWeapon) = 0;

	virtual void         RequestStartFire() = 0;
	virtual void         RequestStopFire() = 0;
	virtual void         RequestReload() = 0;

	virtual void         RequestCancelReload() = 0;

	virtual void         RequestStartMeleeAttack(bool weaponMelee, bool boostedAttack, int8 attackIndex = -1) = 0;
	virtual void         RequestMeleeAttack(bool weaponMelee, const Vec3& pos, const Vec3& dir) = 0;

	virtual void         OnOutOfAmmo(IEntityClass* pAmmoType) = 0;
	virtual CProjectile* SpawnAmmo(IEntityClass* pAmmoType, bool remote = false) = 0;
	virtual int          GetInventoryAmmoCount(IEntityClass* pAmmoType) const = 0;
	virtual void         SetInventoryAmmoCount(IEntityClass* pAmmoType, int count) = 0;

	virtual int          GetMaxZoomSteps() = 0;

	virtual Vec3&        GetAimLocation() = 0;
	virtual Vec3&        GetTargetLocation() = 0;

	virtual void         SetAimLocation(Vec3& location) = 0;
	virtual void         SetTargetLocation(Vec3& location) = 0;

	virtual bool         AIUseEyeOffset() const = 0;
	virtual bool         AIUseOverrideOffset(EStance stance, float lean, float peekOver, Vec3& offset) const = 0;

	virtual void         AutoDrop() = 0;
	virtual void         AddFiredRocket() = 0;

	virtual bool         GetFireAlternation() = 0;
	virtual void         SetFireAlternation(bool fireAlt) = 0;

	virtual EntityId     GetHeldEntityId() const = 0;

	virtual void         ActivateTarget(bool activate) = 0;
	virtual bool         ApplyActorRecoil() const = 0;

	virtual bool         IsRippedOff() const = 0;

	//virtual void AdjustPosition(float time, bool zoomIn, int currentStep) = 0;
	//virtual void SetScopeOffset(const Vec3& offset) = 0;
};
