// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

struct IEntityBehaviorTreeComponent : public IEntityComponent
{
	static void ReflectType(Schematyc::CTypeDesc<IEntityBehaviorTreeComponent>& desc)
	{
		desc.SetGUID("C6B972F7-2878-4498-8257-C7D62653FA50"_cry_guid);
	}

	//! Check if the behavior tree is running.
	//! \return True if the behavior tree is running.
	virtual bool IsRunning() const = 0;

	//! Sends an event to the behavior tree, which will handle it.
	//! \param szEventName Name of the event to be sent.
	//! \note The event may trigger a change in the behavior tree execution logic, such as a variable change, transition to a different state, etc.
	virtual void SendEvent(const char* szEventName) = 0;
	
	//! Sets the value of the given variable name. If the variable exists, it updates the value. If it does not it creates a new entry in the blackboard.
	//! \param szKeyName Name of the variable.
	//! \param value Value of the variable.
	//! \return True if successfully set.
	//! \note It may fail to set the variable value if the id of the provided variable exists but its value has a different type.
	template <class TValue>
	bool SetBBKeyValue(const char* szKeyName, const TValue& value)
	{		
		if (szKeyName == nullptr)
			return false;
		
		if (!IsRunning())
			return false;

		if (BehaviorTree::Blackboard* pBlackboard = gEnv->pAISystem->GetIBehaviorTreeManager()->GetBehaviorTreeBlackboard(GetEntityId()))
		{
			return pBlackboard->SetVariable(BehaviorTree::BlackboardVariableId(szKeyName), value);
		}
		return false;
	}

	//! Gets the value of a blackboard entry by name if it exists.
	//! \param szKeyName Name of the variable.
	//! \param value Value of the variable.
	//! \return True if successfully retrieved the value.
	//! \note It may fail to get the variable value if the id of the provided variable does not exist or if it exists but its value has a different type.
	template <class TValue>
	bool GetBBKeyValue(const char* szKeyName, TValue& value) const
	{
		if (szKeyName == nullptr)
			return false;

		if (!IsRunning())
			return false;

		if (BehaviorTree::Blackboard* pBlackboard = gEnv->pAISystem->GetIBehaviorTreeManager()->GetBehaviorTreeBlackboard(GetEntityId()))
		{
			return pBlackboard->GetVariable(BehaviorTree::BlackboardVariableId(szKeyName), value);
		}
		return false;
	}
};