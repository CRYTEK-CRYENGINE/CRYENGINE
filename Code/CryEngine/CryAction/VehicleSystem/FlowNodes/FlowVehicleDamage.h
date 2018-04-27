// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to handle vehicle damages

   -------------------------------------------------------------------------
   History:
   - 02:12:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __FLOWVEHICLEDAMAGE_H__
#define __FLOWVEHICLEDAMAGE_H__

#include "FlowVehicleBase.h"

class CFlowVehicleDamage
	: public CFlowVehicleBase
{
public:

	CFlowVehicleDamage(SActivationInfo* pActivationInfo) { Init(pActivationInfo); }
	~CFlowVehicleDamage() { Delete(); }

	// CFlowBaseNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& nodeConfig);
	virtual void         ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);
	virtual void         Serialize(SActivationInfo* pActivationInfo, TSerialize ser);
	// ~CFlowBaseNode

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	// ~IVehicleEventListener

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:

	enum EInputs
	{
		IN_HITTRIGGER = 0,
		IN_HITVALUE,
		IN_HITPOS,
		IN_HITRADIUS,
		IN_INDESTRUCTIBLE,
		IN_HITTYPE,
		IN_HITCOMPONENT,
	};

	enum EOutputs
	{
		OUT_DAMAGED = 0,
		OUT_DESTROYED,
		OUT_HIT,
	};
};

#endif
