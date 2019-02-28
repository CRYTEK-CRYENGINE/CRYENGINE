// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: C4 Implementation

-------------------------------------------------------------------------
History:
- 2:3:2006   16:05 : Created by Márcio Martins

*************************************************************************/
#ifndef __C4_H__
#define __C4_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"


class CC4 : public CWeapon
{
private:
	typedef CWeapon BaseClass;

public:
	CC4();
	virtual ~CC4();

	static const EEntityAspects ASPECT_DETONATE = eEA_GameServerStatic;

	virtual bool CanSelect() const;
	virtual void Update(SEntityUpdateContext& ctx, int update);
	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}

	virtual bool OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual bool OnActionAttackSecondary(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual bool OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void RequestDetonate();
	virtual void InitFireModes();
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual NetworkAspectType GetNetSerializeAspects();

	virtual void FullSerialize( TSerialize ser );
	virtual void PostSerialize();

	virtual void OnEnterFirstPerson();

	virtual void PickUp(EntityId picker, bool sound, bool select/* =true */, bool keepHistory/* =true */, const char *setup /* = NULL */);
	virtual void Drop(float impulseScale, bool selectNext, bool byDeath);
	virtual bool CanModify() const;

	void NetSetDetonateSwitch(bool detonate);
	bool NetGetDetonateSwitch() const { return m_detonateSwitch; };
	void NetSetCanDetonate(bool detonate);
	bool NetGetCanDetonate() const;
	void SetDiffuseAndGlow(const ColorF& newColour, const float& newGlow);

	IEntityClass* GetProjectileClass() const;

	DECLARE_SERVER_RMI_NOATTACH(SvRequestDetonate, DefaultParams, eNRT_ReliableOrdered);

protected:	

	virtual void OnUnlowerItem();

	void ResetAmmo(IEntityClass* pC4Ammo);
	void SpawnAndDropNewC4(IEntityClass* pC4Ammo, float impulseScale);

	int m_plantFM;
	int m_detonateFM;
	bool m_detonateSwitch;
	bool m_isArmed;
	bool m_postSerializing;

	IMaterial* m_pDetonatorArmedMaterial;

};

#endif // __C4_H__