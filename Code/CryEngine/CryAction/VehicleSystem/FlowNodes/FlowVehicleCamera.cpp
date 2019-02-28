// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowVehicleBase.h"
#include "VehicleSystem/Vehicle.h"
/*
 * helpers
 */

bool parseSeatViewString(const string seatViewPortString, string* seatName, string* viewName, int* viewId)
{
	if (seatViewPortString.empty())
	{
		return false;
	}

	// string should be of form: 'seatName:viewName (viewId)'
	string::size_type sep = seatViewPortString.find(':');
	string::size_type par = seatViewPortString.rfind('(');
	if (!(sep > 0 && par > sep + 1 && seatViewPortString.length() > par + 2))
	{
		return false;
	}

	*seatName = seatViewPortString.substr(0, sep);
	*viewName = seatViewPortString.substr(sep + 1, par - 2 - sep);
	*viewId = atoi(seatViewPortString.substr(par + 1, seatViewPortString.length() - par - 2));

	return true;
}

/*
 * CFlowVehicleChangeView
 */

class CFlowVehicleChangeView : public CFlowVehicleBase
{
	enum EInputs
	{
		eIn_Trigger,
		eIn_SeatView,
	};

	enum EOutputs
	{
		eOut_Success,
	};

public:

	CFlowVehicleChangeView(SActivationInfo* pActInfo)
	{
		Init(pActInfo);
	}
	~CFlowVehicleChangeView() { Delete(); }

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowVehicleChangeView(pActInfo);
		return pNode;
	}

	void GetConfiguration(SFlowNodeConfig& nodeConfig)
	{
		CFlowVehicleBase::GetConfiguration(nodeConfig);

		static const SInputPortConfig pInConfig[] =
		{
			InputPortConfig_Void("Trigger",     _HELP("Triggers the view change")),
			InputPortConfig<string>("SeatView", _HELP("Seat View to be changed"),  _HELP("Seat View"),_UICONFIG("dt=vehicleSeatViews, ref_entity=entityId")),
			{ 0 }
		};

		static const SOutputPortConfig pOutConfig[] =
		{
			OutputPortConfig<bool>("Success", _HELP("Activated when the view is change is successful or fails")),
			{ 0 }
		};

		nodeConfig.sDescription = _HELP("Set vehicle view per seat");
		nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
		nodeConfig.pInputPorts = pInConfig;
		nodeConfig.pOutputPorts = pOutConfig;
		nodeConfig.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowVehicleBase::ProcessEvent(event, pActInfo); // handles eFE_SetEntityId

		if (event == eFE_Activate)
		{
			bool success = false;

			CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle());
			if (pVehicle)
			{
				const string seatViewPortString = GetPortString(pActInfo, eIn_SeatView);
				string seatName, viewName;
				int viewId;
				if (parseSeatViewString(seatViewPortString, &seatName, &viewName, &viewId))
				{
					IVehicleSeat* pSeat = pVehicle->GetSeatById(pVehicle->GetSeatId(seatName));
					if (pSeat)
					{
						success = pSeat->SetView(viewId);
					}
				}
			}

			ActivateOutput(pActInfo, eOut_Success, success);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

/*
 * CFlowVehicleView
 */

class CFlowVehicleView : public CFlowVehicleBase
{
	enum EInputs
	{
		eIn_Enable,
		eIn_Disable,
		eIn_SeatView,
	};

	enum EOutputs
	{
		eOut_Enabled,
		eOut_Disabled,
	};

public:

	CFlowVehicleView(SActivationInfo* pActInfo)
	{
		Init(pActInfo);
	}
	~CFlowVehicleView() { Delete(); }

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowVehicleView(pActInfo);
		return pNode;
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig pInConfig[] =
		{
			InputPortConfig_Void("Enable",      _HELP("Enable custom view")),
			InputPortConfig_Void("Disable",     _HELP("Disable custom view")),
			InputPortConfig<string>("SeatView", _HELP("Seat View to be enabled/disabled"),0,  _UICONFIG("dt=vehicleSeatViews, ref_entity=entityId")),
			{ 0 }
		};

		static const SOutputPortConfig pOutConfig[] =
		{
			OutputPortConfig_Void("Enabled",  _HELP("Activated when the view is enabled successfully")),
			OutputPortConfig_Void("Disabled", _HELP("Activated when the view is disabled successfully")),
			{ 0 }
		};

		config.sDescription = _HELP("Enables/Disables Views per Seat");
		config.pInputPorts = pInConfig;
		config.pOutputPorts = pOutConfig;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CFlowVehicleBase::ProcessEvent(event, pActInfo); // handles eFE_SetEntityId

		if (event == eFE_Activate)
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle());
			if (pVehicle)
			{
				const string seatViewPortString = GetPortString(pActInfo, eIn_SeatView);
				string seatName, viewName;
				int viewId;
				if (parseSeatViewString(seatViewPortString, &seatName, &viewName, &viewId))
				{
					IVehicleSeat* pSeat = pVehicle->GetSeatById(pVehicle->GetSeatId(seatName));
					IVehicleView* pView = pSeat ? pSeat->GetView(viewId) : NULL;
					if (pView)
					{
						if (IsPortActive(pActInfo, eIn_Enable))
						{
							pView->SetEnabled(true);
							ActivateOutput(pActInfo, eOut_Enabled, true);
						}
						if (IsPortActive(pActInfo, eIn_Disable))
						{
							pView->SetEnabled(false);
							ActivateOutput(pActInfo, eOut_Disabled, true);
						}
					}
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Vehicle:ChangeView", CFlowVehicleChangeView);
REGISTER_FLOW_NODE("Vehicle:View", CFlowVehicleView);
