// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to handle vehicle Attachments

   -------------------------------------------------------------------------
   History:
   - 12:12:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __FLOWVEHICLEATTACHMENT_H__
#define __FLOWVEHICLEATTACHMENT_H__

#include "FlowVehicleBase.h"

class CFlowVehicleAttachment
	: public CFlowVehicleBase
{
public:

	CFlowVehicleAttachment(SActivationInfo* pActivationInfo) { Init(pActivationInfo); }
	~CFlowVehicleAttachment() { Delete(); }

	// CFlowBaseNode
	virtual void         Init(SActivationInfo* pActivationInfo);
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
		IN_ATTACHMENT,
		IN_ENTITYID,
		IN_ATTACH,
		IN_DETACH
	};

	enum EOutputs
	{
	};

	void Attach(SActivationInfo* pActInfo, bool attach);

	EntityId m_attachedId;

};

#endif
