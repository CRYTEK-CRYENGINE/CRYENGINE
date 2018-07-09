// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../FlowGraph.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowSystem;

class CFlowCompositeNodeFactory;
TYPEDEF_AUTOPTR(CFlowCompositeNodeFactory);
typedef CFlowCompositeNodeFactory_AutoPtr CFlowCompositeNodeFactoryPtr;

class CFlowCompositeNode;

namespace NFlowCompositeHelpers
{

class CHook;
TYPEDEF_AUTOPTR(CHook);
typedef CHook_AutoPtr CHookPtr;

class CHook : public IFlowGraphHook
{
public:
	CHook(CFlowSystem*, CFlowCompositeNodeFactoryPtr);
	virtual void                        AddRef();
	virtual void                        Release();
	virtual IFlowNodePtr                CreateNode(IFlowNode::SActivationInfo*, TFlowNodeTypeId typeId);
	virtual bool                        CreatedNode(TFlowNodeId id, const char* name, TFlowNodeTypeId typeId, IFlowNodePtr pNode);
	virtual void                        CancelCreatedNode(TFlowNodeId id, const char* name, TFlowNodeTypeId typeId, IFlowNodePtr pNode);
	virtual IFlowGraphHook::EActivation PerformActivation(IFlowGraphPtr pFlowgraph, TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const TFlowInputData* value) { return eFGH_Pass; };
	TFlowNodeId                         GetID() const                                                                                                                                              { return m_bFail ? InvalidFlowNodeId : m_id; }

private:
	int                          m_refs;
	CFlowSystem*                 m_pFlowSystem;
	CFlowCompositeNodeFactoryPtr m_pFactory;
	TFlowNodeId                  m_id;
	bool                         m_bFail;
};

class CCompositeInteriorNode : public IFlowNode
{
public:
	CCompositeInteriorNode(CFlowCompositeNodeFactoryPtr pFactory);

	virtual IFlowNodePtr Clone(SActivationInfo*);

	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading);
	virtual void         Serialize(SActivationInfo*, TSerialize ser);
	virtual void         PostSerialize(SActivationInfo*) {}

	virtual void         GetMemoryUsage(ICrySizer* s) const;

private:
	CFlowCompositeNodeFactoryPtr m_pFactory;
	CFlowCompositeNode*          m_pParent;
};

class CCompositeGraph : public CFlowGraph
{
public:
	CCompositeGraph(CFlowSystem*);
	virtual IFlowGraphPtr       Clone();
	void                        Reparent(CFlowCompositeNode* pParent) { m_pParent = pParent; }
	IFlowNode::SActivationInfo* GetParentInfo();

	virtual void                GetMemoryUsage(ICrySizer* s) const;

private:
	CFlowCompositeNode* m_pParent;
};

TYPEDEF_AUTOPTR(CCompositeGraph);
typedef CCompositeGraph_AutoPtr CCompositeGraphPtr;

}

class CFlowCompositeNode : public IFlowNode
{
public:
	CFlowCompositeNode(SActivationInfo* pActInfo, IFlowGraphPtr pGraph, CFlowCompositeNodeFactoryPtr pFactory);

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo*);

	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading);
	virtual void         Serialize(SActivationInfo*, TSerialize ser);
	virtual void         PostSerialize(SActivationInfo*) {}
	// ~IFlowNode

	virtual void     GetMemoryUsage(ICrySizer* s) const;

	SActivationInfo* GetParentInfo() { return &m_parentInfo; }

private:
	SActivationInfo              m_parentInfo;
	IFlowGraphPtr                m_pGraph;
	CFlowCompositeNodeFactoryPtr m_pFactory;
};

class CFlowCompositeNodeFactory : public IFlowNodeFactory
{
public:
	CFlowCompositeNodeFactory();
	~CFlowCompositeNodeFactory();

	enum EInitResult
	{
		eIR_NotYet,
		eIR_Ok,
		eIR_Failed
	};

	EInitResult       Init(XmlNodeRef node, CFlowSystem* pSystem);
	void              GetConfiguration(bool exterior, SFlowNodeConfig&);
	ILINE TFlowNodeId GetInterfaceNode() const               { return m_interfaceNode; }
	ILINE size_t      GetInputPortCount(bool exterior) const { return (exterior ? m_inputsExt : m_inputsInt).size(); }

	// IFlowNodeFactory
	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo);

	virtual void         GetMemoryUsage(ICrySizer* s) const;

	void                 Reset() {}
	// ~IFlowNodeFactory

private:
	NFlowCompositeHelpers::CCompositeGraphPtr m_pPrototypeGraph;
	TFlowNodeId   m_interfaceNode;

	const string& AddString(const char* str);
	typedef std::pair<SInputPortConfig, SOutputPortConfig> TPortPair;
	typedef std::unique_ptr<TPortPair>                     TPortPairPtr;
	TPortPairPtr GeneratePortsFromXML(XmlNodeRef port);

	std::vector<string>            m_stringPool;
	std::vector<SInputPortConfig>  m_inputsInt;
	std::vector<SOutputPortConfig> m_outputsInt;
	std::vector<SInputPortConfig>  m_inputsExt;
	std::vector<SOutputPortConfig> m_outputsExt;
};
