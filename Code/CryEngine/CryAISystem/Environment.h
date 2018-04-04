// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   AIEnvironment.h
//  Created:     29/02/2008 by Matthew
//  Description: Simple global struct for accessing major module pointers
//  Notes:       auto_ptrs to handle init/delete would be great,
//               but exposing autoptrs to the rest of the code needs thought
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AIENVIRONMENT
#define __AIENVIRONMENT

#pragma once

#include <CryAISystem/IAISystem.h>

#include "Configuration.h"
#include "AIConsoleVariables.h"
#include "AISignalCRCs.h"

//////////////////////////////////////////////////////////////////////////
// AI system environment.
// For internal use only!
// Contain pointers to commonly needed modules for simple, efficient lookup
// Need a clear point in execution when these will all be valid
//////////////////////////////////////////////////////////////////////////
class ActorLookUp;
struct IGoalOpFactory;
class CObjectContainer;
class CCodeCoverageTracker;
class CCodeCoverageManager;
class CCodeCoverageGUI;
class CStatsManager;
class CTacticalPointSystem;
class CTargetTrackManager;
struct IAIDebugRenderer;
class CPipeManager;
class CGraph;
namespace MNM
{
class PathfinderNavigationSystemUser;
}
class CMNMPathfinder;
class CNavigation;
class CAIActionManager;
class CSmartObjectManager;
class CPerceptionManager;
class CCommunicationManager;
class CCoverSystem;
namespace BehaviorTree
{
class BehaviorTreeManager;
class GraftManager;
}
class CVisionMap;
class CFactionMap;
class CGroupManager;
class CollisionAvoidanceSystem;
class CAIObjectManager;
class WalkabilityCacheManager;
class NavigationSystem;
namespace AIActionSequence {
class SequenceManager;
}
class ClusterDetector;

#ifdef CRYAISYSTEM_DEBUG
class CAIRecorder;
struct IAIBubblesSystem;
#endif //CRYAISYSTEM_DEBUG

struct SAIEnvironment
{
	AIConsoleVars            CVars;
	AISIGNALS_CRC            SignalCRCs;

	SConfiguration           configuration;

	ActorLookUp*             pActorLookUp;
	WalkabilityCacheManager* pWalkabilityCacheManager;
	IGoalOpFactory*          pGoalOpFactory;
	CObjectContainer*        pObjectContainer;

#if !defined(_RELEASE)
	CCodeCoverageTracker* pCodeCoverageTracker;
	CCodeCoverageManager* pCodeCoverageManager;
	CCodeCoverageGUI*     pCodeCoverageGUI;
#endif

	CStatsManager*                       pStatsManager;
	CTacticalPointSystem*                pTacticalPointSystem;
	CTargetTrackManager*                 pTargetTrackManager;
	CAIObjectManager*                    pAIObjectManager;
	CPipeManager*                        pPipeManager;
	CGraph*                              pGraph; // superseded by NavigationSystem - remove when all links are cut
	MNM::PathfinderNavigationSystemUser* pPathfinderNavigationSystemUser;
	CMNMPathfinder*                      pMNMPathfinder; // superseded by NavigationSystem - remove when all links are cut
	CNavigation*                         pNavigation;    // superseded by NavigationSystem - remove when all links are cut
	CAIActionManager*                    pAIActionManager;
	CSmartObjectManager*                 pSmartObjectManager;

	CCommunicationManager*               pCommunicationManager;
	CCoverSystem*                        pCoverSystem;
	NavigationSystem*                    pNavigationSystem;
	BehaviorTree::BehaviorTreeManager*   pBehaviorTreeManager;
	BehaviorTree::GraftManager*          pGraftManager;
	CVisionMap*                          pVisionMap;
	CFactionMap*                         pFactionMap;
	CGroupManager*                       pGroupManager;
	CollisionAvoidanceSystem*            pCollisionAvoidanceSystem;
	struct IMovementSystem*              pMovementSystem;
	AIActionSequence::SequenceManager*   pSequenceManager;
	ClusterDetector*                     pClusterDetector;

#ifdef CRYAISYSTEM_DEBUG
	IAIBubblesSystem* pBubblesSystem;
#endif

	IAISystem::GlobalRayCaster*          pRayCaster;
	IAISystem::GlobalIntersectionTester* pIntersectionTester;

	//more cache friendly
	IPhysicalWorld* pWorld;//TODO use this more, or eliminate it.

	SAIEnvironment();
	~SAIEnvironment();

	IAIDebugRenderer* GetDebugRenderer();
	IAIDebugRenderer* GetNetworkDebugRenderer();
	void              SetDebugRenderer(IAIDebugRenderer* pAIDebugRenderer);
	void              SetNetworkDebugRenderer(IAIDebugRenderer* pAIDebugRenderer);

#ifdef CRYAISYSTEM_DEBUG
	CAIRecorder* GetAIRecorder();
	void         SetAIRecorder(CAIRecorder* pAIRecorder);
#endif //CRYAISYSTEM_DEBUG

	void ShutDown();

	void GetMemoryUsage(ICrySizer* pSizer) const {}
private:
	IAIDebugRenderer* pDebugRenderer;
	IAIDebugRenderer* pNetworkDebugRenderer;

#ifdef CRYAISYSTEM_DEBUG
	CAIRecorder* pRecorder;
#endif //CRYAISYSTEM_DEBUG
};

extern SAIEnvironment gAIEnv;

#endif // __AIENVIRONMENT
