// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to handle vehicle passengers

   -------------------------------------------------------------------------
   History:
   - 09:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleSeat.h"
#include "FlowVehiclePassenger.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//------------------------------------------------------------------------
CFlowVehiclePassenger::CFlowVehiclePassenger(SActivationInfo* pActivationInfo)
	: m_passengerCount(0)
{
	CFlowVehicleBase::Init(pActivationInfo);
}

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehiclePassenger::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehiclePassenger(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehiclePassenger::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig_Void("ActorInTrigger",  _HELP("Forces actor [ActorId] to get inside a vehicle, if a seat is available")),
		InputPortConfig_Void("ActorOutTrigger", _HELP("Forces passenger [ActorId] to get out of a vehicle")),
		InputPortConfig<EntityId>("ActorId",    _HELP("Entity ID of the actor which is used")),
		InputPortConfig<int>("SeatId",          _HELP("Seat ID of the preferred seat where place the new passenger")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		OutputPortConfig<EntityId>("ActorIn",  _HELP("Triggered when ANY Actor got in. Outputs its EntityId.")),
		OutputPortConfig<EntityId>("ActorOut", _HELP("Triggered when ANY Actor got out. Outputs its EntityId.")),
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Handle vehicle passengers");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehiclePassenger::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (flowEvent == eFE_Activate)
	{
		m_actorId = GetPortEntityId(pActivationInfo, IN_ACTORID);
		m_seatId = GetPortInt(pActivationInfo, IN_SEATID);

		if (IVehicle* pVehicle = GetVehicle())
		{
			if (IsPortActive(pActivationInfo, IN_TRIGGERPASSENGERIN))
			{
				if (m_actorId && m_seatId > 0)
				{
					IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_actorId);

					if (pActor)
					{
						IVehicle* pCurrent = pActor->GetLinkedVehicle();

						if (pCurrent && pCurrent != pVehicle)
						{
							if (IVehicleSeat* pSeat = pCurrent->GetSeatForPassenger(m_actorId))
								pSeat->Exit(false, true);
						}

						if (pCurrent == pVehicle && pCurrent->GetSeatForPassenger(m_actorId))
						{
							((CVehicle*)pVehicle)->ChangeSeat(m_actorId, 0, m_seatId);
						}
						else
						{
							if (IVehicleSeat* pSeat = pVehicle->GetSeatById(m_seatId))
								pSeat->Enter(m_actorId);
						}
					}
				}
			}

			if (IsPortActive(pActivationInfo, IN_TRIGGERPASSENGEROUT))
			{
				if (m_actorId)
				{
					if (IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_actorId))
						pSeat->Exit(true);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CFlowVehiclePassenger::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CFlowVehicleBase::OnVehicleEvent(event, params);

	if (event == eVE_PassengerEnter)
	{
		SFlowAddress addr(m_nodeID, OUT_ACTORIN, true);
		m_pGraph->ActivatePort(addr, params.entityId);
	}
	else if (event == eVE_PassengerExit)
	{
		SFlowAddress addr(m_nodeID, OUT_ACTOROUT, true);
		m_pGraph->ActivatePort(addr, params.entityId);
	}
}

REGISTER_FLOW_NODE("Vehicle:Passenger", CFlowVehiclePassenger);
