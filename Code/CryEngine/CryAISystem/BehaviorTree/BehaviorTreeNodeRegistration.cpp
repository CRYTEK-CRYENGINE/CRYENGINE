// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "BehaviorTreeNodeRegistration.h"

#include "BehaviorTreeNodes_Core.h"
#include "BehaviorTreeNodes_AI.h"
#include "BehaviorTreeNodes_Helicopter.h"

namespace BehaviorTree
{
void RegisterBehaviorTreeNodes()
{
	RegisterBehaviorTreeNodes_Core();
	RegisterBehaviorTreeNodes_AI();
	RegisterBehaviorTreeNodesHelicopter();
}
}
