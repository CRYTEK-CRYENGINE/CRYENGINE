// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_Action.h"
#include <CryAISystem/BehaviorTree/Decorator.h>
#include "IActorSystem.h"
#include <CrySystem/XML/XMLAttrReader.h>

namespace BehaviorTree
{

AnimateFragment::RuntimeData::RuntimeData()
	: actionQueuedFlag(false)
{
}

AnimateFragment::RuntimeData::~RuntimeData()
{
	ReleaseAction();
}

void AnimateFragment::RuntimeData::ReleaseAction()
{
	if (this->action.get() != NULL)
	{
		this->action->ForceFinish();
		this->action.reset();
	}
}

AnimateFragment::AnimateFragment() :
	m_fragNameCrc32(0)
{
}

LoadResult AnimateFragment::LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
{
	const string fragName = xml->getAttr("name");
	IF_UNLIKELY (fragName.empty())
	{
		ErrorReporter(*this, context).LogError("Missing attribute 'name'.");
		return LoadFailure;
	}
#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	m_fragName = fragName;
#endif
	m_fragNameCrc32 = CCrc32::ComputeLowercase(fragName);

	return LoadSuccess;
}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
void AnimateFragment::GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
{
	debugText = m_fragName;
}
#endif

void AnimateFragment::OnInitialize(const UpdateContext& context)
{
	QueueAction(context);
}

void AnimateFragment::OnTerminate(const UpdateContext& context)
{
	RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
	runtimeData.ReleaseAction();
}

Status AnimateFragment::Update(const UpdateContext& context)
{
	RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

	IF_UNLIKELY (!runtimeData.actionQueuedFlag)
	{
		return Failure;
	}

	assert(runtimeData.action.get() != NULL);
	const IAction::EStatus currentActionStatus = runtimeData.action->GetStatus();
	IF_UNLIKELY ((currentActionStatus == IAction::None) || (currentActionStatus == IAction::Finished))
	{
		return Success;
	}

	return Running;
}

void AnimateFragment::QueueAction(const UpdateContext& context)
{
	RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

	IActor* actor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(context.entityId);

	IF_UNLIKELY (actor == NULL)
	{
		assert(false);
		return;
	}

	IAnimatedCharacter* animChar = actor->GetAnimatedCharacter();
	IF_UNLIKELY (animChar == NULL)
	{
		assert(false);
		return;
	}

	IF_LIKELY (runtimeData.action.get() == NULL)
	{
		IActionController* pIActionController = animChar->GetActionController();
		const FragmentID fragID = pIActionController->GetFragID(m_fragNameCrc32);
		IF_LIKELY (fragID == FRAGMENT_ID_INVALID)
		{
#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
			ErrorReporter(*this, context).LogError("Invalid fragment name '%s'", m_fragName.c_str());
#else
			ErrorReporter(*this, context).LogError("Invalid fragment name!");
#endif
			return;
		}
		runtimeData.action = new CAnimActionTriState(NORMAL_ACTION_PRIORITY, fragID, *animChar, true /*oneshot*/);
		pIActionController->Queue(*runtimeData.action);
	}

	runtimeData.actionQueuedFlag = true;
}

}