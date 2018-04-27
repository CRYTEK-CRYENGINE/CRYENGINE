// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOWFILTERS_H__
#define __FLOWFILTERS_H__

#include <CryFlowGraph/IFlowSystem.h>

class CFlowFilterBase : public IFlowGraphInspector::IFilter
{
public:
	CFlowFilterBase() : m_refs(0) {}

	virtual void AddRef()
	{
		++m_refs;
	}

	virtual void Release()
	{
		if (--m_refs <= 0)
			delete this;
	}

	virtual void AddFilter(IFlowGraphInspector::IFilterPtr pFilter)
	{
		stl::push_back_unique(m_filters, pFilter);
	}

	virtual void RemoveFilter(IFlowGraphInspector::IFilterPtr pFilter)
	{
		stl::find_and_erase(m_filters, pFilter);
	}

protected:
	int m_refs;
	IFlowGraphInspector::IFilter_AutoArray m_filters;
};

class CFlowFilterGraph : public CFlowFilterBase
{
public:
	// filter lets only pass flows which happen in this graph
	CFlowFilterGraph(IFlowGraph* pGraph);

	virtual EFilterResult Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to);

protected:
	IFlowGraph* m_pGraph;
};

class CFlowFilterAIAction : public CFlowFilterBase
{
public:
	// filter lets only pass AIAction flows with these entity INSTANCES
	CFlowFilterAIAction(IAIAction* pPrototype, IEntity* pUser, IEntity* pObject);

	// filter lets only pass AIAction flows with these entity CLASSES
	CFlowFilterAIAction(IAIAction* pPrototype, IEntityClass* pUserClass, IEntity* pObjectClass);

	virtual EFilterResult Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to);

protected:
	string   m_actionName;
	string   m_userClass;
	string   m_objectClass;
	EntityId m_userId;    // will be 0 if we run on all entity classes
	EntityId m_objectId;  // will be 0 if we run on all entity classes
};

class CFlowFilterNode : public CFlowFilterBase
{
public:
	// filter lets only pass flows for nodes of a specific type and node id
	CFlowFilterNode(const char* pNodeTypeName, TFlowNodeId id);

	virtual EFilterResult Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to);

protected:
	string      m_nodeType;
	TFlowNodeId m_nodeId;
};
#endif

