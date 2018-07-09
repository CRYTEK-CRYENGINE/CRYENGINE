// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   AIEnvironment.cpp
//  Created:     29/02/2008 by Matthew
//  Description: Simple global struct for accessing major module pointers
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Environment.h"

#include "ObjectContainer.h"

#include "GoalOpFactory.h"
#include "StatsManager.h"
#include "TacticalPointSystem/TacticalPointSystem.h"
#include "TargetSelection/TargetTrackManager.h"
#include "NullAIDebugRenderer.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "BehaviorTree/BehaviorTreeGraft.h"
#include "Formation/FormationManager.h"

static CNullAIDebugRenderer nullAIRenderer;

SAIEnvironment::SAIEnvironment()
	: pActorLookUp(NULL)
	, pGoalOpFactory(NULL)
	, pObjectContainer(NULL)
	, pTacticalPointSystem(NULL)
	, pTargetTrackManager(NULL)
	, pStatsManager(NULL)
	, pPipeManager(NULL)
	, pAIActionManager(NULL)
	, pSmartObjectManager(NULL)
	, pCommunicationManager(NULL)
	, pCoverSystem(NULL)
	, pNavigationSystem(NULL)
	, pBehaviorTreeManager(NULL)
	, pGraftManager(NULL)
	, pAuditionMap(NULL)
	, pVisionMap(NULL)
	, pFactionMap(NULL)
	, pFactionSystem(NULL)
	, pGroupManager(NULL)
	, pCollisionAvoidanceSystem(NULL)
	, pRayCaster(NULL)
	, pIntersectionTester(NULL)
	, pMovementSystem(NULL)
	, pSequenceManager(NULL)
	, pAIObjectManager(NULL)
	, pPathfinderNavigationSystemUser(NULL)
	, pMNMPathfinder(NULL)
	, pNavigation(NULL)
	, pClusterDetector(NULL)
	, pFormationManager(NULL)
	, pWorld(NULL)
{
	SetDebugRenderer(0);
	SetNetworkDebugRenderer(0);

#ifdef CRYAISYSTEM_DEBUG
	pRecorder = NULL;
	pBubblesSystem = NULL;
#endif //CRYAISYSTEM_DEBUG
}

SAIEnvironment::~SAIEnvironment()
{
}

void SAIEnvironment::ShutDown()
{
	SAFE_DELETE(pActorLookUp);
	SAFE_DELETE(pFactionMap);
	SAFE_DELETE(pFactionSystem);
	SAFE_DELETE(pGoalOpFactory);
	SAFE_DELETE(pStatsManager);
	SAFE_DELETE(pTacticalPointSystem);
	SAFE_DELETE(pTargetTrackManager);
	SAFE_DELETE(pObjectContainer);
	SAFE_DELETE(pFormationManager);
}

IAIDebugRenderer* SAIEnvironment::GetDebugRenderer()
{
	return pDebugRenderer;
}

IAIDebugRenderer* SAIEnvironment::GetNetworkDebugRenderer()
{
	return pNetworkDebugRenderer;
}

void SAIEnvironment::SetDebugRenderer(IAIDebugRenderer* pAIDebugRenderer)
{
	pDebugRenderer = pAIDebugRenderer ? pAIDebugRenderer : &nullAIRenderer;
}

void SAIEnvironment::SetNetworkDebugRenderer(IAIDebugRenderer* pAINetworkDebugRenderer)
{
	pNetworkDebugRenderer = pAINetworkDebugRenderer ? pAINetworkDebugRenderer : &nullAIRenderer;
}

#ifdef CRYAISYSTEM_DEBUG

CAIRecorder* SAIEnvironment::GetAIRecorder()
{
	return pRecorder;
}

void SAIEnvironment::SetAIRecorder(CAIRecorder* pAIRecorder)
{
	pRecorder = pAIRecorder;
}

#endif //CRYAISYSTEM_DEBUG

SAIEnvironment gAIEnv;
