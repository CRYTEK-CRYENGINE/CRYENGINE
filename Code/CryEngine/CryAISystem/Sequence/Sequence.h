// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef Sequence_h
	#define Sequence_h

	#include <CryAISystem/IAIActionSequence.h>

namespace AIActionSequence
{

class Sequence
{
public:
	Sequence(EntityId entityId, SequenceId sequenceId, TFlowNodeId startFlowNodeId, SequenceProperties sequenceProperties, IFlowGraph* flowgraph)
		: m_entityId(entityId)
		, m_sequenceId(sequenceId)
		, m_startNodeId(startFlowNodeId)
		, m_sequenceProperties(sequenceProperties)
		, m_flowGraph(flowgraph)
		, m_active(false)
		, m_sequenceBehaviorReady(false)
		, m_currentActionNodeId(InvalidFlowNodeId)
		, m_bookmarkNodeId(0)
	{}

	bool       TraverseAndValidateSequence();
	void       Start();
	void       PrepareAgentSequenceBehavior();
	void       SequenceBehaviorReady();
	void       SequenceInterruptibleBehaviorLeft();
	void       Stop();
	void       Cancel();
	void       RequestActionStart(TFlowNodeId actionNodeId);
	void       ActionComplete();
	void       SetBookmark(TFlowNodeId bookmarkNodeId);

	EntityId   GetEntityId() const     { return m_entityId; }
	SequenceId GetSequenceId() const   { return m_sequenceId; }
	bool       IsActive() const        { return m_active; }
	bool       IsInterruptible() const { return m_sequenceProperties.interruptible; }

private:
	Sequence() {}

	void     AssignSequenceToActionNodes();
	void     AssignSequenceToNode(TFlowNodeId nodeId);
	void     SendEventToNode(SequenceEvent event, TFlowNodeId nodeId);
	EntityId GetEntityIdFromNode(TFlowNodeId nodeId) const;

	EntityId           m_entityId;
	SequenceId         m_sequenceId;
	TFlowNodeId        m_startNodeId;
	SequenceProperties m_sequenceProperties;
	IFlowGraph*        m_flowGraph;

	bool               m_active;
	bool               m_sequenceBehaviorReady;

	TFlowNodeId        m_currentActionNodeId;
	TFlowNodeId        m_bookmarkNodeId;
	typedef std::vector<TFlowNodeId> FlowNodeIdVector;
	FlowNodeIdVector   m_actionNodeIds;
};

} // namespace AIActionSequence

#endif //Sequence_h
