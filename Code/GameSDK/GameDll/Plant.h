// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Throw Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 26ye:10:2005   15:45 : Created by Márcio Martins

*************************************************************************/
#ifndef __PLANT_H__
#define __PLANT_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "FireMode.h"
#include "ItemString.h"


class CPlant : public CFireMode
{
	struct StartPlantAction;
	struct MidPlantAction;
	struct EndPlantAction;

private:
	typedef CFireMode BaseClass;

public:
	CRY_DECLARE_GTI(CPlant);

	CPlant();
	virtual ~CPlant();

	virtual void PostInit() override;
	virtual void PostUpdate(float frameTime) override {}
	virtual void UpdateFPView(float frameTime) override;
	virtual void GetMemoryUsage(ICrySizer * s) const override;

	virtual void Activate(bool activate) override;

	virtual int GetAmmoCount() const override;
	virtual int GetClipSize() const override;

	virtual bool OutOfAmmo() const override;
	virtual bool LowAmmo(float thresholdPerCent) const override;
	virtual bool CanReload() const override { return false; }
	virtual void Reload(int zoomed) override {}
	virtual bool IsReloading(bool includePending=true) override { return false; }
	virtual void CancelReload() override {}
	virtual bool CanCancelReload() override { return false; }

	virtual bool AllowZoom() const override { return true; }
	virtual void Cancel() override {}

	virtual float GetRecoil() const override { return 0.0f; }
	virtual float GetSpread() const override { return 0.0f; }
	virtual float GetSpreadForHUD() const override { return 0.0f; }
	virtual float GetMinSpread() const override { return 0.0f; }
	virtual float GetMaxSpread() const override { return 0.0f; }
	virtual const char *GetCrosshair() const override { return ""; }

	virtual bool CanFire(bool considerAmmo=true) const override;
	virtual void StartFire() override;
	virtual void StopFire() override;
	virtual bool IsFiring() const override { return m_planting; }
	virtual bool IsSilenced() const override;

	virtual void NetShoot(const Vec3 &hit, int ph) override;
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph) override;
	virtual void NetEndReload() override {}

	virtual void NetStartFire() override;
	virtual void NetStopFire() override;

	virtual EntityId GetProjectileId() const override;
	virtual void SetProjectileId(EntityId id) override;
	virtual EntityId RemoveProjectileId() override;
	int GetNumProjectiles() const;

	virtual IEntityClass* GetAmmoType() const override;
	virtual int GetDamage() const override;

	virtual float GetSpinUpTime() const override { return 0.0f; }
	virtual float GetNextShotTime() const override { return 0.0f; }
	virtual void SetNextShotTime(float time) override {}
	virtual float GetFireRate() const override { return 0.0f; }

	virtual Vec3 GetFiringPos(const Vec3 &probableHit) const override { return ZERO;}
	virtual Vec3 GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const override { return ZERO;}

	virtual bool HasFireHelper() const override { return false; }
	virtual Vec3 GetFireHelperPos() const override { return Vec3(ZERO); }
	virtual Vec3 GetFireHelperDir() const override { return FORWARD_DIRECTION; }

	virtual int GetCurrentBarrel() const override { return 0; }
	virtual void Serialize(TSerialize ser) override;
	virtual void PostSerialize() override {}

	virtual void OnZoomStateChanged() override;
	virtual void CheckAmmo();

protected:

	virtual void Plant(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, bool net=false, int ph=0);
	virtual bool GetPlantingParameters(Vec3& pos, Vec3& dir, Vec3& vel) const;

	void CacheAmmoGeometry();

	std::vector<EntityId> m_projectiles;

	bool		m_planting;
	bool		m_pressing;

};

#endif 
