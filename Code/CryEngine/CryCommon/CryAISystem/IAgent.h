// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryMath/Cry_Math.h>
#include "AgentParams.h"
#include <CryMemory/CrySizer.h>
#include <CryScriptSystem/IScriptSystem.h> // <> required for Interfuscator
#include <CryAISystem/IAISystem.h>         // <> required for Interfuscator
#include <CryNetwork/SerializeFwd.h>
#include <limits>
#include <CryAISystem/ICommunicationManager.h> // <> required for Interfuscator
#include "IAIMannequin.h"
#include <CryEntitySystem/IEntitySystem.h>

#ifdef max
	#undef max
#endif

struct IAIActorProxy;
struct IAIObject;
struct IAIRateOfDeathHandler;
struct IEntity;
struct IGoalPipe;
struct IGoalPipeListener;
struct IPhysicalEntity;
struct IWeapon;
struct IAIGroupProxy;

class CPipeUser;
struct GoalParams;

//!    Defines for AIObject types.

#define AIOBJECT_NONE              200
#define AIOBJECT_DUMMY             0

#define AIOBJECT_ACTOR             5
#define AIOBJECT_VEHICLE           6

#define AIOBJECT_TARGET            9

#define AIOBJECT_AWARE             10
#define AIOBJECT_ATTRIBUTE         11
#define AIOBJECT_WAYPOINT          12
#define AIOBJECT_HIDEPOINT         13
#define AIOBJECT_SNDSUPRESSOR      14
#define AIOBJECT_NAV_SEED          15
#define AIOBJECT_HELICOPTER        40
#define AIOBJECT_HELICOPTERCRYSIS2 41
#define AIOBJECT_GUNFIRE           42
#define AIOBJECT_INFECTED          45
#define AIOBJECT_ALIENTICK         46
#define AIOBJECT_CAR               50
#define AIOBJECT_BOAT              60
#define AIOBJECT_AIRPLANE          70
#define AIOBJECT_2D_FLY            80
#define AIOBJECT_MOUNTEDWEAPON     90
#define AIOBJECT_GLOBALALERTNESS   94
#define AIOBJECT_LEADER            95
#define AIOBJECT_ORDER             96
#define AIOBJECT_PLAYER            100

#define AIOBJECT_GRENADE           150
#define AIOBJECT_RPG               151

#define AI_USE_HIDESPOTS           (1 << 14)

// Signal ids
#define AISIGNAL_INCLUDE_DISABLED     0
#define AISIGNAL_DEFAULT              1
#define AISIGNAL_PROCESS_NEXT_UPDATE  3
#define AISIGNAL_NOTIFY_ONLY          9
#define AISIGNAL_ALLOW_DUPLICATES     10
#define AISIGNAL_RECEIVED_PREV_UPDATE 30  //!< Internal AI system use only, like AISIGNAL_DEFAULT but used for logging/recording.
//!< A signal sent in the previous update and processed in the current (AISIGNAL_PROCESS_NEXT_UPDATE).
#define AISIGNAL_INITIALIZATION       -100

// Anchors
#define AIANCHOR_FIRST                     200
#define AIANCHOR_COMBAT_HIDESPOT           320
#define AIANCHOR_COMBAT_HIDESPOT_SECONDARY 330
#define AIANCHOR_COMBAT_TERRITORY          342
#define AIANCHOR_MNM_SHAPE                 343

#define AIANCHOR_REINFORCEMENT_SPOT        400
#define AIANCHOR_NOGRENADE_SPOT            405
#define AIANCHOR_ADVANTAGE_POINT           329

// Event types.
#define AIEVENT_ONVISUALSTIMULUS 2
#define AIEVENT_ONSOUNDEVENT     4
#define AIEVENT_AGENTDIED        5
#define AIEVENT_SLEEP            6
#define AIEVENT_WAKEUP           7
#define AIEVENT_ENABLE           8
#define AIEVENT_DISABLE          9
#define AIEVENT_PATHFINDON       11
#define AIEVENT_PATHFINDOFF      12
#define AIEVENT_CLEAR            15
//#define AIEVENT_MOVEMENT_CONTROL	16			//! Based on parameters lets object know if he is allowed to move.

#define AIEVENT_DROPBEACON             17
#define AIEVENT_USE                    19
#define AIEVENT_CLEARACTIVEGOALS       22
#define AIEVENT_DRIVER_IN              23 // To enable/disable AIVehicles.
#define AIEVENT_DRIVER_OUT             24 // To enable/disable AIVehicles.
#define AIEVENT_FORCEDNAVIGATION       25
#define AIEVENT_ADJUSTPATH             26
#define AIEVENT_LOWHEALTH              27
#define AIEVENT_ONBULLETRAIN           28

#define AIEVENT_PLAYER_STUNT_SPRINT    101
#define AIEVENT_PLAYER_STUNT_JUMP      102
#define AIEVENT_PLAYER_STUNT_PUNCH     103
#define AIEVENT_PLAYER_STUNT_THROW     104
#define AIEVENT_PLAYER_STUNT_THROW_NPC 105
#define AIEVENT_PLAYER_THROW           106
#define AIEVENT_PLAYER_STUNT_CLOAK     107
#define AIEVENT_PLAYER_STUNT_UNCLOAK   108
#define AIEVENT_PLAYER_STUNT_ARMORED   109
#define AIEVENT_PLAYER_STAMP_MELEE     110

#define AIREADIBILITY_INTERESTING      5
#define AIREADIBILITY_SEEN             10
#define AIREADIBILITY_LOST             20
#define AIREADIBILITY_NORMAL           30
#define AIREADIBILITY_NOPRIORITY       1

#define AIGOALPIPE_LOOP                0
#define AIGOALPIPE_RUN_ONCE            1 // Todo: Not working yet - see PipeUser.cpp
#define AIGOALPIPE_NOTDUPLICATE        2
#define AIGOALPIPE_HIGHPRIORITY        4  // It will be not removed when a goal pipe is selected.
#define AIGOALPIPE_SAMEPRIORITY        8  // Sets the priority to be the same as active goal pipe.
#define AIGOALPIPE_DONT_RESET_AG       16 // Don't reset the AG Input (by default AG Action input is reset to idle).
#define AIGOALPIPE_KEEP_LAST_SUBPIPE   32 // Keeps the last inserted subpipe.
#define AIGOALPIPE_KEEP_ON_TOP         64 // Keeps the inserted subpipe on the top for its duration, FIFO.

enum ESignalFilter
{
	SIGNALFILTER_SENDER,
	SIGNALFILTER_LASTOP,
	SIGNALFILTER_GROUPONLY,
	SIGNALFILTER_FACTIONONLY,
	SIGNALFILTER_ANYONEINCOMM,
	SIGNALFILTER_TARGET,
	SIGNALFILTER_SUPERGROUP,
	SIGNALFILTER_SUPERFACTION,
	SIGNALFILTER_SUPERTARGET,
	SIGNALFILTER_NEARESTGROUP,
	SIGNALFILTER_NEARESTSPECIES,
	SIGNALFILTER_NEARESTINCOMM,
	SIGNALFILTER_HALFOFGROUP,
	SIGNALFILTER_LEADER,
	SIGNALFILTER_GROUPONLY_EXCEPT,
	SIGNALFILTER_ANYONEINCOMM_EXCEPT,
	SIGNALFILTER_LEADERENTITY,
	SIGNALFILTER_NEARESTINCOMM_FACTION,
	SIGNALFILTER_NEARESTINCOMM_LOOKING,
	SIGNALFILTER_FORMATION,
	SIGNALFILTER_FORMATION_EXCEPT,
	SIGNALFILTER_READABILITY = 100,
	SIGNALFILTER_READABILITYAT,         //!< Readability anticipation.
	SIGNALFILTER_READABILITYRESPONSE,   //!< Readability response.
};

static const float AISPEED_ZERO   = 0.0f;
static const float AISPEED_SLOW   = 0.21f;
static const float AISPEED_WALK   = 0.4f;
static const float AISPEED_RUN    = 1.0f;
static const float AISPEED_SPRINT = 1.4f;

#define AI_JUMP_CHECK_COLLISION (1 << 0)
#define AI_JUMP_ON_GROUND       (1 << 1)
#define AI_JUMP_RELATIVE        (1 << 2)
#define AI_JUMP_MOVING_TARGET   (1 << 3)

//! The coverage half-angle of a hidespot anchor. The total coverage angle is twice this angle.
#define HIDESPOT_COVERAGE_ANGLE_COS 0.5f        // cos(60 degrees)

enum EStance
{
	STANCE_NULL  = -1,
	STANCE_STAND = 0,
	STANCE_CROUCH,
	STANCE_PRONE,
	STANCE_RELAXED,
	STANCE_STEALTH,
	STANCE_LOW_COVER,
	STANCE_ALERTED,
	STANCE_HIGH_COVER,
	STANCE_SWIM,
	STANCE_ZEROG,

	//! This value must be last.
	STANCE_LAST
};

inline const char* GetStanceName(int stance)
{
	switch (stance)
	{
	case STANCE_NULL:
		return "Null";
	case STANCE_STAND:
		return "Stand";
	case STANCE_CROUCH:
		return "Crouch";
	case STANCE_PRONE:
		return "Prone";
	case STANCE_RELAXED:
		return "Relaxed";
	case STANCE_STEALTH:
		return "Stealth";
	case STANCE_ALERTED:
		return "Alerted";
	case STANCE_LOW_COVER:
		return "LowCover";
	case STANCE_HIGH_COVER:
		return "HighCover";
	case STANCE_SWIM:
		return "Swim";
	case STANCE_ZEROG:
		return "Zero-G";
	default:
		return "<Unknown>";
	}
};

//! LookStyle essentially define the transition style when look target is set.
//! Hard style is fast (surprised, startled), while soft is slow, relaxed.
//! NOLOWER variations only turn upper body, do not change actual body direction.
enum ELookStyle
{
	LOOKSTYLE_DEFAULT = 0,
	LOOKSTYLE_HARD,
	LOOKSTYLE_HARD_NOLOWER,
	LOOKSTYLE_SOFT,
	LOOKSTYLE_SOFT_NOLOWER,
	LOOKSTYLE_COUNT
};

enum EFireMode
{
	FIREMODE_OFF                = 0, //!< Do not fire.
	FIREMODE_BURST              = 1, //!< Fire in bursts - living targets only.
	FIREMODE_CONTINUOUS         = 2, //!< Fire continuously - living targets only.
	FIREMODE_FORCED             = 3, //!< Fire continuously - allow any target.
	FIREMODE_AIM                = 4, //!< Aim target only - allow any target.
	FIREMODE_SECONDARY          = 5, //!< Fire secondary weapon (grenades,....).
	FIREMODE_SECONDARY_SMOKE    = 6, //!< Fire smoke grenade.
	FIREMODE_MELEE              = 7, //!< Melee.
	FIREMODE_KILL               = 8, //!< No missing, shoot directly at the target, no matter what aggression/attackRange/accuracy is.
	FIREMODE_BURST_WHILE_MOVING = 9, //!< (To be renamed.
	FIREMODE_PANIC_SPREAD       = 10,
	FIREMODE_BURST_DRAWFIRE     = 11,
	FIREMODE_MELEE_FORCED       = 12, //!< Melee, without distance restrictions.
	FIREMODE_BURST_SNIPE        = 13,
	FIREMODE_AIM_SWEEP          = 14,
	FIREMODE_BURST_ONCE         = 15, //!< Fire a single burst, then go to FIREMODE_OFF.
	FIREMODE_VEHICLE,

	FIREMODE_SERIALIZATION_HELPER_LAST,
	FIREMODE_SERIALIZATION_HELPER_FIRST = 0
};

// Todo: names are not coherent, but they can used together; rename all of them?.
enum EAIGoalFlags
{
	AILASTOPRES_USE            = 0x01,
	AILASTOPRES_LOOKAT         = 0x02,
	AI_LOOK_FORWARD            = 0x04,
	AI_MOVE_BACKWARD           = 0x08, //!< Default direction.
	AI_MOVE_RIGHT              = 0x10,
	AI_MOVE_LEFT               = 0x20,
	AI_MOVE_FORWARD            = 0x40,
	AI_MOVE_BACKRIGHT          = 0x80,
	AI_MOVE_BACKLEFT           = 0x100,
	AI_MOVE_TOWARDS_GROUP      = 0x200, //!< Towards the other group members.
	AI_REQUEST_PARTIAL_PATH    = 0x400,
	AI_BACKOFF_FROM_TARGET     = 0x800,
	AI_BREAK_ON_LIVE_TARGET    = 0x1000,
	AI_RANDOM_ORDER            = 0x2000,
	AI_CONSTANT_SPEED          = 0x2000, //!< Used in stick goal (not using AI_RANDOM_ORDER).
	AI_USE_TIME                = 0x4000, //!< When param can be interpreted as time or distance, use time.
	AI_STOP_ON_ANIMATION_START = 0x8000,
	AI_USE_TARGET_MOVEMENT     = 0x10000, //!< When requesting a direction (AI_MOVE_LEFT/RIGHT etc), make it relative to target movement direction.
	AI_ADJUST_SPEED            = 0x20000, //!< Used in stick goal forces to adjust the speed based on the target.
	AI_CHECK_SLOPE_DISTANCE    = 0x40000, //!< Used in backoff goal, checks the actual distance on slopes rather than horizontal.
};

//! Unit properties for group behavior (see CLeader/CLeaderAction).
//! These bit flags can be combined based on the unit capabilities.
enum EUnitProperties
{
	UPR_COMBAT_GROUND = 1,       //!< The unit can take part in ground actions.
	UPR_COMBAT_FLIGHT = 2,       //!< The unit can take part in flight actions.
	UPR_COMBAT_MARINE = 4,       //!< The unit can take part in marine actions.
	UPR_COMBAT_RECON  = 8,       //!< The unit can take part in recon actions.
	UPR_ALL           = -1,      //!< The unit can take part in all actions.
};

enum EAIUseAction
{
	AIUSEOP_NONE,
	AIUSEOP_PLANTBOMB,
	AIUSEOP_VEHICLE,
	AIUSEOP_RPG
};

enum  ELeaderAction
{
	LA_NONE,
	LA_HIDE,
	LA_HOLD,
	LA_ATTACK,
	LA_SEARCH,
	LA_FOLLOW,
	LA_USE,
	LA_USE_VEHICLE,
	LA_LAST,  // Make sure this one is always the last!
};

enum EAIAlertStatus
{
	AIALERTSTATUS_SAFE,
	AIALERTSTATUS_UNSAFE,
	AIALERTSTATUS_READY,
	AIALERTSTATUS_ACTION
};

enum EAITargetThreat
{
	AITHREAT_NONE,
	AITHREAT_SUSPECT,
	AITHREAT_INTERESTING,
	AITHREAT_THREATENING,
	AITHREAT_AGGRESSIVE,
	AITHREAT_LAST //!< For serialization.
};

enum EAITargetType
{
	// Atomic types.
	AITARGET_NONE,        //!< No target.
	AITARGET_SOUND,       //!< Primary sensory from sound event.
	AITARGET_MEMORY,      //!< Primary sensory from vis event, not visible.
	AITARGET_VISUAL,      //!< Primary sensory from vis event, visible.

	// Backwards compatibility for scriptbind.
	AITARGET_ENEMY,
	AITARGET_FRIENDLY,
	AITARGET_BEACON,
	AITARGET_GRENADE,
	AITARGET_RPG,

	//! For serialization.
	AITARGET_LAST
};

enum EAITargetContextType
{
	// Atomic types.
	AITARGET_CONTEXT_UNKNOWN,       //!< No specific subtype.
	AITARGET_CONTEXT_GUNFIRE,       //!< Gunfire subtype.

	//! For serialization.
	AITARGET_CONTEXT_LAST
};

enum EAITargetZone
{
	AIZONE_IGNORE = 0,      //!< Ignoring target zones.

	AIZONE_KILL,
	AIZONE_COMBAT_NEAR,
	AIZONE_COMBAT_FAR,
	AIZONE_WARN,
	AIZONE_OUT,

	AIZONE_LAST
};

enum EAIWeaponAccessories
{
	AIWEPA_NONE         = 0,
	AIWEPA_LASER        = 0x0001,
	AIWEPA_COMBAT_LIGHT = 0x0002,
	AIWEPA_PATROL_LIGHT = 0x0004,
};

//! In practice, rather than modifying/setting all these values explicitly when
//! making a pathfind request, it is suggested to set up bunch of constant sets
//! of properties, and use them.
struct AgentPathfindingProperties
{
	AgentPathfindingProperties(
	  IAISystem::tNavCapMask navCapMask,
	  float triangularResistanceFactor,
	  float waypointResistanceFactor,
	  float flightResistanceFactor,
	  float volumeResistanceFactor,
	  float roadResistanceFactor,
	  float waterResistanceFactor,
	  float maxWaterDepth,
	  float minWaterDepth,
	  float exposureFactor,
	  float dangerCost,
	  float zScale,
	  uint32 _customNavCapsMask = 0,
	  float radius = 0.3f,
	  float height = 2.0f,
	  float maxSlope = 0.0f,
	  int id = -1,
	  bool avoidObstacles = true)
		: navCapMask(navCapMask), triangularResistanceFactor(triangularResistanceFactor), waypointResistanceFactor(waypointResistanceFactor),
		flightResistanceFactor(flightResistanceFactor), volumeResistanceFactor(volumeResistanceFactor), roadResistanceFactor(roadResistanceFactor),
		waterResistanceFactor(waterResistanceFactor), maxWaterDepth(maxWaterDepth), minWaterDepth(minWaterDepth),
		exposureFactor(exposureFactor), dangerCost(dangerCost),
		zScale(zScale), customNavCapsMask(_customNavCapsMask),
		radius(radius), height(height), maxSlope(maxSlope), id(id),
		avoidObstacles(avoidObstacles)
	{ if (maxWaterDepth < minWaterDepth) maxWaterDepth = minWaterDepth; }

	AgentPathfindingProperties() { SetToDefaults(); }

	void SetToDefaults()
	{
		navCapMask = IAISystem::NAVMASK_ALL;
		triangularResistanceFactor = 0.0f;
		waypointResistanceFactor = 0.0f;
		flightResistanceFactor = 0.0f;
		volumeResistanceFactor = 0.0f;
		roadResistanceFactor = 0.0f;
		waterResistanceFactor = 0.0f;
		maxWaterDepth = 10000.0f;
		minWaterDepth = -10000.0f;
		exposureFactor = 0.0f;
		dangerCost = 0.0f;    // Cost (in meters) per dead body in the destination node!
		zScale = 1.0f;
		customNavCapsMask = 0;
		radius = 0.3f;
		height = 2.0f;
		maxSlope = 0.0f;
		id = -1;
		avoidObstacles = true;
	}

	//! The type of nodes this agent can traverse.
	//! Expected to normally be set just once when the agent is created.
	IAISystem::tNavCapMask navCapMask;

	//! Basic extra costs associated with traversing each node type.
	//! Everything below here could be reasonably set/modified each time
	//! a pathfind request is made.
	float triangularResistanceFactor;
	float waypointResistanceFactor;
	float flightResistanceFactor;
	float volumeResistanceFactor;
	float roadResistanceFactor;

	//! Additional cost modifiers.
	float waterResistanceFactor;

	//! Only travel if water depth is between min and max values.
	float maxWaterDepth;
	float minWaterDepth;
	float exposureFactor;
	float dangerCost;

	//! Quantity the z component should be scaled by when calculating distances.
	float  zScale;
	uint32 customNavCapsMask;

	float  radius;
	float  height;
	float  maxSlope;

	// Unique id of this instance among others. If there were an array
	// of these structs this could have been an index into that array.
	int  id;
	bool avoidObstacles;
};

struct AgentMovementSpeeds
{
	enum EAgentMovementUrgency { AMU_SLOW, AMU_WALK, AMU_RUN, AMU_SPRINT, AMU_NUM_VALUES };
	enum EAgentMovementStance
	{
		AMS_RELAXED, AMS_COMBAT, AMS_STEALTH, AMS_ALERTED, AMS_LOW_COVER, AMS_HIGH_COVER, AMS_CROUCH,
		AMS_PRONE, AMS_SWIM, AMS_NUM_VALUES
	};

	struct SSpeedRange
	{
		float def, min, max;
	};

	AgentMovementSpeeds()
	{
		memset(this, 0, sizeof(*this));
	}

	void SetBasicSpeeds(float slow, float walk, float run, float sprint, float maneuver)
	{
		for (int s = 0; s < AMS_NUM_VALUES; ++s)
		{
			speedRanges[s][AMU_SLOW].def = slow;
			speedRanges[s][AMU_SLOW].min = maneuver;
			speedRanges[s][AMU_SLOW].max = slow;

			speedRanges[s][AMU_WALK].def = walk;
			speedRanges[s][AMU_WALK].min = maneuver;
			speedRanges[s][AMU_WALK].max = walk;

			speedRanges[s][AMU_RUN].def = run;
			speedRanges[s][AMU_RUN].min = maneuver;
			speedRanges[s][AMU_RUN].max = run;

			speedRanges[s][AMU_SPRINT].def = sprint;
			speedRanges[s][AMU_SPRINT].min = maneuver;
			speedRanges[s][AMU_SPRINT].max = sprint;
		}

	}

	void SetRanges(int stance, int urgency, float sdef, float smin, float smax)
	{
		assert(stance >= 0 && stance < AMS_NUM_VALUES);
		assert(urgency >= 0 && urgency < AMU_NUM_VALUES);
		speedRanges[stance][urgency].def = sdef;
		speedRanges[stance][urgency].min = smin;
		speedRanges[stance][urgency].max = smax;
	}

	void CopyRanges(int stance, int toUrgency, int fromUrgency)
	{
		assert(stance >= 0 && stance < AMS_NUM_VALUES);
		assert(toUrgency >= 0 && toUrgency < AMU_NUM_VALUES);
		assert(fromUrgency >= 0 && fromUrgency < AMU_NUM_VALUES);
		speedRanges[stance][toUrgency] = speedRanges[stance][fromUrgency];
	}

	const SSpeedRange& GetRange(int stance, int urgency) const
	{
		assert(stance >= 0 && stance < AMS_NUM_VALUES);
		assert(urgency >= 0 && urgency < AMU_NUM_VALUES);
		return speedRanges[stance][urgency];
	}

	//! Non-const version needed for serialisation.
	SSpeedRange& GetRange(int stance, int urgency)
	{
		assert(stance >= 0 && stance < AMS_NUM_VALUES);
		assert(urgency >= 0 && urgency < AMU_NUM_VALUES);
		return speedRanges[stance][urgency];
	}

private:
	SSpeedRange speedRanges[AMS_NUM_VALUES][AMU_NUM_VALUES];
};

enum EAgentAvoidanceAbilities
{
	eAvoidance_NONE             = 0,

	eAvoidance_Vehicles         = 0x01, //!< Agent can avoid vehicles.
	eAvoidance_Actors           = 0x02, //!< Agent can avoid puppets - DEPRECATED.
	eAvoidance_Players          = 0x04, //!< Agent can avoid players - DEPRECATED.

	eAvoidance_StaticObstacle   = 0x10, //!< Agent can avoid static physical objects (non-pushable).
	eAvoidance_PushableObstacle = 0x20, //!< Agent can avoid pushable objects.

	eAvoidance_DamageRegion     = 0x100, //!< Agent can avoid damage regions.

	eAvoidance_ALL              = 0xFFFF,
	eAvoidance_DEFAULT          = eAvoidance_ALL, //!< Avoid all by default.
};

//! Structure containing all the relevant information describing the way agent moves (can strafe,speeds,inertia,...).
struct AgentMovementAbility
{
	bool  b3DMove;
	bool  bUsePathfinder;
	bool  usePredictiveFollowing;
	bool  allowEntityClampingByAnimation;
	float maxAccel;
	float maxDecel;
	float minTurnRadius;
	float maxTurnRadius;
	float avoidanceRadius;
	float pathLookAhead;              //!< How far to look ahead when path following.
	float pathRadius;                 //!< How wide the path should be treated.
	float pathSpeedLookAheadPerSpeed; //!< How far to look ahead to do speed control (mult this by the speed); -ve means use pathLookAhead.
	float cornerSlowDown;             //!< Slow down at corners: 0 to 1.
	float slopeSlowDown;              //!< Slow down on slopes uphill (0 to 1).
	float optimalFlightHeight;
	float minFlightHeight;
	float maxFlightHeight;
	float maneuverTrh;        //!< Cosine of forward^desired angle, after which to start maneuvering.
	float velDecay;           //!< How fast velocity decays with cos(fwdDir^pathDir) .
	float pathFindPrediction; //!< Time in seconds how much the path finder will predict the start of the path.

	//! Enables/disables the path getting regenerated during tracing (needed for crowd control and dynamic
	//! updates to graph) - regeneration interval (in seconds) - 0 disables.
	float pathRegenIntervalDuringTrace;

	//! Enables/disables teleporting when path following.
	bool teleportEnabled;

	//! Sets to true if the movement speed should be lowered in low light conditions.
	bool lightAffectsSpeed;

	//! Adjusts the movement speed based on the angel between body dir and move dir.
	//! Enables/disables the algorithm to attempt to handle the agent getting stuck during tracing.
	bool  resolveStickingInTrace;
	float directionalScaleRefSpeedMin;
	float directionalScaleRefSpeedMax;

	//! Adjusts how the agent pathfollows around dynamic obstacles, both pushable and non-pushable
	uint32 avoidanceAbilities;
	bool   pushableObstacleWeakAvoidance;
	float  pushableObstacleAvoidanceRadius;

	//! Defines the mass range of obstacles that can be pushed by this agent
	//! If mass < min, agent will not avoid it
	//! If mass > max, agent will always avoid it
	//! If mass is in-between, agent will avoid it with an avoidanceRadius adjusted by the ratio between
	float pushableObstacleMassMin;
	float pushableObstacleMassMax;

	//! True if the agent should participate in collision avoidance; false if not.
	//! \note If disabled, other agents don't avoid it either!.
	bool collisionAvoidanceParticipation;

	//! When moving, this increment value will be progressively added
	//! to the agents avoidance radius (the rate is defined with the
	//! CollisionAvoidanceRadiusIncrementIncreaseRate/DecreaseRate
	//! Console variables)
	//! This is meant to make the agent keep a bit more distance between
	//! itself and other agents/obstacles.
	float                      collisionAvoidanceRadiusIncrement;

	AgentMovementSpeeds        movementSpeeds;

	AgentPathfindingProperties pathfindingProperties;

	AgentMovementAbility() : b3DMove(false), bUsePathfinder(true), usePredictiveFollowing(false), allowEntityClampingByAnimation(false),
		maxAccel(std::numeric_limits<float>::max()), maxDecel(std::numeric_limits<float>::max()),
		minTurnRadius(0), maxTurnRadius(0),
		avoidanceRadius(0), pathLookAhead(3.0f), pathRadius(1.0f), pathSpeedLookAheadPerSpeed(-1), cornerSlowDown(0.0f), slopeSlowDown(1.f),
		optimalFlightHeight(0), minFlightHeight(0), maxFlightHeight(0), maneuverTrh(0.f),
		velDecay(0.0f), pathFindPrediction(0), resolveStickingInTrace(true), pathRegenIntervalDuringTrace(0.0f),
		teleportEnabled(false), lightAffectsSpeed(false),
		directionalScaleRefSpeedMin(-1.0f), directionalScaleRefSpeedMax(-1.0f),
		avoidanceAbilities(eAvoidance_DEFAULT), pushableObstacleWeakAvoidance(false), pushableObstacleAvoidanceRadius(0.35f),
		pushableObstacleMassMin(0.0f), pushableObstacleMassMax(0.0f), collisionAvoidanceRadiusIncrement(0.0f),
		collisionAvoidanceParticipation(true)
	{}
};

struct AIObjectParams
{
	AgentParameters      m_sParamStruct;
	AgentMovementAbility m_moveAbility;

	uint16               type;
	IAIObject*           association;
	const char*          name;
	EntityId             entityID;

	AIObjectParams()
		: type(0)
		, association(0)
		, name(0)
		, entityID(0)
	{
	}

	AIObjectParams(uint16 _type, const AgentParameters& sParamStruct, const AgentMovementAbility& moveAbility)
		: m_sParamStruct(sParamStruct)
		, m_moveAbility(moveAbility)
		, type(_type)
		, association(0)
		, name(0)
		, entityID(0)
	{
	}

	AIObjectParams(uint16 _type, IAIObject* _association = 0, EntityId _entityID = 0)
		: type(_type)
		, association(_association)
		, name(0)
		, entityID(_entityID)
	{
	}
};

struct AIWeaponDescriptor
{
	string firecmdHandler;
	float  fSpeed;          //!< Relevant for projectiles only.
	float  fDamageRadius;   //!< Explosion radius for rockets/grenades.
	float  fChargeTime;
	float  fRangeMax;
	float  fRangeMin;
	bool   bSignalOnShoot;
	int    burstBulletCountMin;   //!< The burst bullet count is scaled between min and max based on several factors (distance, visibility).
	int    burstBulletCountMax;   //!< Each burst is followed by a pause.
	float  burstPauseTimeMin;     //!< The burst pause is scale between min and max.
	float  burstPauseTimeMax;
	float  singleFireTriggerTime; //!< Interval between single shot shots. Scale randomly between 70-125%. Set to -1 for continuous fire.
	float  spreadRadius;          //!< Miss spread radius in meters.
	float  coverFireTime;         //!< How long the shooting should continue after the target is not visible anymore.
	float  drawTime;              //!< How long the AI will shoot around the target before aiming it precisely.
	float  sweepWidth;            //!< How wide is the sweep left/right around the target direction during.
	float  sweepFrequency;        //!< How fast the sweep left/right around the target direction during.
	float  pressureMultiplier;
	float  lobCriticalDistance;
	float  preferredHeight;                 //!< Preferred height used to throw the bullet if using a slow_projectile prediction.
	float  closeDistance;                   //!< What to consider close distance during the slow_projectile prediction.
	float  preferredHeightForCloseDistance; //!< Preferred height used to throw the bullet if using a slow_projectile prediction for close distance.
	float  maxAcceptableDistanceFromTarget; //!< Maximum distance allowed to evaluate the landing of the projectile. If less than 0 it's not taken into account.
	float  minimumDistanceFromFriends;      //!< Minimum distance to agents of a friendly faction. For now it is only used in the lob fire command by overriding the default value.
	Vec3   projectileGravity;               //!< What's the gravity of the fired projectile - Used only for predicting grenade landing.
	string smartObjectClass;

	AIWeaponDescriptor(const string& fcHandler = "instant", float chargeTime = -1.0f, float speed = -1.0f, float damageRadius = -1.0f)
		: firecmdHandler(fcHandler)
		, fSpeed(speed)
		, fDamageRadius(damageRadius)
		, fChargeTime(chargeTime)
		, fRangeMax(1000.f)
		, fRangeMin(1.f)
		, bSignalOnShoot(false)
		, burstBulletCountMin(1)
		, burstBulletCountMax(10)
		, burstPauseTimeMin(0.8f)
		, burstPauseTimeMax(3.5f)
		, singleFireTriggerTime(-1.0f)
		, spreadRadius(1.0f)
		, coverFireTime(2.0f)
		, drawTime(3.0f)
		, sweepWidth(2.5f)
		, sweepFrequency(3.0f)
		, pressureMultiplier(1.0f)
		, lobCriticalDistance(5.0f)
		, preferredHeight(5.0f)
		, closeDistance(0.0f)
		, preferredHeightForCloseDistance(5.0)
		, maxAcceptableDistanceFromTarget(-1.0f)
		, minimumDistanceFromFriends(-1.0f)
		, projectileGravity(0.0f, 0.0f, -9.8f)
	{
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(smartObjectClass);
		pSizer->AddObject(firecmdHandler);
	}
};

//! The description of the various hazardous areas related to the weapon.
struct HazardWeaponDescriptor
{
	//! The maximum look ahead distance for constructing the waring area (>= 0.0f) (in meters).
	float maxHazardDistance;

	//! The radius of the hazard warning area (>= 0.0f) (in meters).
	float hazardRadius;

	//! How far to nudge the start of the warn area in the forwards direction from
	//! the pivot position (in case the gun sticks out quite a bit for example)
	//! (in meters). Negative values will nudge backwards.
	float startPosNudgeOffset;

	//! How much we allow the hazard area approximation to deviate from the actual position of
	//! its source (>= 0.0f) (in meters).
	float maxHazardApproxPosDeviation;

	//! How much we allow the hazard area approximation deviate from the actual movement
	//! direction of its source (>= 0.0f) (in degrees).
	float maxHazardApproxAngleDeviationDeg;

	HazardWeaponDescriptor()
		: maxHazardDistance(40.0f)
		, hazardRadius(4.0f)
		, startPosNudgeOffset(0.0f)
		, maxHazardApproxPosDeviation(5.0f)
		, maxHazardApproxAngleDeviationDeg(10.0f)
	{
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}
};

//! Memory fire control.
enum EMemoryFireType
{
	eMFT_Disabled = 0,     //!< Never allowed to fire at memory.
	eMFT_UseCoverFireTime, //!< Can fire at memory using the weapon's cover fire time.
	eMFT_Always,           //!< Always allowed to fire at memory.

	eMFT_COUNT,
};

//! Current fire state of the AI based on its current fire command handler.
enum EAIFireState
{
	eAIFS_Off = 0,
	eAIFS_On,
	eAIFS_Blocking, //!< Fire command handler is doing extra work and is not ready to fire yet.

	eAIFS_COUNT,
};

//! Projectile information to be used for firing weapon.
struct SFireCommandProjectileInfo
{
	EntityId trackingId;
	float    fSpeedScale;

	SFireCommandProjectileInfo() : trackingId(0), fSpeedScale(1.0f) {}
	void Reset()
	{
		trackingId = 0;
		fSpeedScale = 1.0f;
	}
};

//! Interface to the fire command handler.
struct IFireCommandHandler
{
	// <interfuscator:shuffle>
	virtual ~IFireCommandHandler(){}

	//! Gets the name identifier of the handler.
	virtual const char* GetName() = 0;

	//! Reset is called each time.
	virtual void Reset() = 0;

	//! Update is called on each AI update when the target is valid.
	virtual EAIFireState Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos) = 0;

	//! Checks if it's ok to shoot with this weapon in fireVector - to see if no friends are hit, no ostacles nearby hit, etc.
	virtual bool ValidateFireDirection(const Vec3& fireVector, bool mustHit) = 0;

	//! Deletes the handler.
	virtual void Release() = 0;

	//! Draws debug information.
	virtual void DebugDraw() = 0;

	//! Returns true if default effect should be used for special firemode.
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const = 0;

	//! Reloads a weapon.
	//! Called whenever weapon is reloaded.
	virtual void OnReload() = 0;

	//! Shot event function.
	//! Called whenever shot is done (if weapon descriptor defines EnableWeaponListener).
	virtual void OnShoot() = 0;

	//! Returns how many shots were done.
	virtual int GetShotCount() const = 0;

	//! Used by fire command handler to fill in information about projectiles to be fired by the AI.
	virtual void GetProjectileInfo(SFireCommandProjectileInfo& info) const     {};

	virtual bool GetOverrideAimingPosition(Vec3& overrideAimingPosition) const { return false; }

	//! If this firemode has a pause timer i.e. burst pause, this should return the current value of that timer.
	//! or if the fire mode has a slow fire rate i.e. rocket launcher it should return the fire timer - 0 otherwise.
	virtual float GetTimeToNextShot() const = 0;

	virtual void  GetMemoryUsage(ICrySizer* pSizer) const {}
	// </interfuscator:shuffle>
};

struct SAIEVENT
{
	bool     bFuzzySight;
	bool     bSetObserver;
	int      nDeltaHealth;
	float    fThreat;
	int      nType;
	int      nFlags;
	Vec3     vPosition;
	Vec3     vStimPos;
	Vec3     vForcedNavigation;
	EntityId sourceId;
	EntityId targetId;

	SAIEVENT() : bFuzzySight(false), bSetObserver(false), nDeltaHealth(0), fThreat(0.f),
		nType(0), nFlags(0), vPosition(ZERO), vStimPos(ZERO), vForcedNavigation(ZERO), sourceId(0), targetId(0) {}
};

enum EObjectResetType
{
	AIOBJRESET_INIT,
	AIOBJRESET_SHUTDOWN,
};

enum EGoalPipeEvent
{
	ePN_Deselected,    //!< Sent if replaced by selecting other pipe.
	ePN_Finished,      //!< Sent if reached end of pipe.
	ePN_Inserted,      //!< Sent if pipe is inserted with InsertPipe().
	ePN_Suspended,     //!< Sent if other pipe was inserted.
	ePN_Resumed,       //!< Sent if resumed after finishing inserted pipes.
	ePN_Removed,       //!< Sent if inserted pipe was removed with RemovePipe().
	ePN_Exiting,       //!< Sent if a non-looped pipe is completed.

	ePN_AnimStarted,   //!< Sent when exact positioning animation is started.
	ePN_RefPointMoved, //!< Sent to the last inserted goal pipe when the ref. point is moved.
};

// Todo: Figure out better way to handle this, the structure is almost 1:1 to the SActorTargetParams.
typedef uint32 TAnimationGraphQueryID;
struct SAIActorTargetRequest
{
	SAIActorTargetRequest() :
		id(0),
		approachLocation(ZERO),
		approachDirection(ZERO),
		animLocation(ZERO),
		animDirection(ZERO),
		vehicleSeat(0),
		speed(0),
		directionTolerance(0),
		startArcAngle(0),
		startWidth(0),
		loopDuration(-1),
		signalAnimation(true),
		projectEndPoint(true),
		lowerPrecision(false),
		useAssetAlignment(false),
		stance(STANCE_NULL),
		pQueryStart(0),
		pQueryEnd(0)
	{
	}

	void Reset()
	{
		id = 0;
		approachLocation.zero();
		approachDirection.zero();
		animLocation.zero();
		animDirection.zero();
		vehicleSeat = 0;
		speed = 0;
		directionTolerance = 0;
		startArcAngle = 0;
		startWidth = 0;
		loopDuration = -1;
		signalAnimation = true;
		projectEndPoint = true;
		lowerPrecision = false;
		useAssetAlignment = false;
		stance = STANCE_NULL;
		pQueryStart = 0;
		pQueryEnd = 0;
		vehicleName = "";
		animation = "";
	}

	void Serialize(TSerialize ser);

	int                     id; //!< Id=0 means invalid.
	Vec3                    approachLocation;
	Vec3                    approachDirection;
	Vec3                    animLocation;
	Vec3                    animDirection;
	string                  vehicleName;
	int                     vehicleSeat;
	float                   speed;
	float                   directionTolerance;
	float                   startArcAngle;
	float                   startWidth;
	float                   loopDuration; //!< -1 = forever.
	bool                    signalAnimation;
	bool                    projectEndPoint;
	bool                    lowerPrecision; //!< Lower precision should be true when passing through a navSO.
	bool                    useAssetAlignment;
	string                  animation;
	EStance                 stance;
	TAnimationGraphQueryID* pQueryStart;
	TAnimationGraphQueryID* pQueryEnd;
};

enum CoverHeight
{
	LowCover,
	HighCover
};

enum EBodyOrientationMode
{
	FullyTowardsMovementDirection,
	FullyTowardsAimOrLook,
	HalfwayTowardsAimOrLook
};

struct IPipeUser
{
	typedef std::shared_ptr<Vec3> LookTargetPtr;
	// <interfuscator:shuffle>
	virtual ~IPipeUser() {}
	virtual bool       SelectPipe(int id, const char* name, IAIObject* pArgument = 0, int goalPipeId = 0, bool resetAlways = false, const GoalParams* node = 0) = 0;
	virtual IGoalPipe* InsertSubPipe(int id, const char* name, IAIObject* pArgument = 0, int goalPipeId = 0, const GoalParams* node = 0) = 0;
	virtual bool       CancelSubPipe(int goalPipeId) = 0;
	virtual bool       RemoveSubPipe(int goalPipeId, bool keepInserted = false) = 0;
	virtual bool       IsUsingPipe(const char* name) const = 0;
	virtual bool       IsUsingPipe(int goalPipeId) const = 0;

	virtual void       Pause(bool pause) = 0;
	virtual bool       IsPaused() const = 0;

	virtual IAIObject* GetAttentionTargetAssociation() const = 0;
	virtual IAIObject* GetLastOpResult() = 0;
	virtual void       ResetCurrentPipe(bool bAlways) {; }
	virtual IAIObject* GetSpecialAIObject(const char* objName, float range = 0.0f) = 0;
	virtual void       MakeIgnorant(bool ignorant) = 0;
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	virtual bool       SetCharacter(const char* character, const char* behaviour = NULL) = 0;
#endif

	virtual void        SetInCover(bool bInCover) = 0;
	virtual void        SetMovingToCover(bool bMovingInCover) = 0;
	virtual void        SetCoverCompromised() = 0;
	virtual bool        IsCoverCompromised() const = 0;
	virtual bool        IsInCover() const = 0;
	virtual bool        IsMovingToCover() const = 0;
	virtual bool        IsTakingCover(float distanceThreshold) const = 0;

	virtual void        ClearDevalued() = 0;

	virtual void        RegisterGoalPipeListener(IGoalPipeListener* pListener, int goalPipeId, const char* debugClassName) = 0;
	virtual void        UnRegisterGoalPipeListener(IGoalPipeListener* pListener, int goalPipeId) = 0;
	virtual int         GetGoalPipeId() const = 0;

	virtual IGoalPipe*  GetCurrentGoalPipe() = 0;

	virtual void        SetAllowedStrafeDistances(float start, float end, bool whileMoving) = 0;

	virtual void        SetExtraPriority(float priority) = 0;
	virtual float       GetExtraPriority(void) = 0;

	virtual IAIObject*  GetRefPoint() = 0;
	virtual void        SetRefPointPos(const Vec3& pos) = 0;
	virtual void        SetRefPointPos(const Vec3& pos, const Vec3& dir) = 0;
	virtual void        SetRefShapeName(const char* shapeName) = 0;
	virtual const char* GetRefShapeName() const = 0;

	virtual void        SetActorTargetRequest(const SAIActorTargetRequest& req) = 0;
	virtual void        ClearActorTargetRequest() = 0;

	virtual EntityId    GetLastUsedSmartObjectId() const = 0;
	virtual bool        IsUsingNavSO() const = 0;
	virtual void        ClearPath(const char* dbgString) = 0;

	virtual void        SetFireMode(EFireMode mode) = 0;
	virtual EFireMode   GetFireMode() const = 0;

	//! Adds the current hideobject position to ignore list (will be ignored for specified time).
	virtual void IgnoreCurrentHideObject(float timeOut = 10.0f) = 0;

	//! \returns most probable target position or the target if it is visible.
	virtual Vec3 GetProbableTargetPosition() = 0;

	//! Last finished AIAction sets status as succeed or failed.
	virtual void          SetLastActionStatus(bool bSucceed) = 0;

	virtual void          AllowLowerBodyToTurn(bool bAllowLowerBodyToTurn) = 0;
	virtual bool          IsAllowingBodyTurn() = 0;

	virtual LookTargetPtr CreateLookTarget() = 0;
	// </interfuscator:shuffle>
};

struct IGoalPipeListener
{
	virtual void OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent) = 0;

private:
	friend class CPipeUser;

	typedef DynArray<std::pair<IPipeUser*, int>> VectorRegisteredPipes;
	VectorRegisteredPipes _vector_registered_pipes;

protected:
	virtual ~IGoalPipeListener()
	{
		assert(_vector_registered_pipes.empty());
		while (_vector_registered_pipes.empty() == false)
		{
			_vector_registered_pipes.back().first->UnRegisterGoalPipeListener(this, _vector_registered_pipes.back().second);
		}
	}
};

struct ISmartObjectManager
{
	typedef IStatObj* IStatObjPtr;

	// <interfuscator:shuffle>
	virtual ~ISmartObjectManager(){}
	virtual void        RemoveSmartObject(IEntity* pEntity) = 0;

	virtual const char* GetSmartObjectStateName(int id) const = 0;

	virtual void        RegisterSmartObjectState(const char* sStateName) = 0;
	virtual void        AddSmartObjectState(IEntity* pEntity, const char* sState) = 0;
	virtual void        RemoveSmartObjectState(IEntity* pEntity, const char* sState) = 0;
	virtual bool        CheckSmartObjectStates(const IEntity* pEntity, const char* patternStates) const = 0;
	virtual void        SetSmartObjectState(IEntity* pEntity, const char* sStateName) = 0;
	virtual void        ModifySmartObjectStates(IEntity* pEntity, const char* listStates) = 0;

	virtual void        DrawSOClassTemplate(IEntity* pEntity) = 0;     // TODO make const.
	virtual bool        ValidateSOClassTemplate(IEntity* pEntity) = 0; // TODO make const.

	//! Call with ppStatObjects == NULL to get numAllocStatObjects.
	//! Call with allocated ppStatObjects array with numAllocStatObjects, returns found #.
	virtual uint32             GetSOClassTemplateIStatObj(IEntity* pEntity, IStatObjPtr* ppStatObjects, uint32& numAllocStatObjects) = 0; // TODO make this const at some point if possible.

	virtual void               ReloadSmartObjectRules() = 0;

	virtual int                SmartObjectEvent(const char* sEventName, IEntity*& pUser, IEntity*& pObject, const Vec3* pExtraPoint = NULL, bool bHighPriority = false) = 0;

	virtual SmartObjectHelper* GetSmartObjectHelper(const char* className, const char* helperName) const = 0;
	// </interfuscator:shuffle>
};

struct IAIAction;

struct IAIActionManager : public IGoalPipeListener
{
	// <interfuscator:shuffle>
	//! \return An existing AI Action from the library or NULL if not found.
	virtual IAIAction* GetAIAction(const char* sName) = 0;

	//! \return An existing AI Action by its index in the library or NULL index is out of range.
	virtual IAIAction* GetAIAction(size_t index) = 0;

	virtual void       ReloadActions() = 0;

	//! Aborts specific AI Action (specified by goalPipeId) or all AI Actions (goalPipeId == 0) in which pEntity is a user.
	virtual void AbortAIAction(IEntity* pEntity, int goalPipeId = 0) = 0;

	//! Finishes specific AI Action (specified by goalPipeId) for the pEntity as a user.
	virtual void FinishAIAction(IEntity* pEntity, int goalPipeId) = 0;
	// </interfuscator:shuffle>
};

struct IPuppet
{
	// <interfuscator:shuffle>
	virtual ~IPuppet() {}
	virtual void       UpTargetPriority(const IAIObject* pTarget, float fPriorityIncrement) = 0;
	virtual void       UpdateBeacon() = 0;

	virtual void       SetAlarmed() = 0;
	virtual bool       IsAlarmed() const = 0;
	virtual float      GetPerceptionAlarmLevel() const = 0;

	virtual IAIObject* MakeMeLeader() = 0;

	//! Gets the owner of a dummy AI Object.
	virtual IAIObject* GetEventOwner(IAIObject* pObject) const = 0;

	//! Where to shoot if need to miss now (target is another AI) - select some point around target.
	virtual Vec3 ChooseMissPoint_Deprecated(const Vec3& targetPos) const = 0;

	//! Checks if there are friends in the line of fire.
	//! If the cheapTest flag is set, only cone test is performed, otherwise raycasting.
	//! \return true and sends signal "OnFriendInWay" if there is a friendly agent in the line of fire.
	virtual bool CheckFriendsInLineOfFire(const Vec3& fireDirection, bool cheapTest) = 0;

	//! Gets the distance of an AI object to this, along this path.
	//! \note Must be called with bInit=true first time and then false other time to avoid considering path regeneration after.
	virtual float                     GetDistanceAlongPath(const Vec3& pos, bool bInit) = 0;

	virtual void                      EnableFire(bool enable) = 0;
	virtual bool                      IsFireEnabled() const = 0;
	virtual void                      EnableCoverFire(bool enable) = 0;
	virtual bool                      IsCoverFireEnabled() const = 0;
	virtual bool                      GetPosAlongPath(float dist, bool extrapolateBeyond, Vec3& retPos) const = 0;
	virtual IFireCommandHandler*      GetFirecommandHandler() const = 0;

	virtual const AIWeaponDescriptor& GetCurrentWeaponDescriptor() const = 0;

	//! Changes flag so this puppet can be shoot or not.
	virtual void SetCanBeShot(bool bCanBeShot) {; }
	virtual bool GetCanBeShot() const          { return true; }

	//! Set how this puppet treats firing at its memory target.
	virtual void            SetMemoryFireType(EMemoryFireType eType) {; }
	virtual EMemoryFireType GetMemoryFireType() const                { return eMFT_UseCoverFireTime; }
	virtual bool            CanMemoryFire() const                    { return true; }

	//! Helper to get the puppet's perceived position of an agent.
	//! \return true if puppet has a perceived location for this target.
	virtual bool GetPerceivedTargetPos(IAIObject* pTarget, Vec3& vPos) const = 0;

	//! ROD Handler.
	virtual void SetRODHandler(IAIRateOfDeathHandler* pHandler) = 0;
	virtual void ClearRODHandler() = 0;
	// </interfuscator:shuffle>
};

struct IAISignalExtraData
{
	// <interfuscator:shuffle>
	virtual ~IAISignalExtraData(){}
	virtual void        Serialize(TSerialize ser) = 0;
	virtual const char* GetObjectName() const = 0;
	virtual void        SetObjectName(const char* objectName) = 0;

	virtual void        ToScriptTable(SmartScriptTable& table) const = 0;
	virtual void        FromScriptTable(const SmartScriptTable& table) = 0;
	// </interfuscator:shuffle>

	IAISignalExtraData() : string1(NULL), string2(NULL) {}

	Vec3         point;
	Vec3         point2;
	ScriptHandle nID;
	float        fValue;
	int          iValue;
	int          iValue2;
	const char*  string1;
	const char*  string2;
};

// Put new signal here!

struct AISIGNAL
{
	int                 nSignal;
	uint32              m_nCrcText;
	EntityId            senderID;
	IAISignalExtraData* pEData;
	static const int    SIGNAL_NAME_LENGTH = 50;
	char                strText[SIGNAL_NAME_LENGTH];

	AISIGNAL()
		: nSignal(0)
		, m_nCrcText(0)
		, senderID(0)
		, pEData(NULL)
	{
		strText[0] = 0;
	}

	void        Serialize(TSerialize ser);

	inline bool Compare(uint32 crc) const
	{
		return m_nCrcText == crc;
	}
};

struct PATHPOINT
{
	Vec3 vPos;
	Vec3 vDir;
	PATHPOINT(
	  const Vec3& vPos = Vec3Constants<float>::fVec3_Zero,
	  const Vec3& vDir = Vec3Constants<float>::fVec3_Zero)
		: vPos(vPos), vDir(vDir) {}
};
typedef DynArray<PATHPOINT> PATHPOINTVECTOR;

//! Current phase of actor target animation.
enum EActorTargetPhase
{
	eATP_None,
	eATP_Waiting,
	eATP_Starting,
	eATP_Started,
	eATP_Playing,
	eATP_StartedAndFinished,
	eATP_Finished,
	eATP_Error,
};

enum EAITargetStuntReaction
{
	AITSR_NONE,
	AITSR_SEE_STUNT_ACTION,
	AITSR_SEE_CLOAKED,
	AITSR_LAST
};

//! Current phase of actor target animation.
enum ERequestedGrenadeType
{
	eRGT_INVALID = -1,

	eRGT_ANY,
	eRGT_SMOKE_GRENADE,
	eRGT_FLASHBANG_GRENADE,
	eRGT_FRAG_GRENADE,
	eRGT_EMP_GRENADE,
	eRGT_GRUNT_GRENADE,

	eRGT_COUNT,
};

struct SAIPredictedCharacterState
{
	SAIPredictedCharacterState() : predictionTime(0.f), position(ZERO), velocity(ZERO) {}
	void Set(const Vec3& pos, const Vec3& vel, float predT)
	{
		position = pos;
		velocity = vel;
		predictionTime = predT;
	}
	void Serialize(TSerialize ser);

	Vec3  position;
	Vec3  velocity;
	float predictionTime; //!< Time of prediction relative to when it was made.
};

struct SAIPredictedCharacterStates
{
	SAIPredictedCharacterStates() : nStates(0){}
	void Serialize(TSerialize ser);

	enum {maxStates = 32};
	SAIPredictedCharacterState states[maxStates];
	int                        nStates;
};

enum ECoverBodyDirection
{
	eCoverBodyDirection_Unspecified,
	eCoverBodyDirection_Left,
	eCoverBodyDirection_Right,
};

enum ECoverAction
{
	eCoverAction_Unspecified,
	eCoverAction_Set,
	eCoverAction_Clear,
};

enum ECoverLocationRequest
{
	eCoverLocationRequest_Unspecified,
	eCoverLocationRequest_Set,
	eCoverLocationRequest_Clear,
};

struct SAICoverRequest
{
	SAICoverRequest()
		: coverBodyDirection(eCoverBodyDirection_Unspecified)
		, coverAction(eCoverAction_Unspecified)
		, coverActionName(NULL)
		, coverLocation(IDENTITY)
		, coverLocationRequest(eCoverLocationRequest_Unspecified)
	{}

	//! CoverNormal points OUT of the cover surface.
	void SetCoverBodyDirection(const Vec3& coverNormal, const Vec3& targetDirection)
	{
		const Vec2 coverNormal2D(coverNormal);
		const Vec2 targetDirection2D(targetDirection);
		const float cross = coverNormal2D.Cross(targetDirection2D);

		//assert( coverBodyDirection == eCoverBodyDirection_Unspecified ); // Cannot be guaranteed yet
		coverBodyDirection = (cross < 0.0f) ? eCoverBodyDirection_Left : eCoverBodyDirection_Right;
	}

	//! CoverNormal points OUT of the cover surface.
	void SetCoverBodyDirectionWithThreshold(const Vec3& coverNormal, const Vec3& targetDirection, const float thresholdAngleRadians)
	{
		const Vec2 coverNormal2D(coverNormal);
		const Vec2 targetDirection2D = Vec2(targetDirection).GetNormalizedSafe(Vec2(0, 1));
		const float dot = (-coverNormal2D).Dot(targetDirection2D);

		//assert( coverBodyDirection == eCoverBodyDirection_Unspecified ); // Cannot be guaranteed yet
		const float threshold = cos(thresholdAngleRadians);
		if (dot <= threshold)
		{
			const float cross = coverNormal2D.Cross(targetDirection2D);
			coverBodyDirection = (cross < 0.0f) ? eCoverBodyDirection_Left : eCoverBodyDirection_Right;
		}
	}

	void SetCoverAction(const char* const name, const float postureLean)
	{
		coverActionName = name;
		coverAction = (coverActionName && coverActionName[0]) ? eCoverAction_Set : eCoverAction_Clear;

		//assert( coverBodyDirection == eCoverBodyDirection_Unspecified ); // Cannot be guaranteed yet
		if (postureLean < 0)
		{
			coverBodyDirection = eCoverBodyDirection_Left;
		}
		else if (0 < postureLean)
		{
			coverBodyDirection = eCoverBodyDirection_Right;
		}
	}

	void ClearCoverAction()
	{
		coverActionName = NULL;
		coverAction = eCoverAction_Clear;
	}

	void SetCoverLocation(const Vec3& position, const Vec3& forwardDirection)
	{
		CRY_ASSERT(position.IsValid());
		CRY_ASSERT(forwardDirection.IsValid());
		CRY_ASSERT(forwardDirection.IsUnit());

		coverLocation = QuatT(position, Quat::CreateRotationVDir(forwardDirection));
		coverLocationRequest = eCoverLocationRequest_Set;
	}

	void ClearCoverLocation()
	{
		coverLocationRequest = eCoverLocationRequest_Clear;
	}

	ECoverBodyDirection   coverBodyDirection;

	ECoverAction          coverAction;
	const char*           coverActionName;
	QuatT                 coverLocation;
	ECoverLocationRequest coverLocationRequest;
};

struct SOBJECTSTATE
{
	enum EFireControllerIndex
	{
		eFireControllerIndex_None   = 0,
		eFireControllerIndex_First  = 1,
		eFireControllerIndex_Second = 2,
		eFireControllerIndex_Third  = 3,
		eFireControllerIndex_Fourth = 4,
		eFireControllerIndex_All    = 17185, //!< This value right shifted (4*n) times put in & with 0xf should give you the index of the n+1 fire controller index.
	};

	//! Actor's movement\looking\firing related.
	bool                  jump;
	bool                  aimObstructed;
	bool                  aimTargetIsValid;
	bool                  forceWeaponAlertness;

	EAIFireState          fire;
	EAIFireState          fireSecondary;
	EAIFireState          fireMelee;

	ERequestedGrenadeType requestedGrenadeType;
	int                   weaponAccessories; //!< Weapon accessories to enable.
	int                   bodystate;
	float                 lean;
	float                 peekOver;

	//! The requested position to shoot at. This value must be passed directly to weapon system.
	//! The AI system has decided already if the shot should miss or hit. Notes: can be (0,0,0) if not begin requested!
	Vec3 vShootTargetPos;

	//! The requested position to aim at.
	//! This value is slightly different from the vShootTarget, and is used for
	//! visual representation to where to shoot at. Notes: can be (0,0,0) if not begin requested!
	Vec3 vAimTargetPos;

	//! The requested position to look at. Notes: can be (0,0,0) if not begin requested!
	Vec3                       vLookTargetPos;

	Vec3                       vLobLaunchPosition;
	Vec3                       vLobLaunchVelocity;

	SFireCommandProjectileInfo projectileInfo;

	ELookStyle                 eLookStyle;

	//! Temporary flag to avoid the body turn while doing specific operation.
	//! It should be removed when there could be a proper procedural clip in Mannequin to lock the turn, aiming, etc.
	bool  bAllowLowerBodyToTurn;

	Vec3  vMoveDir;
	Vec3  vForcedNavigation;
	Vec3  vBodyTargetDir;
	Vec3  vDesiredBodyDirectionAtTarget;
	float fForcedNavigationSpeed;

	//! Flags that give context to movement, which can be used by animation selection system.
	//! Hint to animation system what will happen in the future.
	unsigned int                movementContext;
	SAIPredictedCharacterStates predictedCharacterStates;

	// Precision movement support: A replacement for the old velocity based movement system above.
	// NOTE: If the move target and inflection point are equal (and non-zero) they mark the end of the path.
	Vec3 vMoveTarget;               //!< If non-zero, contains the absolute position the agent should (and can) move towards.
	Vec3 vInflectionPoint;          //!< If non-zero, contains the estimated future move target once the agent reaches the current move target.

	//! AISPEED_ZERO, AISPEED_SLOW, AISPEED_WALK, AISPEED_RUN, AISPEED_SPRINT. This affects animation and thus speed only indirectly,
	//! due to it affecting the available max/min speeds.
	float                       fMovementUrgency;
	float                       fDesiredSpeed; //!< < in m/s.
	float                       fTargetSpeed;  //!< < in m/s.
	bool                        allowStrafing; //!< Whether strafing is allowed or not.
	bool                        allowEntityClampingByAnimation;

	float                       fDistanceToPathEnd;
	Vec3                        vDirOffPath; //!< Displacement vector between the physical position and the current path.

	EActorTargetPhase           curActorTargetPhase;
	Vec3                        curActorTargetFinishPos;
	SAIActorTargetRequest       actorTargetReq;

	bool                        bCloseContact;
	bool                        bReevaluate;
	bool                        bTargetEnabled;
	bool                        bTargetIsGroupTarget;
	bool                        continuousMotion;
	EAITargetType               eTargetType;
	EAITargetContextType        eTargetContextType;
	EAITargetThreat             eTargetThreat;
	tAIObjectID                 eTargetID;
	EAITargetType               ePeakTargetType;
	EAITargetThreat             ePeakTargetThreat;
	tAIObjectID                 ePeakTargetID;
	EAITargetType               ePreviousPeakTargetType;
	EAITargetThreat             ePreviousPeakTargetThreat;
	tAIObjectID                 ePreviousPeakTargetID;
	Vec3                        vTargetPos;
	float                       fDistanceFromTarget;
	EAITargetStuntReaction      eTargetStuntReaction;
	int                         nTargetType;
	DynArray<AISIGNAL>          vSignals;
	int                         secondaryIndex;
	CAIMannequinCommandList<32> mannequinRequest;
	SAICoverRequest             coverRequest;
	EBodyOrientationMode        bodyOrientationMode;

	SOBJECTSTATE()
		: forceWeaponAlertness(false)
		, eLookStyle(LOOKSTYLE_DEFAULT)
		, bAllowLowerBodyToTurn(true)
		, continuousMotion(false)
		, vLobLaunchPosition(ZERO)
		, vLobLaunchVelocity(ZERO)
		, bodyOrientationMode(HalfwayTowardsAimOrLook)
	{
		FullReset(true);
	}

	void Reset(bool clearMoveDir = true)
	{
		bCloseContact = false;
		bTargetIsGroupTarget = false;
		eTargetThreat = AITHREAT_NONE;
		eTargetType = AITARGET_NONE;
		eTargetID = 0;
		vTargetPos.zero();
		eTargetContextType = AITARGET_CONTEXT_UNKNOWN;
		eTargetStuntReaction = AITSR_NONE;

		if (clearMoveDir)
		{
			if (!continuousMotion)
				vMoveDir.zero();
		}

		vForcedNavigation.zero();
		fForcedNavigationSpeed = 0.0f;

		projectileInfo.Reset();

		predictedCharacterStates.nStates = 0;
		bTargetEnabled = false;
	}

	void FullReset(bool clearMoveDir = false)
	{
		Reset(clearMoveDir);

		//Only reset peaks on a full reset
		ePeakTargetThreat = AITHREAT_NONE;
		ePeakTargetType = AITARGET_NONE;
		ePeakTargetID = 0;
		ePreviousPeakTargetType = AITARGET_NONE;
		ePreviousPeakTargetThreat = AITHREAT_NONE;
		ePreviousPeakTargetID = 0;

		vLookTargetPos.zero();
		vBodyTargetDir.zero();
		vShootTargetPos.zero();
		vAimTargetPos.zero();

		vLobLaunchPosition.zero();
		vLobLaunchVelocity.zero();

		fDistanceToPathEnd = -1.0f;
		movementContext = 0;

		bAllowLowerBodyToTurn = true;

		vMoveDir.zero();
		vMoveTarget.zero();
		vInflectionPoint.zero();
		vDesiredBodyDirectionAtTarget.zero();
		vDirOffPath.zero();

		jump = false;
		fire = eAIFS_Off;
		fireSecondary = eAIFS_Off;
		fireMelee = eAIFS_Off;
		requestedGrenadeType = eRGT_ANY;
		aimObstructed = false;
		aimTargetIsValid = false;
		weaponAccessories = AIWEPA_NONE;
		allowStrafing = false;
		bodystate = 0;
		fDistanceFromTarget = FLT_MAX;
		actorTargetReq.Reset();
		curActorTargetPhase = eATP_None;
		curActorTargetFinishPos.zero();
		allowEntityClampingByAnimation = false;

		nTargetType = 0;
		fMovementUrgency = AISPEED_WALK;
		fDesiredSpeed = 1.0f;
		fTargetSpeed = 1.0f;

		lean = 0.0f;
		peekOver = 0.0f;

		ClearSignals();

		secondaryIndex = 0;
		continuousMotion = false;
		coverRequest = SAICoverRequest();
		mannequinRequest.ClearCommands();

		bodyOrientationMode = HalfwayTowardsAimOrLook;
	}

	void ClearSignals()
	{
		for (size_t idx = 0, count = vSignals.size(); idx != count; ++idx)
		{
			if (vSignals[idx].pEData)
				gEnv->pAISystem->FreeSignalExtraData(vSignals[idx].pEData);
		}

		vSignals.clear();
	}

	bool operator==(SOBJECTSTATE& other)
	{
		if (fire == other.fire)
			if (bodystate == other.bodystate)
				return true;

		return false;
	}

	void Serialize(TSerialize ser);
};

struct SAIBodyInfo
{
	Vec3    vEyePos;
	Vec3    vEyeDir;     //!< Direction of head	(where I'm looking at).
	Vec3    vEyeDirAnim; //!< Direction of head	from animation .
	Vec3    vEntityDir;
	Vec3    vAnimBodyDir;
	Vec3    vMoveDir;    //!< Direction of entity movement.
	Vec3    vUpDir;      //!< Direction of entity (my up).
	Vec3    vFireDir;    //!< Firing direction.
	Vec3    vFirePos;    //!< Firing position.
	float   maxSpeed;    //!< Maximum speed i can move at.
	float   normalSpeed; //!< The "normal" (walking) speed.
	float   minSpeed;    //!< Minimum speed i need to move anywhere (otherwise I'll consider myself stopped).
	EStance stance;
	AABB    stanceSize;   //!< Approximate local bounds of the stance, relative to the entity position.
	AABB    colliderSize; //!< Approximate local bounds of the stance, relative to the entity position.
	bool    isAiming;
	bool    isFiring;
	bool    isMoving;
	float   lean;     //!< The amount the character is leaning.
	float   peekOver; //!< The amount the character is peaking-over.
	float   slopeAngle;

	//! If valid animated eye direction present, use that for better precision.
	const Vec3& GetEyeDir() const
	{
		return vEyeDirAnim.IsZero() ? vEyeDir : vEyeDirAnim;
	}

	//! If valid animated body direction present, use that for better precision.
	const Vec3& GetBodyDir() const
	{
		return vAnimBodyDir.IsZero() ? vEntityDir : vAnimBodyDir;
	}

	EntityId linkedVehicleEntityId;

	SAIBodyInfo() : vEyePos(ZERO), vEyeDir(ZERO), vEyeDirAnim(ZERO), vEntityDir(ZERO), vAnimBodyDir(ZERO), vMoveDir(ZERO), vUpDir(ZERO), vFireDir(ZERO), vFirePos(ZERO),
		maxSpeed(0), normalSpeed(0), minSpeed(0), stance(STANCE_NULL),
		isAiming(false), isFiring(false), isMoving(false),
		linkedVehicleEntityId(0), lean(0.0f), slopeAngle(0.0f),
		stanceSize(Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero),
		colliderSize(Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero)
	{
	}

	IEntity* GetLinkedVehicleEntity() const
	{
		assert(gEnv && gEnv->pEntitySystem);
		return linkedVehicleEntityId ? gEnv->pEntitySystem->GetEntity(linkedVehicleEntityId) : NULL;
	}
};

struct SAIWeaponInfo
{
	SAIWeaponInfo()
		: lastShotTime((int64)0)
		, ammoCount(0)
		, clipSize(0)
		, outOfAmmo(false)
		, shotIsDone(false)
		, canMelee(false)
		, hasLightAccessory(false)
		, hasLaserAccessory(false)
		, isReloading(false)
		, isFiring(false)
	{
	}

	CTimeValue lastShotTime;
	int        ammoCount;
	int        clipSize;

	bool       canMelee          : 1;
	bool       outOfAmmo         : 1;
	bool       lowAmmo           : 1;
	bool       shotIsDone        : 1;
	bool       hasLightAccessory : 1;
	bool       hasLaserAccessory : 1;
	bool       isReloading       : 1;
	bool       isFiring          : 1;
};

enum EAIAGInput
{
	AIAG_SIGNAL,
	AIAG_ACTION,
};

struct SAIBodyInfoQuery
{
	SAIBodyInfoQuery(const Vec3& pos, const Vec3& trg, EStance _stance, float _lean = 0.0f, float _peekOver = 0.0f, bool _defaultPose = false)
		: stance(_stance)
		, lean(_lean)
		, peekOver(_peekOver)
		, defaultPose(_defaultPose)
		, position(pos)
		, target(trg)
	{
	}

	SAIBodyInfoQuery(EStance _stance, float _lean = 0.0f, float _peekOver = 0.0f, bool _defaultPose = false)
		: stance(_stance)
		, lean(_lean)
		, peekOver(_peekOver)
		, defaultPose(_defaultPose)
		, position(ZERO)
		, target(ZERO)
	{
	}

	SAIBodyInfoQuery()
		: stance(STANCE_NULL)
		, lean(0.0f)
		, peekOver(0.0f)
		, defaultPose(true)
		, position(ZERO)
		, target(ZERO)
	{
	}

	EStance stance;
	float   lean;
	float   peekOver;
	bool    defaultPose;
	Vec3    position;
	Vec3    target;
};

struct SCommunicationSound
{
	SCommunicationSound()
		: playSoundControlId(CryAudio::InvalidControlId)
		, stopSoundControlId(CryAudio::InvalidControlId)
	{
	}

	SCommunicationSound(const CryAudio::ControlId _playSoundControlId, const CryAudio::ControlId _stopSoundControlId)
		: playSoundControlId(_playSoundControlId)
		, stopSoundControlId(_stopSoundControlId)
	{
	}

	CryAudio::ControlId playSoundControlId;
	CryAudio::ControlId stopSoundControlId;
};

struct IAICommunicationHandler
{
	enum ECommunicationHandlerEvent
	{
		SoundStarted = 0,
		SoundFinished,
		SoundCancelled,
		SoundFailed,
		VoiceStarted,
		VoiceFinished,
		VoiceCancelled,
		VoiceFailed,
		ActionStarted,
		ActionCancelled,
		ActionFailed,
		SignalStarted,
		SignalFinished,
		SignalCancelled,
		SignalFailed,
	};

	enum EAnimationMethod
	{
		AnimationMethodSignal = 0,
		AnimationMethodAction,
	};

	struct IEventListener
	{
		// <interfuscator:shuffle>
		virtual ~IEventListener(){}
		virtual void OnCommunicationHandlerEvent(ECommunicationHandlerEvent type, CommPlayID playID, EntityId actorID) = 0;
		// </interfuscator:shuffle>
	};

	// <interfuscator:shuffle>
	virtual ~IAICommunicationHandler(){}
	virtual SCommunicationSound PlaySound(CommPlayID playID, const char* name, IEventListener* listener = 0) = 0;
	virtual void                StopSound(const SCommunicationSound& soundToStop) = 0;

	virtual SCommunicationSound PlayVoice(CommPlayID playID, const char* name, IEventListener* listener = 0) = 0;
	virtual void                StopVoice(const SCommunicationSound& soundToStop) = 0;

	virtual void                SendDialogueRequest(CommPlayID playID, const char* name, IEventListener* listener = 0) = 0;

	virtual void                PlayAnimation(CommPlayID playID, const char* name, EAnimationMethod method, IEventListener* listener = 0) = 0;
	virtual void                StopAnimation(CommPlayID playID, const char* name, EAnimationMethod method) = 0;

	virtual bool                IsInAGState(const char* name) = 0;
	virtual void                ResetAnimationState() = 0;

	virtual bool                IsPlayingAnimation() const = 0;
	virtual bool                IsPlayingSound() const = 0;

	virtual void                OnSoundTriggerFinishedToPlay(CryAudio::ControlId const triggerId) = 0;
	// </interfuscator:shuffle>
};

enum EWaypointNodeType {WNT_UNSET, WNT_WAYPOINT, WNT_HIDE, WNT_ENTRYEXIT, WNT_EXITONLY, WNT_HIDESECONDARY};
enum EWaypointLinkType {WLT_AUTO_PASS = 320, WLT_AUTO_IMPASS = -320, WLT_EDITOR_PASS = 321, WLT_EDITOR_IMPASS = -321, WLT_UNKNOWN_TYPE = 0};

//! \endcond