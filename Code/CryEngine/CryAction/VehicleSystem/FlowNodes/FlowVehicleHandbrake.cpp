// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to enable/disable vehicle handbrake.

   -------------------------------------------------------------------------
   History:
   - 15:04:2010: Created by Paul Slinger

*************************************************************************/

#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "FlowVehicleHandbrake.h"
#include <CryFlowGraph/IFlowBaseNode.h>

//------------------------------------------------------------------------
CFlowVehicleHandbrake::CFlowVehicleHandbrake(SActivationInfo* pActivationInfo)
{
	Init(pActivationInfo);
}

//------------------------------------------------------------------------
CFlowVehicleHandbrake::~CFlowVehicleHandbrake()
{
	Delete();
}

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleHandbrake::Clone(SActivationInfo* pActivationInfo)
{
	return new CFlowVehicleHandbrake(pActivationInfo);
}

//------------------------------------------------------------------------
void CFlowVehicleHandbrake::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig<SFlowSystemVoid>("Activate",   _HELP("Activate handbrake")),
		InputPortConfig<SFlowSystemVoid>("Deactivate", _HELP("Deactivate handbrake")),
		InputPortConfig<float>("ResetTimer",           5.0f,                          _HELP("Time (in seconds) before handbrake is re-activated")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Toggle vehicle handbrake");
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;

	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;

	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleHandbrake::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (flowEvent == eFE_Activate)
	{
		if (IVehicle* pVehicle = GetVehicle())
		{
			bool activate = IsPortActive(pActivationInfo, eIn_Activate);

			bool deactivate = IsPortActive(pActivationInfo, eIn_Deactivate);

			if (activate || deactivate)
			{
				SVehicleMovementEventParams vehicleMovementEventParams;

				vehicleMovementEventParams.bValue = activate;
				vehicleMovementEventParams.fValue = GetPortFloat(pActivationInfo, eIn_ResetTimer);

				pVehicle->GetMovement()->OnEvent(IVehicleMovement::eVME_EnableHandbrake, vehicleMovementEventParams);
			}
		}
	}
}

//------------------------------------------------------------------------
void CFlowVehicleHandbrake::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CFlowVehicleBase::OnVehicleEvent(event, params);
}

//------------------------------------------------------------------------
void CFlowVehicleHandbrake::GetMemoryUsage(ICrySizer* pCrySizer) const
{
	pCrySizer->Add(*this);
}

REGISTER_FLOW_NODE("Vehicle:Handbrake", CFlowVehicleHandbrake);
