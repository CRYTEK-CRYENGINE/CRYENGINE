// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AIBubblesSystem/AIBubblesSystem.h"
#include "CryAISystem/IAIActionSequence.h"

namespace AIActionSequence
{

class SequenceAgentAdapter : public IAgentAdapter
{
public:
	SequenceAgentAdapter()
		: m_pAIActor(nullptr)
	{}

	bool ValidateAgent() const
	{
		return m_pAIActor != nullptr;
	}

	virtual bool InitLocalAgent(EntityId entityID) override
	{
		m_pAIActor = nullptr;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
		if (!pEntity)
			return false;

		IAIObject* pAIObject = pEntity->GetAI();
		if (!pAIObject)
			return false;

		m_pAIActor = pAIObject->CastToCAIActor();
		if (!m_pAIActor)
		{
			stack_string message;
			message.Format("AI SequenceAgent: agent '%s' is not of type CAIActor", pEntity->GetName());
			AIQueueBubbleMessage("AI SequenceAgent: agent is not of type CAIActor", entityID, message, eBNS_LogWarning | eBNS_Balloon);
		}
		return m_pAIActor != nullptr;
	}

	virtual bool OnSequenceStarted(bool interruptible) override
	{
		if (!ValidateAgent())
			return false;

		if (interruptible)
			m_pAIActor->SetBehaviorVariable("ExecuteInterruptibleSequence", true);
		else
			m_pAIActor->SetBehaviorVariable("ExecuteSequence", true);

		return IsRunningSequenceBehavior(interruptible);
	}

	bool IsRunningSequenceBehavior(bool interruptible) const
	{
		IAIActorProxy* pAIActorProxy = m_pAIActor->GetProxy();
		assert(pAIActorProxy);
		if (!pAIActorProxy)
			return false;

		const char* szCurrentBehavior = pAIActorProxy->GetCurrentBehaviorName();
		if (!szCurrentBehavior)
			return false;

		const bool isRunningBehavior = !strcmp(szCurrentBehavior, interruptible ? "ExecuteInterruptibleSequence" : "ExecuteSequence");
		if (isRunningBehavior)
		{
			ClearGoalPipe();
		}
		return isRunningBehavior;
	}

	virtual void OnSequenceCanceled() override
	{
		if (!ValidateAgent() || !m_pAIActor->IsEnabled())
			return;

		m_pAIActor->SetBehaviorVariable("ExecuteSequence", false);
		m_pAIActor->SetBehaviorVariable("ExecuteInterruptibleSequence", false);
	}
	virtual void OnActionCompleted() override
	{
		ClearGoalPipe();
	}

	void ClearGoalPipe() const
	{
		if (IPipeUser* pPipeUser = m_pAIActor->CastToIPipeUser())
		{
			pPipeUser->SelectPipe(0, "Empty", 0, 0, true);
		}
	}

private:
	CAIActor* m_pAIActor;
};

} // namespace AIActionSequence
