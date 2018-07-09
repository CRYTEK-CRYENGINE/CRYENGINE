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

#ifndef __FLOW_VEHICLE_HANDBRAKE_H__
#define __FLOW_VEHICLE_HANDBRAKE_H__

#include "FlowVehicleBase.h"

class CFlowVehicleHandbrake : public CFlowVehicleBase
{
public:

	CFlowVehicleHandbrake(SActivationInfo* pActivationInfo);

	virtual ~CFlowVehicleHandbrake();

	// CFlowBaseNode

	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);

	virtual void         GetConfiguration(SFlowNodeConfig& nodeConfig);

	virtual void         ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);

	// ~CFlowBaseNode

	// IVehicleEventListener

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

	// ~IVehicleEventListener

	virtual void GetMemoryUsage(ICrySizer* pCrySizer) const;

protected:

	enum EInputs
	{
		eIn_Activate = 0,
		eIn_Deactivate,
		eIn_ResetTimer
	};
};

#endif //__FLOW_VEHICLE_HANDBRAKE_H__
