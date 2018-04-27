// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "FlowVehicleAttachment.h"
#include "VehicleSystem/VehiclePartAttachment.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//------------------------------------------------------------------------
void CFlowVehicleAttachment::Init(SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::Init(pActivationInfo);

	m_attachedId = 0;
}

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleAttachment::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleAttachment(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleAttachment::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig<string>("Attachment",   _HELP("Name of Attachment socket (see Vehicle Editor, listbox planned here)")),
		InputPortConfig<EntityId>("AttachedId", _HELP("Entity to attach/detach"),                                              _HELP("EntityId")),
		InputPortConfig_Void("Attach",          _HELP("Trigger to attach the entity")),
		InputPortConfig_Void("Detach",          _HELP("Trigger to detach the entity")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Handle vehicle entity attachments");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED); // kindly approved by alex for only two paulaners in return
}

//------------------------------------------------------------------------
void CFlowVehicleAttachment::Attach(SActivationInfo* pActInfo, bool attach)
{
	IVehicle* pVehicle = GetVehicle();
	if (!pVehicle)
		return;

	IVehiclePart* pPart = pVehicle->GetPart(GetPortString(pActInfo, IN_ATTACHMENT).c_str());
	if (!pPart)
		return;

	CVehiclePartEntityAttachment* pAttachment = CAST_VEHICLEOBJECT(CVehiclePartEntityAttachment, pPart);
	if (!pAttachment)
		return;

	if (!attach)
	{
		pAttachment->SetAttachmentEntity(0);
		m_attachedId = 0;
	}
	else
	{
		EntityId entityId = GetPortEntityId(pActInfo, IN_ENTITYID);
		pAttachment->SetAttachmentEntity(entityId);
		m_attachedId = entityId;
	}
}

//------------------------------------------------------------------------
void CFlowVehicleAttachment::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (!GetVehicle())
		return;

	switch (flowEvent)
	{
	case eFE_Initialize:
		{
			if (m_attachedId)
			{
				Attach(pActivationInfo, false);
			}
			break;
		}
	case eFE_Activate:
		{
			if (IsPortActive(pActivationInfo, IN_DETACH))
			{
				Attach(pActivationInfo, false);
			}
			else if (IsPortActive(pActivationInfo, IN_ATTACH))
			{
				Attach(pActivationInfo, true);
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
void CFlowVehicleAttachment::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	CFlowVehicleBase::Serialize(pActivationInfo, ser);
}

//------------------------------------------------------------------------
void CFlowVehicleAttachment::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CFlowVehicleBase::OnVehicleEvent(event, params);
}

REGISTER_FLOW_NODE("Vehicle:Attachment", CFlowVehicleAttachment);
