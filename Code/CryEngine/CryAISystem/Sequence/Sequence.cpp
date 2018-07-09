// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Sequence.h"
#include "SequenceAgent.h"
#include "SequenceFlowNodes.h"
#include "AIBubblesSystem/AIBubblesSystem.h"

namespace
{
typedef std::set<TFlowNodeId> FlowNodeIdSet;
}

namespace AIActionSequence
{

bool Sequence::TraverseAndValidateSequence()
{
	assert(m_flowGraph);
	if (!m_flowGraph)
		return false;

	m_actionNodeIds.clear();

	FlowNodeIdSet nodesToVisit;
	FlowNodeIdSet visitedNodes;
	nodesToVisit.insert(m_startNodeId);

	IFlowEdgeIteratorPtr edgeIterator;
	IFlowEdgeIterator::Edge edge;

	while (!nodesToVisit.empty())
	{
		FlowNodeIdSet::iterator it = nodesToVisit.begin();
		TFlowNodeId visitingNodeId = *it;

		SFlowNodeConfig visitingNodeConfig;
		m_flowGraph->GetNodeConfiguration(visitingNodeId, visitingNodeConfig);
		uint32 typeFlags = visitingNodeConfig.GetTypeFlags();

		if (!(typeFlags & EFLN_AISEQUENCE_ACTION || typeFlags & EFLN_AISEQUENCE_SUPPORTED || typeFlags & EFLN_AISEQUENCE_END))
		{
			CryStackStringT<char, 256> message;
			message.Format("AI Sequence failed to register. It contains a flownode not supported to by the AI Sequence system : %s", m_flowGraph->GetNodeTypeName(visitingNodeId));
			AIQueueBubbleMessage("AI Sequence - Flownode not supported", m_entityId, message, eBNS_LogWarning | eBNS_Balloon | eBNS_BlockingPopup);
			return false;
		}

		const EntityId entityIdFromNode = GetEntityIdFromNode(visitingNodeId);

		if ((typeFlags & EFLN_AISEQUENCE_ACTION || typeFlags & EFLN_AISEQUENCE_END) && entityIdFromNode == m_entityId)
		{
			m_actionNodeIds.push_back(visitingNodeId);
		}

		visitedNodes.insert(visitingNodeId);
		nodesToVisit.erase(it);

		// Don't check connection past the end nodes
		if (typeFlags & EFLN_AISEQUENCE_END)
			continue;

		edgeIterator = m_flowGraph->CreateEdgeIterator();
		while (edgeIterator->Next(edge))
		{
			if (edge.fromNodeId != visitingNodeId)
				continue;

			FlowNodeIdSet::iterator nodesIt = visitedNodes.find(edge.toNodeId);
			if (nodesIt != visitedNodes.end())
				continue;

			nodesToVisit.insert(edge.toNodeId);
		}
	}

	AssignSequenceToActionNodes();
	return true;
}

void Sequence::Start()
{
	assert(!m_active);
	PrepareAgentSequenceBehavior();
	m_active = true;
	m_bookmarkNodeId = 0;
}

void Sequence::PrepareAgentSequenceBehavior()
{
	SequenceAgent agent(m_entityId);
	agent.SetSequenceBehavior(m_sequenceProperties.interruptible);

	if (agent.IsRunningSequenceBehavior(m_sequenceProperties.interruptible))
	{
		agent.ClearGoalPipe();
		SequenceBehaviorReady();
	}
}

void Sequence::SequenceBehaviorReady()
{
	if (m_bookmarkNodeId)
	{
		SendEventToNode(TriggerBookmark, m_bookmarkNodeId);
	}
	else
	{
		SendEventToNode(StartSequence, m_startNodeId);
	}
}

void Sequence::SequenceInterruptibleBehaviorLeft()
{
	if (m_sequenceProperties.resumeAfterInterrupt)
	{
		Stop();
	}
	else
	{
		Cancel();
	}
}

void Sequence::Stop()
{
	if (m_currentActionNodeId != InvalidFlowNodeId)
		SendEventToNode(SequenceStopped, m_currentActionNodeId);

	m_currentActionNodeId = InvalidFlowNodeId;
}

void Sequence::Cancel()
{
	Stop();
	m_active = false;
	SequenceAgent agent(m_entityId);
	agent.ClearSequenceBehavior();
}

void Sequence::RequestActionStart(TFlowNodeId actionNodeId)
{
	CRY_ASSERT_MESSAGE(actionNodeId != InvalidFlowNodeId, "Sequence::RequestActionStart: clash between passed in actionNodeId and the magic value that specifies an invalid node");

	if (!m_active || actionNodeId == m_currentActionNodeId)
		return;

	if (m_currentActionNodeId == InvalidFlowNodeId)
	{
		m_currentActionNodeId = actionNodeId;
		SendEventToNode(StartAction, actionNodeId);
	}
	else
	{
		CryStackStringT<char, 256> message;
		message.Format("AI Sequence action (%s) failed to start. There is already an active action (%s) running in this sequence.", m_flowGraph->GetNodeTypeName(actionNodeId), m_flowGraph->GetNodeTypeName(m_currentActionNodeId));
		AIQueueBubbleMessage("AI Sequence - Action failed to start", m_entityId, message, eBNS_LogWarning | eBNS_Balloon | eBNS_BlockingPopup);
	}
}

void Sequence::ActionComplete()
{
	SequenceAgent agent(m_entityId);
	agent.ClearGoalPipe();
	m_currentActionNodeId = InvalidFlowNodeId;
}

void Sequence::SetBookmark(TFlowNodeId bookmarkNodeId)
{
	if (m_active)
	{
		m_bookmarkNodeId = bookmarkNodeId;
	}
}

void Sequence::AssignSequenceToActionNodes()
{
	FlowNodeIdVector::iterator it = m_actionNodeIds.begin();
	FlowNodeIdVector::iterator itEnd = m_actionNodeIds.end();
	for (; it != itEnd; ++it)
	{
		AssignSequenceToNode(*it);
	}
}

void Sequence::AssignSequenceToNode(TFlowNodeId nodeId)
{
	IFlowNodeData* flowNodeData = m_flowGraph->GetNodeData(nodeId);
	assert(flowNodeData);
	SequenceFlowNodeBase* flowNode = static_cast<SequenceFlowNodeBase*>(flowNodeData->GetNode());
	assert(flowNode);
	flowNode->AssignSequence(m_sequenceId, m_entityId);
}

void Sequence::SendEventToNode(SequenceEvent event, TFlowNodeId nodeId)
{
	IFlowNodeData* flowNodeData = m_flowGraph->GetNodeData(nodeId);
	assert(flowNodeData);
	SequenceFlowNodeBase* flowNode = static_cast<SequenceFlowNodeBase*>(flowNodeData->GetNode());
	assert(flowNode);
	flowNode->HandleSequenceEvent(event);
}

EntityId Sequence::GetEntityIdFromNode(TFlowNodeId nodeId) const
{
	IFlowNodeData* flowNodeData = m_flowGraph->GetNodeData(nodeId);
	assert(flowNodeData);
	if (!flowNodeData)
		return 0;

	EntityId entityId = flowNodeData->GetCurrentForwardingEntity();
	if (entityId == (EntityId)0)
	{
		entityId = m_flowGraph->GetEntityId(nodeId);
		// If the sequence is using the "graph entity" as entity id of the nodes
		// then we need to call the GetGraphEntity to retrieve the correct entity id
		if (entityId == EFLOWNODE_ENTITY_ID_GRAPH1 || entityId == EFLOWNODE_ENTITY_ID_GRAPH2)
		{
			const int entityIndex = EFLOWNODE_ENTITY_ID_GRAPH1 ? 0 : 1;
			entityId = m_flowGraph->GetGraphEntity(entityIndex);
		}
	}

	return entityId;
}

} // namespace AIActionSequence
