// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   GoalOp_Crysis2.h
   Description: Crysi2 GoalOps
             These should move into GameDLL when interfaces allow!
   -------------------------------------------------------------------------
   History:
   - 18:11:2009 - Created by Márcio Martins
 *********************************************************************/

#ifndef __GoalOp_Crysis2_H__
#define __GoalOp_Crysis2_H__

#pragma once

#include "GoalOp.h"
#include "GoalOpFactory.h"
#include "Communication/Communication.h"
#include "PostureManager.h"
#include "GenericAStarSolver.h"

// Forward declarations
class COPPathFind;
class COPTrace;

/**
 * Factory for Crysis2 goalops
 *
 */
class CGoalOpFactoryCrysis2 : public IGoalOpFactory
{
	IGoalOp* GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const;
	IGoalOp* GetGoalOp(EGoalOperations op, GoalParameters& params) const;
};

////////////////////////////////////////////////////////
//
//	Adjust aim while staying still.
//
////////////////////////////////////////////////////////
class COPCrysis2AdjustAim : public CGoalOp
{
	CTimeValue                     m_startTime;
	CTimeValue                     m_lastGood;
	bool                           m_useLastOpAsBackup;
	bool                           m_allowProne;
	float                          m_timeoutRandomness;
	int                            m_timeoutMs;
	int                            m_runningTimeoutMs;
	int                            m_nextUpdateMs;
	PostureManager::PostureID      m_bestPostureID;
	PostureManager::PostureQueryID m_queryID;

	int  RandomizeTimeInterval() const;
	bool ProcessQueryResult(CPipeUser* pipeUser, AsyncState queryStatus, PostureManager::PostureInfo* postureInfo);

public:
	COPCrysis2AdjustAim(bool useLastOpAsBackup, bool allowProne, float timeout);
	explicit COPCrysis2AdjustAim(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
};

////////////////////////////////////////////////////////
//
//	Peek while staying in cover.
//
////////////////////////////////////////////////////////
class COPCrysis2Peek :
	public CGoalOp
{
	CTimeValue                     m_startTime;
	CTimeValue                     m_lastGood;
	bool                           m_useLastOpAsBackup;
	float                          m_timeoutRandomness;
	int                            m_timeoutMs;
	int                            m_runningTimeoutMs;
	int                            m_nextUpdateMs;
	PostureManager::PostureID      m_bestPostureID;
	PostureManager::PostureQueryID m_queryID;

	int  RandomizeTimeInterval() const;
	bool ProcessQueryResult(CPipeUser* pipeUser, AsyncState queryStatus, PostureManager::PostureInfo* postureInfo);

public:
	COPCrysis2Peek(bool useLastOpAsBackup, float timeout);
	explicit COPCrysis2Peek(const XmlNodeRef& node);

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
};

////////////////////////////////////////////////////////////
//
//	HIDE - makes agent find closest hiding place and then hide there
//
////////////////////////////////////////////////////////
class COPCrysis2Hide : public CGoalOp
{
public:
	COPCrysis2Hide(EAIRegister location, bool exact);
	explicit COPCrysis2Hide(const XmlNodeRef& node);
	COPCrysis2Hide(const COPCrysis2Hide& rhs);
	virtual ~COPCrysis2Hide();

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);

	void                  UpdateMovingToCoverAnimation(CPipeUser* pPipeUser) const;

	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;
private:
	void                  CreateHideTarget(CPipeUser* pPipeUser, const Vec3& pos, const Vec3& dir = Vec3Constants<float>::fVec3_Zero);

	CStrongRef<CAIObject> m_refHideTarget;
	EAIRegister           m_location;
	bool                  m_exact;

	COPPathFind*          m_pPathfinder;
	COPTrace*             m_pTracer;
};

////////////////////////////////////////////////////////////
//
//	COMMUNICATE - makes agent communicate (duh!)
//
////////////////////////////////////////////////////////
class COPCrysis2Communicate : public CGoalOp
{
public:
	COPCrysis2Communicate(CommID commID, CommChannelID channelID, float expirity, EAIRegister target,
	                      SCommunicationRequest::EOrdering ordering);
	explicit COPCrysis2Communicate(const XmlNodeRef& node);
	virtual ~COPCrysis2Communicate();

	EGoalOpResult Execute(CPipeUser* pPipeUser);
private:
	CommID                           m_commID;
	CommChannelID                    m_channelID;
	SCommunicationRequest::EOrdering m_ordering;

	float                            m_expirity;
	EAIRegister                      m_target;
};

////////////////////////////////////////////////////////////
//
//	STICKPATH - Follows an AI path, sticking to a target
//				projected on the path
//
////////////////////////////////////////////////////////////
class COPCrysis2StickPath : public CGoalOp
{
public:
	COPCrysis2StickPath(bool bFinishInRange, float fMaxStickDist, float fMinStickDist = 0.0f, bool bCanReverse = true);
	explicit COPCrysis2StickPath(const XmlNodeRef& node);
	COPCrysis2StickPath(const COPCrysis2StickPath& rhs);
	virtual ~COPCrysis2StickPath();

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

private:
	// State execute helpers
	EGoalOpResult ExecuteCurrentState(CPipeUser* pPipeUser, bool bDryUpdate);
	bool          ExecuteState_Prepare(CPuppet* pPuppet, bool bDryUpdate);
	bool          ExecuteState_Navigate(CPuppet* pPuppet, bool bDryUpdate);
	bool          ExecuteState_Wait(CPuppet* pPuppet, bool bDryUpdate);

	// Project the target onto the path
	void ProjectTargetOntoPath(Vec3& outPos, float& outNearestDistance, float& outDistanceAlongPath) const;

	// Returns true if within range of target
	enum ETargetRange
	{
		eTR_TooFar,
		eTR_InRange,
		eTR_TooClose,
	};
	ETargetRange GetTargetRange(const Vec3& vMyPos) const;
	ETargetRange GetTargetRange(const Vec3& vMyPos, const Vec3& vTargetPos) const;

	// Gets the projected position to use for the vehicle based on speed settings
	Vec3 GetProjectedPos(CPuppet* pPuppet) const;

private:
	// Current state
	enum EStates
	{
		eState_FIRST,

		eState_Prepare = eState_FIRST,
		eState_Navigate,
		eState_Wait,

		eState_COUNT,
	};
	EStates m_State;

	// Current designer path shape
	SShape                m_Path;
	float                 m_fPathLength;

	CWeakRef<CAIObject>   m_refTarget;
	CStrongRef<CAIObject> m_refNavTarget;

	float                 m_fMinStickDistSq;
	float                 m_fMaxStickDistSq;
	bool                  m_bFinishInRange;
	bool                  m_bCanReverse;
};

class COPAcquirePosition : public CGoalOp
{
	enum COPAcquirePositionState
	{
		C2AP_INVALID,
		C2AP_INIT,
		C2AP_COMPUTING,
		C2AP_FAILED,
	};

	enum COPAPComputingSubstate
	{
		C2APCS_GATHERSPANS,
		C2APCS_CHECKSPANS,
		C2APCS_VISTESTS,
	};

	COPAcquirePositionState           m_State;
	COPAPComputingSubstate            m_SubState;
	EAIRegister                       m_target;
	EAIRegister                       m_output;
	Vec3                              m_curPosition;
	Vec3                              m_destination;
	std::vector<Vec3i>                m_Coords;

	//const CFlightNavRegion2::NavData* m_Graph;

public:

	COPAcquirePosition();
	explicit COPAcquirePosition(const XmlNodeRef& node);

	virtual ~COPAcquirePosition();

	EGoalOpResult Execute(CPipeUser* pPipeUser);
	void          ExecuteDry(CPipeUser* pPipeUser);
	void          Reset(CPipeUser* pPipeUser);
	bool          GetTarget(CPipeUser* pPipeUser, Vec3& target);
	bool          SetOutput(CPipeUser* pPipeUser, Vec3& target);
};

struct AStarNodeCompareFlight
{
	bool operator()(const AStarNode<Vec3i>& LHS, const AStarNode<Vec3i>& RHS) const
	{
		const Vec3i& lhs = LHS.graphNodeIndex;
		const Vec3i& rhs = RHS.graphNodeIndex;

		return lhs.x + lhs.y * 1000 + lhs.z * 1000000 < rhs.x + rhs.y * 1000 + rhs.z * 1000000;
	}
};

class NodeContainerFlight
{
	std::set<AStarNode<Vec3i>, AStarNodeCompareFlight> aStarNodes;

public:

	AStarNode<Vec3i>* GetAStarNode(Vec3i nodeIndex)
	{
		std::pair<std::set<AStarNode<Vec3i>, AStarNodeCompareFlight>::iterator, bool> result =
		  aStarNodes.insert(AStarNode<Vec3i>(nodeIndex));

		return const_cast<AStarNode<Vec3i>*>(&*result.first);
	}

	void Clear()
	{
		aStarNodes.clear();
	}
};

class ClosedListFlight
{
	std::set<Vec3i, AStarNodeCompareFlight> closedList;
public:

	void Resize(Vec3i size)
	{
		closedList.clear();
	}

	void Close(Vec3i index)
	{
		closedList.insert(index);
	}

	bool IsClosed(Vec3i index) const
	{
		return closedList.find(index) != closedList.end();
	}

	//For users debug benefit they may template specialize this function:
	void Print() const {}
};

class COPCrysis2Fly : public CGoalOpParallel
{

	typedef CGoalOpParallel Base;

	enum COPCrysis2FlyState
	{
		C2F_INVALID,
		C2F_PATHFINDING,
		C2F_FOLLOWINGPATH,
		C2F_FOLLOW_AND_CALC_NEXT,
		C2F_FOLLOW_AND_SWITCH,
		C2F_FAILED,
		C2F_FAILED_TEMP,
	};

	enum TargetResult
	{
		C2F_NO_TARGET,
		C2F_TARGET_FOUND,
		C2F_SET_PATH,
	};

	struct Threat
	{
		EntityId threatId;
		Vec3     threatPos;
		Vec3     threatDir;

		Threat() { Reset(); }

		void Reset() { threatId = 0; threatPos = ZERO; threatDir = ZERO; }
	};

	COPCrysis2FlyState m_State;
	EAIRegister        m_target;
	Vec3               m_CurSegmentDir;
	Vec3               m_destination;
	Vec3               m_nextDestination;
	uint32             m_Length;
	uint32             m_PathSizeFinish;
	float              m_Timeout;
	float              m_lookAheadDist;
	float              m_desiredSpeed;
	float              m_currentSpeed;

	float              m_minDistance;
	float              m_maxDistance;
	int                m_TargetEntity;

	Threat             m_Threat;

	bool               m_Circular;
public:

	//typedef GenericAStarSolver<CFlightNavRegion2::NavData, CFlightNavRegion2::NavData, CFlightNavRegion2::NavData, DefaultOpenList<Vec3i>, ClosedListFlight, NodeContainerFlight> ASTARSOLVER;
	//typedef stl::PoolAllocator<sizeof(ASTARSOLVER)>                                                                                                                               SolverAllocator;

private:

	std::vector<Vec3>                 m_PathOut;
	std::vector<Vec3>                 m_Reversed;

	/*ASTARSOLVER*                      m_Solver;

	const CFlightNavRegion2::NavData* m_Graph;

	static SolverAllocator            m_Solvers;

	ASTARSOLVER* AllocateSolver()
	{
		return new(m_Solvers.Allocate())ASTARSOLVER;
	}

	void DestroySolver(ASTARSOLVER* solver)
	{
		solver->~ASTARSOLVER();
		m_Solvers.Deallocate(solver);
	}*/

	TargetResult  GetTarget(CPipeUser* pPipeUser, Vec3& target) const;
	EGoalOpResult CalculateTarget(CPipeUser* pPipeUser);
	bool          HandleThreat(CPipeUser* pPipeUser, const Vec3& lookAheadPos);

public:

	COPCrysis2Fly();
	COPCrysis2Fly(const COPCrysis2Fly& rhs);
	explicit COPCrysis2Fly(const XmlNodeRef& node);

	virtual ~COPCrysis2Fly();

	EGoalOpResult Execute(CPipeUser* pPipeUser);
	void          ExecuteDry(CPipeUser* pPipeUser);
	void          Reset(CPipeUser* pPipeUser);

	void          ParseParam(const char* param, const GoalParams& value);
	void          SendEvent(CPipeUser* pPipeUser, const char* eventName);

	float         CheckTargetEntity(const CPipeUser* pPipeUser, const Vec3& lookAheadPos);

	void          Serialize(TSerialize ser);

};

class COPCrysis2ChaseTarget : public CGoalOpParallel
{
	typedef CGoalOpParallel Base;

	enum COPCrysis2ChaseTargetState
	{
		C2F_INVALID,
		C2F_FINDINGLOCATION,
		C2F_WAITING_LOCATION_RESULT,
		C2F_PREPARING,
		C2F_CHASING,
		C2F_FAILED,
		C2F_FAILED_TEMP,
	};

	enum TargetResult
	{
		C2F_NO_TARGET,
		C2F_TARGET_FOUND,
		C2F_SET_PATH,
	};

	struct CandidateLocation
	{
		Vec3   point;
		size_t index;
		float  fraction;

		float  score;

		CandidateLocation()
		{
			// nothing
		}

		explicit CandidateLocation(type_zero)
			: point(Vec3Constants<float>::fVec3_Zero)
			, index(0)
			, fraction(0.0f)
			, score(0.0f)
		{
		}

		bool operator<(const CandidateLocation& other) const
		{
			return score > other.score;
		}
	};

	typedef std::vector<CandidateLocation> Candidates;

	COPCrysis2ChaseTargetState m_State;
	EAIRegister                m_target;
	Vec3                       m_destination;
	Vec3                       m_nextDestination;
	float                      m_lookAheadDist;
	float                      m_desiredSpeed;

	float                      m_distanceMin;
	float                      m_distanceMax;

	QueuedRayID                m_visCheckRayID[2];
	size_t                     m_visCheckResultCount;
	CandidateLocation          m_bestLocation;
	CandidateLocation          m_testLocation;

	ListPositions              m_PathOut;
	ListPositions              m_throughOriginBuf;

	GoalParams                 m_firePauseParams;

	TargetResult  GetTarget(CPipeUser* pPipeUser, Vec3& target, IAIObject** targetObject = 0) const;
	EGoalOpResult Chase(CPipeUser* pPipeUser);
	void          TestLocation(const CandidateLocation& location, CPipeUser* pipeUser, const Vec3& target);

	size_t        GatherCandidateLocations(float currentLocationAlong, const ListPositions& path, float pathLength,
	                                       bool closed, float spacing, const Vec3& target, Candidates& candidates);
	Candidates m_candidates;

	void  VisCheckRayComplete(const QueuedRayID& rayID, const RayCastResult& result);

	float CalculateShortestPathTo(const ListPositions& path, bool closed, size_t startIndex, float startSegmentFraction,
	                              size_t endIndex, float endSegmentFraction, ListPositions& pathOut);

public:
	COPCrysis2ChaseTarget();
	COPCrysis2ChaseTarget(const COPCrysis2ChaseTarget& rhs);
	explicit COPCrysis2ChaseTarget(const XmlNodeRef& node);

	virtual ~COPCrysis2ChaseTarget();

	EGoalOpResult Execute(CPipeUser* pPipeUser);
	void          ExecuteDry(CPipeUser* pPipeUser);
	void          Reset(CPipeUser* pPipeUser);

	void          ParseParam(const char* param, const GoalParams& value);
};

class COPCrysis2FlightFireWeapons : public CGoalOpParallel
{
	enum
	{
		AI_REG_INTERNAL_TARGET = AI_REG_LAST + 1,
	};

	enum COPCrysis2FlightFireWeaponsState
	{
		eFP_START,
		eFP_STOP,
		eFP_DONE,
		eFP_WAIT,
		eFP_WAIT_INIT,
		eFP_PREPARE,
		eFP_PAUSED,
		eFP_PAUSED_OVERRIDE,
		eFP_RUN_INDEFINITELY,
	};

	COPCrysis2FlightFireWeaponsState m_State;
	COPCrysis2FlightFireWeaponsState m_NextState;
	Vec3                             m_LastTarget;
	float                            m_InitCoolDown;
	float                            m_WaitTime;
	float                            m_minTime;
	float                            m_maxTime;
	float                            m_PausedTime;
	float                            m_PauseOverrideTime;
	float                            m_Rotation;
	uint32                           m_SecondaryWeapons;
	uint32                           m_targetId;
	EAIRegister                      m_target;
	bool                             m_FirePrimary;
	bool                             m_OnlyLookAt;

public:

	COPCrysis2FlightFireWeapons();
	explicit COPCrysis2FlightFireWeapons(const XmlNodeRef& node);
	COPCrysis2FlightFireWeapons(EAIRegister reg, float minTime, float maxTime, bool primary, uint32 secondary);

	virtual ~COPCrysis2FlightFireWeapons();

	EGoalOpResult Execute(CPipeUser* pPipeUser);
	void          ExecuteDry(CPipeUser* pPipeUser);
	void          ExecuteShoot(CPipeUser* pPipeUser, const Vec3& targetPos);
	void          Reset(CPipeUser* pPipeUser);
	bool          GetTarget(CPipeUser* pPipeUser, Vec3& target);

	void          ParseParam(const char* param, const GoalParams& value);

	bool          CalculateHitPosPlayer(CPipeUser* pPipeUser, CAIPlayer* targetPlayer, Vec3& target);
	bool          CalculateHitPos(CPipeUser* pPipeUser, CAIObject* targetObject, Vec3& target);

	void          Serialize(TSerialize ser);

};

class COPCrysis2Hover : public CGoalOp
{
	enum COPHoverState
	{
		CHS_INVALID,
		CHS_HOVERING,
		CHS_HOVERING_APPROACH,
	};

	Vec3          m_InitialPos;
	Vec3          m_InitialForward;
	Vec3          m_InitialTurn;
	Vec3          m_CurrentTarget;
	EAIRegister   m_target;
	bool          m_Continuous;

	COPHoverState m_State;
	COPHoverState m_NextState;
public:

	COPCrysis2Hover();
	COPCrysis2Hover(EAIRegister reg, bool continuous);
	explicit COPCrysis2Hover(const XmlNodeRef& node);

	virtual ~COPCrysis2Hover();

	void          Reset(CPipeUser* pPipeUser);
	EGoalOpResult Execute(CPipeUser* pPipeUser);
	void          ExecuteDry(CPipeUser* pPipeUser);
	bool          GetTarget(CPipeUser* pPipeUser, Vec3& target);

};
#endif //__GoalOp_Crysis2_H__
