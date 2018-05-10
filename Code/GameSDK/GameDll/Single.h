// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Single-shot Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 11:9:2004   15:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __SINGLE_H__
#define __SINGLE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Weapon.h"
#include "TracerManager.h"
#include "GameParameters.h"
#include "Recoil.h"
#include "MuzzleEffect.h"
#include "WeaponSharedParams.h"
#include "FireMode.h"
#include "AI/HazardModule/HazardShared.h"


#define WEAPON_HIT_RANGE				(250.0f)
#define WEAPON_HIT_MIN_DISTANCE	(1.5f)

#define MAX_PROBABLE_HITS				(16)

struct SAmmoParams;

enum EProbableHitDeferredState
{
	eProbableHitDeferredState_none,
	eProbableHitDeferredState_dispatched,
	eProbableHitDeferredState_done,
};

struct SProbableHitInfo
{
	QueuedRayID					m_queuedRayID;
	Vec3						m_hit;
	EProbableHitDeferredState	m_state;

	SProbableHitInfo();
	~SProbableHitInfo();

	void OnDataReceived(const QueuedRayID& rayID, const RayCastResult& result);
	void CancelPendingRay();		
};

class CSingle :
	public CFireMode
{
	struct FillAmmoAction;
	struct EndReloadAction;
	class ScheduleAutoReload;

private:
	typedef CFireMode BaseClass;

public:
	CRY_DECLARE_GTI(CSingle);

	CSingle();
	virtual ~CSingle();

	//IFireMode
	virtual void PostInit() override;
	virtual void Update(float frameTime, uint32 frameId) override;
	virtual void UpdateFPView(float frameTime) override;
	virtual void GetMemoryUsage(ICrySizer * s) const override;
	void GetInternalMemoryUsage(ICrySizer * s) const;
	virtual void ResetParams() override;

	virtual void Activate(bool activate) override;
	virtual void OnEnterFirstPerson() override;

	virtual int GetAmmoCount() const override;
	virtual int GetClipSize() const override;
	virtual int GetChamberSize() const override;

	virtual bool OutOfAmmo() const override;
	virtual bool LowAmmo(float thresholdPerCent) const override;
	virtual bool CanReload() const override;
	virtual void Reload(int zoomed) override;
	virtual void CancelReload() override;
	virtual bool CanCancelReload() override { return true;};

	virtual bool CanFire(bool considerAmmo = true) const override;
	virtual void StartFire() override;
	virtual void StopFire() override;
	virtual void StopPendingFire() override;
	virtual bool IsFiring() const override { return m_firing; };
	virtual bool Fired() const override {return m_fired;}
	virtual bool FirstFire() const override {return m_firstFire;}
	virtual bool IsSilenced() const override;
	virtual void SetProjectileSpeedScale(float fSpeedScale) override;
	
	virtual bool AllowZoom() const override;
	virtual void Cancel() override;

	virtual void NetShoot(const Vec3 &hit, int predictionHandle) override;
	virtual void NetShootDeferred(const Vec3 &inHit);
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle) override;
	virtual void NetEndReload() override { m_reloadPending=false; }
	virtual void ReplayShoot() override;

	virtual void NetStartFire() override {}
	virtual void NetStopFire() override {}

	virtual bool IsReadyToFire() const { return CanFire(true); };

	virtual EntityId GetProjectileId() const override { return m_projectileId; }
	virtual EntityId RemoveProjectileId() override;
	virtual void SetProjectileId(EntityId id) override { m_projectileId = id; }

	virtual IEntityClass* GetAmmoType() const override;

	virtual float GetSpinUpTime() const override;
	virtual float GetNextShotTime() const override;
	virtual void SetNextShotTime(float time) override;
	virtual float GetFireRate() const override;
	
  virtual bool HasFireHelper() const override;
  virtual Vec3 GetFireHelperPos() const override;
  virtual Vec3 GetFireHelperDir() const override;

  virtual int GetCurrentBarrel() const override { return m_barrelId; }
	virtual void Serialize(TSerialize ser) override
	{ 
		if(ser.GetSerializationTarget() != eST_Network)
		{
			ser.BeginGroup("firemode");
			ser.Value("enabled", m_enabled);
			ser.Value("nextShot", m_next_shot);
			ser.EndGroup();
			if(ser.IsReading())
				m_saved_next_shot = m_next_shot;
			m_HazardID.Serialize(ser);
		}
	};

	virtual void PostSerialize() override
	{
		SetNextShotTime(m_saved_next_shot);
	};

	void PatchSpreadMod(const SSpreadModParams &sSMP, float modMultiplier) override;
	void ResetSpreadMod() override;
	void PatchRecoilMod(const SRecoilModParams &sRMP, float modMultiplier) override;
	void ResetRecoilMod() override;

	virtual void OnZoomStateChanged() override;
	virtual void SetProjectileLaunchParams(const SProjectileLaunchParams &launchParams) override;
  //~IFireMode

	//CFireMode
	virtual void InitFireMode(IWeapon* pWeapon, const SParentFireModeParams* pParams) override;
	//~CFireMode

	virtual void StartReload(int zoomed);
	virtual void EndReload(int zoomed);
	virtual bool IsReloading(bool includePending=true) override;
	virtual bool Shoot(bool resetAnimation, bool autoreload, bool isRemote=false);
	virtual bool FillAmmo(bool fromInventory) override;

	virtual bool ShootFromHelper(const Vec3 &eyepos, const Vec3 &probableHit) const;
	virtual Vec3 GetProbableHit(float maxRayLength, bool *pbHit=0, ray_hit *hit=0) const;
	virtual	void DeferGetProbableHit(float maxRayLength);
	virtual Vec3 GetFiringPos(const Vec3 &probableHit) const override;
	virtual Vec3 GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const override;
	virtual Vec3 GetFiringVelocity(const Vec3& dir) const;
	virtual Vec3 ApplySpread(const Vec3 &dir, float spread, int quadrant = -1) const override;
	virtual void ApplyAutoAim(Vec3 &rDir, const Vec3 &pos) const override;

	virtual Vec3 GetTracerPos(const Vec3 &firingPos, const STracerParams* useTracerParams);

	virtual int GetDamage() const override;

	float GetRecoil() const override {return m_recoil.GetRecoil();}
	virtual float GetSpread() const override {return m_recoil.GetSpread();}
	virtual float GetSpreadForHUD() const override { return GetSpread(); }
	float GetMinSpread() const override {return m_recoil.GetMinSpread();}
	float GetMaxSpread() const override {return m_recoil.GetMaxSpread();}
	virtual void SmokeEffect(bool effect=true);
	virtual void SpinUpEffect(bool attach);
  virtual void RecoilImpulse(const Vec3& firingPos, const Vec3& firingDir);

	// recoil/spread
	virtual void ResetRecoil(bool spread=true) {m_recoil.Reset(spread);}

	virtual void PostUpdate(float frameTime) override {}

	virtual float GetProjectileFiringAngle(float v, float g, float x, float y);

	static void GetSkipEntities(CWeapon* pWeapon, PhysSkipList& skipList);
	float GetFireAnimationWeight() const;

	virtual void OnHostMigrationCompleted() override;

#ifndef _RELEASE
	virtual void DebugUpdate(float frameTime) const override;
#endif

protected:

	bool DoesFireRayIntersectFrustum(const Vec3& vPos, bool& firePosInFrustum);
	bool DoesFireLineSegmentIntersectFrustum(const Vec3& start, const Vec3& end);
	
	virtual void CheckNearMisses(const Vec3 &probableHit, const Vec3 &pos, const Vec3 &dir, float range, float radius);
	void CacheAmmoGeometry();

	const SAmmoParams* GetAmmoParams() const;

	void EmitTracer(const Vec3& pos,const Vec3& destination, const STracerParams * useTracerParams, CProjectile* pProjectile);

	void UpdateFireAnimationWeight(float frameTime);
	bool DampRecoilEffects() const;
	
	float GetFireFFeedbackWeight() const;

	virtual void PlayShootAction(int ammoCount);
	virtual int GetShootAmmoCost(bool playerIsShooter);

	virtual float CalculateSpreadMultiplier(CActor* pOwnerActor) const;
	float CalculateRecoilMultiplier(CActor* pOwnerActor) const;

	bool IsProceduralRecoilEnabled() const { return m_fireParams->proceduralRecoilParams.enabled; }
	virtual void SetReloadFragmentTags(CTagState& fragTags, int ammoCount);

	float GetReloadSpeedMultiplier(const CActor* pOwnerActor) const;

	void OnOutOfAmmo(const CActor* pActor, bool autoReload);

	bool			m_fired;
	bool			m_firstFire;
	bool			m_firing;
	bool			m_reloading;
	bool			m_emptyclip;

	float			m_next_shot_dt;
	float			m_next_shot;
	float     m_saved_next_shot; //For serialization
	short     m_barrelId;

	EntityId	m_projectileId;

	EntityEffects::TAttachedEffectId m_spinUpEffectId;
	float			m_spinUpTimer;		

	float			m_speed_scale;

	uint32						m_ammoid;

	float						m_spinUpTime;

	CRecoil			m_recoil;
	CMuzzleEffect	m_muzzleEffect;

	EntityEffects::TAttachedEffectId m_smokeEffectId;

	float						m_fireAnimationWeight;

	int							m_reloadStartFrame;

	bool						m_reloadCancelled;
	bool						m_reloadPending;
	bool						m_autoReloading;
	bool						m_firePending;
	bool						m_cocking;

	SProbableHitInfo	m_probableHits[MAX_PROBABLE_HITS];
	CryFixedArray<SProbableHitInfo*, MAX_PROBABLE_HITS>		m_queuedProbableHits;

private:
	// The hazard ID of the area in front of the weapon (undefined if 
	// none has been registered).
	HazardSystem::HazardProjectileID m_HazardID;

private:
	// Hazard management:
	const SHazardDescriptor*    GetHazardDescriptor() const;
	void                        RetrieveHazardAreaPoseInFrontOfWeapon (Vec3* hazardStartPos, Vec3* hazardForwardNormal) const;
	bool                        ShouldGenerateHazardArea() const;
	void                        RegisterHazardAreaInFrontOfWeapon();
	void                        SyncHazardAreaInFrontOfWeapon();
	void                        UnregisterHazardAreaInFrontOfWeapon();
};

#endif //__SINGLE_H__
