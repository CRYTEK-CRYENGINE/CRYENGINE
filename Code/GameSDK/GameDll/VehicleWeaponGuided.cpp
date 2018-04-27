// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 20:04:2006   13:02 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "VehicleWeaponGuided.h"
#include "Actor.h"
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include "Single.h"
#include "GameCVars.h"
#include "Player.h"
#include "Game.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

//------------------------------------------------------------------------
CVehicleWeaponGuided::CVehicleWeaponGuided() 
	: CVehicleWeapon()
	, m_State(eWGS_INVALID)
	, m_NextState(eWGS_INVALID)
	, m_DesiredHomingTarget(ZERO)
	, m_firedTimer(0.f)
	, m_pVehicleAnim(nullptr)
	, m_PreState(-1)
	, m_PostState(-1)
{ 
}

void CVehicleWeaponGuided::ReadProperties(IScriptTable *pScriptTable)
{
	CVehicleWeapon::ReadProperties(pScriptTable);



}


//------------------------------------------------------------------------
bool CVehicleWeaponGuided::Init( IGameObject * pGameObject )
{
  return CVehicleWeapon::Init(pGameObject);
}

//------------------------------------------------------------------------
void CVehicleWeaponGuided::PostInit( IGameObject * pGameObject )
{
  CVehicleWeapon::PostInit(pGameObject); 
}

//------------------------------------------------------------------------
void CVehicleWeaponGuided::Reset()
{
  CVehicleWeapon::Reset();
}

void CVehicleWeaponGuided::StartFire()
{
	IVehicle* pVehicle = NULL;

	if(!m_vehicleId && GetEntity()->GetParent())
	{
		m_vehicleId = GetEntity()->GetParent()->GetId();
		pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
		CRY_ASSERT(pVehicle && "Using VehicleWeapons on non-vehicles may lead to unexpected behavior.");

		if(m_weaponsharedparams->pVehicleGuided)
		{
			m_pVehicleAnim = pVehicle->GetAnimation(m_weaponsharedparams->pVehicleGuided->animationName.c_str());

			CRY_ASSERT(m_pVehicleAnim);

			m_PreState	= m_pVehicleAnim->GetStateId(m_weaponsharedparams->pVehicleGuided->preState.c_str());
			m_PostState	= m_pVehicleAnim->GetStateId(m_weaponsharedparams->pVehicleGuided->postState.c_str());
		}
		else
		{
			m_pVehicleAnim	=	NULL;
			m_PreState			= -1;
			m_PostState			= -1;
		}
	}
	else
	{
		pVehicle = GetVehicle();
	}

	if (pVehicle && !gEnv->bMultiplayer)
	{
		m_destination = pVehicle->GetEntity()->GetPos() + pVehicle->GetEntity()->GetRotation().GetRow1() * 25.0f;
	}

	if (eWGS_INVALID == m_State)
	{
		m_State = eWGS_PREPARATION;
	}

	EnableUpdate(true, eIUS_General);
	RequireUpdate(eIUS_General);
}

void CVehicleWeaponGuided::SetDestination(const Vec3& pos)
{ 
	if(gEnv->bMultiplayer)
	{
		m_destination = pos;
	}
	else
	{
		m_DesiredHomingTarget = pos; 
    m_destination = pos;

		IVehicle *pVehicle = GetVehicle();
    if (pVehicle)
		{
			Vec3 offset = pos - pVehicle->GetEntity()->GetPos();
			offset.Normalize();

			//m_DesiredHomingTarget += offset * 50.0f;
		}
	}
}

const Vec3& CVehicleWeaponGuided::GetDestination()
{ 
	return m_destination; 
}

//Vec3 CVehicleWeaponGuided::GetSlotHelperPos(int slot, const char *helper, bool worldSpace, bool relative) const
//{
//	return ZERO''
//}

void CVehicleWeaponGuided::Update(SEntityUpdateContext& ctx, int update)
{
	switch(m_State)
	{
	case eWGS_INVALID:
		break;
	case eWGS_PREPARATION:
		{
			if(!m_vehicleId && GetEntity()->GetParent())
			{
				m_vehicleId = GetEntity()->GetParent()->GetId();
				CRY_ASSERT(gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId) && "Using VehicleWeapons on non-vehicles may lead to unexpected behavior.");
			}

			IVehicle* pVehicle = GetVehicle();
			if (pVehicle && m_weaponsharedparams->pVehicleGuided)
			{
				m_pVehicleAnim = pVehicle->GetAnimation(m_weaponsharedparams->pVehicleGuided->animationName.c_str());

				CRY_ASSERT(m_pVehicleAnim);

				m_PreState = m_pVehicleAnim->GetStateId(m_weaponsharedparams->pVehicleGuided->preState.c_str());
				m_PostState = m_pVehicleAnim->GetStateId(m_weaponsharedparams->pVehicleGuided->postState.c_str());
				m_pVehicleAnim->ChangeState(m_PreState);
				m_pVehicleAnim->StartAnimation(); 
				m_State = eWGS_WAIT;
				m_NextState = eWGS_FIRE;
			}
			else
			{
				m_State = eWGS_FIRE;
				m_NextState = eWGS_FIRE;
			}
		}
		break;
	case eWGS_FIRE:
		CVehicleWeapon::StartFire();
		m_State = (m_weaponsharedparams->pVehicleGuided && m_weaponsharedparams->pVehicleGuided->waitWhileFiring) ? eWGS_FIRING : eWGS_POSTSTATE;
		m_firedTimer = 0.f;
		break;
	case eWGS_FIRING:
		if(!m_fm || !m_fm->IsFiring())
		{
			m_State = eWGS_POSTSTATE;
		}
		break;
	case eWGS_POSTSTATE:
		{
			float firedTimer = m_firedTimer;

			firedTimer += ctx.fFrameTime;

      if (m_weaponsharedparams->pVehicleGuided)
      {
			  if(firedTimer >= m_weaponsharedparams->pVehicleGuided->postStateWaitTime)
			  {
				  m_pVehicleAnim->ChangeState(m_PostState); 
				  m_pVehicleAnim->StartAnimation();
				  m_State = eWGS_WAIT;
				  m_NextState = eWGS_INVALID;
			  }
      }
      else
      {
        m_State = eWGS_INVALID;
        m_NextState = eWGS_INVALID;
      }

			m_firedTimer = firedTimer;
		}
		break;
	case eWGS_WAIT:
		if (m_pVehicleAnim->GetAnimTime(true) > 1.0f - FLT_EPSILON)
		{
			m_State = m_NextState;
		}
		break;
	}

	if(!gEnv->bMultiplayer)
	{
		m_destination.SetLerp(m_destination, m_DesiredHomingTarget, ctx.fFrameTime);
	}
	
	CVehicleWeapon::Update(ctx, update);
}