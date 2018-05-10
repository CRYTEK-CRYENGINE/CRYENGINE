// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _HAND_GRENADES_H_
#define _HAND_GRENADES_H_

#include "Weapon.h"
#include "IActionMapManager.h"

class CThrow;

class CHandGrenades : public CWeapon
{

public:
	CHandGrenades();
	virtual ~CHandGrenades();

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}

private:

	typedef CWeapon inherited;

	//------------ CWeapon -----------------------
	
	bool CanSelect() const;
	bool CanDeselect() const;
	bool OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAttackPrimary(EntityId actorId, const ActionId& actionId, int activationMode, float value) { return OnActionAttack(actorId, actionId, activationMode, value); }
	void StartSprint(CActor* pOwnerActor);
	bool ShouldSendOnShootHUDEvent() const;

	virtual void OnPickedUp(EntityId actorId, bool destroyed);
	virtual void OnDropped(EntityId actorId, bool ownerWasAI);
	virtual void OnSetAmmoCount(EntityId shooterId);
	virtual void OnSelected(bool selected);
	virtual void InitFireModes();

	virtual	uint32 StartDeselection(bool fastDeselect);

	virtual bool AllowInteraction(EntityId interactionEntity, EInteractionType interactionType);

	bool CanOwnerThrowGrenade() const;

	void FumbleGrenade();

	virtual void StartQuickGrenadeThrow();
	virtual void StopQuickGrenadeThrow();

	virtual void ForcePendingActions(uint8 blockedActions = 0);

	virtual bool CancelCharge();

	//------------ ~CWeapon -----------------------

	void UpdateStowedWeapons();

	CThrow* m_pThrow;

	int m_numStowedCopies;
	int m_stowSlot;

	bool m_quickThrowRequested;
	bool m_bInQuickThrow;
	bool m_throwCancelled;
};

#endif