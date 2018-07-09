// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FlowVehicleBase.h"

class CFlowVehicleSeat
	: public CFlowVehicleBase
{
public:

	CFlowVehicleSeat(SActivationInfo* pActivationInfo) {}
	~CFlowVehicleSeat() { Delete(); }

	// CFlowBaseNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& nodeConfig);
	virtual void         ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);
	// ~CFlowBaseNode

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) {}
	// ~IVehicleEventListener

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:

	void ActivateOutputPorts(SActivationInfo* pActivationInfo);

	enum EInputs
	{
		eIn_Trigger,
		eIn_Seat,
		eIn_Lock,
		eIn_LockType,
	};

	enum EOutputs
	{
		eOut_Success,
	};
};

//////////////////////////////////////////////////////////////////////////
// Gets vehicle seat helper positions
//////////////////////////////////////////////////////////////////////////
class CFlowNode_GetSeatHelperVehicle : public CFlowVehicleBase
{
public:
	enum EInputs
	{
		eIn_Get,
		eIn_Seat,
	};
	enum EOutputs
	{
		eOut_Pos,
		eOut_Dir,
	};

	CFlowNode_GetSeatHelperVehicle(SActivationInfo* pActivationInfo)
	{
		Init(pActivationInfo);
	}
	~CFlowNode_GetSeatHelperVehicle() { Delete(); }
	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowNode_GetSeatHelperVehicle(pActInfo);
		return pNode;
	}
	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo);

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

