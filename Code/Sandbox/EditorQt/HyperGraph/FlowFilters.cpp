// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FlowFilters.h"
#include <CryFlowGraph/IFlowSystem.h>
#include <CryAISystem/IAIAction.h>
#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntityClass.h>

CFlowFilterGraph::CFlowFilterGraph(IFlowGraph* pGraph)
{
	m_pGraph = pGraph;
}

/* virtual */ IFlowGraphInspector::IFilter::EFilterResult
CFlowFilterGraph::Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to)
{
	return pGraph == m_pGraph ? eFR_Pass : eFR_Block;
}

CFlowFilterAIAction::CFlowFilterAIAction(IAIAction* pPrototype, IEntity* pUser, IEntity* pObject)
{
	m_actionName = pPrototype->GetName();
	if (pUser)
	{
		m_userId = pUser->GetId();
		m_userClass = pUser->GetClass()->GetName();
	}
	else
		m_userId = 0;

	if (pObject)
	{
		m_objectId = pObject->GetId();
		m_objectClass = pObject->GetClass()->GetName();
	}
	else
		m_objectId = 0;
}

CFlowFilterAIAction::CFlowFilterAIAction(IAIAction* pPrototype, IEntityClass* pUserClass, IEntity* pObjectClass)
{
	m_actionName = pPrototype->GetName();
	m_userId = 0;
	m_objectId = 0;
	if (pUserClass)
		m_userClass = pUserClass->GetName();
	if (pObjectClass)
		m_objectClass = pObjectClass->GetName();
}

/* virtual */ IFlowGraphInspector::IFilter::EFilterResult
CFlowFilterAIAction::Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to)
{
	IAIAction* pAction = pGraph->GetAIAction();

	// no AI action
	if (!pAction)
		return eFR_Block;

	// it's an AI action, but not the one we're interested in -> block
	if (m_actionName.compare(pAction->GetName()) != 0)
		return eFR_Block;

	IEntity* pUser = pAction->GetUserEntity();
	// check for specific user entity
	if (m_userId > 0)
	{
		if (!pUser) return eFR_Block;                     // there is no user at all
		if (pUser->GetId() != m_userId) return eFR_Block; // user entity does not match
	}
	// otherwise check for the user's class if applicable
	else if (!m_userClass.empty())
	{
		if (!pUser) return eFR_Block; // no user at all
		if (m_userClass.compare(pUser->GetClass()->GetName()) != 0) return eFR_Block;
	}

	IEntity* pObject = pAction->GetObjectEntity();
	// check for specific object entity
	if (m_objectId > 0)
	{
		if (!pObject) return eFR_Block;
		if (pObject->GetId() != m_objectId) return eFR_Block;
	}
	// otherwise check for the object's class if applicable
	else if (!m_objectClass.empty())
	{
		if (!pObject) return eFR_Block; // no object at all
		if (m_objectClass.compare(pObject->GetClass()->GetName()) != 0) return eFR_Block;
	}

	// it passed all tests and no subfilters -> great
	if (m_filters.empty())
		return eFR_Pass;

	// ask sub-filters: if anyone says yes, so do we (basically ORing the sub-filter results)
	// otherwise we block
	IFilter::EFilterResult res = eFR_Block;
	IFlowGraphInspector::IFilter_AutoArray::iterator iter(m_filters.begin());
	IFlowGraphInspector::IFilter_AutoArray::iterator end(m_filters.end());
	while (iter != end)
	{
		res = (*iter)->Apply(pGraph, from, to);
		if (res == eFR_Pass)
			break;
		++iter;
	}
	return res;
}

CFlowFilterNode::CFlowFilterNode(const char* pNodeTypeName, TFlowNodeId id)
{
	assert(pNodeTypeName != 0);
	m_nodeType = pNodeTypeName;
	m_nodeId = id;
}

/* virtual */ IFlowGraphInspector::IFilter::EFilterResult
CFlowFilterNode::Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to)
{
	if (from.node == m_nodeId || to.node == m_nodeId)
	{
		if (m_nodeType.compare(pGraph->GetNodeTypeName(m_nodeId)) != 0) return eFR_Block;
	}
	return eFR_Pass;
}

