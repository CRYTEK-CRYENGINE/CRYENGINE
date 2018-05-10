// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IBehaviorTreeGraft_h
#define IBehaviorTreeGraft_h

#pragma once

namespace BehaviorTree
{
struct IGraftNode
{
	virtual bool RunBehavior(EntityId entityId, const char* behaviorName, XmlNodeRef behaviorXmlNode) = 0;
};

struct IGraftModeListener
{
	virtual void GraftModeReady(EntityId entityId) = 0;
	virtual void GraftModeInterrupted(EntityId entityId) = 0;
};

struct IGraftBehaviorListener
{
	virtual void GraftBehaviorComplete(EntityId entityId) = 0;
};

struct IGraftManager
{
	virtual ~IGraftManager() {}

	virtual bool RunGraftBehavior(EntityId entityId, const char* behaviorName, XmlNodeRef behaviorXml, IGraftBehaviorListener* listener) = 0;
	virtual bool RequestGraftMode(EntityId entityId, IGraftModeListener* listener) = 0;
	virtual void CancelGraftMode(EntityId entityId) = 0;
};
}

#endif // IBehaviorTreeGraft_h
