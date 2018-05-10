// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to use vehicle horn

   -------------------------------------------------------------------------
   History:
   - 03:03:2007: Created by MichaelR

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "FlowVehicleBase.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleSeat.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowVehicleHonk
	: public CFlowVehicleBase
{
public:
	CFlowVehicleHonk(SActivationInfo* pActivationInfo) : m_timeout(0.0f)
	{
		Init(pActivationInfo);
	}

	~CFlowVehicleHonk()
	{
		Delete();
	}

	enum EInputs
	{
		eIn_Trigger,
		eIn_Duration
	};

	enum EOutputs
	{
	};

	//------------------------------------------------------------------------
	void GetConfiguration(SFlowNodeConfig& nodeConfig)
	{
		CFlowVehicleBase::GetConfiguration(nodeConfig);

		static const SInputPortConfig pInConfig[] =
		{
			InputPortConfig<SFlowSystemVoid>("Trigger", _HELP("Trigger to honk")),
			InputPortConfig<float>("Duration",          2.0f,                     _HELP("Duration in seconds")),
			{ 0 }
		};

		static const SOutputPortConfig pOutConfig[] =
		{
			{ 0 }
		};

		nodeConfig.sDescription = _HELP("Use a vehicle's horn");
		nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
		nodeConfig.pInputPorts = pInConfig;
		nodeConfig.pOutputPorts = pOutConfig;
		nodeConfig.SetCategory(EFLN_APPROVED);
	}

	//------------------------------------------------------------------------
	CVehicleSeat* GetDriverSeat()
	{
		if (IVehicle* pIVehicle = GetVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pIVehicle);
			TVehicleSeatId seatId = pVehicle->GetDriverSeatId();
			if (seatId != InvalidVehicleSeatId)
				return static_cast<CVehicleSeat*>(pVehicle->GetSeatById(seatId));
		}
		return 0;
	}

	void StopAudio(SActivationInfo* pActivationInfo)
	{
		if (CVehicleSeat* pSeat = GetDriverSeat())
			pSeat->OnAction(eVAI_Horn, eAAM_OnRelease, 1);
		pActivationInfo->pGraph->SetRegularlyUpdated(pActivationInfo->myID, true);
		m_timeout = 0.0f;
	}

	//------------------------------------------------------------------------
	void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
	{
		CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

		if (flowEvent == eFE_Activate && IsPortActive(pActivationInfo, eIn_Trigger))
		{
			if (CVehicleSeat* pVehicleSeat = GetDriverSeat())
			{
				pVehicleSeat->OnAction(eVAI_Horn, eAAM_OnPress, 1);
				m_timeout = GetPortFloat(pActivationInfo, eIn_Duration);
				if (m_timeout <= 0.0f)
					m_timeout = 0.1f;
				pActivationInfo->pGraph->SetRegularlyUpdated(pActivationInfo->myID, true);
			}
		}
		if (flowEvent == eFE_Update)
		{
			if (m_timeout > 0.0f)
			{
				m_timeout -= gEnv->pTimer->GetFrameTime();
				if (m_timeout <= 0.0f)
					StopAudio(pActivationInfo);
			}
			else
				pActivationInfo->pGraph->SetRegularlyUpdated(pActivationInfo->myID, true);
		}
	}

	// IVehicleEventListener
	void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
	{
		CFlowVehicleBase::OnVehicleEvent(event, params);
		if (event == eVE_Destroyed)
		{
			if (m_timeout > 0.0f)
			{
				m_timeout = 0.0f;
				// will stop node update on next eFE_Update
			}
		}
	}
	// ~IVehicleEventListener

	//------------------------------------------------------------------------
	IFlowNodePtr Clone(SActivationInfo* pActivationInfo)
	{
		IFlowNode* pNode = new CFlowVehicleHonk(pActivationInfo);
		return pNode;
	}

	//------------------------------------------------------------------------
	void Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
	{
		CFlowVehicleBase::Serialize(pActivationInfo, ser);
		if (ser.IsReading())
			StopAudio(pActivationInfo);
	}

	//------------------------------------------------------------------------
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	float m_timeout;
};

REGISTER_FLOW_NODE("Vehicle:Honk", CFlowVehicleHonk);
