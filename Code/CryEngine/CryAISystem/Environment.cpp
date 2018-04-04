// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

#include "CodeCoverageManager.h"
#include "CodeCoverageGUI.h"
#include "GoalOpFactory.h"
#include "StatsManager.h"
#include "TacticalPointSystem/TacticalPointSystem.h"
#include "TargetSelection/TargetTrackManager.h"
#include "Walkability/WalkabilityCacheManager.h"
#include "NullAIDebugRenderer.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "BehaviorTree/BehaviorTreeGraft.h"

static CNullAIDebugRenderer nullAIRenderer;

SAIEnvironment::SAIEnvironment()
	: pActorLookUp(NULL)
	, pGoalOpFactory(NULL)
	, pObjectContainer(NULL)
#if !defined(_RELEASE)
	, pCodeCoverageTracker(NULL)
	, pCodeCoverageManager(NULL)
	, pCodeCoverageGUI(NULL)
#endif
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
	, pVisionMap(NULL)
	, pFactionMap(NULL)
	, pGroupManager(NULL)
	, pCollisionAvoidanceSystem(NULL)
	, pRayCaster(NULL)
	, pIntersectionTester(NULL)
	, pMovementSystem(NULL)
	, pSequenceManager(NULL)
	, pWalkabilityCacheManager(NULL)
	, pAIObjectManager(NULL)
	, pGraph(NULL)
	, pPathfinderNavigationSystemUser(NULL)
	, pMNMPathfinder(NULL)
	, pNavigation(NULL)
	, pClusterDetector(NULL)
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
	SAFE_DELETE(pWalkabilityCacheManager);
	SAFE_DELETE(pGoalOpFactory);
#if !defined(_RELEASE)
	SAFE_DELETE(pCodeCoverageTracker);
	SAFE_DELETE(pCodeCoverageManager);
	SAFE_DELETE(pCodeCoverageGUI);
#endif
	SAFE_DELETE(pStatsManager);
	SAFE_DELETE(pTacticalPointSystem);
	SAFE_DELETE(pTargetTrackManager);
	SAFE_DELETE(pObjectContainer);
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
