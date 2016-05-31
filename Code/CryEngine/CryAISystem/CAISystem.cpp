// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   CAISystem.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 2001				: Created by Petar Kotevski
   - 2003				: Taken over by Kirill Bulatsev

 *********************************************************************/

#include "StdAfx.h"

#include "CAISystem.h"
#include "AILog.h"

#include "ScriptBind_AI.h"

#include "AIVehicle.h"
#include "AIPlayer.h"
#include "ObjectContainer.h"
#include "CodeCoverageManager.h"
#include "CodeCoverageGUI.h"
#include <CryAISystem/IVisionMap.h>
#include <CryAISystem/HidespotQueryContext.h>
#include <CryAISystem/AISystemListener.h>
#include "PerceptionManager.h"
#include "SmartObjects.h"
#include "CalculationStopper.h"
#include "AIRadialOcclusion.h"
#include "AIActions.h"
#include "GraphNodeManager.h"
#include "FireCommand.h"
#include "CentralInterestManager.h"
#include "TacticalPointSystem/TacticalPointSystem.h"
#include "TargetSelection/TargetTrackManager.h"
#include "Communication/CommunicationManager.h"
#include "Cover/CoverSystem.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "PathfinderNavigationSystemUser.h"
#include "SelectionTree/SelectionTreeManager.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include "BehaviorTree/BehaviorTreeNodeRegistration.h"
#include "CollisionAvoidance/CollisionAvoidanceSystem.h"
#include "BehaviorTree/BehaviorTreeGraft.h"
#include <CryAISystem/IMovementSystem.h>
#include "Movement/MovementSystemCreator.h"
#include "Group/GroupManager.h"
#include "Factions/FactionMap.h"
#include "Walkability/WalkabilityCacheManager.h"
#include "DebugDrawContext.h"
#include "FlightNavRegion2.h"
#include "Sequence/SequenceManager.h"
#include "ClusterDetector.h"
#include "SmartPathFollower.h"

#include "AIObjectIterators.h"

#include <CrySystem/File/CryBufferedFileReader.h>

#include "GoalOpFactory.h"
#include "StatsManager.h"
#include "GameSpecific/GoalOp_G02.h"     //TODO move these out of AISystem
#include "GameSpecific/GoalOp_G04.h"     //TODO move these out of AISystem
#include "GameSpecific/GoalOp_Crysis2.h" //TODO move these out of AISystem

#ifdef CRYAISYSTEM_DEBUG
	#include "AIBubblesSystem/AIBubblesSystem.h"
#endif

#include "FlowNodes/AIFlowBaseNode.h"
#include "FlyHelpers_TacticalPointLanguageExtender.h"

#include <algorithm>  // std::min()

FlyHelpers::CTacticalPointLanguageExtender g_flyHelpersTacticalLanguageExtender;

// Description:
//	 Helper class for declaring fire command descriptors.
template<class T>
class CFireCommandDescBase : public IFireCommandDesc
{
public:
	CFireCommandDescBase(const char* name) : m_name(name) {};
	virtual const char*          GetName()                { return m_name.c_str(); }
	virtual IFireCommandHandler* Create(IAIActor* pOwner) { return new T(pOwner); }
	virtual void                 Release()                { delete this; }

protected:
	string m_name;
};
#define CREATE_FIRECOMMAND_DESC(name, type) new CFireCommandDescBase<type>((name))

// used to determine if a forbidden area should be rasterised

static const size_t AILogMaxIdLen = 32;
static const char* sCodeCoverageContextFile = "ccContext.txt";

#define BroadcastToListeners(call)                                       \
  SystemListenerSet::iterator listenerIt = m_setSystemListeners.begin(); \
  SystemListenerSet::iterator listenerEnd = m_setSystemListeners.end();  \
  for (; listenerIt != listenerEnd; ++listenerIt)                        \
  {                                                                      \
    (*listenerIt)->call;                                                 \
  }

template<> std::vector<SAIObjectMapIter<CWeakRef>*> SAIObjectMapIter<CWeakRef>::pool = std::vector<SAIObjectMapIter<CWeakRef>*>();
template<> std::vector<SAIObjectMapIterOfType<CWeakRef>*> SAIObjectMapIterOfType<CWeakRef>::pool = std::vector<SAIObjectMapIterOfType<CWeakRef>*>();
template<> std::vector<SAIObjectMapIterInRange<CWeakRef>*> SAIObjectMapIterInRange<CWeakRef>::pool = std::vector<SAIObjectMapIterInRange<CWeakRef>*>();
template<> std::vector<SAIObjectMapIterOfTypeInRange<CWeakRef>*> SAIObjectMapIterOfTypeInRange<CWeakRef>::pool = std::vector<SAIObjectMapIterOfTypeInRange<CWeakRef>*>();
template<> std::vector<SAIObjectMapIterInShape<CWeakRef>*> SAIObjectMapIterInShape<CWeakRef>::pool = std::vector<SAIObjectMapIterInShape<CWeakRef>*>();
template<> std::vector<SAIObjectMapIterOfTypeInShape<CWeakRef>*> SAIObjectMapIterOfTypeInShape<CWeakRef>::pool = std::vector<SAIObjectMapIterOfTypeInShape<CWeakRef>*>();
template<> std::vector<SAIObjectMapIter<CCountedRef>*> SAIObjectMapIter<CCountedRef>::pool = std::vector<SAIObjectMapIter<CCountedRef>*>();
template<> std::vector<SAIObjectMapIterOfType<CCountedRef>*> SAIObjectMapIterOfType<CCountedRef>::pool = std::vector<SAIObjectMapIterOfType<CCountedRef>*>();
template<> std::vector<SAIObjectMapIterInRange<CCountedRef>*> SAIObjectMapIterInRange<CCountedRef>::pool = std::vector<SAIObjectMapIterInRange<CCountedRef>*>();
template<> std::vector<SAIObjectMapIterOfTypeInRange<CCountedRef>*> SAIObjectMapIterOfTypeInRange<CCountedRef>::pool = std::vector<SAIObjectMapIterOfTypeInRange<CCountedRef>*>();
template<> std::vector<SAIObjectMapIterInShape<CCountedRef>*> SAIObjectMapIterInShape<CCountedRef>::pool = std::vector<SAIObjectMapIterInShape<CCountedRef>*>();
template<> std::vector<SAIObjectMapIterOfTypeInShape<CCountedRef>*> SAIObjectMapIterOfTypeInShape<CCountedRef>::pool = std::vector<SAIObjectMapIterOfTypeInShape<CCountedRef>*>();

//===================================================================
// ClearAIObjectIteratorPools
//===================================================================
void ClearAIObjectIteratorPools()
{
	// (MATT) Iterators now have their destructors called before they enter the pool - so we only need to free the memory here {2008/12/04}
	std::for_each(SAIObjectMapIter<CWeakRef>::pool.begin(), SAIObjectMapIter<CWeakRef>::pool.end(), DeleteAIObjectMapIter<CWeakRef> );
	std::for_each(SAIObjectMapIterOfType<CWeakRef>::pool.begin(), SAIObjectMapIterOfType<CWeakRef>::pool.end(), DeleteAIObjectMapIter<CWeakRef> );
	std::for_each(SAIObjectMapIterInRange<CWeakRef>::pool.begin(), SAIObjectMapIterInRange<CWeakRef>::pool.end(), DeleteAIObjectMapIter<CWeakRef> );
	std::for_each(SAIObjectMapIterOfTypeInRange<CWeakRef>::pool.begin(), SAIObjectMapIterOfTypeInRange<CWeakRef>::pool.end(), DeleteAIObjectMapIter<CWeakRef> );
	std::for_each(SAIObjectMapIterInShape<CWeakRef>::pool.begin(), SAIObjectMapIterInShape<CWeakRef>::pool.end(), DeleteAIObjectMapIter<CWeakRef> );
	std::for_each(SAIObjectMapIterOfTypeInShape<CWeakRef>::pool.begin(), SAIObjectMapIterOfTypeInShape<CWeakRef>::pool.end(), DeleteAIObjectMapIter<CWeakRef> );
	std::for_each(SAIObjectMapIter<CCountedRef>::pool.begin(), SAIObjectMapIter<CCountedRef>::pool.end(), DeleteAIObjectMapIter<CCountedRef> );
	std::for_each(SAIObjectMapIterOfType<CCountedRef>::pool.begin(), SAIObjectMapIterOfType<CCountedRef>::pool.end(), DeleteAIObjectMapIter<CCountedRef> );
	std::for_each(SAIObjectMapIterInRange<CCountedRef>::pool.begin(), SAIObjectMapIterInRange<CCountedRef>::pool.end(), DeleteAIObjectMapIter<CCountedRef> );
	std::for_each(SAIObjectMapIterOfTypeInRange<CCountedRef>::pool.begin(), SAIObjectMapIterOfTypeInRange<CCountedRef>::pool.end(), DeleteAIObjectMapIter<CCountedRef> );
	std::for_each(SAIObjectMapIterInShape<CCountedRef>::pool.begin(), SAIObjectMapIterInShape<CCountedRef>::pool.end(), DeleteAIObjectMapIter<CCountedRef> );
	std::for_each(SAIObjectMapIterOfTypeInShape<CCountedRef>::pool.begin(), SAIObjectMapIterOfTypeInShape<CCountedRef>::pool.end(), DeleteAIObjectMapIter<CCountedRef> );
	stl::free_container(SAIObjectMapIter<CWeakRef>::pool);
	stl::free_container(SAIObjectMapIterOfType<CWeakRef>::pool);
	stl::free_container(SAIObjectMapIterInRange<CWeakRef>::pool);
	stl::free_container(SAIObjectMapIterOfTypeInRange<CWeakRef>::pool);
	stl::free_container(SAIObjectMapIterInShape<CWeakRef>::pool);
	stl::free_container(SAIObjectMapIterOfTypeInShape<CWeakRef>::pool);
	stl::free_container(SAIObjectMapIter<CCountedRef>::pool);
	stl::free_container(SAIObjectMapIterOfType<CCountedRef>::pool);
	stl::free_container(SAIObjectMapIterInRange<CCountedRef>::pool);
	stl::free_container(SAIObjectMapIterOfTypeInRange<CCountedRef>::pool);
	stl::free_container(SAIObjectMapIterInShape<CCountedRef>::pool);
	stl::free_container(SAIObjectMapIterOfTypeInShape<CCountedRef>::pool);
}

void RemoveNonActors(CAISystem::AIActorSet& actorSet)
{
	for (CAISystem::AIActorSet::iterator it = actorSet.begin(); it != actorSet.end(); )
	{
		IAIObject* pAIObject = it->GetAIObject();
		CRY_ASSERT_TRACE(pAIObject, ("An AI Actors set contains a null entry for object id %d!", it->GetObjectID()));

		// [AlexMcC|29.06.10] We can't trust that this is an AI Actor, because CWeakRef::GetAIObject()
		// doesn't do any type checking. If this weakref becomes invalid, then the id is used by another
		// object (which can happen if we chainload or load a savegame), this might not be an actor anymore.
		const bool bIsActor = pAIObject ? (pAIObject->CastToCAIActor() != NULL) : false;
		CRY_ASSERT_MESSAGE(bIsActor, "A non-actor is in an AI actor set!");

		if (pAIObject && bIsActor)
		{
			++it;
		}
		else
		{
			it = actorSet.erase(it);
		}
	}
}

//====================================================================
// CAISystem
//====================================================================
CAISystem::CAISystem(ISystem* pSystem)
	: m_actorProxyFactory(nullptr)
	, m_groupProxyFactory(nullptr)
	, m_pGraph(nullptr)
	, m_pNavigation(nullptr)
	, m_pAIActionManager(nullptr)
	, m_pSmartObjectManager(nullptr)
	, m_bUpdateSmartObjects(false)
	, m_IsEnabled(true)
	, m_enabledActorsUpdateError(0.0f)
	, m_enabledActorsUpdateHead(0)
	, m_totalActorsUpdateCount(0)
	, m_disabledActorsUpdateError(0.0f)
	, m_disabledActorsHead(0)
	, m_iteratingActorSet(false)
	, m_pScriptAI(nullptr)
	, m_walkabilityGeometryBox(nullptr)
	, m_DEBUG_screenFlash(0.0f)
	, m_bCodeCoverageFailed(false)
	, m_nTickCount(0)
	, m_bInitialized(false)
	, m_frameDeltaTime(0.0f)
	, m_frameStartTimeSeconds(0.0f)
	, m_lastVisBroadPhaseTime(-10.0f)
	, m_lastAmbientFireUpdateTime(-10.0f)
	, m_lastExpensiveAccessoryUpdateTime(-10.0f)
	, m_lastGroupUpdateTime(-10.0f)
	, m_nFrameTicks(0)
	, m_agentDebugTarget(0)
{
	ZeroArray(m_AlertnessCounters);
	ZeroArray(m_pAreaList);

	//TODO see if we can init the actual environment yet?
	gAIEnv.CVars.Init(); // CVars need to be init before any logging takes place
	gAIEnv.SignalCRCs.Init();

	REGISTER_COMMAND("ai_reload", (ConsoleCommandFunc)ReloadConsoleCommand, VF_CHEAT, "Reload AI system scripts and data");
	REGISTER_COMMAND("ai_CheckGoalpipes", (ConsoleCommandFunc)CheckGoalpipes, VF_CHEAT, "Checks goalpipes and dumps report to console.");
	REGISTER_COMMAND("ai_dumpCheckpoints", (ConsoleCommandFunc)DumpCodeCoverageCheckpoints, VF_CHEAT, "Dump CodeCoverage checkpoints to file");
	REGISTER_COMMAND("ai_Recorder_Start", (ConsoleCommandFunc)StartAIRecorder, VF_CHEAT, "Reset and start the AI Recorder on demand");
	REGISTER_COMMAND("ai_Recorder_Stop", (ConsoleCommandFunc)StopAIRecorder, VF_CHEAT, "Stop the AI Recorder. If logging in memory, saves it to disk.");

	CFlightNavRegion2::InitCVars();
	CTacticalPointSystem::RegisterCVars();

#ifdef CRYAISYSTEM_DEBUG
	m_Recorder.Init();
	gAIEnv.SetAIRecorder(&m_Recorder);
#endif //CRYAISYSTEM_DEBUG

	// Don't Init() here! It will be called later after creation of the Entity System
	//	Init();
}
//
//-----------------------------------------------------------------------------------------------------------
CAISystem::~CAISystem()
{
	if (GetISystem() && GetISystem()->GetISystemEventDispatcher())
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

	//Reset(IAISystem::RESET_EXIT_GAME);

	m_PipeManager.ClearAllGoalPipes();

	// (MATT) Note that we need to later trigger the object tracker to release all the objects {2009/03/25}
	if (gAIEnv.pAIObjectManager)
		gAIEnv.pAIObjectManager->Reset();

	m_mapFaction.clear();
	m_mapGroups.clear();
	m_mapBeacons.clear();

	for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
		delete it->second;
	m_mapAIGroups.clear();

	if (m_pNavigation)
		m_pNavigation->ShutDown();

	if (gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager->Shutdown();

	delete m_pSmartObjectManager;
	delete m_pAIActionManager;

	// Release fire command factory.
	for (std::vector<IFireCommandDesc*>::iterator it = m_firecommandDescriptors.begin(); it != m_firecommandDescriptors.end(); ++it)
		(*it)->Release();
	m_firecommandDescriptors.clear();

	SAFE_RELEASE(m_pScriptAI);

	// (MATT) Flush all the objects that have been deregistered.
	// Really, this should delete all AI objects regardless {2009/03/27}
	if (gAIEnv.pObjectContainer)
	{
		gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(true);
		gAIEnv.pObjectContainer->Reset();
	}

	if (gAIEnv.pPathfinderNavigationSystemUser)
	{
		gAIEnv.pNavigationSystem->UnRegisterUser(gAIEnv.pPathfinderNavigationSystemUser);
		delete gAIEnv.pPathfinderNavigationSystemUser;
		gAIEnv.pMNMPathfinder = 0;
	}

	delete m_pNavigation;
	gAIEnv.pNavigation = m_pNavigation = 0;

	delete m_pGraph;
	gAIEnv.pGraph = m_pGraph = 0;

	SAFE_DELETE(gAIEnv.pPerceptionManager);
	SAFE_DELETE(gAIEnv.pCommunicationManager);
	SAFE_DELETE(gAIEnv.pCoverSystem);
	SAFE_DELETE(gAIEnv.pNavigationSystem);
	SAFE_DELETE(gAIEnv.pSelectionTreeManager);
	SAFE_DELETE(gAIEnv.pBehaviorTreeManager);
	SAFE_DELETE(gAIEnv.pGraftManager);
	SAFE_DELETE(gAIEnv.pVisionMap);
	SAFE_DELETE(gAIEnv.pFactionMap);
	SAFE_DELETE(gAIEnv.pGroupManager);
	SAFE_DELETE(gAIEnv.pRayCaster);
	SAFE_DELETE(gAIEnv.pIntersectionTester);
	SAFE_DELETE(gAIEnv.pMovementSystem);
	SAFE_DELETE(gAIEnv.pSequenceManager);
	SAFE_DELETE(gAIEnv.pClusterDetector);

#ifdef CRYAISYSTEM_DEBUG
	SAFE_DELETE(gAIEnv.pBubblesSystem);
#endif

	gAIEnv.ShutDown();
}

//-----------------------------------------------------------------------------------------------------------

bool CAISystem::Init()
{
	LOADING_TIME_PROFILE_SECTION;

	AILogProgress("[AISYSTEM] Initialization started.");

	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this);

	SetupAIEnvironment();

	// ensure the available communications are ready before the AI scripts get loaded (they in turn might load GoalPipes and other stuff that depend on communication data)
	gAIEnv.pCommunicationManager->LoadConfigurationAndScanRootFolder("Scripts/AI/Communication");

	if (!m_pScriptAI)
		m_pScriptAI = new CScriptBind_AI();

	Reset(IAISystem::RESET_INTERNAL);

	m_nTickCount = 0;
	gAIEnv.pWorld = gEnv->pPhysicalWorld;

	m_frameStartTime = gEnv->pTimer->GetFrameStartTime();
	m_fLastPuppetUpdateTime = m_frameStartTime;
	m_frameDeltaTime = 0.0f;
	m_frameStartTimeSeconds = m_frameStartTime.GetSeconds();

	// Register fire command factories.
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("instant", CFireCommandInstant));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("instant_single", CFireCommandInstantSingle));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("projectile_slow", CFireCommandProjectileSlow));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("projectile_fast", CFireCommandProjectileFast));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("grenade", CFireCommandGrenade));
#if 0
	// deprecated and won't compile at all...
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("strafing", CFireCommandStrafing));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hurricane", CFireCommandHurricane));
	// TODO: move this to game.dll
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("fast_light_moar", CFireCommandFastLightMOAR));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hunter_moar", CFireCommandHunterMOAR));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hunter_sweep_moar", CFireCommandHunterSweepMOAR));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hunter_singularity_cannon", CFireCommandHunterSingularityCannon));
#endif
	m_bInitialized = true;

	if (gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager->Init();

	gAIEnv.pSelectionTreeManager->ScanFolder("Scripts/AI/SelectionTrees");

	BehaviorTree::RegisterBehaviorTreeNodes();

	AILogProgress("[AISYSTEM] Initialization finished.");

	if (gEnv->IsEditor())
	{
		if (!gAIEnv.pGraph)
		{
			gAIEnv.pGraph = m_pGraph = new CGraph;
		}
		if (!gAIEnv.pNavigation)
		{
			gAIEnv.pNavigation = m_pNavigation = new CNavigation(gEnv->pSystem);
			m_pNavigation->Init();
		}
		if (!gAIEnv.pCoverSystem)
		{
			gAIEnv.pCoverSystem = new CCoverSystem("Scripts/AI/Cover.xml");
		}
		if (!gAIEnv.pNavigationSystem)
		{
			gAIEnv.pNavigationSystem = new NavigationSystem("Scripts/AI/Navigation.xml");
		}
		if (!gAIEnv.pMNMPathfinder)
		{
			gAIEnv.pPathfinderNavigationSystemUser = new MNM::PathfinderNavigationSystemUser;
			gAIEnv.pMNMPathfinder = gAIEnv.pPathfinderNavigationSystemUser->GetPathfinderImplementation();
			assert(gAIEnv.pNavigationSystem);
			gAIEnv.pNavigationSystem->RegisterUser(gAIEnv.pPathfinderNavigationSystemUser, "PathfinderExtension");
		}
		if (!gAIEnv.pVisionMap)
		{
			gAIEnv.pVisionMap = new CVisionMap();
		}
		if (!gAIEnv.pGroupManager)
		{
			gAIEnv.pGroupManager = new CGroupManager();
		}
		if (!gAIEnv.pAIActionManager)
		{
			gAIEnv.pAIActionManager = m_pAIActionManager = new CAIActionManager();
		}
		if (!gAIEnv.pSmartObjectManager)
		{
			gAIEnv.pSmartObjectManager = m_pSmartObjectManager = new CSmartObjectManager();
		}
		if (!gAIEnv.pRayCaster)
		{
			gAIEnv.pRayCaster = new SAIEnvironment::GlobalRayCaster;
			gAIEnv.pRayCaster->SetQuota(gAIEnv.CVars.RayCasterQuota);
		}
		if (!gAIEnv.pIntersectionTester)
		{
			gAIEnv.pIntersectionTester = new SAIEnvironment::GlobalIntersectionTester;
			gAIEnv.pIntersectionTester->SetQuota(gAIEnv.CVars.IntersectionTesterQuota);
		}
		if (!gAIEnv.pMovementSystem)
		{
			gAIEnv.pMovementSystem = MovementSystemCreator().CreateMovementSystem();
		}
		if (!gAIEnv.pSequenceManager)
		{
			gAIEnv.pSequenceManager = new AIActionSequence::SequenceManager();
		}
		if (!gAIEnv.pClusterDetector)
		{
			gAIEnv.pClusterDetector = new ClusterDetector();
		}
		if (!gAIEnv.pActorLookUp)
		{
			gAIEnv.pActorLookUp = new ActorLookUp();
		}
		if (!gAIEnv.pFactionMap)
		{
			gAIEnv.pFactionMap = new CFactionMap();
		}
		if (!gAIEnv.pWalkabilityCacheManager)
		{
			gAIEnv.pWalkabilityCacheManager = new WalkabilityCacheManager();
		}
		if (!gAIEnv.pTacticalPointSystem)
		{
			gAIEnv.pTacticalPointSystem = new CTacticalPointSystem();
			g_flyHelpersTacticalLanguageExtender.Initialize();
		}
	}

	m_AIObjectManager.Init();
	m_globalPerceptionScale.Reload();

	return true;
}

bool CAISystem::CompleteInit()
{
	AIFlowBaseNode::RegisterFactory();
	return true;
}

void CAISystem::SetAIHacksConfiguration()
{
	/////////////////////////////////////////////////////////////
	//TODO This is hack support and should go away at some point!
	// Set the compatibility mode for feature/setting emulation
	const char* sValue = gAIEnv.CVars.CompatibilityMode;

	EConfigCompatabilityMode eCompatMode = ECCM_CRYSIS2;

	if (strcmpi("crysis", sValue) == 0)
		eCompatMode = ECCM_CRYSIS;
	if (strcmpi("game04", sValue) == 0)
		eCompatMode = ECCM_GAME04;
	if (strcmpi("warface", sValue) == 0)
		eCompatMode = ECCM_WARFACE;
	if (strcmpi("crysis2", sValue) == 0)
		eCompatMode = ECCM_CRYSIS2;
	gAIEnv.configuration.eCompatibilityMode = eCompatMode;
	/////////////////////////////////////////////////////////////
}

void CAISystem::SetupAIEnvironment()
{
	SetAIHacksConfiguration();

	//TODO make these members of CAISystem that are pointed to, NOT allocated!

	if (!gAIEnv.pActorLookUp)
		gAIEnv.pActorLookUp = new ActorLookUp();

	if (!gAIEnv.pWalkabilityCacheManager)
		gAIEnv.pWalkabilityCacheManager = new WalkabilityCacheManager();

	gAIEnv.pAIObjectManager = &m_AIObjectManager;

	gAIEnv.pPipeManager = &m_PipeManager;

	// GoalOpFactory
	if (!gAIEnv.pGoalOpFactory)
	{
		CGoalOpFactoryOrdering* pFactoryOrdering = new CGoalOpFactoryOrdering();
		gAIEnv.pGoalOpFactory = pFactoryOrdering;

		//TODO move these out of AISystem, to be added thru interface function from game side
		pFactoryOrdering->PrepareForFactories(3);
		pFactoryOrdering->AddFactory(new CGoalOpFactoryCrysis2);

#if 0
		// deprecated and won't compile at all...
		pFactoryOrdering->AddFactory(new CGoalOpFactoryG04);
#endif

		pFactoryOrdering->AddFactory(new CGoalOpFactoryG02);
	}

	// ObjectContainer
	if (!gAIEnv.pObjectContainer)
		gAIEnv.pObjectContainer = new CObjectContainer;

#if !defined(_RELEASE)
	// CodeCoverage
	if (!gAIEnv.pCodeCoverageTracker)
		gAIEnv.pCodeCoverageTracker = new CCodeCoverageTracker;
	if (!gAIEnv.pCodeCoverageManager)
		gAIEnv.pCodeCoverageManager = new CCodeCoverageManager;
	if (!gAIEnv.pCodeCoverageGUI)
		gAIEnv.pCodeCoverageGUI = new CCodeCoverageGUI;
#endif

	// StatsManager
	if (!gAIEnv.pStatsManager)
		gAIEnv.pStatsManager = new CStatsManager;

	if (!gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager = new CTargetTrackManager();

	gAIEnv.pAIActionManager = m_pAIActionManager = new CAIActionManager();
	gAIEnv.pSmartObjectManager = m_pSmartObjectManager = new CSmartObjectManager();

	gAIEnv.pPerceptionManager = new CPerceptionManager();
	gAIEnv.pCommunicationManager = new CCommunicationManager("Scripts/AI/Communication/CommunicationSystemConfiguration.xml");
	gAIEnv.pSelectionTreeManager = new CSelectionTreeManager();
	gAIEnv.pBehaviorTreeManager = new BehaviorTree::BehaviorTreeManager();
	gAIEnv.pGraftManager = new BehaviorTree::GraftManager();

	if (!gAIEnv.pFactionMap)
	{
		gAIEnv.pFactionMap = new CFactionMap();
		m_globalPerceptionScale.Reload();
	}

	gAIEnv.pCollisionAvoidanceSystem = new CollisionAvoidanceSystem();

	gAIEnv.pRayCaster = new SAIEnvironment::GlobalRayCaster;
	gAIEnv.pRayCaster->SetQuota(gAIEnv.CVars.RayCasterQuota);

	gAIEnv.pIntersectionTester = new SAIEnvironment::GlobalIntersectionTester;
	gAIEnv.pIntersectionTester->SetQuota(gAIEnv.CVars.IntersectionTesterQuota);

	gAIEnv.pMovementSystem = MovementSystemCreator().CreateMovementSystem();

	gAIEnv.pSequenceManager = new AIActionSequence::SequenceManager();

	gAIEnv.pClusterDetector = new ClusterDetector();

#ifdef CRYAISYSTEM_DEBUG
	if (!gAIEnv.pBubblesSystem)
	{
		gAIEnv.pBubblesSystem = new CAIBubblesSystem();
		gAIEnv.pBubblesSystem->Init();
	}
#endif // CRYAISYSTEM_DEBUG
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::InitSmartObjects()
{
	AILogProgress("[AISYSTEM] Initializing AI Actions.");
	m_pAIActionManager->LoadLibrary(AI_ACTIONS_PATH);

	AILogProgress("[AISYSTEM] Initializing Smart Objects.");
	m_pSmartObjectManager->LoadSmartObjectsLibrary();

	return true;
}

//====================================================================
// Reload
//====================================================================
void CAISystem::Reload()
{
	gEnv->pLog->Log("-------------------------------------------");
	gEnv->pLog->Log("Reloading AI...");

	// Clear out any state that would otherwise persist over a reload (e.g. goalpipes)
	ClearForReload();

	gAIEnv.pCommunicationManager->Reload();
	gAIEnv.pSelectionTreeManager->Reload();

	gAIEnv.pFactionMap->Reload();
	m_globalPerceptionScale.Reload();
	g_flyHelpersTacticalLanguageExtender.Initialize();

	// Reload the root of the AI system scripts, forcing reload of this and all dependencies
	IScriptSystem* pSS = gEnv->pScriptSystem;
	CRY_ASSERT(pSS);
	if (pSS->ExecuteFile("Scripts/AI/aiconfig.lua", true, true))
	{
		// Reset blackboards etc.
		pSS->BeginCall("AIReset");
		pSS->EndCall();

		// Reload goal pipes, queries etc.
		pSS->BeginCall("AIReload");
		pSS->EndCall();
	}

	if (gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager->ReloadConfig();

	gAIEnv.pCoverSystem->ReloadConfig();

	gAIEnv.pBehaviorTreeManager->Reset();
	gAIEnv.pGraftManager->Reset();
}

//====================================================================
// SetLevelPath
//====================================================================
void CAISystem::SetLevelPath(const char* sPath)
{
	m_sWorkingFolder = sPath;
}

//====================================================================
// RegisterListener
//====================================================================
bool CAISystem::RegisterListener(IAISystemListener* pListener)
{
	bool result;
	result = m_setSystemListeners.insert(pListener).second;
	return result;
}

//====================================================================
// UnregisterListener
//====================================================================
bool CAISystem::UnregisterListener(IAISystemListener* pListener)
{
	int nErased;
	nErased = m_setSystemListeners.erase(pListener);
	if (m_setSystemListeners.empty())
		stl::free_container(m_setSystemListeners);
	return (nErased == 1);
}

//====================================================================
// OnAgentDeath
//====================================================================
void CAISystem::OnAgentDeath(EntityId deadEntityID, EntityId killerID)
{
	BroadcastToListeners(OnAgentDeath(deadEntityID, killerID));
}

//====================================================================
// InvalidatePathsThroughArea
//====================================================================
void CAISystem::InvalidatePathsThroughArea(const ListPositions& areaShape)
{
	for (AIObjectOwners::iterator it = gAIEnv.pAIObjectManager->m_Objects.begin(); it != gAIEnv.pAIObjectManager->m_Objects.end(); ++it)
	{
		CAIObject* pObject = it->second.GetAIObject();
		CPipeUser* pPiper = pObject->CastToCPipeUser();
		if (pPiper)
		{
			const CNavPath& path = pPiper->m_Path;
			const TPathPoints& listPath = path.GetPath();
			Vec3 pos = pPiper->GetPhysicsPos();
			for (TPathPoints::const_iterator pathIt = listPath.begin(); pathIt != listPath.end(); ++pathIt)
			{
				Lineseg pathSeg(pos, pathIt->vPos);
				pos = pathIt->vPos;
				if (Overlap::Lineseg_Polygon2D(pathSeg, areaShape))
				{
					pPiper->PathIsInvalid();
					break;
				}
			}
		}
	} // loop over objects
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetAlertness() const
{
	int maxAlertness = 0;
	for (int i = NUM_ALERTNESS_COUNTERS - 1; i; i--)
		if (m_AlertnessCounters[i])
		{
			maxAlertness = i;
			break;
		}
	return maxAlertness;
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetAlertness(const IAIAlertnessPredicate& alertnessPredicate)
{
	int maxAlertness = 0;

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	lookUp.Prepare(ActorLookUp::Proxy);

	size_t activeActorCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
	{
		CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);
		IAIActorProxy* pProxy = lookUp.GetProxy(actorIndex);

		if ((pProxy != NULL) && alertnessPredicate.ConsiderAIObject(pAIActor))
		{
			maxAlertness = max(maxAlertness, pProxy->GetAlertnessState());
		}
	}

	return maxAlertness;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ClearForReload(void)
{
	m_PipeManager.ClearAllGoalPipes();
	g_flyHelpersTacticalLanguageExtender.Deinitialize();
	gAIEnv.pTacticalPointSystem->DestroyAllQueries();
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ReloadConsoleCommand(IConsoleCmdArgs* pArgs)
{
	// Needs to be static function to be registered as console command
	// The arguments are not used
	GetAISystem()->Reload();
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::StartAIRecorder(IConsoleCmdArgs* pArgs)
{
#ifdef CRYAISYSTEM_DEBUG

	if (pArgs->GetArgCount() < 2)
	{
		gEnv->pAISystem->Warning("<StartAIRecorder> ", "Expecting record mode as argument (1 == Memory, 2 == Disk)");
		return;
	}

	char const* szMode = pArgs->GetArg(1);
	char const* szFile = pArgs->GetArgCount() > 2 ? pArgs->GetArg(2) : NULL;

	EAIRecorderMode mode = eAIRM_Off;
	if (strcmp(szMode, "1") == 0)
		mode = eAIRM_Memory;
	else if (strcmp(szMode, "2") == 0)
		mode = eAIRM_Disk;

	string sRealFile;
	if (szFile)
		sRealFile = PathUtil::Make("", szFile, "rcd");

	if (mode > eAIRM_Off)
	{
		CAISystem* pAISystem = GetAISystem();
		pAISystem->m_Recorder.Reset();
		pAISystem->m_Recorder.Start(mode, sRealFile.c_str());
	}

#endif //CRYAISYSTEM_DEBUG
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::StopAIRecorder(IConsoleCmdArgs* pArgs)
{
#ifdef CRYAISYSTEM_DEBUG

	char const* szFile = pArgs->GetArgCount() > 1 ? pArgs->GetArg(1) : NULL;

	string sRealFile;
	if (szFile)
		sRealFile = PathUtil::Make("", szFile, "rcd");

	GetAISystem()->m_Recorder.Stop(sRealFile.c_str());

#endif //CRYAISYSTEM_DEBUG
}

//====================================================================
// CheckUnusedGoalpipes
//====================================================================
void CAISystem::CheckGoalpipes(IConsoleCmdArgs*)
{
	GetAISystem()->m_PipeManager.CheckGoalpipes();
}

void CAISystem::DumpCodeCoverageCheckpoints(IConsoleCmdArgs* pArgs)
{
#if !defined(_RELEASE)
	if (gAIEnv.pCodeCoverageManager)
	{
		// Any second argument triggers dump to file rather than log
		bool bLogToFile = (pArgs->GetArgCount() > 1);
		gAIEnv.pCodeCoverageManager->DumpCheckpoints(bLogToFile);
	}
#endif
}

//
//-----------------------------------------------------------------------------------------------------------
CAIObject* CAISystem::GetPlayer() const
{
	AIObjectOwners::const_iterator ai;
	if ((ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_PLAYER)) != gAIEnv.pAIObjectManager->m_Objects.end())
		return ai->second.GetAIObject();
	return 0;
}

// Sends a signal using the desired filter to the desired agents
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SendSignal(unsigned char cFilter, int nSignalId, const char* szText, IAIObject* pSenderObject, IAISignalExtraData* pData, uint32 crcCode /*= 0*/)
{
	// (MATT) This is quite a switch statement. Needs replacing. {2009/02/11}
	CCCPOINT(CAISystem_SendSignal);
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	// This deletes the passed pData parameter if it wasn't set to NULL
	struct DeleteBeforeReturning
	{
		IAISignalExtraData** _p;
		DeleteBeforeReturning(IAISignalExtraData** p) : _p(p) {}
		~DeleteBeforeReturning()
		{
			if (*_p)
				delete (AISignalExtraData*)*_p;
		}
	} autoDelete(&pData);

	// Calling this with no senderObject is an error
	assert(pSenderObject);

	CAIActor* pSender = CastToCAIActorSafe(pSenderObject);
	//filippo: can be that sender is null, for example when you send this signal in multiplayer.
	if (!pSender)
		return;

	float fRange = pSender->GetParameters().m_fCommRange;
	fRange *= pSender->GetParameters().m_fCommRange;
	Vec3 pos = pSender->GetPos();
	IEntity* pSenderEntity(pSender->GetEntity());

	CWeakRef<CAIObject> refSender = GetWeakRefSafe(pSender);

	switch (cFilter)
	{

	case SIGNALFILTER_SENDER:
		{
			pSender->SetSignal(nSignalId, szText, pSenderEntity, pData, crcCode);
			pData = NULL;
			break;
		}
	case SIGNALFILTER_HALFOFGROUP:
		{
			CCCPOINT(CAISystem_SendSignal_HALFOFGROUP);

			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;

			if ((ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				int groupmembers = m_mapGroups.count(groupid);
				groupmembers /= 2;

				for (; ai != m_mapGroups.end(); )
				{
					if ((ai->first != groupid) || (!groupmembers--))
						break;

					// Object may have been removed
					CAIActor* pReciever = CastToCAIActorSafe(ai->second.GetAIObject());
					if ((pReciever != NULL) && pReciever != pSenderObject)
					{
						pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
					}
					else
						++groupmembers;   // dont take into account sender
					++ai;
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}
		}
		break;
	case SIGNALFILTER_NEARESTGROUP:
		{
			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;
			CAIActor* pNearest = 0;
			float mindist = 2000;
			if ((ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				for (; ai != m_mapGroups.end(); ++ai)
				{
					if (ai->first != groupid)
						break;

					CAIObject* pObject = ai->second.GetAIObject();

					if ((pObject != NULL) && (pObject != pSenderObject))
					{
						float dist = (pObject->GetPos() - pSender->GetPos()).GetLength();
						if (dist < mindist)
						{
							pNearest = pObject->CastToCAIActor();
							if (pNearest)
								mindist = (pObject->GetPos() - pSender->GetPos()).GetLength();
						}
					}
				}

				if (pNearest)
				{
					pNearest->SetSignal(nSignalId, szText, pSenderEntity, pData, crcCode);
					pData = NULL;
				}
			}
		}
		break;

	case SIGNALFILTER_NEARESTINCOMM:
		{
			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;
			CAIActor* pNearest = 0;
			float mindistSq = sqr(pSender->GetParameters().m_fCommRange);
			if ((ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				for (; ai != m_mapGroups.end(); ++ai)
				{
					if (ai->first == groupid)
					{
						CAIObject* pObject = ai->second.GetAIObject();

						if ((pObject != NULL) && (pObject != pSenderObject))
						{
							float distSq = (pObject->GetPos() - pSender->GetPos()).GetLengthSquared();
							if (distSq < mindistSq)
							{
								if (pNearest = pObject->CastToCAIActor())
									mindistSq = distSq;
							}
						}
					}
					else
					{
						break;
					}
				}

				if (pNearest)
				{
					pNearest->SetSignal(nSignalId, szText, pSenderEntity, pData, crcCode);
					pData = NULL;
				}
			}
		}
		break;
	case SIGNALFILTER_NEARESTINCOMM_LOOKING:
		{
			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;
			CAIActor* pSenderActor = pSenderObject->CastToCAIActor();
			CAIActor* pNearest = 0;
			float mindist = pSender->GetParameters().m_fCommRange;
			float closestDist2(std::numeric_limits<float>::max());
			if (pSenderActor && (ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				for (; ai != m_mapGroups.end(); )
				{
					if (ai->first != groupid)
						break;

					CAIObject* pObject = ai->second.GetAIObject();

					if ((pObject != NULL) && (pObject != pSenderObject))
					{
						CPuppet* pCandidatePuppet = pObject->CastToCPuppet();
						float dist = (pObject->GetPos() - pSender->GetPos()).GetLength();
						if (pCandidatePuppet && dist < mindist)
							if (CheckVisibilityToBody(pCandidatePuppet, pSenderActor, closestDist2))
								pNearest = pCandidatePuppet;
					}
					++ai;
				}
				if (pNearest)
				{
					pNearest->SetSignal(nSignalId, szText, pSenderEntity, pData, crcCode);
					pData = NULL;
				}
			}
		}
		break;
	case SIGNALFILTER_NEARESTINCOMM_FACTION:
		{
			uint8 factionID = pSender->GetFactionID();
			AIObjects::iterator ai;
			CAIActor* pNearest = 0;
			float mindist = pSender->GetParameters().m_fCommRange;
			if ((ai = m_mapFaction.find(factionID)) != m_mapFaction.end())
			{
				for (; ai != m_mapFaction.end(); )
				{
					CAIObject* pObject = ai->second.GetAIObject();
					if (ai->first != factionID)
						break;

					if (pObject && (pObject != pSenderObject && pObject->IsEnabled()))
					{
						float dist = (pObject->GetPos() - pSender->GetPos()).GetLength();
						if (dist < mindist)
						{
							pNearest = pObject->CastToCAIActor();
							if (pNearest)
								mindist = (pObject->GetPos() - pSender->GetPos()).GetLength();
						}
					}
					++ai;
				}

				if (pNearest)
				{
					pNearest->SetSignal(nSignalId, szText, pSenderEntity, pData, crcCode);
					pData = NULL;
				}
			}
		}
		break;
	case SIGNALFILTER_SUPERGROUP:
		{
			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;
			if ((ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				for (; ai != m_mapGroups.end(); ++ai)
				{
					CAIActor* pReciever = CastToCAIActorSafe(ai->second.GetAIObject());
					if (ai->first != groupid)
						break;
					if (pReciever)
						pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}
			// send message also to player
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
			if (pPlayer)
			{
#pragma warning(push)
#pragma warning(disable:6011)
				if (pSender->GetParameters().m_nGroup == pPlayer->GetParameters().m_nGroup)
					pPlayer->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
#pragma warning(pop)
			}
		}
		break;
	case SIGNALFILTER_SUPERFACTION:
		{
			uint8 factionID = pSender->GetFactionID();
			AIObjects::iterator ai;

			if ((ai = m_mapFaction.find(factionID)) != m_mapFaction.end())
			{
				for (; ai != m_mapFaction.end(); )
				{
					CAIActor* pReciever = CastToCAIActorSafe(ai->second.GetAIObject());
					if (ai->first != factionID)
						break;
					if (pReciever)
						pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
					++ai;
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}
			// send message also to player
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
			if (pPlayer)
			{
				pPlayer->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
			}
		}
		break;

	case SIGNALFILTER_GROUPONLY:
	case SIGNALFILTER_GROUPONLY_EXCEPT:
		{
			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;
			if ((ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				for (; ai != m_mapGroups.end(); ++ai)
				{
					CAIActor* pReciever = CastToCAIActorSafe(ai->second.GetAIObject());
					if (ai->first != groupid)
						break;
					if ((pReciever != NULL) && !(cFilter == SIGNALFILTER_GROUPONLY_EXCEPT && pReciever == pSender))
					{
						pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
					}
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}

			// send message also to player
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
			if (pPlayer)
			{
				if (pSender->GetParameters().m_nGroup == pPlayer->GetParameters().m_nGroup && Distance::Point_PointSq(pPlayer->GetPos(), pos) < fRange && !(cFilter == SIGNALFILTER_GROUPONLY_EXCEPT && pPlayer == pSender))
					pPlayer->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
			}
		}
		break;
	case SIGNALFILTER_FACTIONONLY:
		{
			uint8 factionID = pSender->GetFactionID();
			AIObjects::iterator ai;

			if ((ai = m_mapFaction.find(factionID)) != m_mapFaction.end())
			{
				for (; ai != m_mapFaction.end(); ++ai)
				{
					CAIActor* pReciever = CastToCAIActorSafe(ai->second.GetAIObject());
					if (ai->first != factionID)
						break;
					if (pReciever)
					{
						Vec3 mypos = pReciever->GetPos();
						mypos -= pos;

						if (mypos.GetLengthSquared() < fRange)
							pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
					}
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}
			// send message also to player
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
			if (pPlayer)
			{
				if (pSender->GetFactionID() == pPlayer->GetFactionID() && Distance::Point_PointSq(pPlayer->GetPos(), pos) < fRange)
					pPlayer->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
			}
		}
		break;
	case SIGNALFILTER_ANYONEINCOMM:
	case SIGNALFILTER_ANYONEINCOMM_EXCEPT:
		{
			// send to puppets and aware objects in the communications range of the sender

			// Added the evaluation of AIOBJECT_VEHICLE so that vehicles can get signals.
			// 20/12/05 Tetsuji
			AIObjectOwners::iterator ai;

			std::vector<int> objectKinds;
			objectKinds.push_back(AIOBJECT_ACTOR);
			objectKinds.push_back(AIOBJECT_VEHICLE);

			std::vector<int>::iterator ki, ki2, kiend = objectKinds.end();

			for (ki = objectKinds.begin(); ki != kiend; ++ki)
			{
				int objectKind = *ki;
				// first look for all the puppets
				if ((ai = gAIEnv.pAIObjectManager->m_Objects.find(objectKind)) != gAIEnv.pAIObjectManager->m_Objects.end())
				{
					for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); )
					{
						for (ki2 = objectKinds.begin(); ki2 != kiend; ++ki2)
							if (ai->first == *ki)
							{
								CAIActor* pReciever = ai->second->CastToCAIActor();
								if (pReciever && Distance::Point_PointSq(pReciever->GetPos(), pos) < fRange && !(cFilter == SIGNALFILTER_ANYONEINCOMM_EXCEPT && pReciever == pSender))
									pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
							}
						++ai;
					}
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}

			// now look for aware objects
			if ((ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_AWARE)) != gAIEnv.pAIObjectManager->m_Objects.end())
			{
				for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); )
				{
					if (ai->first != AIOBJECT_AWARE)
						break;
					CAIActor* pReciever = ai->second->CastToCAIActor();
					if (pReciever)
					{
						Vec3 mypos = pReciever->GetPos();
						mypos -= pos;
						if (mypos.GetLengthSquared() < fRange)
							pReciever->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
					}
					++ai;
				}
			}
			// send message also to player

			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
			if (pPlayer)
			{
				if (Distance::Point_PointSq(pPlayer->GetPos(), pos) < fRange && !(cFilter == SIGNALFILTER_ANYONEINCOMM_EXCEPT && pPlayer == pSender))
				{
					PREFAST_SUPPRESS_WARNING(6011)
					pPlayer->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
				}
			}
			// don't delete pData!!! - it will be deleted at the end of this function
		}
		break;
	case SIGNALFILTER_LEADER:
	case SIGNALFILTER_LEADERENTITY:
		{
			int groupid = pSender->GetGroupId();
			AIObjects::iterator ai;
			if ((ai = m_mapGroups.find(groupid)) != m_mapGroups.end())
			{
				for (; ai != m_mapGroups.end(); )
				{
					CLeader* pLeader = CastToCLeaderSafe(ai->second.GetAIObject());
					if (ai->first != groupid)
						break;
					if (pLeader)
					{
						CAIObject* pRecipientObj = (cFilter == SIGNALFILTER_LEADER ? pLeader : pLeader->GetAssociation().GetAIObject());
						if (pRecipientObj)
						{
							CAIActor* pRecipient = pRecipientObj->CastToCAIActor();
							if (pRecipient)
								pRecipient->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
						}
						break;
					}
					++ai;
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}
		}
		break;

	case SIGNALFILTER_FORMATION:
	case SIGNALFILTER_FORMATION_EXCEPT:
		{
			for (FormationMap::iterator fi(m_mapActiveFormations.begin()); fi != m_mapActiveFormations.end(); ++fi)
			{
				CFormation* pFormation = fi->second;
				if (pFormation && pFormation->GetFormationPoint(refSender))
				{
					CCCPOINT(CAISystem_SendSignal_Formation);

					int s = pFormation->GetSize();
					for (int i = 0; i < s; i++)
					{
						CAIObject* pMember = pFormation->GetPointOwner(i);
						if (pMember && (cFilter == SIGNALFILTER_FORMATION || pMember != pSender))
						{
							CAIActor* pRecipient = pMember->CastToCAIActor();
							if (pRecipient)
								pRecipient->SetSignal(nSignalId, szText, pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL, crcCode);
						}
					}
					break;
				}
			}
			break;
		}
	}
}

// adds an object to a group
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddToGroup(CAIActor* pObject, int nGroupId)
{
	if (!pObject->CastToCPipeUser() && !pObject->CastToCLeader() && !pObject->CastToCAIPlayer())
		return;

	CWeakRef<CAIObject> refObj = GetWeakRef(pObject);

	if (nGroupId >= 0)
		pObject->SetGroupId(nGroupId);
	else
		nGroupId = pObject->GetGroupId();

	AIObjects::iterator gi;
	bool found = false;
	if ((gi = m_mapGroups.find(nGroupId)) != m_mapGroups.end())
	{
		// check whether it was added before
		while ((gi != m_mapGroups.end()) && (gi->first == nGroupId))
		{
			if (gi->second == refObj)
			{
				found = true;
				break;
			}
			++gi;
		}
	}

	// in the map - do nothing
	if (found)
	{
		// Update the group count status and create the group object if necessary.
		UpdateGroupStatus(nGroupId);
		return;
	}

	m_mapGroups.insert(AIObjects::iterator::value_type(nGroupId, refObj));

	// Update the group count status and create the group object if necessary.
	UpdateGroupStatus(nGroupId);

	//group leaders related stuff
	AIGroupMap::iterator it = m_mapAIGroups.find(nGroupId);
	if (it != m_mapAIGroups.end())
	{
		if (CLeader* pLeader = pObject->CastToCLeader())
		{
			//it's a team leader - add him to leaders map
			pLeader->SetAIGroup(it->second);
			it->second->SetLeader(pLeader);
		}
		else if (pObject->CastToCPipeUser())
		{
			// it's a soldier - find the leader of hi's team
			it->second->AddMember(pObject);
		}
	}
}

// removes specified object from group
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::RemoveFromGroup(int nGroupID, CAIObject* pObject)
{
	AIObjects::iterator gi;

	for (gi = m_mapGroups.find(nGroupID); gi != m_mapGroups.end(); )
	{
		if (gi->first != nGroupID)
			break;

		if (gi->second == pObject)
		{
			m_mapGroups.erase(gi);
			break;
		}
		++gi;
	}

	if (m_mapAIGroups.find(nGroupID) == m_mapAIGroups.end())
		return;

	UpdateGroupStatus(nGroupID);

	AIGroupMap::iterator it = m_mapAIGroups.find(nGroupID);
	if (it != m_mapAIGroups.end())
	{
		if (pObject->CastToCLeader())
			it->second->SetLeader(0);
		else
			it->second->RemoveMember(pObject->CastToCAIActor());
	}
}

void CAISystem::UpdateGroupStatus(int nGroupID)
{
	AIGroupMap::iterator it = m_mapAIGroups.find(nGroupID);
	if (it == m_mapAIGroups.end())
	{
		CAIGroup* pNewGroup = new CAIGroup(nGroupID);
		m_mapAIGroups.insert(AIGroupMap::iterator::value_type(nGroupID, pNewGroup));
		it = m_mapAIGroups.find(nGroupID);
	}

	CAIGroup* pGroup = it->second;
	AIAssert(pGroup);
	pGroup->UpdateGroupCountStatus();
}

// adds an object to a species
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddToFaction(CAIObject* pObject, uint8 factionID)
{
	if (factionID != IFactionMap::InvalidFactionID)
	{
		CWeakRef<CAIObject> refObj = GetWeakRef(pObject);

		AIObjects::iterator gi;

		// (MATT) Bit longwinded, isn't it? {2009/04/01}

		if ((gi = m_mapFaction.find(factionID)) != m_mapFaction.end())
		{
			// check whether it was added before
			while ((gi != m_mapFaction.end()) && (gi->first == factionID))
			{
				if (gi->second == refObj)
					return;
				++gi;
			}
		}

		// make sure it is not in with another species already
		AIObjects::iterator oi(m_mapFaction.begin());
		for (; oi != m_mapFaction.end(); ++oi)
		{
			if (oi->second == refObj)
			{
				m_mapFaction.erase(oi);
				break;
			}
		}

		// it has not been added, add it now
		m_mapFaction.insert(AIObjects::iterator::value_type(factionID, refObj));
	}
}

// creates a formation and associates it with a group of agents
//
//-----------------------------------------------------------------------------------------------------------
CFormation* CAISystem::CreateFormation(CWeakRef<CAIObject> refOwner, const char* szFormationName, Vec3 vTargetPos)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(refOwner);
	if (fi == m_mapActiveFormations.end())
	{
		FormationDescriptorMap::iterator di;
		di = m_mapFormationDescriptors.find(szFormationName);
		if (di != m_mapFormationDescriptors.end())
		{
			CCCPOINT(CAISystem_CreateFormation);

			CFormation* pFormation = new CFormation();
			pFormation->Create(di->second, refOwner, vTargetPos);
			m_mapActiveFormations.insert(FormationMap::iterator::value_type(refOwner, pFormation));
			return pFormation;
		}
	}
	else
		return (fi->second);
	return 0;
}

string CAISystem::GetFormationNameFromCRC32(unsigned int nCrc32ForFormationName) const
{
	FormationDescriptorMap::const_iterator currentFormation = m_mapFormationDescriptors.begin();
	FormationDescriptorMap::const_iterator end = m_mapFormationDescriptors.end();
	for (; currentFormation != end; ++currentFormation)
	{
		if (currentFormation->second.m_nNameCRC32 == nCrc32ForFormationName)
			return currentFormation->first;
	}

	return "";
}

// return a formation by id (used for serialization)
//
//-----------------------------------------------------------------------------------------------------------
CFormation* CAISystem::GetFormation(CFormation::TFormationID id)
{
	for (FormationMap::const_iterator it = m_mapActiveFormations.begin(), end = m_mapActiveFormations.end(); it != end; ++it)
	{
		if (it->second->GetId() == id)
			return it->second;
	}

	return NULL;
}

// change the current formation for the given group
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::ChangeFormation(IAIObject* pOwner, const char* szFormationName, float fScale)
{
	if (pOwner)
	{
		CFormation* pFormation = ((CAIObject*)pOwner)->m_pFormation;
		if (pFormation)
		{
			FormationDescriptorMap::iterator di;
			di = m_mapFormationDescriptors.find(szFormationName);
			if (di != m_mapFormationDescriptors.end())
			{
				CCCPOINT(CAISystem_ChangeFormation);

				pFormation->Change(di->second, fScale);
				return true;
			}
		}
	}
	return false;
}

// scale the current formation for the given group
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::ScaleFormation(IAIObject* pOwner, float fScale)
{
	CWeakRef<CAIObject> refOwner = GetWeakRef(static_cast<CAIObject*>(pOwner));
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(refOwner);
	if (fi != m_mapActiveFormations.end())
	{
		CFormation* pFormation = fi->second;
		if (pFormation)
		{
			CCCPOINT(CAISystem_ScaleFormation);

			pFormation->SetScale(fScale);
			return true;
		}
	}
	return false;
}

// scale the current formation for the given group
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::SetFormationUpdate(IAIObject* pOwner, bool bUpdate)
{
	CWeakRef<CAIObject> refOwner = GetWeakRef(static_cast<CAIObject*>(pOwner));
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(refOwner);
	if (fi != m_mapActiveFormations.end())
	{
		CFormation* pFormation = fi->second;
		if (pFormation)
		{
			pFormation->SetUpdate(bUpdate);
			return true;
		}
	}
	return false;
}

// check if puppet and vehicle are in the same formation
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::SameFormation(const CPuppet* pHuman, const CAIVehicle* pVehicle)
{
	if (pHuman->IsHostile(pVehicle))
		return false;
	for (FormationMap::iterator fi(m_mapActiveFormations.begin()); fi != m_mapActiveFormations.end(); ++fi)
	{
		CFormation* pFormation = fi->second;
		if (pFormation->GetFormationPoint(GetWeakRef(pHuman)))
		{
			CAIObject* pFormationOwner(pFormation->GetPointOwner(0));
			if (!pFormationOwner)
				return false;
			if (pFormationOwner == pVehicle->GetAttentionTarget())
			{
				CCCPOINT(CAISystem_SameFormation);
				return true;
			}
			return false;
		}
	}
	return false;
}

// Resets all agent states to initial
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Reset(IAISystem::EResetReason reason)
{
	MEMSTAT_LABEL("CAISystem::Reset");

	// Faction Map needs to reset and be present even when AISystem is disabled (i.e. multiplayer)
	{
		switch (reason)
		{
		case RESET_LOAD_LEVEL:
			gAIEnv.pFactionMap->Reload();
			break;
		case RESET_UNLOAD_LEVEL:
			gAIEnv.pFactionMap->Clear();
			break;
		default:
			break;
		}
	}

	// Don't reset if disabled, unless we're unloading the level in which case allow the cleanup to go ahead
	if (!IsEnabled() && reason != RESET_UNLOAD_LEVEL)
	{
		if (reason == RESET_LOAD_LEVEL)
		{
			/*

			   TODO : For games different from HUNT we will need to take a look at the memory leaks of the AI System
			   when the level is loaded without forcing the RESET_UNLOAD_LEVEL

			   // -------------------------------------------------------------------------------------------------------------------------

			   // Make sure RESET_UNLOAD_LEVEL is done.
			   // Currently if going to multi player without going to single player first there are lots of memory leaks if this isn't done
			   // due to allocations in CAISystem::Init().

			   // Don't do this since we use AI in multiplayer in Hunt, and this will cause
			   // it to not be initialized.
			   reason = RESET_UNLOAD_LEVEL;

			 */
		}
		else
		{
			return;
		}
	}

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "AI Reset");
	LOADING_TIME_PROFILE_SECTION;

	switch (reason)
	{
	case RESET_LOAD_LEVEL:
#if CAPTURE_REPLAY_LOG
		CryGetIMemReplay()->AddLabelFmt("AILoad");
#endif
		if (!gAIEnv.pTacticalPointSystem)
		{
			gAIEnv.pTacticalPointSystem = new CTacticalPointSystem();
			g_flyHelpersTacticalLanguageExtender.Initialize();
		}
		if (!gAIEnv.pGraph)
		{
			gAIEnv.pGraph = m_pGraph = new CGraph;
		}
		if (!gAIEnv.pNavigation)
		{
			gAIEnv.pNavigation = m_pNavigation = new CNavigation(gEnv->pSystem);
			m_pNavigation->Init();
		}
		if (!gAIEnv.pCoverSystem)
		{
			gAIEnv.pCoverSystem = new CCoverSystem("Scripts/AI/Cover.xml");
		}
		if (!gAIEnv.pNavigationSystem)
		{
			gAIEnv.pNavigationSystem = new NavigationSystem("Scripts/AI/Navigation.xml");
		}
		if (!gAIEnv.pMNMPathfinder)
		{
			gAIEnv.pPathfinderNavigationSystemUser = new MNM::PathfinderNavigationSystemUser;
			gAIEnv.pMNMPathfinder = gAIEnv.pPathfinderNavigationSystemUser->GetPathfinderImplementation();
			assert(gAIEnv.pNavigationSystem);
			gAIEnv.pNavigationSystem->RegisterUser(gAIEnv.pPathfinderNavigationSystemUser, "PathfinderExtension");
		}
		if (!gAIEnv.pVisionMap)
		{
			gAIEnv.pVisionMap = new CVisionMap();
		}
		if (!gAIEnv.pGroupManager)
		{
			gAIEnv.pGroupManager = new CGroupManager();
		}
		if (!gAIEnv.pAIActionManager)
		{
			gAIEnv.pAIActionManager = m_pAIActionManager = new CAIActionManager();
		}
		if (!gAIEnv.pSmartObjectManager)
		{
			gAIEnv.pSmartObjectManager = m_pSmartObjectManager = new CSmartObjectManager();
		}
		if (!gAIEnv.pRayCaster)
		{
			gAIEnv.pRayCaster = new SAIEnvironment::GlobalRayCaster;
			gAIEnv.pRayCaster->SetQuota(gAIEnv.CVars.RayCasterQuota);
		}
		if (!gAIEnv.pIntersectionTester)
		{
			gAIEnv.pIntersectionTester = new SAIEnvironment::GlobalIntersectionTester;
			gAIEnv.pIntersectionTester->SetQuota(gAIEnv.CVars.IntersectionTesterQuota);
		}
		if (!gAIEnv.pMovementSystem)
		{
			gAIEnv.pMovementSystem = MovementSystemCreator().CreateMovementSystem();
		}
		if (!gAIEnv.pActorLookUp)
		{
			gAIEnv.pActorLookUp = new ActorLookUp();
		}
		if (!gAIEnv.pWalkabilityCacheManager)
		{
			gAIEnv.pWalkabilityCacheManager = new WalkabilityCacheManager();
		}

		if (gAIEnv.pTargetTrackManager)
		{
			gAIEnv.pTargetTrackManager->Reset(reason);
		}
		if (gAIEnv.pPerceptionManager)
		{
			gAIEnv.pPerceptionManager->Reset(reason);
		}

		m_bInitialized = true;

		return;

	case RESET_UNLOAD_LEVEL:
		if (!gEnv->IsEditor())
		{
			if (gAIEnv.pTargetTrackManager)
				gAIEnv.pTargetTrackManager->Reset(RESET_UNLOAD_LEVEL);

#if !defined(_RELEASE)
			if (gAIEnv.pCodeCoverageGUI)
				gAIEnv.pCodeCoverageGUI->Reset(RESET_UNLOAD_LEVEL);
#endif

			for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
				delete it->second;
			stl::free_container(m_mapAIGroups);

			SAFE_DELETE(m_pGraph);
			gAIEnv.pGraph = NULL;
			SAFE_DELETE(m_pNavigation);
			gAIEnv.pNavigation = NULL;
			gAIEnv.pNavigationSystem->UnRegisterUser(gAIEnv.pPathfinderNavigationSystemUser);
			gAIEnv.pMNMPathfinder = NULL;
			SAFE_DELETE(gAIEnv.pPathfinderNavigationSystemUser);
			SAFE_DELETE(gAIEnv.pVisionMap);
			SAFE_DELETE(gAIEnv.pCoverSystem);
			SAFE_DELETE(gAIEnv.pNavigationSystem);
			SAFE_DELETE(gAIEnv.pGroupManager);
			SAFE_DELETE(m_pAIActionManager);
			gAIEnv.pAIActionManager = NULL;
			SAFE_DELETE(m_pSmartObjectManager);
			gAIEnv.pSmartObjectManager = NULL;
			SAFE_DELETE(gAIEnv.pRayCaster);
			SAFE_DELETE(gAIEnv.pIntersectionTester);
			SAFE_DELETE(gAIEnv.pActorLookUp);
			SAFE_DELETE(gAIEnv.pWalkabilityCacheManager);
			SAFE_DELETE(gAIEnv.pTacticalPointSystem);
			m_mapAuxSignalsFired.clear();
			stl::free_container(m_sWorkingFolder);
			stl::free_container(m_priorityTargets);
			gAIEnv.pPerceptionManager->Reset(reason);
			gAIEnv.pCommunicationManager->Reset();
			m_PipeManager.ClearAllGoalPipes();
			CPuppet::ClearStaticData();
			stl::free_container(m_walkabilityPhysicalEntities);
			if (m_walkabilityGeometryBox)
			{
				m_walkabilityGeometryBox->Release();
				m_walkabilityGeometryBox = NULL;
			}

			gAIEnv.pCollisionAvoidanceSystem->Reset(true);

			CleanupAICollision();
			ClearAIObjectIteratorPools();

			CPathObstacles::ResetOfStaticData();
			AISignalExtraData::CleanupPool();

			stl::free_container(m_tmpFullUpdates);
			stl::free_container(m_tmpDryUpdates);
			stl::free_container(m_tmpAllUpdates);

#ifdef CRYAISYSTEM_DEBUG
			stl::free_container(m_DEBUG_fakeTracers);
			stl::free_container(m_DEBUG_fakeHitEffect);
			stl::free_container(m_DEBUG_fakeDamageInd);
#endif //CRYAISYSTEM_DEBUG

			gAIEnv.pBehaviorTreeManager->Reset();
			gAIEnv.pGraftManager->Reset();
			gAIEnv.pMovementSystem->Reset();

			m_bInitialized = false;

			MEMSTAT_LABEL("AIUnload");
		}
		return;
	}

	AILogEvent("CAISystem::Reset %d", reason);
	m_bUpdateSmartObjects = false;

	// Notify listeners
	for (SystemListenerSet::iterator it = m_setSystemListeners.begin(); it != m_setSystemListeners.end(); ++it)
	{
		(*it)->OnEvent(IAISystemListener::eAISE_Reset);
	}

	if (gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager->Reset(reason);

#ifdef CRYAISYSTEM_DEBUG
	m_Recorder.OnReset(reason);
#endif //CRYAISYSTEM_DEBUG

	if (gAIEnv.pCoverSystem)
		gAIEnv.pCoverSystem->Reset();

	if (gAIEnv.pMovementSystem)
		gAIEnv.pMovementSystem->Reset();

	// It would be much better to have this at the end of Reset, when state is minimal
	// However there are various returns below that stop it being hit

	SetAIHacksConfiguration();

	// May need to make calls here to get those settings reflected in the system

	if (reason != RESET_ENTER_GAME)
		// Reset Interest System
		GetCentralInterestManager()->Reset();

	m_lightManager.Reset();

#if !defined(_RELEASE)
	gAIEnv.pCodeCoverageGUI->Reset(reason);
#endif

	// reset the groups.
	for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
		it->second->Reset();

	for (int i = 0; i < NUM_ALERTNESS_COUNTERS; i++)
		m_AlertnessCounters[i] = 0;

	if (!m_bInitialized)
		return;

	const char* sStatsTarget = gAIEnv.CVars.StatsTarget;
	if ((*sStatsTarget != '\0') && (stricmp("none", sStatsTarget) != 0))
		Record(NULL, IAIRecordable::E_RESET, NULL);

	if (m_pSmartObjectManager->IsInitialized())
	{
		m_pAIActionManager->Reset();

		// If CAISystem::Reset() was called from CAISystem::Shutdown() IEntitySystem is already deleted
		if (gEnv->pEntitySystem)
		{
			if (reason == IAISystem::RESET_ENTER_GAME || reason == IAISystem::RESET_INTERNAL_LOAD)
			{
				m_pSmartObjectManager->SoftReset();
			}
		}
	}
	else
	{
		// If CAISystem::Reset() was called from CAISystem::Shutdown() IEntitySystem is already deleted
		if (gEnv->pEntitySystem)
			InitSmartObjects();
	}

	//m_mapDEBUGTiming.clear();

	AIObjectOwners::iterator ai;

	EObjectResetType objectResetType = AIOBJRESET_INIT;
	switch (reason)
	{
	case RESET_EXIT_GAME:
		objectResetType = AIOBJRESET_SHUTDOWN;
		break;
	case RESET_INTERNAL:
	case RESET_INTERNAL_LOAD:
	case RESET_ENTER_GAME:
	default:
		objectResetType = AIOBJRESET_INIT;
		break;
	}

	const bool editorToGameMode = (gEnv->IsEditor() && reason == RESET_ENTER_GAME);
	if (editorToGameMode)
	{
		gAIEnv.pBehaviorTreeManager->Reset();
		gAIEnv.pGraftManager->Reset();

		CallReloadTPSQueriesScript();
	}

	if (reason == RESET_LOAD_LEVEL)
	{
		CallReloadTPSQueriesScript();
	}

	for (ai = gAIEnv.pAIObjectManager->m_Objects.begin(); ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		// Strong, so always valid
		CAIObject* pObject = ai->second.GetAIObject();
		pObject->Reset(objectResetType);

#ifdef CRYAISYSTEM_DEBUG
		// Reset the AI recordable stuff when entering game mode.
		if (reason == IAISystem::RESET_ENTER_GAME)
		{
			CTimeValue startTime = gEnv->pTimer->GetFrameStartTime();
			CRecorderUnit* pRecord = static_cast<CRecorderUnit*>(pObject->GetAIDebugRecord());
			if (pRecord)
			{
				pRecord->ResetStreams(startTime);
			}
		}
#endif //CRYAISYSTEM_DEBUG

	}

#ifdef CRYAISYSTEM_DEBUG

	// Reset the recorded trajectory.
	m_lastStatsTargetTrajectoryPoint.Set(0, 0, 0);
	m_lstStatsTargetTrajectory.clear();

	m_DEBUG_fakeTracers.clear();
	m_DEBUG_fakeHitEffect.clear();
	m_DEBUG_fakeDamageInd.clear();
	m_DEBUG_screenFlash = 0.0f;

#endif //CRYAISYSTEM_DEBUG

	if (!m_mapActiveFormations.empty())
	{
		FormationMap::iterator fi;
		for (fi = m_mapActiveFormations.begin(); fi != m_mapActiveFormations.end(); ++fi)
		{
			CFormation* pFormation = fi->second;
			if (pFormation)
			{
				delete pFormation;
			}
		}
		m_mapActiveFormations.clear();
	}

	if (reason == RESET_EXIT_GAME)
	{
		gAIEnv.pTacticalPointSystem->Reset();
		gAIEnv.pCommunicationManager->Reset();
		gAIEnv.pSelectionTreeManager->Reset();
		gAIEnv.pVisionMap->Reset();
		gAIEnv.pFactionMap->Reload();

		gAIEnv.pRayCaster->ResetContentionStats();
		gAIEnv.pIntersectionTester->ResetContentionStats();
	}

	gAIEnv.pGroupManager->Reset(objectResetType);

	ClearAIObjectIteratorPools();
	m_pNavigation->Reset(reason);
	gAIEnv.pNavigationSystem->Reset();

	m_pGraph->ClearMarks();
	m_pGraph->Reset();

	CPathObstacles::ResetOfStaticData();

	m_dynHideObjectManager.Reset();

	m_mapAuxSignalsFired.clear();

	m_mapBeacons.clear();

	// Remove temporary shapes and re-enable all shapes.
	for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); )
	{
		// Enable
		it->second.enabled = true;
		// Remove temp
		if (it->second.temporary)
			m_mapGenericShapes.erase(it++);
		else
			++it;
	}

	m_pSmartObjectManager->ResetBannedSOs();

	m_lastAmbientFireUpdateTime.SetSeconds(-10.0f);
	m_lastExpensiveAccessoryUpdateTime.SetSeconds(-10.0f);
	m_lastVisBroadPhaseTime.SetSeconds(-10.0f);
	m_lastGroupUpdateTime.SetSeconds(-10.0f);

	m_delayedExpAccessoryUpdates.clear();

	gAIEnv.pPerceptionManager->Reset(reason);

	ResetAIActorSets(reason != RESET_ENTER_GAME);

	DebugOutputObjects("End of reset");

#ifdef CALIBRATE_STOPPER
	AILogAlways("Calculation stopper calibration:");
	for (CCalculationStopper::TMapCallRate::const_iterator it = CCalculationStopper::m_mapCallRate.begin(); it != CCalculationStopper::m_mapCallRate.end(); ++it)
	{
		const std::pair<unsigned, float>& result = it->second;
		const string& name = it->first;
		float rate = result.second > 0.0f ? result.first / result.second : -1.0f;
		AILogAlways("%s calls = %d time = %6.2f sec call-rate = %7.2f", f
		            name.c_str(), result.first, result.second, rate);
	}
#endif

#ifdef CRYAISYSTEM_DEBUG
	if (gAIEnv.pBubblesSystem)
		gAIEnv.pBubblesSystem->Reset();
#endif

	if (reason == IAISystem::RESET_ENTER_GAME || reason == IAISystem::RESET_UNLOAD_LEVEL)
	{
		gAIEnv.pSequenceManager->Reset();
		gAIEnv.pClusterDetector->Reset();
	}

	if (reason == RESET_EXIT_GAME || reason == RESET_UNLOAD_LEVEL)
	{
		gAIEnv.pBehaviorTreeManager->Reset();
		gAIEnv.pGraftManager->Reset();
	}
}

//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetGroupCount(int nGroupID, int flags, int type)
{
	if (type == 0)
	{
		AIGroupMap::iterator it = m_mapAIGroups.find(nGroupID);
		if (it == m_mapAIGroups.end())
			return 0;

		return it->second->GetGroupCount(flags);
	}
	else
	{
		int count = 0;
		int countEnabled = 0;
		for (AIObjects::iterator it = m_mapGroups.find(nGroupID); it != m_mapGroups.end() && it->first == nGroupID; ++it)
		{
			CAIObject* pObject = it->second.GetAIObject();
			;
			if (pObject->GetType() == type)
			{
				if (pObject->IsEnabled())
					countEnabled++;
				count++;
			}
		}

		if (flags & IAISystem::GROUP_MAX)
		{
			AIWarning("GetGroupCount does not support specified type and max count to be used at the same time." /*, type*/);
			flags &= ~IAISystem::GROUP_MAX;
		}

		if (flags == 0 || flags == IAISystem::GROUP_ALL)
			return count;
		else if (flags == IAISystem::GROUP_ENABLED)
			return countEnabled;
	}

	return 0;
}

// gets the I-th agent in a specified group
//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetGroupMember(int nGroupID, int nIndex, int flags, int type)
{
	if (nIndex < 0)
		return NULL;

	bool bOnlyEnabled = false;
	if (flags & IAISystem::GROUP_ENABLED)
		bOnlyEnabled = true;

	AIObjects::iterator gi = m_mapGroups.find(nGroupID);
	for (; gi != m_mapGroups.end() && nIndex >= 0; ++gi)
	{
		CAIObject* pObject = gi->second.GetAIObject();
		if (gi->first != nGroupID)
			return NULL;
		if ((pObject != NULL) && pObject->GetType() != AIOBJECT_LEADER && // skip leaders
		    (!type || pObject->GetType() == type) &&
		    (!bOnlyEnabled || pObject->IsEnabled()))
		{
			if (0 == nIndex)
				return pObject;
			--nIndex;
			// (MATT) Why decrement index? {2009/03/26}
		}
	}
	return NULL;
}

//====================================================================
// GetLeader
//====================================================================
CLeader* CAISystem::GetLeader(int nGroupID)
{
	AIGroupMap::iterator it = m_mapAIGroups.find(nGroupID);
	if (it != m_mapAIGroups.end())
		return it->second->GetLeader();
	return 0;
}

CLeader* CAISystem::GetLeader(const CAIActor* pSoldier)
{
	return (pSoldier ? GetLeader(pSoldier->GetGroupId()) : NULL);
}

//====================================================================
// SetLeader
//====================================================================
void CAISystem::SetLeader(IAIObject* pObject)
{
	// can't make disabled/dead puppet a leader
	if (!pObject->IsEnabled() || pObject->GetProxy() && pObject->GetProxy()->IsDead())
		return;
	CAIActor* pActor = pObject->CastToCAIActor();
	if (!pActor)
		return;
	int groupid = pActor->GetGroupId();
	CLeader* pLeader = GetLeader(groupid);
	if (!pLeader)
	{
		pLeader = (CLeader*) m_AIObjectManager.CreateAIObject(AIObjectParams(AIOBJECT_LEADER, pObject));
		CAIGroup* pGroup = GetAIGroup(groupid);
		if (pGroup)
			pGroup->SetLeader(pLeader);
	}
	else
	{
		pLeader->SetAssociation(GetWeakRef((CAIObject*)pObject));
		CCCPOINT(CAISystem_SetLeader);
	}

	return;
}

//====================================================================
// SetLeader
//====================================================================
CLeader* CAISystem::CreateLeader(int nGroupID)
{
	CLeader* pLeader = GetLeader(nGroupID);
	if (pLeader)
		return NULL;
	CAIGroup* pGroup = GetAIGroup(nGroupID);
	if (!pGroup)
		return NULL;
	TUnitList& unitsList = pGroup->GetUnits();
	if (unitsList.empty())
		return NULL;

	CAIActor* pLeaderActor = (unitsList.begin())->m_refUnit.GetAIObject();
	pLeader = (CLeader*) m_AIObjectManager.CreateAIObject(AIObjectParams(AIOBJECT_LEADER, pLeaderActor));
	pGroup->SetLeader(pLeader);
	return pLeader;
}

//====================================================================
// GetAIGroup
//====================================================================
CAIGroup* CAISystem::GetAIGroup(int nGroupID)
{
	AIGroupMap::iterator it = m_mapAIGroups.find(nGroupID);
	if (it != m_mapAIGroups.end())
		return it->second;
	return 0;
}

IAIGroup* CAISystem::GetIAIGroup(int nGroupID)
{
	return (IAIGroup*)GetAIGroup(nGroupID);
}

//====================================================================
// ReadAreasFromFile
//====================================================================
#if defined(SEG_WORLD)
void CAISystem::ReadAreasFromFile(const char* fileNameAreas, const Vec3& vSegmentOffset)
#else
void CAISystem::ReadAreasFromFile(const char* fileNameAreas)
#endif
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Navigation, 0, "Areas (%s)", fileNameAreas);
	LOADING_TIME_PROFILE_SECTION

	CCryBufferedFileReader file;
	if (false != file.Open(fileNameAreas, "rb"))
	{
		int iNumber = 0;
		file.ReadType(&iNumber);

		if (iNumber == 18)
		{
			AIWarning("AI area file version 18 found - suitable only for subsequent export");
		}
		else if (iNumber < BAI_AREA_FILE_VERSION_READ)
		{
			AIError("Incompatible AI area file version - found %d, expected range [%d,%d] - regenerate triangulation [Design bug]", iNumber, BAI_AREA_FILE_VERSION_READ, BAI_AREA_FILE_VERSION_WRITE);
			file.Close();
			return;
		}

		if (iNumber < BAI_AREA_FILE_VERSION_WRITE)
			m_pNavigation->ReadAreasFromFile_Old(file, iNumber);
		else
		{
#if defined(SEG_WORLD)
			m_pNavigation->ReadAreasFromFile(file, iNumber, vSegmentOffset);
#else
			m_pNavigation->ReadAreasFromFile(file, iNumber);
#endif
		}

#if defined(SEG_WORLD)
		// don't flush all areas here if we're in segmented world
		if (!gEnv->p3DEngine->GetSegmentsManager()) //@TODO: make seg-world manager available from gEnv.
			FlushAllAreas();
#else
		FlushAllAreas();
#endif

		unsigned numAreas;

		// Read generic shapes
		if (iNumber > 15)
		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0; iArea < numAreas; ++iArea)
			{
				ListPositions lp;
				string name;
#if defined(SEG_WORLD)
				ReadPolygonArea(file, iNumber, name, lp, vSegmentOffset);
#else
				ReadPolygonArea(file, iNumber, name, lp);
#endif

				int navType, type;
				file.ReadType(&navType);
				file.ReadType(&type);
				float height = 0;
				int lightLevel = 0;
				if (iNumber >= 18)
				{
					file.ReadType(&height);
					file.ReadType(&lightLevel);
				}

				if (m_mapGenericShapes.find(name) != m_mapGenericShapes.end())
					AIError("CAISystem::ReadAreasFromFile: Shape '%s' already exists, please rename the path and reexport.", name.c_str());
				else
					m_mapGenericShapes.insert(ShapeMap::iterator::value_type(name, SShape(lp, false, (IAISystem::ENavigationType)navType, type, true, height, (EAILightLevel)lightLevel)));
			}
		}
		file.Close();
	}
	else
	{
		AIWarning("Unable to open areas file %s", fileNameAreas);
	}
}

// // loads the triangulation for this level and mission
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::LoadNavigationData(const char* szLevel, const char* szMission,
                                   const bool bRequiredQuickLoading /* = false */, const bool bAfterExporting /* = false */)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Navigation, 0, "AI Navigation");

	LOADING_TIME_PROFILE_SECTION(GetISystem());

	if (!IsEnabled())
		return;

	if (!szLevel || !szMission)
		return;

	if (m_bInitialized)
		Reset(IAISystem::RESET_INTERNAL_LOAD);
	m_pNavigation->LoadNavigationData(szLevel, szMission);

	if (!bRequiredQuickLoading)
	{
		//////////////////////////////////////////////////////////////////////////
		/// MNM
		/// First clear any previous data, and then load stored meshes
		/// Note smart objects must go after, they will link to the mesh after is loaded
		gAIEnv.pNavigationSystem->Clear();
		ISegmentsManager* pSegmentsManager = gEnv->p3DEngine->GetSegmentsManager();
		if (gEnv->IsEditor() || !pSegmentsManager)
		{
			char mnmFileName[1024] = { 0 };
			cry_sprintf(mnmFileName, "%s/mnmnav%s.bai", szLevel, szMission);
			gAIEnv.pNavigationSystem->ReadFromFile(mnmFileName, bAfterExporting);
		}
#ifdef DEDICATED_SERVER
		else
		{
			// load ai data from segment pak instead
			pSegmentsManager->ForceLoadSegments(ISegmentsManager::slfNavigation);
		}
#endif
	}

	m_pSmartObjectManager->SoftReset(); // Re-register smart objects
}

void CAISystem::LoadCover(const char* szLevel, const char* szMission)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Navigation, 0, "Cover system");
	LOADING_TIME_PROFILE_SECTION(GetISystem());
	gAIEnv.pCoverSystem->Clear();

	char coverFileName[1024] = { 0 };
	cry_sprintf(coverFileName, "%s/cover%s.bai", szLevel, szMission);
	gAIEnv.pCoverSystem->ReadSurfacesFromFile(coverFileName);
}

void CAISystem::LoadLevelData(const char* szLevel, const char* szMission, const bool bRequiredQuickLoading /* = false */)
{
	LOADING_TIME_PROFILE_SECTION;

	CRY_ASSERT(szLevel);
	CRY_ASSERT_TRACE(szMission && szMission[0],
	                 ("Loading AI data for level %s without a mission: AI navigation will be broken!",
	                  szLevel ? szLevel : "<None>"));

	bool bLoadAI = true;
	if (gEnv->bMultiplayer)
	{
		bLoadAI = IsEnabled();
		if (ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem"))
		{
			if (!pEnableAI->GetIVal())
			{
				bLoadAI = false;
			}
		}
	}
	if (bLoadAI)
	{
		SetLevelPath(szLevel);
		LoadNavigationData(szLevel, szMission, bRequiredQuickLoading);
		LoadCover(szLevel, szMission);
	}
}

//====================================================================
// OnMissionLoaded
//====================================================================
void CAISystem::OnMissionLoaded()
{
	m_pNavigation->OnMissionLoaded();
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ReleaseFormation(CWeakRef<CAIObject> refOwner, bool bDelete)
{
	CCCPOINT(CAISystem_ReleaseFormation);

	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(refOwner);
	if (fi != m_mapActiveFormations.end())
	{
		CFormation* pFormation = fi->second;
		if (bDelete && pFormation)
		{
			//pFormation->ReleasePoints();
			delete pFormation;
		}
		m_mapActiveFormations.erase(fi);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::FreeFormationPoint(CWeakRef<CAIObject> refOwner)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(refOwner);
	if (fi != m_mapActiveFormations.end())
		(fi->second)->FreeFormationPoint(refOwner);
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::FlushSystem(bool bDeleteAll)
{
	if (!IsEnabled())
	{
		return;
	}

	AILogEvent("CAISystem::FlushSystem");
	FlushSystemNavigation(bDeleteAll);
	// remove all the leaders first;
	gAIEnv.pAIObjectManager->m_Objects.erase(AIOBJECT_LEADER);
	;
	m_lightManager.Reset();

	if (m_pSmartObjectManager)
		m_pSmartObjectManager->ResetBannedSOs();

	m_dynHideObjectManager.Reset();

	m_mapFaction.clear();
	m_mapGroups.clear();
	m_mapBeacons.clear();

	FormationMap::iterator iFormation = m_mapActiveFormations.begin(), iEnd = m_mapActiveFormations.end();
	for (; iFormation != iEnd; ++iFormation)
		delete iFormation->second;
	m_mapActiveFormations.clear();

	if (gAIEnv.pVisionMap)
		gAIEnv.pVisionMap->Reset();

	if (gAIEnv.pGroupManager)
		gAIEnv.pGroupManager->Reset(AIOBJRESET_SHUTDOWN);

	GetCentralInterestManager()->Reset();

	if (gAIEnv.pCommunicationManager)
		gAIEnv.pCommunicationManager->Reset();

	if (gAIEnv.pCoverSystem)
		gAIEnv.pCoverSystem->Clear();

	if (gAIEnv.pRayCaster)
		gAIEnv.pRayCaster->Reset();

	if (gAIEnv.pIntersectionTester)
		gAIEnv.pIntersectionTester->Reset();

	if (gAIEnv.pMovementSystem)
		gAIEnv.pMovementSystem->Reset();

	if (gAIEnv.pTacticalPointSystem)
		gAIEnv.pTacticalPointSystem->Reset();

	if (gAIEnv.pSequenceManager)
		gAIEnv.pSequenceManager->Reset();

	if (gAIEnv.pClusterDetector)
		gAIEnv.pClusterDetector->Reset();

#ifdef CRYAISYSTEM_DEBUG
	if (gAIEnv.pBubblesSystem)
		gAIEnv.pBubblesSystem->Reset();
#endif

	// Remove all the objects themselves
	gAIEnv.pAIObjectManager->Reset();

	// (MATT) Flush all the objects that may have been deregistered on leaving a previous level
	// Really, this should delete all AI objects regardless {2009/03/27}
	gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(true);
	gAIEnv.pObjectContainer->Reset();

	AIAssert(m_enabledAIActorsSet.empty());
	AIAssert(m_disabledAIActorsSet.empty());
	ResetAIActorSets(true);

	stl::free_container(m_mapGenericShapes);

	gAIEnv.pBehaviorTreeManager->Reset();
	gAIEnv.pGraftManager->Reset();
}

void CAISystem::LayerEnabled(const char* layerName, bool enabled, bool serialized)
{
	if (enabled)
	{
		if (m_pNavigation)
			m_pNavigation->BumpDynamicLinkConnectionUpdateTime(gAIEnv.CVars.LayerSwitchDynamicLinkBump, gAIEnv.CVars.LayerSwitchDynamicLinkBumpDuration);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::FlushSystemNavigation(bool bDeleteAll)
{
	AILogEvent("CAISystem::FlushSystemNavigation");
	// clear any paths in the puppets
	AIObjectOwners::iterator ai;
	for (ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_ACTOR); ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		// Strong, so always valid
		CPuppet* pPuppet = ai->second.GetAIObject()->CastToCPuppet();
		if (ai->first != AIOBJECT_ACTOR)
			break;
		if (pPuppet)
			pPuppet->Reset(AIOBJRESET_INIT);
	}

	for (ai = gAIEnv.pAIObjectManager->m_Objects.begin(); ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		CAIObject* pObject = ai->second.GetAIObject();

		if (pObject)
		{
			pObject->Reset(AIOBJRESET_INIT);
		}
	}

	if (m_pNavigation)
	{
		m_pNavigation->FlushSystemNavigation(bDeleteAll);
	}
}

float CAISystem::GetUpdateInterval() const
{
	if (gAIEnv.CVars.AIUpdateInterval)
		return gAIEnv.CVars.AIUpdateInterval;
	else
		return 0.1f;
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetAITickCount(void)
{
	return m_nTickCount;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SendAnonymousSignal(int signalID, const char* text, const Vec3& pos, float radius, IAIObject* pSenderObject, IAISignalExtraData* pData)
{
	CCCPOINT(CAISystem_SendAnonymousSignal);

	IEntity* const pSenderEntity = pSenderObject ? pSenderObject->GetEntity() : NULL;

	// Go trough all the puppets and vehicles in the surrounding area.
	// Still makes precise radius check inside because the grid might
	// return objects in a bigger radius.
	SEntityProximityQuery query;
	query.nEntityFlags = ENTITY_FLAG_HAS_AI;
	query.pEntityClass = NULL;
	query.box = AABB(Vec3(pos.x - radius, pos.y - radius, pos.z - radius), Vec3(pos.x + radius, pos.y + radius, pos.z + radius));
	gEnv->pEntitySystem->QueryProximity(query);

	const float radiusSq = square(radius);
	for (int i = 0; i < query.nCount; ++i)
	{
		IEntity* const pReceiverEntity = query.pEntities[i];
		if (pReceiverEntity)
		{
			IAIObject* pReceiverAI = pReceiverEntity->GetAI();
			if (!pReceiverAI)
				continue;

			CPuppet* pReceiverPuppet = pReceiverAI->CastToCPuppet();
			if (pReceiverPuppet)
			{
				if (pReceiverPuppet->GetParameters().m_PerceptionParams.perceptionScale.visual > 0.01f &&
				    pReceiverPuppet->GetParameters().m_PerceptionParams.perceptionScale.audio > 0.01f &&
				    Distance::Point_PointSq(pReceiverPuppet->GetPos(), pos) < radiusSq)
				{
					pReceiverPuppet->SetSignal(signalID, text, pSenderEntity, (pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL));
				}
			}
		}
	}

	// Finally delete pData since all recipients have their own copies
	if (pData)
	{
		delete (AISignalExtraData*)pData;
	}
}

void CAISystem::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_SW_SHIFT_WORLD:
		// offset all AI areas when segmented world shifts.
		OffsetAllAreas(*(const Vec3*)wparam);
		break;
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ReleaseFormationPoint(CAIObject* pReserved)
{
	if (m_mapActiveFormations.empty())
		return;

	CCCPOINT(CAISystem_ReleaseFormationPoint);

	CWeakRef<CAIObject> refReserved = GetWeakRef(pReserved);

	FormationMap::iterator fi;
	CAIActor* pActor = pReserved->CastToCAIActor();
	CLeader* pLeader = GetLeader(pActor);
	if (pLeader)
		pLeader->FreeFormationPoint(refReserved);
	else
		for (fi = m_mapActiveFormations.begin(); fi != m_mapActiveFormations.end(); ++fi)
			(fi->second)->FreeFormationPoint(refReserved);
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Update(CTimeValue frameStartTime, float frameDeltaTime)
{
	CRY_PROFILE_REGION(PROFILE_AI, "AI System: Update");
	CRYPROFILE_SCOPE_PROFILE_MARKER("AI System: Update");
	AISYSTEM_LIGHT_PROFILER();

	static CTimeValue lastFrameStartTime;
	if (frameStartTime == lastFrameStartTime)
		return;

	lastFrameStartTime = frameStartTime;

	if (!m_bInitialized || !IsEnabled())
		return;

	if (!gAIEnv.CVars.AiSystem)
		return;

	CCCPOINT(CAISystem_Update);

	//	for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
	//		it->second->Validate();

	{
		FRAME_PROFILER("AIUpdate - 1", gEnv->pSystem, PROFILE_AI)

		if (m_pSmartObjectManager && !m_pSmartObjectManager->IsInitialized())
			InitSmartObjects();

		if (m_pAIActionManager)
			m_pAIActionManager->Update();

		m_frameStartTime = frameStartTime;
		m_frameStartTimeSeconds = frameStartTime.GetSeconds();
		m_frameDeltaTime = frameDeltaTime;

		CAIRadialOcclusionRaycast::UpdateActiveCount();

		m_lightManager.Update(false);

		if (m_pNavigation->GetNavDataState() == CNavigation::NDS_BAD)
		{
			static CTimeValue lastTime = frameStartTime;
			if ((frameStartTime - lastTime).GetMilliSecondsAsInt64() > 5000)
			{
				AIWarning("*** AI navigation is bad. Please regenerate. AI SYSTEM IS NOT UPDATED");
				lastTime = frameStartTime;
			}
			return;
		}
		else if (m_pNavigation->GetNavDataState() != CNavigation::NDS_OK)
		{
			gAIEnv.pPerceptionManager->Update(m_frameDeltaTime);
			return;
		}

#ifdef STOPPER_CAN_USE_COUNTER
		CCalculationStopper::m_useCounter = gAIEnv.CVars.UseCalculationStopperCounter != 0;
#endif

		m_pNavigation->Update(frameStartTime, frameDeltaTime);

		assert(m_pSmartObjectManager);
		PREFAST_ASSUME(m_pSmartObjectManager);
		m_pSmartObjectManager->UpdateBannedSOs(m_frameDeltaTime);
	}

	{
		FRAME_PROFILER("AIUpdate - 2", gEnv->pSystem, PROFILE_AI);

		UpdateAmbientFire();
		UpdateExpensiveAccessoryQuota();

		gAIEnv.pPerceptionManager->Update(m_frameDeltaTime);

		gAIEnv.pCommunicationManager->Update(frameDeltaTime);
		gAIEnv.pVisionMap->Update(frameDeltaTime);
		gAIEnv.pGroupManager->Update(frameDeltaTime);
		gAIEnv.pCoverSystem->Update(frameDeltaTime);

		{
			FRAME_PROFILER("AIUpdate - 2 - NavigationSystem", gEnv->pSystem, PROFILE_AI);

			gAIEnv.pNavigationSystem->Update(false);
		}

		{
			FRAME_PROFILER("AIUpdate - 2 - Walkability Cache", gEnv->pSystem, PROFILE_AI);

			gAIEnv.pWalkabilityCacheManager->PreUpdate();
		}
	}

	{
		FRAME_PROFILER("AIUpdate 3", gEnv->pSystem, PROFILE_AI)

		// Marcio: Update all players.
		AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_PLAYER);

		for (; ai != gAIEnv.pAIObjectManager->m_Objects.end() && ai->first == AIOBJECT_PLAYER; ++ai)
		{
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(ai->second.GetAIObject());
			if (pPlayer)
				pPlayer->Update(AIUPDATE_FULL);
		}

		{
			FRAME_PROFILER("AIUpdate 3 - Groups", gEnv->pSystem, PROFILE_AI);

			const int64 dt = (frameStartTime - m_lastGroupUpdateTime).GetMilliSecondsAsInt64();
			if (dt > gAIEnv.CVars.AIUpdateInterval)
			{
				for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
					it->second->Update();

				m_lastGroupUpdateTime = frameStartTime;
			}
		}

		UpdateAuxSignalsMap();
	}

	{
		FRAME_PROFILER("MovementSystem", gEnv->pSystem, PROFILE_AI);

		gAIEnv.pMovementSystem->Update(frameDeltaTime);
	}

	{
		FRAME_PROFILER("AIUpdate 4 - Puppet Update", gEnv->pSystem, PROFILE_AI);

		AIAssert(!m_iteratingActorSet);
		m_iteratingActorSet = true;

		RemoveNonActors(m_enabledAIActorsSet);

		AIActorVector& fullUpdates = m_tmpFullUpdates;
		fullUpdates.resize(0);

		AIActorVector& dryUpdates = m_tmpDryUpdates;
		dryUpdates.resize(0);

		AIActorVector& allUpdates = m_tmpAllUpdates;
		allUpdates.resize(0);

		uint32 activeAIActorCount = m_enabledAIActorsSet.size();
		gAIEnv.pStatsManager->SetStat(eStat_ActiveActors, static_cast<float>(activeAIActorCount));

		if (activeAIActorCount > 0)
		{
			const float updateInterval = max(gAIEnv.CVars.AIUpdateInterval, 0.0001f);
			const float updatesPerSecond = (activeAIActorCount / updateInterval) + m_enabledActorsUpdateError;
			unsigned actorUpdateCount = (unsigned)floorf(updatesPerSecond * m_frameDeltaTime);
			if (m_frameDeltaTime > 0.0f)
				m_enabledActorsUpdateError = updatesPerSecond - actorUpdateCount / m_frameDeltaTime;

			// Collect list of enabled priority targets (players, grenades, projectiles, etc).
			m_priorityTargets.resize(0);

			for (unsigned i = 0, ni = m_priorityObjectTypes.size(); i < ni; ++i)
			{
				short type = m_priorityObjectTypes[i];
				AIObjectOwners::iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(type);
				for (; ai != gAIEnv.pAIObjectManager->m_Objects.end() && ai->first == type; ++ai)
				{
					CAIObject* pTarget = ai->second.GetAIObject();
					if (!pTarget->IsEnabled())
						continue;

					m_priorityTargets.push_back(pTarget);
				}
			}

			uint32 fullUpdateCount = 0;
			uint32 skipped = 0;
			m_enabledActorsUpdateHead %= activeAIActorCount;
			uint32 idx = m_enabledActorsUpdateHead;

			for (uint32 i = 0; i < activeAIActorCount; ++i)
			{
				// [AlexMcC|29.06.10] We can't trust that this is an AI Actor, because CWeakRef::GetAIObject()
				// doesn't do any type checking. If this weakref becomes invalid, then the id is used by another
				// object (which can happen if we chainload or load a savegame), this might not be an actor anymore.

				IAIObject* object = m_enabledAIActorsSet[idx++ % activeAIActorCount].GetAIObject();
				CAIActor* actor = object->CastToCAIActor();
				AIAssert(actor);

				if (actor)
				{
					if (object->GetAIType() != AIOBJECT_PLAYER)
					{
						if (fullUpdates.size() < actorUpdateCount)
							fullUpdates.push_back(actor);
						else
							dryUpdates.push_back(actor);
					}
					else if (fullUpdates.size() < actorUpdateCount)
					{
						++skipped;
					}

					allUpdates.push_back(actor);
				}
			}

			{
				FRAME_PROFILER("AIUpdate 4 - Full Updates", gEnv->pSystem, PROFILE_AI);

				AIActorVector::iterator it = fullUpdates.begin();
				AIActorVector::iterator end = fullUpdates.end();

				for (; it != end; ++it)
				{
					CAIActor* pAIActor = *it;
					if (CPuppet* pPuppet = pAIActor->CastToCPuppet())
						pPuppet->SetUpdatePriority(CalcPuppetUpdatePriority(pPuppet));

					// Full update
					gAIEnv.pPerceptionManager->UpdatePerception(pAIActor, m_priorityTargets);
					pAIActor->Update(AIUPDATE_FULL);

					BroadcastToListeners(OnAgentUpdate(pAIActor->GetEntityID()));

					if (!pAIActor->m_bUpdatedOnce && m_bUpdateSmartObjects && pAIActor->IsEnabled())
					{
						if (CPipeUser* pPipeUser = pAIActor->CastToCPipeUser())
						{
							if (!pPipeUser->GetCurrentGoalPipe() || !pPipeUser->GetCurrentGoalPipe()->GetSubpipe())
							{
								pAIActor->m_bUpdatedOnce = true;
							}
						}
						else
						{
							pAIActor->m_bUpdatedOnce = true;
						}
					}

					m_totalActorsUpdateCount++;

					if (m_totalActorsUpdateCount >= (int)activeAIActorCount)
					{
						// full update cycle finished on all ai objects
						// now allow updating smart objects
						m_bUpdateSmartObjects = true;
						m_totalActorsUpdateCount = 0;
					}

					fullUpdateCount++;
				}

				// CE-1629: special case if there is only a CAIPlayer (and no other CAIActor) to ensure that smart-objects will get updated
				if (fullUpdates.empty() && actorUpdateCount > 0)
				{
					m_bUpdateSmartObjects = true;
				}

				// Advance update head.
				m_enabledActorsUpdateHead += fullUpdateCount;
				m_enabledActorsUpdateHead += skipped;
			}

			{
				FRAME_PROFILER("AIUpdate 4 - Dry Updates", gEnv->pSystem, PROFILE_AI);

				AIActorVector::iterator it = dryUpdates.begin();
				AIActorVector::iterator end = dryUpdates.end();

				for (; it != end; ++it)
					SingleDryUpdate(*it);
			}

			{
				gAIEnv.pTargetTrackManager->ShareFreshestTargetData();
			}

			{
				FRAME_PROFILER("AIUpdate 4 - Collision Avoidance", gEnv->pSystem, PROFILE_AI);

				if (gAIEnv.CVars.EnableORCA)
					UpdateCollisionAvoidance(allUpdates, frameDeltaTime);
			}

			{
				FRAME_PROFILER("AIUpdate 4 - Proxy Updates", gEnv->pSystem, PROFILE_AI);

				{
					{
						AIActorVector::iterator fit = fullUpdates.begin();
						AIActorVector::iterator fend = fullUpdates.end();

						for (; fit != fend; ++fit)
							(*fit)->UpdateProxy(AIUPDATE_FULL);
					}

					{
						AIActorVector::iterator dit = dryUpdates.begin();
						AIActorVector::iterator dend = dryUpdates.end();

						for (; dit != dend; ++dit)
							(*dit)->UpdateProxy(AIUPDATE_DRY);
					}
				}
			}

			gAIEnv.pStatsManager->SetStat(eStat_FullUpdates, static_cast<float>(fullUpdateCount));

			RemoveNonActors(m_disabledAIActorsSet);
		}
		else
		{
			// No active puppets, allow updating smart objects
			m_bUpdateSmartObjects = true;
		}

		// Update disabled

		RemoveNonActors(m_disabledAIActorsSet);

		if (!m_disabledAIActorsSet.empty())
		{
			uint32 inactiveAIActorCount = m_disabledAIActorsSet.size();
			const float updateInterval = 0.3f;
			const float updatesPerSecond = (inactiveAIActorCount / updateInterval) + m_disabledActorsUpdateError;
			unsigned aiActorDisabledUpdateCount = (unsigned)floorf(updatesPerSecond * m_frameDeltaTime);
			if (m_frameDeltaTime > 0.0f)
				m_disabledActorsUpdateError = updatesPerSecond - aiActorDisabledUpdateCount / m_frameDeltaTime;

			m_disabledActorsHead %= inactiveAIActorCount;
			uint32 idx = m_disabledActorsHead;

			for (unsigned i = 0; (i < aiActorDisabledUpdateCount) && inactiveAIActorCount; ++i)
			{
				// [AlexMcC|29.06.10] We can't trust that this is an AI Actor, because CWeakRef::GetAIObject()
				// doesn't do any type checking. If this weakref becomes invalid, then the id is used by another
				// object (which can happen if we chainload or load a savegame), this might not be an actor anymore.
				IAIObject* object = m_disabledAIActorsSet[idx++ % inactiveAIActorCount].GetAIObject();
				CAIActor* actor = object ? object->CastToCAIActor() : NULL;
				AIAssert(actor);

				actor->UpdateDisabled(AIUPDATE_FULL);

				// [AlexMcC|28.09.09] UpdateDisabled might remove the puppet from the disabled set, so the size might change
				inactiveAIActorCount = m_disabledAIActorsSet.size();
			}

			// Advance update head.
			m_disabledActorsHead += aiActorDisabledUpdateCount;
		}

		AIAssert(m_iteratingActorSet);
		m_iteratingActorSet = false;

		//
		//	update all leaders here (should be not every update (full update only))
		const static float leaderUpdateRate(.2f);
		static float leaderNoUpdatedTime(.0f);
		leaderNoUpdatedTime += m_frameDeltaTime;
		if (leaderNoUpdatedTime > leaderUpdateRate)
		{
			leaderNoUpdatedTime = 0.0f;
			AIObjectOwners::iterator aio = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_LEADER);

			for (; aio != gAIEnv.pAIObjectManager->m_Objects.end(); ++aio)
			{
				if (aio->first != AIOBJECT_LEADER)
					break;

				CLeader* pLeader = aio->second.GetAIObject()->CastToCLeader();
				if (pLeader)
					pLeader->Update(AIUPDATE_FULL);
			}
		}
		// leaders update over
		if (m_bUpdateSmartObjects)
			m_pSmartObjectManager->Update();

		//	fLastUpdateTime = currTime;
		++m_nTickCount;

#ifdef CRYAISYSTEM_DEBUG
		UpdateDebugStuff();
#endif //CRYAISYSTEM_DEBUG

		// Update interest system
		ICentralInterestManager* pInterestManager = CCentralInterestManager::GetInstance();
		if (pInterestManager->Enable(gAIEnv.CVars.InterestSystem != 0))
		{
			pInterestManager->Update(frameDeltaTime);
		}
	}

	gAIEnv.pBehaviorTreeManager->Update();

	{
		{
			FRAME_PROFILER("GlobalRayCaster", gEnv->pSystem, PROFILE_AI);

			gAIEnv.pRayCaster->SetQuota(gAIEnv.CVars.RayCasterQuota);
			gAIEnv.pRayCaster->Update(frameDeltaTime);
		}

		{
			FRAME_PROFILER("GlobalIntersectionTester", gEnv->pSystem, PROFILE_AI);

			gAIEnv.pIntersectionTester->SetQuota(gAIEnv.CVars.IntersectionTesterQuota);
			gAIEnv.pIntersectionTester->Update(frameDeltaTime);
		}

		{
			FRAME_PROFILER("ClusterDetector", gEnv->pSystem, PROFILE_AI);

			gAIEnv.pClusterDetector->Update(frameDeltaTime);
		}

#ifdef CRYAISYSTEM_DEBUG
		{
			FRAME_PROFILER("AIBubblesSystem", gEnv->pSystem, PROFILE_AI);

			gAIEnv.pBubblesSystem->Update();
		}
#endif

		if (gAIEnv.CVars.DebugDrawPhysicsAccess)
		{
			SAIEnvironment::GlobalRayCaster::ContentionStats rstats = gAIEnv.pRayCaster->GetContentionStats();
			stack_string text;

			text.Format(
			  "RayCaster\n"
			  "---------\n"
			  "Quota: %u\n"
			  "Queue Size: %u / %u\n"
			  "Immediate Count: %u / %u\n"
			  "Immediate Average: %.1f\n"
			  "Deferred Count: %u / %u\n"
			  "Deferred Average: %.1f",
			  rstats.quota,
			  rstats.queueSize,
			  rstats.peakQueueSize,
			  rstats.immediateCount,
			  rstats.peakImmediateCount,
			  rstats.immediateAverage,
			  rstats.deferredCount,
			  rstats.peakDeferredCount,
			  rstats.deferredAverage);

			bool warning = (rstats.immediateCount + rstats.deferredCount) > rstats.quota;
			warning = warning || (rstats.immediateAverage + rstats.deferredAverage) > rstats.quota;
			warning = warning || rstats.queueSize > (3 * rstats.quota);

			CDebugDrawContext dc;
			dc->Draw2dLabel((float)dc->GetWidth() * 0.1f, (float)dc->GetHeight() * 0.25f, 1.25f, warning ? Col_Red : Col_DarkTurquoise, false, "%s", text.c_str());

			SAIEnvironment::GlobalIntersectionTester::ContentionStats istats = gAIEnv.pIntersectionTester->GetContentionStats();
			text.Format(
			  "IntersectionTester\n"
			  "------------------\n"
			  "Quota: %u\n"
			  "Queue Size: %u / %u\n"
			  "Immediate Count: %u / %u\n"
			  "Immediate Average: %.1f\n"
			  "Deferred Count: %u / %u\n"
			  "Deferred Average: %.1f",
			  istats.quota,
			  istats.queueSize,
			  istats.peakQueueSize,
			  istats.immediateCount,
			  istats.peakImmediateCount,
			  istats.immediateAverage,
			  istats.deferredCount,
			  istats.peakDeferredCount,
			  istats.deferredAverage);

			warning = (istats.immediateCount + istats.deferredCount) > istats.quota;
			warning = warning || (istats.immediateAverage + istats.deferredAverage) > istats.quota;
			warning = warning || istats.queueSize > (3 * istats.quota);

			dc->Draw2dLabel(600.0, 745.0f, 1.25f, warning ? Col_Red : Col_DarkTurquoise, false, "%s", text.c_str());
		}
		/*
		    static Vec3 LastFrom(0.0f, 0.0f, 0.0f);
		    static Vec3 LastTo(0.0f, 0.0f, 0.0f);

		    IAIObject* fromObject = gAIEnv.pAIObjectManager->GetAIObjectByName("CheckWalkabilityFrom");
		    IAIObject* toObject = gAIEnv.pAIObjectManager->GetAIObjectByName("CheckWalkabilityTo");

		    if (fromObject && toObject)
		    {
		      if (((LastFrom-fromObject->GetPos()).len2() > 0.001f) || ((LastTo-toObject->GetPos()).len2() > 0.001f))
		      {
		        AccurateStopTimer fast;
		        AccurateStopTimer normal;

		        {
		          ScopedAutoTimer __fast(fast);
		          CheckWalkabilityFast(fromObject->GetPos(), toObject->GetPos(), 0.4, AICE_ALL);
		        }

		        {
		          ScopedAutoTimer __normal(normal);
		          ListPositions b;
		          CheckWalkability(SWalkPosition(fromObject->GetPos()), SWalkPosition(toObject->GetPos()), 0.15, false,
		            b, AICE_ALL);
		        }

		        LastFrom = fromObject->GetPos();
		        LastTo = toObject->GetPos();

		        if (fast.GetTime() < normal.GetTime())
		          CryLogAlways("$3Fast: %.5f  // Normal: %.5f", fast.GetTime().GetMilliSeconds(), normal.GetTime().GetMilliSeconds());
		        else
		          CryLogAlways("$4Fast: %.5f  // Normal: %.5f", fast.GetTime().GetMilliSeconds(), normal.GetTime().GetMilliSeconds());
		      }
		    }
		 */
	}

	// Monitor code coverage
	int nCodeCoverageMode = gAIEnv.CVars.CodeCoverage;

#if !defined(_RELEASE)
	// First, try to load context if required
	CCodeCoverageManager* pCCManager = gAIEnv.pCodeCoverageManager;
	if (nCodeCoverageMode && !m_bCodeCoverageFailed && !pCCManager->IsContextValid())
	{
		stack_string filePath = PathUtil::Make(m_sWorkingFolder, sCodeCoverageContextFile);
		FILE* ccFile = gEnv->pCryPak->FOpen(filePath.c_str(), "r");
		bool bOk = false;
		if (ccFile)
		{
			bOk = pCCManager->ReadCodeCoverageContext(ccFile);
			gEnv->pCryPak->FClose(ccFile);

			if (bOk)
				AILogAlways("CodeCoverageManager: Successfully loaded code coverage context file \"%s\"", filePath.c_str());
			else
				AILogAlways("CodeCoverageManager: Failed during read of code coverage context file \"%s\"", filePath.c_str());
		}
		else
		{
			AILogAlways("CodeCoverageManager: Failed to find code coverage context file \"%s\"", filePath.c_str());
		}

		m_bCodeCoverageFailed = !bOk;
	}

	if (gAIEnv.pCodeCoverageManager && gAIEnv.pCodeCoverageManager->IsContextValid())
		gAIEnv.pCodeCoverageManager->Update();

	if (nCodeCoverageMode > 0)
		gAIEnv.pCodeCoverageGUI->Update(frameStartTime, frameDeltaTime);
#endif

	// Update asynchronous TPS processing
	float fMaxTime = gAIEnv.CVars.TacticalPointUpdateTime;
	gAIEnv.pTacticalPointSystem->Update(fMaxTime);

	// Housekeeping
	gAIEnv.pObjectContainer->ReleaseDeregisteredObjects(false);
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateBeacon(unsigned short nGroupID, const Vec3& vPos, IAIObject* pOwner)
{
	Vec3 pos = vPos;
	ray_hit hit;

	//FIXME: this should never happen!
	if (!pOwner)
	{
		AIAssert(false);
		return;
	}

	BeaconMap::iterator bi;
	bi = m_mapBeacons.find(nGroupID);

	if (bi == m_mapBeacons.end())
	{
		CCCPOINT(CAISystem_UpdateBeacon_New)

		BeaconStruct bs;
		char name[32];
		cry_sprintf(name, "BEACON_%u", nGroupID);
		CStrongRef<CAIObject> refBeacon;
		gAIEnv.pAIObjectManager->CreateDummyObject(refBeacon, name);
		CAIObject* pObject = refBeacon.GetAIObject();

		pObject->SetPos(pos);
		pObject->SetMoveDir(pOwner->GetMoveDir());
		pObject->SetBodyDir(pOwner->GetBodyDir());
		pObject->SetEntityDir(pOwner->GetEntityDir());
		pObject->SetViewDir(pOwner->GetViewDir());
		pObject->SetType(AIOBJECT_WAYPOINT);
		pObject->SetSubType(IAIObject::STP_BEACON);
		pObject->SetGroupId(nGroupID);

		bs.refBeacon = refBeacon;
		m_mapBeacons.insert(BeaconMap::iterator::value_type(nGroupID, bs));
	}
	else
	{
		CCCPOINT(CAISystem_UpdateBeacon_Reuse)
		// (MATT) It's possible that the owner is now outdated, but we reset that anyway. {2009/02/16}

		// beacon found, update its position
		  (bi->second).refBeacon->SetPos(pos);
		(bi->second).refBeacon->SetBodyDir(pOwner->GetBodyDir());
		(bi->second).refBeacon->SetEntityDir(pOwner->GetEntityDir());
		(bi->second).refBeacon->SetMoveDir(pOwner->GetMoveDir());
		(bi->second).refBeacon->SetViewDir(pOwner->GetViewDir());
		(bi->second).refOwner.Reset();
	}
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetBeacon(unsigned short nGroupID)
{
	BeaconMap::iterator bi;
	bi = m_mapBeacons.find(nGroupID);
	if (bi == m_mapBeacons.end())
		return NULL;

	CCCPOINT(CAISystem_GetBeacon)

	return (bi->second).refBeacon.GetAIObject();
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetBeaconGroupId(CAIObject* pBeacon)
{
	BeaconMap::iterator bi, biEnd = m_mapBeacons.end();

	for (bi = m_mapBeacons.begin(); bi != biEnd; ++bi)
		if (bi->second.refBeacon == pBeacon)
		{
			CCCPOINT(CAISystem_GetBeaconGroupId);

			return bi->first;
		}
	return -1;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SetAssesmentMultiplier(unsigned short type, float fMultiplier)
{
	if (fMultiplier <= 0.f)
		fMultiplier = 0.01f;

	MapMultipliers::iterator mi;

	mi = m_mapMultipliers.find(type);
	if (mi == m_mapMultipliers.end())
		m_mapMultipliers.insert(MapMultipliers::iterator::value_type(type, fMultiplier));
	else
		mi->second = fMultiplier;

	if (std::find(m_priorityObjectTypes.begin(), m_priorityObjectTypes.end(), type) == m_priorityObjectTypes.end())
		m_priorityObjectTypes.push_back(type);
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetNearestToObjectInRange(IAIObject* pRef, unsigned short nType, float fRadiusMin,
                                                float fRadiusMax, float inCone, bool bFaceAttTarget, bool bSeesAttTarget, bool bDevalue)
{
	AIObjectOwners::iterator ai;
	ai = gAIEnv.pAIObjectManager->m_Objects.find(nType);

	float fRadiusMinSQR = fRadiusMin * fRadiusMin;
	float fRadiusMaxSQR = fRadiusMax * fRadiusMax;

	if (ai == gAIEnv.pAIObjectManager->m_Objects.end())
		return NULL;

	CAIActor* pAIActor = pRef->CastToCAIActor();
	CAIObject* pAttTarget = pAIActor ? (CAIObject*)pAIActor->GetAttentionTarget() : 0;

	IAIObject* pRet = 0;
	CPuppet* pPuppet = pRef->CastToCPuppet();
	float eyeOffset = 0.f;
	if ((pAIActor != NULL) && bSeesAttTarget)
	{
		SAIBodyInfo bi;
		pAIActor->GetProxy()->QueryBodyInfo(SAIBodyInfoQuery(STANCE_CROUCH, 0.0f, 0.0f, true), bi);
		eyeOffset = bi.vEyePos.z - pAIActor->GetEntity()->GetWorldPos().z;
	}
	float maxdist = std::numeric_limits<float>::max();
	Vec3 pos = pRef->GetPos();
	for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		CAIObject* const pObject = ai->second.GetAIObject();
		if (ai->first != nType)
			break;
		if (!pObject || !pObject->IsEnabled())
			continue;
		Vec3 ob_pos = pObject->GetPos();
		Vec3 ob_dir = ob_pos - pos;
		float f = ob_dir.GetLengthSquared();

		// only consider objects in cone
		if (inCone > 0.0f)
		{
			ob_dir.Normalize();
			float dot = pRef->GetMoveDir().Dot(ob_dir);
			if (dot < inCone)
				continue;
			// Favor points which have tighter hide spot.
			float a = 0.5f + 0.5f * (1.0f - max(0.0f, dot));
			f *= a * a;
		}

		if (bFaceAttTarget && pAttTarget)
		{
			Vec3 dirToAtt = pAttTarget->GetPos() - ob_pos;
			Vec3 coverDir = pObject->GetMoveDir();
			dirToAtt.NormalizeSafe();
			if (dirToAtt.Dot(coverDir) < 0.3f)    // let's make it 70 degree threshold
				continue;
		}

		// (MATT) Crysis surely depends on this but it's not a nice behaviour {2008/11/11}
		if (pPuppet && (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS || gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2))
		{
			if (pPuppet->IsDevalued(pObject))
				continue;
		}

		//use anchor radius ---------------------
		float fCurRadiusMaxSQR((ai->second)->GetRadius() < 0.01f ? fRadiusMaxSQR : (fRadiusMax + (ai->second)->GetRadius()) * (fRadiusMax + (ai->second)->GetRadius()));
		if (f > fRadiusMinSQR && f < fCurRadiusMaxSQR)
		{
			if ((f < maxdist))
			{
				if (bSeesAttTarget && pAttTarget)
				{
					ray_hit hit;
					float terrainZ = gEnv->p3DEngine->GetTerrainElevation(ob_pos.x, ob_pos.y);
					Vec3 startTracePos(ob_pos.x, ob_pos.y, terrainZ + eyeOffset);
					Vec3 ob_at_dir = pAttTarget->GetPos() - startTracePos;

					PhysSkipList skipList;
					pAIActor->GetPhysicalSkipEntities(skipList);
					if (pAttTarget->CastToCAIActor())
						pAttTarget->CastToCAIActor()->GetPhysicalSkipEntities(skipList);

					if (gAIEnv.pWorld->RayWorldIntersection(startTracePos, ob_at_dir, ent_static | ent_terrain | ent_sleeping_rigid,
					                                        rwi_stop_at_pierceable, &hit, 1, &skipList[0], skipList.size()))
					{
						continue;
					}
				}
				pRet = pObject;
				maxdist = f;
			}
		}
	}
	if (pRet)
	{
		if (!bDevalue)
			Devalue(pRef, pRet, true, .05f);    // no devalue - just make sure it's not used in the same update again
		else
			Devalue(pRef, pRet, true);
	}
	return pRet;
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetRandomObjectInRange(IAIObject* pRef, unsigned short nType, float fRadiusMin, float fRadiusMax, bool bFaceAttTarget)
{
	// Make sure there is at least one object of type present.
	AIObjectOwners::iterator ai;
	ai = gAIEnv.pAIObjectManager->m_Objects.find(nType);
	if (ai == gAIEnv.pAIObjectManager->m_Objects.end())
		return NULL;

	CAIActor* pAIActor = pRef->CastToCAIActor();
	IAIObject* pAttTarget = (bFaceAttTarget && pAIActor) ? pAIActor->GetAttentionTarget() : 0;

	// Collect all the points within the given range.
	const float fRadiusMinSQR = fRadiusMin * fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax * fRadiusMax;
	std::vector<IAIObject*> lstObjectsInRange;
	Vec3 pos = pRef->GetPos();
	CPuppet* pPuppet = pRef->CastToCPuppet();
	for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		// Skip objects of wrong type.
		if (ai->first != nType)
			break;

		// Strong so always valid
		CAIObject* pObject = ai->second.GetAIObject();

		// Skip disable objects.
		if (!pObject->IsEnabled())
			continue;

		// Skip devalued objects.
		// (MATT) Crysis surely depends on this but it's not a nice behaviour {2008/11/11}
		if ((pPuppet != NULL) && (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS || gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2))
		{
			if (pPuppet->IsDevalued(pObject))
				continue;
		}

		// check if facing target
		if (pAttTarget)
		{
			Vec3 candidate2Target(pAttTarget->GetPos() - pObject->GetPos());
			float dotS(pObject->GetMoveDir() * candidate2Target);
			if (dotS < 0.f)
				continue;
		}
		// Skip objects out of range.
		Vec3 ob_pos = pObject->GetPos();
		Vec3 ob_dir = ob_pos - pos;
		float f = ob_dir.GetLengthSquared();
		//use anchor radius ---------------------
		float fCurRadiusMaxSQR(pObject->GetRadius() < 0.01f ? fRadiusMaxSQR : (fRadiusMax + pObject->GetRadius()) * (fRadiusMax + pObject->GetRadius()));
		if (f > fRadiusMinSQR && f < fCurRadiusMaxSQR)
			lstObjectsInRange.push_back(pObject);
	}

	// Choose random object.
	IAIObject* pRet = 0;
	if (!lstObjectsInRange.empty())
	{
		int choice = cry_random(0, (int)lstObjectsInRange.size() - 1);
		std::vector<IAIObject*>::iterator randIt = lstObjectsInRange.begin();
		std::advance(randIt, choice);
		if (randIt != lstObjectsInRange.end())
			pRet = (*randIt);
	}

	if (pRet && (pPuppet != NULL))
		pPuppet->Devalue(pRet, false, 2.f);

	return pRet;
}

//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetBehindObjectInRange(IAIObject* pRef, unsigned short nType, float fRadiusMin, float fRadiusMax)
{
	// Find an Object to escape from a target. 04/11/05 tetsuji

	// Make sure there is at least one object of type present.
	AIObjectOwners::iterator ai;
	ai = gAIEnv.pAIObjectManager->m_Objects.find(nType);
	if (ai == gAIEnv.pAIObjectManager->m_Objects.end())
		return NULL;

	CAIActor* pAIActor = pRef->CastToCAIActor();
	assert(pAIActor);
	PREFAST_ASSUME(pAIActor);

	CAIObject* pAttTarget = pAIActor ? (CAIObject*)pAIActor->GetAttentionTarget() : 0;

	// Collect all the points within the given range.
	const float fRadiusMinSQR = fRadiusMin * fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax * fRadiusMax;
	std::vector<IAIObject*> lstObjectsInRange;

	Vec3 pos = pRef->GetPos();
	Vec3 vForward = pRef->GetMoveDir();

	vForward.SetLength(10.0);

	// If no att target, I assume the target is the front point of the object.
	// 20/12/05 Tetsuji
	Vec3 vTargetPos = pAttTarget ? pAttTarget->GetPos() : pos + vForward;
	Vec3 vTargetToPos = pos - vTargetPos;

	if (!pAIActor->GetMovementAbility().b3DMove)
		vTargetToPos.z = 0.f;
	vTargetToPos.NormalizeSafe();

	for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		// Skip objects of wrong type.
		if (ai->first != nType)
			break;

		// Strong so always valid
		CAIObject* pObject = ai->second.GetAIObject();

		// Skip disable objects.
		if (!pObject->IsEnabled())
			continue;

		// Skip devalued objects.
		// (MATT) Crysis surely depends on this but it's not a nice behaviour {2008/11/11}
		CPuppet* pPuppet = pRef->CastToCPuppet();
		if (pPuppet && (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS || gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2))
		{
			if (pPuppet->IsDevalued(pObject))
				continue;
		}

		// Skip objects out of range.
		Vec3 ob_pos = pObject->GetPos();
		Vec3 ob_dir = ob_pos - pos;
		float f = ob_dir.GetLengthSquared();
		//use anchor radius ---------------------
		float fCurRadiusMaxSQR(pObject->GetRadius() < 0.01f ? fRadiusMaxSQR : (fRadiusMax + pObject->GetRadius()) * (fRadiusMax + (pObject->GetRadius())));
		if (f > fRadiusMinSQR && f < fCurRadiusMaxSQR)
			lstObjectsInRange.push_back(pObject);
	}

	// Choose object.
	IAIObject* pRet = 0;
	float maxdot = -10.0f;

	if (!lstObjectsInRange.empty())
	{
		std::vector<IAIObject*>::iterator choiceIt = lstObjectsInRange.begin();
		std::vector<IAIObject*>::iterator choiceItEnd = lstObjectsInRange.end();
		for (; choiceIt != choiceItEnd; ++choiceIt)
		{
			IAIObject* pObj = *choiceIt;
			Vec3 vPosToObj = pObj->GetPos() - pos;
			CPuppet* pPuppet = pRef->CastToCPuppet();
			if ((pPuppet != NULL) && !pPuppet->GetMovementAbility().b3DMove)
				vPosToObj.z = 0.f;

			if ((pPuppet != NULL) && !pPuppet->CheckFriendsInLineOfFire(vPosToObj, true))
				continue;

			float dot = vPosToObj.Dot(vTargetToPos);
			if (dot < .2f)
				continue;
			if (dot > maxdot)
			{
				maxdot = dot;
				pRet = pObj;
			}
		}
	}

	if (pRet)
		Devalue(pRef, pRet, true);

	return pRet;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Devalue(IAIObject* pRef, IAIObject* pObject, bool group, float fDevalueTime)
{
	if (!pRef || !pObject)
		return;

	CPipeUser* pPipeUser = pRef->CastToCPipeUser();
	if (!pPipeUser)
		return;

	if (group)
	{
		// Devalue the object for whole group.
		int myGroup = pPipeUser->GetGroupId();
		AIObjects::iterator gri = m_mapGroups.find(myGroup);
		for (; gri != m_mapGroups.end(); ++gri)
		{
			if (gri->first != myGroup)
				break;
			CPuppet* pGroupMember = CastToCPuppetSafe(gri->second.GetAIObject());
			if (pGroupMember)
				pGroupMember->Devalue((CAIObject*)pObject, false, fDevalueTime);
		}
	}
	else
	{
		// Devalue the object for one puppet
		if (CPuppet* pPuppet = pRef->CastToCPuppet())
			pPuppet->Devalue((CAIObject*)pObject, false, fDevalueTime);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetNearestObjectOfTypeInRange(const Vec3& pos, unsigned int nTypeID, float fRadiusMin, float fRadiusMax, IAIObject* pSkip, int nOption)
{
	IAIObject* pRet = 0;
	float mindist = 100000000;
	const float fRadiusMinSQR = fRadiusMin * fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax * fRadiusMax;

	AIObjectOwners::iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(nTypeID), aiEnd = gAIEnv.pAIObjectManager->m_Objects.end();
	for (; ai != aiEnd && ai->first == nTypeID; ++ai)
	{
		CAIObject* object = ai->second.GetAIObject();
		if (pSkip == object || !(nOption & AIFAF_INCLUDE_DISABLED) && !object->IsEnabled())
			continue;

		float f = (object->GetPos() - pos).GetLengthSquared();
		//use anchor radius ---------------------
		float fCurRadiusMaxSQR(object->GetRadius() < 0.01f ? fRadiusMaxSQR : (fRadiusMax + object->GetRadius()) * (fRadiusMax + object->GetRadius()));
		if (f < mindist && f > fRadiusMinSQR && f < fCurRadiusMaxSQR)
		{
			pRet = object;
			mindist = f;
		}
	}

	return pRet;
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetNearestObjectOfTypeInRange(IAIObject* pRequester, unsigned int nTypeID, float fRadiusMin, float fRadiusMax, int nOption)
{
	CCCPOINT(GetNearestObjectOfTypeInRange);

	// (MATT) This method seems to try to work with non-puppets, but it's fishy {2008/11/12}
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);
	if (pPuppet == NULL) AIWarning("GetNearestObjectOfTypeInRange passed a non-puppet");

	Vec3 reqPos = pRequester->GetPos();
	if (nOption & AIFAF_USE_REFPOINT_POS)
	{
		if ((pPuppet == NULL) || !pPuppet->GetRefPoint())
			return 0;
		reqPos = pPuppet->GetRefPoint()->GetPos();
	}

	IAIObject* pRet = 0;
	float mindist = 100000000;
	const float fRadiusMinSQR = fRadiusMin * fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax * fRadiusMax;

	AIObjectOwners::iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(nTypeID), aiEnd = gAIEnv.pAIObjectManager->m_Objects.end();
	for (; ai != aiEnd && ai->first == nTypeID; ++ai)
	{
		CAIObject* pObj = ai->second.GetAIObject();
		if (pObj == pRequester || !(nOption & AIFAF_INCLUDE_DISABLED) && !pObj->IsEnabled())
			continue;
		if (nOption & AIFAF_SAME_GROUP_ID)
		{
			CAIActor* pActor = pObj->CastToCAIActor();
			if ((pActor != NULL) && pActor->GetGroupId() != pPuppet->GetGroupId())
				continue;
		}

		const Vec3& objPos = pObj->GetPos();
		float f = (objPos - reqPos).GetLengthSquared();
		if (f < mindist && f > fRadiusMinSQR && f < fRadiusMaxSQR)
		{
			// (MATT) Crysis surely depends on this but it's not a nice behaviour {2008/11/11}
			if ((gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS || gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2) ||
			    (nOption & AIFAF_INCLUDE_DEVALUED) ||
			    (pPuppet && !pPuppet->IsDevalued(pObj)))
			{
				if (nOption & AIFAF_HAS_FORMATION && !pObj->m_pFormation)
				{
					continue;
				}

				if (nOption & AIFAF_VISIBLE_FROM_REQUESTER)
				{
					ray_hit rh;
					int colliders(0);
					colliders = gAIEnv.pWorld->RayWorldIntersection(
					  objPos, reqPos - objPos, COVER_OBJECT_TYPES,
					  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
					  &rh, 1);
					if (colliders)
						continue;
				}
				if (nOption & (AIFAF_LEFT_FROM_REFPOINT | AIFAF_RIGHT_FROM_REFPOINT))
				{
					if (pPuppet && pPuppet->GetRefPoint())
					{
						const Vec3 one = pPuppet->GetRefPoint()->GetPos() - reqPos;
						const Vec3 two = objPos - reqPos;

						float zcross = one.x * two.y - one.y * two.x;

						if (nOption & AIFAF_LEFT_FROM_REFPOINT)
						{
							if (zcross < 0)
								continue;
						}
						else
						{
							if (zcross > 0)
								continue;
						}
					}
				}

				if (nOption & AIFAF_INFRONT_OF_REQUESTER)
				{
					const Vec3 toTargetDir((objPos - reqPos).GetNormalizedSafe());
					float diffCosine(toTargetDir * pPuppet->m_State.vMoveDir);

					if (diffCosine < .7f)
						continue;
				}

				pRet = pObj;
				mindist = f;
			}
		}
	}

	if (pRet && !(nOption & AIFAF_DONT_DEVALUE))
		Devalue(pRequester, pRet, true);

	return pRet;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road)
{
	if (m_pNavigation->DoesNavigationShapeExists(szName, areaType, road))
		return true;

	if (areaType == AREATYPE_OCCLUSION_PLANE)
	{
		return m_mapOcclusionPlanes.find(szName) != m_mapOcclusionPlanes.end();
	}
	else if (areaType == AREATYPE_GENERIC)
	{
		return m_mapGenericShapes.find(szName) != m_mapGenericShapes.end();
	}

	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::CreateNavigationShape(const SNavigationShapeParams& params)
{
	// need at least one point in a path. Some paths need more than one (areas need 3)
	if (params.nPoints == 0)
		return true; // Do not report too few points as errors.

	// TODO Jan 31, 2008: <pvl> 'true' returned here can mean either area type
	// wasn't matched (in which case we should go on matching here) or that
	// there a shape was actually created already (meaning we should return
	// immediately).  In the latter case we have a bit of inefficiency here.
	if (m_pNavigation->CreateNavigationShape(params) == false)
		return false;

	std::vector<Vec3> vecPts(params.points, params.points + params.nPoints);

	if (params.areaType == AREATYPE_PATH && params.pathIsRoad == false)
	{
		//designer path need to preserve directions
	}
	else
	{
		if (params.closed)
			EnsureShapeIsWoundAnticlockwise<std::vector<Vec3>, float>(vecPts);
	}

	ListPositions listPts(vecPts.begin(), vecPts.end());

	if (params.areaType == AREATYPE_OCCLUSION_PLANE)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapOcclusionPlanes.find(params.szPathName) != m_mapOcclusionPlanes.end())
		{
			AIError("CAISystem::CreateNavigationShape: Occlusion plane '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		m_mapOcclusionPlanes.insert(ShapeMap::iterator::value_type(params.szPathName, SShape(listPts)));
	}
	else if (params.areaType == AREATYPE_PERCEPTION_MODIFIER)
	{
		if (listPts.size() < 2)
			return false;

		PerceptionModifierShapeMap::iterator di;
		di = m_mapPerceptionModifiers.find(params.szPathName);

		if (di == m_mapPerceptionModifiers.end())
		{
			SPerceptionModifierShape pms(listPts, params.fReductionPerMetre, params.fReductionMax, params.fHeight, params.closed);
			m_mapPerceptionModifiers.insert(PerceptionModifierShapeMap::iterator::value_type(params.szPathName, pms));
		}
		else
			return false;
	}
	else if (params.areaType == AREATYPE_GENERIC)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapGenericShapes.find(params.szPathName) != m_mapGenericShapes.end())
		{
			AIError("CAISystem::CreateNavigationShape: Shape '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		m_mapGenericShapes.insert(ShapeMap::iterator::value_type(params.szPathName,
		                                                         SShape(listPts, false, IAISystem::NAV_UNSET, params.nAuxType, params.closed, params.fHeight, params.lightLevel)));
	}

	return true;
}

// deletes designer created path
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DeleteNavigationShape(const char* szName)
{
	ShapeMap::iterator di;

	m_pNavigation->DeleteNavigationShape(szName);

	di = m_mapGenericShapes.find(szName);
	if (di != m_mapGenericShapes.end())
	{
		m_mapGenericShapes.erase(di);

		// Make sure there is no dangling pointers left.
		DetachFromTerritoryAllAIObjectsOfType(szName, AIOBJECT_ACTOR);
		DetachFromTerritoryAllAIObjectsOfType(szName, AIOBJECT_VEHICLE);
	}

	di = m_mapOcclusionPlanes.find(szName);
	if (di != m_mapOcclusionPlanes.end())
		m_mapOcclusionPlanes.erase(di);

	PerceptionModifierShapeMap::iterator pmsi = m_mapPerceptionModifiers.find(szName);
	if (pmsi != m_mapPerceptionModifiers.end())
		m_mapPerceptionModifiers.erase(pmsi);

	// Light manager might have pointers to shapes and areas, update it.
	m_lightManager.Update(true);
}

//====================================================================
// GetEnclosingGenericShapeOfType
//====================================================================
const char* CAISystem::GetEnclosingGenericShapeOfType(const Vec3& reqPos, int type, bool checkHeight)
{
	ShapeMap::iterator end = m_mapGenericShapes.end();
	ShapeMap::iterator nearest = end;
	float nearestDist = FLT_MAX;
	for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != end; ++it)
	{
		const SShape& shape = it->second;
		if (!shape.enabled) continue;
		if (shape.type != type) continue;
		if (shape.IsPointInsideShape(reqPos, (type == AIANCHOR_COMBAT_TERRITORY) ? shape.height >= 0.01f : checkHeight))
		{
			float dist = 0;
			Vec3 pt;
			shape.NearestPointOnPath(reqPos, false, dist, pt);
			if (dist < nearestDist)
			{
				nearest = it;
				nearestDist = dist;
			}
		}
	}

	if (nearest != end)
		return nearest->first.c_str();

	return 0;
}

//====================================================================
// DistanceToGenericShape
//====================================================================
float CAISystem::DistanceToGenericShape(const Vec3& reqPos, const char* shapeName, bool checkHeight)
{
	if (!shapeName || shapeName[0] == 0)
		return 0.0f;

	ShapeMap::iterator it = m_mapGenericShapes.find(shapeName);
	if (it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::DistanceToGenericShape Unable to find generic shape called %s", shapeName);
		return 0.0f;
	}
	const SShape& shape = it->second;
	float dist;
	Vec3 nearestPt;
	shape.NearestPointOnPath(reqPos, false, dist, nearestPt);
	if (checkHeight)
		return dist;
	return Distance::Point_Point2D(reqPos, nearestPt);
}

//====================================================================
// IsPointInsideGenericShape
//====================================================================
bool CAISystem::IsPointInsideGenericShape(const Vec3& reqPos, const char* shapeName, bool checkHeight)
{
	if (!shapeName || shapeName[0] == 0)
		return false;

	ShapeMap::iterator it = m_mapGenericShapes.find(shapeName);
	if (it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::IsPointInsideGenericShape Unable to find generic shape called %s", shapeName);
		return false;
	}
	const SShape& shape = it->second;
	return shape.IsPointInsideShape(reqPos, checkHeight);
}

//====================================================================
// ConstrainInsideGenericShape
//====================================================================
bool CAISystem::ConstrainInsideGenericShape(Vec3& pos, const char* shapeName, bool checkHeight)
{
	if (!shapeName || shapeName[0] == 0)
		return false;

	ShapeMap::iterator it = m_mapGenericShapes.find(shapeName);
	if (it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::ConstrainInsideGenericShape Unable to find generic shape called %s", shapeName);
		return false;
	}
	const SShape& shape = it->second;
	return shape.ConstrainPointInsideShape(pos, checkHeight);
}

//====================================================================
// GetGenericShapeOfName
//====================================================================
SShape* CAISystem::GetGenericShapeOfName(const char* shapeName)
{
	if (!shapeName || shapeName[0] == 0)
		return 0;

	ShapeMap::iterator it = m_mapGenericShapes.find(shapeName);
	if (it == m_mapGenericShapes.end())
		return 0;
	return &it->second;
}

//====================================================================
// CreateTemporaryGenericShape
//====================================================================
const char* CAISystem::CreateTemporaryGenericShape(Vec3* points, int npts, float height, int type)
{
	if (npts < 2)
		return 0;

	// Make sure the shape is wound clockwise.
	std::vector<Vec3> vecPts(points, points + npts);
	EnsureShapeIsWoundAnticlockwise<std::vector<Vec3>, float>(vecPts);
	ListPositions listPts(vecPts.begin(), vecPts.end());

	// Create random name for the shape.
	char name[16];
	ShapeMap::iterator di;
	do
	{
		cry_sprintf(name, "Temp%08x", cry_random(0U, (uint) ~0));
		di = m_mapGenericShapes.find(name);
	}
	while (di != m_mapGenericShapes.end());

	std::pair<ShapeMap::iterator, bool> pr;

	pr = m_mapGenericShapes.insert(ShapeMap::iterator::value_type(name, SShape(listPts, false, IAISystem::NAV_UNSET, type, true, height, AILL_NONE, true)));
	if (pr.second == true)
		return (pr.first)->first.c_str();
	return 0;
}

//====================================================================
// EnableGenericShape
//====================================================================
void CAISystem::EnableGenericShape(const char* shapeName, bool state)
{
	if (!shapeName || strlen(shapeName) < 1)
		return;
	ShapeMap::iterator it = m_mapGenericShapes.find(shapeName);
	if (it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::EnableGenericShape Unable to find generic shape called %s", shapeName);
		return;
	}

	SShape& shape = it->second;

	// If the new state is the same, no need to inform the users.
	if (shape.enabled == state)
		return;

	// Change the state of the shape
	shape.enabled = state;

	// Notify the puppets that are using the shape that the shape state has changed.
	AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_ACTOR);
	for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_ACTOR)
			break;

		CAIObject* obj = ai->second.GetAIObject();
		CPuppet* puppet = obj->CastToCPuppet();
		if (!puppet)
			continue;

		if (state)
		{
			// Shape enabled
			if (shape.IsPointInsideShape(puppet->GetPos(), true))
			{
				IAISignalExtraData* pData = CreateSignalExtraData();
				pData->SetObjectName(shapeName);
				pData->iValue = shape.type;
				puppet->SetSignal(1, "OnShapeEnabled", puppet->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnShapeEnabled);
			}
		}
		else
		{
			// Shape disabled
			int val = 0;
			if (puppet->GetRefShapeName() && strcmp(puppet->GetRefShapeName(), shapeName) == 0)
				val |= 1;
			if (puppet->GetTerritoryShapeName() && strcmp(puppet->GetTerritoryShapeName(), shapeName) == 0)
				val |= 2;
			if (val)
			{
				IAISignalExtraData* pData = CreateSignalExtraData();
				pData->iValue = val;
				puppet->SetSignal(1, "OnShapeDisabled", puppet->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnShapeDisabled);
			}
		}
	}
}

void CAISystem::FlushAllAreas()
{
	// Flushes all non-navigation areas
	m_mapGenericShapes.clear();
	m_mapPerceptionModifiers.clear();
}

//
// Offsets all areas when segmented world shifts.
void CAISystem::OffsetAllAreas(const Vec3& additionalOffset)
{
	for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
		it->second.OffsetShape(additionalOffset);
	for (PerceptionModifierShapeMap::iterator it = m_mapPerceptionModifiers.begin(); it != m_mapPerceptionModifiers.end(); ++it)
		it->second.OffsetShape(additionalOffset);

	if (m_pNavigation)
		m_pNavigation->OffsetAllAreas(additionalOffset);
}

//====================================================================
// GetOccupiedHideObjectPositions
//====================================================================
void CAISystem::GetOccupiedHideObjectPositions(const CPipeUser* pRequester, std::vector<Vec3>& hideObjectPositions)
{
	CCCPOINT(GetOccupiedHideObjectPositions);

	// Iterate over the list of active puppets and collect positions of the valid hidespots in use.
	hideObjectPositions.clear();

	AIActorSet::const_iterator it = m_enabledAIActorsSet.begin();
	AIActorSet::const_iterator end = m_enabledAIActorsSet.end();

	for (; it != end; ++it)
	{
		CPipeUser* pOther = it->GetAIObject()->CastToCPipeUser();
		if (!pOther || (pRequester && (pRequester == pOther)))
			continue;

		// Include the position of each enemy regardless of the validity of the hidepoint itself - seems to get better results
		hideObjectPositions.push_back(pOther->GetPos());

		if (!pOther->m_CurrentHideObject.IsValid())
			continue;

		// One reason why it can be important to also add the hidepoint itself is because the AI may not have reached it yet
		hideObjectPositions.push_back(pOther->m_CurrentHideObject.GetObjectPos());
	}
}

// Mrcio: Seriously, we need to get some kind of ID system for HideSpots.
// God kills a kitten each time he looks at this code.
bool CAISystem::IsHideSpotOccupied(CPipeUser* pRequester, const Vec3& pos) const
{
	AIActorSet::const_iterator it = m_enabledAIActorsSet.begin();
	AIActorSet::const_iterator end = m_enabledAIActorsSet.end();

	for (; it != end; ++it)
	{
		CPipeUser* pOther = it->GetAIObject()->CastToCPipeUser();
		if (!pOther || (pRequester && (pRequester == pOther)))
			continue;

		if ((pOther->GetPos() - pos).len2() < 0.5 * 0.5f)
			return true;

		if (!pOther->m_CurrentHideObject.IsValid())
			continue;

		if ((pOther->m_CurrentHideObject.GetObjectPos() - pos).len2() < 0.5 * 0.5f)
			return true;
	}

	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::NotifyEnableState(CAIActor* pAIActor, bool state)
{
	// (MATT)
	// The puppet (AI Actor, as of 25.04.2010 - evgeny) should, ideally, be both erased and then inserted
	// In many cases the method is called when the state is unchanged and so the puppet is neither erased or inserted
	// On creation it will not be found for erase, but should be added anyway - should really be handled separately
	// If this happens later, it may be a bug - e.g. re-adding during puppet Release()
	// {2008/11/10}

	assert(pAIActor);
	CWeakRef<CAIActor> refAIActor = GetWeakRef(pAIActor);

	gAIEnv.pWalkabilityCacheManager->EnableActor(pAIActor->GetAIObjectID(), state);

	if (state)
	{
		gAIEnv.pActorLookUp->AddActor(pAIActor);

		bool bErased = m_disabledAIActorsSet.erase(refAIActor) > 0;
		bool bInserted = m_enabledAIActorsSet.insert(refAIActor).second;

		if (bErased != bInserted)
		{
			if (bInserted)
				AILogComment("AI Actor %p %s added to enable list", pAIActor, pAIActor->GetName());
			else
				AIWarning("AI Actor %p %s removed from disable, but already present in enable list",
				          pAIActor, pAIActor->GetName());
		}
		AILogComment("AI Actor %p %s moved from disabled to enabled set A/B:1/1 %d %d",
		             pAIActor, pAIActor->GetName(), bErased, bInserted);
	}
	else
	{
		gAIEnv.pSequenceManager->AgentDisabled(pAIActor->GetEntityID());

		gAIEnv.pActorLookUp->RemoveActor(pAIActor);

		pAIActor->ClearProbableTargets();

		bool bErased = m_enabledAIActorsSet.erase(refAIActor) > 0;
		bool bInserted = m_disabledAIActorsSet.insert(refAIActor).second;

		if (bErased != bInserted)
		{
			if (bInserted)
				AILogComment("AI Actor %p %s added to disable list", pAIActor, pAIActor->GetName());
			else
				AIWarning("AI Actor %p %s removed from enable, but already present in disable list",
				          pAIActor, pAIActor->GetName());
		}
		AILogComment("AI Actor %p %s moved from enabled to disabled set A/B:1/1 %d %d",
		             pAIActor, pAIActor->GetName(), bErased, bInserted);
	}
}

//TODO: find better solution - not to send a signal from there but notify AIProxy - make AIHAndler send the signal, same way OnEnemySeen works
//
// sand signal to all objects of type nType, which have the pDeadObject as attention targer
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::NotifyTargetDead(IAIObject* pDeadObject)
{
	if (!pDeadObject)
		return;

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	lookUp.Prepare(ActorLookUp::Proxy);

	size_t activeActorCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
	{
		CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);

		if (pAIActor->GetAttentionTarget() == pDeadObject)
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->SetObjectName(pDeadObject->GetName());
			pData->nID = pDeadObject->GetEntityID();
			pData->string1 = gAIEnv.pFactionMap->GetFactionName(pDeadObject->GetFactionID());
			pAIActor->SetSignal(0, "OnTargetDead", pAIActor->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnTargetDead);
		}
	}

	// Check if the death of the actor was part of any player stunt.
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
	CAIActor* pDeadActor = pDeadObject->CastToCAIActor();
	if ((pPlayer != NULL) && (pDeadActor != NULL))
	{
		if (pPlayer->IsPlayerStuntAffectingTheDeathOf(pDeadActor))
		{
			// Skip the nearest thrown entity, since it is potentially blocking the view to the corpse.
			EntityId nearestThrownEntId = pPlayer->GetNearestThrownEntity(pDeadActor->GetPos());
			IEntity* pNearestThrownEnt = nearestThrownEntId ? gEnv->pEntitySystem->GetEntity(nearestThrownEntId) : 0;
			IPhysicalEntity* pNearestThrownEntPhys = pNearestThrownEnt ? pNearestThrownEnt->GetPhysics() : 0;

			short gid = (short)pDeadObject->GetGroupId();
			AIObjects::iterator aiIt = m_mapGroups.find(gid);
			AIObjects::iterator endIt = m_mapGroups.end();

			for (; aiIt != endIt && aiIt->first == gid; ++aiIt)
			{
				CPuppet* pPuppet = CastToCPuppetSafe(aiIt->second.GetAIObject());
				if (!pPuppet)
					continue;

				if (pPuppet->GetEntityID() == pDeadActor->GetEntityID())
					continue;

				float dist = FLT_MAX;
				if (!CheckVisibilityToBody(pPuppet, pDeadActor, dist, pNearestThrownEntPhys))
					continue;

				pPuppet->SetSignal(1, "OnGroupMemberMutilated", pDeadObject->GetEntity(), 0);
			}
		}
	}

}

std::shared_ptr<IPathFollower> CAISystem::CreateAndReturnNewDefaultPathFollower(const PathFollowerParams& params, const IPathObstacles& pathObstacleObject)
{
	return IPathFollowerPtr(new CSmartPathFollower(params, pathObstacleObject));
}

//===================================================================
// ExitNodeImpossible
//===================================================================
bool CAISystem::ExitNodeImpossible(CGraphLinkManager& linkManager, const GraphNode* pNode, float fRadius) const
{
	int16 radiusInCm = NavGraphUtils::InCentimeters(fRadius);
	for (unsigned gl = pNode->firstLinkIndex; gl; gl = linkManager.GetNextLink(gl))
	{
		if (linkManager.GetRadiusInCm(gl) >= radiusInCm)
			return false;
	}
	return true;
}

//===================================================================
// EnterNodeImpossible
//===================================================================
bool CAISystem::EnterNodeImpossible(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, float fRadius) const
{
	int16 radiusInCm = NavGraphUtils::InCentimeters(fRadius);
	for (unsigned link = pNode->firstLinkIndex; link; link = linkManager.GetNextLink(link))
	{
		unsigned nextNodeIndex = linkManager.GetNextNode(link);
		GraphNode* nextNode = nodeManager.GetNode(nextNodeIndex);
		unsigned incomingLink = nextNode->GetLinkTo(nodeManager, linkManager, pNode);
		if (incomingLink && linkManager.GetRadiusInCm(incomingLink) >= radiusInCm)
			return false;
	}
	return true;
}

//===================================================================
// SetFactionThreatMultiplier
//===================================================================
void CAISystem::SetFactionThreatMultiplier(uint8 factionID, float fMultiplier)
{
	if (fMultiplier > 1.f)
		fMultiplier = 1.f;

	if (fMultiplier < 0.f) // Modified from <= 0 -> = 0.01
		fMultiplier = 0.0f;

	// will use this multiplier any time a puppet perceives a target of these species
	MapMultipliers::iterator mi;

	mi = m_mapFactionThreatMultipliers.find(factionID);
	if (mi == m_mapFactionThreatMultipliers.end())
		m_mapFactionThreatMultipliers.insert(MapMultipliers::iterator::value_type(factionID, fMultiplier));
	else
		mi->second = fMultiplier;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DumpStateOf(IAIObject* pObject)
{
	AILogAlways("AIName: %s", pObject->GetName());
	CPuppet* pPuppet = CastToCPuppetSafe(pObject);
	if (pPuppet)
	{
		CGoalPipe* pPipe = pPuppet->GetCurrentGoalPipe();
		if (pPipe)
		{
			AILogAlways("Current pipes: %s", pPipe->GetName());
			while (pPipe->IsInSubpipe())
			{
				pPipe = pPipe->GetSubpipe();
				AILogAlways("   subpipe: %s", pPipe->GetName());
			}
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
float CAISystem::GetRayPerceptionModifier(const Vec3& start, const Vec3& end, const char* actorName)
{
	const Lineseg ray(start, end);
	const float rayLen((end - start).len());
	const int max_intersects = 8;
	float intersects[max_intersects];
	float fPerception = 1.0f;
	Vec3 hit;

	int icvDrawPerceptionDebugging = gAIEnv.CVars.DrawPerceptionDebugging;

	PerceptionModifierShapeMap::iterator pmsi = m_mapPerceptionModifiers.begin(), pmsiEnd = m_mapPerceptionModifiers.end();
	for (; pmsi != pmsiEnd; ++pmsi)
	{
		SPerceptionModifierShape& shape = pmsi->second;
		if (shape.shape.empty()) continue;

		// Quick bounding box check, skip if no intersection and both points aren't inside AABB
		if (Intersect::Lineseg_AABB(ray, shape.aabb, hit) == 0x00 &&
		    !(shape.aabb.IsContainPoint(start) && shape.aabb.IsContainPoint(end))) continue;

#ifdef CRYAISYSTEM_DEBUG
		char text[64];
#endif //CRYAISYSTEM_DEBUG

		// Proper test
		bool bStartInsideShape(shape.IsPointInsideShape(start, true));
		bool bEndInsideShape(shape.IsPointInsideShape(end, true));
		if (bStartInsideShape && bEndInsideShape)
		{
			float fMod = rayLen * shape.fReductionPerMetre;
			fMod = clamp_tpl(fMod, 0.0f, shape.fReductionMax);
			fPerception -= fMod;

#ifdef CRYAISYSTEM_DEBUG
			if (icvDrawPerceptionDebugging != 0)
			{
				cry_sprintf(text, "%s-0", actorName ? actorName : "");
				AddPerceptionDebugLine(text, start + Vec3(0.f, 0.f, -0.1f), end + Vec3(0.f, 0.f, -0.1f), 50, 255, 50, 1.f, 3.f);
			}
#endif //CRYAISYSTEM_DEBUG

		}
		else
		{
			int nIntersects = shape.GetIntersectionDistances(start, end, intersects, max_intersects, true, true);
			if (nIntersects > 0)
			{
				// Special handling for open shapes
				if (!shape.closed)
				{
					fPerception -= shape.fReductionMax * nIntersects;
					if (fPerception < 0.0f)
						fPerception = 0.0f;

#ifdef CRYAISYSTEM_DEBUG
					if (icvDrawPerceptionDebugging != 0)
					{
						Vec3 intersectionPos;
						for (int i = 0; i < nIntersects; ++i)
						{
							cry_sprintf(text, "%s-0-%d", actorName ? actorName : "", i + 1);
							intersectionPos = start + (intersects[i] / rayLen) * (end - start) + Vec3(0.f, 0.f, -0.1f);
							AddPerceptionDebugLine(text,
							                       Vec3(intersectionPos.x, intersectionPos.y, shape.aabb.min.z),
							                       Vec3(intersectionPos.x, intersectionPos.y, shape.aabb.max.z),
							                       50, 255, 50, 1.f, 3.f);
						}
					}
#endif    //CRYAISYSTEM_DEBUG

					continue;
				}

				float fMod(0.f);
				int in, out;
				float inPos, outPos;
				if (bStartInsideShape)
				{
					in = -1;
					inPos = 0.f;
					out = 0;
					outPos = intersects[0];
				}
				else if (nIntersects > 1)
				{
					in = 0;
					inPos = intersects[0];
					out = 1;
					outPos = intersects[1];
				}
				else
				{
					out = nIntersects;
				}

				while (out < nIntersects)
				{
					fMod = (outPos - inPos) * shape.fReductionPerMetre;
					fMod = clamp_tpl(fMod, 0.0f, shape.fReductionMax);
					fPerception -= fMod;

#ifdef CRYAISYSTEM_DEBUG
					if (icvDrawPerceptionDebugging != 0)
					{
						Vec3 startPos(start + (inPos / rayLen) * (end - start) + Vec3(0.f, 0.f, -0.1f));
						Vec3 endPos(start + (outPos / rayLen) * (end - start) + Vec3(0.f, 0.f, -0.1f));
						cry_sprintf(text, "%s-%d-1", actorName ? actorName : "", out);
						AddPerceptionDebugLine(text, startPos, endPos, 50, 255, 50, 1.f, 3.f);
						cry_sprintf(text, "%s-%d-2", actorName ? actorName : "", out);
						AddPerceptionDebugLine(text,
						                       Vec3(startPos.x, startPos.y, shape.aabb.min.z),
						                       Vec3(startPos.x, startPos.y, shape.aabb.max.z),
						                       50, 255, 50, 1.f, 3.f);
						cry_sprintf(text, "%s-%d-3", actorName ? actorName : "", out);
						AddPerceptionDebugLine(text,
						                       Vec3(endPos.x, endPos.y, shape.aabb.min.z),
						                       Vec3(endPos.x, endPos.y, shape.aabb.max.z),
						                       50, 255, 50, 1.f, 3.f);
					}
#endif    //CRYAISYSTEM_DEBUG

					in += 2;
					inPos = intersects[in];
					out += 2;
					outPos = intersects[out];
				}

				if (bEndInsideShape)
				{
					fMod = (rayLen - intersects[nIntersects - 1]) * shape.fReductionPerMetre;
					fMod = clamp_tpl(fMod, 0.0f, shape.fReductionMax);
					fPerception -= fMod;

#ifdef CRYAISYSTEM_DEBUG
					if (icvDrawPerceptionDebugging != 0)
					{
						cry_sprintf(text, "%s-%d-1", actorName ? actorName : "", nIntersects);
						Vec3 startPos(start + (intersects[nIntersects - 1] / rayLen) * (end - start) + Vec3(0.f, 0.f, -0.1f));
						AddPerceptionDebugLine(text, startPos, end + Vec3(0.f, 0.f, -0.1f), 50, 255, 50, 1.f, 3.f);

						cry_sprintf(text, "%s-%d-2", actorName ? actorName : "", nIntersects);
						AddPerceptionDebugLine(text,
						                       Vec3(startPos.x, startPos.y, shape.aabb.min.z),
						                       Vec3(startPos.x, startPos.y, shape.aabb.max.z),
						                       50, 255, 50, 1.f, 3.f);
					}
#endif    //CRYAISYSTEM_DEBUG

				}
			}
		}
	}

	return fPerception;
}

IAIObjectIter* CAISystem::GetFirstAIObjectInShape(EGetFirstFilter filter, short n, const char* shapeName, bool checkHeight)
{
	ShapeMap::iterator it = m_mapGenericShapes.find(shapeName);

	// Return dummy iterator.
	if (it == m_mapGenericShapes.end())
		return SAIObjectMapIter<CCountedRef>::Allocate(gAIEnv.pAIObjectManager->m_Objects.end(), gAIEnv.pAIObjectManager->m_Objects.end());

	SShape& shape = it->second;

	if (filter == OBJFILTER_GROUP)
	{
		return SAIObjectMapIterOfTypeInShape<CWeakRef>::Allocate(n, m_mapGroups.find(n), m_mapGroups.end(), shape, checkHeight);
	}
	else if (filter == OBJFILTER_FACTION)
	{
		return SAIObjectMapIterOfTypeInShape<CWeakRef>::Allocate(n, m_mapFaction.find(n), m_mapFaction.end(), shape, checkHeight);
	}
	else if (filter == OBJFILTER_DUMMYOBJECTS)
	{
		return SAIObjectMapIterOfTypeInShape<CWeakRef>::Allocate(n, gAIEnv.pAIObjectManager->m_mapDummyObjects.find(n), gAIEnv.pAIObjectManager->m_mapDummyObjects.end(), shape, checkHeight);
	}
	else
	{
		if (n == 0)
			return SAIObjectMapIterInShape<CCountedRef>::Allocate(gAIEnv.pAIObjectManager->m_Objects.begin(), gAIEnv.pAIObjectManager->m_Objects.end(), shape, checkHeight);
		else
			return SAIObjectMapIterOfTypeInShape<CCountedRef>::Allocate(n, gAIEnv.pAIObjectManager->m_Objects.find(n), gAIEnv.pAIObjectManager->m_Objects.end(), shape, checkHeight);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Event(int event, const char* name)
{
	CCCPOINT(CAISystem_Event);

	switch (event)
	{
	case AISYSEVENT_DISABLEMODIFIER:
		{
			CCCPOINT(CAISystem_Event_DM);
			m_pNavigation->DisableModifier(name);
			break;
		}
	default:
		break;
	}
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::CreateFormationDescriptor(const char* name)
{
	FormationDescriptorMap::iterator itD = m_mapFormationDescriptors.find(name);
	if (itD != m_mapFormationDescriptors.end())
		m_mapFormationDescriptors.erase(itD);
	CFormationDescriptor fdesc;
	fdesc.m_sName = name;
	fdesc.m_nNameCRC32 = CCrc32::Compute(name);
	m_mapFormationDescriptors.insert(FormationDescriptorMap::iterator::value_type(fdesc.m_sName, fdesc));
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::EnumerateFormationNames(unsigned int maxNames, const char** names, unsigned int* nameCount) const
{
	CRY_ASSERT(names);
	CRY_ASSERT(nameCount);

	*nameCount = 0;

	FormationDescriptorMap::const_iterator it = m_mapFormationDescriptors.begin();
	FormationDescriptorMap::const_iterator end = m_mapFormationDescriptors.end();

	for (; it != end; ++it)
	{
		if (*nameCount == maxNames)
		{
			gEnv->pLog->LogError("CAISystem::EnumerateFormationNames: Can't fit more formation names. Number of names = %" PRISIZE_T ", maximum number of names = %u.", m_mapFormationDescriptors.size(), maxNames);
			return;
		}

		names[(*nameCount)++] = it->first.c_str();
	}
}

//-----------------------------------------------------------------------------------------------------------
bool CAISystem::IsFormationDescriptorExistent(const char* name)
{
	return m_mapFormationDescriptors.find(name) != m_mapFormationDescriptors.end();
}
//-----------------------------------------------------------------------------------------------------------

void CAISystem::AddFormationPoint(const char* name, const FormationNode& nodeDescriptor)
{
	FormationDescriptorMap::iterator fdit = m_mapFormationDescriptors.find(name);
	if (fdit != m_mapFormationDescriptors.end())
	{
		fdit->second.AddNode(nodeDescriptor);
	}
}

//-----------------------------------------------------------------------------------------------------------

IAIObject* CAISystem::GetLeaderAIObject(int iGroupID)
{
	CLeader* pLeader = GetLeader(iGroupID);
	return pLeader ? pLeader->GetAssociation().GetAIObject() : NULL;
}

IAIObject* CAISystem::GetLeaderAIObject(IAIObject* pObject)
{
	CAIActor* pActor = pObject->CastToCAIActor();
	CLeader* pLeader = GetLeader(pActor);
	return pLeader ? pLeader->GetAssociation().GetAIObject() : NULL;
}

IAIObject* CAISystem::GetFormationPoint(IAIObject* pObject)
{
	CAIActor* pActor = pObject->CastToCAIActor();
	CLeader* pLeader = GetLeader(pActor);
	if (pLeader)
	{
		CCCPOINT(CAISystem_GetFormationPoint);
		return pLeader->GetFormationPoint(GetWeakRef((CAIObject*)pObject)).GetAIObject();
	}
	return NULL;
}

int CAISystem::GetFormationPointClass(const char* descriptorName, int position)
{
	FormationDescriptorMap::iterator di;
	di = m_mapFormationDescriptors.find(descriptorName);
	if (di != m_mapFormationDescriptors.end())
	{
		return di->second.GetNodeClass(position);
	}
	return -1;
}

//====================================================================
// Warning
//====================================================================
void CAISystem::Warning(const char* id, const char* format, ...) const
{
#ifdef CRYAISYSTEM_VERBOSITY
	if (!AIGetWarningErrorsEnabled() || !AICheckLogVerbosity(AI_LOG_WARNING))
		return;

	char outputBuffer[MAX_WARNING_LENGTH + AILogMaxIdLen];  // extra for the id/prefix
	const size_t idLen = std::min(strlen(id), AILogMaxIdLen);
	memcpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AIWarning("%s", outputBuffer);
#endif
}

//====================================================================
// Error
//====================================================================
void CAISystem::Error(const char* id, const char* format, ...)
{
#ifdef CRYAISYSTEM_VERBOSITY
	if (!AIGetWarningErrorsEnabled() || !AICheckLogVerbosity(AI_LOG_ERROR))
		return;

	char outputBuffer[MAX_WARNING_LENGTH + AILogMaxIdLen]; // extra for the id/prefix
	const size_t idLen = std::min(strlen(id), AILogMaxIdLen);
	memcpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AIError("%s", outputBuffer);
#endif
}

//====================================================================
// LogProgress
//====================================================================
void CAISystem::LogProgress(const char* id, const char* format, ...)
{
#ifdef CRYAISYSTEM_VERBOSITY
	if (!AICheckLogVerbosity(AI_LOG_PROGRESS))
		return;

	char outputBuffer[MAX_WARNING_LENGTH + AILogMaxIdLen]; // extra for the id/prefix
	const size_t idLen = std::min(strlen(id), AILogMaxIdLen);
	memcpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AILogProgress("%s", outputBuffer);
#endif
}

//====================================================================
// LogEvent
//====================================================================
void CAISystem::LogEvent(const char* id, const char* format, ...)
{
#ifdef CRYAISYSTEM_VERBOSITY
	if (!AICheckLogVerbosity(AI_LOG_EVENT))
		return;

	char outputBuffer[MAX_WARNING_LENGTH + AILogMaxIdLen]; // extra for the id/prefix
	const size_t idLen = std::min(strlen(id), AILogMaxIdLen);
	memcpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AILogEvent("%s", outputBuffer);
#endif
}

//====================================================================
// LogComment
//====================================================================
void CAISystem::LogComment(const char* id, const char* format, ...)
{
#ifdef CRYAISYSTEM_VERBOSITY
	if (!AICheckLogVerbosity(AI_LOG_COMMENT))
		return;

	char outputBuffer[MAX_WARNING_LENGTH + AILogMaxIdLen]; // extra for the id/prefix
	const size_t idLen = std::min(strlen(id), AILogMaxIdLen);
	memcpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AILogComment("%s", outputBuffer);
#endif
}

// (MATT) Handy {2009/03/25}
//bool bNeedsToBeCreated(entityID==0 && type!=AIOBJECT_LEADER);

//====================================================================
// DebugOutputObjects
//====================================================================
void CAISystem::DebugOutputObjects(const char* txt) const
{
#ifdef CRYAISYSTEM_VERBOSITY
	// early out if log verbosity is so low that we won't get any output
	if (!AICheckLogVerbosity(AI_LOG_COMMENT))
		return;

	AILogComment("================= DebugOutputObjects: %s =================", txt);

	AILogComment("gAIEnv.pAIObjectManager->m_Objects");
	bool bNoEntityWarning = (AIGetLogConsoleVerbosity() >= 3 || AIGetLogFileVerbosity() >= 3);

	for (AIObjectOwners::const_iterator it = gAIEnv.pAIObjectManager->m_Objects.begin(); it != gAIEnv.pAIObjectManager->m_Objects.end(); ++it)
	{
		unsigned short type = it->first;
		const CAIObject* pObject = it->second.GetAIObject();
		if (!pObject)
			continue;

		const char* name = pObject->GetName();
		unsigned entID = pObject->GetEntityID();
		unsigned short ID = pObject->GetAIObjectID();

		AILogComment("ID %d, entID = %d, type = %d, ptr = %p, name = %s", ID, entID, type, pObject, name);

		if (bNoEntityWarning && entID > 0 && !gEnv->pEntitySystem->GetEntity(pObject->GetEntityID())
		    && pObject->GetType() != AIOBJECT_LEADER)
			AIWarning("No entity for AIObject %s", pObject->GetName());
	}

	AILogComment("================= DebugOutputObjects End =================");
#endif
}

void CAISystem::UnregisterAIActor(CWeakRef<CAIActor> destroyedObject)
{
	CRY_ASSERT_TRACE(!m_iteratingActorSet, ("Removing AIActor %i from enabled/disabled set while iterating over the set (fix this by adding double buffering, or nulling instead of erasing)"));

	m_enabledAIActorsSet.erase(destroyedObject);
	m_disabledAIActorsSet.erase(destroyedObject);
}

//====================================================================
// SerializeObjectIDs
//====================================================================
void CAISystem::SerializeObjectIDs(TSerialize ser)
{
	gAIEnv.pObjectContainer->SerializeObjectIDs(ser);
}

//====================================================================
// Serialize
//====================================================================
void CAISystem::Serialize(TSerialize ser)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "AI serialization");

	DebugOutputObjects("Start of serialize");

	if (ser.IsReading())
	{
		gAIEnv.pWalkabilityCacheManager->Reset();

		if (m_pAIActionManager)
			m_pAIActionManager->Reset();
	}
	// reregister smart objects with navigation before serializing anything
	// so that code can safely query the completed navgraph
	m_pSmartObjectManager->Serialize(ser);

	if (ser.IsReading())
	{
		m_mapBeacons.clear();
	}

	m_PipeManager.Serialize(ser);

	// start serialising
	ser.BeginGroup("AISystemStatus");
	{
		// at this point there may be more objects+dummies than when saved because some entity objects (e.g. CPipeUser
		// always contain an internal non-entity object - when they sync up the numbers should match.

		// It makes sense to clear the root objects, which should deregister all objects, before loading new ones in via objectContainer
		// Without it, it was crashing trying to deregister old objects as it read in old ones, in CCountedRef
		// That's because the standard container serialisation code clears it first, triggering deletes, even if the serialisation methods themselves don't.
		//
		// However: pooled AI objects are not removed (hence the false passed to Reset) as they have recently been created by the entity pool manager serialization.
		if (ser.IsReading())
		{
			gAIEnv.pAIObjectManager->Reset(false);
		}

		ser.BeginGroup("AIObjectContainer");
		gAIEnv.pObjectContainer->Serialize(ser);
		ser.EndGroup();

		if (gAIEnv.CVars.ForceSerializeAllObjects)
		{
			ser.Value("ObjectOwners", gAIEnv.pAIObjectManager->m_Objects);
			ser.Value("MapDummyObjects", gAIEnv.pAIObjectManager->m_mapDummyObjects);
		}
		else
		{
			if (ser.IsReading())
			{
				gAIEnv.pObjectContainer->RebuildObjectMaps(gAIEnv.pAIObjectManager->m_Objects, gAIEnv.pAIObjectManager->m_mapDummyObjects);
			}
		}

		ser.Value("MapGroups", m_mapGroups);
		ser.Value("MapFaction", m_mapFaction);
		ser.Value("EnabledPuppetsSet", m_enabledAIActorsSet);
		ser.Value("DisabledPuppetsSet", m_disabledAIActorsSet);

		SerializeInternal(ser);

		ser.Value("m_lastAmbientFireUpdateTime", m_lastAmbientFireUpdateTime);
		ser.Value("m_lastExpensiveAccessoryUpdateTime", m_lastExpensiveAccessoryUpdateTime);
		ser.Value("m_lastVisBroadPhaseTime", m_lastVisBroadPhaseTime);
		ser.Value("m_lastGroupUpdateTime", m_lastGroupUpdateTime);
	}
	ser.EndGroup();

	if (m_pAIActionManager)
		m_pAIActionManager->Serialize(ser);

	if (gAIEnv.pTargetTrackManager)
		gAIEnv.pTargetTrackManager->Serialize(ser);

	if (gAIEnv.pTacticalPointSystem)
		gAIEnv.pTacticalPointSystem->Serialize(ser);

	CCentralInterestManager::GetInstance()->Serialize(ser);

	gAIEnv.pPerceptionManager->Serialize(ser);
	if (ser.IsReading())
		gAIEnv.pFactionMap->Reload();
	gAIEnv.pFactionMap->Serialize(ser);

	m_globalPerceptionScale.Serialize(ser);

	//serialize AI lua globals
	IScriptSystem* pSS = gEnv->pScriptSystem;
	if (pSS)
	{
		SmartScriptTable pScriptAI;
		if (pSS->GetGlobalValue("AI", pScriptAI))
			if (pScriptAI->HaveValue("OnSave") && pScriptAI->HaveValue("OnLoad"))
			{
				ser.BeginGroup("ScriptBindAI");
				SmartScriptTable persistTable(pSS);
				if (ser.IsWriting())
					Script::CallMethod(pScriptAI, "OnSave", persistTable);
				ser.Value("ScriptData", persistTable.GetPtr());
				if (ser.IsReading())
					Script::CallMethod(pScriptAI, "OnLoad", persistTable);
				ser.EndGroup();
			}
	}

	// Call post serialize on systems that need it -
	//  All objects should now be in place
	if (ser.IsReading())
	{
		gAIEnv.pObjectContainer->PostSerialize();
	}

	if (ser.IsReading())
		gAIEnv.pSequenceManager->Reset();
}

//===================================================================
// Serialize
//===================================================================
void PathfindRequest::Serialize(TSerialize ser)
{
	ser.BeginGroup("PathFindRequest");
	ser.Value("startPos", startPos);
	ser.Value("startDir", startDir);
	ser.Value("endPos", endPos);
	ser.Value("endDir", endDir);
	ser.Value("bSuccess", bSuccess);

	// (MATT) Locally convert to weak ref just here - this may or may not work in this case. {2009/02/16}
	CWeakRef<CAIActor> refRequester;
	if (ser.IsReading())
	{
		refRequester.Serialize(ser, "refRequester");
		pRequester = refRequester.GetAIObject(); // Just use the IAIPathAgent
	}
	else if (IEntity* requesterEntity = pRequester->GetPathAgentEntity())
	{
		if (IAIObject* requesterAIObject = requesterEntity->GetAI())
			refRequester = GetWeakRef(CastToCAIActorSafe(requesterAIObject));
		refRequester.Serialize(ser, "refRequester");
	}

	ser.Value("bSuccess", bSuccess);
	ser.Value("nForceTargetBuildingId", nForceTargetBuildingId);
	ser.Value("allowDangerousDestination", allowDangerousDestination);
	ser.Value("endTol", endTol);
	ser.Value("endDistance", endDistance);
	ser.Value("isDirectional", isDirectional);
	ser.Value("bPathEndIsAsRequested", bPathEndIsAsRequested);
	ser.Value("id", id);
	ser.Value("navCapMask", navCapMask);
	AIAssert(navCapMask != IAISystem::NAV_UNSET);

	ser.Value("passRadius", passRadius);
	ser.EnumValue("type", type, TYPE_ACTOR, TYPE_RAW);

	if (ser.IsReading())
	{
		startIndex = 0;
		endIndex = 0;
	}
	ser.EndGroup();
}

//====================================================================
// Serialize
//====================================================================
void CAISystem::SerializeInternal(TSerialize ser)
{
	// WIP Notes:
	// Leaders seem special. Can't serialise them like the other gAIEnv.pAIObjectManager->m_Objects.
	// m_mapAIGroups might well be interfering with other group list

	ser.BeginGroup("CAISystem");
	{
		ser.BeginGroup("Beacons");
		{
			int count = m_mapBeacons.size();
			ser.Value("count", count);
			char name[32];

			if (ser.IsWriting())
			{
				int i = 0;
				for (BeaconMap::iterator it = m_mapBeacons.begin(); it != m_mapBeacons.end(); ++it, ++i)
				{
					cry_sprintf(name, "beacon-%d", i);
					ser.BeginGroup(name);
					{
						unsigned short id = it->first;
						BeaconStruct& bs = it->second;

						ser.Value("id", id);
						bs.refBeacon.Serialize(ser, "refBeacon");
						bs.refOwner.Serialize(ser, "refOwner");
					}
					ser.EndGroup();
				}
			}
			else
			{
				m_mapBeacons.clear();
				for (int i = 0; i < count; ++i)
				{
					cry_sprintf(name, "beacon-%d", i);
					ser.BeginGroup(name);
					{
						unsigned short id;
						BeaconStruct bs;
						ser.Value("id", id);
						bs.refBeacon.Serialize(ser, "refBeacon");
						bs.refOwner.Serialize(ser, "refOwner");
						m_mapBeacons[id] = bs;
					}
					ser.EndGroup();
				}
			}
		}
		ser.EndGroup(); //  "Beacons"

		m_lightManager.Serialize(ser);

		// Active formations
		ser.BeginGroup("ActiveFormations");
		{
			int count = m_mapActiveFormations.size();
			ser.Value("numObjects", count);

			if (ser.IsReading())
			{
				FormationMap::iterator iFormation = m_mapActiveFormations.begin(), iEnd = m_mapActiveFormations.end();
				for (; iFormation != iEnd; ++iFormation)
				{
					delete iFormation->second;
				}

				m_mapActiveFormations.clear();
			}

			char formationName[32];
			while (--count >= 0)
			{
				CFormation* pFormationObject(0);
				CWeakRef<CAIObject> refOwner;

				if (ser.IsWriting())
				{
					FormationMap::iterator iFormation = m_mapActiveFormations.begin();
					std::advance(iFormation, count);
					refOwner = iFormation->first;
					pFormationObject = iFormation->second;
				}
				else
				{
					pFormationObject = new CFormation();
				}

				cry_sprintf(formationName, "Formation_%d", count);
				ser.BeginGroup(formationName);
				{
					refOwner.Serialize(ser, "formationOwner");
					pFormationObject->Serialize(ser);
				}
				ser.EndGroup();
				if (ser.IsReading())
				{
					m_mapActiveFormations.insert(FormationMap::iterator::value_type(refOwner, pFormationObject));
				}
			}
		}
		ser.EndGroup(); // "ActiveFormations"

		// Serialize temporary shapes.
		ser.BeginGroup("TempGenericShapes");
		if (ser.IsWriting())
		{
			int nTempShapes = 0;
			for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
				if (it->second.temporary)
					nTempShapes++;

			ser.Value("nTempShapes", nTempShapes);
			for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
			{
				SShape& shape = it->second;
				if (shape.temporary)
				{
					ser.BeginGroup("Shape");
					string name(it->first);
					ser.Value("name", name);
					ser.Value("aabbMin", shape.aabb.min);
					ser.Value("aabbMax", shape.aabb.max);
					ser.Value("type", shape.type);
					ser.Value("shape", shape.shape);
					ser.EndGroup();
				}
			}
		}
		else
		{
			// Remove temporary shapes.
			for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); )
			{
				// Enable
				it->second.enabled = true;
				// Remove temp
				if (it->second.temporary)
					m_mapGenericShapes.erase(it++);
				else
					++it;
			}
			// Create new temp shapes.
			int nTempShapes = 0;
			ser.Value("nTempShapes", nTempShapes);
			for (int i = 0; i < nTempShapes; ++i)
			{
				string name;
				SShape shape;
				ser.BeginGroup("Shape");
				ser.Value("name", name);
				ser.Value("aabbMin", shape.aabb.min);
				ser.Value("aabbMax", shape.aabb.max);
				ser.Value("type", shape.type);
				ser.Value("shape", shape.shape);
				ser.EndGroup();
				shape.temporary = true;
				shape.closed = true;
				m_mapGenericShapes.insert(ShapeMap::iterator::value_type(name, shape));
			}
		}
		ser.EndGroup(); // "TempGenericShapes"

		ser.BeginGroup("DisabledGenericShapes");
		if (ser.IsWriting())
		{
			int nDisabledShapes = 0;
			for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
				if (!it->second.enabled)
					nDisabledShapes++;
			ser.Value("nDisabledShapes", nDisabledShapes);

			for (ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
			{
				if (it->second.enabled) continue;
				ser.BeginGroup("Shape");
				ser.Value("name", it->first);
				ser.EndGroup();
			}
		}
		else
		{
			int nDisabledShapes = 0;
			ser.Value("nDisabledShapes", nDisabledShapes);
			string name;
			for (int i = 0; i < nDisabledShapes; ++i)
			{
				ser.BeginGroup("Shape");
				ser.Value("name", name);
				ser.EndGroup();
				ShapeMap::iterator di = m_mapGenericShapes.find(name);
				if (di != m_mapGenericShapes.end())
					di->second.enabled = false;
				else
					AIWarning("CAISystem::Serialize Unable to find generic shape called %s", name.c_str());
			}
		}
		ser.EndGroup(); // "DisabledGenericShapes"

		m_pNavigation->Serialize(ser);

		ser.Value("m_priorityObjectTypes", m_priorityObjectTypes);

		ser.Value("m_frameStartTime", m_frameStartTime);
		ser.Value("m_frameStartTimeSeconds", m_frameStartTimeSeconds);
		ser.Value("m_frameDeltaTime", m_frameDeltaTime);
		ser.Value("m_fLastPuppetUpdateTime", m_fLastPuppetUpdateTime);
		if (ser.IsReading())
		{
			// Danny: physics doesn't serialise its time (it doesn't really use it) so we can
			// set it here.
			GetISystem()->GetIPhysicalWorld()->SetPhysicsTime(m_frameStartTime.GetSeconds());
		}

		AIObjectOwners::iterator itobjend = gAIEnv.pAIObjectManager->m_Objects.end();

		ser.BeginGroup("AI_Groups");
		{
			//regenerate the whole groups map when reading
			if (ser.IsReading())
			{
				for (AIGroupMap::iterator it = m_mapAIGroups.begin(); !m_mapAIGroups.empty(); )
				{
					delete it->second;
					m_mapAIGroups.erase(it++);
				}

				for (AIObjectOwners::iterator itobj = gAIEnv.pAIObjectManager->m_Objects.begin(); itobj != itobjend; ++itobj)
				{
					CAIObject* pObject = itobj->second.GetAIObject();
					if (pObject->CastToCPuppet() || pObject->CastToCAIPlayer() || pObject->CastToCLeader())
					{
						CAIActor* pActor = pObject->CastToCAIActor();
						int groupid = pActor->GetGroupId();
						if (m_mapAIGroups.find(groupid) == m_mapAIGroups.end())
						{
							CAIGroup* pGroup = new CAIGroup(groupid);
							m_mapAIGroups.insert(std::make_pair(groupid, pGroup));
						}
					}
				}
			}
			for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
			{
				CAIGroup* pGroup = it->second;
				pGroup->Serialize(ser);
				if (ser.IsReading())
				{
					// assign group to leader and viceversa
					int groupid = it->first;
					CLeader* pLeader = NULL;
					bool found = false;

					AIObjectOwners::const_iterator itl = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_LEADER);

					while (!found && itl != itobjend && itl->first == AIOBJECT_LEADER)
					{
						pLeader = (CLeader*)itl->second.GetAIObject();
						found = pLeader && (pLeader->GetGroupId() == groupid);
						++itl;
					}

					if (found && pLeader)
					{
						pGroup->SetLeader(pLeader);
					}
				}
			}
		}
		ser.EndGroup(); // "AI_Groups"

		gAIEnv.pGroupManager->Serialize(ser);

		char name[32];
		ser.BeginGroup("AI_Alertness");
		for (int i = 0; i < 4; i++)
		{
			cry_sprintf(name, "AlertnessCounter%d", i);
			ser.Value(name, m_AlertnessCounters[i]);
		}
		ser.EndGroup();

		ser.Value("m_nTickCount", m_nTickCount);
		ser.Value("m_bUpdateSmartObjects", m_bUpdateSmartObjects);

		ser.Value("m_mapAuxSignalsFired", m_mapAuxSignalsFired);
	}
	ser.EndGroup();
}

// notifies that entity has changed its position, which is important for smart objects
void CAISystem::NotifyAIObjectMoved(IEntity* pEntity, SEntityEvent event)
{
	if (!IsEnabled())
		return;

	AIAssert(m_pSmartObjectManager);
	((IEntitySystemSink*)m_pSmartObjectManager)->OnEvent(pEntity, event);
}

//====================================================================
// RegisterFirecommandHandler
//====================================================================
void CAISystem::RegisterFirecommandHandler(IFireCommandDesc* desc)
{
	for (std::vector<IFireCommandDesc*>::iterator it = m_firecommandDescriptors.begin(); it != m_firecommandDescriptors.end(); ++it)
	{
		if ((*it) == desc)
		{
			desc->Release();
			return;
		}
	}
	m_firecommandDescriptors.push_back(desc);
}

//====================================================================
// CreateFirecommandHandler
//====================================================================
IFireCommandHandler* CAISystem::CreateFirecommandHandler(const char* name, IAIActor* pShooter)
{
	for (std::vector<IFireCommandDesc*>::iterator it = m_firecommandDescriptors.begin(); it != m_firecommandDescriptors.end(); ++it)
	{
		if (stricmp((*it)->GetName(), name) == 0)
			return (*it)->Create(pShooter);
	}
	AIWarning("CAISystem::CreateFirecommandHandler: Could not find firecommand handler '%s'", name);
	return 0;
}

//====================================================================
// CombatClasses - add new
//====================================================================
void CAISystem::AddCombatClass(int combatClass, float* pScalesVector, int size, const char* szCustomSignal)
{
	if (size == 0)
	{
		m_CombatClasses.clear();
		return;
	}

	// Sanity check
	AIAssert(combatClass >= 0 && combatClass < 100);

	if (combatClass >= (int)m_CombatClasses.size())
		m_CombatClasses.resize(combatClass + 1);

	SCombatClassDesc& desc = m_CombatClasses[combatClass];
	if (szCustomSignal && strlen(szCustomSignal) > 1)
		desc.customSignal = szCustomSignal;
	else
		desc.customSignal.clear();

	desc.mods.resize(size);
	for (int i = 0; i < size; ++i)
		desc.mods[i] = pScalesVector[i];
}

//====================================================================
// GetCombatClassScale - retrieves scale for given target/shooter
//====================================================================
float CAISystem::GetCombatClassScale(int shooterClass, int targetClass)
{
	if (targetClass < 0 || shooterClass < 0 || shooterClass >= (int)m_CombatClasses.size())
		return 1.0f;
	SCombatClassDesc& desc = m_CombatClasses[shooterClass];
	if (targetClass >= (int)desc.mods.size())
		return 1.0f;
	return desc.mods[targetClass];
}

//===================================================================
// GetDangerSpots
//===================================================================
unsigned int CAISystem::GetDangerSpots(const IAIObject* requester, float range, Vec3* positions, unsigned int* types,
                                       unsigned int n, unsigned int flags)
{
	float rangeSq = sqr(range);

	const Vec3& reqPos = requester->GetPos();

	const CAIActor* pActor = requester->CastToCAIActor();
	uint8 factionID = pActor->GetFactionID();
	uint32 i = 0;

	if (flags & DANGER_EXPLOSIVE)
	{
		// Collect all grenades and merge close ones.
		std::vector<Vec3> grenades;
		AIObjectOwners::const_iterator aio = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_GRENADE);
		for (; aio != gAIEnv.pAIObjectManager->m_Objects.end(); ++aio)
		{
			// TODO: add other explosives here too (maybe check if the requester knows about them also too)
			if (aio->first != AIOBJECT_GRENADE)
				break;

			// Skip explosives which are too far away.
			const Vec3& pos = aio->second.GetAIObject()->GetPos();
			if (Distance::Point_PointSq(pos, reqPos) > rangeSq)
				continue;

			// Merge with current greanades if possible.
			bool merged = false;
			for (std::vector<Vec3>::iterator it = grenades.begin(); it != grenades.end(); ++it)
			{
				if (Distance::Point_Point((*it), pos) < 3.0f)
				{
					merged = true;
					break;
				}
			}

			// If cannot be merged, add new.
			if (!merged)
				grenades.push_back(pos);
		}

		for (std::vector<Vec3>::iterator it = grenades.begin(); it != grenades.end(); ++it)
		{
			// Limit the number of points to output.
			if (i >= n) break;
			// Output point
			positions[i] = (*it);
			if (types) types[i] = DANGER_EXPLOSIVE;
			i++;
		}
	}

	// Returns number of points.
	return i;
}

//===================================================================
// DynOmniLightEvent
//===================================================================
void CAISystem::DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, EntityId shooterId, float time)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;
	if (!IsEnabled())
		return;

	IEntity* pShooterEnt = gEnv->pEntitySystem->GetEntity(shooterId);
	if (!pShooterEnt) return;
	CAIActor* pActor = CastToCAIActorSafe(pShooterEnt->GetAI());
	if (pActor)
		m_lightManager.DynOmniLightEvent(pos, radius, type, pActor, time);
}

//===================================================================
// DynSpotLightEvent
//===================================================================
void CAISystem::DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, EntityId shooterId, float time)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;
	if (!IsEnabled())
		return;

	IEntity* pShooterEnt = gEnv->pEntitySystem->GetEntity(shooterId);
	if (!pShooterEnt) return;
	CAIActor* pActor = CastToCAIActorSafe(pShooterEnt->GetAI());
	if (pActor)
		m_lightManager.DynSpotLightEvent(pos, dir, radius, fov, type, pActor, time);
}

//===================================================================
// RegisterAIEventListener
//===================================================================
void CAISystem::RegisterAIEventListener(IAIEventListener* pListener, const Vec3& pos, float rad, int flags)
{
	gAIEnv.pPerceptionManager->RegisterAIEventListener(pListener, pos, rad, flags);
}

//===================================================================
// UnregisterAIEventListener
//===================================================================
void CAISystem::UnregisterAIEventListener(IAIEventListener* pListener)
{
	gAIEnv.pPerceptionManager->UnregisterAIEventListener(pListener);
}

//===================================================================
// RegisterStimulus
//===================================================================
void CAISystem::RegisterStimulus(const SAIStimulus& stim)
{
	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;
	if (!IsEnabled())
		return;

	gAIEnv.pPerceptionManager->RegisterStimulus(stim);
}

//===================================================================
// IgnoreStimulusFrom
//===================================================================
void CAISystem::IgnoreStimulusFrom(EntityId sourceId, EAIStimulusType type, float time)
{
	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	if (!IsEnabled())
		return;

	gAIEnv.pPerceptionManager->IgnoreStimulusFrom(sourceId, type, time);
}

//===================================================================
// RegisterDamageRegion
//===================================================================
void CAISystem::RegisterDamageRegion(const void* pID, const Sphere& sphere)
{
	if (sphere.radius > 0.0f)
		m_damageRegions[pID] = sphere;
	else
		m_damageRegions.erase(pID);
}

//===================================================================
// GetHidespotsInRange
//===================================================================
MultimapRangeHideSpots& CAISystem::GetHideSpotsInRange(MultimapRangeHideSpots& hidespots,
                                                       MapConstNodesDistance& traversedNodes,
                                                       const Vec3& startPos, float maxDist,
                                                       IAISystem::tNavCapMask navCapMask, float passRadius, bool skipNavigationTest,
                                                       IEntity* pSmartObjectUserEntity,
                                                       unsigned lastNavNodeIndex,
                                                       const class CAIObject* pRequester)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);
	hidespots.clear();

	float maxDistSq = square(maxDist);

	if (!skipNavigationTest)
	{
		m_pGraph->GetNodesInRange(traversedNodes, startPos, maxDist, navCapMask, passRadius, lastNavNodeIndex, pRequester);
		if (traversedNodes.empty())
			return hidespots;
	}

	// waypoint
	if (navCapMask & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
	{
		if (skipNavigationTest)
		{
			// (MATT) Commented out the warning for milestone.
			// Ideally, we would not skip the test anyway, and gain performance by avoiding the hash spacelookup of graph hidespots {2009/12/11}
			//AIWarning("Not implemented skipNavigationTest for waypoint");
		}
		else
		{
			const MapConstNodesDistance::const_iterator itEnd = traversedNodes.end();
			for (MapConstNodesDistance::const_iterator it = traversedNodes.begin(); it != itEnd; ++it)
			{
				const GraphNode* pNode = it->first;
				if (pNode->navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
				{
					float distance = it->second;
					const SWaypointNavData* pData = pNode->GetWaypointNavData();
					if (pData->type != WNT_HIDE)
						continue;
					MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
					                                                                                                SHideSpot(SHideSpotInfo::eHST_WAYPOINT, pNode->GetPos(), pData->dir)));
					resultIt->second.pNavNode = pNode;
				}
			}
		}
	}

	// triangular
	if (navCapMask & IAISystem::NAV_TRIANGULAR)
	{
		assert(false);
	}

	// hide anchors
	{
		int anchorTypes[2] = { AIANCHOR_COMBAT_HIDESPOT, AIANCHOR_COMBAT_HIDESPOT_SECONDARY };
		for (int at = 0; at < 2; ++at)
		{
			const AIObjectOwners::const_iterator oiEnd = gAIEnv.pAIObjectManager->m_Objects.end();
			for (AIObjectOwners::const_iterator oi = gAIEnv.pAIObjectManager->m_Objects.find(anchorTypes[at]); oi != oiEnd; ++oi)
			{
				CAIObject* object = oi->second.GetAIObject();
				if (object->GetType() != anchorTypes[at])
					break;
				if (!object->IsEnabled())
					continue;
				float distanceSq = Distance::Point_PointSq(object->GetPos(), startPos);
				if (distanceSq > maxDistSq)
					continue;

				const GraphNode* pNode = gAIEnv.pGraph->GetNode(object->GetNavNodeIndex());
				if (!pNode)
					continue;

				float distance;
				MapConstNodesDistance::const_iterator it;
				if (!skipNavigationTest)
				{
					it = traversedNodes.find(pNode);
					if (it == traversedNodes.end())
						continue;
					distance = it->second;
				}
				else
				{
					distance = sqrtf(distanceSq);
				}

				MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
				                                                                                                SHideSpot(SHideSpotInfo::eHST_ANCHOR, object->GetPos(), object->GetMoveDir())));
				resultIt->second.pNavNode = pNode;
				resultIt->second.pAnchorObject = object;
			}
		}
	}

	// Dynamic hidespots.
	if (gAIEnv.CVars.DynamicHidespotsEnabled != 0)
	{
		typedef std::vector<SDynamicObjectHideSpot> TDynamicHideSpots;
		static TDynamicHideSpots dynamicHideSpots;
		m_dynHideObjectManager.GetHidePositionsWithinRange(dynamicHideSpots, startPos, maxDist, navCapMask, passRadius, lastNavNodeIndex);

		const TDynamicHideSpots::const_iterator diEnd = dynamicHideSpots.end();
		for (TDynamicHideSpots::const_iterator di = dynamicHideSpots.begin(); di != diEnd; ++di)
		{
			const SDynamicObjectHideSpot& hs = *di;
			GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(hs.nodeIndex);
			if (!pNode)
				continue;
			float distance;
			MapConstNodesDistance::const_iterator it;
			if (!skipNavigationTest)
			{
				it = traversedNodes.find(pNode);
				if (it == traversedNodes.end())
					continue;
				distance = it->second;
			}
			else
			{
				distance = Distance::Point_Point(hs.pos, startPos);
			}

			MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
			                                                                                                SHideSpot(SHideSpotInfo::eHST_DYNAMIC, hs.pos, hs.dir)));
			resultIt->second.pNavNode = pNode;
			resultIt->second.entityId = hs.entityId;
		}
	}

	// smart objects
	if (pSmartObjectUserEntity)
	{
		QueryEventMap query;
		IEntity* pObjectEntity = NULL;
		m_pSmartObjectManager->TriggerEvent("Hide", pSmartObjectUserEntity, pObjectEntity, &query);

		const QueryEventMap::const_iterator itEnd = query.end();
		for (QueryEventMap::const_iterator it = query.begin(); it != itEnd; ++it)
		{
			const CQueryEvent* pQueryEvent = &it->second;

			Vec3 pos;
			if (pQueryEvent->pRule->pObjectHelper)
				pos = pQueryEvent->pObject->GetHelperPos(pQueryEvent->pRule->pObjectHelper);
			else
				pos = pQueryEvent->pObject->GetPos();
			Vec3 dir = pQueryEvent->pObject->GetOrientation(pQueryEvent->pRule->pObjectHelper);

			// [AlexMc|08.06.10]: This doesn't actually get a SO navnode! It gets a non-SO navnode that
			// encloses the SO. Do we want to call GetCorrespondingNavNode instead?
			unsigned nodeIndex = pQueryEvent->pObject->GetEnclosingNavNode(pQueryEvent->pRule->pObjectHelper);
			if (nodeIndex)
			{
				const GraphNode* pNavNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
				AIAssert(pNavNode);
				MapConstNodesDistance::const_iterator dit(traversedNodes.end());
				if (!skipNavigationTest)
					dit = traversedNodes.find(pNavNode);
				if (skipNavigationTest || dit != traversedNodes.end())
				{
					float distance = skipNavigationTest ? Distance::Point_Point(pos, startPos) : dit->second;
					MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
					                                                                                                SHideSpot(SHideSpotInfo::eHST_SMARTOBJECT, pos, dir)));
					resultIt->second.pNavNode = pNavNode;
					resultIt->second.SOQueryEvent = *pQueryEvent;
				}
			}
		}
	}

	// Volume hidespots
	if (navCapMask & IAISystem::NAV_VOLUME)
	{
		// well.. it could be done but it's not used?
		/*
		    static std::vector<SVolumeHideSpot> hidePositions;
		    hidePositions.resize(0);
		    m_pVolumeNavRegion->GetHideSpotsWithinRadius(startPos, maxDist, SVolumeHideFunctor(hidePositions));
		 */
	}

	return hidespots;
}

//===================================================================
// AdjustDirectionalCoverPosition
//===================================================================
// (MATT) Raycasts. No new code uses this, just seekcover and hide {2009/07/10}
void CAISystem::AdjustDirectionalCoverPosition(Vec3& pos, const Vec3& dir, float agentRadius, float testHeight)
{
	Vec3 floorPos(pos);
	// Add fudge to the initial position in case the point is very near to ground.
	GetFloorPos(floorPos, pos + Vec3(0.0f, 0.0f, max(0.5f, testHeight * 0.5f)), WalkabilityFloorUpDist,
	            WalkabilityFloorDownDist, WalkabilityDownRadius, AICE_ALL);
	floorPos.z += testHeight;
	Vec3 hitPos;
	const float distToWall = AGENT_COVER_CLEARANCE + agentRadius;
	float hitDist = distToWall;
	if (IntersectSweptSphere(&hitPos, hitDist, Lineseg(floorPos, floorPos + dir * distToWall), 0.15f, AICE_ALL))
		pos = floorPos + dir * (hitDist - distToWall);
	else
		pos = floorPos;
}

//===================================================================
// AdjustOmniDirectionalCoverPosition
// if hideBehind false - move point to left/right side of cover, to locate spot by cover, but not hiden
//===================================================================
void CAISystem::AdjustOmniDirectionalCoverPosition(Vec3& pos, Vec3& dir, float hideRadius, float agentRadius, const Vec3& hideFrom, const bool hideBehind)
{
	dir = hideFrom - pos;
	dir.z = 0;
	dir.NormalizeSafe();

	if (!hideBehind)
	{
		// dir is left/right offset when not hiding behind cover
		float tmp(dir.x);
		dir.x = -dir.y;
		dir.y = tmp;
		if (cry_random(0, 99) < 50)
			dir = -dir;
	}
	else
	{

		if (gAIEnv.configuration.eCompatibilityMode == ECCM_GAME04)
		{
			// MERGE : (MATT) The maths wasn't quite right here, and this might not suit Crysis anyway {2008/01/04:12:59:04}
			// If hide object has a radius significantly bigger than the AI's, get a position near the edge so we can lean out
			if (hideRadius > agentRadius * 1.1f)
			{
				// Move the position off-centre
				Vec3 adjustDir = dir.Cross(Vec3(0, 0, 1));
				if (cry_random_uint32() & 1)
					adjustDir = -adjustDir;
				float fAdjustDist = hideRadius - agentRadius;
				adjustDir.SetLength(fAdjustDist);
				pos += adjustDir;

				// Adjust length of dir, since we're now less than a radius from the centre of the obstacle
				dir *= cosf(gf_PI * 0.5f * fAdjustDist / hideRadius);
			}
		}

	}

	pos -= dir * (max(hideRadius, 0.0f) + agentRadius + AGENT_COVER_CLEARANCE);
}

//===================================================================
// TempHideSpot
// Structure to hold hide points.
// Includes distance to sort the points and both the found hide pos and the object pos/dir.
//===================================================================
struct TempHideSpot
{
	TempHideSpot(const Vec3& pos, const Vec3& objPos, const Vec3& objDir, float rad, float dist, bool collidable, bool directional) :
		pos(pos), objDir(objDir), rad(rad), dist(dist), collidable(collidable), directional(directional) {}
	bool operator<(const TempHideSpot& rhs) const { return dist < rhs.dist; }
	Vec3  pos, objDir;
	float rad;
	float dist;
	bool  collidable;
	bool  directional;
};

//===================================================================
// GetHideSpotsInRange
//===================================================================
unsigned int CAISystem::GetHideSpotsInRange(IAIObject* requester, const Vec3& reqPos,
                                            const Vec3& hideFrom, float minRange, float maxRange, bool collidableOnly, bool validatedOnly,
                                            unsigned int maxPts, Vec3* coverPos, Vec3* coverObjPos, Vec3* coverObjDir, float* coverRad, bool* coverCollidable)
{
	CPuppet* puppet = CastToCPuppetSafe(requester);

	IAISystem::tNavCapMask navCapMask = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_SMARTOBJECT;
	float passRadius = 0.5f;
	IEntity* pRequesterEnt = 0;
	unsigned lastNavNodeIndex = 0;

	if (puppet)
	{
		navCapMask = puppet->GetMovementAbility().pathfindingProperties.navCapMask;
		passRadius = puppet->GetParameters().m_fPassRadius;
		pRequesterEnt = puppet->GetEntity();
		lastNavNodeIndex = puppet->GetNavNodeIndex();
	}

	MultimapRangeHideSpots hidespots;
	MapConstNodesDistance traversedNodes;

	GetHideSpotsInRange(hidespots, traversedNodes, reqPos, maxRange,
	                    navCapMask, passRadius, false, pRequesterEnt, lastNavNodeIndex, (CAIObject*)requester);

	// Only update the obstacles if validation is requested.
	const CPathObstacles* obstacles = 0;

	if (puppet)
		obstacles = &puppet->GetPathAdjustmentObstacles(validatedOnly);

	float minRangeSq = sqr(minRange);
	float maxRangeSq = sqr(maxRange);

	std::vector<TempHideSpot> spots;
	spots.reserve(hidespots.size());

	const MultimapRangeHideSpots::iterator liEnd = hidespots.end();
	for (MultimapRangeHideSpots::iterator li = hidespots.begin(); li != liEnd; ++li)
	{
		const SHideSpot& hs = li->second;

		if (hs.info.type == SHideSpotInfo::eHST_SMARTOBJECT)
			continue;

		bool collidable(true);
		float rad = 0.0f;

		if (hs.pObstacle && !hs.pObstacle->IsCollidable())
			collidable = false;
		if (hs.pAnchorObject && hs.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY)
		{
			collidable = false;
			rad = 0.2f;
		}

		if (collidableOnly && !collidable)
			continue;

		if (hs.pObstacle)
			rad = hs.pObstacle->fApproxRadius;

		Vec3 pos = hs.info.pos;
		Vec3 dir = hs.info.dir;
		const Vec3& objPos(hs.info.pos); // AlexMcC: should this be referencing pos or objPos?

		if (hs.pObstacle || (hs.pAnchorObject && hs.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY))
			AdjustOmniDirectionalCoverPosition(pos, dir, max(rad, 0.0f), passRadius, hideFrom);

		float d = Distance::Point_PointSq(pos, reqPos);

		if (d >= minRangeSq && d <= maxRangeSq)
			spots.push_back(TempHideSpot(pos, objPos, dir, rad, d, collidable,
			                             hs.info.type == SHideSpotInfo::eHST_ANCHOR || hs.info.type == SHideSpotInfo::eHST_WAYPOINT || hs.info.type == SHideSpotInfo::eHST_DYNAMIC));
	}

	// Output best points.
	std::sort(spots.begin(), spots.end());

	unsigned int npts = 0;

	for (unsigned i = 0, n = spots.size(); i < n; ++i)
	{
		TempHideSpot& spot = spots[i];
		// Check if the point is inside another cover object and skip if it is.
		bool embedded = false;
		for (unsigned j = 0, m = spots.size(); j < m; ++j)
		{
			TempHideSpot& spotj = spots[j];
			if (i == j || !spotj.collidable || spotj.rad < 0.01f) continue;
			if (Distance::Point_Point2DSq(spot.pos, spotj.pos) < sqr(spotj.rad + passRadius))
			{
				embedded = true;
				break;
			}
		}
		if (embedded)
			continue;

		// Discard points which do not offer cover.
		if (validatedOnly)
		{
			if (spot.directional)
			{
				Vec3 dirHideToEnemy = hideFrom - spot.pos;
				dirHideToEnemy.NormalizeSafe();
				if (dirHideToEnemy.Dot(spot.objDir) < HIDESPOT_COVERAGE_ANGLE_COS)
					continue;
			}
		}

		if (coverPos) coverPos[npts] = spot.pos;
		if (coverObjPos) coverObjPos[npts] = spot.pos;
		if (coverObjDir) coverObjDir[npts] = spot.objDir;
		if (coverRad) coverRad[npts] = spot.rad;
		if (coverCollidable) coverCollidable[npts] = spot.collidable;
		++npts;

		if (npts >= maxPts) break;
	}

	return npts;
}

//===================================================================
// SetPerceptionDistLookUp
//===================================================================
void CAISystem::SetPerceptionDistLookUp(float* pLookUpTable, int tableSize)
{
	AILinearLUT& lut(m_VisDistLookUp);
	lut.Set(pLookUpTable, tableSize);
}

//===================================================================
// UpdateGlobalPerceptionScale
//===================================================================
void CAISystem::UpdateGlobalPerceptionScale(const float visualScale, const float audioScale, const EAIFilterType filterType /* = eAIFT_All */, const char* factionName /* = NULL */)
{
	const IFactionMap& pFactionMap = gEnv->pAISystem->GetFactionMap();
	uint8 factionID = factionName ? pFactionMap.GetFactionID(factionName) : 0;

	m_globalPerceptionScale.SetGlobalPerception(visualScale, audioScale, filterType, factionID);
}

void CAISystem::DisableGlobalPerceptionScaling()
{
	m_globalPerceptionScale.ResetGlobalPerception();
}

//===================================================================
// GetGlobalVisualScale and GetGlobalAudioScale
//===================================================================
float CAISystem::GetGlobalVisualScale(const IAIObject* pAIObject) const
{
	return m_globalPerceptionScale.GetGlobalVisualScale(pAIObject);
}

float CAISystem::GetGlobalAudioScale(const IAIObject* pAIObject) const
{
	return m_globalPerceptionScale.GetGlobalAudioScale(pAIObject);
}

void CAISystem::RegisterGlobalPerceptionListener(IAIGlobalPerceptionListener* pListner)
{
	m_globalPerceptionScale.RegisterListener(pListner);
}

void CAISystem::UnregisterGlobalPerceptionlistener(IAIGlobalPerceptionListener* pListner)
{
	m_globalPerceptionScale.UnregisterListener(pListner);
}

//===================================================================
// GetVisPerceptionDistScale
//===================================================================
float CAISystem::GetVisPerceptionDistScale(float distRatio)
{
	AILinearLUT& lut(m_VisDistLookUp);

	// Make sure the dist ratio is in required range [0..1]
	distRatio = clamp_tpl(distRatio, 0.0f, 1.0f);

	// If no table is setup, use simple quadratic fall off function.
	if (lut.GetSize() < 2)
		return sqr(1.0f - distRatio);
	return lut.GetValue(distRatio) / 100.0f;
}

//===================================================================
// DebugReportHitDamage
//===================================================================
void CAISystem::DebugReportHitDamage(IEntity* pVictim, IEntity* pShooter, float damage, const char* material)
{
#ifdef CRYAISYSTEM_DEBUG
	if (!pVictim)
		return;

	CAIActor* pVictimAI = CastToCAIActorSafe(pVictim->GetAI());
	if ((pVictimAI != NULL) && IsRecording(pVictimAI, IAIRecordable::E_HIT_DAMAGE))
	{
		switch (pVictimAI->GetType())
		{
		case AIOBJECT_PLAYER:
		case AIOBJECT_ACTOR:
			{
				char msg[64];
				cry_sprintf(msg, "%.1f (%s)", damage, material);
				IAIRecordable::RecorderEventData recorderEventData(msg);

				pVictimAI->RecordEvent(IAIRecordable::E_HIT_DAMAGE, &recorderEventData);
			}
		}
	}
#endif //CRYAISYSTEM_DEBUG
}

//===================================================================
// DebugReportDeath
//===================================================================
void CAISystem::DebugReportDeath(IAIObject* pVictim)
{
#ifdef CRYAISYSTEM_DEBUG
	if (!pVictim) return;

	if (CPuppet* pPuppet = pVictim->CastToCPuppet())
	{
		IAIRecordable::RecorderEventData recorderEventData("Death");
		pPuppet->RecordEvent(IAIRecordable::E_DEATH, &recorderEventData);
	}
	else if (CAIPlayer* pPlayer = pVictim->CastToCAIPlayer())
	{
		if (IsRecording(pPlayer, IAIRecordable::E_DEATH))
		{
			pPlayer->IncDeathCount();
			char msg[32];
			cry_sprintf(msg, "Death %d", pPlayer->GetDeathCount());
			IAIRecordable::RecorderEventData recorderEventData(msg);
			pPlayer->RecordEvent(IAIRecordable::E_DEATH, &recorderEventData);
		}
	}
#endif //CRYAISYSTEM_DEBUG
}

//===================================================================
// NotifyDeath
//===================================================================
void CAISystem::NotifyDeath(IAIObject* pVictim)
{
	assert(pVictim);

	if (pVictim != NULL)
	{
		CAIActor* pActor = pVictim->CastToCAIActor();
		if (pActor != NULL)
		{
			pActor->NotifyDeath();
		}
	}
}

//===================================================================
// WouldHumanBeVisible
//===================================================================
bool CAISystem::WouldHumanBeVisible(const Vec3& footPos, bool fullCheck) const
{
	int ignore = gAIEnv.CVars.IgnoreVisibilityChecks;
	if (ignore)
		return false;

	static float radius = 0.5f;
	static float height = 1.8f;

	const CCamera& cam = GetISystem()->GetViewCamera();
	AABB aabb(AABB::RESET);
	aabb.Add(footPos + Vec3(0, 0, radius), radius);
	aabb.Add(footPos + Vec3(0, 0, height - radius), radius);

	if (!cam.IsAABBVisible_F(aabb))
		return false;

	// todo Danny do fullCheck
	return true;
}

// Static formation point
struct SAIStatFormPt
{
	enum ETested { NOT_TESTED, VALID, INVALID };
	SAIStatFormPt(const Vec3& p, float w, IAISystem::ENavigationType navType) : pos(p), w(w), d(0), vis(0), test(NOT_TESTED), navType(navType) {}
	Vec3                       pos;
	float                      d;
	float                      w;
	unsigned char              vis;
	ETested                    test;
	IAISystem::ENavigationType navType;
};

// Static formation histogram island
struct SAIStatFormIsland
{
	SAIStatFormIsland(int a, int b, float v) : a(a), b(b), v(v) {}
	inline bool operator<(const SAIStatFormIsland& rhs) const { return v < rhs.v; }
	inline bool operator>(const SAIStatFormIsland& rhs) const { return v > rhs.v; }
	int   a, b;
	float v;
};

// Static formation unit
struct SAIStatFormUnit
{
	SAIStatFormUnit(const Vec3& pos, unsigned i) : pos(pos), i(i), targetPos(0, 0, 0), d(0) {}
	inline bool operator<(const SAIStatFormUnit& rhs) const { return d < rhs.d; }
	inline bool operator>(const SAIStatFormUnit& rhs) const { return d > rhs.d; }
	Vec3     pos;
	Vec3     targetPos;
	float    d;
	unsigned i;
};

//===================================================================
// ProcessBalancedDamage
//===================================================================
float CAISystem::ProcessBalancedDamage(IEntity* pShooterEntity, IEntity* pTargetEntity, float damage, const char* damageType)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	if (!pShooterEntity || !pTargetEntity || !pShooterEntity->HasAI() || !pTargetEntity->HasAI())
		return damage;

	//	AILogEvent("CAISystem::ProcessBalancedDamage: %s -> %s Damage:%f type:%s\n", pShooterEntity->GetName(), pTargetEntity->GetName(), damage, damageType);

	IAIObject* pShooterAI = pShooterEntity->GetAI();
	CAIActor* pShooterActor = pShooterAI ? pShooterAI->CastToCAIActor() : 0;

	if (!pShooterActor)
		return damage;

	if (pShooterActor->GetType() == AIOBJECT_PLAYER)
		return damage;

	if (IAIActorProxy* proxy = pShooterActor->GetProxy())
	{
		if (proxy->IsDriver())
		{
			if (EntityId vehicleId = proxy->GetLinkedVehicleEntityId())
			{
				IEntity* pVehicleEnt = gEnv->pEntitySystem->GetEntity(vehicleId);
				if (IAIObject* vehicleAIObject = pVehicleEnt ? pVehicleEnt->GetAI() : 0)
				{
					if (vehicleAIObject->CastToCAIActor())
					{
						pShooterActor = pShooterAI->CastToCAIActor();
					}
				}
			}
		}
	}

	IAIObject* pTargetAI = pTargetEntity->GetAI();
	CAIActor* pTargetActor = pTargetAI ? pTargetAI->CastToCAIActor() : 0;
	if (!pTargetActor)
		return 0.0f;

	if (damageType && *damageType && !pShooterActor->IsHostile(pTargetAI))
	{
		// Skip friendly bullets.
		// but only if this is not grabbed AI (human shield.)
		CAIObject* player = GetPlayer();
		IAIActorProxy* proxy = player ? player->GetProxy() : 0;

		if (proxy && (proxy->GetGrabbedEntity() != pTargetEntity) && (strcmp(damageType, "bullet") == 0))
			return 0.0f;
	}

	if (damageType && strcmp(damageType, "bullet") != 0)
		return damage;

	CPuppet* pShooterPuppet = pShooterActor->CastToCPuppet();
	if (!pShooterPuppet)
		return 0.0f;

	if (pShooterPuppet->CanDamageTarget())
	{
		if (gAIEnv.configuration.eCompatibilityMode != ECCM_CRYSIS2)
		{
			if (pTargetActor->GetType() == AIOBJECT_PLAYER)
			{
				if (pTargetActor->GetProxy())
				{
					Vec3 dirToTarget = pShooterPuppet->GetPos() - pTargetActor->GetPos();
					float distToTarget = dirToTarget.NormalizeSafe();

					float dirMod = (1.0f + dirToTarget.Dot(pTargetActor->GetViewDir())) * 0.5f;
					float distMod = 1.0f - sqr(1.0f - distToTarget / pShooterPuppet->GetParameters().m_fAttackRange);

					const float maxDirDamageMod = 0.6f;
					const float maxDistDamageMod = 0.7f;

					dirMod = maxDirDamageMod + (1 - maxDirDamageMod) * sqr(dirMod);
					distMod = maxDistDamageMod + (1 - maxDistDamageMod) * distMod;

					damage *= dirMod * distMod;
					damage *= cry_random(0.6f, 1.0f);

#ifdef CRYAISYSTEM_DEBUG

					float maxHealth = (float)pTargetActor->GetProxy()->GetActorMaxHealth();

					DEBUG_AddFakeDamageIndicator(pShooterPuppet, (damage / maxHealth) * 5.0f);

					m_DEBUG_screenFlash += damage / maxHealth;
					if (m_DEBUG_screenFlash > 2.0f)
						m_DEBUG_screenFlash = 2.0f;

#endif    //CRYAISYSTEM_DEBUG

				}
			}
		}

		return damage;
	}

	return 0.0f;
}

//===================================================================
// Interest System
//===================================================================
ICentralInterestManager* CAISystem::GetCentralInterestManager(void)
{
	return CCentralInterestManager::GetInstance();
}

ICentralInterestManager const* CAISystem::GetCentralInterestManager(void) const
{
	return CCentralInterestManager::GetInstance();
}

//===================================================================
struct SAIPunchableObject
{
	SAIPunchableObject(const AABB& bounds, float dist, IPhysicalEntity* pPhys) :
		bounds(bounds), pPhys(pPhys), dist(dist) {}

	inline bool operator<(const SAIPunchableObject& rhs) const { return dist < rhs.dist; }

	AABB             bounds;
	float            dist;
	IPhysicalEntity* pPhys;
};

//===================================================================
// GetNearestPunchableObjectPosition
//===================================================================
bool CAISystem::GetNearestPunchableObjectPosition(IAIObject* pRef, const Vec3& searchPos, float searchRad, const Vec3& targetPos,
                                                  float minSize, float maxSize, float minMass, float maxMass,
                                                  Vec3& posOut, Vec3& dirOut, IEntity** objEntOut)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRef);
	if (!pPuppet)
		return false;

	const float agentRadius = 0.6f;
	const float distToObject = -0.1f;

	std::vector<SAIPunchableObject> objects;

	AABB aabb(searchPos - Vec3(searchRad, searchRad, searchRad / 2), searchPos + Vec3(searchRad, searchRad, searchRad / 2));
	PhysicalEntityListAutoPtr entities;
	unsigned nEntities = GetEntitiesFromAABB(entities, aabb, AICE_DYNAMIC);

	const CPathObstacles& pathAdjustmentObstacles = pPuppet->GetPathAdjustmentObstacles();
	IAISystem::tNavCapMask navCapMask = pPuppet->GetMovementAbility().pathfindingProperties.navCapMask;
	const float passRadius = pPuppet->GetParameters().m_fPassRadius;

	Vec3 dirToTarget = targetPos - searchPos;
	dirToTarget.Normalize();

	for (unsigned i = 0; i < nEntities; ++i)
	{
		IPhysicalEntity* pPhysEntity = entities[i];

		// Skip moving objects
		pe_status_dynamics status;
		pPhysEntity->GetStatus(&status);
		if (status.v.GetLengthSquared() > sqr(0.1f))
			continue;
		if (status.w.GetLengthSquared() > sqr(0.1f))
			continue;

		pe_status_pos statusPos;
		pPhysEntity->GetStatus(&statusPos);

		AABB bounds(statusPos.BBox[0] + statusPos.pos, statusPos.BBox[1] + statusPos.pos);

		float dist = Distance::Point_Point(statusPos.pos, searchPos);
		if (dist > searchRad)
			continue;

		float rad = bounds.GetRadius();
		if (rad < minSize || rad > maxSize)
			continue;
		float height = bounds.max.z - bounds.min.z;
		if (height < minSize || height > maxSize)
			continue;

		pe_status_dynamics dyn;
		pPhysEntity->GetStatus(&dyn);
		float mass = dyn.mass;
		if (mass < minMass || mass > maxMass)
			continue;

		// Weight towards target.
		Vec3 dirToObject = statusPos.pos - searchPos;
		dirToObject.NormalizeFast();
		float dot = dirToTarget.Dot(dirToObject);
		float w = 0.25f + 0.75f * sqr((1 - dot + 1) / 2);

		objects.push_back(SAIPunchableObject(bounds, dist * w, pPhysEntity));
	}

	std::sort(objects.begin(), objects.end());

	for (unsigned i = 0; i < objects.size(); ++i)
	{
		// check the altitude
		IPhysicalEntity* pPhysEntity = objects[i].pPhys;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPhysEntity);
		if (!pEntity)
			continue;

		const AABB& bounds = objects[i].bounds;
		Vec3 center = bounds.GetCenter();

		Vec3 groundPos(center.x, center.y, center.z - 4.0f);
		Vec3 delta = groundPos - center;
		ray_hit hit;
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(center, delta, AICE_ALL,
		                                                rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
			continue;
		groundPos = hit.pt;

		Vec3 dirObjectToTarget = targetPos - center;
		dirObjectToTarget.z = 0;
		dirObjectToTarget.Normalize();

		float radius = bounds.GetRadius();

		Vec3 posBehindObject = center - dirObjectToTarget * (agentRadius + distToObject + radius);

		// Check if the point is reachable.
		delta = -dirObjectToTarget * (agentRadius + distToObject + radius);
		if (gEnv->pPhysicalWorld->RayWorldIntersection(center, delta, AICE_ALL,
		                                               rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
		{
			continue;
		}

		float height = bounds.max.z - bounds.min.z;

		// Raycast to find ground pos.
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(posBehindObject, Vec3(0, 0, -(height / 2 + 1)), AICE_ALL,
		                                                rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
			continue;
		posBehindObject = hit.pt;
		posBehindObject.z += 0.2f;

		// Check if it possible to stand at the object position.
		if (OverlapCapsule(Lineseg(Vec3(posBehindObject.x, posBehindObject.y, posBehindObject.z + agentRadius + 0.3f),
		                           Vec3(posBehindObject.x, posBehindObject.y, posBehindObject.z + 2.0f - agentRadius)), agentRadius, AICE_ALL))
			continue;

		// Check stuff in front of the object.
		delta = dirObjectToTarget * 3.0f;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(center, delta, AICE_ALL,
		                                               rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
		{
			continue;
		}

		// The object is valid.
		posOut = posBehindObject;
		dirOut = dirObjectToTarget;
		*objEntOut = pEntity;

		return true;
	}

	return false;
}

// NOTE Mai 21, 2007: <pvl> I put this function here because there's no AllNodesContainer.cpp
// and creating it for a single debugging function that might go away any time seems overkill.
// It's also too complex in terms of dependencies to go into AllNodesContainer.h .
// ATTN Mai 21, 2007: <pvl> if something seems strange in this function, it might well be
// a bug.  This is a piece of debugging code that was written too fast.
// Update: <mikko> Changed the validation to reflect the changes in hash space code.
bool CAllNodesContainer::ValidateHashSpace() const
{
	std::unique_ptr<Iterator> nodeIt(new Iterator(*this, IAISystem::NAV_WAYPOINT_HUMAN));
	int numWayptNodes = 0;

	AILogProgress(">>> Validating HashSpace");
	while (unsigned int nodeIndex = nodeIt->GetNode())
	{
		GraphNode* node = gAIEnv.pGraph->GetNodeManager().GetNode(nodeIndex);

		++numWayptNodes;

		typedef std::vector<std::pair<float, unsigned>> TNodes;
		static TNodes nodes;
		nodes.resize(0);

		GetAllNodesWithinRange(nodes, node->GetPos(), 0.05f, IAISystem::NAV_WAYPOINT_HUMAN);

		if (nodes.size() > 1)
			AIWarning("More than one node within 5cm from each other.");

		bool validated = false;

		TNodes::const_iterator it = nodes.begin();
		TNodes::const_iterator end = nodes.end();
		for (; it != end; ++it)
		{
			if (it->second == nodeIndex)
			{
				validated = true;
				break;
			}
		}
		if (!validated)
		{
			AIWarning("HashSpace probably corrupt - a node in a wrong cell!");
			return false;
		}

		nodeIt->Increment();
	}
	AILogProgress(">>> HashSpace validation: %d waypts in the level", numWayptNodes);

	for (unsigned i = 0, ni = m_hashSpace.GetBucketCount(); i < ni; ++i)
	{
		for (unsigned j = 0, nj = m_hashSpace.GetObjectCountInBucket(i); j < nj; ++j)
		{
			const SGraphNodeRecord& nodeRec = m_hashSpace.GetObjectInBucket(j, i);

			if (!DoesNodeExist(nodeRec.nodeIndex))
			{
				Vec3 nodePos = nodeRec.GetPos(gAIEnv.pGraph->GetNodeManager());
				AIWarning("HashSpace probably corrupt - contains a node that's not in AllNodesContainer!");
				AIWarning("  pos = (%5.2f, %5.2f, %5.2f), index = %d", nodePos.x, nodePos.y, nodePos.z, nodeRec.nodeIndex);
				return false;
			}

			int ii, jj, kk;
			m_hashSpace.GetIJKFromPosition(nodeRec.GetPos(gAIEnv.pGraph->GetNodeManager()), ii, jj, kk);
			unsigned bucket = m_hashSpace.GetHashBucketIndex(ii, jj, kk);
			if (i != bucket)
				AIWarning("HashSpace probably corrupt - a node incorrectly assigned to cell!");
		}
	}

	return true;
}

//====================================================================
// AllocGoalPipeId
//====================================================================
int CAISystem::AllocGoalPipeId() const
{
	CCCPOINT(CAISystem_AllocGoalPipeId);
	static int g_iLastActionId = 0;
	g_iLastActionId++;
	if (g_iLastActionId < 0)
		g_iLastActionId = 1;
	return g_iLastActionId;
}

//====================================================================
// CreateSignalExtraData
//====================================================================
IAISignalExtraData* CAISystem::CreateSignalExtraData() const
{
	return new AISignalExtraData;
}

//====================================================================
// FreeSignalExtraData
//====================================================================
void CAISystem::FreeSignalExtraData(IAISignalExtraData* pData) const
{
	if (pData)
		delete (AISignalExtraData*) pData;
}

//===================================================================
// Get the tactical point system
//===================================================================
ITacticalPointSystem* CAISystem::GetTacticalPointSystem(void)
{
	return gAIEnv.pTacticalPointSystem;
}

ICommunicationManager* CAISystem::GetCommunicationManager() const
{
	return gAIEnv.pCommunicationManager;
}

ICoverSystem* CAISystem::GetCoverSystem() const
{
	return gAIEnv.pCoverSystem;
}

INavigationSystem* CAISystem::GetNavigationSystem() const
{
	return gAIEnv.pNavigationSystem;
}

ISelectionTreeManager* CAISystem::GetSelectionTreeManager() const
{
	return gAIEnv.pSelectionTreeManager;
}

BehaviorTree::IBehaviorTreeManager* CAISystem::GetIBehaviorTreeManager() const
{
	return gAIEnv.pBehaviorTreeManager;
}

BehaviorTree::IGraftManager* CAISystem::GetIGraftManager() const
{
	return gAIEnv.pGraftManager;
}

ITargetTrackManager* CAISystem::GetTargetTrackManager() const
{
	return gAIEnv.pTargetTrackManager;
}

IMovementSystem* CAISystem::GetMovementSystem() const
{
	return gAIEnv.pMovementSystem;
}

AIActionSequence::ISequenceManager* CAISystem::GetSequenceManager() const
{
	return gAIEnv.pSequenceManager;
}

IClusterDetector* CAISystem::GetClusterDetector() const
{
	return gAIEnv.pClusterDetector;
}

//===================================================================
// AssignPFPropertiesToPathType
//===================================================================
void CAISystem::AssignPFPropertiesToPathType(const string& sPathType, const AgentPathfindingProperties& properties)
{
	mapPFProperties.insert(PFPropertiesMap::value_type(sPathType, properties));
}

//===================================================================
// GetPFPropertiesOfPathType
//===================================================================
const AgentPathfindingProperties* CAISystem::GetPFPropertiesOfPathType(const string& sPathType)
{
	PFPropertiesMap::iterator iterPFProperties = mapPFProperties.find(sPathType);
	return (iterPFProperties != mapPFProperties.end()) ? &iterPFProperties->second : 0;
}

//===================================================================
// GetRegisteredPathTypes
//===================================================================
string CAISystem::GetPathTypeNames()
{
	string pathNames;
	for (PFPropertiesMap::iterator iter = mapPFProperties.begin(); iter != mapPFProperties.end(); ++iter)
		pathNames += iter->first + ' ';
	return pathNames;
}

//===================================================================
// Parse AI description tables
//===================================================================
bool CAISystem::ParseTables(int firstTable, bool parseMovementAbility, IFunctionHandler* pH, AIObjectParams& aiParams, bool& updateAlways)
{
	return m_pScriptAI->ParseTables(firstTable, parseMovementAbility, pH, aiParams, updateAlways);
}

bool CAISystem::CompareFloatsFPUBugWorkaround(float fLeft, float fRight)
{
	//[AlexMcC|06.04.10]: Pass floats through these parameters to force the values out of the
	// fp registers on x86. This ensures we're working with 32-bit precision, and prevents comparison
	// errors caused by using 80-bit precision for one value and 32-bit precision for the other:
	// http://stackoverflow.com/questions/1338045/gcc-precision-bug
	// http://hal.archives-ouvertes.fr/docs/00/28/14/29/PDF/floating-point-article.pdf (section 3.1.1 x87 floating-point unit)
	// This only solves problems in code outside of this cpp file (we found problems in Puppet.h/cpp)

	CRY_ASSERT_MESSAGE(!(fLeft > fRight) || !(fRight > fLeft), "Floating point precision error");

	return fLeft > fRight;
}

void IAISystem::tNavCapMask::Serialize(TSerialize ser)
{
	ser.Value("navCapMask", m_navCaps);
}

void CAISystem::Release()
{
	delete this;
}

void CAISystem::Enable(bool enable /*=true*/)
{
	if (!enable && (m_IsEnabled != enable))
	{
		FlushSystem();
	}

	m_IsEnabled = enable;
}

void CAISystem::SetActorProxyFactory(IAIActorProxyFactory* pFactory)
{
	m_actorProxyFactory = pFactory;
}

IAIActorProxyFactory* CAISystem::GetActorProxyFactory() const
{
	return m_actorProxyFactory;
}

void CAISystem::SetGroupProxyFactory(IAIGroupProxyFactory* pFactory)
{
	m_groupProxyFactory = pFactory;
}

IAIGroupProxyFactory* CAISystem::GetGroupProxyFactory() const
{
	return m_groupProxyFactory;
}

IAIGroupProxy* CAISystem::GetAIGroupProxy(int groupID)
{
	const Group& group = gAIEnv.pGroupManager->GetGroup(groupID);
	return group.GetProxy();
}

const ShapeMap& CAISystem::GetGenericShapes() const
{
	return m_mapGenericShapes;
}

const ShapeMap& CAISystem::GetOcclusionPlanes() const
{
	return m_mapOcclusionPlanes;
}

const CAISystem::TDamageRegions& CAISystem::GetDamageRegions() const
{
	return m_damageRegions;
}

CPerceptionManager* CAISystem::GetPerceptionManager()
{
	return gAIEnv.pPerceptionManager;
}

CAILightManager* CAISystem::GetLightManager()
{
	return &m_lightManager;
}

CAIDynHideObjectManager* CAISystem::GetDynHideObjectManager()
{
	return &m_dynHideObjectManager;
}

bool CAISystem::IsRecording(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event) const
{
#ifdef CRYAISYSTEM_DEBUG
	return m_DbgRecorder.IsRecording(pTarget, event);
#else
	return false;
#endif //CRYAISYSTEM_DEBUG
}

void CAISystem::Record(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString) const
{
#ifdef CRYAISYSTEM_DEBUG
	m_DbgRecorder.Record(pTarget, event, pString);
#endif //CRYAISYSTEM_DEBUG
}

void CAISystem::GetRecorderDebugContext(SAIRecorderDebugContext*& pContext)
{
	pContext = &m_recorderDebugContext;
}

IAIRecorder* CAISystem::GetIAIRecorder()
{
#ifdef CRYAISYSTEM_DEBUG
	return &m_Recorder;
#else
	return NULL;
#endif //CRYAISYSTEM_DEBUG
}

INavigation* CAISystem::GetINavigation()
{
	return m_pNavigation;
}

IAIPathFinder* CAISystem::GetIAIPathFinder()
{
	return NULL;
}

IMNMPathfinder* CAISystem::GetMNMPathfinder() const
{
	return gAIEnv.pMNMPathfinder;
}

IAIDebugRenderer* CAISystem::GetAIDebugRenderer()
{
	return gAIEnv.GetDebugRenderer();
}

IAIDebugRenderer* CAISystem::GetAINetworkDebugRenderer()
{
	return gAIEnv.GetNetworkDebugRenderer();
}

void CAISystem::SetAINetworkDebugRenderer(IAIDebugRenderer* pAINetworkDebugRenderer)
{
	gAIEnv.SetNetworkDebugRenderer(pAINetworkDebugRenderer);
}

void CAISystem::SetAIDebugRenderer(IAIDebugRenderer* pAIDebugRenderer)
{
	gAIEnv.SetDebugRenderer(pAIDebugRenderer);
}

const CAISystem::AIActorSet& CAISystem::GetEnabledAIActorSet() const
{
	return m_enabledAIActorsSet;
}

bool CAISystem::IsEnabled() const
{
	return m_IsEnabled;
}

IAIActionManager* CAISystem::GetAIActionManager()
{
	return m_pAIActionManager;
}

ISmartObjectManager* CAISystem::GetSmartObjectManager()
{
	return m_pSmartObjectManager;
}

IAIObjectManager* CAISystem::GetAIObjectManager()
{
	return &m_AIObjectManager;
}

void CAISystem::ResetAIActorSets(bool clearSets)
{
	AIAssert(!m_iteratingActorSet);
	m_iteratingActorSet = false;

	m_enabledActorsUpdateError = 0;
	m_enabledActorsUpdateHead = 0;

	m_totalActorsUpdateCount = 0;

	m_disabledActorsUpdateError = 0;
	m_disabledActorsHead = 0;

	if (clearSets)
	{
		stl::free_container(m_enabledAIActorsSet);
		stl::free_container(m_disabledAIActorsSet);
	}
}

void CAISystem::DetachFromTerritoryAllAIObjectsOfType(const char* szTerritoryName, unsigned short int nType)
{
	AIObjectOwners::iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(nType);
	for (; ai != gAIEnv.pAIObjectManager->m_Objects.end(); ++ai)
	{
		if (ai->first != nType)
			break;

		if (CAIActor* pAIActor = ai->second.GetAIObject()->CastToCAIActor())
		{
			if (CPipeUser* pPipeUser = pAIActor->CastToCPipeUser())
			{
				if (!strcmp(pPipeUser->GetRefShapeName(), szTerritoryName))
					pPipeUser->SetRefShapeName(0);
			}

			if (!strcmp(pAIActor->GetTerritoryShapeName(), szTerritoryName))
				pAIActor->SetTerritoryShapeName(0);
		}
	}
}

void CAISystem::UpdateCollisionAvoidance(const AIActorVector& agents, float updateTime)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	gAIEnv.pCollisionAvoidanceSystem->Reset();

	AIActorVector::const_iterator it = agents.begin();
	AIActorVector::const_iterator end = agents.end();

	float forcedSpeed = gAIEnv.CVars.DebugCollisionAvoidanceForceSpeed;
	bool useForcedSpeed = fabs_tpl(forcedSpeed) > 0.0001f;

	const float targetCutoff = gAIEnv.CVars.CollisionAvoidanceTargetCutoffRange;
	const float pathEndCutoff = gAIEnv.CVars.CollisionAvoidancePathEndCutoffRange;
	const float smartObjectCutoff = gAIEnv.CVars.CollisionAvoidanceSmartObjectCutoffRange;

	const size_t MaxAvoidingAgents = 512;
	CryFixedArray<CAIActor*, MaxAvoidingAgents> avoidingAgents;

	for (; it != end; ++it)
	{
		CAIActor* actor = *it;
		CPipeUser* pipeUser = actor->CastToCPipeUser();

		IF_UNLIKELY (!IsParticipatingInCollisionAvoidance(actor))
		{
			continue;
		}

		uint16 aiType = actor->GetAIType();

		bool isActor = (aiType == AIOBJECT_ALIENTICK) || (aiType == AIOBJECT_ACTOR) || (aiType == AIOBJECT_INFECTED);
		bool isMoving = (fabs_tpl(actor->m_State.fDesiredSpeed) > 0.0001f);
		bool cuttoff = (actor->m_State.fDistanceFromTarget < targetCutoff) ||
		               (actor->m_State.fDistanceToPathEnd < pathEndCutoff) ||
		               (pipeUser && pipeUser->GetPendingSmartObjectID() && (actor->m_State.fDistanceToPathEnd < smartObjectCutoff));
		bool isObstacle = (aiType == AIOBJECT_PLAYER);

		if (isActor && isMoving && !cuttoff && (avoidingAgents.size() < MaxAvoidingAgents))
		{
			float minSpeed;
			float maxSpeed;
			float normalSpeed;

			actor->GetMovementSpeedRange(actor->m_State.fMovementUrgency, false, normalSpeed, minSpeed, maxSpeed);

			CollisionAvoidanceSystem::Agent agent;
			agent.radius = actor->m_Parameters.m_fPassRadius + gAIEnv.CVars.CollisionAvoidanceAgentExtraFat;
			if (gAIEnv.CVars.CollisionAvoidanceEnableRadiusIncrement)
				agent.radius += actor->m_currentCollisionAvoidanceRadiusIncrement;
			agent.maxSpeed = min(actor->m_State.fDesiredSpeed, maxSpeed);
			agent.maxAcceleration = min(agent.maxAcceleration, actor->m_movementAbility.maxAccel);
			agent.currentLocation = actor->GetPhysicsPos();
			agent.currentVelocity = Vec2(actor->GetVelocity());

			agent.desiredVelocity = useForcedSpeed ? Vec2(actor->GetMoveDir() * forcedSpeed) :
			                        Vec2(actor->m_State.vMoveDir * actor->m_State.fDesiredSpeed);
			agent.currentLookDirection = Vec2(agent.desiredVelocity);

			CollisionAvoidanceSystem::AgentID agentID = gAIEnv.pCollisionAvoidanceSystem->CreateAgent(actor->GetAIObjectID());
			gAIEnv.pCollisionAvoidanceSystem->SetAgent(agentID, agent);

			avoidingAgents.push_back(actor);
		}
		else if (isObstacle || (isActor && (!isMoving || cuttoff)))
		{
			CollisionAvoidanceSystem::Obstacle obstacle;
			obstacle.currentLocation = actor->GetPhysicsPos();
			obstacle.currentVelocity = Vec2(actor->GetVelocity());
			obstacle.radius = actor->m_Parameters.m_fPassRadius + gAIEnv.CVars.CollisionAvoidanceAgentExtraFat;

			CollisionAvoidanceSystem::ObstacleID obstacleID = gAIEnv.pCollisionAvoidanceSystem->CreateObstable();
			gAIEnv.pCollisionAvoidanceSystem->SetObstacle(obstacleID, obstacle);
		}
	}

	gAIEnv.pCollisionAvoidanceSystem->Update(updateTime);

	if (gAIEnv.CVars.CollisionAvoidanceUpdateVelocities || gAIEnv.CVars.CollisionAvoidanceEnableRadiusIncrement)
	{
		size_t avoidingAgentCount = avoidingAgents.size();

		for (size_t i = 0; i < avoidingAgentCount; ++i)
		{
			CAIActor* actor = avoidingAgents[i];

			if (gAIEnv.CVars.CollisionAvoidanceUpdateVelocities)
			{
				actor->m_State.allowStrafing = false;
				actor->ResetBodyTargetDir();

				const Vec2 avoidanceVelocity = gAIEnv.pCollisionAvoidanceSystem->GetAvoidanceVelocity(i);

				const Vec3 currentVelocity(actor->m_State.vMoveDir * actor->m_State.fDesiredSpeed);
				const Vec3 avoidanceVelocity3D(avoidanceVelocity.x, avoidanceVelocity.y, currentVelocity.z);

				if ((avoidanceVelocity - Vec2(currentVelocity)).GetLength2() >= 0.000001f)
				{
					float speedSq = avoidanceVelocity3D.len2();

					if (actor->m_State.bodyOrientationMode != FullyTowardsAimOrLook)
					{
						actor->m_State.allowStrafing = true;
						actor->SetBodyTargetDir(actor->m_State.vMoveDir);
					}

					if (speedSq > 0.000001f)
					{
						float speed = sqrt_tpl(speedSq);

						actor->m_State.vMoveDir = avoidanceVelocity3D / speed;
						actor->m_State.fDesiredSpeed = speed;
					}
					else
					{
						actor->m_State.vMoveDir.zero();
						actor->m_State.fDesiredSpeed = 0.0f;
					}

					actor->m_State.vMoveTarget.zero();
				}
			}

			if (gAIEnv.CVars.CollisionAvoidanceEnableRadiusIncrement)
			{
				UpdateCollisionAvoidanceRadiusIncrement(actor, updateTime);
			}
		}
	}
}

void CAISystem::DummyFunctionNumberOne()
{
	cry_strcpy(gEnv->szDebugStatus, "dummyfunctionnumberone");
}

void CAISystem::UpdateCollisionAvoidanceRadiusIncrement(CAIActor* actor, float updateTime)
{
	if (actor->m_State.fDesiredSpeed > 0.5f)
	{
		actor->m_currentCollisionAvoidanceRadiusIncrement = min(
		  actor->m_currentCollisionAvoidanceRadiusIncrement + (actor->m_movementAbility.collisionAvoidanceRadiusIncrement * gAIEnv.CVars.CollisionAvoidanceRadiusIncrementIncreaseRate * updateTime),
		  actor->m_movementAbility.collisionAvoidanceRadiusIncrement
		  );
	}
	else
	{
		actor->m_currentCollisionAvoidanceRadiusIncrement = max(
		  actor->m_currentCollisionAvoidanceRadiusIncrement - (actor->m_movementAbility.collisionAvoidanceRadiusIncrement * gAIEnv.CVars.CollisionAvoidanceRadiusIncrementDecreaseRate * updateTime),
		  0.0f
		  );
	}
}

// ===========================================================================
//	Query if an actor should participate in collision avoidance.
//
//	In:		The actor (NULL will abort!)
//
//	Returns:	True if participating; otherwise false.
//
bool CAISystem::IsParticipatingInCollisionAvoidance(CAIActor* actor) const
{
	if (actor != NULL)
	{
		return actor->GetMovementAbility().collisionAvoidanceParticipation;
	}

	return false;
}

void CAISystem::CallReloadTPSQueriesScript()
{
	const HSCRIPTFUNCTION reloadTPS = gEnv->pScriptSystem->GetFunctionPtr("ReloadTPS");

	if (reloadTPS)
	{
		gEnv->pScriptSystem->BeginCall(reloadTPS);
		gEnv->pScriptSystem->EndCall();
	}
}
