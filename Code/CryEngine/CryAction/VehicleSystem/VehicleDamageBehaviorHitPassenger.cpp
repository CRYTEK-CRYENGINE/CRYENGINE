// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorHitPassenger.h"

#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "IGameRulesSystem.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorHitPassenger::CVehicleDamageBehaviorHitPassenger()
	: m_pVehicle(nullptr)
	, m_damage(0)
	, m_isDamagePercent(false)
{}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorHitPassenger::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	CVehicleParams hitPassTable = table.findChild("HitPassenger");
	if (!hitPassTable)
		return false;

	if (!hitPassTable.getAttr("damage", m_damage))
		return false;

	if (!hitPassTable.getAttr("isDamagePercent", m_isDamagePercent))
		m_isDamagePercent = false;

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorHitPassenger::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event != eVDBE_Hit)
		return;

	IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
	CRY_ASSERT(pActorSystem);

	TVehicleSeatId lastSeatId = m_pVehicle->GetLastSeatId();
	for (TVehicleSeatId seatId = FirstVehicleSeatId; seatId <= lastSeatId; ++seatId)
	{
		if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(seatId))
		{
			EntityId passengerId = pSeat->GetPassenger();
			if (IActor* pActor = pActorSystem->GetActor(passengerId))
			{
				int damage = m_damage;

				if (m_isDamagePercent)
				{
					damage = (int)(pActor->GetMaxHealth() * float(m_damage) / 100.f);
				}

				if (gEnv->bServer)
				{
					if (IGameRules* pGameRules = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules())
					{
						HitInfo hit;

						hit.targetId = pActor->GetEntityId();
						hit.shooterId = behaviorParams.shooterId;
						hit.weaponId = 0;
						hit.damage = (float)damage;
						hit.type = 0;

						pGameRules->ServerHit(hit);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorHitPassenger::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorHitPassenger);
