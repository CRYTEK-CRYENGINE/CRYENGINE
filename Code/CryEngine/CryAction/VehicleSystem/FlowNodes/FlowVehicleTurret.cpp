// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowVehicleTurret.h"

#include "IVehicleSystem.h"
#include "VehicleSystem/VehicleSeatActionRotateTurret.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleSeat.h"

IFlowNodePtr CFlowVehicleTurret::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleTurret(pActivationInfo);
	return pNode;
}

void CFlowVehicleTurret::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig_AnyType("Trigger", _HELP("Triggers the turret to be rotated")),
		InputPortConfig<string>("seatId",  _HELP("Seat which can rotate the turret"),              _HELP("Seat"),_UICONFIG("dt=vehicleSeats, ref_entity=entityId")),
		InputPortConfig<Vec3>("aimPos",    _HELP("World position at which the turret should aim")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		OutputPortConfig<bool>("Success", _HELP("Triggers when the node is processed, outputing 0 or 1 representing fail or success")),
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Control the aim of the vehicle's turret or mounted weapon");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

void CFlowVehicleTurret::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	CFlowVehicleBase::ProcessEvent(event, pActInfo); // handles eFE_SetEntityId

	if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, eIn_Trigger))
		{
			bool bSuccess = false;

			const string seatName = GetPortString(pActInfo, eIn_Seat);
			Vec3 aimPos = GetPortVec3(pActInfo, eIn_AimPos);

			CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle());
			if (pVehicle)
			{
				CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(pVehicle->GetSeatId(seatName)));
				if (pSeat)
				{
					TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
					for (TVehicleSeatActionVector::iterator ite = seatActions.begin(); ite != seatActions.end(); ++ite)
					{
						IVehicleSeatAction* pSeatAction = ite->pSeatAction;
						CVehicleSeatActionRotateTurret* pActionRotateTurret = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction);
						if (pActionRotateTurret)
						{
							if (!aimPos.IsZero())
							{
								pActionRotateTurret->SetAimGoal(aimPos);
							}
							bSuccess = true;
							break;
						}
					}
				}
			}
			ActivateOutput(pActInfo, eOut_Success, bSuccess);
		}
	}
}

void CFlowVehicleTurret::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CFlowVehicleBase::OnVehicleEvent(event, params);
}

void CFlowVehicleTurret::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	CFlowVehicleBase::Serialize(pActivationInfo, ser);
}

REGISTER_FLOW_NODE("Vehicle:Turret", CFlowVehicleTurret);
