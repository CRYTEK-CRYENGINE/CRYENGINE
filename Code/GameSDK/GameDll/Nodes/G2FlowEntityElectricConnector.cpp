// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  Flow nodes for working with XBox 360 controller
   -------------------------------------------------------------------------
   History:
   - 31:3:2008        : Created by Kevin

*************************************************************************/

#include "StdAfx.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowEntityElectricConnector : public CFlowBaseNode<eNCT_Instanced>, IEntityEventListener
{
	enum EInputPorts
	{
		eIP_Emitter = 0,
		eIP_Receiver,
		eIP_Disabled,
	};

	enum EOutputPorts
	{
		eOP_Flow = 0,
		//eOP_ConnectedEntity,
		eOP_Last,
	};

public:
	CFlowEntityElectricConnector(SActivationInfo* pActInfo)
	{
		m_nodeID = pActInfo->myID;
		m_pGraph = pActInfo->pGraph;
		m_flow = 0;
		m_entityId = 0;
	}

	~CFlowEntityElectricConnector()
	{
		UnregisterEvents();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)   { return new CFlowEntityElectricConnector(pActInfo); }
	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("entity", m_entityId);
		ser.Value("flow", m_flow);

		if (ser.IsReading())
		{
			m_actInfo = *pActInfo;
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Emitter",  _HELP("Enable entity as electric emitter.")),
			InputPortConfig<bool>("Receiver", _HELP("Enable entity as electric receiver.")),
			InputPortConfig<bool>("Disabled", _HELP("Disable entity as electric emitter and receiver.")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<bool>("ElectricFlow", _HELP("Flow is true when this electric connector is successfully connected with an opposite electric connector.")),
			//OutputPortConfig<EntityId>( "ConnectedEntity", _HELP("Entity ID of successfully connected opposite electric connector.")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Marks assigned entity as an electric connector (for NanoRope connections).");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				if (GetPortBool(pActInfo, eIP_Emitter))
					m_flow = +1;
				if (GetPortBool(pActInfo, eIP_Receiver))
					m_flow = -1;
				if (GetPortBool(pActInfo, eIP_Disabled))
					m_flow = 0;
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Emitter))
				{
					UnmarkElectricConnector(m_entityId);
					m_flow = +1;
					MarkElectricConnector(m_entityId);
				}
				if (IsPortActive(pActInfo, eIP_Receiver))
				{
					UnmarkElectricConnector(m_entityId);
					m_flow = -1;
					MarkElectricConnector(m_entityId);
				}
				if (IsPortActive(pActInfo, eIP_Disabled))
				{
					UnmarkElectricConnector(m_entityId);
					m_flow = 0;
				}
			}
			break;

		case eFE_SetEntityId:
			{
				UnregisterEvents();

				UnmarkElectricConnector(m_entityId);
				m_entityId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : 0;
				MarkElectricConnector(m_entityId);

				RegisterEvents();
			}
			break;
		}
	}

	const char* GetConnectorTypeLinkName(int flow)
	{
		if (flow == 0)
			flow = m_flow;

		if (flow > 0)
			return "ElectricSource";
		if (flow < 0)
			return "ElectricTarget";
		return NULL;
	}

	void UnmarkElectricConnector(EntityId eid)
	{
		if (eid == 0)
			return;

		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(eid);
		if (!pEnt)
			return;

		IEntityLink* pLink = pEnt->GetEntityLinks();
		while (pLink)
		{
			if (strcmp(pLink->name, GetConnectorTypeLinkName(+1)) == 0)
			{
				pEnt->RemoveEntityLink(pLink);
				pLink = pEnt->GetEntityLinks();
			}
			else if (strcmp(pLink->name, GetConnectorTypeLinkName(-1)) == 0)
			{
				pEnt->RemoveEntityLink(pLink);
				pLink = pEnt->GetEntityLinks();
			}
			else
			{
				pLink = pLink->next;
			}
		}
	}

	void MarkElectricConnector(EntityId eid)
	{
		if (eid == 0)
			return;

		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(eid);
		if (!pEnt)
			return;

		const char* tag = GetConnectorTypeLinkName(0);
		if (tag)
			pEnt->AddEntityLink(tag, 0,CryGUID::Null());
	}

	void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (!m_pGraph->IsEnabled() || m_pGraph->IsSuspended() || !m_pGraph->IsActive())
			return;

		if (event.event == ENTITY_EVENT_DONE)
		{
			m_pGraph->SetEntityId(m_nodeID, 0);
		}
		else if (event.event == ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT)
		{
			SFlowNodeConfig config;
			GetConfiguration(config);

			for (int i = 0; i < eOP_Last; i++)
			{
				if (strcmp(config.pOutputPorts[i].name, (const char*)event.nParam[0]) == 0)
				{
					SFlowAddress addr(m_nodeID, i, true);
					if (event.nParam[1] == IEntityClass::EVT_BOOL)
						m_pGraph->ActivatePort(addr, *(bool*)event.nParam[2]);
					else if (event.nParam[1] == IEntityClass::EVT_ENTITY)
						m_pGraph->ActivatePort(addr, *(EntityId*)event.nParam[2]);

					break;
				}
			}

		}
	}

	void RegisterEvents()
	{
		if (m_entityId)
		{
			gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
			gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT, this);
		}

	}
	void UnregisterEvents()
	{
		if (m_entityId)
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
			gEnv->pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT, this);
		}
	}

private:
	SActivationInfo m_actInfo; // Activation info instance
	TFlowNodeId     m_nodeID;
	IFlowGraph*     m_pGraph;
	EntityId        m_entityId;
	int             m_flow;
};

REGISTER_FLOW_NODE("Entity:ElectricConnector", CFlowEntityElectricConnector);
