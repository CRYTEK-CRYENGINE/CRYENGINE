// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_Action.h"
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/BehaviorTree/Action.h>
#include <CryAISystem/BehaviorTree/Decorator.h>
#include "IActorSystem.h"
#include "Mannequin/AnimActionTriState.h"
#include <CrySystem/XML/XMLAttrReader.h>

namespace BehaviorTree
{
// Play an animation fragment directly through Mannequin (start a fragment
// for a specific FragmentID), and wait until it is done.
class AnimateFragment : public Action
{
public:
	// TODO: Move mannequin priorities into CryAction or get this from a central location
	static const int NORMAL_ACTION_PRIORITY = 3;   // currently equal to PP_Action, just above movement, but underneath urgent actions, hit death, etc.
	static const int URGENT_ACTION_PRIORITY = 4;   // currently equal to PP_Urgent_Action, just above normal actions, but underneath hit death, etc.

	struct RuntimeData
	{
		// The action animation action that is running or NULL if none.
		typedef _smart_ptr<CAnimActionTriState> CAnimActionTriStatePtr;
		CAnimActionTriStatePtr action;

		// True if the action was queued; false if an error occured.
		bool actionQueuedFlag;

		RuntimeData()
			: actionQueuedFlag(false)
		{
		}

		~RuntimeData()
		{
			ReleaseAction();
		}

		void ReleaseAction()
		{
			if (this->action.get() != NULL)
			{
				this->action->ForceFinish();
				this->action.reset();
			}
		}
	};

	AnimateFragment() :
		m_fragNameCrc32(0)
#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		, m_fragName()
#endif
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
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
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText = m_fragName;
	}
#endif

protected:

	virtual void OnInitialize(const UpdateContext& context) override
	{
		QueueAction(context);
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.ReleaseAction();
	}

	virtual Status Update(const UpdateContext& context) override
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

private:

	void QueueAction(const UpdateContext& context)
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

private:

	// The CRC value for the fragment name.
	uint32 m_fragNameCrc32;

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	// The actual fragment name.
	string m_fragName;
#endif
};
}

void RegisterActionBehaviorTreeNodes()
{
	using namespace BehaviorTree;

	assert(gEnv->pAISystem->GetIBehaviorTreeManager());

	IBehaviorTreeManager& manager = *gEnv->pAISystem->GetIBehaviorTreeManager();

	REGISTER_BEHAVIOR_TREE_NODE(manager, AnimateFragment);
}
