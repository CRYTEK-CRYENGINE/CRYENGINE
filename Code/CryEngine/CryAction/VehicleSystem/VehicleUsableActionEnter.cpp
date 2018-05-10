// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a usable action to enter a vehicle seat

   -------------------------------------------------------------------------
   History:
   - 19:01:2006: Created by Mathieu Pinard

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
#include "VehicleSeat.h"
#include "VehicleSeatGroup.h"
#include "VehicleUsableActionEnter.h"

//------------------------------------------------------------------------
bool CVehicleUsableActionEnter::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	CVehicleParams enterTable = table.findChild("Enter");
	if (!enterTable)
		return false;

	m_pVehicle = static_cast<CVehicle*>(pVehicle);

	if (CVehicleParams seatsTables = enterTable.findChild("Seats"))
	{
		int c = seatsTables.getChildCount();
		int i = 0;
		m_seatIds.reserve(c);

		for (; i < c; i++)
		{
			CVehicleParams seatRef = seatsTables.getChild(i);

			if (const char* pSeatName = seatRef.getAttr("value"))
			{
				if (TVehicleSeatId seatId = m_pVehicle->GetSeatId(pSeatName))
					m_seatIds.push_back(seatId);
			}
		}
	}

	return !m_seatIds.empty();
}

//------------------------------------------------------------------------
int CVehicleUsableActionEnter::OnEvent(int eventType, SVehicleEventParams& eventParams)
{
	if (eventType == eVAE_IsUsable)
	{
		EntityId& userId = eventParams.entityId;

		for (TVehicleSeatIdVector::iterator ite = m_seatIds.begin(), end = m_seatIds.end(); ite != end; ++ite)
		{
			TVehicleSeatId seatId = *ite;
			IVehicleSeat* pSeat = m_pVehicle->GetSeatById(seatId);
			if (IsSeatAvailable(pSeat, userId))
			{
				eventParams.iParam = seatId;
				return 1;
			}
		}

		return 0;
	}
	else if (eventType == eVAE_OnUsed)
	{
		EntityId& userId = eventParams.entityId;

		IVehicleSeat* pSeat = m_pVehicle->GetSeatById(eventParams.iParam);

		if (IsSeatAvailable(pSeat, userId))
			return pSeat->Enter(userId);

		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------
bool CVehicleUsableActionEnter::IsSeatAvailable(IVehicleSeat* pSeat, EntityId userId)
{
	if (!pSeat)
		return false;

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(userId);
	if (!static_cast<CVehicleSeat*>(pSeat)->IsFree(pActor))
		return false;

	return true;
}

void CVehicleUsableActionEnter::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
	s->AddContainer(m_seatIds);
}

DEFINE_VEHICLEOBJECT(CVehicleUsableActionEnter);
