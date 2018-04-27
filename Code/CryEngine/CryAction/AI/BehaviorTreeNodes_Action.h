// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Mannequin/AnimActionTriState.h"
#include <CryAISystem/BehaviorTree/Action.h>


namespace BehaviorTree
{
// Play an animation fragment directly through Mannequin (start a fragment for a specific FragmentID), and wait until it is done
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

		RuntimeData();
		~RuntimeData();

		void ReleaseAction();
	};

	AnimateFragment();

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override;

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override;
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override;
	virtual void OnTerminate(const UpdateContext& context) override;
	virtual Status Update(const UpdateContext& context) override;

private:
	void QueueAction(const UpdateContext& context);

private:
	// The CRC value for the fragment name.
	uint32 m_fragNameCrc32;

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	// The actual fragment name.
	string m_fragName;
#endif
};
}