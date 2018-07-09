// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GOALOP_TRACE_H_
#define __GOALOP_TRACE_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include "GoalOp.h"

#include <limits>

// Observes a characters progression along a path and
// signals when it stopped progressing. It's stuck.
class StuckDetector
{
public:
	enum Status
	{
		UserIsStuck,
		UserIsMovingOnFine
	};

	StuckDetector()
	{
		Reset();
	}

	void Reset()
	{
		m_closest = std::numeric_limits<float>::max();
		m_lastProgress = gEnv->pTimer->GetCurrTime();
	}

	Status Update(const float distToEnd)
	{
		const float now = gEnv->pTimer->GetCurrTime();

		if (distToEnd + 0.05f < m_closest)
		{
			m_closest = distToEnd;
			m_lastProgress = now;
		}
		else
		{
			const float timeWithoutProgress = now - m_lastProgress;

			if (timeWithoutProgress > 2.0f)
			{
				return StuckDetector::UserIsStuck;
			}
		}

		return StuckDetector::UserIsMovingOnFine;
	}

private:
	float m_closest;
	float m_lastProgress;
};

////////////////////////////////////////////////////////////
//
//				TRACE - makes agent follow generated path... does nothing if no path generated
//
////////////////////////////////////////////////////////////

class COPTrace : public CGoalOp
{
public:
	COPTrace(
	  bool bExactFollow,
	  float fEndAccuracy = 1.f,
	  bool bForceReturnPartialPath = false,
	  bool bStopOnAnimationStart = false,
	  ETraceEndMode eTraceEndMode = eTEM_FixedDistance
	  );
	COPTrace(const XmlNodeRef& node);
	COPTrace(const COPTrace& rhs);
	virtual ~COPTrace();

	virtual void          Reset(CPipeUser* pPipeUser);
	virtual void          Serialize(TSerialize ser);
	virtual EGoalOpResult Execute(CPipeUser* pPipeUser);
	virtual bool          IsPathRegenerationInhibited() const { return m_inhibitPathRegen || m_passingStraightNavSO; }
	virtual void          DebugDraw(CPipeUser* pPipeUser) const;

	bool                  ExecuteTrace(CPipeUser* pPipeUser, bool bFullUpdate);

	void                  SetControlSpeed(bool bValue) { m_bControlSpeed = bValue; }

	//////////////////////////////////////////////////////////////////////////
	enum EManeuver { eMV_None, eMV_Back, eMV_Fwd };
	enum EManeuverDir { eMVD_Clockwise, eMVD_AntiClockwise };
	enum TraceResult { StillTracing, TraceDone };

	EManeuver     m_Maneuver;
	EManeuverDir  m_ManeuverDir;
	ETraceEndMode m_eTraceEndMode;

	/// we aim to stop within m_fAccuracy of the target minus m_fStickDistance (normally do better than this)
	float m_fEndAccuracy;
	/// finish tracing after this duration
	/// fDuration is used to make us stop gracefully after a certain time
	//  float m_fDuration;

	bool m_passingStraightNavSO;

private:
	bool        ExecutePreamble(CPipeUser* pPipeUser);
	bool        ExecutePostamble(CPipeUser* pPipeUser, bool& reachedEnd, bool fullUpdate, bool twoD);

	bool        ExecutePathFollower(CPipeUser* pPipeUser, bool fullUpdate, IPathFollower* pPathFollower);
	bool        Execute2D(CPipeUser* pPipeUser, bool fullUpdate);
	bool        Execute3D(CPipeUser* pPipeUser, bool fullUpdate);

	void        ExecuteManeuver(CPipeUser* pPipeUser, const Vec3& pathDir);
	/// returns true when landing is completed
	bool        ExecuteLanding(CPipeUser* pPipeUser, const Vec3& pathEnd);

	inline void ExecuteTraceDebugDraw(CPipeUser* pPipeUser);

	inline bool HandleAnimationPhase(CPipeUser* pPipeUser, bool bFullUpdate, bool* pbForceRegeneratePath, bool* pbExactPositioning, bool* pbTraceFinished);
	inline bool HandlePathResult(CPipeUser* pPipeUser, bool* pbReturnValue);
	inline bool IsVehicleAndDriverIsFallen(CPipeUser* pPipeUser);
	inline void RegeneratePath(CPipeUser* pPipeUser, bool* pbForceRegeneratePath);
	void        StopMovement(CPipeUser* pPipeUser);
	inline void TriggerExactPositioning(CPipeUser* pPipeUser, bool* pbForceRegeneratePath, bool* pbExactPositioning);
	void        Teleport(CPipeUser& pipeUser, const Vec3& teleportDestination);

private:
	static PathFollowResult::TPredictedStates s_tmpPredictedStates;
	static int                                s_instanceCount;

private:
	//////////////////////////////////////////////////////////////////////////
	bool       m_bBlock_ExecuteTrace_untilFullUpdateThenReset; // set when we should follow the path no more

	float      m_ManeuverDist;
	CTimeValue m_ManeuverTime;
	/// For things like helicopters that land then this gets set to some offset used at the end of the
	/// path. Then at the path end the descent is gradually controlled - path only finishes when the
	/// agent touches ground
	float               m_landHeight;
	/// Specifies how high we should be above the path at the current point
	float               m_workingLandingHeightOffset;
	// The ground position and facing direction
	Vec3                m_landingPos;
	Vec3                m_landingDir;

	bool                m_bExactFollow; /// Don't know what bExactFollow means!
	/// bForceReturnPartialPath is used when we regen the path internally
	bool                m_bForceReturnPartialPath;
	Vec3                m_lastPosition;
	CTimeValue          m_prevFrameStartTime;
	float               m_TimeStep;
	CWeakRef<CPipeUser> m_refPipeUser;  // The destructor needs to know the most recent pipe user
	int                 m_looseAttentionId;
	/// keep track of how long we've been tracing a path for - however, this is more of a time
	/// since the last "event" - it can get reset when we regenerate the path etc
	float m_fTotalTracingTime;
	bool  m_inhibitPathRegen;

	bool  m_bWaitingForPathResult;
	bool  m_bWaitingForBusySmartObject; // Indicates movement is paused waiting for a busy smart object to become available

	bool  m_earlyPathRegen;
	bool  m_bControlSpeed; // indicates whether this goalop will control/modify PIpeUser's speed

	/// How far we've moved since the very start
	float m_fTravelDist;
	float m_accumulatedFailureTime;

	enum ETraceActorTgtRequest
	{
		eTATR_None,      // no request
		eTATR_NavSO,     // navigational smart object
		eTATR_EndOfPath, // end of path exact positioning
	};

	/// flag indicating the the trace requested exact positioning.
	ETraceActorTgtRequest m_actorTargetRequester;
	ETraceActorTgtRequest m_pendingActorTargetRequester;
	CStrongRef<CAIObject> m_refNavTarget;

	StuckDetector         m_stuckDetector;

	/// bStopOnAnimationStart would make trace finish once the exact positioning animation at the end of path is started
	bool m_stopOnAnimationStart;
};

#endif  // #ifndef __GOALOP_TRACE_H_
