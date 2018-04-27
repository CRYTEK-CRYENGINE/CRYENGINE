// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Basic Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 26:02:2010   12:00 : Created by Claire Allan

*************************************************************************/

#pragma once

#ifndef __FIREMODE_H__
#define __FIREMODE_H__

#include "Weapon.h"
#include "ItemString.h"
#include "FireModePluginParams.h"
#include "GameTypeInfo.h"
#include "ItemAnimation.h"

#include "ICryMannequin.h"

class CWeapon;
struct SFireModeParams;
struct SParentFireModeParams;
class IFireModePlugin;
class CPlayer;

struct IFireModeListener
{
	virtual ~IFireModeListener(){}
	virtual void OnFireModeDeleted() = 0;
	virtual void OnFireModeActivated(bool activated) = 0;
};

class CFireMode : public IFireMode
{
public:
	CRY_DECLARE_GTI_BASE(CFireMode);

	CFireMode();
	virtual ~CFireMode();

	virtual void InitFireMode(IWeapon* pWeapon, const SParentFireModeParams* pParams);

	virtual void Release();

	void ResetSharedParams(const SFireModeParams* pParams) { m_fireParams = pParams; ResetParams(); }
	ILINE const SFireModeParams* GetShared() const { return m_fireParams; }
	ILINE const SParentFireModeParams* GetParentShared() const { return m_parentFireParams; }

	virtual void ResetParams();

	virtual void PatchSpreadMod(const SSpreadModParams &sSMP, float modMultiplier);
	virtual void ResetSpreadMod();

	virtual void PatchRecoilMod(const SRecoilModParams &sRMP, float modMultiplier);
	virtual void ResetRecoilMod();

	virtual void Update(float frameTime, uint32 frameId);
	virtual void Activate(bool activate);

	virtual bool CanOverheat() const;
	virtual float GetHeat() const;

	virtual Vec3 ApplySpread(const Vec3 &dir, float spread, int quadrant = -1) const { return dir; }
	virtual void ApplyAutoAim(Vec3 &rDir, const Vec3 &pos) const {}

	virtual bool CanZoom() const;
	virtual float GetRange() const;
	virtual bool FillAmmo(bool fromInventory);
	virtual int GetClipSize() const;
	virtual int GetChamberSize() const;
	virtual void OnEnterFirstPerson();

	virtual void StopPendingFire() {};
	
	virtual void SetProjectileSpeedScale(float fSpeedScale);

	virtual void SetName(const char *name) { m_name = name; }
	virtual const char *GetName()const { return m_name.c_str(); }

	//------------------------------------------------------------------------
	ILINE void Enable(bool enable)
	{
		m_enabled = enable;
		m_accessoryEnabled = false;
	}

	//------------------------------------------------------------------------
	ILINE bool IsEnabled() const
	{
		return m_enabled;
	}

	//------------------------------------------------------------------------
	ILINE void EnableByAccessory(bool enable)
	{
		m_enabled = enable;
		m_accessoryEnabled = enable;
	}

	//------------------------------------------------------------------------
	ILINE bool IsEnabledByAccessory()
	{
		return m_accessoryEnabled;
	}

	virtual void GetMemoryUsage(ICrySizer * pSizer) const;

	void GetInternalMemoryUsage(ICrySizer * pSizer) const;

	virtual bool Fired() const {return false;}
	virtual bool FirstFire() const {return false;}
	virtual bool IsCharging() const {return false;}
	virtual void NetSetCharging(bool charging) {}

	CWeapon* GetWeapon() {return m_pWeapon;}
	const CWeapon* GetWeapon() const {return m_pWeapon;}
	const SMannequinItemParams& GetMannequinItemParams() const {return GetWeapon()->GetMannequinItemParams();}
	const SMannequinItemParams::FragmentIDs& GetFragmentIds() const {return GetWeapon()->GetFragmentIds();}
	void UpdateMannequinTags(bool enable);

	void RegisterListener(IFireModeListener *pListener)
	{
		m_listeners.Add(pListener);
	}

	void UnregisterListener(IFireModeListener *pListener)
	{
		m_listeners.Remove(pListener);
	}

	float GetTimeBetweenShotsMultiplier(const CPlayer* pPlayer) const;

	virtual uint8 GetShotIncrementAmount() { return 1; }
	virtual void OnHostMigrationCompleted() {}

#ifndef _RELEASE
	virtual void DebugUpdate(float frameTime) const {}
#endif

protected:
	virtual void OnSpawnProjectile(CProjectile* pAmmo);

	bool PluginsAllowFire() const;
	virtual void OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);
	void OnReplayShoot();
	void AlterFiringDirection(const Vec3& firingPos, Vec3& rFiringDir) const;
	const IFireModePlugin* FindPluginOfType(const CGameTypeInfo* pluginType) const;

	CWeapon*											m_pWeapon;
	const SFireModeParams*				m_fireParams;
	
	bool													m_enabled;

private:
	const SParentFireModeParams*	m_parentFireParams;
	ItemString										m_name;
	bool													m_accessoryEnabled;

	typedef std::vector<IFireModePlugin*> TFireModePluginVector;

	typedef CListenerSet<IFireModeListener*> TFireModeListeners;
	TFireModeListeners m_listeners;

	TFireModePluginVector m_plugins;

};

template <class FIREMODE>
class TFiremodeAction : public TAction<SAnimationContext>, public IFireModeListener
{
private:
	typedef TAction<SAnimationContext> BaseClass;

public:
	TFiremodeAction(int priority, FragmentID fragmentID, FIREMODE *pFiremode, const TagState fragTags = TAG_STATE_EMPTY)
		:
		BaseClass(priority, fragmentID, fragTags),
		m_pFireMode(pFiremode)
	{
		pFiremode->RegisterListener(this);
	}

	~TFiremodeAction()
	{
		if (m_pFireMode)
		{
			m_pFireMode->UnregisterListener(this);
		}
	}

	virtual void OnFireModeDeleted()
	{
		m_eStatus =  IAction::Finished;
		m_pFireMode = NULL;
	}
	virtual void OnFireModeActivated(bool activated)
	{
	}

protected:
	FIREMODE *m_pFireMode;
};

#endif //__FIREMODE_H__
