// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Mounted machine gun that can be ripped off by the player
and move around with it
-------------------------------------------------------------------------
History:
- 20:01:2009: Created by Benito G.R.
  30:09:2009: Ported from Rippable turret gun

*************************************************************************/

#pragma once

#ifndef _HEAVY_MOUNTED_WEAPON_H_
#define _HEAVY_MOUNTED_WEAPON_H_

#include "HeavyWeapon.h"

class CHeavyMountedWeapon : public CHeavyWeapon
{
private:
	struct EndRippingOff;
	struct RemoveViewLimitsAction;

	typedef CHeavyWeapon BaseClass;

	static const EEntityAspects ASPECT_RIPOFF = eEA_GameServerStatic;

public:

	CHeavyMountedWeapon();
	virtual ~CHeavyMountedWeapon();

	//Common functions
	virtual void OnReset();
	virtual void UpdateFPView(float frameTime);
	virtual void Update(SEntityUpdateContext& ctx, int slot);
	virtual void FadeCrosshair(float to, float time, float delay = 0.0f);
	
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual NetworkAspectType GetNetSerializeAspects();
	virtual void InitClient(int channelId);
	virtual void PostInit( IGameObject * pGameObject );

	virtual void FullSerialize( TSerialize ser );
	virtual void PostSerialize();

	virtual void ReadProperties(IScriptTable *pScriptTable);

	//Input handling
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	//Use functions
	virtual bool CanPickUp(EntityId userId) const;
	virtual bool CanUse(EntityId userId) const;
	virtual void Use(EntityId userId);
	virtual void StartUse(EntityId userId);
	virtual void StopUse(EntityId userId);

	virtual bool CanDeselect() const;
	virtual bool AllowInteraction(EntityId interactionEntity, EInteractionType interactionType);

	//Mount/rip-off functions
	virtual bool IsRippedOff() const { return m_rippedOff; }
	virtual bool IsRippingOrRippedOff() const { return (m_rippingOff || m_rippedOff); }
	virtual bool CanRipOff() const;
	virtual void SetRippingOff(bool ripOff);
	virtual void ForceRippingOff(bool ripOff);
	void RemoveViewLimits();
	virtual bool IsRippingOff() const {return m_rippingOff;}

	virtual void OnOutOfAmmo(IEntityClass* pAmmoType);
	virtual void OnFireWhenOutOfAmmo();

	virtual void GetAngleLimits(EStance stance, float& minAngle, float& maxAngle);
	virtual bool UpdateAimAnims(SParams_WeaponFPAiming &aimAnimParams);
	virtual void UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis);

	virtual void ProcessEvent(const SEntityEvent& event);

	virtual void Select(bool select);

	virtual void FinishRipOff();
	void UnlinkMountedGun();

	virtual void OnBeginCutScene();
	virtual void OnEndCutScene();

	DECLARE_SERVER_RMI_NOATTACH(SvRequestRipOff, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClRipOff, BaseClass::SHeavyWeaponUserParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH(ClDropped, EmptyParams, eNRT_ReliableUnordered);
	
	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		GetInternalMemoryUsage(s);
	}
	void GetInternalMemoryUsage(ICrySizer * s) const
	{
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}

protected:

	virtual void PerformRipOff(CActor* pOwner);

private:

	void TryRipOffGun();
	void SetUnMountedConfiguration();
	//tSoundID PlayRotationSound();

	//Input handling
	void RegisterActionsHMG();
	bool OnActionRipOff(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	void SwitchToRippedOffFireMode();

	EntityId m_linkedParentId;		//To remember linked entity

	bool		m_rippedOff;
	bool		m_rippingOff;
	//tSoundID m_rotatingSoundID;
	float		m_RotationSoundTimeOut;
	float		m_lastXAngle;
	float		m_lastZAngle;
	int			m_lastUsedFrame;
};

#endif
