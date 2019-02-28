// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GoalOpTrace.h"
#include "DebugDrawContext.h"
#include "Puppet.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Navigation/NavigationSystem/OffMeshNavigationManager.h"
#include "Navigation/MNM/OffGridLinks.h"

PathFollowResult::TPredictedStates COPTrace::s_tmpPredictedStates;
int COPTrace::s_instanceCount;

//===================================================================
// COPTrace
//===================================================================
COPTrace::COPTrace(bool bExactFollow, float fEndAccuracy, bool bForceReturnPartialPath, bool bStopOnAnimationStart, ETraceEndMode eTraceEndMode)
	: m_Maneuver(eMV_None)
	, m_ManeuverDir(eMVD_Clockwise)
	, m_eTraceEndMode(eTraceEndMode)
	, m_fEndAccuracy(fEndAccuracy)
	, m_passingStraightNavSO(false)
	, m_bBlock_ExecuteTrace_untilFullUpdateThenReset(false)
	, m_ManeuverDist(0.f)
	, m_landHeight(0.f)
	, m_workingLandingHeightOffset(0.f)
	, m_landingPos(ZERO)
	, m_landingDir(ZERO)
	, m_bExactFollow(bExactFollow)
	, m_bForceReturnPartialPath(bForceReturnPartialPath)
	, m_lastPosition(ZERO)
	, m_prevFrameStartTime(-1000ll)
	, m_TimeStep(0.1f)
	, m_looseAttentionId(0)
	, m_fTotalTracingTime(0.f)
	, m_inhibitPathRegen(false)
	, m_bWaitingForPathResult(false)
	, m_bWaitingForBusySmartObject(false)
	, m_earlyPathRegen(false)
	, m_bControlSpeed(true)
	, m_fTravelDist(0.f)
	, m_accumulatedFailureTime(0.0f)
	, m_actorTargetRequester(eTATR_None)
	, m_pendingActorTargetRequester(eTATR_None)
	, m_stopOnAnimationStart(bStopOnAnimationStart)
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPTrace::COPTrace %p", this);

	++s_instanceCount;
}

COPTrace::COPTrace(const XmlNodeRef& node)
	: m_Maneuver(eMV_None)
	, m_ManeuverDir(eMVD_Clockwise)
	, m_eTraceEndMode(eTEM_FixedDistance)
	, m_fEndAccuracy(0.f)
	, m_passingStraightNavSO(false)
	, m_bBlock_ExecuteTrace_untilFullUpdateThenReset(false)
	, m_ManeuverDist(0.f)
	, m_landHeight(0.f)
	, m_workingLandingHeightOffset(0.f)
	, m_landingPos(ZERO)
	, m_landingDir(ZERO)
	, m_bExactFollow(s_xml.GetBool(node, "exactFollow"))
	, m_bForceReturnPartialPath(s_xml.GetBool(node, "forceReturnPartialPath"))
	, m_lastPosition(ZERO)
	, m_prevFrameStartTime(-1000ll)
	, m_TimeStep(0.1f)
	, m_looseAttentionId(0)
	, m_fTotalTracingTime(0.f)
	, m_inhibitPathRegen(false)
	, m_bWaitingForPathResult(false)
	, m_bWaitingForBusySmartObject(false)
	, m_earlyPathRegen(false)
	, m_bControlSpeed(true)
	, m_fTravelDist(0.f)
	, m_accumulatedFailureTime(0.0f)
	, m_actorTargetRequester(eTATR_None)
	, m_pendingActorTargetRequester(eTATR_None)
	, m_stopOnAnimationStart(s_xml.GetBool(node, "stopOnAnimationStart"))
{
	node->getAttr("endAccuracy", m_fEndAccuracy);

	++s_instanceCount;
}

COPTrace::COPTrace(const COPTrace& rhs)
	: m_Maneuver(eMV_None)
	, m_ManeuverDir(eMVD_Clockwise)
	, m_eTraceEndMode(eTEM_FixedDistance)
	, m_fEndAccuracy(rhs.m_fEndAccuracy)
	, m_passingStraightNavSO(false)
	, m_bBlock_ExecuteTrace_untilFullUpdateThenReset(false)
	, m_ManeuverDist(0.f)
	//, m_ManeuverTime(0.f)
	, m_landHeight(0.f)
	, m_workingLandingHeightOffset(0.f)
	, m_landingPos(ZERO)
	, m_landingDir(ZERO)
	, m_bExactFollow(rhs.m_bExactFollow)
	, m_bForceReturnPartialPath(rhs.m_bForceReturnPartialPath)
	, m_lastPosition(ZERO)
	, m_prevFrameStartTime(-1000ll)
	, m_TimeStep(0.1f)
	, m_looseAttentionId(0)
	, m_fTotalTracingTime(0.f)
	, m_inhibitPathRegen(false)
	, m_bWaitingForPathResult(false)
	, m_bWaitingForBusySmartObject(false)
	, m_earlyPathRegen(false)
	, m_bControlSpeed(true)
	, m_fTravelDist(0.f)
	, m_accumulatedFailureTime(0.0f)
	, m_actorTargetRequester(eTATR_None)
	, m_pendingActorTargetRequester(eTATR_None)
	, m_refNavTarget()
	, m_stopOnAnimationStart(rhs.m_stopOnAnimationStart)
{
	++s_instanceCount;
}

//===================================================================
// ~COPTrace
//===================================================================
COPTrace::~COPTrace()
{
	CCCPOINT(COPTrace_Destructor);

	CPipeUser* const pPipeUser = m_refPipeUser.GetAIObject();
	if (pPipeUser)
	{
		pPipeUser->CancelRequestedPath(false);
		pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
		m_looseAttentionId = 0;

		SOBJECTSTATE& state = pPipeUser->m_State;
		if (!state.continuousMotion)
			state.fDesiredSpeed = 0.f;

		pPipeUser->ClearPath("COPTrace::~COPTrace m_Path");
	}

	m_refPipeUser.Reset();
	m_refNavTarget.Release();

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPTrace::~COPTrace %p %s", this, GetNameSafe(pPipeUser));

	--s_instanceCount;
	if (s_instanceCount == 0)
	{
		stl::free_container(s_tmpPredictedStates);
	}
}

//===================================================================
// Reset
//===================================================================
void COPTrace::Reset(CPipeUser* pPipeUser)
{
	CCCPOINT(COPTrace_Reset);

	if (pPipeUser && gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPTrace::Reset %s", GetNameSafe(pPipeUser));

	if (pPipeUser)
	{
		CCCPOINT(COPTrace_Reset_A);

		pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
		m_looseAttentionId = 0;

		// If we have regenerated the path as part of the periodic regeneration
		// then the request must be cancelled. However, don't always cancel requests
		// because if stick is trying to regenerate the path and we're
		// resetting because we reached the end of the current one, then we'll stop
		// stick from getting its pathfind request.
		// Generally the m_Path.Empty() condition decides between these cases, but
		// Danny todo: not sure if it will always work.
		if (!pPipeUser->m_Path.Empty())
		{
			pPipeUser->CancelRequestedPath(false);
		}

		pPipeUser->m_nPathDecision = PATHFINDER_ABORT;
		pPipeUser->m_State.fDesiredSpeed = 0.f;
		pPipeUser->ClearPath("COPTrace::Reset m_Path");
	}

	// (MATT) Surely we should reset the stored operand here - but the dummy might best stay, if it exists already {2009/02/18}
	m_refPipeUser.Reset();
	m_refNavTarget.Release();

	m_fTotalTracingTime = 0.f;
	m_Maneuver = eMV_None;
	m_ManeuverDist = 0.f;
	m_ManeuverTime = GetAISystem()->GetFrameStartTime();
	m_landHeight = 0.f;
	m_landingDir.zero();
	m_workingLandingHeightOffset = 0.f;
	m_inhibitPathRegen = false;
	m_bBlock_ExecuteTrace_untilFullUpdateThenReset = false;
	m_actorTargetRequester = eTATR_None;
	m_pendingActorTargetRequester = eTATR_None;
	m_bWaitingForPathResult = false;
	m_bWaitingForBusySmartObject = false;
	m_passingStraightNavSO = false;
	m_fTravelDist = 0;

	m_TimeStep = 0.1f;
	m_prevFrameStartTime = -1000ll;
	m_accumulatedFailureTime = 0.0f;

	if (pPipeUser)
	{
		if (m_bExactFollow)
		{
			pPipeUser->m_bLooseAttention = false;
		}
	}
}

//===================================================================
// Serialize
//===================================================================
void COPTrace::Serialize(TSerialize ser)
{
	ser.Value("m_bBlock_ExecuteTrace_untilFullUpdateThenReset", m_bBlock_ExecuteTrace_untilFullUpdateThenReset);
	ser.Value("m_ManeuverDist", m_ManeuverDist);
	ser.Value("m_ManeuverTime", m_ManeuverTime);
	ser.Value("m_landHeight", m_landHeight);
	ser.Value("m_workingLandingHeightOffset", m_workingLandingHeightOffset);
	ser.Value("m_landingPos", m_landingPos);
	ser.Value("m_landingDir", m_landingDir);

	ser.Value("m_bExactFollow", m_bExactFollow);
	ser.Value("m_bForceReturnPartialPath", m_bForceReturnPartialPath);
	ser.Value("m_lastPosition", m_lastPosition);
	ser.Value("m_prevFrameStartTime", m_prevFrameStartTime);
	ser.Value("m_fTravelDist", m_fTravelDist);
	ser.Value("m_TimeStep", m_TimeStep);

	m_refPipeUser.Serialize(ser, "m_refPipeUser");

	ser.EnumValue("m_Maneuver", m_Maneuver, eMV_None, eMV_Fwd);
	ser.EnumValue("m_ManeuverDir", m_ManeuverDir, eMVD_Clockwise, eMVD_AntiClockwise);

	ser.Value("m_fTotalTracingTime", m_fTotalTracingTime);
	ser.Value("m_inhibitPathRegen", m_inhibitPathRegen);
	ser.Value("m_fEndAccuracy", m_fEndAccuracy);
	ser.Value("m_looseAttentionId", m_looseAttentionId);

	ser.Value("m_bWaitingForPathResult", m_bWaitingForPathResult);
	ser.Value("m_bWaitingForBusySmartObject", m_bWaitingForBusySmartObject);
	ser.Value("m_passingStraightNavSO", m_passingStraightNavSO);
	ser.EnumValue("m_actorTargetRequester", m_actorTargetRequester, eTATR_None, eTATR_EndOfPath);
	ser.EnumValue("m_pendingActorTargetRequester", m_pendingActorTargetRequester, eTATR_None, eTATR_EndOfPath);
	ser.Value("m_stopOnAnimationStart", m_stopOnAnimationStart);

	m_refNavTarget.Serialize(ser, "m_refNavTarget");
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPTrace::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPTrace_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool bTraceFinished = ExecuteTrace(pPipeUser, /* full update */ true);

	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

	if (pPipeUser->m_nPathDecision == PATHFINDER_STILLFINDING)
	{
		if (bTraceFinished)
		{
			pipeUserState.fDesiredSpeed = 0.f;
		}
		return eGOR_IN_PROGRESS;
	}
	else
	{
		if (bTraceFinished)
		{
			// Kevin - Clean up residual data that is causing problems elsewhere
			pipeUserState.fDistanceToPathEnd = 0.f;

			if (pipeUserState.curActorTargetPhase == eATP_Error)
			{
				return eGOR_FAILED;
			}
		}
		return bTraceFinished ? eGOR_SUCCEEDED : eGOR_IN_PROGRESS;
	}
}

//===================================================================
// ExecuteTrace
// this is the same old COPTrace::Execute method
// but now, smart objects should control when it
// should be executed and how its return value
// will be interpreted
//===================================================================
bool COPTrace::ExecuteTrace(CPipeUser* pPipeUser, bool bFullUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// The destructor needs to know the most recent pipe user
	if (m_refPipeUser != pPipeUser)
		m_refPipeUser = GetWeakRef(pPipeUser);

	// HACK: Special case fix for drivers in fall&play
	if (IsVehicleAndDriverIsFallen(pPipeUser))
		return false; // Trace not finished.

	bool bTraceFinished = false;

	if (m_bWaitingForPathResult)
		if (!HandlePathResult(pPipeUser, &bTraceFinished))
			return bTraceFinished;

	if (m_bBlock_ExecuteTrace_untilFullUpdateThenReset)
	{
		if (bFullUpdate)
		{
			Reset(pPipeUser);
		}
		else
		{
			StopMovement(pPipeUser);
		}
		return true;  // Trace finished
	}

	bool bForceRegeneratePath = false;
	bool bExactPositioning = false;

	// Handle exact positioning and vaSOs.
	if (!HandleAnimationPhase(pPipeUser, bFullUpdate, &bForceRegeneratePath, &bExactPositioning, &bTraceFinished))
		return bTraceFinished;

	// On first frame: [6/4/2010 evgeny]
	if (m_prevFrameStartTime.GetValue() < 0ll)
	{
		// Reset the action input before starting to move.
		// Calling SetAGInput instead of ResetAGInput since we want to reset
		// the input even if some other system has set it to non-idle
		pPipeUser->GetProxy()->SetAGInput(AIAG_ACTION, "idle");

		// Change the SO state to match the movement.
		IEntity* pEntity = pPipeUser->GetEntity();
		IEntity* pNullEntity = 0;
		if (gAIEnv.pSmartObjectManager->SmartObjectEvent("OnMove", pEntity, pNullEntity) != 0)
			return false; // Trace not finished
	}

	CTimeValue now = GetAISystem()->GetFrameStartTime();

	float ftimeStep = static_cast<float>((m_prevFrameStartTime.GetValue() > 0ll)
	                                     ? (now - m_prevFrameStartTime).GetMilliSecondsAsInt64()
	                                     : 0);
	m_prevFrameStartTime = now;

	bool isUsing3DNavigation = pPipeUser->IsUsing3DNavigation();

	if (bExactPositioning)
	{
		m_passingStraightNavSO = false;
	}
	else
	{
		IPathFollower* pPathFollower = gAIEnv.CVars.PredictivePathFollowing ? pPipeUser->GetPathFollower() : 0;
		float distToSmartObject = pPathFollower ? pPathFollower->GetDistToSmartObject() : pPipeUser->m_Path.GetDistToSmartObject(!isUsing3DNavigation);
		m_passingStraightNavSO = distToSmartObject < 1.f;
	}

	if (bFullUpdate)
	{
		float fDistanceToPathEnd = pPipeUser->m_State.fDistanceToPathEnd;
		const float fExactPosTriggerDistance = 2.5f; //10.0f;

		// Trigger the exact positioning when path is found (sometimes it is still in progress because of previous navSO usage),
		// and when close enough or just finished trace without following it.
		m_bWaitingForBusySmartObject = false;

		if ((fDistanceToPathEnd >= 0.f && fDistanceToPathEnd <= fExactPosTriggerDistance) ||
		    pPipeUser->m_Path.GetPathLength(!pPipeUser->IsUsing3DNavigation()) <= fExactPosTriggerDistance)
		{
			TriggerExactPositioning(pPipeUser, &bForceRegeneratePath, &bExactPositioning);
		}
	}

#ifdef _DEBUG
	ExecuteTraceDebugDraw(pPipeUser);
#endif

	float timeStep = max(0.0f, ftimeStep * 0.001f);
	m_fTotalTracingTime += timeStep;
	m_TimeStep = timeStep;

	// If this path was generated with the pathfinder quietly regenerate the path
	// periodically in case something has moved.
	if (bForceRegeneratePath || (bFullUpdate && !m_inhibitPathRegen && !m_passingStraightNavSO && !m_bWaitingForBusySmartObject &&
	                             pPipeUser->m_movementAbility.pathRegenIntervalDuringTrace > 0.01f &&
	                             pPipeUser->m_movementAbility.pathRegenIntervalDuringTrace < m_fTotalTracingTime &&
	                             !pPipeUser->m_Path.GetParams().precalculatedPath &&
	                             !pPipeUser->m_Path.GetParams().inhibitPathRegeneration))
	{
		if (gAIEnv.CVars.CrowdControlInPathfind != 0 || gAIEnv.CVars.AdjustPathsAroundDynamicObstacles != 0)
		{
			RegeneratePath(pPipeUser, &bForceRegeneratePath);
		}

		// The path was forced to be regenerated when a navigational smartobject has been passed.
		if (bForceRegeneratePath)
		{
			m_bWaitingForPathResult = true;
			return false;   // Trace not finished
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// ExecuteTrace Core
	if (!m_bWaitingForPathResult && !m_bWaitingForBusySmartObject)
	{
		IPathFollower* pPathFollower = gAIEnv.CVars.PredictivePathFollowing ? pPipeUser->GetPathFollower() : 0;
		bTraceFinished = pPathFollower ? ExecutePathFollower(pPipeUser, bFullUpdate, pPathFollower)
		                 : isUsing3DNavigation ? Execute3D(pPipeUser, bFullUpdate) : Execute2D(pPipeUser, bFullUpdate);
	}
	//////////////////////////////////////////////////////////////////////////

	if (bExactPositioning)
	{
		// If exact positioning is in motion, do not allow to finish the goal op, just yet.
		// This may happen in some cases when the exact positioning start position is just over
		// the starting point of a navSO, or during the animation playback to keep the
		// prediction warm.

		bTraceFinished = false;
	}

	// prevent future updates unless we get an external reset
	if (bTraceFinished && pPipeUser->m_nPathDecision != PATHFINDER_STILLFINDING)
	{
		if (bFullUpdate)
		{
			Reset(pPipeUser);
			return true;  // Full update, trace finished
		}
		else
		{
			StopMovement(pPipeUser);
			m_bBlock_ExecuteTrace_untilFullUpdateThenReset = true;
			return false; // Waiting for full update, trace not yet finished at the moment
		}
	}

	return bTraceFinished;
}

//===================================================================
// ExecuteManeuver
//===================================================================
void COPTrace::ExecuteManeuver(CPipeUser* pPipeUser, const Vec3& steerDir)
{
	if (fabs(pPipeUser->m_State.fDesiredSpeed) < 0.001f)
	{
		m_Maneuver = eMV_None;
		return;
	}

#ifdef _DEBUG
	// Update the debug movement reason.
	pPipeUser->m_DEBUGmovementReason = CPipeUser::AIMORE_MANEUVER;
#endif

	// first work out which direction to rotate (normally const over the whole
	// maneuver). Subsequently work out fwd/backwards. Then steer based on
	// the combination of the two

	float cosTrh = pPipeUser->m_movementAbility.maneuverTrh;
	if (pPipeUser->m_movementAbility.maneuverTrh >= 1.f || pPipeUser->m_IsSteering)
		return;

	Vec3 myDir = pPipeUser->GetMoveDir();
	if (pPipeUser->m_State.fMovementUrgency < 0.0f)
		myDir *= -1.0f;
	myDir.z = 0.0f;
	myDir.NormalizeSafe();
	Vec3 reqDir = steerDir;
	reqDir.z = 0.0f;
	reqDir.NormalizeSafe();
	Vec3 myVel = pPipeUser->GetVelocity();
	Vec3 myPos = pPipeUser->GetPhysicsPos();

	float diffCos = reqDir.Dot(myDir);
	// if not maneuvering then require a big angle change to enter it
	if (diffCos > cosTrh && m_Maneuver == eMV_None)
		return;

	CTimeValue now(GetAISystem()->GetFrameStartTime());

	// prevent very small wiggles.
	const int maneuverTimeMinLimitMs = 300;
	const int maneuverTimeMaxLimitMs = 5000;
	const int manTimeMs = (int)(m_Maneuver != eMV_None ? (now - m_ManeuverTime).GetMilliSecondsAsInt64() : 0);

	// if maneuvering only stop when closely lined up
	static float exitDiffCos = 0.98f;
	if (diffCos > exitDiffCos && m_Maneuver != eMV_None && manTimeMs > maneuverTimeMinLimitMs)
	{
		m_Maneuver = eMV_None;
		return;
	}

	// hack for instant turning
	Vec3 camPos = GetISystem()->GetViewCamera().GetPosition();
	Vec3 opPos = pPipeUser->GetPhysicsPos();
	float distSq = Distance::Point_Point(camPos, opPos);
	static float minDistSq = square(5.0f);
	if (distSq > minDistSq)
	{
		bool visible = GetISystem()->GetViewCamera().IsSphereVisible_F(Sphere(opPos, pPipeUser->GetRadius()));
		if (!visible)
		{
			float x = reqDir.Dot(myDir);
			float y = reqDir.Dot(Vec3(-myDir.y, myDir.x, 0.0f));
			// do it in steps to help physics resolve penetrations...
			float rotAngle = 0.02f * atan2f(y, x);
			Quat q;
			q.SetRotationAA(rotAngle, Vec3(0, 0, 1));

			IEntity* pEntity = pPipeUser->GetEntity();
			Quat qCur = pEntity->GetRotation();
			pEntity->SetRotation(q * qCur);
			m_Maneuver = eMV_None;
			return;
		}
	}

	// set the direction
	Vec3 dirCross = myDir.Cross(reqDir);
	m_ManeuverDir = dirCross.z > 0.0f ? eMVD_AntiClockwise : eMVD_Clockwise;

	bool movingFwd = myDir.Dot(myVel) > 0.0f;

	static float maneuverDistLimit = 5;

	// start a new maneuver?
	if (m_Maneuver == eMV_None)
	{
		m_Maneuver = eMV_Back;
		m_ManeuverDist = 0.5f * maneuverDistLimit;
		m_ManeuverTime = GetAISystem()->GetFrameStartTime();
	}
	else
	{
		// reverse direction when we accumulate sufficient distance in the direction
		// we're supposed to be going in
		Vec3 delta = myPos - m_lastPosition;
		float dist = fabs(delta.Dot(myDir));
		if (movingFwd && m_Maneuver == eMV_Back)
			dist = 0.0f;
		else if (!movingFwd && m_Maneuver == eMV_Fwd)
			dist = 0.0f;
		m_ManeuverDist += dist;

		if (manTimeMs > maneuverTimeMaxLimitMs)
		{
			m_Maneuver = m_Maneuver == eMV_Fwd ? eMV_Back : eMV_Fwd;
			m_ManeuverDist = 0.0f;
			m_ManeuverTime = now;
		}
		else if (m_Maneuver == eMV_Back)
		{
			if (fabsf(reqDir.Dot(myDir)) < cosf(DEG2RAD(85.0f)))
			{
				m_Maneuver = eMV_Fwd;
				m_ManeuverDist = 0.0f;
				m_ManeuverTime = now;
			}
		}
		else
		{
			if (fabsf(reqDir.Dot(myDir)) > cosf(DEG2RAD(5.0f)))
			{
				m_Maneuver = eMV_Back;
				m_ManeuverDist = 0.0f;
				m_ManeuverTime = now;
			}
		}

	}

	// now turn these into actual requests
	float normalSpeed, minSpeed, maxSpeed;
	pPipeUser->GetMovementSpeedRange(AISPEED_WALK, false, normalSpeed, minSpeed, maxSpeed);

	pPipeUser->m_State.fDesiredSpeed = minSpeed; //pPipeUser->GetManeuverMovementSpeed();
	if (m_Maneuver == eMV_Back)
		pPipeUser->m_State.fDesiredSpeed = -5.0;

	Vec3 leftDir(-myDir.y, myDir.x, 0.0f);

	if (m_ManeuverDir == eMVD_AntiClockwise)
		pPipeUser->m_State.vMoveDir = leftDir;
	else
		pPipeUser->m_State.vMoveDir = -leftDir;

	if (pPipeUser->m_State.fMovementUrgency < 0.0f)
		pPipeUser->m_State.vMoveDir *= -1.0f;
}

//===================================================================
// ExecutePreamble
//===================================================================
bool COPTrace::ExecutePreamble(CPipeUser* pPipeUser)
{
	CCCPOINT(COPTrace_ExecutePreamble);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_lastPosition.IsZero())
		m_lastPosition = pPipeUser->GetPhysicsPos();

	if (!m_refNavTarget.GetAIObject())
	{
		if (pPipeUser->m_Path.Empty())
		{
			pPipeUser->m_State.fDesiredSpeed = 0.f;
			m_inhibitPathRegen = true;
		}
		else
		{
			// Obtain a NavTarget
			stack_string name("navTarget_");
			name += GetNameSafe(pPipeUser);

			gAIEnv.pAIObjectManager->CreateDummyObject(m_refNavTarget, name, CAIObject::STP_REFPOINT);
			m_refNavTarget.GetAIObject()->SetPos(pPipeUser->GetPhysicsPos());

			m_inhibitPathRegen = false;
		}
	}
	else
	{
		m_inhibitPathRegen = false;
	}

	return m_inhibitPathRegen;
}

//===================================================================
// ExecutePostamble
//===================================================================
bool COPTrace::ExecutePostamble(CPipeUser* pPipeUser, bool& reachedEnd, bool fullUpdate, bool b2D)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// do this after maneuver since that needs to track how far we moved
	Vec3 opPos = pPipeUser->GetPhysicsPos();
	m_fTravelDist += b2D ? Distance::Point_Point2D(opPos, m_lastPosition) : Distance::Point_Point(opPos, m_lastPosition);
	m_lastPosition = opPos;

	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

	// only consider trace to be done once the agent has stopped.
	if (reachedEnd && m_fEndAccuracy >= 0.f)
	{
		Vec3 vel = pPipeUser->GetVelocity();
		vel.z = 0.f;
		float speed = vel.Dot(pipeUserState.vMoveDir);
		const float criticalSpeed = 0.01f;
		if (speed > criticalSpeed)
		{

			if (gAIEnv.CVars.DebugPathFinding)
				AILogAlways("COPTrace reached end but waiting for speed %5.2f to fall below %5.2f %s",
				            speed, criticalSpeed, GetNameSafe(pPipeUser));

			reachedEnd = false;
			pipeUserState.fDesiredSpeed = 0.f;
			m_inhibitPathRegen = true;
		}
	}

	// only consider trace to be done once the agent has stopped.
	if (reachedEnd)
	{
		pipeUserState.fDesiredSpeed = 0.f;
		m_inhibitPathRegen = true;
		return true;  // Trace finished
	}

	// code below here checks/handles dynamic objects
	if (pPipeUser->m_Path.GetParams().precalculatedPath)
		return false;

	return false;
}

//===================================================================
// ExecutePathFollower
//===================================================================
bool COPTrace::ExecutePathFollower(CPipeUser* pPipeUser, bool fullUpdate, IPathFollower* pPathFollower)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	AIAssert(pPathFollower);
	if (!pPathFollower)
		return true;

	if (m_TimeStep <= 0.f)
		return false;

	if (ExecutePreamble(pPipeUser))
	{
		// Path empty, nav target nil, path regeneration inhibited, trace finished [6/5/2010 evgeny]
		return true;
	}

	CCCPOINT(COPTrace_ExecutePathFollower);

	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

	float fNormalSpeed, fMinSpeed, fMaxSpeed;
	pPipeUser->GetMovementSpeedRange(pipeUserState.fMovementUrgency, pipeUserState.allowStrafing,
	                                 fNormalSpeed, fMinSpeed, fMaxSpeed);

	// Update the path follower - really these things shouldn't change
	// TODO: Leaving const_cast here it indicate how awful this is!
	PathFollowerParams& params = const_cast<PathFollowerParams&>(pPathFollower->GetParams());
	params.minSpeed = fMinSpeed;
	params.maxSpeed = fMaxSpeed;
	params.normalSpeed = clamp_tpl(fNormalSpeed, params.minSpeed, params.maxSpeed);

	params.endDistance = 0.f;

	bool bContinueMovingAtEnd = pPipeUser->m_Path.GetParams().continueMovingAtEnd;

	// [Mikko] Slight hack to make the formation following better.
	// When following formation, the agent is often very close to the formation.
	// This code forces the actor to lag a bit behind, which helps to follow the formation much more smoothly.
	// TODO: Revive approach-at-distance without cutting the path and add that as extra parameter for stick.
	if (bContinueMovingAtEnd && m_pendingActorTargetRequester == eTATR_None)
	{
		CAIObject* pPathFindTarget = pPipeUser->m_refPathFindTarget.GetAIObject();
		if (pPathFindTarget && (pPathFindTarget->GetSubType() == IAIObject::STP_FORMATION))
		{
			params.endDistance = 1.f;
		}
	}

	params.maxAccel = pPipeUser->m_movementAbility.maxAccel;
	params.maxDecel = pPipeUser->m_movementAbility.maxDecel;
	params.stopAtEnd = !bContinueMovingAtEnd;
	params.isAllowedToShortcut = true;

	PathFollowResult result;

	PathFollowResult::TPredictedStates& predictedStates = s_tmpPredictedStates;

	// Lower the prediction calculations for puppets which have lo update priority (means invisible or distant).
	bool highPriority;
	if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
	{
		EPuppetUpdatePriority ePuppetUpdatePriority = pPuppet->GetUpdatePriority();
		highPriority = (ePuppetUpdatePriority == AIPUP_VERY_HIGH) || (ePuppetUpdatePriority == AIPUP_HIGH);
	}
	else
	{
		highPriority = true;
	}

	const float PREDICTION_DELTA_TIME = 0.1f;
	const float PREDICTION_TIME = 1.f;

	if (highPriority)
	{
		result.desiredPredictionTime = PREDICTION_TIME;
		predictedStates.resize(size_t(PREDICTION_TIME / PREDICTION_DELTA_TIME + 0.5f));
		result.predictedStates = &predictedStates;
	}
	else
	{
		result.desiredPredictionTime = 0.f;
		predictedStates.clear();
		result.predictedStates = 0;
	}

	result.predictionDeltaTime = PREDICTION_DELTA_TIME;

	Vec3 curPos = pPipeUser->GetPhysicsPos();

	// If there's an animation in progress (typically SO playing)
	bool runningSO = false;
	if (m_actorTargetRequester == eTATR_NavSO)
	{
		m_stuckDetector.Reset();

		switch (pipeUserState.curActorTargetPhase)
		{
		case eATP_Playing:
		case eATP_Finished:
		case eATP_StartedAndFinished:
			// Path follow from the starting position of the path
			assert(!pipeUserState.curActorTargetFinishPos.IsZero());
			curPos = pipeUserState.curActorTargetFinishPos;
			runningSO = true;
			//pPipeUser->m_Path.GetPosAlongPath(curPos, 0.0f);
		}
	}

	const Vec3 curVel = pPipeUser->GetVelocity();

	const bool targetReachable = pPathFollower->Update(result, curPos, curVel, m_TimeStep);

	const float distToEnd = pPathFollower->GetDistToEnd(&curPos);
	//  if (m_stuckDetector.Update(distToEnd) == StuckDetector::UserIsStuck)
	//  {
	//    const CNavPath& path = pPipeUser->m_Path;
	//
	//    if (!path.Empty())
	//    {
	//      const Vec3& endPos = path.GetLastPathPos();
	//
	//      const bool playerIsLooking =
	//        GetAISystem()->WouldHumanBeVisible(curPos, false) ||
	//        GetAISystem()->WouldHumanBeVisible(endPos, false);
	//
	//      if (playerIsLooking)
	//        StopMovement(pPipeUser);
	//      else
	//        Teleport(*pPipeUser, endPos);
	//
	//      return StillTracing;
	//    }
	//  }

	if (targetReachable)
	{
		Vec3 desiredMoveDir = result.velocityOut;
		float desiredSpeed = desiredMoveDir.NormalizeSafe();

		pipeUserState.fDesiredSpeed = desiredSpeed;
		pipeUserState.vMoveDir = desiredMoveDir;
		pipeUserState.fDistanceToPathEnd = distToEnd;

		int num = min<int>(predictedStates.size(), SAIPredictedCharacterStates::maxStates);
		pipeUserState.predictedCharacterStates.nStates = num;
		for (int i = 0; i < num; ++i)
		{
			const PathFollowResult::SPredictedState& state = predictedStates[i];
			pipeUserState.predictedCharacterStates.states[i].Set(state.pos, state.vel, (1 + i) * PREDICTION_DELTA_TIME);
		}

		// Precision movement support: A replacement for the old velocity based movement system above.
		// NOTE: If the move target and inflection point are equal (and non-zero) they mark the end of the path.
		pipeUserState.vMoveTarget = result.followTargetPos;
		pipeUserState.vInflectionPoint = result.inflectionPoint;

		return ExecutePostamble(pPipeUser, result.reachedEnd, fullUpdate, params.use2D);
	}
	else
	{
		if (!runningSO) // do not regenerate path while running SO
		{
			m_accumulatedFailureTime += gEnv->pTimer->GetFrameTime();

			if (m_accumulatedFailureTime > 0.5f)
			{
				m_accumulatedFailureTime = 0.0f;

				bool forceRegeneratePath = true;
				RegeneratePath(pPipeUser, &forceRegeneratePath);
				m_bWaitingForPathResult = true;
			}
		}

		return StillTracing;
	}
}

//====================================================================
// Execute2D
//====================================================================
bool COPTrace::Execute2D(CPipeUser* const pPipeUser, bool fullUpdate)
{
	if (ExecutePreamble(pPipeUser))
		return true;

	// input
	Vec3 fwdDir = pPipeUser->GetMoveDir();
	if (pPipeUser->m_State.fMovementUrgency < 0.0f)
		fwdDir *= -1.0f;
	Vec3 opPos = pPipeUser->GetPhysicsPos();
	pe_status_dynamics dSt;
	pPipeUser->GetPhysics()->GetStatus(&dSt);
	Vec3 opVel = m_Maneuver == eMV_None ? dSt.v : fwdDir * 5.0f;
	float lookAhead = pPipeUser->m_movementAbility.pathLookAhead;
	float pathRadius = pPipeUser->m_movementAbility.pathRadius;
	bool resolveSticking = pPipeUser->m_movementAbility.resolveStickingInTrace;

	// output
	Vec3 steerDir(ZERO);
	float distToEnd = 0.0f;
	float distToPath = 0.0f;
	Vec3 pathDir(ZERO);
	Vec3 pathAheadDir(ZERO);
	Vec3 pathAheadPos(ZERO);

	bool isResolvingSticking = false;

	bool stillTracingPath = pPipeUser->m_Path.UpdateAndSteerAlongPath(
	  steerDir, distToEnd, distToPath, isResolvingSticking,
	  pathDir, pathAheadDir, pathAheadPos,
	  opPos, opVel, lookAhead, pathRadius, m_TimeStep, resolveSticking, true);
	pPipeUser->m_State.fDistanceToPathEnd = max(0.0f, distToEnd);
	pPipeUser->m_Path.GetDirectionToPathFromPoint(opPos, pPipeUser->m_State.vDirOffPath);

	pathAheadDir.z = 0.0f;
	pathAheadDir.NormalizeSafe();
	Vec3 steerDir2D(steerDir);
	steerDir2D.z = 0.0f;
	steerDir2D.NormalizeSafe();

#ifdef _DEBUG
	// Update the debug movement reason.
	pPipeUser->m_DEBUGmovementReason = CPipeUser::AIMORE_TRACE;
#endif

	distToEnd -= /*m_fEndDistance*/ -pPipeUser->m_Path.GetDiscardedPathLength();
	bool reachedEnd = false;
	if (stillTracingPath && distToEnd > .1f)
	{
		Vec3 targetPos;
		if (m_refNavTarget && pPipeUser->m_Path.GetPosAlongPath(targetPos, lookAhead, true, true))
			m_refNavTarget->SetPos(targetPos);

		//turning maneuvering
		//FIXME: Investigate if this is necessary anymore with new smart path follower
		const bool doManeuver = (gAIEnv.configuration.eCompatibilityMode != ECCM_CRYSIS2);
		if (doManeuver)
			ExecuteManeuver(pPipeUser, steerDir);

		if (m_Maneuver != eMV_None)
		{
			Vec3 curPos = pPipeUser->GetPhysicsPos();
			m_fTravelDist += Distance::Point_Point2D(curPos, m_lastPosition);
			m_lastPosition = curPos;
			// prevent path regen
			m_fTotalTracingTime = 0.0f;
			return false;
		}

		//    float normalSpeed = pPipeUser->GetNormalMovementSpeed(pPipeUser->m_State.fMovementUrgency, true);
		//    float slowSpeed = pPipeUser->GetManeuverMovementSpeed();
		//    if (slowSpeed > normalSpeed) slowSpeed = normalSpeed;
		float normalSpeed, minSpeed, maxSpeed;
		pPipeUser->GetMovementSpeedRange(pPipeUser->m_State.fMovementUrgency, pPipeUser->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		// These will be adjusted to the range 0-1 to select a speed between slow and normal
		float dirSpeedMod = 1.0f;
		float curveSpeedMod = 1.0f;
		float endSpeedMod = 1.0f;
		float slopeMod = 1.0f;
		float moveDirMod = 1.0f;

		// speed falloff - slow down if current direction is too different from desired
		if (pPipeUser->GetType() == AIOBJECT_VEHICLE)
		{
			static float offset = 1.0f; // if this is > 1 then there is a "window" where no slow-down occurs
			float velFalloff = offset * pathAheadDir.Dot(fwdDir);
			float velFalloffD = 1 - velFalloff;
			if (velFalloffD > 0.0f && pPipeUser->m_movementAbility.velDecay > 0.0f)
				dirSpeedMod = velFalloff / (velFalloffD * pPipeUser->m_movementAbility.velDecay);
		}

		// slow down due to the path curvature
		float lookAheadForSpeedControl;
		if (pPipeUser->m_movementAbility.pathSpeedLookAheadPerSpeed < 0.0f)
			lookAheadForSpeedControl = -pPipeUser->m_movementAbility.pathSpeedLookAheadPerSpeed * lookAhead * pPipeUser->m_State.fMovementUrgency;
		else
			lookAheadForSpeedControl = pPipeUser->m_movementAbility.pathSpeedLookAheadPerSpeed * pPipeUser->GetVelocity().GetLength();

		if (lookAheadForSpeedControl > 0.0f)
		{
			Vec3 pos, dir;
			float lowestPathDot = 0.0f;
			bool curveOK = pPipeUser->m_Path.GetPathPropertiesAhead(lookAheadForSpeedControl, true, pos, dir, 0, lowestPathDot, true);
			Vec3 thisPathSegDir = (pPipeUser->m_Path.GetNextPathPoint()->vPos - pPipeUser->m_Path.GetPrevPathPoint()->vPos);
			thisPathSegDir.z = 0.0f;
			thisPathSegDir.NormalizeSafe();
			float thisDot = thisPathSegDir.Dot(steerDir2D);
			if (thisDot < lowestPathDot)
				lowestPathDot = thisDot;
			if (curveOK)
			{
				float a = 1.0f - 2.0f * pPipeUser->m_movementAbility.cornerSlowDown; // decrease this to make the speed drop off quicker with angle
				float b = 1.0f - a;
				curveSpeedMod = a + b * lowestPathDot;
			}

			// slow down at end
			if (m_fEndAccuracy >= 0.0f && m_eTraceEndMode != eTEM_MinimumDistance)
			{
				static float slowDownDistScale = 2.0f;
				static float minEndSpeedMod = 0.1f;
				float slowDownDist = slowDownDistScale * lookAheadForSpeedControl;
				float workingDistToEnd = m_fEndAccuracy + distToEnd - 0.2f * lookAheadForSpeedControl;
				if (slowDownDist > 0.1f && workingDistToEnd < slowDownDist)
				{
					endSpeedMod = workingDistToEnd / slowDownDist;
					Limit(endSpeedMod, minEndSpeedMod, 1.0f);
				}
			}
		}

		float slopeModCoeff = pPipeUser->m_movementAbility.slopeSlowDown;
		// slow down when going down steep slopes (especially stairs!)
		int buildingID = -1;
		if ((slopeModCoeff > 0) &&
		    (gAIEnv.pNavigation->CheckNavigationType(pPipeUser->GetPos(), buildingID,
		                                             pPipeUser->GetMovementAbility().pathfindingProperties.navCapMask) == IAISystem::NAV_WAYPOINT_HUMAN))
		{
			static float slowDownSlope = 0.5f;
			float pathHorDist = steerDir.GetLength2D();
			if (pathHorDist > 0.0f && steerDir.z < 0.0f)
			{
				float slope = -steerDir.z / pathHorDist * slopeModCoeff;
				slopeMod = 1.0f - slope / slowDownSlope;
				static float minSlopeMod = 0.5f;
				Limit(slopeMod, minSlopeMod, 1.0f);
			}
		}

		// slow down when going up steep slopes
		if (slopeModCoeff > 0)
		{
			IPhysicalEntity* pPhysics = pPipeUser->GetPhysics();
			pe_status_living status;
			int valid = pPhysics->GetStatus(&status);
			if (valid)
			{
				if (!status.bFlying)
				{
					Vec3 sideDir(-steerDir2D.y, steerDir2D.x, 0.0f);
					Vec3 slopeN = status.groundSlope - status.groundSlope.Dot(sideDir) * sideDir;
					slopeN.NormalizeSafe();
					// d is -ve for uphill
					float d = steerDir2D.Dot(status.groundSlope);
					Limit(d, -0.99f, 0.99f);
					// map d=-1 -> -inf, d=0 -> 1, d=1 -> inf
					float uphillSlopeMod = (1 + d / (1.0f - square(d))) * slopeModCoeff;
					static float minUphillSlopeMod = 0.5f;
					if (uphillSlopeMod < minUphillSlopeMod)
						uphillSlopeMod = minUphillSlopeMod;
					if (uphillSlopeMod < 1.0f)
						slopeMod = min(slopeMod, uphillSlopeMod);
				}
			}
		}

		float maxMod = min(min(min(min(dirSpeedMod, curveSpeedMod), endSpeedMod), slopeMod), moveDirMod);
		Limit(maxMod, 0.0f, 1.0f);
		//    maxMod = pow(maxMod, 2.0f);

		if (m_bControlSpeed == true)
		{
			float newDesiredSpeed = (1.0f - maxMod) * minSpeed + maxMod * normalSpeed;

			float change = newDesiredSpeed - pPipeUser->m_State.fDesiredSpeed;
			if (change > m_TimeStep * pPipeUser->m_movementAbility.maxAccel)
				change = m_TimeStep * pPipeUser->m_movementAbility.maxAccel;
			else if (change < -m_TimeStep * pPipeUser->m_movementAbility.maxDecel)
				change = -m_TimeStep * pPipeUser->m_movementAbility.maxDecel;
			pPipeUser->m_State.fDesiredSpeed += change;
		}

		pPipeUser->m_State.vMoveDir = steerDir2D;
		if (pPipeUser->m_State.fMovementUrgency < 0.0f)
			pPipeUser->m_State.vMoveDir *= -1.0f;

		// prediction
		//    static bool doPrediction = true;
		pPipeUser->m_State.predictedCharacterStates.nStates = 0;
	}
	else
	{
		reachedEnd = true;
	}

	return ExecutePostamble(pPipeUser, reachedEnd, fullUpdate, true);
}

//====================================================================
// Execute3D
//====================================================================
bool COPTrace::Execute3D(CPipeUser* pPipeUser, bool fullUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (ExecutePreamble(pPipeUser))
		return true;

	// Ideally do this check less... but beware we might regen the path and the end might change a bit(?)
	if (fullUpdate)
	{
		if (pPipeUser->GetType() == AIOBJECT_VEHICLE && m_fEndAccuracy == 0.0f && !pPipeUser->m_Path.GetPath().empty())
		{
			Vec3 endPt = pPipeUser->m_Path.GetPath().back().vPos;
			// ideally we would use AICE_ALL here, but that can result in intersection with the heli when it
			// gets to the end of the path...
			bool gotFloor = GetFloorPos(m_landingPos, endPt, 0.5f, 1.0f, 1.0f, AICE_STATIC);
			if (gotFloor)
				m_landHeight = 2.0f;
			else
				m_landHeight = 0.0f;
			if (m_workingLandingHeightOffset > 0.0f)
				m_inhibitPathRegen = true;
		}
		else
		{
			m_landHeight = 0.0f;
			m_inhibitPathRegen = false;
		}
	}

	// input
	Vec3 opPos = pPipeUser->GetPhysicsPos();
	Vec3 fakeOpPos = opPos;
	fakeOpPos.z -= m_workingLandingHeightOffset;
	if (pPipeUser->m_IsSteering)
		fakeOpPos.z -= pPipeUser->m_flightSteeringZOffset;

	pe_status_dynamics dSt;
	pPipeUser->GetPhysics()->GetStatus(&dSt);
	Vec3 opVel = dSt.v;
	float lookAhead = pPipeUser->m_movementAbility.pathLookAhead;
	float pathRadius = pPipeUser->m_movementAbility.pathRadius;
	bool resolveSticking = pPipeUser->m_movementAbility.resolveStickingInTrace;

	// output
	Vec3 steerDir(ZERO);
	float distToEnd = 0.0f;
	float distToPath = 0.0f;
	Vec3 pathDir(ZERO);
	Vec3 pathAheadDir(ZERO);
	Vec3 pathAheadPos(ZERO);

	bool isResolvingSticking = false;

	bool stillTracingPath = pPipeUser->m_Path.UpdateAndSteerAlongPath(steerDir, distToEnd, distToPath, isResolvingSticking,
	                                                                  pathDir, pathAheadDir, pathAheadPos,
	                                                                  fakeOpPos, opVel, lookAhead, pathRadius, m_TimeStep, resolveSticking, false);
	pPipeUser->m_State.fDistanceToPathEnd = max(0.0f, distToEnd);

#ifdef _DEBUG
	// Update the debug movement reason.
	pPipeUser->m_DEBUGmovementReason = CPipeUser::AIMORE_TRACE;
#endif

	distToEnd -= distToPath;
	distToEnd -= m_landHeight * 2.0f;
	if (distToEnd < 0.0f)
		stillTracingPath = false;

	if (!stillTracingPath && m_landHeight > 0.0f)
	{
		return ExecuteLanding(pPipeUser, m_landingPos);
	}

	distToEnd -= /*m_fEndDistance*/ -pPipeUser->m_Path.GetDiscardedPathLength();
	bool reachedEnd = !stillTracingPath;
	if (stillTracingPath && distToEnd > 0.0f)
	{
		Vec3 targetPos;
		if (m_refNavTarget && pPipeUser->m_Path.GetPosAlongPath(targetPos, lookAhead, true, true))
			m_refNavTarget->SetPos(targetPos);

		//    float normalSpeed = pPipeUser->GetNormalMovementSpeed(pPipeUser->m_State.fMovementUrgency, true);
		//    float slowSpeed = pPipeUser->GetManeuverMovementSpeed();
		//    if (slowSpeed > normalSpeed) slowSpeed = normalSpeed;
		float normalSpeed, minSpeed, maxSpeed;
		pPipeUser->GetMovementSpeedRange(pPipeUser->m_State.fMovementUrgency, pPipeUser->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		// These will be adjusted to the range 0-1 to select a speed between slow and normal
		float dirSpeedMod = 1.0f;
		float curveSpeedMod = 1.0f;
		float endSpeedMod = 1.0f;
		float moveDirMod = 1.0f;

		// slow down due to the path curvature
		float lookAheadForSpeedControl;
		if (pPipeUser->m_movementAbility.pathSpeedLookAheadPerSpeed < 0.0f)
			lookAheadForSpeedControl = lookAhead * pPipeUser->m_State.fMovementUrgency;
		else
			lookAheadForSpeedControl = pPipeUser->m_movementAbility.pathSpeedLookAheadPerSpeed * pPipeUser->GetVelocity().GetLength();

		lookAheadForSpeedControl -= distToPath;
		if (lookAheadForSpeedControl < 0.0f)
			lookAheadForSpeedControl = 0.0f;

		if (lookAheadForSpeedControl > 0.0f)
		{
			Vec3 pos, dir;
			float lowestPathDot = 0.0f;
			bool curveOK = pPipeUser->m_Path.GetPathPropertiesAhead(lookAheadForSpeedControl, true, pos, dir, 0, lowestPathDot, true);
			Vec3 thisPathSegDir = (pPipeUser->m_Path.GetNextPathPoint()->vPos - pPipeUser->m_Path.GetPrevPathPoint()->vPos).GetNormalizedSafe();
			float thisDot = thisPathSegDir.Dot(steerDir);
			if (thisDot < lowestPathDot)
				lowestPathDot = thisDot;
			if (curveOK)
			{
				float a = 1.0f - 2.0f * pPipeUser->m_movementAbility.cornerSlowDown; // decrease this to make the speed drop off quicker with angle
				float b = 1.0f - a;
				curveSpeedMod = a + b * lowestPathDot;
			}
		}

		// slow down at end
		if (m_fEndAccuracy >= 0.0f)
		{
			static float slowDownDistScale = 1.0f;
			float slowDownDist = slowDownDistScale * lookAheadForSpeedControl;
			float workingDistToEnd = m_fEndAccuracy + distToEnd - 0.2f * lookAheadForSpeedControl;
			if (slowDownDist > 0.1f && workingDistToEnd < slowDownDist)
			{
				// slow speeds are for manouevering - here we want something that will actually be almost stationary
				minSpeed *= 0.1f;
				endSpeedMod = workingDistToEnd / slowDownDist;
				Limit(endSpeedMod, 0.0f, 1.0f);
				m_workingLandingHeightOffset = (1.0f - endSpeedMod) * m_landHeight;
			}
			else
			{
				m_workingLandingHeightOffset = 0.0f;
			}
		}

		float maxMod = min(min(min(dirSpeedMod, curveSpeedMod), endSpeedMod), moveDirMod);
		Limit(maxMod, 0.0f, 1.0f);

		float newDesiredSpeed = (1.0f - maxMod) * minSpeed + maxMod * normalSpeed;
		float change = newDesiredSpeed - pPipeUser->m_State.fDesiredSpeed;
		if (change > m_TimeStep * pPipeUser->m_movementAbility.maxAccel)
			change = m_TimeStep * pPipeUser->m_movementAbility.maxAccel;
		else if (change < -m_TimeStep * pPipeUser->m_movementAbility.maxDecel)
			change = -m_TimeStep * pPipeUser->m_movementAbility.maxDecel;
		pPipeUser->m_State.fDesiredSpeed += change;

		pPipeUser->m_State.vMoveDir = steerDir;
		if (pPipeUser->m_State.fMovementUrgency < 0.0f)
			pPipeUser->m_State.vMoveDir *= -1.0f;

		// prediction
		//    static bool doPrediction = true;
		pPipeUser->m_State.predictedCharacterStates.nStates = 0;
	}
	else
	{
		reachedEnd = true;
	}

	return ExecutePostamble(pPipeUser, reachedEnd, fullUpdate, false);
}

//===================================================================
// ExecuteLanding
//===================================================================
bool COPTrace::ExecuteLanding(CPipeUser* pPipeUser, const Vec3& pathEnd)
{
	m_inhibitPathRegen = true;
	float normalSpeed, minSpeed, maxSpeed;
	pPipeUser->GetMovementSpeedRange(pPipeUser->m_State.fMovementUrgency, false, normalSpeed, minSpeed, maxSpeed);
	//  float slowSpeed = pPipeUser->GetManeuverMovementSpeed();
	Vec3 opPos = pPipeUser->GetPhysicsPos();

	Vec3 horMoveDir = pathEnd - opPos;
	horMoveDir.z = 0.0f;
	float error = horMoveDir.NormalizeSafe();

	Limit(error, 0.0f, 1.0f);
	float horSpeed = 0.3f * minSpeed * error;
	float verSpeed = 1.0f;

	pPipeUser->m_State.vMoveDir = horSpeed * horMoveDir - Vec3(0, 0, verSpeed);
	pPipeUser->m_State.vMoveDir.NormalizeSafe();
	pPipeUser->m_State.fDesiredSpeed = sqrtf(square(horSpeed) + square(verSpeed));

	if (pPipeUser->m_State.fMovementUrgency < 0.0f)
		pPipeUser->m_State.vMoveDir *= -1.0f;

	// set look dir
	if (m_landingDir.IsZero())
	{

		if (gAIEnv.CVars.DebugPathFinding)
			AILogAlways("COPTrace::ExecuteLanding starting final landing %s", GetNameSafe(pPipeUser));

		m_landingDir = pPipeUser->GetMoveDir();
		m_landingDir.z = 0.0f;
		m_landingDir.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
	}
	Vec3 navTargetPos = opPos + 100.0f * m_landingDir;
	m_refNavTarget->SetPos(navTargetPos);

	if (!pPipeUser->m_bLooseAttention)
	{
		m_looseAttentionId = pPipeUser->SetLooseAttentionTarget(m_refNavTarget);
	}

	//
	pe_status_collisions stat;
	stat.pHistory = 0;
	int collisions = pPipeUser->GetPhysics()->GetStatus(&stat);
	if (collisions > 0)
		return true;
	else
		return false;
}

//===================================================================
// DebugDraw
//===================================================================
void COPTrace::DebugDraw(CPipeUser* pPipeUser) const
{
	if (IsPathRegenerationInhibited())
	{
		CDebugDrawContext dc;
		dc->Draw3dLabel(pPipeUser->GetPhysicsPos(), 1.5f, "PATH LOCKED\n%s %s",
		                m_inhibitPathRegen ? "Inhibit" : "", m_passingStraightNavSO ? "NavSO" : "");
	}
}

void COPTrace::ExecuteTraceDebugDraw(CPipeUser* pPipeUser)
{
	if (gAIEnv.CVars.DebugPathFinding)
	{
		if (IAIDebugRenderer* pRenderer = gAIEnv.GetDebugRenderer())
		{
			if (m_bWaitingForPathResult)
			{
				pRenderer->DrawSphere(pPipeUser->GetPos() + Vec3(0, 0, 1.f), .5f, ColorB(255, 255, 0));
			}

			if (m_bWaitingForBusySmartObject)
			{
				pRenderer->DrawSphere(pPipeUser->GetPos() + Vec3(0, 0, 1.2f), .5f, ColorB(255, 0, 0));
			}
		}
	}
}

bool COPTrace::HandleAnimationPhase(CPipeUser* pPipeUser, bool bFullUpdate, bool* pbForceRegeneratePath, bool* pbExactPositioning, bool* pbTraceFinished)
{
	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

	switch (pipeUserState.curActorTargetPhase)
	{
	case eATP_Error:
		{
			if (m_actorTargetRequester == eTATR_None)
			{
				m_actorTargetRequester = m_pendingActorTargetRequester;
				m_pendingActorTargetRequester = eTATR_None;
			}

			switch (m_actorTargetRequester)
			{
			case eTATR_EndOfPath:

				if (gAIEnv.CVars.DebugPathFinding)
					AILogAlways("COPTrace::ExecuteTrace resetting since error occurred during exact positioning %s", GetNameSafe(pPipeUser));

				if (bFullUpdate)
				{
					Reset(pPipeUser);
				}
				else
				{
					// TODO: Handle better the error case!
					StopMovement(pPipeUser);
					m_bBlock_ExecuteTrace_untilFullUpdateThenReset = true;
				}
				*pbTraceFinished = true;
				return false;

			case eTATR_NavSO:
				// Exact positioning has been failed at navigation smart object, regenerate path.
				*pbForceRegeneratePath = true;
				m_inhibitPathRegen = false;
				break;
			}

			m_actorTargetRequester = eTATR_None;
			m_pendingActorTargetRequester = eTATR_None;
		}
		// END eATP_Error
		break;

	case eATP_Waiting:
		*pbExactPositioning = true;
		break;

	case eATP_Playing:
		// While playing, keep the trace & prediction running.
		*pbExactPositioning = true;

		// Resurrect any remaining path section while we're playing the animation.
		// This allows path following to continue based on the next path's start position.
		pPipeUser->m_Path.ResurrectRemainingPath();
		break;

	case eATP_Starting:
	case eATP_Started:
		*pbExactPositioning = true;

		if (m_pendingActorTargetRequester != eTATR_None)
		{
			m_actorTargetRequester = m_pendingActorTargetRequester;
			m_pendingActorTargetRequester = eTATR_None;
		}

		if (m_stopOnAnimationStart && m_actorTargetRequester == eTATR_EndOfPath)
		{
			if (bFullUpdate)
			{
				Reset(pPipeUser);
			}
			else
			{
				StopMovement(pPipeUser);
				m_bBlock_ExecuteTrace_untilFullUpdateThenReset = true;
			}
			*pbTraceFinished = true;
			return false;
		}
		break;

	case eATP_Finished:
	case eATP_StartedAndFinished:
		switch (m_actorTargetRequester)
		{
		case eTATR_EndOfPath:
			m_actorTargetRequester = eTATR_None;
			// Exact positioning has been finished at the end of the path, the trace is completed.

			if (gAIEnv.CVars.DebugPathFinding)
				AILogAlways("COPTrace::ExecuteTrace resetting since exact position reached/animation finished %s", GetNameSafe(pPipeUser));

			if (bFullUpdate)
			{
				Reset(pPipeUser);
			}
			else
			{
				StopMovement(pPipeUser);
				m_bBlock_ExecuteTrace_untilFullUpdateThenReset = true;
			}
			*pbTraceFinished = true;
			return false;

		case eTATR_NavSO:
			{
				// Exact positioning and animation has been finished at navigation smart object, resurrect path.
				pPipeUser->m_State.fDistanceToPathEnd = pPipeUser->m_Path.GetDiscardedPathLength();
				pPipeUser->m_Path.ResurrectRemainingPath();
				pPipeUser->m_Path.PrepareNavigationalSmartObjectsForMNM(pPipeUser->GetEntity());
				pPipeUser->AdjustPath();
				*pbForceRegeneratePath = false;

				m_actorTargetRequester = eTATR_None;
				m_prevFrameStartTime.SetValue(0ll);

				// Update distance traveled during the exact-pos anim.
				Vec3 opPos = pPipeUser->GetPhysicsPos();
				m_fTravelDist += !pPipeUser->IsUsing3DNavigation() ?
				                 Distance::Point_Point2D(opPos, m_lastPosition) : Distance::Point_Point(opPos, m_lastPosition);
				m_lastPosition = opPos;

				m_bWaitingForPathResult = false;
				m_inhibitPathRegen = true;
			}
			break;

		default:
			// A pending exact positioning request, maybe from previously interrupted trace.
			// Regenerate path, since the current path may be bogus because it was generated
			// before the animation had finished.
			*pbForceRegeneratePath = true;
			m_actorTargetRequester = eTATR_None;
			m_bWaitingForPathResult = true;
			m_bWaitingForBusySmartObject = false;
			m_inhibitPathRegen = false;
		}
		// END eATP_Finished & eATP_StartedAndFinished:
		break;
	}

	return true;
}

bool COPTrace::HandlePathResult(CPipeUser* pPipeUser, bool* pbReturnValue)
{
	switch (pPipeUser->m_nPathDecision)
	{
	case PATHFINDER_PATHFOUND:
		// Path found, continue.
		m_bWaitingForPathResult = false;
		return true;

	case PATHFINDER_NOPATH:
		// Could not find path, fail.
		m_bWaitingForPathResult = false;
		m_bBlock_ExecuteTrace_untilFullUpdateThenReset = true;
		*pbReturnValue = true;
		return false;

	default:
		// Wait for the path finder result, disable movement and wait.
		StopMovement(pPipeUser);
		*pbReturnValue = false;
		return false;
	}
}

bool COPTrace::IsVehicleAndDriverIsFallen(CPipeUser* pPipeUser)
{
	if (pPipeUser->GetType() == AIOBJECT_VEHICLE)
	{
		// Check if the vehicle driver is tranq'd and do not move if it is.
		if (EntityId driverId = pPipeUser->GetProxy()->GetLinkedDriverEntityId())
		{
			if (IEntity* pDriverEntity = gEnv->pEntitySystem->GetEntity(driverId))
			{
				if (IAIObject* pDriverAI = pDriverEntity->GetAI())
				{
					if (CAIActor* pDriverActor = pDriverAI->CastToCAIActor())
					{
						if (IAIActorProxy* pDriverProxy = pDriverActor->GetProxy())
						{
							if (pDriverProxy->GetActorIsFallen())
							{
								// The driver is unable to drive, do not drive.
								StopMovement(pPipeUser);
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void COPTrace::RegeneratePath(CPipeUser* pPipeUser, bool* pbForceRegeneratePath)
{
	gEnv->pAISystem->LogComment("COPTrace::RegeneratePath", "Currently regenerate the path in the GoalOp trace is not supported by the MNM Navigation System.");
}

void COPTrace::StopMovement(CPipeUser* pPipeUser)
{
	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

	pipeUserState.vMoveDir.zero();
	pipeUserState.fDesiredSpeed = 0.f;
	pipeUserState.predictedCharacterStates.nStates = 0;
}

void COPTrace::TriggerExactPositioning(CPipeUser* pPipeUser, bool* pbForceRegeneratePath, bool* pbExactPositioning)
{
	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;
	SAIActorTargetRequest& pipeUserActorTargetRequest = pipeUserState.actorTargetReq;

	switch (pipeUserState.curActorTargetPhase)
	{
	case eATP_None:
		{
			if (gAIEnv.CVars.DebugPathFinding)
			{
				SNavSOStates& pipeUserPendingNavSOStates = pPipeUser->m_pendingNavSOStates;
				if (!pipeUserPendingNavSOStates.IsEmpty())
				{
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pipeUserPendingNavSOStates.objectEntId);
					if (pEntity)
					{
						AILogAlways("COPTrace::ExecuteTrace %s trying to use exact positioning while a navSO (entity=%s) is still active.",
						            GetNameSafe(pPipeUser), pEntity->GetName());
					}
					else
					{
						AILogAlways("COPTrace::ExecuteTrace %s trying to use exact positioning while a navSO (entityId=%d) is still active.",
						            GetNameSafe(pPipeUser), pipeUserPendingNavSOStates.objectEntId);
					}
				}
			}

			// Handle the exact positioning request
			const PathPointDescriptor::OffMeshLinkData* pSmartObjectMNMData = pPipeUser->m_Path.GetLastPathPointMNNSOData();
			const bool smartObject = (pSmartObjectMNMData && pSmartObjectMNMData->offMeshLinkID != MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID);

			if (smartObject)
			{
				MNM::OffMeshLink* pOffMeshLink = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager()->GetOffMeshLink(pSmartObjectMNMData->offMeshLinkID);
				OffMeshLink_SmartObject* pSOLink = pOffMeshLink ? pOffMeshLink->CastTo<OffMeshLink_SmartObject>() : NULL;

				assert(pSOLink);
				if (pSOLink)
				{
					CSmartObjectManager* pSmartObjectManager = gAIEnv.pSmartObjectManager;
					CSmartObject* pSmartObject = pSOLink->m_pSmartObject;

					const EntityId smartObjectId = pSmartObject->GetEntityId();

					// If the smart object isn't busy
					if (pSmartObjectManager->IsSmartObjectBusy(pSmartObject))
					{
						// Attempt to wait for the SO to become free
						m_bWaitingForBusySmartObject = true;
					}
					else
					{
						const Vec3 smartObjectStart = pSmartObject->GetHelperPos(pSOLink->m_pFromHelper);
						CAIActor* closestActor = pPipeUser->CastToCAIActor();

						// Find the closest of all actors trying to use this SmartObject
						{
							ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
							size_t activeCount = lookUp.GetActiveCount();

							float distanceClosestSq = Distance::Point_PointSq(closestActor->GetPos(), smartObjectStart);

							for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
							{
								const Vec3 position = lookUp.GetPosition(actorIndex);
								const float distanceSq = Distance::Point_PointSq(position, smartObjectStart);

								if (distanceSq < distanceClosestSq)
								{
									if (CPipeUser* closerActor = lookUp.GetActor<CPipeUser>(actorIndex))
									{
										if (closerActor->GetPendingSmartObjectID() == smartObjectId)
										{
											distanceClosestSq = distanceSq;

											closestActor = closerActor;
										}
									}
								}
							}
						}

						assert(closestActor);

						// Are we the closest candidate?
						if (closestActor && (closestActor == pPipeUser->CastToCAIActor()))
						{
							// Fill in the actor target request, and figure out the navSO method.
							if (pSmartObjectManager->PrepareNavigateSmartObject(pPipeUser, pSmartObject, pSOLink->m_pSmartObjectClass, pSOLink->m_pFromHelper, pSOLink->m_pToHelper) && pPipeUser->m_eNavSOMethod != nSOmNone)
							{
								pipeUserActorTargetRequest.id = ++pPipeUser->m_actorTargetReqId;
								pipeUserActorTargetRequest.lowerPrecision = true;

								m_pendingActorTargetRequester = eTATR_NavSO;

								// In case we hit here because the path following has finished, keep the trace alive.
								*pbExactPositioning = true;

								// Enforce to use the current path.
								pPipeUser->m_Path.GetParams().inhibitPathRegeneration = true;
								pPipeUser->CancelRequestedPath(false);

								m_bWaitingForBusySmartObject = false;
							}
							else // Can't navigate using this SO for some reason
							{
								// Failed to use the navSO. Instead of resetting the goalop, set the state
								// to error, to prevent the link being reused.
								*pbForceRegeneratePath = true;
								pipeUserActorTargetRequest.Reset();
								m_actorTargetRequester = eTATR_None;
								m_pendingActorTargetRequester = eTATR_None;
							}
						}
						else // We are not the closest in line
						{
							// Attempt to wait for the SO to become free
							m_bWaitingForBusySmartObject = true;
						}
					}
				}
			}
			// No SO nav data
			else if (const SAIActorTargetRequest* pActiveActorTargetRequest = pPipeUser->GetActiveActorTargetRequest())
			{
				// Actor target requested at the end of the path.
				pipeUserActorTargetRequest = *pActiveActorTargetRequest;
				pipeUserActorTargetRequest.id = ++pPipeUser->m_actorTargetReqId;
				pipeUserActorTargetRequest.lowerPrecision = false;
				m_pendingActorTargetRequester = eTATR_EndOfPath;

				// In case we hit here because the path following has finished, keep the trace alive.
				*pbExactPositioning = true;

				// Enforce to use the current path.
				pPipeUser->m_Path.GetParams().inhibitPathRegeneration = true;
				pPipeUser->CancelRequestedPath(false);
			}
			break;
		}

	case eATP_Error:
		break;

	default:
		// The exact positioning is in motion but not yet playing, keep the trace alive.
		*pbExactPositioning = true;
	}
}

void COPTrace::Teleport(CPipeUser& pipeUser, const Vec3& teleportDestination)
{
	if (IEntity* entity = pipeUser.GetEntity())
	{
		Matrix34 transform = entity->GetWorldTM();
		transform.SetTranslation(teleportDestination);
		entity->SetWorldTM(transform);
		m_stuckDetector.Reset();
		bool forceGeneratePath = true;
		RegeneratePath(&pipeUser, &forceGeneratePath);
		m_bWaitingForPathResult = true;
	}
}
