// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Charge.h"
#include "Item.h"
#include "Weapon.h"
#include "Projectile.h"

#include "WeaponSharedParams.h"



CRY_IMPLEMENT_GTI(CCharge, CAutomatic);


//------------------------------------------------------------------------
CCharge::CCharge()
	: m_charged(0)
	, m_charging(false)
	, m_chargeTimer(0.0f)
	, m_autoreload(false)
	, m_chargeEffectId(0)
	, m_chargedEffectTimer(0.0f)
{
}

//------------------------------------------------------------------------
CCharge::~CCharge()
{
}

//----------------------------------------"--------------------------------
void CCharge::Update(float frameTime, uint32 frameId)
{
	if (m_charging)
	{
		if (m_chargeTimer>0.0f)
		{
			m_chargeTimer -= frameTime;
			if (m_chargeTimer<=0.0f)
			{
				m_charged++;
				if (m_charged >= m_fireParams->chargeparams.max_charges)
				{
					m_charging = false;
					m_charged = m_fireParams->chargeparams.max_charges;
					if (!m_fireParams->chargeparams.shoot_on_stop)
					{
						m_pWeapon->SetBusy(false);
						ChargedShoot();
					}
				}
			}
		}

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
	else
	{
		if (!m_fireParams->chargeparams.shoot_on_stop)
		{
			CAutomatic::Update(frameTime, frameId);
		}
		else
		{
			CSingle::Update(frameTime, frameId);
		}
	}

	// update spinup effect
	if (m_chargedEffectTimer>0.0f)
	{
		m_chargedEffectTimer -= frameTime;
		if (m_chargedEffectTimer <= 0.0f)
		{
			m_chargedEffectTimer = 0.0f;
			if (m_chargeEffectId)
				ChargeEffect(false);
		}

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CCharge::Activate(bool activate)
{
	CAutomatic::Activate(activate);

	ChargeEffect(0);

	m_charged=0;
	m_charging=false;
	m_chargeTimer=0.0;

	if(activate == false)
	{
		m_pWeapon->SetItemFlag(CItem::eIF_BlockActions, false);
	}
}

//------------------------------------------------------------------------
void CCharge::StartFire()
{
	if(m_pWeapon->AreAnyItemFlagsSet(CItem::eIF_BlockActions) || m_pWeapon->IsBusy() || m_next_shot > 0.f)
	{
		return;
	}

	CAutomatic::StartFire();
	m_pWeapon->SetItemFlag(CItem::eIF_BlockActions, true);
}

//------------------------------------------------------------------------
void CCharge::StopFire()
{
	if (m_fireParams->chargeparams.shoot_on_stop)
	{
		if (m_charged > 0)
		{
			ChargedShoot();
		}
		m_pWeapon->PlayAction(GetFragmentIds().uncharge);
		m_charged = 0;
		m_charging = false;
		m_chargeTimer = 0.0f;
	}

	CAutomatic::StopFire();
}

//------------------------------------------------------------------------
bool CCharge::Shoot(bool resetAnimation, bool autoreload /* =true */, bool isRemote)
{
	m_autoreload = autoreload;

	if (!m_charged)
	{
		m_charging = true;
		m_chargeTimer = m_fireParams->chargeparams.time;
		m_pWeapon->PlayAction(GetFragmentIds().charge,  0, false, CItem::eIPAF_Default);

		ChargeEffect(true);
	}
	else if (!m_charging && m_firing)
		ChargedShoot();

	m_pWeapon->RequireUpdate(eIUS_FireMode);

	return true;
}

//------------------------------------------------------------------------
void CCharge::ChargedShoot()
{
	CAutomatic::Shoot(true, m_autoreload);

	m_charged=0;

	if(m_fireParams->chargeparams.reset_spinup)
		StopFire();

	m_pWeapon->SetItemFlag(CItem::eIF_BlockActions, false);
}

//------------------------------------------------------------------------
void CCharge::ChargeEffect(bool attach)
{
	m_pWeapon->DetachEffect(m_chargeEffectId);
	m_chargeEffectId = 0;

	if (attach)
	{
		int slot = m_pWeapon->GetStats().fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		m_chargeEffectId = m_pWeapon->AttachEffect(slot, m_fireParams->chargeeffect.helperFromAccessory, m_fireParams->chargeeffect.effect[id].c_str(), 
			m_fireParams->chargeeffect.helper[id].c_str(), Vec3(0,0,0), Vec3(0,1,0), 1.0f, false);

		m_chargedEffectTimer = (float)(m_fireParams->chargeeffect.time[id]);
	}
}

//-----------------------------------------------
void CCharge::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));	
	CAutomatic::GetInternalMemoryUsage(s);	// collect memory of parent class
}
