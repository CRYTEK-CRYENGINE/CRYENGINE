// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to handle a vehicle movement features

   -------------------------------------------------------------------------
   History:
   - 11:07:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "FlowVehicleMovement.h"
#include <CryFlowGraph/IFlowBaseNode.h>

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleMovement::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleMovement(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleMovement::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig<bool>("warmupEngineTrigger",    _HELP("Warmup the vehicle engine")),
		InputPortConfig<SFlowSystemVoid>("zeroMass",    _HELP("Zero vehicle mass to make it static (this is usually a crude hack, don't use it unless you absolutely have to).")),
		InputPortConfig<SFlowSystemVoid>("restoreMass", _HELP("Restore the vehicles mass.")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Handle vehicle movement");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleMovement::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (flowEvent == eFE_Activate)
	{
		if (IVehicle* pVehicle = GetVehicle())
		{
			if (IsPortActive(pActivationInfo, In_WarmupEngine))
			{
				if (IVehicleMovement* pMovement = pVehicle->GetMovement())
				{
					SVehicleMovementEventParams eventParams;
					pMovement->OnEvent(IVehicleMovement::eVME_WarmUpEngine, eventParams);
				}
			}

			if (IsPortActive(pActivationInfo, In_ZeroMass) || IsPortActive(pActivationInfo, In_RestoreMass))
			{
				ZeroMass(!IsPortActive(pActivationInfo, In_RestoreMass));
			}
		}
	}
	else if (flowEvent == eFE_Initialize)
	{
		ZeroMass(false);
	}
	else if (flowEvent == eFE_Update)
	{
		if (m_shouldZeroMass)
		{
			ZeroMass(false); // reset vehicle to initial state
			ZeroMass(true);  // reapply the zero mass
			m_shouldZeroMass = false;
		}

		pActivationInfo->pGraph->SetRegularlyUpdated(pActivationInfo->myID, false);
	}
}

//------------------------------------------------------------------------
void CFlowVehicleMovement::ZeroMass(bool enable)
{
	IVehicle* pVehicle = GetVehicle();
	if (!pVehicle || !pVehicle->GetEntity())
		return;

	IPhysicalEntity* pPhysics = pVehicle->GetEntity()->GetPhysics();
	if (!pPhysics)
		return;

	if (enable)
	{
		// store list of partIds + mass
		pe_status_nparts nparts;
		int count = pPhysics->GetStatus(&nparts);

		for (int i = 0; i < count; ++i)
		{
			pe_params_part paramsGet;
			paramsGet.ipart = i;
			if (pPhysics->GetParams(&paramsGet) && paramsGet.mass > 0.0f)
			{
				SPartMass mass(paramsGet.partid, paramsGet.mass);
				m_partMass.push_back(mass);

				pe_params_part paramsSet;
				paramsSet.ipart = i;
				paramsSet.mass = 0.0f;
				pPhysics->SetParams(&paramsSet);
			}
		}
	}
	else
	{
		for (int i = 0, n = m_partMass.size(); i < n; ++i)
		{
			pe_params_part params;
			params.partid = m_partMass[i].partid;
			params.mass = m_partMass[i].mass;
			pPhysics->SetParams(&params);
		}

		m_partMass.clear();
	}
}

//------------------------------------------------------------------------
void CFlowVehicleMovement::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	CFlowVehicleBase::Serialize(pActivationInfo, ser);

	ser.BeginGroup("FlowVehicleMovement");

	if (ser.IsReading())
		m_partMass.resize(0);

	int count = m_partMass.size();
	int oldCount = count;
	ser.Value("count", count);

	if (count > oldCount)
		m_partMass.resize(count);

	char buf[16];
	for (int i = 0; i < count; ++i)
	{
		cry_sprintf(buf, "partid_%i", i);
		ser.Value(buf, m_partMass[i].partid);

		cry_sprintf(buf, "mass_%i", i);
		ser.Value(buf, m_partMass[i].mass);
	}

	if (ser.IsReading())
	{
		m_shouldZeroMass = (count > 0);
		if (m_shouldZeroMass)
			pActivationInfo->pGraph->SetRegularlyUpdated(pActivationInfo->myID, true);
	}

	ser.EndGroup();
}

//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleMovementParams::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleMovementParams(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleMovementParams::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig<SFlowSystemVoid>("Trigger",  _HELP("Applies the changes")),
		InputPortConfig<float>("MaxSpeedFactor",     1.0f,                         _HELP("Multiplier to the vehicle's max speed.")),
		InputPortConfig<float>("AccelerationFactor", 1.0f,                         _HELP("Multiplier to the vehicle's acceleration.")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Modifies vehicle movement params");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleMovementParams::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (flowEvent == eFE_Activate)
	{
		if (IVehicle* pVehicle = GetVehicle())
		{
			if (IsPortActive(pActivationInfo, In_Trigger))
			{
				if (IVehicleMovement* pMovement = pVehicle->GetMovement())
				{
					SVehicleMovementEventParams eventParams;
					eventParams.fValue = GetPortFloat(pActivationInfo, In_MaxSpeedFactor);
					pMovement->OnEvent(IVehicleMovement::eVME_SetFactorMaxSpeed, eventParams);

					eventParams.fValue = GetPortFloat(pActivationInfo, In_MaxAcceleration);
					pMovement->OnEvent(IVehicleMovement::eVME_SetFactorAccel, eventParams);
				}
			}
		}
	}
}

REGISTER_FLOW_NODE("Vehicle:Movement", CFlowVehicleMovement);
REGISTER_FLOW_NODE("Vehicle:MovementParams", CFlowVehicleMovementParams);
