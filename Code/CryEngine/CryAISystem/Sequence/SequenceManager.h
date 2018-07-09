// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Sequence.h"

namespace AIActionSequence
{

class SequenceManager : public ISequenceManager
{
public:
	SequenceManager()
		: m_sequenceIdCounter(1)
	{
	}

	~SequenceManager()
	{
	}

	void Reset();

	// AIActionSequence::ISequenceManager
	virtual bool RegisterSequence(EntityId entityId, TFlowNodeId startNodeId, SequenceProperties sequenceProperties, IFlowGraph* flowGraph);
	virtual void UnregisterSequence(SequenceId sequenceId);

	virtual void StartSequence(SequenceId sequenceId);
	virtual void CancelSequence(SequenceId sequenceId);
	virtual bool IsSequenceActive(SequenceId sequenceId);

	virtual void SequenceBehaviorReady(EntityId entityId);
	virtual void SequenceInterruptibleBehaviorLeft(EntityId entityId);
	virtual void SequenceNonInterruptibleBehaviorLeft(EntityId entityId);
	virtual void AgentDisabled(EntityId entityId);

	virtual void RequestActionStart(SequenceId sequenceId, TFlowNodeId actionNodeId);
	virtual void ActionCompleted(SequenceId sequenceId);
	virtual void SetBookmark(SequenceId sequenceId, TFlowNodeId bookmarkNodeId);
	// ~AIActionSequence::ISequenceManager

private:
	SequenceId GenerateUniqueSequenceId();
	Sequence*  GetSequence(SequenceId sequenceId);
	void       CancelActiveSequencesForThisEntity(EntityId entityId);

	typedef std::map<SequenceId, Sequence> SequenceMap;
	SequenceMap m_registeredSequences;
	SequenceId  m_sequenceIdCounter;
};

} // namespace AIActionSequence
