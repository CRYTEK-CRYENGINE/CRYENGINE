// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a usable action to recover a vehicle by flipping it over

   -------------------------------------------------------------------------
   History:
   - Created by Stan Fichele

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "GameObjects/GameObject.h"
#include "IActorSystem.h"
#include "IGameObject.h"
#include "IItem.h"
#include "IItemSystem.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleUsableActionFlip.h"

static const float s_postOrientTime = 0.5f;
static const float s_reorientCutOffDot = 0.707f;

//------------------------------------------------------------------------
CVehicleUsableActionFlip::CVehicleUsableActionFlip()
	: m_pVehicle(nullptr)
	, m_postReorientedTimer(0.0f)
	, m_localAngVel(ZERO)
	, m_timer(0.0f)
{
}

bool CVehicleUsableActionFlip::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = static_cast<CVehicle*>(pVehicle);
	return 1;
}

void CVehicleUsableActionFlip::Reset()
{
	// Inform the vehicle that the flipping has stopped
	SVehicleEventParams params;
	params.bParam = false;
	m_pVehicle->BroadcastVehicleEvent(eVE_BeingFlipped, params);
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
	m_timer = 0.f;
	m_postReorientedTimer = 0.f;
}

//------------------------------------------------------------------------
int CVehicleUsableActionFlip::OnEvent(int eventType, SVehicleEventParams& eventParams)
{
	if (IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics())
	{
		if (eventType == eVAE_IsUsable)
		{
			return 1;
		}
		else if (eventType == eVAE_OnUsed)
		{
			if (m_pVehicle->IsFlipped())
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(eventParams.entityId);
				if (pEntity)
				{
					static const float speed = 3.f;

					// Decide which axis to turn around
					pe_status_pos physPos;
					pe_status_dynamics physDyn;
					pPhysics->GetStatus(&physPos);
					pPhysics->GetStatus(&physDyn);
					const Vec3 xAxis = physPos.q.GetColumn0();
					const Vec3 yAxis = physPos.q.GetColumn1();

					if (fabsf(yAxis.z) > 0.6f)
					{
						// Standing on its nose or tail
						// Rotate about the x
						m_localAngVel.Set(speed * (float)__fsel(yAxis.z, -1.f, +1.f), 0.f, 0.f);
					}
					else
					{
						// Rotate about the y (away from the user)
						const Vec3 centre = physPos.pos + (physPos.BBox[0] + physPos.BBox[1]) * 0.5f;
						const Vec3 entityPos = pEntity->GetWorldPos();
						m_localAngVel.Set(0.f, speed * (float)__fsel((entityPos - centre).dot(xAxis), +1.f, -1.f), 0.f);
					}

					// Start the update
					m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

					// Inform the vehicle that flipping has started
					SVehicleEventParams params;
					params.bParam = true;
					m_pVehicle->BroadcastVehicleEvent(eVE_BeingFlipped, params);
					m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
				}
			}
			return 0;
		}
	}

	return 0;
}

void CVehicleUsableActionFlip::Update(const float deltaTime)
{
	if (IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics())
	{
		pe_status_pos physPos;
		pe_status_dynamics physDyn;
		pPhysics->GetStatus(&physPos);
		pPhysics->GetStatus(&physDyn);
		const Vec3 zAxis = physPos.q.GetColumn2();
		const Vec3 centre = physPos.pos + (physPos.BBox[0] + physPos.BBox[1]) * 0.5f;
		const Vec3 angVel = physPos.q * m_localAngVel;
		const Vec3 vel = angVel.cross(physDyn.centerOfMass - centre);
		pe_action_set_velocity setVelocity;
		setVelocity.v = vel;
		setVelocity.w = angVel;
		pPhysics->Action(&setVelocity);

		if (zAxis.z > 0.1f)
		{
			m_postReorientedTimer += deltaTime;
		}
		else
		{
			m_postReorientedTimer = 0.0f;
		}
		m_timer += deltaTime;
		if ((m_timer > 5.f) || ((zAxis.z > 0.1f) && (m_postReorientedTimer > s_postOrientTime)) || (zAxis.z > s_reorientCutOffDot))
		{
			// Stop
			Reset();
		}
	}
	else
	{
		Reset();
	}

}

void CVehicleUsableActionFlip::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
}

DEFINE_VEHICLEOBJECT(CVehicleUsableActionFlip);
