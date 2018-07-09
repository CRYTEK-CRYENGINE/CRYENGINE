// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowCompositeNode.h"
#include "../FlowSerialize.h"

using namespace NFlowCompositeHelpers;

static const char* INTERIOR_NODE_TYPE = "CompositeInterior";

/*
 * inner-node
 */

CCompositeInteriorNode::CCompositeInteriorNode(CFlowCompositeNodeFactoryPtr pFactory)
	: m_pFactory(pFactory)
	, m_pParent(nullptr)
{
}

IFlowNodePtr CCompositeInteriorNode::Clone(SActivationInfo*)
{
	return this;
}

void CCompositeInteriorNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(false, config);
}

void CCompositeInteriorNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
		{
			size_t numPorts = m_pFactory->GetInputPortCount(false);
			TFlowNodeId interfaceNode = m_pFactory->GetInterfaceNode();
			CCompositeGraph* pMyGraph = (CCompositeGraph*) pActInfo->pGraph;
			pActInfo = pMyGraph->GetParentInfo();

			for (size_t i = 0; i < numPorts; i++)
			{
				if (IsPortActive(pActInfo, i))
					pActInfo->pGraph->ActivatePort(SFlowAddress(pActInfo->myID, static_cast<TFlowPortId>(i), true), GetPortAny(pActInfo, i));
			}
		}
		break;
	}
}

bool CCompositeInteriorNode::SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading)
{
	return true;
}

void CCompositeInteriorNode::Serialize(SActivationInfo*, TSerialize ser)
{
}

void CCompositeInteriorNode::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}

/*
 * hook
 */

CHook::CHook(CFlowSystem* pFlowSystem, CFlowCompositeNodeFactoryPtr pFactory)
	: m_refs(0), m_pFlowSystem(pFlowSystem), m_pFactory(pFactory), m_id(InvalidFlowNodeId), m_bFail(false)
{
}

void CHook::Release()
{
	if (0 == --m_refs)
		delete this;
}

void CHook::AddRef()
{
	++m_refs;
}

IFlowNodePtr CHook::CreateNode(IFlowNode::SActivationInfo*, TFlowNodeTypeId typeId)
{
	if (!strcmp(INTERIOR_NODE_TYPE, m_pFlowSystem->GetTypeName(typeId)))
		return new CCompositeInteriorNode(m_pFactory);
	return IFlowNodePtr();
}

bool CHook::CreatedNode(TFlowNodeId id, const char* name, TFlowNodeTypeId typeId, IFlowNodePtr pNode)
{
	if (!strcmp(INTERIOR_NODE_TYPE, m_pFlowSystem->GetTypeName(typeId)))
	{
		if (m_id != InvalidFlowNodeId)
			m_bFail = true;
		else
			m_id = id;
	}
	return !m_bFail;
}

void CHook::CancelCreatedNode(TFlowNodeId id, const char* name, TFlowNodeTypeId typeId, IFlowNodePtr pNode)
{
	if (id == m_id)
	{
		m_id = InvalidFlowNodeId;
		m_bFail = true;
	}
}

/*
 * graph
 */

CCompositeGraph::CCompositeGraph(CFlowSystem* pSys) : CFlowGraph(pSys), m_pParent(NULL)
{
}

IFlowGraphPtr CCompositeGraph::Clone()
{
	CCompositeGraph* pClone = new CCompositeGraph(GetSys());
	CloneInner(pClone);
	return pClone;
}

IFlowNode::SActivationInfo* CCompositeGraph::GetParentInfo()
{
	return m_pParent->GetParentInfo();
}

void CCompositeGraph::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	CFlowGraph::GetMemoryUsage(s);
}

/*
 * node
 */

CFlowCompositeNode::CFlowCompositeNode(SActivationInfo* pActInfo, IFlowGraphPtr pGraph, CFlowCompositeNodeFactoryPtr pFactory)
	: m_parentInfo(*pActInfo), m_pGraph(pGraph), m_pFactory(pFactory)
{
	((CCompositeGraph*)&*pGraph)->Reparent(this);
	pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
}

IFlowNodePtr CFlowCompositeNode::Clone(SActivationInfo* pActInfo)
{
	CFlowCompositeNode* out = new CFlowCompositeNode(pActInfo, m_pGraph->Clone(), m_pFactory);
	return out;
}

void CFlowCompositeNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(true, config);
}

void CFlowCompositeNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Update:
		m_pGraph->Update();
		break;
	case eFE_Activate:
	case eFE_Initialize:
		{
			size_t numPorts = m_pFactory->GetInputPortCount(true);
			TFlowNodeId interfaceNode = m_pFactory->GetInterfaceNode();

			for (size_t i = 0; i < numPorts; i++)
			{
				if (IsPortActive(pActInfo, i))
					m_pGraph->ActivatePort(SFlowAddress(interfaceNode, static_cast<TFlowPortId>(i), true), GetPortAny(pActInfo, i));
			}
		}
		break;
	}
}

bool CFlowCompositeNode::SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading)
{
	return false;
}

void CFlowCompositeNode::Serialize(SActivationInfo*, TSerialize ser)
{
	ser.BeginGroup("ChildGraph");
	m_pGraph->Serialize(ser);
	ser.EndGroup();
}

void CFlowCompositeNode::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	m_pGraph->GetMemoryUsage(s);
}

/*
 * factory
 */

CFlowCompositeNodeFactory::CFlowCompositeNodeFactory()
{
}

CFlowCompositeNodeFactory::~CFlowCompositeNodeFactory()
{
}

CFlowCompositeNodeFactory::EInitResult CFlowCompositeNodeFactory::Init(XmlNodeRef node, CFlowSystem* pSystem)
{
	CRY_ASSERT(this->UseCount());

	XmlNodeRef ports = node->findChild("Ports");
	if (!ports)
	{
		GameWarning("[flow] Composite %s has no ports", node->getAttr("name"));
		return eIR_Failed;
	}

	int nPorts = ports->getChildCount();
	for (int i = 0; i < nPorts; i++)
	{
		XmlNodeRef port = ports->getChild(i);
		if (!port)
		{
			GameWarning("[flow] Composite %s has no port %d", node->getAttr("name"), i);
			return eIR_Failed;
		}

		const char* tag = port->getTag();
		TPortPairPtr pPorts = GeneratePortsFromXML(port);
		if (!pPorts.get())
		{
			GameWarning("[flow] Composite %s cannot generate port %d", node->getAttr("name"), i);
			return eIR_Failed;
		}

		if (0 == strcmp("Input", tag))
		{
			m_inputsExt.push_back(pPorts->first);
			m_outputsInt.push_back(pPorts->second);
		}
		else if (0 == strcmp("Output", tag))
		{
			m_inputsInt.push_back(pPorts->first);
			m_outputsExt.push_back(pPorts->second);
		}
		else
		{
			GameWarning("[flow] Composite %s - no such port type %s (on port %d)", node->getAttr("name"), tag, i);
			return eIR_Failed;
		}
	}

	SOutputPortConfig termOut = { 0 };
	m_outputsInt.push_back(termOut);
	m_outputsExt.push_back(termOut);
	SInputPortConfig termIn = { 0 };
	m_inputsInt.push_back(termIn);
	m_inputsExt.push_back(termIn);

	CHookPtr pHook = new CHook(pSystem, this);
	m_pPrototypeGraph = new CCompositeGraph(pSystem);
	m_pPrototypeGraph->RegisterHook(&*pHook);
	if (!m_pPrototypeGraph->SerializeXML(node, true))
		return eIR_NotYet;
	m_interfaceNode = pHook->GetID();
	m_pPrototypeGraph->UnregisterHook(&*pHook);
	if (m_interfaceNode == InvalidFlowNodeId)
	{
		GameWarning("[flow] Composite %s has no %s node", node->getAttr("name"), INTERIOR_NODE_TYPE);
		return eIR_Failed;
	}

	return eIR_Ok;
}

void CFlowCompositeNodeFactory::GetConfiguration(bool exterior, SFlowNodeConfig& config)
{
	if (exterior)
	{
		config.pInputPorts = &m_inputsExt[0];
		config.pOutputPorts = &m_outputsExt[0];
	}
	else
	{
		config.pInputPorts = &m_inputsInt[0];
		config.pOutputPorts = &m_outputsInt[0];
	}
}

IFlowNodePtr CFlowCompositeNodeFactory::Create(IFlowNode::SActivationInfo* pActInfo)
{
	return new CFlowCompositeNode(pActInfo, m_pPrototypeGraph->Clone(), this);
}

const string& CFlowCompositeNodeFactory::AddString(const char* str)
{
	m_stringPool.push_back(str);
	return m_stringPool.back();
}

CFlowCompositeNodeFactory::TPortPairPtr CFlowCompositeNodeFactory::GeneratePortsFromXML(XmlNodeRef port)
{
	TPortPairPtr result(new TPortPair);

	const char* c_name = port->getAttr("name");
	if (!c_name || !c_name[0])
	{
		GameWarning("[flow] Composite: Port has no name!");
		return TPortPairPtr();
	}
	const string& name = AddString(c_name);

	int type = -1;
	if (port->getAttr("type", type))
	{
		if (type != -1)
		{
			if (!result->first.defaultData.SetDefaultForTag(type))
			{
				GameWarning("[flow] Composite: No node type %d", type);
				return TPortPairPtr();
			}
			result->first.defaultData.SetLocked();
			if (!SetFromString(result->first.defaultData, port->getAttr("value")))
			{
				GameWarning("[flow] Composite: Data '%s' is invalid for type %d", port->getAttr("value"), type);
				return TPortPairPtr();
			}
		}
	}
	if (type == -1)
		result->first.defaultData.SetUnlocked();
	result->second.type = type;
	result->first.name = result->second.name = name.c_str();

	return result;
}

void CFlowCompositeNodeFactory::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "CFlowCompositeNodeFactory");
	s->Add(*this);
	s->AddObject(m_stringPool);
	s->AddObject(m_inputsInt);
	s->AddObject(m_inputsExt);
	s->AddObject(m_outputsInt);
	s->AddObject(m_outputsExt);
}

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Composite_Input : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Composite_Input(SActivationInfo* pActInfo) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("in"),
			{ 0 }
		};
		config.sDescription = _HELP("Input for the composite node");
		config.pInputPorts = 0;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Composite_Output : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Composite_Output(SActivationInfo* pActInfo) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("out"),
			{ 0 }
		};
		config.sDescription = _HELP("Output for the composite node");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_ADVANCED);
	}
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
	}
};

// REGISTER_FLOW_NODE("Composite:Input", CFlowNode_Composite_Input );
// REGISTER_FLOW_NODE("Composite:Output", CFlowNode_Composite_Output );
