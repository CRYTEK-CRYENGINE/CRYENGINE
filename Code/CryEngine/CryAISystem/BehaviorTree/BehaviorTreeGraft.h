// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef BehaviorTreeGraft_h
#define BehaviorTreeGraft_h

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTreeGraft.h>

namespace BehaviorTree
{
class GraftManager
	: public IGraftManager
{
public:

	GraftManager() {}
	~GraftManager() {}

	void Reset();

	void GraftNodeReady(EntityId entityId, IGraftNode* graftNode);
	void GraftNodeTerminated(EntityId entityId);
	void GraftBehaviorComplete(EntityId entityId);

	// IGraftManager
	virtual bool RunGraftBehavior(EntityId entityId, const char* behaviorName, XmlNodeRef behaviorXml, IGraftBehaviorListener* listener) override;
	virtual bool RequestGraftMode(EntityId entityId, IGraftModeListener* listener) override;
	virtual void CancelGraftMode(EntityId entityId) override;
	// ~IGraftManager

private:

	typedef VectorMap<EntityId, IGraftModeListener*> GraftModeRequestsContainer;
	GraftModeRequestsContainer m_graftModeRequests;

	typedef VectorMap<EntityId, IGraftBehaviorListener*> GraftBehaviorRequestsContainer;
	GraftBehaviorRequestsContainer m_graftBehaviorRequests;

	typedef VectorMap<EntityId, IGraftNode*> ActiveGraftNodesContainer;
	ActiveGraftNodesContainer m_activeGraftNodes;
};
}

#endif // BehaviorTreeGraft_h
