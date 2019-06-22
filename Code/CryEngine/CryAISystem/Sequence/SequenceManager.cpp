// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequenceManager.h"
#include "SequenceFlowNodes.h"
#include "AIBubblesSystem/AIBubblesSystem.h"

namespace AIActionSequence
{

void SequenceManager::Reset()
{
	m_registeredSequences.clear();
	m_sequenceIdCounter = 1;
}

bool SequenceManager::RegisterSequence(EntityId entityId, TFlowNodeId startNodeId, SequenceProperties sequenceProperties, IFlowGraph* flowGraph)
{
	SequenceId newSequenceId = GenerateUniqueSequenceId();
	Sequence sequence(entityId, newSequenceId, startNodeId, sequenceProperties, flowGraph);

	if (m_pAgentAdapter && !InitAgentAdapter(entityId))
		return false;

	if (!sequence.TraverseAndValidateSequence())
		return false;

	m_registeredSequences.insert(std::pair<SequenceId, Sequence>(newSequenceId, sequence));
	return true;
}

void SequenceManager::UnregisterSequence(SequenceId sequenceId)
{
	m_registeredSequences.erase(sequenceId);
}

void SequenceManager::StartSequence(SequenceId sequenceId)
{
	Sequence* sequence = GetSequence(sequenceId);
	if (!sequence)
	{
		CRY_ASSERT_MESSAGE(false, "Could not access sequence.");
		return;
	}

	CancelActiveSequencesForThisEntity(sequence->GetEntityId());

	IAgentAdapter* pAgentAdapter = InitAgentAdapter(sequence->GetEntityId());
	if (!pAgentAdapter || pAgentAdapter->OnSequenceStarted(sequence->IsInterruptible()))
	{
		sequence->SequenceBehaviorReady();
	}
	sequence->Start();
}

void SequenceManager::CancelSequence(SequenceId sequenceId)
{
	Sequence* sequence = GetSequence(sequenceId);
	if (!sequence)
	{
		CRY_ASSERT_MESSAGE(false, "Could not access sequence.");
		return;
	}

	if (sequence->IsActive())
	{
		CancelSequence(*sequence);
	}
}

void SequenceManager::CancelSequence(Sequence& sequence)
{
	sequence.Cancel();

	if (IAgentAdapter* pAgentAdapter = InitAgentAdapter(sequence.GetEntityId()))
	{
		pAgentAdapter->OnSequenceCanceled();
	}
}

bool SequenceManager::IsSequenceActive(SequenceId sequenceId)
{
	Sequence* sequence = GetSequence(sequenceId);
	if (!sequence)
		return false;

	return sequence->IsActive();
}

void SequenceManager::SequenceBehaviorReady(EntityId entityId)
{
	SequenceMap::iterator sequenceIterator = m_registeredSequences.begin();
	SequenceMap::iterator sequenceIteratorEnd = m_registeredSequences.end();
	for (; sequenceIterator != sequenceIteratorEnd; ++sequenceIterator)
	{
		Sequence& sequence = sequenceIterator->second;
		if (sequence.IsActive() && sequence.GetEntityId() == entityId)
		{
			sequence.SequenceBehaviorReady();
			return;
		}
	}

	CRY_ASSERT_MESSAGE(false, "Entity not registered with any sequence.");
}

void SequenceManager::SequenceInterruptibleBehaviorLeft(EntityId entityId)
{
	SequenceMap::iterator sequenceIterator = m_registeredSequences.begin();
	SequenceMap::iterator sequenceIteratorEnd = m_registeredSequences.end();
	for (; sequenceIterator != sequenceIteratorEnd; ++sequenceIterator)
	{
		Sequence& sequence = sequenceIterator->second;
		if (sequence.IsActive() && sequence.IsInterruptible() && sequence.GetEntityId() == entityId)
		{
			sequence.SequenceInterruptibleBehaviorLeft();
		}
	}
}

void SequenceManager::SequenceNonInterruptibleBehaviorLeft(EntityId entityId)
{
	SequenceMap::iterator sequenceIterator = m_registeredSequences.begin();
	SequenceMap::iterator sequenceIteratorEnd = m_registeredSequences.end();
	for (; sequenceIterator != sequenceIteratorEnd; ++sequenceIterator)
	{
		Sequence& sequence = sequenceIterator->second;
		if (sequence.IsActive() && !sequence.IsInterruptible() && sequence.GetEntityId() == entityId)
		{
			AIQueueBubbleMessage("AI Sequence Error", entityId, "The sequence behavior has unexpectedly been deselected and the sequence has been canceled.", eBNS_LogWarning | eBNS_Balloon);
			CancelSequence(sequence);
		}
	}
}

void SequenceManager::AgentDisabled(EntityId entityId)
{
	SequenceMap::iterator sequenceIterator = m_registeredSequences.begin();
	SequenceMap::iterator sequenceIteratorEnd = m_registeredSequences.end();
	for (; sequenceIterator != sequenceIteratorEnd; ++sequenceIterator)
	{
		Sequence& sequence = sequenceIterator->second;
		if (sequence.IsActive() && sequence.GetEntityId() == entityId)
		{
			CancelSequence(sequence);
		}
	}
}

void SequenceManager::RequestActionStart(SequenceId sequenceId, TFlowNodeId actionNodeId)
{
	Sequence* sequence = GetSequence(sequenceId);
	if (!sequence)
	{
		CRY_ASSERT_MESSAGE(false, "Could not access sequence.");
		return;
	}

	sequence->RequestActionStart(actionNodeId);
}

void SequenceManager::ActionCompleted(SequenceId sequenceId)
{
	Sequence* sequence = GetSequence(sequenceId);
	if (!sequence)
	{
		CRY_ASSERT_MESSAGE(false, "Could not access sequence.");
		return;
	}

	if (IAgentAdapter* pAgentAdapter = InitAgentAdapter(sequence->GetEntityId()))
	{
		pAgentAdapter->OnActionCompleted();
	}
	sequence->ActionComplete();
}

void SequenceManager::SetBookmark(SequenceId sequenceId, TFlowNodeId bookmarkNodeId)
{
	Sequence* sequence = GetSequence(sequenceId);
	if (!sequence)
	{
		CRY_ASSERT_MESSAGE(false, "Could not access sequence.");
		return;
	}

	sequence->SetBookmark(bookmarkNodeId);
}

void SequenceManager::SetAgentAdapter(IAgentAdapter* pAgentAdapter)
{
	m_pAgentAdapter = pAgentAdapter;
}

IAgentAdapter* SequenceManager::InitAgentAdapter(EntityId entityId)
{
	if (!m_pAgentAdapter)
		return nullptr;
	
	return m_pAgentAdapter->InitLocalAgent(entityId) ? m_pAgentAdapter : nullptr;
}

SequenceId SequenceManager::GenerateUniqueSequenceId()
{
	return m_sequenceIdCounter++;
}

Sequence* SequenceManager::GetSequence(SequenceId sequenceId)
{
	SequenceMap::iterator sequenceIterator = m_registeredSequences.find(sequenceId);
	if (sequenceIterator == m_registeredSequences.end())
	{
		return 0;
	}

	return &sequenceIterator->second;
}

void SequenceManager::CancelActiveSequencesForThisEntity(EntityId entityId)
{
	SequenceMap::iterator sequenceIterator = m_registeredSequences.begin();
	SequenceMap::iterator sequenceIteratorEnd = m_registeredSequences.end();
	for (; sequenceIterator != sequenceIteratorEnd; ++sequenceIterator)
	{
		Sequence& sequence = sequenceIterator->second;
		if (sequence.IsActive() && sequence.GetEntityId() == entityId)
		{
			CancelSequence(sequence);
		}
	}
}

} // namespace AIActionSequence
