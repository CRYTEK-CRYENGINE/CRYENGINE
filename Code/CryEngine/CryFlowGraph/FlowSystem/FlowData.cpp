// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowData.h"
#include "FlowSerialize.h"

// do some magic stuff to be backwards compatible with flowgraphs
// which don't use the REAL port name but a stripped version

#define FG_ALLOW_STRIPPED_PORTNAMES
// #undef  FG_ALLOW_STRIPPED_PORTNAMES

#define FG_WARN_ABOUT_STRIPPED_PORTNAMES
#undef  FG_WARN_ABOUT_STRIPPED_PORTNAMES

// Maximum number of output ports allowed for dynamic output
#define DYNAMIC_OUTPUT_MAX (64)

CFlowData::CFlowData()
	: m_pInputData(nullptr),
	m_pOutputFirstEdge(nullptr),
	m_getFlowgraphForwardingEntity(0),
	m_forwardingEntityID(0),
	m_nInputs(0),
	m_nOutputs(0),
	m_hasEntity(false),
	m_failedGettingFlowgraphForwardingEntity(false)
{
}

CFlowData::CFlowData(IFlowNodePtr pImpl, const string& name, TFlowNodeTypeId typeId)
{
	m_pImpl = pImpl;
	m_name = name;
	m_typeId = typeId;
	m_getFlowgraphForwardingEntity = 0;
	m_failedGettingFlowgraphForwardingEntity = false;
	m_forwardingEntityID = 0;

	SFlowNodeConfig config;
	DoGetConfiguration(config);
	m_hasEntity = 0 != (config.nFlags & EFLN_TARGET_ENTITY);
	m_nInputs = 0;
	if (config.pInputPorts)
	{
		while (config.pInputPorts[m_nInputs].name != nullptr)
		{
			++m_nInputs;
		}
	}
	if (0 != (config.nFlags & EFLN_DYNAMIC_OUTPUT))
	{
		m_nOutputs = DYNAMIC_OUTPUT_MAX; // Allow for so many output ports to be made
	}
	else if (!config.pOutputPorts)
	{
		m_nOutputs = 0;
	}
	else
	{
		m_nOutputs = 0;
		while (config.pOutputPorts[m_nOutputs].name != nullptr)
		{
			++m_nOutputs;
		}
	}

	m_pInputData = std::unique_ptr<TFlowInputData[]>(new TFlowInputData[m_nInputs]());
	for (int i = 0; i < m_nInputs; i++)
	{
		CRY_ASSERT(config.pInputPorts != NULL);
		m_pInputData[i] = config.pInputPorts[i].defaultData;
	}
	m_pOutputFirstEdge = std::unique_ptr<int[]>(new int[m_nOutputs]());
}

CFlowData::~CFlowData()
{
	if (m_getFlowgraphForwardingEntity)
		gEnv->pScriptSystem->ReleaseFunc(m_getFlowgraphForwardingEntity);
}

CFlowData::CFlowData(const CFlowData& rhs)
{
	bool bCopy = true;
	if (NULL != rhs.m_pImpl)
	{
		SFlowNodeConfig config;
		rhs.DoGetConfiguration(config);
		if (EFLN_DYNAMIC_OUTPUT == (config.nFlags & EFLN_DYNAMIC_OUTPUT))
		{
			m_pImpl = rhs.m_pImpl->Clone(NULL);
			bCopy = false;
		}
	}
	if (true == bCopy)
	{
		m_pImpl = rhs.m_pImpl;
	}

	m_nInputs = rhs.m_nInputs;
	m_nOutputs = rhs.m_nOutputs;

	m_pInputData = std::unique_ptr<TFlowInputData[]>(new TFlowInputData[m_nInputs]());
	m_pOutputFirstEdge = std::unique_ptr<int[]>(new int[m_nOutputs]());

	for (int i = 0; i < m_nInputs; ++i)
		m_pInputData[i] = rhs.m_pInputData[i];
	for (int i = 0; i < m_nOutputs; ++i)
		m_pOutputFirstEdge[i] = rhs.m_pOutputFirstEdge[i];

	m_name = rhs.m_name;
	m_typeId = rhs.m_typeId;
	m_hasEntity = rhs.m_hasEntity;
	m_failedGettingFlowgraphForwardingEntity = true;
	m_getFlowgraphForwardingEntity = 0;
	m_forwardingEntityID = 0;
	m_entityGuid = rhs.m_entityGuid;
}

void CFlowData::DoGetConfiguration(SFlowNodeConfig& config) const
{
	const int MAX_INPUT_PORTS = 64;

	m_pImpl->GetConfiguration(config);
	if (config.nFlags & EFLN_TARGET_ENTITY)
	{
		static SInputPortConfig* inputs = new SInputPortConfig[MAX_INPUT_PORTS];

		SInputPortConfig* pInput = inputs;
		*pInput++ = InputPortConfig<EntityId>("entityId", _HELP("Changes the attached entity dynamically"));

		if (config.pInputPorts)
		{
			while (config.pInputPorts->name)
			{
				CRY_ASSERT(pInput != inputs + MAX_INPUT_PORTS);
				*pInput++ = *config.pInputPorts++;
			}
		}

		SInputPortConfig nullInput = { 0 };
		*pInput++ = nullInput;

		config.pInputPorts = inputs;
	}
}

void CFlowData::Swap(CFlowData& rhs)
{
	std::swap(m_nInputs, rhs.m_nInputs);
	std::swap(m_nOutputs, rhs.m_nOutputs);
	std::swap(m_pInputData, rhs.m_pInputData);
	std::swap(m_pOutputFirstEdge, rhs.m_pOutputFirstEdge);
	std::swap(m_pImpl, rhs.m_pImpl);
	m_name.swap(rhs.m_name);
	TFlowNodeTypeId typeId = rhs.m_typeId;
	rhs.m_typeId = m_typeId;
	m_typeId = typeId;
	bool temp;
	temp = rhs.m_hasEntity;
	rhs.m_hasEntity = m_hasEntity;
	m_hasEntity = temp;
	temp = rhs.m_failedGettingFlowgraphForwardingEntity;
	rhs.m_failedGettingFlowgraphForwardingEntity = m_failedGettingFlowgraphForwardingEntity;
	m_failedGettingFlowgraphForwardingEntity = temp;

	std::swap(m_forwardingEntityID, rhs.m_forwardingEntityID);
	std::swap(m_getFlowgraphForwardingEntity, rhs.m_getFlowgraphForwardingEntity);
	std::swap(m_entityGuid,rhs.m_entityGuid);
}

CFlowData& CFlowData::operator=(const CFlowData& rhs)
{
	CFlowData temp(rhs);
	Swap(temp);
	return *this;
}

bool CFlowData::ResolvePort(const char* name, TFlowPortId& port, bool isOutput)
{
	SFlowNodeConfig config;
	DoGetConfiguration(config);
	if (isOutput)
	{
		for (int i = 0; i < m_nOutputs; i++)
		{
			const char* sPortName = config.pOutputPorts[i].name;

			if (NULL != sPortName && !stricmp(name, sPortName))
			{
				port = i;
				return true;
			}
			if (NULL == sPortName)
				break;
		}
	}
	else
	{
		for (int i = 0; i < m_nInputs; i++)
		{
			const char* sPortName = config.pInputPorts[i].name;

			if (NULL != sPortName && !stricmp(name, sPortName))
			{
				port = i;
				return true;
			}

#ifdef FG_ALLOW_STRIPPED_PORTNAMES
			// fix for t_ stuff in current graphs these MUST NOT be stripped!
			if (sPortName != 0 && sPortName[0] == 't' && sPortName[1] == '_')
			{
				if (!stricmp(name, sPortName))
				{
					port = i;
					return true;
				}
			}
			else if (sPortName)
			{
				// strip special char '_' and text before it!
				// it defines special data type only...
				const char* sSpecial = strchr(sPortName, '_');
				if (sSpecial)
					sPortName = sSpecial + 1;

				if (!stricmp(name, sPortName))
				{
	#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
					CryLogAlways("[flow] CFlowData::ResolvePort: Deprecation warning for port name: '%s' should be '%s'",
					             sPortName, config.pInputPorts[i].name);
					CryLogAlways("[flow] Solution is to resave flowgraph in editor!");
	#endif
					port = i;
					return true;
				}
			}
#endif
		}
	}

	return false;
}

bool CFlowData::SerializeXML(IFlowNode::SActivationInfo* pActInfo, const XmlNodeRef& node, bool reading)
{
	SFlowNodeConfig config;
	DoGetConfiguration(config);
	if (reading)
	{
		if (XmlNodeRef inputsNode = node->findChild("Inputs"))
		{
			for (int i = m_hasEntity; i < m_nInputs; i++)
			{
				const char* value = inputsNode->getAttr(config.pInputPorts[i].name);
				if (value[0])
					SetFromString(m_pInputData[i], value);
#ifdef FG_ALLOW_STRIPPED_PORTNAMES
				else
				{
					// strip special char '_' and text before it!
					// it defines special data type only...
					const char* sPortName = config.pInputPorts[i].name;
					const char* sSpecial = strchr(sPortName, '_');
					if (sSpecial)
						sPortName = sSpecial + 1;

					value = inputsNode->getAttr(sPortName);
					if (value[0])
					{
	#ifdef FG_WARN_ABOUT_STRIPPED_PORTNAMES
						CryLogAlways("[flow] CFlowData::SerializeXML: Deprecation warning for port name: '%s' should be '%s'",
						             sPortName, config.pInputPorts[i].name);
						CryLogAlways("[flow] Solution is to resave flowgraph in editor!");
	#endif
						SetFromString(m_pInputData[i], value);
					}
				}
#endif
			}
		}

		if (config.nFlags & EFLN_DYNAMIC_OUTPUT)
		{
			XmlNodeRef outputsNode = node->findChild("Outputs");
			if (outputsNode)
			{
				// Ugly way to hide this
				m_pImpl->SerializeXML(pActInfo, outputsNode, true);
				DoGetConfiguration(config);

				// Recalculate output size
				if (NULL == config.pOutputPorts)
				{
					m_nOutputs = 0;
				}
				else
				{
					m_nOutputs = 0;
					while (config.pOutputPorts[m_nOutputs].name != nullptr)
					{
						++m_nOutputs;
					}
				}

				m_pOutputFirstEdge = std::unique_ptr<int[]>(new int[m_nOutputs]());
			}
		}

		if (config.nFlags & EFLN_TARGET_ENTITY)
		{
			const char* sGraphEntity = node->getAttr("GraphEntity");
			if (*sGraphEntity != 0)
			{
				int nIndex = atoi(sGraphEntity);
				if (nIndex == 0)
				{
					if (SetEntityId((EntityId)EFLOWNODE_ENTITY_ID_GRAPH1))
						pActInfo->pGraph->ActivateNode(pActInfo->myID);
				}
				else
				{
					if (SetEntityId((EntityId)EFLOWNODE_ENTITY_ID_GRAPH2))
						pActInfo->pGraph->ActivateNode(pActInfo->myID);
				}
			}
			else
			{
				if (node->haveAttr("EntityGUID"))
				{
					node->getAttr("EntityGUID", m_entityGuid);
				}
				else if (node->haveAttr("EntityGUID_64"))
				{
					node->getAttr("EntityGUID_64", m_entityGuid);
				}
				else
				{
					//int id;
					//node->getAttr("Id",id);
					//const char* nodeClass = node->getAttr("Class");
					//GameWarning("FlowGraph Node entity target not specified name=%s nodeId=%d nodeClass=%s",m_name.c_str(),id,nodeClass );
				}
			}
		}
	}

	return true;
}

void CFlowData::Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Flowgraph serialization");
	SFlowNodeConfig config;
	DoGetConfiguration(config);

	int startIndex = 0;

	ser.Value("m_forwardingEntityID", m_forwardingEntityID);

	// need to make sure we activate the node if we change the entity id due to serialization
	// so that the node ends up knowing
	if (ser.IsReading())
	{
		if (m_hasEntity)
		{
			startIndex = 1;
			TFlowInputData data;
			ser.ValueWithDefault("in_0", data, config.pInputPorts[0].defaultData);

			// StuartM: 07/01/10: we should only activate and re-set the EntityId if it actually changes
			// otherwise nodes like Entity:EntityId will re-trigger on quickload for no reason and confusing other nodes
			EntityId oldId = *(m_pInputData[0].GetPtr<EntityId>());
			EntityId newId = *data.GetPtr<EntityId>();

			if (oldId != newId)
			{
				SetEntityId(newId);
				pActInfo->pGraph->ActivateNode(pActInfo->myID);
			}
		}

		if (m_forwardingEntityID != 0)
		{
			m_failedGettingFlowgraphForwardingEntity = true;
		}
	}

	char idName[16] = "in_x";
	for (int i = startIndex; i < m_nInputs; i++)
	{
		itoa(i, &idName[3], 10);
		ser.ValueWithDefault(idName, m_pInputData[i], config.pInputPorts[i].defaultData);
	}

	CompleteActivationInfo(pActInfo);
	m_pImpl->Serialize(pActInfo, ser);
}

void CFlowData::PostSerialize(IFlowNode::SActivationInfo* pActInfo)
{
	if (!pActInfo->pEntity && !m_entityGuid.IsNull())
	{
		// Try to bind entity id.
		EntityId entityId = gEnv->pEntitySystem->FindEntityByGuid(m_entityGuid);
		if (entityId != INVALID_ENTITYID)
		{
			if (SetEntityId(entityId))
				pActInfo->pGraph->ActivateNode(pActInfo->myID);
		}
		else
		{
			EntityId graphEntityId = pActInfo->pGraph->GetGraphEntity(0);
			IEntity* pGraphEntity = gEnv->pEntitySystem->GetEntity(graphEntityId);
			GameWarning("Flow Graph Node targets unknown entity guid: %s, graph entity id: %d name '%s'", m_entityGuid.ToDebugString(), graphEntityId, pGraphEntity ? pGraphEntity->GetName() : "[NULL]");
		}
	}

	CompleteActivationInfo(pActInfo);
	m_pImpl->PostSerialize(pActInfo);
	if (m_forwardingEntityID != 0)
	{
		pActInfo->pEntity = gEnv->pEntitySystem->GetEntity(m_forwardingEntityID);
		if (pActInfo->pEntity)
			m_pImpl->ProcessEvent(IFlowNode::eFE_SetEntityId, pActInfo);
	}
}

void CFlowData::FlagInputPorts()
{
	for (int i = 0; i < m_nInputs; i++)
		m_pInputData[i].SetUserFlag(true);
}

bool CFlowData::SetEntityId(const EntityId id)
{
	if (m_hasEntity)
	{
		m_pInputData[0].Set(id);
		m_pInputData[0].SetUserFlag(true);

		if (m_getFlowgraphForwardingEntity)
			gEnv->pScriptSystem->ReleaseFunc(m_getFlowgraphForwardingEntity);
		m_getFlowgraphForwardingEntity = 0;
		m_failedGettingFlowgraphForwardingEntity = true;
		if (id == 0)
			m_forwardingEntityID = 0;
	}
	return m_hasEntity;
}

bool CFlowData::NoForwarding(IFlowNode::SActivationInfo* pActInfo)
{
	if (m_forwardingEntityID)
	{
		m_pImpl->ProcessEvent(IFlowNode::eFE_SetEntityId, pActInfo);

		m_forwardingEntityID = 0;
	}
	return false;
}

bool CFlowData::ForwardingActivated(IFlowNode::SActivationInfo* pActInfo, IFlowNode::EFlowEvent event)
{
	bool forwardingDone = DoForwardingIfNeed(pActInfo);

	if (forwardingDone)
	{
		m_pImpl->ProcessEvent(event, pActInfo);
		forwardingDone = HasForwarding(pActInfo);
	}

	return false;
}

bool CFlowData::DoForwardingIfNeed(IFlowNode::SActivationInfo* pActInfo)
{
	CompleteActivationInfo(pActInfo);

	bool isForwarding = m_getFlowgraphForwardingEntity != NULL;

	if (m_failedGettingFlowgraphForwardingEntity)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity)
		{
			SmartScriptTable scriptTable = pEntity->GetScriptTable();
			if (!!scriptTable)
			{
				scriptTable->GetValue("GetFlowgraphForwardingEntity", m_getFlowgraphForwardingEntity);
				m_failedGettingFlowgraphForwardingEntity = false;
				isForwarding = m_getFlowgraphForwardingEntity != NULL;
			}
			else
			{
				return NoForwarding(pActInfo);
			}
		}
		else
		{
			if (!m_forwardingEntityID)  // the original entity could not be accesible because is bookmarked. in this case we want to keep trying (this is rare. can happen only for waveAIs, only after a savegame load, only if that savegame happened after at least one AI was re-spawned, and only until next AI is spawned from the same original one). So performance is not a problem
				return NoForwarding(pActInfo);
			isForwarding = true;
		}
	}

	if (!isForwarding)
		return NoForwarding(pActInfo);

	IEntity* pEntity = pActInfo->pEntity;
	EntityId forwardingEntityID = 0;
	if (pEntity)
	{
		SmartScriptTable pTable = pEntity->GetScriptTable();
		if (!pTable)
			return NoForwarding(pActInfo);

		ScriptHandle ent(0);
		if (!Script::CallReturn(pTable->GetScriptSystem(), m_getFlowgraphForwardingEntity, pTable, ent))
			return NoForwarding(pActInfo);

		forwardingEntityID = (EntityId)ent.n;
	}
	else
	{
		return NoForwarding(pActInfo);
	}

	if (forwardingEntityID)
	{
		pActInfo->pEntity = gEnv->pEntitySystem->GetEntity(forwardingEntityID);

		if (m_forwardingEntityID != forwardingEntityID)
		{
			m_pImpl->ProcessEvent(IFlowNode::eFE_SetEntityId, pActInfo);

			m_forwardingEntityID = forwardingEntityID;
		}
	}

	return true;
}
