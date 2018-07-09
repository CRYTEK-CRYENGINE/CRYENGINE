// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryPhysics/RayCastQueue.h>
#include <CryPhysics/IntersectionTestQueue.h>

#include <CryNetwork/SerializeFwd.h>
#include <CryAISystem/IAIRecorder.h>  // <> required for Interfuscator
#include <CryThreading/IJobManager.h> // <> required for Interfuscator
#include <CryPhysics/IPhysics.h>
#include <CryCore/Containers/CryFixedArray.h>
#include <CryEntitySystem/IEntity.h>

struct AgentPathfindingProperties;
struct INavigation;
struct IAIPathFinder;
struct IMNMPathfinder;
class ICrySizer;
struct IEntity;
struct IAIDebugRenderer;
struct IAIObject;
struct IAISignalExtraData;
struct ICoordinationManager;
struct ICommunicationManager;
struct ICoverSystem;
struct INavigationSystem;
namespace BehaviorTree
{
struct IBehaviorTreeManager;
}
struct IFunctionHandler;
class ICentralInterestManager;
struct ITacticalPointSystem;
struct ITargetTrackManager;
struct Sphere;
struct IAIActionManager;
struct ISmartObjectManager;
struct HidespotQueryContext;
struct IAuditionMap;
class IVisionMap;
struct IFactionMap;
class IAISystemComponent;
struct IAIObjectManager;
struct IAIActorProxyFactory;
struct IAIGroupProxyFactory;
struct IAIGroupProxy;
struct IAIGroupManager;
struct SAIDetectionLevels;
struct IAIActor;
struct IClusterDetector;
class IPathFollower;
struct IPathObstacles;
struct PathFollowerParams;
struct IAIBubblesSystem;
struct IActorLookUp;
namespace AIActionSequence {
struct ISequenceManager;
}

//! Define the oldest AI Area file version that can still be read.
#define BAI_AREA_FILE_VERSION_READ 19

//! Define the AI Area file version that will be written.
#define BAI_AREA_FILE_VERSION_WRITE 24

#define SMART_OBJECTS_XML           "Libs/SmartObjects.xml"
#define AI_ACTIONS_PATH             "Libs/ActionGraphs"

typedef CryFixedArray<IPhysicalEntity*, 32> PhysSkipList;

//! \cond INTERNAL
//! If this is changed be sure to change the table aiCollisionEntitiesTable in AICollision.cpp.
enum EAICollisionEntities
{
	AICE_STATIC                        = ent_static | ent_terrain | ent_ignore_noncolliding,
	AICE_ALL                           = ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain | ent_ignore_noncolliding,
	AICE_ALL_SOFT                      = ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain,
	AICE_DYNAMIC                       = ent_sleeping_rigid | ent_rigid | ent_ignore_noncolliding,
	AICE_STATIC_EXCEPT_TERRAIN         = ent_static | ent_ignore_noncolliding,
	AICE_ALL_EXCEPT_TERRAIN            = ent_static | ent_sleeping_rigid | ent_rigid | ent_ignore_noncolliding,
	AICE_ALL_INLUDING_LIVING           = ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain | ent_ignore_noncolliding | ent_living,
	AICE_ALL_EXCEPT_TERRAIN_AND_STATIC = ent_sleeping_rigid | ent_rigid | ent_ignore_noncolliding
};

enum EnumAreaType
{
	AREATYPE_PATH,
	AREATYPE_FORBIDDEN,
	AREATYPE_FORBIDDENBOUNDARY,
	AREATYPE_NAVIGATIONMODIFIER,
	AREATYPE_OCCLUSION_PLANE,
	AREATYPE_EXTRALINKCOST,
	AREATYPE_GENERIC,
	AREATYPE_PERCEPTION_MODIFIER,
};

//! ENavModifierType: Values are important and some types have been removed.
enum ENavModifierType
{
	NMT_INVALID            = -1,
	NMT_WAYPOINTHUMAN      = 0,
	NMT_VOLUME             = 1,
	NMT_FLIGHT             = 2,
	NMT_WATER              = 3,
	NMT_WAYPOINT_3DSURFACE = 4,
	NMT_EXTRA_NAV_COST     = 5,
	NMT_FREE_2D            = 7,
	NMT_TRIANGULATION      = 8,
	NMT_LAYERED_NAV_MESH   = 9,
	NMT_FLIGHT2,
};

enum EAILightLevel
{
	AILL_NONE,      //!< No effect.
	AILL_LIGHT,     //!< Light.
	AILL_MEDIUM,    //!< Medium.
	AILL_DARK,      //!< Dark.
	AILL_SUPERDARK, //!< Super dark.
	AILL_LAST,      //!< This always has to be the last one.
};

//! AI sound event types. They are roughly in the order of priority.
//! The priority is not enforced but may be used as a hint when handling the sound event.
enum EAISoundStimType
{
	AISOUND_GENERIC,        //!< Generic sound event.
	AISOUND_COLLISION,      //!< Sound event from collisions.
	AISOUND_COLLISION_LOUD, //!< Sound event from collisions, loud.
	AISOUND_MOVEMENT,       //!< Movement related sound event.
	AISOUND_MOVEMENT_LOUD,  //!< Movement related sound event, very loud, like walking in water or a vehicle sound.
	AISOUND_WEAPON,         //!< Weapon firing related sound event.
	AISOUND_EXPLOSION,      //!< Explosion related sound event.
	AISOUND_LAST,
};

//! Different grenade events reported into the AIsystem.
enum EAIGrenadeStimType
{
	AIGRENADE_THROWN,
	AIGRENADE_COLLISION,
	AIGRENADE_FLASH_BANG,
	AIGRENADE_SMOKE,
	AIGRENADE_LAST,
};

//! Different light events reported into the AIsystem.
enum EAILightEventType
{
	AILE_GENERIC,
	AILE_MUZZLE_FLASH,
	AILE_FLASH_LIGHT,
	AILE_LASER,
	AILE_LAST,
};

enum EActionType
{
	eAT_None = 0,
	eAT_Action,
	eAT_PriorityAction,
	eAT_Approach,
	eAT_PriorityApproach,
	eAT_ApproachAction,
	eAT_PriorityApproachAction,
	eAT_AISignal,
	eAT_AnimationSignal,
	eAT_AnimationAction,
	eAT_PriorityAnimationSignal,
	eAT_PriorityAnimationAction,
};

typedef uint16 EAILoadDataFlags;
enum EAILoadDataFlag : EAILoadDataFlags
{
	eAILoadDataFlag_None          = 0,
	eAILoadDataFlag_MNM           = BIT(0),
	eAILoadDataFlag_DesignedAreas = BIT(1),
	eAILoadDataFlag_Covers        = BIT(2),

	eAILoadDataFlag_AfterExport   = BIT(14),
	eAILoadDataFlag_QuickLoad     = BIT(15),

	eAILoadDataFlag_Navigation    = eAILoadDataFlag_MNM | eAILoadDataFlag_DesignedAreas,
	eAILoadDataFlag_AllSystems    = 0xFFFF & ~(eAILoadDataFlag_AfterExport | eAILoadDataFlag_QuickLoad),
};

struct SNavigationShapeParams
{
	SNavigationShapeParams(
		const char* szPathName = 0, EnumAreaType areaType = AREATYPE_PATH, bool pathIsRoad = true,
		bool closed = false, const Vec3* points = 0, unsigned nPoints = 0, float fHeight = 0,
		int nNavType = 0, int nAuxType = 0, EAILightLevel lightLevel = AILL_NONE,
		float fReductionPerMetre = 0.0f, float fReductionMax = 1.0f)
		: szPathName(szPathName)
		, areaType(areaType)
		, pathIsRoad(pathIsRoad)
		, closed(closed), points(points)
		, nPoints(nPoints)
		, fHeight(fHeight)
		, nNavType(nNavType)
		, nAuxType(nAuxType)
		, fReductionPerMetre(fReductionPerMetre)
		, fReductionMax(fReductionMax)
		, lightLevel(lightLevel)
	{}

	const char*          szPathName;
	EnumAreaType         areaType;
	bool                 pathIsRoad;
	bool                 closed;
	const Vec3*          points;
	unsigned             nPoints;
	float                fHeight;
	int                  nNavType;
	int                  nAuxType;
	EAILightLevel        lightLevel;

	//! Size of the triangles to create when it's a nav modifier that adds extra triangles for parameters for PerceptionModifier.
	float         fReductionPerMetre;
	float         fReductionMax;
};

struct SmartObjectCondition
{
	string      sUserClass;
	string      sUserState;
	string      sUserHelper;

	string      sObjectClass;
	string      sObjectState;
	string      sObjectHelper;

	float       fDistanceFrom;
	float       fDistanceTo;
	float       fOrientationLimit;
	bool        bHorizLimitOnly;
	float       fOrientationToTargetLimit;

	float       fMinDelay;
	float       fMaxDelay;
	float       fMemory;

	float       fProximityFactor;
	float       fOrientationFactor;
	float       fVisibilityFactor;
	float       fRandomnessFactor;

	float       fLookAtOnPerc;
	string      sUserPreActionState;
	string      sObjectPreActionState;
	EActionType eActionType;
	string      sAction;
	string      sUserPostActionState;
	string      sObjectPostActionState;

	int         iMaxAlertness;
	bool        bEnabled;
	string      sName;
	string      sFolder;
	string      sDescription;
	int         iOrder;

	int         iRuleType; //!< 0 - normal rule; 1 - navigational rule.
	string      sEvent;
	string      sChainedUserEvent;
	string      sChainedObjectEvent;
	string      sEntranceHelper;
	string      sExitHelper;

	int         iTemplateId;

	// exact positioning related
	float  fApproachSpeed;
	int    iApproachStance;
	string sAnimationHelper;
	string sApproachHelper;
	float  fStartWidth;
	float  fDirectionTolerance;
	float  fStartArcAngle;

	bool operator==(const SmartObjectCondition& other) const
	{
		return
		  iTemplateId == other.iTemplateId &&

		  iOrder == other.iOrder &&
		  sUserClass == other.sUserClass &&
		  sUserState == other.sUserState &&
		  sUserHelper == other.sUserHelper &&

		  sObjectClass == other.sObjectClass &&
		  sObjectState == other.sObjectState &&
		  sObjectHelper == other.sObjectHelper &&

		  iRuleType == other.iRuleType &&
		  sEvent == other.sEvent &&
		  sEntranceHelper == other.sEntranceHelper &&
		  sExitHelper == other.sExitHelper &&

		  fDistanceFrom == other.fDistanceFrom &&
		  fDistanceTo == other.fDistanceTo &&
		  fOrientationLimit == other.fOrientationLimit &&
		  bHorizLimitOnly == other.bHorizLimitOnly &&
		  fOrientationToTargetLimit == other.fOrientationToTargetLimit &&

		  fMinDelay == other.fMinDelay &&
		  fMaxDelay == other.fMaxDelay &&
		  fMemory == other.fMemory &&

		  fProximityFactor == other.fProximityFactor &&
		  fOrientationFactor == other.fOrientationFactor &&
		  fVisibilityFactor == other.fVisibilityFactor &&
		  fRandomnessFactor == other.fRandomnessFactor &&

		  fLookAtOnPerc == other.fLookAtOnPerc &&
		  sUserPreActionState == other.sUserPreActionState &&
		  sObjectPreActionState == other.sObjectPreActionState &&
		  eActionType == other.eActionType &&
		  sAction == other.sAction &&
		  sUserPostActionState == other.sUserPostActionState &&
		  sObjectPostActionState == other.sObjectPostActionState &&

		  iMaxAlertness == other.iMaxAlertness &&
		  bEnabled == other.bEnabled &&
		  sName == other.sName &&
		  sFolder == other.sFolder &&
		  sDescription == other.sDescription &&

		  fApproachSpeed == other.fApproachSpeed &&
		  iApproachStance == other.iApproachStance &&
		  sAnimationHelper == other.sAnimationHelper &&
		  sApproachHelper == other.sApproachHelper &&
		  fStartWidth == other.fStartWidth &&
		  fDirectionTolerance == other.fDirectionTolerance &&
		  fStartArcAngle == other.fStartArcAngle;
	}

	bool operator<(const SmartObjectCondition& other) const
	{
		return iOrder < other.iOrder;
	}
};

struct SmartObjectHelper
{
	SmartObjectHelper() : templateHelperIndex(-1) {}
	QuatT  qt;
	string name;
	string description;
	int    templateHelperIndex;
};

#if defined(GetObject)
	#undef GetObject
#endif

//! AI object iterator interface, see IAISystem::GetFirst.
struct IAIObjectIter
{
	virtual ~IAIObjectIter(){}

	//! Advance to next object.
	virtual void Next() = 0;

	//! Returns the current object.
	virtual IAIObject* GetObject() = 0;

	//! Delete the iterator.
	virtual void Release() = 0;
};

//! Helper class for AI object iterator.
class AutoAIObjectIter
{
public:
	AutoAIObjectIter() : m_pIter(NULL) {}
	AutoAIObjectIter(IAIObjectIter* it) : m_pIter(NULL) { Assign(it); }
	~AutoAIObjectIter() { Assign(NULL); }
	IAIObjectIter* operator->()              { return m_pIter; }
	IAIObjectIter* GetPtr()                  { return m_pIter; }
	void           Assign(IAIObjectIter* it) { SAFE_RELEASE(m_pIter); m_pIter = it; }

private:
	AutoAIObjectIter(const AutoAIObjectIter&);
	AutoAIObjectIter& operator=(const AutoAIObjectIter&);

private:
	IAIObjectIter* m_pIter;
};

//! AI Global perception Listener.
struct IAIGlobalPerceptionListener
{
	enum EGlobalPerceptionScaleEvent
	{
		eGPS_Set,
		eGPS_Disabled,
	};

	// <interfuscator:shuffle>
	virtual ~IAIGlobalPerceptionListener() {}
	virtual void OnPerceptionScalingEvent(const EGlobalPerceptionScaleEvent event) = 0;
	// </interfuscator:shuffle>
};

struct IAIAlertnessPredicate
{
	// <interfuscator:shuffle>
	virtual ~IAIAlertnessPredicate(){}
	virtual bool ConsiderAIObject(IAIObject* pAIObject) const = 0;
	// </interfuscator:shuffle>
};

enum EAIFilterType
{
	eAIFT_All = 0,
	eAIFT_Enemies,
	eAIFT_Friends,
	eAIFT_Faction,
	eAIFT_None,
};
//! \endcond

struct IAIEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IAIEngineModule, "4b00591d-c874-43c7-9bca-78a59ecd6d9c"_cry_guid);
};

struct IAISystemCallbacks
{
	virtual CFunctorsList<Functor1<IAIObject*>>& ObjectCreated() = 0;
	virtual CFunctorsList<Functor1<IAIObject*>>& ObjectRemoved() = 0;
	virtual CFunctorsList<Functor2<tAIObjectID, bool>>& EnabledStateChanged() = 0;
	virtual CFunctorsList<Functor2<EntityId, EntityId>>& AgentDied() = 0;
};

//! Interface to AI system. Defines functions to control the AI system.
struct IAISystem
{
	typedef RayCastQueue<41>                    GlobalRayCaster;
	typedef IntersectionTestQueue<43>           GlobalIntersectionTester;
	
	//! Flags used by the GetGroupCount.
	enum EGroupFlags
	{
		GROUP_ALL     = 0x01,     //!< Returns all agents in the group (default).
		GROUP_ENABLED = 0x02,     //!< Returns only the count of enabled agents (exclusive with all).
		GROUP_MAX     = 0x04,     //!< Returns the maximum number of agents during the game (can be combined with all or enabled).
	};

	//! Indication of (a) what a graph node represents and (b) what kind of graph node an AI entity can navigate.
	//! In the latter case it can be used as a bit mask.
	enum ENavigationType
	{
		NAV_UNSET              = 1 << 0,
		NAV_TRIANGULAR         = 1 << 1,
		NAV_WAYPOINT_HUMAN     = 1 << 2,
		NAV_WAYPOINT_3DSURFACE = 1 << 3,
		NAV_FLIGHT             = 1 << 4,
		NAV_VOLUME             = 1 << 5,
		NAV_ROAD               = 1 << 6,
		NAV_SMARTOBJECT        = 1 << 7,
		NAV_FREE_2D            = 1 << 8,
		NAV_CUSTOM_NAVIGATION  = 1 << 9,
		NAV_MAX_VALUE          = NAV_CUSTOM_NAVIGATION
	};
	enum {NAV_TYPE_COUNT = 10};

	//! Two masks that summarise the basic abilities.
	enum
	{
		NAVMASK_SURFACE = NAV_TRIANGULAR | NAV_WAYPOINT_HUMAN | NAV_ROAD | NAV_SMARTOBJECT,
		NAVMASK_AIR     = NAV_FLIGHT | NAV_VOLUME | NAV_SMARTOBJECT,
		NAVMASK_ALL     = NAV_TRIANGULAR | NAV_WAYPOINT_HUMAN | NAV_WAYPOINT_3DSURFACE | NAV_ROAD | NAV_FLIGHT | NAV_VOLUME | NAV_SMARTOBJECT | NAV_FREE_2D | NAV_CUSTOM_NAVIGATION,
	};

	enum EResetReason
	{
		RESET_INTERNAL,     //!< Called by the AI system itself.
		RESET_ENTER_GAME,
		RESET_EXIT_GAME,
		RESET_INTERNAL_LOAD,  //!< Called by the AI system itself.
		RESET_LOAD_LEVEL,
		RESET_UNLOAD_LEVEL
	};

	//! Bit mask using ENavigationType.
	//! \note NavCapMask is no longer a primitive type.  This
	//! thin wrapper around primitive unsigned is necessary to transparently support
	//! the LNM.  The LNM breaks the design assumptions of the current system by
	//! producing meshes tailored to various agent type capabilities.  While the
	//! question "can triangulation be used" was well-formed it doesn't make sense
	//! in the context of the LNM where there's multiple meshes, out of which some
	//! might be useable and some not.
	//!
	//! To narrow the choice down to a single mesh, still making the rest of
	//! the system work as it did before, 's_lnmBits' are set aside to be used
	//! to discriminate among LNM meshes built for different agent types.
	//! Operators are overloaded so that non-LNM parts of the system never see
	//! the LNM part.  LNM-aware parts can request the LNM bits explicitly.
	//!
	//! Note also that the LNM bits aren't used as a bitmask.
	class NavCapMask
	{
		static const unsigned s_maskWidth = 8 * 4 /*sizeof (unsigned)*/;
		static const unsigned s_lnmBits = 8;
		// ATTN Aug 17, 2009: <pvl> if you change the following you also need to
		// change 'pathfindProperties.lua'
		static const unsigned s_lnmShift = s_maskWidth - s_lnmBits;
		static const unsigned s_lnmMask = ((1 << s_lnmBits) - 1) << s_lnmShift;
		unsigned              m_navCaps;
	public:
		NavCapMask() : m_navCaps(0) {}
		NavCapMask(unsigned caps) : m_navCaps(caps) {}

		// NOTE Oct 21, 2009: <pvl> only legacy (pre-LNM) code can sometimes
		// use NavCapMask as unsigned.  That code doesn't expect to see LNM bits
		// so strip them off.
		operator unsigned() const { return m_navCaps & ~s_lnmMask; }

		NavCapMask& operator&=(unsigned rhs)     { m_navCaps &= rhs; return *this; }
		NavCapMask& operator|=(unsigned rhs)     { m_navCaps |= rhs; return *this; }

		unsigned GetLnmCaps() const           { return (m_navCaps & s_lnmMask) >> s_lnmShift; }
		void     SetLnmCaps(unsigned lnmData) { m_navCaps |= lnmData << s_lnmShift; }

		// NOTE Oct 21, 2009: <pvl> unlike operator unsigned() this one returns
		// the unedited mask including the LNM part
		unsigned GetFullMask() const { return m_navCaps; }

		void     Serialize(TSerialize ser);
	};
	typedef NavCapMask tNavCapMask;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Basic
	IAISystem() {}
	// <interfuscator:shuffle>
	virtual ~IAISystem() {}

	virtual bool                        Init() = 0;

	virtual void                        Reload() {}
	virtual void                        Reset(EResetReason reason) = 0;
	virtual void                        Release() = 0;

	virtual IAISystemCallbacks&         Callbacks() = 0;

	virtual void                        SetActorProxyFactory(IAIActorProxyFactory* pFactory) = 0;
	virtual IAIActorProxyFactory*       GetActorProxyFactory() const = 0;

	virtual void                        SetGroupProxyFactory(IAIGroupProxyFactory* pFactory) = 0;
	virtual IAIGroupProxyFactory*       GetGroupProxyFactory() const = 0;

	virtual IAIGroupProxy*              GetAIGroupProxy(int groupID) = 0;

	virtual IActorLookUp*               GetActorLookup() = 0;

	virtual IAISystem::GlobalRayCaster*          GetGlobalRaycaster() = 0;
	virtual IAISystem::GlobalIntersectionTester* GetGlobalIntersectionTester() = 0;

	//If disabled most things early out
	virtual void Enable(bool enable = true) = 0;
	virtual bool IsEnabled() const = 0;

	//! Every frame (multiple time steps per frame possible?)
	//! \param currentTime AI time since game start in seconds (GetCurrentTime).
	//! \param frameTime AI time since last update (GetFrameTime).
	virtual void                Update(CTimeValue currentTime, float frameTime) = 0;

	virtual bool                RegisterSystemComponent(IAISystemComponent* pComponent) = 0;
	virtual bool                UnregisterSystemComponent(IAISystemComponent* pComponent) = 0;

	virtual void                SendAnonymousSignal(int nSignalId, const char* szText, const Vec3& pos, float fRadius, IAIObject* pSenderObject, IAISignalExtraData* pData = NULL) = 0;
	virtual void                SendSignal(unsigned char cFilter, int nSignalId, const char* szText, IAIObject* pSenderObject, IAISignalExtraData* pData = NULL, uint32 crcCode = 0) = 0;
	virtual void                FreeSignalExtraData(IAISignalExtraData* pData) const = 0;
	virtual IAISignalExtraData* CreateSignalExtraData() const = 0;
	virtual void                Event(int eventT, const char*) = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Time/Updates
	//! Over-ride auto-disable for distant AIs.
	virtual bool GetUpdateAllAlways() const = 0;

	//! \return The basic AI system update interval.
	virtual float GetUpdateInterval() const = 0;

	//! Used for profiling.
	virtual int GetAITickCount() = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// FileIO
	//! Save/load.
	virtual void SerializeObjectIDs(TSerialize ser) = 0;
	virtual void Serialize(TSerialize ser) = 0;

	//! Set a path for the current level as working folder for level-specific metadata.
	virtual void SetLevelPath(const char* sPath) = 0;

	//! This called before loading (level load/serialization).
	virtual void FlushSystem(bool bDeleteAll = false) = 0;
	virtual void FlushSystemNavigation(bool bDeleteAll = false) = 0;

	virtual void LayerEnabled(const char* layerName, bool enabled, bool serialized) = 0;

	virtual void LoadLevelData(const char* szLevel, const char* szMission, const EAILoadDataFlags loadDataFlags = eAILoadDataFlag_AllSystems) = 0;
	virtual void LoadNavigationData(const char* szLevel, const char* szMission, const EAILoadDataFlags loadDataFlags = eAILoadDataFlag_AllSystems) = 0;

#if defined(SEG_WORLD)
	// Reads areas from file. clears the existing areas.
	// SEG_WORLD: adds offset to the areas read, and doesn't clear existing areas.
	virtual void ReadAreasFromFile(const char* fileNameAreas, const Vec3& vSegmentOffset) = 0;
#else
	//! Reads areas from file. clears the existing areas.
	virtual void ReadAreasFromFile(const char* fileNameAreas) = 0;
#endif

	virtual void OnMissionLoaded() = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Debugging
	// AI DebugDraw.
	virtual IAIDebugRenderer* GetAIDebugRenderer() = 0;
	virtual IAIDebugRenderer* GetAINetworkDebugRenderer() = 0;
	virtual void              SetAIDebugRenderer(IAIDebugRenderer* pAIDebugRenderer) = 0;
	virtual void              SetAINetworkDebugRenderer(IAIDebugRenderer* pAINetworkDebugRenderer) = 0;

	virtual void              SetAgentDebugTarget(const EntityId id) = 0;
	virtual EntityId          GetAgentDebugTarget() const = 0;

	virtual IAIBubblesSystem* GetAIBubblesSystem() = 0;

	// Debug recorder.
	virtual bool IsRecording(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event) const = 0;
	virtual void Record(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString) const = 0;
	virtual void GetRecorderDebugContext(SAIRecorderDebugContext*& pContext) = 0;
	virtual void AddDebugLine(const Vec3& start, const Vec3& end, uint8 r, uint8 g, uint8 b, float time) = 0;
	virtual void AddDebugSphere(const Vec3& pos, float radius, uint8 r, uint8 g, uint8 b, float time) = 0;

	virtual void DebugReportHitDamage(IEntity* pVictim, IEntity* pShooter, float damage, const char* material) = 0;
	virtual void DebugReportDeath(IAIObject* pVictim) = 0;

	// Functions to let external systems (e.g. lua) access the AI logging functions.
	// The external system should pass in an identifier (e.g. "<Lua>").
	virtual void Warning(const char* id, const char* format, ...) const PRINTF_PARAMS(3, 4) = 0;
	virtual void Error(const char* id, const char* format, ...) PRINTF_PARAMS(3, 4) = 0;
	virtual void LogProgress(const char* id, const char* format, ...) PRINTF_PARAMS(3, 4) = 0;
	virtual void LogEvent(const char* id, const char* format, ...) PRINTF_PARAMS(3, 4) = 0;
	virtual void LogComment(const char* id, const char* format, ...) PRINTF_PARAMS(3, 4) = 0;

	//! Draws a fake tracer around the player.
	virtual void DebugDrawFakeTracer(const Vec3& pos, const Vec3& dir) = 0;

	virtual void GetMemoryStatistics(ICrySizer* pSizer) = 0;

	// debug members ============= DO NOT USE
	virtual void DebugDraw() = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Get Subsystems
	virtual IAIObjectManager*                   GetAIObjectManager() = 0;
	virtual ISmartObjectManager*                GetSmartObjectManager() = 0;

	virtual ITargetTrackManager*                GetTargetTrackManager() const = 0;
	virtual BehaviorTree::IBehaviorTreeManager* GetIBehaviorTreeManager() const = 0;
	virtual ICoverSystem*                       GetCoverSystem() const = 0;
	virtual INavigationSystem*                  GetNavigationSystem() const = 0;
	virtual IMNMPathfinder*                     GetMNMPathfinder() const = 0;
	virtual ICommunicationManager*              GetCommunicationManager() const = 0;
	virtual ITacticalPointSystem*               GetTacticalPointSystem(void) = 0;
	virtual ICentralInterestManager*            GetCentralInterestManager(void) = 0;
	virtual INavigation*                        GetINavigation() = 0;
	virtual IAIRecorder*                        GetIAIRecorder() = 0;
	virtual struct IMovementSystem*             GetMovementSystem() const = 0;
	virtual AIActionSequence::ISequenceManager* GetSequenceManager() const = 0;
	virtual IClusterDetector*                   GetClusterDetector() const = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// AI Actions
	virtual IAIActionManager* GetAIActionManager() = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Leader/Formations
	virtual void       EnumerateFormationNames(unsigned int maxNames, const char** names, unsigned int* nameCount) const = 0;
	virtual int        GetGroupCount(int nGroupID, int flags = GROUP_ALL, int type = 0) = 0;
	virtual IAIObject* GetGroupMember(int groupID, int index, int flags = GROUP_ALL, int type = 0) = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Goal Pipes
	// TODO: get rid of this; => it has too many confusing uses to remove just yet.
	virtual int AllocGoalPipeId() const = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Navigation / Pathfinding
	virtual bool CreateNavigationShape(const SNavigationShapeParams& params) = 0;
	virtual void DeleteNavigationShape(const char* szPathName) = 0;
	virtual bool DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road = false) = 0;
	virtual void EnableGenericShape(const char* shapeName, bool state) = 0;

	//Temporary - move to perception system in the future
	virtual int  RayOcclusionPlaneIntersection(const Vec3& start, const Vec3& end) = 0;

	// Pathfinding properties.
	virtual void                              AssignPFPropertiesToPathType(const string& sPathType, const AgentPathfindingProperties& properties) = 0;
	virtual const AgentPathfindingProperties* GetPFPropertiesOfPathType(const string& sPathType) = 0;
	virtual string                            GetPathTypeNames() = 0;

	//! Register a spherical region that causes damage (so should be avoided in pathfinding).
	//! \param pID a unique identifier - so if this is called multiple times with the same pID then the damage region
	//! will simply be moved. If radius <= 0 then the region is disabled.
	virtual void RegisterDamageRegion(const void* pID, const Sphere& sphere) = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	virtual void DummyFunctionNumberOne(void) = 0;

	//! \return A point which is a valid distance away from a wall in front of the point.
	virtual void AdjustDirectionalCoverPosition(Vec3& pos, const Vec3& dir, float agentRadius, float testHeight) = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Perception
	//! Current global AI alertness value (what's the most alerted puppet).
	virtual int GetAlertness() const = 0;
	virtual int GetAlertness(const IAIAlertnessPredicate& alertnessPredicate) = 0;

	//! Look up table to be used when calculating visual time-out increment.
	virtual void SetPerceptionDistLookUp(float* pLookUpTable, int tableSize) = 0;

	// Global perception functionalities.
	virtual void  UpdateGlobalPerceptionScale(const float visualScale, const float audioScale, EAIFilterType filterTypeName = eAIFT_All, const char* factionName = NULL) = 0;
	virtual float GetGlobalVisualScale(const IAIObject* pAIObject) const = 0;
	virtual float GetGlobalAudioScale(const IAIObject* pAIObject) const = 0;
	virtual void  DisableGlobalPerceptionScaling() = 0;
	virtual void  RegisterGlobalPerceptionListener(IAIGlobalPerceptionListener* pListner) = 0;
	virtual void  UnregisterGlobalPerceptionlistener(IAIGlobalPerceptionListener* pListner) = 0;

	// Fills the array with possible dangers, returns number of dangers.
	virtual unsigned int GetDangerSpots(const IAIObject* requester, float range, Vec3* positions, unsigned int* types, unsigned int n, unsigned int flags) = 0;

	virtual void         DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, EntityId shooterId, float time = 5.0f) = 0;
	virtual void         DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, EntityId shooterId, float time = 5.0f) = 0;

	virtual IAuditionMap*  GetAuditionMap() = 0;
	virtual IVisionMap*  GetVisionMap() = 0;
	virtual IFactionMap& GetFactionMap() = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//WTF are these?
	virtual IAIObject* GetBeacon(unsigned short nGroupID) = 0;
	virtual void       UpdateBeacon(unsigned short nGroupID, const Vec3& vPos, IAIObject* pOwner = 0) = 0;

	virtual bool       ParseTables(int firstTable, bool parseMovementAbility, IFunctionHandler* pH, AIObjectParams& aiParams, bool& updateAlways) = 0;

	// Added to resolve merge conflict: to be removed in dev/c2!
	virtual float GetFrameStartTimeSecondsVirtual() const = 0;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! Light frame profiler for AI support.
	//! Add nTicks to the number of Ticks spend this frame in particle functions.
	virtual void AddFrameTicks(uint64 nTicks) = 0;

	//! Reset Ticks Counter.
	virtual void ResetFrameTicks() = 0;

	//! Get number of Ticks accumulated over this frame.
	virtual uint64                         NumFrameTicks() const = 0;

	virtual void                           NotifyTargetDead(IAIObject* pDeadObject) = 0;

	virtual std::shared_ptr<IPathFollower> CreateAndReturnNewDefaultPathFollower(const PathFollowerParams& params, const IPathObstacles& pathObstacleObject) = 0;
	// </interfuscator:shuffle>
};

#if defined(ENABLE_LW_PROFILERS)
//! \cond INTERNAL
class CAILightProfileSection
{
public:
	CAILightProfileSection()
		: m_nTicks(CryGetTicks())
	{
	}

	//! Need to force as no_inline, else on xbox(if cstr and dstr are inlined), we get totaly wrong numbers.
	NO_INLINE ~CAILightProfileSection()
	{
		IAISystem* pAISystem = gEnv->pAISystem;
		uint64 nTicks = CryGetTicks();
		IF (pAISystem != NULL, 1)
		{
			pAISystem->AddFrameTicks(nTicks - m_nTicks);
		}
	}
private:
	uint64 m_nTicks;
};
//! \endcond

	#define AISYSTEM_LIGHT_PROFILER() CAILightProfileSection _aiLightProfileSection;
#else
	#define AISYSTEM_LIGHT_PROFILER()
#endif