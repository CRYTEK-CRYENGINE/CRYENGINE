// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GOALOP_H_
#define __GOALOP_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include "AIPIDController.h"
#include <CryMath/Random.h>
#include <CryAISystem/ITacticalPointSystem.h>
#include <CrySystem/TimeValue.h>
#include <CrySystem/XML/XMLAttrReader.h>

class CAIObject;
class CPipeUser;
class CPuppet;
struct GraphNode;

class COPPathFind;
class COPTrace;

enum
{
	AI_LOOKAT_CONTINUOUS  = 1 << 0,
	AI_LOOKAT_USE_BODYDIR = 1 << 1,
};

enum EAIRegister
{
	AI_REG_NONE,
	AI_REG_LASTOP,
	AI_REG_REFPOINT,
	AI_REG_ATTENTIONTARGET,
	AI_REG_COVER,
	AI_REG_PATH,
	AI_REG_LAST
};

enum ELookMotivation
{
	AILOOKMOTIVATION_LOOK = 0,
	AILOOKMOTIVATION_GLANCE,
	AILOOKMOTIVATION_STARTLE,
	AILOOKMOTIVATION_DOUBLETAKE,
};

#if CRY_PLATFORM_APPLE
// WAIT_ANY is defined as (-1) in sys/wait.h
	#undef WAIT_ANY
#endif // CRY_PLATFORM_APPLE

enum EOPWaitType
{
	WAIT_ALL   = 0, // Wait for all group to be finished.
	WAIT_ANY   = 1, // Wait for any goal in the group to be finished.
	WAIT_ANY_2 = 2, // Wait for any goal in the group to be finished.
	WAIT_LAST
};

enum EOPBranchType
{
	IF_ACTIVE_GOALS = 0,                // Default as it was in CryEngine1 - jumps if there are active (not finished) goal operations.
	IF_ACTIVE_GOALS_HIDE,               // Default as it was in CryEngine1 - jumps if there are active goals or hide spot wasn't found.
	IF_NO_PATH,                         // Jumps if there wasn't a path in the last "pathfind" goal operation.
	IF_PATH_STILL_FINDING,              // Jumps if the current pathfind request hasn't been completed yet.
	IF_IS_HIDDEN,                       // Jumps if the last "hide" goal operation has succeed and distance to hide point is small.
	IF_CAN_HIDE,                        // Jumps if the last "hide" goal operation has succeed.
	IF_CANNOT_HIDE,                     // Jumps if the last "hide" goal operation has failed.
	IF_STANCE_IS,                       // Jumps if the stance of this PipeUser is equal to value specified as 5-th argument.
	IF_FIRE_IS,                         // Jumps if the argument of the last "firecmd" on this PipeUser is equal to 5-th argument.
	IF_HAS_FIRED,                       // Jumps if the PipeUser just fired - fire flag passed to actor.
	IF_NO_LASTOP,                       // Jumps if m_pLastOpResult is NULL (for example if "locate" goalOp returned NULL).
	IF_SEES_LASTOP,                     // Jumps if m_pLastOpResult is visible from here.
	IF_SEES_TARGET,                     // Jumps if attention target is visible from here.
	IF_CAN_SHOOT_TARGET,                // Jumps if nothing between weapon and attention target (can shoot).
	IF_CAN_MELEE,                       // If current weapon has melee fire mode.
	IF_NO_ENEMY_TARGET,                 // Jumps if it hasn't got an enemy attention target.
	IF_PATH_LONGER,                     // Jumps if current path is longer than 5-th argument.
	IF_PATH_SHORTER,                    // Jumps if current path is shorter than 5-th argument.
	IF_PATH_LONGER_RELATIVE,            // Jumps if current path is longer than (5-th argument) times the distance to requested destination.
	IF_NAV_WAYPOINT_HUMAN,              // Jumps if the current navigation graph is waypoint (use for checking indoor).
	IF_NAV_TRIANGULAR,                  // Jumps if the current navigation graph is triangular (use for checking outdoor).
	IF_TARGET_DIST_LESS,                // Jumps if the distance to target is less.
	IF_TARGET_DIST_LESS_ALONG_PATH,     // Jumps if the distance along path to target is less.
	IF_TARGET_DIST_GREATER,             // Jumps if the distance to target is more.
	IF_TARGET_IN_RANGE,                 // Jumps if the distance to target is less than the attackRannge.
	IF_TARGET_OUT_OF_RANGE,             // Jumps if the distance to target is more than the attackRannge.
	IF_TARGET_TO_REFPOINT_DIST_LESS,    // Jumps if the distance between target and refpoint is less.
	IF_TARGET_TO_REFPOINT_DIST_GREATER, // Jumps if the distance between target and refpoint is more.
	IF_TARGET_LOST_TIME_MORE,           // Jumps if target lost time is more.
	IF_TARGET_LOST_TIME_LESS,           // Jumps if target lost time is less.
	IF_LASTOP_DIST_LESS,                // Jumps if the distance to last op result is less.
	IF_LASTOP_DIST_LESS_ALONG_PATH,     // Jumps if the distance to last op result along path is less.
	IF_TARGET_MOVED_SINCE_START,        // Jumps if the distance between current targetPos and targetPos when pipe started more than threshold.
	IF_TARGET_MOVED,                    // Jumps if the distance between current targetPos and targetPos when pipe started (or last time it was checked) more than threshold.
	IF_EXPOSED_TO_TARGET,               // Jumps if the upper torso of the agent is visible towards the target.
	IF_COVER_COMPROMISED,               // Jumps if the current cover cannot be used for hiding or if the hide spots does not exists.
	IF_COVER_NOT_COMPROMISED,           // Negated version of IF_COVER_COMPROMISED.
	IF_COVER_SOFT,                      // If current cover is soft cover.
	IF_COVER_NOT_SOFT,                  // If current cover is not soft cover.
	IF_CAN_SHOOT_TARGET_PRONED,
	IF_CAN_SHOOT_TARGET_CROUCHED,
	IF_CAN_SHOOT_TARGET_STANDING,
	IF_COVER_FIRE_ENABLED,        // If cover fire is not enabled .
	IF_RANDOM,
	IF_LASTOP_FAILED,
	IF_LASTOP_SUCCEED,
	BRANCH_ALWAYS,            // Unconditional jump.
	NOT = 0x100
};

enum ECoverUsageLocation
{
	eCUL_None,
	eCUL_Automatic,
	eCUL_Left,
	eCUL_Right,
	eCUL_Center,
};

enum ETraceEndMode
{
	eTEM_FixedDistance,
	eTEM_MinimumDistance
};

ILINE const char* GetNameSafe(CAIObject* pObject)
{
	return pObject ? pObject->GetName() : "<Null Object>";
}

inline int MovementUrgencyToIndex(float urgency)
{
	int sign = urgency < 0.0f ? -1 : 1;
	if (urgency < 0) urgency = -urgency;

	if (urgency < (AISPEED_ZERO + AISPEED_SLOW) / 2)
		return 0;
	else if (urgency < (AISPEED_SLOW + AISPEED_WALK) / 2)
		return 1 * sign;  // slow
	else if (urgency < (AISPEED_WALK + AISPEED_RUN) / 2)
		return 2 * sign;  // walk
	else if (urgency < (AISPEED_RUN + AISPEED_SPRINT) / 2)
		return 3 * sign;  // run
	else
		return 4 * sign;  // sprint
}

class CGoalOpXMLReader
{
	CXMLAttrReader<unsigned short int>  m_dictAIObjectType;
	CXMLAttrReader<EAnimationMode>      m_dictAnimationMode;
	CXMLAttrReader<bool>                m_dictBools;
	CXMLAttrReader<ECoverUsageLocation> m_dictCoverLocation;
	CXMLAttrReader<EFireMode>           m_dictFireMode;
	CXMLAttrReader<ELookMotivation>     m_dictLook;
	CXMLAttrReader<EAIRegister>         m_dictRegister;
	CXMLAttrReader<ESignalFilter>       m_dictSignalFilter;
	CXMLAttrReader<EStance>             m_dictStance;
	CXMLAttrReader<float>               m_dictUrgency;

public:
	CGoalOpXMLReader();

	template<typename T>
	T Get(const XmlNodeRef& node, const char* szAttrName, T defaultValue)
	{
		T value;
		if (node->getAttr(szAttrName, value))
		{
			return value;
		}

		return defaultValue;
	}

	template<typename T>
	bool GetMandatory(const XmlNodeRef& node, const char* szAttrName, T& value)
	{
		if (node->getAttr(szAttrName, value))
		{
			return true;
		}

		AIError("Unable to get mandatory attribute '%s' of node '%s'.",
		        szAttrName, node->getTag());

		return false;
	}

	bool GetAIObjectType(const XmlNodeRef& node, const char* szAttrName, unsigned short int& nValue, bool bMandatory = false)
	{ return m_dictAIObjectType.Get(node, szAttrName, nValue, bMandatory); }
	bool GetAnimationMode(const XmlNodeRef& node, const char* szAttrName, EAnimationMode& eValue, bool bMandatory = false)
	{ return m_dictAnimationMode.Get(node, szAttrName, eValue, bMandatory); }
	bool GetCoverLocation(const XmlNodeRef& node, const char* szAttrName, ECoverUsageLocation& eValue, bool bMandatory = false)
	{ return m_dictCoverLocation.Get(node, szAttrName, eValue, bMandatory); }
	bool GetFireMode(const XmlNodeRef& node, const char* szAttrName, EFireMode& eValue, bool bMandatory = false)
	{ return m_dictFireMode.Get(node, szAttrName, eValue, bMandatory); }
	bool GetLook(const XmlNodeRef& node, const char* szAttrName, ELookMotivation& eValue, bool bMandatory = false)
	{ return m_dictLook.Get(node, szAttrName, eValue, bMandatory); }
	bool GetRegister(const XmlNodeRef& node, const char* szAttrName, EAIRegister& eValue, bool bMandatory = false)
	{ return m_dictRegister.Get(node, szAttrName, eValue, bMandatory); }
	bool GetSignalFilter(const XmlNodeRef& node, const char* szAttrName, ESignalFilter& eValue, bool bMandatory = false)
	{ return m_dictSignalFilter.Get(node, szAttrName, eValue, bMandatory); }
	bool GetStance(const XmlNodeRef& node, const char* szAttrName, EStance& eValue, bool bMandatory = false)
	{ return m_dictStance.Get(node, szAttrName, eValue, bMandatory); }
	bool GetUrgency(const XmlNodeRef& node, const char* szAttrName, float& fValue, bool bMandatory = false)
	{ return m_dictUrgency.Get(node, szAttrName, fValue, bMandatory); }

	bool        GetBool(const XmlNodeRef& node, const char* szAttrName, bool bDefaultValue = false);
	bool        GetMandatoryBool(const XmlNodeRef& node, const char* szAttrName);

	const char* GetMandatoryString(const XmlNodeRef& node, const char* szAttrName);

	enum { MANDATORY = 1 };
};

// Return value type of the IGoalOp::Execute
enum EGoalOpResult
{
	eGOR_NONE,         // Default value; should not be returned
	eGOR_IN_PROGRESS,  // Keep on executing
	eGOR_SUCCEEDED,    // The goalop succeeded
	eGOR_FAILED,       // The goalop failed
	eGOR_DONE,         // The goalop is finished (neutral, do not change result state)
	eGOR_LAST
};

struct IGoalOp
{
	virtual ~IGoalOp() {}

	virtual void          DebugDraw(CPipeUser* pPipeUser) const = 0;
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser) = 0;
	/// Stripped-down execute - should only be implemented if this goalop needs
	/// to be really responsive (gets called every frame - i.e. on the dry update)
	virtual void ExecuteDry(CPipeUser* pPipeUser) = 0;
	virtual void Reset(CPipeUser* pPipeUser) = 0;
	virtual void Serialize(TSerialize ser) = 0;

	virtual void ParseParams(const GoalParams& node) = 0;
	virtual void ParseParam(const char* param, const GoalParams& node) = 0;

};

// (MATT) CGoalOp must be a smart pointer for it's use in containers of QGoals {2009/10/13}
class CGoalOp : public IGoalOp, public _i_reference_target_t
{
public:
	// due to poor realization of QGoal::Clone() we need to define ctors which
	// does not bit-copy of _i_reference_target_t
	CGoalOp(){}
	CGoalOp(const CGoalOp& op) {}

	virtual void          DebugDraw(CPipeUser* pPipeUser) const                 {}
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser)                         { return eGOR_DONE; }
	virtual void          ExecuteDry(CPipeUser* pPipeUser)                      {}
	virtual void          Reset(CPipeUser* pPipeUser)                           {}
	virtual void          Serialize(TSerialize ser)                             {}

	virtual void          ParseParams(const GoalParams& node)                   {} //const XmlNodeRef &node) {};
	virtual void          ParseParam(const char* param, const GoalParams& node) {} //const XmlNodeRef &value) {};

protected:
	static CGoalOpXMLReader s_xml;
};

class CGoalOpParallel : public CGoalOp
{

	CGoalPipe* m_NextConcurrentPipe;
	CGoalPipe* m_ConcurrentPipe;

	struct OpInfo
	{
		bool done;
		bool blocking;

		OpInfo() : done(false), blocking(false)
		{}
	};

	std::vector<OpInfo> m_OpInfos;

public:

	CGoalOpParallel() : m_NextConcurrentPipe(0), m_ConcurrentPipe(0)
	{}

	virtual ~CGoalOpParallel();

	virtual void          ParseParams(const GoalParams& node);

	void                  SetConcurrentPipe(CGoalPipe* goalPipe);

	void                  ReleaseConcurrentPipe(CPipeUser* pPipeUser, bool clearNextPipe = false);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);

	void                  Reset(CPipeUser* pPipeUser)
	{
		ReleaseConcurrentPipe(pPipeUser, true);

		std::vector<OpInfo> tmp;
		m_OpInfos.swap(tmp);
	};
};

////////////////////////////////////////////////////////////
//
//				ACQUIRE TARGET - acquires desired target, even if it is outside of view
//
////////////////////////////////////////////////////////
class COPAcqTarget : public CGoalOp
{
	string m_sTargetName;

public:
	// Empty or invalid sTargetName means "use the LastOp"
	COPAcqTarget(const string& sTargetName) : m_sTargetName(sTargetName) {}
	COPAcqTarget(const XmlNodeRef& node) : m_sTargetName(node->getAttr("name")) {}

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				APPROACH - makes agent approach to "distance" from current att target
//
////////////////////////////////////////////////////////
class COPApproach : public CGoalOp
{
public:
	COPApproach(float fEndDistance, float fEndAccuracy = 1.f, float fDuration = 0.0f,
	            bool bUseLastOpResult = false, bool bLookAtLastOp = false,
	            bool bForceReturnPartialPath = false, bool bStopOnAnimationStart = false, const char* szNoPathSignalText = 0);
	COPApproach(const XmlNodeRef& node);
	virtual ~COPApproach() { Reset(0); }

	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

private:
	float  m_fLastDistance;
	float  m_fInitialDistance;
	bool   m_bUseLastOpResult;
	bool   m_bLookAtLastOp;
	bool   m_stopOnAnimationStart;
	float  m_fEndDistance;/// Stop short of the path end
	float  m_fEndAccuracy;/// allow some error in the stopping position
	float  m_fDuration; /// Stop after this time (0 means disabled)
	bool   m_bForceReturnPartialPath;
	string m_noPathSignalText;
	int    m_looseAttentionId;

	float GetEndDistance(CPipeUser* pPipeUser) const;

	// set to true when the path is first found
	bool         m_bPathFound;

	COPTrace*    m_pTraceDirective;
	COPPathFind* m_pPathfindDirective;
};

////////////////////////////////////////////////////////////
//
// FOLLOWPATH - makes agent follow the predefined path stored in the operand
//
////////////////////////////////////////////////////////

class COPFollowPath : public CGoalOp
{
public:
	/// pathFindToStart: whether the operand should path-find to the start of the path, or just go there in a straight line
	/// reverse: whether the operand should navigate the path from the first point to the last, or in reverse
	/// startNearest: whether the operand should start from the first point on the path, or from the nearest point on the path to his current location
	/// loops: how many times to traverse the path (goes round in a loop)
	COPFollowPath(bool pathFindToStart, bool reverse, bool startNearest, int loops, float fEndAccuracy, bool bUsePointList, bool bControlSpeed, float desiredSpeedOnPath);
	COPFollowPath(const XmlNodeRef& node);
	COPFollowPath(const COPFollowPath& rhs);
	virtual ~COPFollowPath() { Reset(0); }

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

private:

	COPTrace*             m_pTraceDirective;
	COPPathFind*          m_pPathFindDirective;
	CStrongRef<CAIObject> m_refPathStartPoint;
	bool                  m_pathFindToStart;
	bool                  m_reversePath;
	bool                  m_startNearest;
	bool                  m_bUsePointList;
	bool                  m_bControlSpeed;
	int                   m_loops;
	int                   m_loopCounter;
	int                   m_notMovingTimeMs;
	CTimeValue            m_lastTime;
	bool                  m_returningToPath;
	float                 m_fEndAccuracy;
	float                 m_fDesiredSpeedOnPath;

};

////////////////////////////////////////////////////////////
//
//				BACKOFF - makes agent back off to "distance" from current att target
//
////////////////////////////////////////////////////////
class COPBackoff : public CGoalOp
{
public:
	/// If distance < 0 then the backoff is done from the agent's current position,
	/// else it is done from the target position.
	/// If duration > 0 then backoff stops after that duration
	COPBackoff(float distance, float duration = 0.f, int filter = 0, float minDistance = 0.f);
	COPBackoff(const XmlNodeRef& node);
	COPBackoff(const COPBackoff& rhs);
	virtual ~COPBackoff() { Reset(0); }

	virtual void          Reset(CPipeUser* pPipeUser);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
	virtual void          Serialize(TSerialize ser);

private:
	void ResetNavigation(CPipeUser* pPipeUser);
	void ResetMoveDirections();

private:
	bool                  m_bUseTargetPosition; // distance from target or from own current position
	int                   m_iDirection;
	int                   m_iCurrentDirectionIndex;
	std::vector<int>      m_MoveDirections;

	float                 m_fDistance;
	float                 m_fDuration;
	CTimeValue            m_fInitTime;
	CTimeValue            m_fLastUpdateTime;
	bool                  m_bUseLastOp;
	bool                  m_bLookForward;
	Vec3                  m_moveStart;
	Vec3                  m_moveEnd;
	int                   m_currentDirMask;
	float                 m_fMinDistance;
	Vec3                  m_vBestFailedBackoffPos;
	float                 m_fMaxDistanceFound;
	bool                  m_bTryingLessThanMinDistance;
	int                   m_looseAttentionId;
	bool                  m_bRandomOrder;
	bool                  m_bCheckSlopeDistance;

	COPTrace*             m_pTraceDirective;
	COPPathFind*          m_pPathfindDirective;
	CStrongRef<CAIObject> m_refBackoffPoint;
};

////////////////////////////////////////////////////////////
//
//				TIMEOUT - counts down a timer...
//
////////////////////////////////////////////////////////
class COPTimeout : public CGoalOp
{
	CTimeValue m_startTime;
	float      m_fIntervalMin, m_fIntervalMax;
	int        m_actualIntervalMs;

public:
	COPTimeout(float fIntervalMin, float fIntervalMax = 0.f);
	COPTimeout(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser*);
	virtual void          Reset(CPipeUser*);
	virtual void          Serialize(TSerialize ser);
};

////////////////////////////////////////////////////////////
//
//				STRAFE makes agent strafe left or right (1,-1)... 0 stops strafing
//
////////////////////////////////////////////////////////
class COPStrafe : public CGoalOp
{
	float m_fDistanceStart;
	float m_fDistanceEnd;
	bool  m_bStrafeWhileMoving;

public:
	COPStrafe(float fDistanceStart, float fDistanceEnd, bool bStrafeWhileMoving);
	COPStrafe(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				FIRECMD - 1 allows agent to fire, 0 forbids agent to fire
//
////////////////////////////////////////////////////////
class COPFireCmd : public CGoalOp
{
	EFireMode  m_eFireMode;
	bool       m_bUseLastOp;

	float      m_fIntervalMin;
	float      m_fIntervalMax;
	int        m_actualIntervalMs;
	CTimeValue m_startTime;

public:
	COPFireCmd(EFireMode eFireMode, bool bUseLastOp, float fIntervalMin, float fIntervalMax);
	COPFireCmd(const XmlNodeRef& node);

	virtual void          Reset(CPipeUser* pPipeUser);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				BODYCMD - controls agents stance (0- stand, 1-crouch, 2-prone)
//
////////////////////////////////////////////////////////
class COPBodyCmd : public CGoalOp
{
	EStance m_nBodyStance;
	bool    m_bDelayed; // this stance has to be set at the end of next/current trace

public:
	COPBodyCmd(EStance bodyStance, bool bDelayed = false);
	COPBodyCmd(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				RUNCMD - makes the agent run if 1, walk if 0
//
////////////////////////////////////////////////////////
class COPRunCmd : public CGoalOp
{
	float ConvertUrgency(float speed);

	float m_fMaxUrgency;
	float m_fMinUrgency;
	float m_fScaleDownPathLength;

public:
	COPRunCmd(float fMaxUrgency, float fMinUrgency, float fScaleDownPathLength);
	COPRunCmd(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				LOOKAROUND - looks in a random direction
//
////////////////////////////////////////////////////////
class COPLookAround : public CGoalOp
{
	float      m_fLastDot;
	float      m_fLookAroundRange;
	float      m_fIntervalMin, m_fIntervalMax;
	float      m_fScanIntervalRange;
	int        m_scanTimeOutMs;
	int        m_timeOutMs;
	CTimeValue m_startTime;
	CTimeValue m_scanStartTime;
	ELookStyle m_eLookStyle;
	bool       m_breakOnLiveTarget;
	bool       m_useLastOp;
	float      m_lookAngle;
	float      m_lookZOffset;
	float      m_lastLookAngle;
	float      m_lastLookZOffset;
	int        m_looseAttentionId;
	bool       m_bInitialized;
	Vec3       m_initialDir;
	bool       m_checkForObstacles;
	void       UpdateLookAtTarget(CPipeUser* pPipeUser);
	Vec3       GetLookAtDir(CPipeUser* pPipeUser, float angle, float dz) const;
	void       SetRandomLookDir();

	ILINE void RandomizeScanTimeout()
	{
		m_scanTimeOutMs = (int)((m_fScanIntervalRange + cry_random(0.2f, 1.0f)) * 1000.0f);
	}

	ILINE void RandomizeTimeout()
	{
		m_timeOutMs = (int)(cry_random(m_fIntervalMin, m_fIntervalMax) * 1000.0f);
	}

public:
	void Reset(CPipeUser* pPipeUser);
	COPLookAround(float lookAtRange, float scanIntervalRange, float intervalMin, float intervalMax, bool bBodyTurn, bool breakOnLiveTarget, bool useLastOp, bool checkForObstacles);
	COPLookAround(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
	virtual void          Serialize(TSerialize ser);
};

////////////////////////////////////////////////////////////
//
//				PATHFIND - generates a path to desired AI object
//
////////////////////////////////////////////////////////
class COPPathFind : public CGoalOp
{
	string              m_sTargetName;
	CWeakRef<CAIObject> m_refTarget;
	// if m_fDirectionOnlyDistance > 0, attempt to pathfind that distance in the direction
	// between the supposed path start/end points
	float m_fDirectionOnlyDistance;
	float m_fEndTolerance;
	float m_fEndDistance;
	Vec3  m_vTargetPos;
	Vec3  m_vTargetOffset;

	int   m_nForceTargetBuildingID; // if >= 0 forces the path destination to be within a particular building

public:
	COPPathFind(
	  const char* szTargetName,
	  CAIObject* pTarget = 0,
	  float fEndTolerance = 0.f,
	  float fEndDistance = 0.f,
	  float fDirectionOnlyDistance = 0.f
	  );
	COPPathFind(const XmlNodeRef& node);

	virtual void          Reset(CPipeUser* pPipeUser);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

	void                  SetForceTargetBuildingId(int nForceTargetBuildingID) { m_nForceTargetBuildingID = nForceTargetBuildingID; }
	void                  SetTargetOffset(const Vec3& vTargetOffset)           { m_vTargetOffset = vTargetOffset; }

	bool m_bWaitingForResult;
};

////////////////////////////////////////////////////////////
//
//				LOCATE - locates an aiobject in the map
//
////////////////////////////////////////////////////////
class COPLocate : public CGoalOp
{
	string             m_sName;
	unsigned short int m_nObjectType;
	float              m_fRange;
	bool               m_bWarnIfFailed;

public:
	COPLocate(const char* szName, unsigned short int nObjectType = 0, float fRange = 0.f);
	COPLocate(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				IGNOREALL - 1, puppet does not reevaluate threats, 0 evaluates again
//
////////////////////////////////////////////////////////
class COPIgnoreAll : public CGoalOp
{
	bool m_bIgnoreAll;

public:
	COPIgnoreAll(bool bIgnoreAll) : m_bIgnoreAll(bIgnoreAll) {}
	COPIgnoreAll(const XmlNodeRef& node) : m_bIgnoreAll(s_xml.GetBool(node, "id", true)) {}

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				SIGNAL - send a signal to himself or other agents
//
////////////////////////////////////////////////////////
class COPSignal : public CGoalOp
{
	int           m_nSignalID;
	string        m_sSignal;
	ESignalFilter m_cFilter;
	bool          m_bSent;
	int           m_iDataValue;

public:
	COPSignal(int nSignalID, const string& sSignal, ESignalFilter cFilter, int iDataValue) :
		m_nSignalID(nSignalID), m_sSignal(sSignal), m_cFilter(cFilter), m_bSent(false), m_iDataValue(iDataValue) {}
	COPSignal(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				SCRIPT - execute a piece of script
//
////////////////////////////////////////////////////////
class COPScript : public CGoalOp
{
	string m_scriptCode;

public:
	COPScript(const string& scriptCode);
	COPScript(const XmlNodeRef& node);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//				DEVALUE - devalues current attention target
//
////////////////////////////////////////////////////////
class COPDeValue : public CGoalOp
{
	bool m_bDevaluePuppets;
	bool m_bClearDevalued;

public:
	COPDeValue(int nPuppetsAlso = 0, bool bClearDevalued = false);
	COPDeValue(const XmlNodeRef& goalOpNode);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

#if 0
// deprecated and won't compile at all...

////////////////////////////////////////////////////////////
//
//			HIDE - makes agent find closest hiding place and then hide there
//
////////////////////////////////////////////////////////
class COPHide : public CGoalOp
{
	CStrongRef<CAIObject> m_refHideTarget;
	Vec3                  m_vHidePos;
	Vec3                  m_vLastPos;
	float                 m_fSearchDistance;
	float                 m_fMinDistance;
	int                   m_nEvaluationMethod;
	COPPathFind*          m_pPathFindDirective;
	COPTrace*             m_pTraceDirective;
	bool                  m_bLookAtHide;
	bool                  m_bLookAtLastOp;
	bool                  m_bAttTarget;
	bool                  m_bEndEffect;
	int                   m_iActionId;
	int                   m_looseAttentionId;

public:
	COPHide(float distance, float minDistance, int method, bool bExact, bool bLookatLastOp)
	{
		m_fSearchDistance = distance;
		m_nEvaluationMethod = method;
		m_fMinDistance = minDistance;
		m_pPathFindDirective = 0;
		m_pTraceDirective = 0;
		m_bLookAtHide = bExact;
		m_bLookAtLastOp = bLookatLastOp;
		m_iActionId = 0;
		m_looseAttentionId = 0;
		m_vHidePos.zero();
		m_vLastPos.zero();
		m_bAttTarget = false;
		m_bEndEffect = false;
	}

	EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void  ExecuteDry(CPipeUser* pPipeUser);
	virtual void  Reset(CPipeUser* pPipeUser);
	virtual void  Serialize(TSerialize ser);

private:
	void CreateHideTarget(string sName, const Vec3& vPos);
	bool IsBadHiding(CPipeUser* pPipeUser);
};
#endif // 0

////////////////////////////////////////////////////////////
//
//			TacticalPos - makes agent find a tactical position (and go there)
//
///////////////////////////////////////////////////////////

class COPTacticalPos : public CGoalOp
{
	// Evaluation, pathfinding and moving to the point is regulated by a state machine
	enum eTacticalPosState
	{
		eTPS_INVALID,
		eTPS_QUERY_INIT,
		eTPS_QUERY,
		eTPS_PATHFIND_INIT,
		eTPS_PATHFIND,
		eTPS_TRACE_INIT,
		eTPS_TRACE,
	};

	int                         m_iOptionUsed;               // Stored so that later signals (say, on reaching point) can give the option number
	CStrongRef<CAIObject>       m_refHideTarget;
	COPPathFind*                m_pPathFindDirective;
	COPTrace*                   m_pTraceDirective;
	bool                        m_bLookAtHide;
	EAIRegister                 m_nReg;
	eTacticalPosState           m_state;
	CTacticalPointQueryInstance m_queryInstance;             // Help that managed the async queries
	Vec3                        m_vLastHidePos;

public:
	COPTacticalPos(int tacQueryID, EAIRegister nReg);
	COPTacticalPos(const XmlNodeRef& node);
	COPTacticalPos(const COPTacticalPos& rhs);
	virtual ~COPTacticalPos() { Reset(NULL); }

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	void                  QueryDryUpdate(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	bool                  IsBadHiding(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

protected:
	enum { eTPGOpState_NoPointFound, eTPGOpState_PointFound, eTPGOpState_DestinationReached };
	void SendStateSignal(CPipeUser* pPipeUser, int nState);
};

////////////////////////////////////////////////////////////
//
//			Look - make an agent look somewhere (operates on looktarget)
//
///////////////////////////////////////////////////////////

class COPLook : public CGoalOp
{
	ELookStyle  m_eLookThere;
	ELookStyle  m_eLookBack;
	EAIRegister m_nReg;
	int         m_nLookID; // keep track of this look command
	bool        m_bInitialised;
	float       m_fLookTime, m_fTimeLeft;

public:
	COPLook(int lookMode, bool bBodyTurn, EAIRegister nReg);
	COPLook(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
};

#if 0
// deprecated and won't compile at all...

////////////////////////////////////////////////////////////
//
//			FORM - this agent creates desired formation
//
////////////////////////////////////////////////////////
// (MATT) Appears probably useless. Consider removing {2009/02/27}
class COPForm : public CGoalOp
{
	string m_sName;

public:
	COPForm(const char* name) : m_sName(name) {}
	COPForm(const XmlNodeRef& node) : m_sName(s_xml.GetMandatoryString(node, "name")) {}

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};
#endif // 0

////////////////////////////////////////////////////////////
//
//			CLEAR - clears the actions for the operand puppet
//
////////////////////////////////////////////////////////
class COPClear : public CGoalOp
{
	string m_sName;

public:
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////////
//
//			LOOKAT - look toward a specified direction
//
////////////////////////////////////////////////////////
class COPLookAt : public CGoalOp
{
	float      m_fStartAngle;
	float      m_fEndAngle;
	float      m_fLastDot;
	float      m_fAngleThresholdCos;
	CTimeValue m_startTime;
	ELookStyle m_eLookStyle;

	bool       m_bUseLastOp;
	bool       m_bContinuous;
	bool       m_bInitialized;
	bool       m_bUseBodyDir;
	bool       m_bYawOnly;

	void ResetLooseTarget(CPipeUser* pPipeUser, bool bForceReset = false);

public:
	COPLookAt(float fStartAngle, float fEndAngle, int mode = 0, bool bBodyTurn = true, bool bUseLastOp = false);
	COPLookAt(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
};

////////////////////////////////////////////////////////////
//
//				CONTINUOUS - continuous movement, keep on going in last movement direction. Not to stop while tracing path.
//
////////////////////////////////////////////////////////
class COPContinuous : public CGoalOp
{
	bool m_bKeepMoving;

public:
	COPContinuous(bool bKeepMoving);
	COPContinuous(const XmlNodeRef& goalOpNode);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser) {}
};

#if 0
// deprecated and won't compile at all...

////////////////////////////////////////////////////////////
//
//				STEER - Makes the puppet try to reach a point and stay near it all the time using the "steering behavior engine"
//
////////////////////////////////////////////////////////
class COPSteer : public CGoalOp
{
public:
	COPSteer(float fSteerDistance = 0.0f, float fPathLenLimit = 0.0f);
	COPSteer(const XmlNodeRef& node);
	virtual ~COPSteer();

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
	virtual void          Serialize(TSerialize ser);

private:
	void RegeneratePath(CPipeUser* pPipeUser, const Vec3& destination);

	Vec3         m_vLastUsedTargetPos;
	CTimeValue   m_fLastRegenTime;

	float        m_fPathLenLimit;
	float        m_fSteerDistanceSqr;
	float        m_fLastDistance;
	float        m_fMinEndDistance, m_fMaxEndDistance;
	float        m_fEndAccuracy;
	bool         m_bNeedHidespot;
	bool         m_bFirstExec;

	COPTrace*    m_pTraceDirective;
	COPPathFind* m_pPathfindDirective;
	CAIObject*   m_pPosDummy;
};
#endif // 0

////////////////////////////////////////////////////////
//
//	waitsignal - waits for a signal and counts down a timer...
//
////////////////////////////////////////////////////////
class COPWaitSignal : public CGoalOp
{
	string m_sSignal;
	enum _edMode
	{
		edNone,
		edString,
		edInt,
		edId
	}          m_edMode;
	string     m_sObjectName;
	int        m_iValue;
	EntityId   m_nID;

	CTimeValue m_startTime;
	int        m_intervalMs;
	bool       m_bSignalReceived;

public:
	COPWaitSignal(const XmlNodeRef& node);

	// (MATT) Note that it appears all but the first form could be removed {2008/08/09}
	COPWaitSignal(const char* sSignal, float fInterval = 0);
	COPWaitSignal(const char* sSignal, const char* sObjectName, float fInterval = 0);
	COPWaitSignal(const char* sSignal, int iValue, float fInterval = 0);
	COPWaitSignal(const char* sSignal, EntityId nID, float fInterval = 0);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

	bool                  NotifySignalReceived(CAIObject* pPipeUser, const char* szText, IAISignalExtraData* pData);
};

////////////////////////////////////////////////////////
//
//	animation -  sets AG input.
//
////////////////////////////////////////////////////////
class COPAnimation : public CGoalOp
{
protected:
	EAnimationMode m_eMode;
	string         m_sValue;
	bool           m_bIsUrgent;

	bool           m_bAGInputSet;

public:
	COPAnimation(EAnimationMode mode, const char* value, const bool isUrgent);
	COPAnimation(const XmlNodeRef& node);

	virtual void          Serialize(TSerialize ser);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
};

////////////////////////////////////////////////////////
//
//	exact positioning animation to be played at the end of the path
//
////////////////////////////////////////////////////////
class COPAnimTarget : public CGoalOp
{
	string m_sAnimName;
	float  m_fStartWidth;
	float  m_fDirectionTolerance;
	float  m_fStartArcAngle;
	Vec3   m_vApproachPos;
	bool   m_bSignal;
	bool   m_bAutoAlignment;

public:
	COPAnimTarget(bool signal, const char* animName, float startWidth, float dirTolerance, float startArcAngle, const Vec3& approachPos, bool autoAlignment);
	COPAnimTarget(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
};

////////////////////////////////////////////////////////
//
//	wait for group of goals to be executed
//
////////////////////////////////////////////////////////
class COPWait : public CGoalOp
{
	EOPWaitType m_WaitType;
	int         m_BlockCount; //	number of non-blocking goals in the block

public:
	COPWait(int waitType, int blockCount);
	COPWait(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
};

#if 0
// deprecated and won't compile at all...

////////////////////////////////////////////////////////////
//
// PROXIMITY - Send signal on proximity
//
////////////////////////////////////////////////////////

class COPProximity : public CGoalOp
{
public:
	COPProximity(float radius, const string& signalName, bool signalWhenDisabled, bool visibleTargetOnly);
	COPProximity(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;

private:
	float               m_radius;
	bool                m_triggered;
	bool                m_signalWhenDisabled;
	bool                m_visibleTargetOnly;
	string              m_signalName;
	CWeakRef<CAIObject> m_refProxObject;
};

////////////////////////////////////////////////////////////
//
//				MOVETOWARDS - move specified distance towards last op result
//
////////////////////////////////////////////////////////
class COPMoveTowards : public CGoalOp
{
public:
	COPMoveTowards(float distance, float duration);
	COPMoveTowards(const XmlNodeRef& node);
	virtual ~COPMoveTowards() { Reset(0); }

	virtual void          Reset(CPipeUser* pPipeUser);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
	virtual void          Serialize(TSerialize ser);

private:
	void  ResetNavigation(CPipeUser* pPipeUser);

	float GetEndDistance(CPipeUser* pPipeUser) const;

	float        m_distance;
	float        m_duration;
	int          m_looseAttentionId;
	Vec3         m_moveStart;
	Vec3         m_moveEnd;
	float        m_moveDist;
	float        m_moveSearchRange;

	COPTrace*    m_pTraceDirective;
	COPPathFind* m_pPathfindDirective;
};

////////////////////////////////////////////////////////////
//
// DODGE - Dodges a target
//
////////////////////////////////////////////////////////

class COPDodge : public CGoalOp
{
public:
	COPDodge(float distance, bool useLastOpAsBackup);
	COPDodge(const XmlNodeRef& node);
	virtual ~COPDodge() { Reset(0); }

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;

private:

	typedef std::pair<Vec3, float> Vec3FloatPair;

	bool OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos);
	void GetNearestActors(CAIActor* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions);

	float                      m_distance;
	bool                       m_useLastOpAsBackup;
	float                      m_endAccuracy;
	CTimeValue                 m_lastTime;
	float                      m_notMovingTime;
	COPTrace*                  m_pTraceDirective;
	std::vector<Vec3FloatPair> m_avoidPos;

	std::vector<Vec3>          m_DEBUG_testSegments;
	Matrix33                   m_basis;
	Vec3                       m_targetPos;
	Vec3                       m_targetView;
	Vec3                       m_bestPos;
};

////////////////////////////////////////////////////////////
//
// COMPANIONSTICK - Special stick for companion that introduces
//	speed control and rubberband-like movement
//
////////////////////////////////////////////////////////

class COPCompanionStick : public CGoalOp
{
public:
	COPCompanionStick(float fNotReactingDistance, float fForcedMoveDistance, float fLeaderInfluenceRange);
	COPCompanionStick(const XmlNodeRef& node);
	virtual ~COPCompanionStick();

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual void          DebugDraw(CPipeUser* pOperandconst) const;

private:
	void RegeneratePath(CPipeUser* pPipeUser, const Vec3& destination);
	void AdjustSpeed(CPipeUser* pPipeUser);

	Vec3         m_vLastUsedTargetPos;
	Vec3         m_vCurrentTargetPos;           // caution: different from the point used to generate path
	CTimeValue   m_fLastRegenTime;

	float        m_fMoveWillingness;
	float        m_fLeaderInfluenceRange;
	float        m_fNotReactingDistance;
	float        m_fForcedMoveDistance;

	float        m_fPathLenLimit;
	float        m_fSteerDistanceSqr;
	float        m_fLastDistance;
	float        m_fMinEndDistance, m_fMaxEndDistance;
	float        m_fEndAccuracy;
	bool         m_bNeedHidespot;
	bool         m_bFirstExec;

	COPTrace*    m_pTraceDirective;
	COPPathFind* m_pPathfindDirective;
	CAIObject*   m_pPosDummy;
};

#endif // 0

////////////////////////////////////////////////////////////
//
//			COMMUNICATION - play AI communication
//
////////////////////////////////////////////////////////
class COPCommunication : public CGoalOp, ICommunicationManager::ICommInstanceListener
{
	CommID                           m_commID;
	CommChannelID                    m_channelID;
	SCommunicationRequest::EOrdering m_ordering;
	float                            m_expirity;
	float                            m_minSilence;

	bool                             m_ignoreSound;
	bool                             m_ignoreAnim;

	float                            m_timeout;
	CTimeValue                       m_startTime;

	CommPlayID                       m_playID;

	bool                             m_bInitialized;

	//Set on communication end
	bool m_commFinished;
	bool m_waitUntilFinished;

public:
	COPCommunication(const char* commName, const char* channelName, const char* ordering = "unordered", float expirity = 0.0f, float minSilence = -1.0f, bool ignoreSound = false, bool ignoreAnim = false);
	COPCommunication(const XmlNodeRef& node);

	virtual ~COPCommunication(){ Reset(0); }

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

	virtual void          Reset(CPipeUser* pPipeUser);

	//ICommunicationManager::ICommInstanceListener
	virtual void OnCommunicationEvent(ICommunicationManager::ECommunicationEvent event, EntityId actorID,
	                                  const CommPlayID& playID);
	//~ICommunicationManager::ICommInstanceListener

};

////////////////////////////////////////////////////////////
//
// DUMMY - Does nothing except for returning AIGOR_DONE
//
////////////////////////////////////////////////////////

class COPDummy : public CGoalOp
{
public:
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
};

#endif  // #ifndef __GOALOP_H_
