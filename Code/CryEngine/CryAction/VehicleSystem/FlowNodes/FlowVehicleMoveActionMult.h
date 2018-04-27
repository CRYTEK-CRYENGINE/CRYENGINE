// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to apply multipliers to vehicle
   movement actions

   -------------------------------------------------------------------------
   History:
   - 26:01:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __FLOWVEHICLEMOVEACTIONMULT_H__
#define __FLOWVEHICLEMOVEACTIONMULT_H__

#include "FlowVehicleBase.h"

class CFlowVehicleMoveActionMult
	: public CFlowVehicleBase,
	  public IVehicleMovementActionFilter
{
public:

	CFlowVehicleMoveActionMult(SActivationInfo* pActivationInfo);
	~CFlowVehicleMoveActionMult() { Delete(); }

	// CFlowBaseNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& nodeConfig);
	virtual void         ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);
	virtual void         Serialize(SActivationInfo* pActivationInfo, TSerialize ser);
	// ~CFlowBaseNode

	// IVehicleMovementActionFilter
	void OnProcessActions(SVehicleMovementAction& movementAction);
	// ~IVehicleMovementActionFilter

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:

	enum EInputs
	{
		IN_ENABLETRIGGER = 0,
		IN_DISABLETRIGGER,

		IN_POWERMULT,
		IN_ROTATEPITCHMULT,
		IN_ROTATEYAWMULT,

		IN_POWERMUSTBEPOSITIVE,
	};

	enum EOutputs
	{
	};

	float m_powerMult;
	float m_pitchMult;
	float m_yawMult;
	bool  m_powerMustBePositive;
};

#endif
