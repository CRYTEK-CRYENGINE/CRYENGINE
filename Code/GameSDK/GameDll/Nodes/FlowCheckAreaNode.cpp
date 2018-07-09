// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Flow node to check if an entity is inside an area

   -------------------------------------------------------------------------
   History:
   - 27:07:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_CheckArea : public CFlowBaseNode<eNCT_Instanced>, public IEntityEventListener
{
public:
	CFlowNode_CheckArea(SActivationInfo* pActInfo) : m_entityId(0), m_bInside(false)
	{

	}

	virtual ~CFlowNode_CheckArea()
	{
		UnregisterEvents();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_CheckArea(pActInfo);
	}

	void RegisterEvents()
	{
		gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_ENTERAREA, this);
		gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_LEAVEAREA, this);
	}

	void UnregisterEvents()
	{
		gEnv->pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_ENTERAREA, this);
		gEnv->pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_LEAVEAREA, this);
	}

	enum EInputPorts
	{
		EIP_Trigger = 0,
		EIP_Entity,
		EIP_Automatic,
	};

	enum EOutputPorts
	{
		EOP_IsInside = 0,
		EOP_Enter,
		EOP_Leave,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Check",       _HELP("Check now if the entity is inside the node entity's area")),
			InputPortConfig<EntityId>("Entity", _HELP("Node will output when this entity enters/leaves the node entity's area")),
			InputPortConfig<bool>("Automatic",  false,                                                                           _HELP("Automatically report when the entity enters/leaves the node entity's area")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<bool>("Result", _HELP("Outputs the current result of the check. True for inside, false for outside.")),
			OutputPortConfig_Void("Inside",  _HELP("Outputs when the entity is inside the node entity's area")),
			OutputPortConfig_Void("Outside", _HELP("Outputs when the entity is outside the node entity's area")),
			{ 0 }
		};

		config.sDescription = _HELP("Checks if the given entity is inside the node entity's area");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ActivateOutputs()
	{
		ActivateOutput(&m_actInfo, EOP_IsInside, m_bInside);
		if (m_bInside)
			ActivateOutput(&m_actInfo, EOP_Enter, true);
		else
			ActivateOutput(&m_actInfo, EOP_Leave, true);
	}

	void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_ENTERAREA:
		case ENTITY_EVENT_LEAVEAREA:
			{
				const EntityId checkId = GetPortEntityId(&m_actInfo, EIP_Entity);
				const EntityId triggeredId = (EntityId)event.nParam[0];
				if (checkId == triggeredId)
				{
					m_bInside = (event.event == ENTITY_EVENT_ENTERAREA);

					if (GetPortBool(&m_actInfo, EIP_Automatic))
						ActivateOutputs();
				}
			}
			break;

		default:
			CRY_ASSERT_MESSAGE(false, "CFlowNode_IsInArea::OnEntityEvent Received unhandled event");
			break;
		}
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Initialize == event)
		{
			m_actInfo = *pActInfo;
		}
		else if (eFE_SetEntityId == event)
		{
			EntityId newEntityId = 0;
			if (pActInfo->pEntity)
			{
				newEntityId = pActInfo->pEntity->GetId();
			}

			if (m_entityId && newEntityId)
			{
				UnregisterEvents();
			}

			if (newEntityId)
			{
				m_entityId = newEntityId;
				m_bInside = false;

				RegisterEvents();
			}
		}
		else if (eFE_Activate == event && IsPortActive(pActInfo, EIP_Trigger))
		{
			ActivateOutputs();
		}
	}

private:
	SActivationInfo m_actInfo;
	EntityId        m_entityId;
	bool            m_bInside;
};

REGISTER_FLOW_NODE("Entity:CheckArea", CFlowNode_CheckArea);
