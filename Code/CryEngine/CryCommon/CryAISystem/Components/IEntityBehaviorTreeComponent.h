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

	virtual bool IsRunning() const = 0;
	virtual void SendEvent(const char* szEventName) = 0;
	
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
};