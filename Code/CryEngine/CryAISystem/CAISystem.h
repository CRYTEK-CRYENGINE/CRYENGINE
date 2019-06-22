// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CAISYSTEM_H_
#define _CAISYSTEM_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAISystem.h>

typedef std::vector<Vec3> ListPositions;

#include "NavPath.h"
#include "ObjectContainer.h"
#include "Formation/Formation.h"
#include "PipeManager.h"
#include "AIObject.h"
#include "AICollision.h"
#include "AIGroup.h"
#include "AIQuadTree.h"
#include <CryCore/Containers/MiniQueue.h>
#include "AIRadialOcclusion.h"
#include "AILightManager.h"
#include "Shape.h"
#include "ShapeContainer.h"
#include "Shape2.h"
#include "Navigation.h"
#include "VisionMap.h"
#include "Group/Group.h"
#include "Factions/FactionSystem.h"
#include "AIObjectManager.h"
#include "GlobalPerceptionScaleHandler.h"
#include "ClusterDetector.h"
#include "ActorLookUp.h"
#include "Sequence/SequenceAgentAdapter.h"

#ifdef CRYAISYSTEM_DEBUG
	#include "AIDbgRecorder.h"
	#include "AIRecorder.h"
#endif //CRYAISYSTEM_DEBUG

#define AIFAF_VISIBLE_FROM_REQUESTER 0x0001   // Requires whoever is requesting the anchor to also have a line of sight to it.
//#define AIFAF_VISIBLE_TARGET					0x0002  // Requires that there is a line of sight between target and anchor.
#define AIFAF_INCLUDE_DEVALUED       0x0004   // Don't care if the object is devalued.
#define AIFAF_INCLUDE_DISABLED       0x0008   // Don't care if the object is disabled.
#define AIFAF_HAS_FORMATION          0x0010   // Want only formations owners.
#define AIFAF_LEFT_FROM_REFPOINT     0x0020   // Requires that the anchor is left from the target.
#define AIFAF_RIGHT_FROM_REFPOINT    0x0040   // Requires that the anchor is right from the target.
#define AIFAF_INFRONT_OF_REQUESTER   0x0080   // Requires that the anchor is within a 30-degree cone in front of requester.
#define AIFAF_SAME_GROUP_ID          0x0100   // Want same group id.
#define AIFAF_DONT_DEVALUE           0x0200   // Do not devalue the object after having found it.
#define AIFAF_USE_REFPOINT_POS       0x0400   // Use ref point position instead of AI object pos.

struct ICoordinationManager;
struct ITacticalPointSystem;
struct AgentPathfindingProperties;
struct IFireCommandDesc;
struct IVisArea;

namespace AISignals
{
	struct IAISignalExtraData;
}

class CAIActionManager;
class ICentralInterestManager;

class CScriptBind_AI;

namespace Schematyc
{
	struct IEnvRegistrar;
}

#define AGENT_COVER_CLEARANCE 0.35f

const static int NUM_ALERTNESS_COUNTERS = 4;

enum EPuppetUpdatePriority
{
	AIPUP_VERY_HIGH,
	AIPUP_HIGH,
	AIPUP_MED,
	AIPUP_LOW,
};

enum AsyncState
{
	AsyncFailed = 0,
	AsyncReady,
	AsyncInProgress,
	AsyncComplete,
};

// Description:
//	 Fire command handler descriptor/factory.
struct IFireCommandDesc
{
	virtual ~IFireCommandDesc(){}
	//	 Returns the name identifier of the handler.
	virtual const char* GetName() = 0;
	// Summary:
	//	 Creates new instance of a fire command handler.
	virtual IFireCommandHandler* Create(IAIActor* pShooter) = 0;
	// Summary:
	//	 Deletes the factory.
	virtual void Release() = 0;
};

SERIALIZATION_ENUM_BEGIN_NESTED(IAISystem, ESubsystemUpdateFlag, "ESubsystemUpdateFlag")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::AuditionMap, "AuditionMap", "Audition Map")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::BehaviorTreeManager, "BehaviorTreeManager", "Behavior Tree Manager")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::ClusterDetector, "ClusterDetector", "Cluster Detector")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::CoverSystem, "CoverSystem", "Cover System")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::MovementSystem, "MovementSystem", "Movement System")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::NavigationSystem, "NavigationSystem", "Navigation System")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::GlobalIntersectionTester, "GlobalIntersectionTester", "Global Intersection Tester")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::GlobalRaycaster, "GlobalRaycaster", "Global Raycaster")
SERIALIZATION_ENUM(IAISystem::ESubsystemUpdateFlag::VisionMap, "VisionMap", "Vision Map")
SERIALIZATION_ENUM_END()

//====================================================================
// CAISystemCallbacks
//====================================================================
class CAISystemCallbacks : public IAISystemCallbacks
{
public:
	virtual CFunctorsList<Functor1<IAIObject*>>&         ObjectCreated()       { return m_objectCreated; }
	virtual CFunctorsList<Functor1<IAIObject*>>&         ObjectRemoved()       { return m_objectRemoved; }
	virtual CFunctorsList<Functor2<tAIObjectID, bool>>&  EnabledStateChanged() { return m_enabledStateChanged; }
	virtual CFunctorsList<Functor2<EntityId, EntityId>>& AgentDied()           { return m_agentDied; }

private:
	CFunctorsList<Functor1<IAIObject*>>         m_objectCreated;
	CFunctorsList<Functor1<IAIObject*>>         m_objectRemoved;
	CFunctorsList<Functor2<tAIObjectID, bool>>  m_enabledStateChanged;
	CFunctorsList<Functor2<EntityId, EntityId>> m_agentDied;
};

//====================================================================
// CAISystem
//====================================================================
class CAISystem :
	public IAISystem,
	public ISystemEventListener
{
public:
	//-------------------------------------------------------------

	/// Flags used by the GetDangerSpots.
	enum EDangerSpots
	{
		DANGER_DEADBODY       = 0x01, // Include dead bodies.
		DANGER_EXPLOSIVE      = 0x02, // Include explosives - unexploded grenades.
		DANGER_EXPLOSION_SPOT = 0x04, // Include recent explosions.
		DANGER_ALL            = DANGER_DEADBODY | DANGER_EXPLOSIVE,
	};

	CAISystem(ISystem* pSystem);
	~CAISystem();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//IAISystem/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Basic////////////////////////////////////////////////////////////////////////////////////////////////////////

	virtual bool                Init() override;

	virtual void                Reload() override;
	virtual void                Reset(EResetReason reason) override;//TODO this is called by lots of people including destructor, but causes NEW ALLOCATIONS! Needs big refactor!
	virtual void                Release() override;

	virtual IAISystemCallbacks& Callbacks() override { return m_callbacks; }

	virtual void                DummyFunctionNumberOne(void) override;

	//If disabled most things early out
	virtual void                                  Enable(bool enable = true) override;
	virtual void                                  SetActorProxyFactory(IAIActorProxyFactory* pFactory) override;
	virtual IAIActorProxyFactory*                 GetActorProxyFactory() const override;
	virtual void                                  SetGroupProxyFactory(IAIGroupProxyFactory* pFactory) override;
	virtual IAIGroupProxyFactory*                 GetGroupProxyFactory() const override;
	virtual IAIGroupProxy*                        GetAIGroupProxy(int groupID) override;

	virtual IActorLookUp*                         GetActorLookup() override { return gAIEnv.pActorLookUp; }

	virtual IAISystem::GlobalRayCaster*           GetGlobalRaycaster() override { return gAIEnv.pRayCaster; }
	virtual IAISystem::GlobalIntersectionTester*  GetGlobalIntersectionTester() override { return gAIEnv.pIntersectionTester; }

	//Every frame (multiple time steps per frame possible?)		//TODO find out
	//	currentTime - AI time since game start in seconds (GetCurrentTime)
	//	frameTime - since last update (GetFrameTime)
	virtual void                                  Update(const CTimeValue currentTime, const float frameTime) override;
	virtual void                                  UpdateSubsystem(const CTimeValue currentTime, const float frameTime, const ESubsystemUpdateFlag subsystemUpdateFlag) override;

	virtual bool                                  RegisterSystemComponent(IAISystemComponent* pComponent) override;
	virtual bool                                  UnregisterSystemComponent(IAISystemComponent* pComponent) override;
												  
	void                                          OnAgentDeath(EntityId deadEntityID, EntityId killerID);
												  
	void                                          OnAIObjectCreated(CAIObject* pObject);
	void                                          OnAIObjectRemoved(CAIObject* pObject);
												  
	virtual void                                  Event(int eventT, const char*) override;
	virtual void                                  SendSignal(unsigned char cFilter, const AISignals::SignalSharedPtr& pSignal) override;
	virtual void                                  SendAnonymousSignal(const AISignals::SignalSharedPtr& pSignal, const Vec3& pos, float radius) override;
	virtual AISignals::IAISignalExtraData*        CreateSignalExtraData() const override;
	virtual void                                  FreeSignalExtraData(AISignals::IAISignalExtraData* pData) const override;
	virtual void                                  OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

	//Basic////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Subsystem updates///////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// These subsystems are always updated in the Update(...) function
		//Subsystem updates///////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void SubsystemUpdateActionManager();
	void SubsystemUpdateRadialOcclusionRaycast();
	void SubsystemUpdateLightManager();
	void SubsystemUpdateNavigation();
	void SubsystemUpdateBannedSOs();
	void SubsystemUpdateSystemComponents();
	void SubsystemUpdateCommunicationManager();
	void SubsystemUpdateGroupManager();
	void SubsystemUpdatePlayers();
	void SubsystemUpdateGroups();
	void SubsystemUpdateLeaders();
	void SubsystemUpdateSmartObjectManager();
	void SubsystemUpdateInterestManager();
	void SubsystemUpdateTacticalPointSystem();
	void SubsystemUpdateAmbientFire();
	void SubsystemUpdateExpensiveAccessoryQuota();
	void SubsystemUpdateActorsAndTargetTrackAndORCA();
	void SubsystemUpdateTargetTrackManager();
	void SubsystemUpdateCollisionAvoidanceSystem();

	// These subsystems are updated in the Update(...) function if the respective subsystem flag is not set in m_overrideUpdateFlags
	// If it's set the subsystem will not get updated when executing Update(...) function but
	// may get updated via UpdateSubsystem with the specific flag from Game code

	void TrySubsystemUpdateVisionMap(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateAuditionMap(const CTimeValue frameStartTime,const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateCoverSystem(const CTimeValue frameStartTime,const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateNavigationSystem(const CTimeValue frameStartTime, const float frameDeltaTime,const bool isAutomaticUpdate);

	void TrySubsystemUpdateMovementSystem(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateGlobalRayCaster(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateGlobalIntersectionTester(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateClusterDetector(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate);
	void TrySubsystemUpdateBehaviorTreeManager(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate);
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Time/Updates/////////////////////////////////////////////////////////////////////////////////////////////////

	virtual CEnumFlags<ESubsystemUpdateFlag>& GetOverrideUpdateFlags() override
	{
		return m_overrideUpdateFlags;
	}

	//Over-ride auto-disable for distant AIs
	virtual bool GetUpdateAllAlways() const override;

	// Returns the current time (seconds since game began) that AI should be working with -
	// This may be different from the system so that we can support multiple updates per
	// game loop/update.
	ILINE CTimeValue GetFrameStartTime() const
	{
		return m_frameStartTime;
	}

	ILINE float GetFrameStartTimeSeconds() const
	{
		return m_frameStartTimeSeconds;
	}

	// Time interval between this and the last update
	ILINE float GetFrameDeltaTime() const
	{
		return m_frameDeltaTime;
	}

	// returns the basic AI system update interval
	virtual float GetUpdateInterval() const override;

	// profiling
	virtual int GetAITickCount() override;

	//Time/Updates/////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//FileIO///////////////////////////////////////////////////////////////////////////////////////////////////////

	// save/load
	virtual void Serialize(TSerialize ser) override;
	virtual void SerializeObjectIDs(TSerialize ser) override;

	//! Set a path for the current level as working folder for level-specific metadata
	virtual void SetLevelPath(const char* sPath) override;

	/// this called before loading (level load/serialization)
	virtual void FlushSystem(bool bDeleteAll = false) override;
	virtual void FlushSystemNavigation(bool bDeleteAll = false) override;

	virtual void LayerEnabled(const char* layerName, bool enabled, bool serialized) override;

	// reads areas from file. clears the existing areas
	virtual void ReadAreasFromFile(const char* fileNameAreas) override;

	virtual void LoadLevelData(const char* szLevel, const char* szMission, const EAILoadDataFlags loadDataFlags = eAILoadDataFlag_AllSystems) override;
	virtual void LoadNavigationData(const char* szLevel, const char* szMission, const EAILoadDataFlags loadDataFlags = eAILoadDataFlag_AllSystems) override;

	virtual void OnMissionLoaded() override;

	//FileIO///////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Debugging////////////////////////////////////////////////////////////////////////////////////////////////////

	// AI DebugDraw
	virtual IAIDebugRenderer* GetAIDebugRenderer() override;
	virtual IAIDebugRenderer* GetAINetworkDebugRenderer() override;
	virtual void              SetAIDebugRenderer(IAIDebugRenderer* pAIDebugRenderer) override;
	virtual void              SetAINetworkDebugRenderer(IAIDebugRenderer* pAINetworkDebugRenderer) override;

	virtual void              SetAgentDebugTarget(const EntityId id) override { m_agentDebugTarget = id; }
	virtual EntityId          GetAgentDebugTarget() const override            { return m_agentDebugTarget; }

	virtual IAIBubblesSystem* GetAIBubblesSystem() override;

	// debug recorder
	virtual bool IsRecording(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event) const override;
	virtual void Record(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString) const override;
	virtual void GetRecorderDebugContext(SAIRecorderDebugContext*& pContext) override;
	virtual void AddDebugLine(const Vec3& start, const Vec3& end, uint8 r, uint8 g, uint8 b, float time) override;
	virtual void AddDebugSphere(const Vec3& pos, float radius, uint8 r, uint8 g, uint8 b, float time) override;

	virtual void DebugReportHitDamage(IEntity* pVictim, IEntity* pShooter, float damage, const char* material) override;
	virtual void DebugReportDeath(IAIObject* pVictim) override;

	// functions to let external systems (e.g. lua) access the AI logging functions.
	// the external system should pass in an identifier (e.g. "<Lua> ")
	virtual void Warning(const char* id, const char* format, ...) const override PRINTF_PARAMS(3, 4);
	virtual void Error(const char* id, const char* format, ...) override PRINTF_PARAMS(3, 4);
	virtual void LogProgress(const char* id, const char* format, ...) override PRINTF_PARAMS(3, 4);
	virtual void LogEvent(const char* id, const char* format, ...) override PRINTF_PARAMS(3, 4);
	virtual void LogComment(const char* id, const char* format, ...) override PRINTF_PARAMS(3, 4);

	virtual void GetMemoryStatistics(ICrySizer* pSizer) override;

	// debug members ============= DO NOT USE
	virtual void DebugDraw() override;

	//Debugging////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Get Subsystems///////////////////////////////////////////////////////////////////////////////////////////////
	virtual IAIRecorder*                        GetIAIRecorder() override;
	virtual INavigation*                        GetINavigation() override;
	virtual IMNMPathfinder*                     GetMNMPathfinder() const override;
	virtual ICentralInterestManager*            GetCentralInterestManager(void) override;
	virtual ICentralInterestManager const*      GetCentralInterestManager(void) const override;
	virtual ITacticalPointSystem*               GetTacticalPointSystem(void) override;
	virtual ICommunicationManager*              GetCommunicationManager() const override;
	virtual ICoverSystem*                       GetCoverSystem() const override;
	virtual INavigationSystem*                  GetNavigationSystem() const override;
    virtual Cry::AI::CollisionAvoidance::ISystem* GetCollisionAvoidanceSystem() const override;
	virtual BehaviorTree::IBehaviorTreeManager* GetIBehaviorTreeManager() const override;
	virtual ITargetTrackManager*                GetTargetTrackManager() const override;
	virtual struct IMovementSystem*             GetMovementSystem() const override;
	virtual AIActionSequence::ISequenceManager* GetSequenceManager() const override;
	virtual IClusterDetector*                   GetClusterDetector() const override;
	virtual AISignals::ISignalManager*          GetSignalManager() const override;

	virtual ISmartObjectManager*                GetSmartObjectManager() override;
	virtual IAIObjectManager*                   GetAIObjectManager() override;
	//Get Subsystems///////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//AI Actions///////////////////////////////////////////////////////////////////////////////////////////////////
	virtual IAIActionManager* GetAIActionManager() override;
	//AI Actions///////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Leader/Formations////////////////////////////////////////////////////////////////////////////////////////////
	virtual void       EnumerateFormationNames(unsigned int maxNames, const char** names, unsigned int* nameCount) const override;
	virtual int        GetGroupCount(int nGroupID, int flags = GROUP_ALL, int type = 0) override;
	virtual IAIObject* GetGroupMember(int groupID, int index, int flags = GROUP_ALL, int type = 0) override;
	//Leader/Formations////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Goal Pipes///////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: get rid of this; => it too many confusing uses to remove just yet
	virtual int AllocGoalPipeId() const override;
	//Goal Pipes///////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Navigation / Pathfinding/////////////////////////////////////////////////////////////////////////////////////
	virtual bool CreateNavigationShape(const SNavigationShapeParams& params) override;
	virtual void DeleteNavigationShape(const char* szPathName) override;
	virtual bool DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road = false) override;
	virtual void EnableGenericShape(const char* shapeName, bool state) override;

	//Temporary - move to perception system in the future
	virtual int RayOcclusionPlaneIntersection(const Vec3& start, const Vec3& end) override;

	const char* GetEnclosingGenericShapeOfType(const Vec3& pos, int type, bool checkHeight);
	bool        IsPointInsideGenericShape(const Vec3& pos, const char* shapeName, bool checkHeight);
	float       DistanceToGenericShape(const Vec3& pos, const char* shapeName, bool checkHeight);
	bool        ConstrainInsideGenericShape(Vec3& pos, const char* shapeName, bool checkHeight);
	const char* CreateTemporaryGenericShape(Vec3* points, int npts, float height, int type);

	// Pathfinding properties
	virtual void                              AssignPFPropertiesToPathType(const string& sPathType, const AgentPathfindingProperties& properties) override;
	virtual const AgentPathfindingProperties* GetPFPropertiesOfPathType(const string& sPathType) override;
	virtual string                            GetPathTypeNames() override;

	/// Register a spherical region that causes damage (so should be avoided in pathfinding). pID is just
	/// a unique identifying - so if this is called multiple times with the same pID then the damage region
	/// will simply be moved. If radius <= 0 then the region is disabled.
	virtual void RegisterDamageRegion(const void* pID, const Sphere& sphere) override;

	//Navigation / Pathfinding/////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Hide spots///////////////////////////////////////////////////////////////////////////////////////////////////

	// Returns a point which is a valid distance away from a wall in front of the point.
	virtual void AdjustDirectionalCoverPosition(Vec3& pos, const Vec3& dir, float agentRadius, float testHeight) override;

	//Hide spots///////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Perception///////////////////////////////////////////////////////////////////////////////////////////////////

	//TODO PerceptionManager://Some stuff in this section maps to that..

	// current global AI alertness value (what's the most alerted puppet)
	virtual int          GetAlertness() const override;
	virtual int          GetAlertness(const IAIAlertnessPredicate& alertnessPredicate) override;
	virtual void         SetPerceptionDistLookUp(float* pLookUpTable, int tableSize) override; //look up table to be used when calculating visual time-out increment
	// Global perception scale handler functionalities
	virtual void         UpdateGlobalPerceptionScale(const float visualScale, const float audioScale, EAIFilterType filterType = eAIFT_All, const char* factionName = NULL) override;
	virtual float        GetGlobalVisualScale(const IAIObject* pAIObject) const override;
	virtual float        GetGlobalAudioScale(const IAIObject* pAIObject) const override;
	virtual void         DisableGlobalPerceptionScaling() override;
	virtual void         RegisterGlobalPerceptionListener(IAIGlobalPerceptionListener* pListner) override;
	virtual void         UnregisterGlobalPerceptionlistener(IAIGlobalPerceptionListener* pListner) override;
	/// Fills the array with possible dangers, returns number of dangers.
	virtual unsigned int GetDangerSpots(const IAIObject* requester, float range, Vec3* positions, unsigned int* types, unsigned int n, unsigned int flags) override;

	virtual void         DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, EntityId shooterId, float time = 5.0f) override;
	virtual void         DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, EntityId shooterId, float time = 5.0f) override;
	virtual IAuditionMap* GetAuditionMap() override;
	virtual IVisionMap*  GetVisionMap() override { return gAIEnv.pVisionMap; }
	virtual IFactionMap& GetFactionMap() override { return *gAIEnv.pFactionMap; }

	//Perception///////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//WTF are these?///////////////////////////////////////////////////////////////////////////////////////////////
	virtual IAIObject*           GetBeacon(unsigned short nGroupID) override;
	virtual void                 UpdateBeacon(unsigned short nGroupID, const Vec3& vPos, IAIObject* pOwner = 0) override;

	IFireCommandHandler* CreateFirecommandHandler(const char* name, IAIActor* pShooter);

	virtual bool                 ParseTables(int firstTable, bool parseMovementAbility, IFunctionHandler* pH, AIObjectParams& aiParams, bool& updateAlways) override;

	void                         AddCombatClass(int combatClass, float* pScalesVector, int size, const char* szCustomSignal);
	float                        ProcessBalancedDamage(IEntity* pShooterEntity, IEntity* pTargetEntity, float damage, const char* damageType);
	void                         NotifyDeath(IAIObject* pVictim);

	// !!! added to resolve merge conflict: to be removed in dev/c2 !!!
	virtual float GetFrameStartTimeSecondsVirtual() const override
	{
		return GetFrameStartTimeSeconds();
	}

	//IAISystem/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Light frame profiler for AI support
	// Add nTicks to the number of Ticks spend this frame in particle functions
	virtual void AddFrameTicks(uint64 nTicks) override { m_nFrameTicks += nTicks; }

	// Reset Ticks Counter
	virtual void ResetFrameTicks() override { m_nFrameTicks = 0; }

	// Get number of Ticks accumulated over this frame
	virtual uint64 NumFrameTicks() const override { return m_nFrameTicks; }

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//CAISystem/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	CAILightManager*         GetLightManager();

	bool                     InitSmartObjects();

	void InvalidatePathsThroughArea(const ListPositions& areaShape);

	typedef VectorSet<CWeakRef<CAIActor>> AIActorSet;
	typedef std::vector<CAIActor*>        AIActorVector;

	bool       GetNearestPunchableObjectPosition(IAIObject* pRef, const Vec3& searchPos, float searchRad, const Vec3& targetPos, float minSize, float maxSize, float minMass, float maxMass, Vec3& posOut, Vec3& dirOut, IEntity** objEntOut);
	void       DumpStateOf(IAIObject* pObject);
	IAIObject* GetBehindObjectInRange(IAIObject* pRef, unsigned short nType, float fRadiusMin, float fRadiusMax);
	IAIObject* GetRandomObjectInRange(IAIObject* pRef, unsigned short nType, float fRadiusMin, float fRadiusMax, bool bFaceAttTarget = false);
	IAIObject* GetNearestObjectOfTypeInRange(const Vec3& pos, unsigned int type, float fRadiusMin, float fRadiusMax, IAIObject* pSkip = NULL, int nOption = 0);
	IAIObject* GetNearestObjectOfTypeInRange(IAIObject* pObject, unsigned int type, float fRadiusMin, float fRadiusMax, int nOption = 0);

	//// Iterates over AI objects within specified shape.
	//// Parameter 'name' specify the enclosing shape and parameter 'checkHeight' specifies if hte height of the shape is taken into account too,
	//// for other parameters see GetFirstAIObject.
	IAIObjectIter* GetFirstAIObjectInShape(EGetFirstFilter filter, short n, const char* shapeName, bool checkHeight);
	IAIObject*     GetNearestToObjectInRange(IAIObject* pRef, unsigned short nType, float fRadiusMin, float fRadiusMax, float inCone = -1, bool bFaceAttTarget = false, bool bSeesAttTarget = false, bool bDevalue = true);

	//// Devalues an AI object for the refence object only or for whole group.
	void                  Devalue(IAIObject* pRef, IAIObject* pObject, bool group, float fDevalueTime = 20.f);

	CAIObject*            GetPlayer() const;

	void                  NotifyEnableState(CAIActor* pAIActor, bool state);
	EPuppetUpdatePriority CalcPuppetUpdatePriority(CPuppet* pPuppet) const;

	//// non-virtual, accessed from CAIObject.
	//// notifies that AIObject has changed its position, which is important for smart objects
	void                                   NotifyAIObjectMoved(IEntity* pEntity, SEntityEvent event);
	virtual void                           NotifyTargetDead(IAIObject* pDeadObject) override;

	virtual std::shared_ptr<IPathFollower> CreateAndReturnNewDefaultPathFollower(const PathFollowerParams& params, const IPathObstacles& pathObstacleObject) override;
	virtual std::shared_ptr<INavPath>      CreateAndReturnNewNavPath() override;

	const AIActorSet& GetEnabledAIActorSet() const;

	CFactionSystem*   GetFactionSystem() { return gAIEnv.pFactionSystem; }
	void              AddToFaction(CAIObject* pObject, uint8 factionID);
	void              OnFactionReactionChanged(uint8 factionOne, uint8 factionTwo, IFactionMap::ReactionType reaction);

	IAIObject*        GetLeaderAIObject(int iGroupId);
	IAIObject*        GetLeaderAIObject(IAIObject* pObject);
	IAIGroup*         GetIAIGroup(int groupid);
	void              SetLeader(IAIObject* pObject);
	CLeader*          GetLeader(int nGroupID);
	CLeader*          GetLeader(const CAIActor* pSoldier);
	CLeader*          CreateLeader(int nGroupID);
	CAIGroup*         GetAIGroup(int nGroupID);
	// removes specified object from group
	void              RemoveFromGroup(int nGroupID, CAIObject* pObject);
	// adds an object to a group
	void              AddToGroup(CAIActor* pObject, int nGroupId = -1);
	int               GetBeaconGroupId(CAIObject* pBeacon);
	void              UpdateGroupStatus(int groupId);

	void              FlushAllAreas();

	// offsets all AI areas when the segmented world shifts.
	void            OffsetAllAreas(const Vec3& additionalOffset);

	SShape*         GetGenericShapeOfName(const char* shapeName);
	const ShapeMap& GetGenericShapes() const;

	/// Indicates if a human would be visible if placed at the specified position
	/// If fullCheck is false then only a camera frustum check will be done. If true
	/// then more precise (perhaps expensive) checking will be done.
	bool            WouldHumanBeVisible(const Vec3& footPos, bool fullCheck) const;

	const ShapeMap& GetOcclusionPlanes() const;

	bool            CheckPointsVisibility(const Vec3& from, const Vec3& to, float rayLength, IPhysicalEntity* pSkipEnt = 0, IPhysicalEntity* pSkipEntAux = 0);
	bool            CheckObjectsVisibility(const IAIObject* pObj1, const IAIObject* pObj2, float rayLength);
	float           GetVisPerceptionDistScale(float distRatio); //distRatio (0-1) 1-at sight range
	float           GetRayPerceptionModifier(const Vec3& start, const Vec3& end, const char* actorName);

	// returns value in range [0, 1]
	// calculate how much targetPos is obstructed by water
	// takes into account water depth and density
	float GetWaterOcclusionValue(const Vec3& targetPos) const;

	bool  CheckVisibilityToBody(CAIActor* pObserver, CAIActor* pBody, float& closestDistSq, IPhysicalEntity* pSkipEnt = 0);

	void AdjustOmniDirectionalCoverPosition(Vec3& pos, Vec3& dir, float hideRadius, float agentRadius, const Vec3& hideFrom, const bool hideBehind = true);

	//combat classes scale
	float GetCombatClassScale(int shooterClass, int targetClass);

	typedef std::map<const void*, Sphere> TDamageRegions;
	/// Returns the regions game code has flagged as causing damage
	const TDamageRegions& GetDamageRegions() const;

	static void           ReloadConsoleCommand(IConsoleCmdArgs*);
	static void           CheckGoalpipes(IConsoleCmdArgs*);
	static void           StartAIRecorder(IConsoleCmdArgs*);
	static void           StopAIRecorder(IConsoleCmdArgs*);

	// Clear out AI system for clean script reload
	void ClearForReload(void);

	void SetupAIEnvironment();
	void SetAIHacksConfiguration();

	void TrySubsystemInitCommunicationSystem();
	void TrySubsystemInitScriptBind();
	void TrySubsystemInitFormationSystem();
	void TrySubsystemInitTacticalPointSystem();
	void TrySubsystemInitCoverSystem();
	void TrySubsystemInitGroupSystem();
	void TrySubsystemInitORCA();
	void TrySubsystemInitTargetTrackSystem();

	/// Our own internal serialisation - just serialise our state (but not the things
	/// we own that are capable of serialising themselves)
	void SerializeInternal(TSerialize ser);

	void SingleDryUpdate(CAIActor* pAIActor);

	// just steps through objects - for debugging
	void         DebugOutputObjects(const char* txt) const;

	virtual bool IsEnabled() const override;

	void         UnregisterAIActor(CWeakRef<CAIActor> destroyedObject);

	//! Return array of pairs position - navigation agent type. When agent type is 0, position is used for all navmesh layers.
	void GetNavigationSeeds(std::vector<std::pair<Vec3, NavigationAgentTypeID>>& seeds);

	struct SObjectDebugParams
	{
		Vec3 objectPos;
		Vec3 entityPos;
		EntityId entityId;
	};

	bool GetObjectDebugParamsFromName(const char* szObjectName, SObjectDebugParams& outParams);

	///////////////////////////////////////////////////
	IAIActorProxyFactory* m_actorProxyFactory;
	IAIGroupProxyFactory* m_groupProxyFactory;
	CAIObjectManager      m_AIObjectManager;
	CPipeManager          m_PipeManager;
	CNavigation*          m_pNavigation;
	CAIActionManager*     m_pAIActionManager;
	CSmartObjectManager*  m_pSmartObjectManager;
	bool                  m_bUpdateSmartObjects;
	bool                  m_IsEnabled;//TODO eventually find how to axe this!
	///////////////////////////////////////////////////

	AIObjects m_mapGroups;
	AIObjects m_mapFaction;

	// This map stores the AI group info.
	typedef std::map<int, CAIGroup*> AIGroupMap;
	AIGroupMap m_mapAIGroups;

	//AIObject Related Data structs:
	AIActorSet m_enabledAIActorsSet;  // Set of enabled AI Actors
	AIActorSet m_disabledAIActorsSet; // Set of disabled AI Actors
	float      m_enabledActorsUpdateError;
	int        m_enabledActorsUpdateHead;
	int        m_totalActorsUpdateCount;
	float      m_disabledActorsUpdateError;
	int        m_disabledActorsHead;
	bool       m_iteratingActorSet;

	AIActionSequence::SequenceAgentAdapter m_sequenceAgentAdapter;

	struct BeaconStruct
	{
		CCountedRef<CAIObject> refBeacon;
		CWeakRef<CAIObject>    refOwner;
	};
	typedef std::map<unsigned short, BeaconStruct> BeaconMap;
	BeaconMap m_mapBeacons;

	//AIObject Related Data structs:
	///////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////
	//Subsystems
	CAILightManager         m_lightManager;
	SAIRecorderDebugContext m_recorderDebugContext;
	//Subsystems
	////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////
	//pathfinding data structures
	typedef std::map<string, AgentPathfindingProperties> PFPropertiesMap;
	PFPropertiesMap mapPFProperties;
	ShapeMap        m_mapOcclusionPlanes;
	ShapeMap        m_mapGenericShapes;
	////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////
	//scripting data structures
	CScriptBind_AI*                     m_pScriptAI;

	std::vector<const IPhysicalEntity*> m_walkabilityPhysicalEntities;
	IGeometry*                          m_walkabilityGeometryBox;

	CAISystemCallbacks                  m_callbacks;

	////////////////////////////////////////////////////////////////////
	//system components
	typedef VectorSet<IAISystemComponent*> SystemComponentsSet;
	SystemComponentsSet m_setSystemComponents;
	//system listeners
	////////////////////////////////////////////////////////////////////

	typedef std::map<int, float> MapMultipliers;
	MapMultipliers m_mapMultipliers;
	MapMultipliers m_mapFactionThreatMultipliers;

	//<<FIXME>> just used for profiling:
	//typedef std::map<tAIObjectID,float> TimingMap;
	//TimingMap m_mapDEBUGTiming;

	//typedef std::map<string,float> NamedTimingMap;
	//NamedTimingMap m_mapDEBUGTimingGOALS;

	int m_AlertnessCounters[NUM_ALERTNESS_COUNTERS];

	// (MATT) For now I bloat CAISystem with this, but it should move into some new struct of Environment.
	// Stores level path for metadata - i.e. the code coverage data files {2009/02/17}
	string                         m_sWorkingFolder;

	float                          m_DEBUG_screenFlash;

	bool                           m_bCodeCoverageFailed;

	unsigned int                   m_nTickCount;
	bool                           m_bInitialized;

	IVisArea*                      m_pAreaList[100];

	float                          m_frameDeltaTime;
	float                          m_frameStartTimeSeconds;
	CTimeValue                     m_fLastPuppetUpdateTime;
	CTimeValue                     m_frameStartTime;
	CTimeValue                     m_lastVisBroadPhaseTime;
	CTimeValue                     m_lastAmbientFireUpdateTime;
	CTimeValue                     m_lastExpensiveAccessoryUpdateTime;
	CTimeValue                     m_lastGroupUpdateTime;

	PerceptionModifierShapeMap     m_mapPerceptionModifiers;

	std::vector<IFireCommandDesc*> m_firecommandDescriptors;

	TDamageRegions                 m_damageRegions;

	struct SAIDelayedExpAccessoryUpdate
	{
		SAIDelayedExpAccessoryUpdate(CPuppet* pPuppet, int timeMs, bool state)
			: pPuppet(pPuppet)
			, timeMs(timeMs)
			, state(state)
		{
		}
		CPuppet* pPuppet;
		int      timeMs;
		bool     state;
	};
	std::vector<SAIDelayedExpAccessoryUpdate> m_delayedExpAccessoryUpdates;

	// combat classes
	// vector of target selection scale multipliers
	struct SCombatClassDesc
	{
		std::vector<float> mods;
		string             customSignal;
	};
	std::vector<SCombatClassDesc> m_CombatClasses;

	class AILinearLUT
	{
		int    m_size;
		float* m_pData;

	public:
		AILinearLUT() : m_size(0), m_pData(0) {}
		~AILinearLUT()
		{
			if (m_pData)
				delete[] m_pData;
		}

		// Returns the size of the table.
		int   GetSize() const             { return m_size; }
		// Returns the value is specified sample.
		float GetSampleValue(int i) const { return m_pData[i]; }

		/// Set the lookup table from a array of floats.
		void Set(float* values, int n)
		{
			delete[] m_pData;
			m_size = n;
			m_pData = new float[n];
			for (int i = 0; i < n; ++i) m_pData[i] = values[i];
		}

		// Returns linearly interpolated value, t in range [0..1]
		float GetValue(float t) const
		{
			const int last = m_size - 1;

			// Convert 't' to a sample.
			t *= (float)last;
			const int n = (int)floorf(t);

			// Check for out of range cases.
			if (n < 0) return m_pData[0];
			if (n >= last) return m_pData[last];

			// Linear interpolation between the two adjacent samples.
			const float a = t - (float)n;
			return m_pData[n] + (m_pData[n + 1] - m_pData[n]) * a;
		}
	};

	AILinearLUT m_VisDistLookUp;

	CEnumFlags<IAISystem::ESubsystemUpdateFlag> m_overrideUpdateFlags;

	////////////////////////////////////////////////////////////////////
	//Debugging / Logging subsystems

#ifdef CRYAISYSTEM_DEBUG
	CAIDbgRecorder m_DbgRecorder;
	CAIRecorder    m_Recorder;

	struct SPerceptionDebugLine
	{
		SPerceptionDebugLine(const char* name_, const Vec3& start_, const Vec3& end_, const ColorB& color_, float time_, float thickness_)
			: start(start_)
			, end(end_)
			, color(color_)
			, time(time_)
			, thickness(thickness_)
		{
			cry_strcpy(name, name_);
		}

		Vec3   start, end;
		ColorB color;
		float  time;
		float  thickness;
		char   name[64];
	};
	std::list<SPerceptionDebugLine> m_lstDebugPerceptionLines;

	struct SDebugFakeDamageInd
	{
		SDebugFakeDamageInd(const Vec3& pos, float t) : p(pos), t(t), tmax(t) {}
		std::vector<Vec3> verts;
		Vec3              p;
		float             t, tmax;
	};
	std::vector<SDebugFakeDamageInd> m_DEBUG_fakeDamageInd;

	void TryUpdateDebugFakeDamageIndicators();

	void DebugDrawEnabledActors();
	void DebugDrawEnabledPlayers() const;
	void DebugDrawUpdate() const;
	void DebugDrawFakeDamageInd() const;
	void DebugDrawPlayerRanges() const;
	void DebugDrawPerceptionIndicators();
	void DebugDrawPerceptionModifiers();
	void DebugDrawTargetTracks() const;
	void DebugDrawDebugAgent();
	void DebugDrawNavigation() const;
	void DebugDrawLightManager();
	void DebugDrawP0AndP1() const;
	void DebugDrawPuppetPaths();
	void DebugDrawCheckCapsules() const;
	void DebugDrawCheckRay() const;
	void DebugDrawCheckFloorPos() const;
	void DebugDrawCheckGravity() const;
	void DebugDrawDebugShapes();
	void DebugDrawGroupTactic();
	void DebugDrawDamageParts() const;
	void DebugDrawStanceSize() const;
	void DebugDrawForceAGAction() const;
	void DebugDrawForceAGSignal() const;
	void DebugDrawForceStance() const;
	void DebugDrawForcePosture() const;
	void DebugDrawPlayerActions() const;
	void DebugDrawPath();
	void DebugDrawPathAdjustments() const;
	void DebugDrawPathSingle(const CPipeUser* pPipeUser) const;
	void DebugDrawAgents() const;
	void DebugDrawAgent(CAIObject* pAgent) const;
	void DebugDrawStatsTarget(const char* pName);
	void DebugDrawType() const;
	void DebugDrawTypeSingle(CAIObject* pAIObj) const;
	void DebugDrawPendingEvents(CPuppet* pPuppet, int xPos, int yPos) const;
	void DebugDrawTargetsList() const;
	void DebugDrawTargetUnit(CAIObject* pAIObj) const;
	void DebugDrawStatsList() const;
	void DebugDrawLocate() const;
	void DebugDrawLocateUnit(CAIObject* pAIObj) const;
	void DebugDrawSteepSlopes();
	void DebugDrawVegetationCollision();
	void DebugDrawGroups();
	void DebugDrawOneGroup(float x, float& y, float& w, float fontSize, short groupID, const ColorB& textColor,
	                       const ColorB& worldColor, bool drawWorld);
	void DebugDrawCrowdControl();
	void DebugDrawRadar();
	void DrawRadarPath(CPipeUser* pPipeUser, const Matrix34& world, const Matrix34& screen);
	void DebugDrawRecorderRange() const;
	void DebugDrawShooting() const;
	void DebugDrawAreas() const;
	void DebugDrawAmbientFire() const;
	void DebugDrawInterestSystem(int iLevel) const;
	void DebugDrawExpensiveAccessoryQuota() const;
	void DebugDrawDamageControlGraph() const;
	void DebugDrawAdaptiveUrgency() const;
	enum EDrawUpdateMode
	{
		DRAWUPDATE_NONE = 0,
		DRAWUPDATE_NORMAL,
		DRAWUPDATE_WARNINGS_ONLY,
	};
	bool DebugDrawUpdateUnit(CAIActor* pTargetAIActor, int row, EDrawUpdateMode mode) const;

	void DEBUG_AddFakeDamageIndicator(CAIActor* pShooter, float t);

	void DebugDrawSelectedTargets();
	void TryDebugDrawPhysicsAccess();

	struct SDebugLine
	{
		SDebugLine(const Vec3& start_, const Vec3& end_, const ColorB& color_, float time_, float thickness_)
			: start(start_)
			, end(end_)
			, color(color_)
			, time(time_)
			, thickness(thickness_)
		{}
		Vec3   start, end;
		ColorB color;
		float  time;
		float  thickness;
	};
	std::vector<SDebugLine> m_vecDebugLines;
	Vec3                    m_lastStatsTargetTrajectoryPoint;
	std::list<SDebugLine>   m_lstStatsTargetTrajectory;

	struct SDebugBox
	{
		SDebugBox(const Vec3& pos_, const OBB& obb_, const ColorB& color_, float time_)
			: pos(pos_)
			, obb(obb_)
			, color(color_)
			, time(time_)
		{}
		Vec3   pos;
		OBB    obb;
		ColorB color;
		float  time;
	};
	std::vector<SDebugBox> m_vecDebugBoxes;

	struct SDebugSphere
	{
		SDebugSphere(const Vec3& pos_, float radius_, const ColorB& color_, float time_)
			: pos(pos_)
			, radius(radius_)
			, color(color_)
			, time(time_)
		{}
		Vec3   pos;
		float  radius;
		ColorB color;
		float  time;
	};
	std::vector<SDebugSphere> m_vecDebugSpheres;

	struct SDebugCylinder
	{
		SDebugCylinder(const Vec3& pos_, const Vec3& dir_, float radius_, float height_, const ColorB& color_, float time_)
			: pos(pos_)
			, dir(dir_)
			, height(height_)
			, radius(radius_)
			, color(color_)
			, time(time_)
		{}
		Vec3   pos;
		Vec3   dir;
		float  radius;
		float  height;
		ColorB color;
		float  time;
	};
	std::vector<SDebugCylinder> m_vecDebugCylinders;

	struct SDebugCone
	{
		SDebugCone(const Vec3& pos_, const Vec3& dir_, float radius_, float height_, const ColorB& color_, float time_)
			: pos(pos_)
			, dir(dir_)
			, height(height_)
			, radius(radius_)
			, color(color_)
			, time(time_)
		{}
		Vec3   pos;
		Vec3   dir;
		float  radius;
		float  height;
		ColorB color;
		float  time;
	};
	std::vector<SDebugCone> m_vecDebugCones;

	void DrawDebugShape(const SDebugLine&);
	void DrawDebugShape(const SDebugBox&);
	void DrawDebugShape(const SDebugSphere&);
	void DrawDebugShape(const SDebugCylinder&);
	void DrawDebugShape(const SDebugCone&);
	template<typename ShapeContainer>
	void DrawDebugShapes(ShapeContainer& shapes, float dt);

	void AddDebugLine(const Vec3& start, const Vec3& end, const ColorB& color, float time, float thickness = 1.0f);

	void AddDebugBox(const Vec3& pos, const OBB& obb, uint8 r, uint8 g, uint8 b, float time);
	void AddDebugCylinder(const Vec3& pos, const Vec3& dir, float radius, float length, const ColorB& color, float time);
	void AddDebugCone(const Vec3& pos, const Vec3& dir, float radius, float length, const ColorB& color, float time);

	void AddPerceptionDebugLine(const char* tag, const Vec3& start, const Vec3& end, uint8 r, uint8 g, uint8 b, float time, float thickness);

#endif //CRYAISYSTEM_DEBUG

	// returns fLeft > fRight
	static bool CompareFloatsFPUBugWorkaround(float fLeft, float fRight);

private:
	bool        InitUpdate(const CTimeValue frameStartTime, const float frameDeltaTime);
	bool        InitializeSmartObjectsIfNotInitialized();
	
	bool        ShouldUpdateSubsystem(const IAISystem::ESubsystemUpdateFlag subsystemUpdateFlag, const bool isAutomaticUpdate) const;

	void        ResetAIActorSets(bool clearSets);

	void        DetachFromTerritoryAllAIObjectsOfType(const char* szTerritoryName, unsigned short int nType);

	void        LoadCover(const char* szLevel, const char* szMission);
	void        LoadMNM(const char* szLevel, const char* szMission, bool afterExporting);

	//Debugging / Logging subsystems
	////////////////////////////////////////////////////////////////////

private:
	bool CompleteInit();
	void RegisterSchematycEnvPackage(Schematyc::IEnvRegistrar& registrar);

	void RegisterFirecommandHandler(IFireCommandDesc* desc);

	void CallReloadTPSQueriesScript();

	CGlobalPerceptionScaleHandler m_globalPerceptionScale;

	//CAISystem/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	uint64        m_nFrameTicks; // counter for light ai system profiler

	AIActorVector m_tmpFullUpdates;
	AIActorVector m_tmpDryUpdates;
	AIActorVector m_tmpAllUpdates;

	EntityId      m_agentDebugTarget;
};

#endif // _CAISYSTEM_H_
