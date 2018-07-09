// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to toggel vehicle lights

   -------------------------------------------------------------------------
   History:
   - 03:03:2007: Created by MichaelR

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "FlowVehicleLights.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehiclePartLight.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleLights::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleLights(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleLights::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig<string>("LightType",           _HELP("Type of light to toggle"), _HELP("LightType"), _UICONFIG("enum_global:vehicleLightTypes")),
		InputPortConfig<SFlowSystemVoid>("Activate",   _HELP("Activate lights")),
		InputPortConfig<SFlowSystemVoid>("Deactivate", _HELP("Deactivate lights")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Toggle vehicle lights");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleLights::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
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
				const string& type = GetPortString(pActivationInfo, eIn_LightType);

				int partCount = pVehicle->GetPartCount();
				for (int i = 0; i < partCount; ++i)
				{
					IVehiclePart* pPart = pVehicle->GetPart(i);

					if (!pPart)
						continue;

					if (CVehiclePartLight* pLightPart = CAST_VEHICLEOBJECT(CVehiclePartLight, pPart))
					{
						if (pLightPart->GetLightType() == type || type == "All")
						{
							pLightPart->ToggleLight(activate);
						}
					}
				}
			}
		}
	}
}

REGISTER_FLOW_NODE("Vehicle:Lights", CFlowVehicleLights);
