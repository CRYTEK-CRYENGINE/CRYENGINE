// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Rapid Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 26:10:2005   14:15 : Created by Márcio Martins

*************************************************************************/
#ifndef __RAPID_H__
#define __RAPID_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Single.h"
#include "ICryMannequin.h"

class CRapid : public CSingle
{
private:
	
	typedef CSingle BaseClass;

public:
	CRY_DECLARE_GTI(CRapid);

	enum ERapidFlag
	{
		eRapidFlag_none					 = (0),
		eRapidFlag_accelerating  = (1 << 0),
		eRapidFlag_decelerating  = (1 << 1),
		eRapidFlag_netShooting	 = (1 << 2),
		eRapidFlag_rapidFiring	 = (1 << 3),
		eRapidFlag_startedFiring = (1 << 4),
	};

	CRapid();

	// CSingle
	virtual void Update(float frameTime, uint32 frameId) override;

	virtual void GetMemoryUsage(ICrySizer * s) const override;

	virtual void Activate(bool activate) override;

	virtual void StartReload(int zoomed) override;

	virtual void StartFire() override;
	virtual void StopFire() override;
	virtual bool IsFiring() const override { return m_firing || (m_rapidFlags & eRapidFlag_accelerating); };

	virtual void NetStartFire() override;
	virtual void NetStopFire() override;

	virtual float GetSpinUpTime() const override;
	virtual bool AllowZoom() const override;

	// ~CSingle

	float GetAmmoSoundParam();

protected:
	virtual int GetShootAmmoCost(bool playerIsShooter) override;
	void Accelerate(float acc);
	void Firing(bool firing);
	void UpdateRotation(float frameTime);
	void UpdateFiring(CActor* pOwnerActor, bool ownerIsClient, bool ownerIsPlayer, float frameTime);
	void FinishDeceleration();
	void PlayRapidFire(float speedOverride, bool concentratedFire);
	void PlayStopRapidFireIfNeeded();

  CTimeValue m_startFiringTime;
	_smart_ptr<IAction> m_pBarrelSpinAction;
	bool m_queueRapidFireAction;

	float	m_speed;
	float	m_acceleration;
	float m_rotation_angle;

	uint32 m_rapidFlags;
};

class CRapidFireAction : public TFiremodeAction<CRapid>
{
private:
	typedef TFiremodeAction<CRapid> BaseClass;

public:

	DEFINE_ACTION("RapidFireAction");

	CRapidFireAction( int priority, FragmentID fragmentID, CRapid* pRapid)
		: BaseClass(priority, fragmentID, pRapid)
	{
	}

	virtual EStatus Update(float timePassed) override;

	virtual EPriorityComparison ComparePriority(const IAction &actionCurrent) const override
	{
		return (IAction::Installed == actionCurrent.GetStatus() && IAction::Installing & ~actionCurrent.GetFlags()) ? Higher : BaseClass::ComparePriority(actionCurrent);
	}

private:
};

#endif