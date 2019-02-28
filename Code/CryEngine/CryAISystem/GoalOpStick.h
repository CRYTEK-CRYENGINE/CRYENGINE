// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GOALOP_STICK_H_
#define __GOALOP_STICK_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include "GoalOp.h"

////////////////////////////////////////////////////////////
//
//			STICK - the agent keeps at a constant distance to his target
//
// regenerate path if target moved for more than m_fTrhDistance
////////////////////////////////////////////////////////

class COPStick : public CGoalOp
{
public:
	COPStick(float fStickDistance, float fAccuracy, float fDuration, int fFlags, int nFlagsAux, ETraceEndMode eTraceEndMode = eTEM_FixedDistance);
	COPStick(const XmlNodeRef& node);
	virtual ~COPStick();

	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);

	virtual void          ExecuteDry(CPipeUser* pPipeUser);
	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);

private:
	virtual void DebugDraw(CPipeUser* pPipeUser) const;
	/// Keep a list of "safe" points of the stick target. This will be likely to have a valid
	/// path between the point and the stick target - so when teleporting is necessary one
	/// of these points (probably the one furthest back and invisible) can be chosen
	void                 UpdateStickTargetSafePoints(CPipeUser* pPipeUser);
	/// Attempts to teleport, and if it's necessary/practical does it, and returns true.
	bool                 TryToTeleport(CPipeUser* pPipeUser);
	/// sets teleporting info to the default don't-need-to-teleport state
	void                 ClearTeleportData();
	void                 RegeneratePath(CPipeUser* pPipeUser, const Vec3& destination);

	float                GetEndDistance(CPipeUser* pPipeUser) const;

	inline bool          GetStickAndSightTargets_CreatePathfindAndTraceGoalOps(CPipeUser* pPipeUser, EGoalOpResult* peGoalOpResult);
	inline bool          Trace(CPipeUser* pPipeUser, CAIObject* pStickTarget, EGoalOpResult* peTraceResult);
	inline bool          HandleHijack(CPipeUser* pPipeUser, const Vec3& vStickTargetPos, float fPathDistanceLeft, EGoalOpResult eTraceResult);
	inline void          HandleTargetPrediction(CPipeUser* pPipeUser, const Vec3& vStickTargetPos);
	inline bool          IsTargetDirty(CPipeUser* pPipeUser, const Vec3& vPipeUserPos, bool b2D, const Vec3& vStickTargetPos, IAISystem::ENavigationType ePipeUserLastNavNodeType);
	inline EGoalOpResult HandlePathDecision(CPipeUser* pPipeUser, int nPathDecision, bool b2D);

	/// Stores when/where the stick target was
	struct SSafePoint
	{
		SSafePoint() : pos(ZERO), safe(false), passRadius(0.0f) {}

		SSafePoint(const Vec3& pos_, CPipeUser& pipeUser);

		void Reset();
		void Serialize(TSerialize ser);

		Vec3       pos;
		bool       safe; // used to store _unsafe_ locations too
		CTimeValue time;

	private:
		string                 requesterName;
		// only used for navgraph and serialization
		IAISystem::tNavCapMask navCapMask;
		float                  passRadius;
	};
	/// Point closest to the stick target is at the front
	typedef MiniQueue<SSafePoint, 32> TStickTargetSafePoints;
	TStickTargetSafePoints m_stickTargetSafePoints;
	/// distance between safe points
	float                  m_safePointInterval;
	/// teleport current/destination locations
	Vec3                   m_teleportCurrent, m_teleportEnd;
	/// time at which the old teleport position was checked
	CTimeValue             m_lastTeleportTime;
	/// time at which the operand was last visible
	CTimeValue             m_lastVisibleTime;

	// teleport params
	/// Only teleport if the resulting apparent speed would not exceed this.
	/// If 0 it disables teleporting
	float               m_maxTeleportSpeed;
	// Only teleport if the operand's path (if it exists) is longer than this
	float               m_pathLengthForTeleport;
	// Don't teleport closer than this to player
	float               m_playerDistForTeleport;

	Vec3                m_vLastUsedTargetPos;
	float               m_fTrhDistance;
	CWeakRef<CAIObject> m_refStickTarget;
	CWeakRef<CAIObject> m_refSightTarget;

	ETraceEndMode       m_eTraceEndMode;
	float               m_fApproachTime;
	float               m_fHijackDistance;

	/// Aim to stay/stop this far from the target
	float m_fStickDistance;
	/// we aim to stop within m_fAccuracy of the target minus m_fStickDistance
	float m_fEndAccuracy;
	/// Stop after this time (0 means disabled)
	float m_fDuration;
	float m_fCorrectBodyDirTime;
	float m_fTimeSpentAligning;
	bool  m_bContinuous;            // stick OR just approach moving target
	bool  m_bTryShortcutNavigation; //
	bool  m_bUseLastOpResult;
	bool  m_bLookAtLastOp;  // where to look at
	bool  m_bInitialized;
	bool  m_bBodyIsAligned;
	bool  m_bAlignBodyBeforeMove;
	bool  m_bForceReturnPartialPath;
	bool  m_bStopOnAnimationStart;
	float m_targetPredictionTime;
	bool  m_bConstantSpeed;

	// used for estimating the target position movement
	CTimeValue m_lastTargetPosTime;
	Vec3       m_lastTargetPos;
	Vec3       m_smoothedTargetVel;

	int        m_looseAttentionId;

	// set to true when the path is first found
	bool         m_bPathFound;
	COPTrace*    m_pTraceDirective;
	COPPathFind* m_pPathfindDirective;

	string       m_sLocateName;
};

#endif  // #ifndef __GOALOP_STICK_H_
