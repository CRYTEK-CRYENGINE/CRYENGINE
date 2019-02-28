// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GoalOpStick.h"
#include "GoalOpTrace.h"
#include "Puppet.h"
#include "DebugDrawContext.h"

// Ugly
#define C_MaxDistanceForPathOffset 2 // threshold (in m) used in COPStick and COPApproach, to detect if the returned path
// is bringing the agent too far from the expected destination

//
//----------------------------------------------------------------------------------------------------------
COPStick::COPStick(float fStickDistance, float fEndAccuracy, float fDuration, int nFlags, int nFlagsAux, ETraceEndMode eTraceEndMode) :
	m_vLastUsedTargetPos(ZERO),
	m_fTrhDistance(1.f),  // regenerate path if target moved for more than this
	m_eTraceEndMode(eTraceEndMode),
	m_fApproachTime(-1.f),
	m_fHijackDistance(-1.f),
	m_fStickDistance(fStickDistance),
	m_fEndAccuracy(fEndAccuracy),
	m_fDuration(fDuration),
	m_bContinuous((nFlagsAux & 0x01) == 0),
	m_bTryShortcutNavigation((nFlagsAux & 0x02) != 0),
	m_bUseLastOpResult((nFlags & AILASTOPRES_USE) != 0),
	m_bLookAtLastOp((nFlags & AILASTOPRES_LOOKAT) != 0),
	m_bInitialized(false),
	m_bForceReturnPartialPath((nFlags & AI_REQUEST_PARTIAL_PATH) != 0),
	m_bStopOnAnimationStart((nFlags & AI_STOP_ON_ANIMATION_START) != 0),
	m_targetPredictionTime(0.f),
	m_pTraceDirective(NULL),
	m_pPathfindDirective(NULL),
	m_looseAttentionId(0),
	m_bPathFound(false),
	m_bBodyIsAligned(false),
	m_bAlignBodyBeforeMove(false),
	m_fCorrectBodyDirTime(0.0f),
	m_fTimeSpentAligning(0.0f)
{
	if (m_fStickDistance < 0.f)
	{
		AIWarning("COPStick::COPStick: Negative stick distance provided.");
	}

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPStick::COPStick %p", this);

	m_smoothedTargetVel.zero();
	m_lastTargetPos.zero();
	m_safePointInterval = 1.0f;
	m_maxTeleportSpeed = 10.0f;
	m_pathLengthForTeleport = 20.0f;
	m_playerDistForTeleport = 3.0f;

	m_lastVisibleTime.SetValue(0);
	ClearTeleportData();

	if (m_bContinuous)
	{
		// Continuous stick defaults to speed adjustment, allow to make it constant.
		m_bConstantSpeed = (nFlags & AI_CONSTANT_SPEED) != 0;
	}
	else
	{
		// Non-continuous stick defaults to constant speed, allow to enable adjustment.
		m_bConstantSpeed = (nFlags & AI_ADJUST_SPEED) == 0;
	}
}

//
//----------------------------------------------------------------------------------------------------------
COPStick::COPStick(const XmlNodeRef& node) :
	m_vLastUsedTargetPos(ZERO),
	m_fTrhDistance(1.f),  // regenerate path if target moved for more than this

	// [9/21/2010 evgeny] eTEM_FixedDistance for goalop "Stick", eTEM_MinimumDistance for goalop "StickMinimumDistance",
	// which uses the same class, COPStick
	m_eTraceEndMode(stricmp(node->getTag(), "Stick") ? eTEM_MinimumDistance : eTEM_FixedDistance),

	m_fApproachTime(-1.f),
	m_fHijackDistance(-1.f),
	m_fStickDistance(0.f),
	m_fEndAccuracy(0.f),
	m_fDuration(0.f),
	m_bContinuous(s_xml.GetBool(node, "continuous", false)),
	m_bTryShortcutNavigation(s_xml.GetBool(node, "tryShortcutNavigation")),
	m_bUseLastOpResult(s_xml.GetBool(node, "useLastOp")),
	m_bLookAtLastOp(s_xml.GetBool(node, "lookAtLastOp")),
	m_bInitialized(false),
	m_bForceReturnPartialPath(s_xml.GetBool(node, "requestPartialPath")),
	m_bStopOnAnimationStart(s_xml.GetBool(node, "stopOnAnimationStart")),
	m_targetPredictionTime(0.f),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_looseAttentionId(0),
	m_bPathFound(false),
	m_smoothedTargetVel(ZERO),
	m_lastTargetPos(ZERO),
	m_safePointInterval(1.f),
	m_maxTeleportSpeed(10.f),
	m_pathLengthForTeleport(20.f),
	m_playerDistForTeleport(3.f),
	m_sLocateName(node->getAttr("locateName")),
	m_bBodyIsAligned(false),
	m_bAlignBodyBeforeMove(s_xml.GetBool(node, "alignBeforeMove")),
	m_fCorrectBodyDirTime(0.0f),
	m_fTimeSpentAligning(0.0f)
{
	if (node->getAttr("duration", m_fDuration))
	{
		m_fDuration = fabsf(m_fDuration);
	}
	else
	{
		node->getAttr("distance", m_fStickDistance);
		if (m_fStickDistance < 0.f)
		{
			AIWarning("COPStick::COPStick: Negative stick distance provided.");
		}
	}

	if (!node->getAttr("endAccuracy", m_fEndAccuracy))
	{
		m_fEndAccuracy = m_fStickDistance;
	}

	// Random variation on the end distance.
	float fDistanceVariation;
	if (node->getAttr("distanceVariation", fDistanceVariation))
	{
		if (fDistanceVariation > 0.01f)
		{
			float u = cry_random(0, 9) / 9.f;
			m_fStickDistance = (m_fStickDistance > 0.f) ? max(0.f, m_fStickDistance - u * fDistanceVariation)
			                   : min(0.f, m_fStickDistance + u * fDistanceVariation);
		}
	}

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPStick::COPStick %p", this);

	m_lastVisibleTime.SetValue(0);
	ClearTeleportData();

	if (m_bContinuous)
	{
		// Continuous stick defaults to speed adjustment, allow to make it constant.
		m_bConstantSpeed = s_xml.GetBool(node, "constantSpeed");
	}
	else
	{
		// Non-continuous stick defaults to constant speed, allow to enable adjustment.
		m_bConstantSpeed = !s_xml.GetBool(node, "adjustSpeed");
	}

	node->getAttr("regenDistThreshold", m_fTrhDistance);
}

//
//----------------------------------------------------------------------------------------------------------
COPStick::~COPStick()
{
	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPStick::~COPStick %p", this);
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::Reset(CPipeUser* pPipeUser)
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPStick::Reset %s", GetNameSafe(pPipeUser));

	m_refStickTarget.Reset();
	m_refSightTarget.Reset();

	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	m_bBodyIsAligned = false;
	m_fCorrectBodyDirTime = 0.0f;
	m_fTimeSpentAligning = 0.0f;

	m_bPathFound = false;
	m_vLastUsedTargetPos.zero();

	m_smoothedTargetVel.zero();
	m_lastTargetPos.zero();

	ClearTeleportData();

	if (pPipeUser)
	{
		pPipeUser->ClearPath("COPStick::Reset m_Path");
		if (m_bLookAtLastOp)
		{
			pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
			m_looseAttentionId = 0;
		}

		// Clear the movement information so that the agent doesn't move
		SOBJECTSTATE& state = pPipeUser->m_State;
		state.vMoveDir.zero();
		state.vMoveTarget.zero();
		state.vInflectionPoint.zero();
		state.fDesiredSpeed = 0.0f;
		state.fDistanceToPathEnd = 0.0f;
		state.predictedCharacterStates.nStates = 0;
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::SSafePoint::Serialize(TSerialize ser)
{
	ser.BeginGroup("StickSafePoint");
	ser.Value("pos", pos);
	ser.Value("time", time);
	ser.Value("navCapMask", navCapMask);
	ser.Value("passRadius", passRadius);
	ser.Value("requesterName", requesterName); // is this needed?
	ser.EndGroup();
}

//
//----------------------------------------------------------------------------------------------------------
COPStick::SSafePoint::SSafePoint(const Vec3& pos_, CPipeUser& pipeUser)
	: pos(pos_)
	, time(GetAISystem()->GetFrameStartTime())
	, requesterName(pipeUser.GetName())
	, navCapMask(pipeUser.m_movementAbility.pathfindingProperties.navCapMask)
	, passRadius(pipeUser.m_Parameters.m_fPassRadius)
{
	Reset();
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::SSafePoint::Reset()
{
	safe = false;
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::Serialize(TSerialize ser)
{

	ser.BeginGroup("COPStick");
	{
		ser.Value("m_vLastUsedTargetPos", m_vLastUsedTargetPos);
		ser.Value("m_fTrhDistance", m_fTrhDistance);
		ser.Value("m_fStickDistance", m_fStickDistance);
		ser.Value("m_fEndAccuracy", m_fEndAccuracy);
		ser.Value("m_fDuration", m_fDuration);
		ser.Value("m_bContinuous", m_bContinuous);
		ser.Value("m_bLookAtLastOp", m_bLookAtLastOp);
		ser.Value("m_bTryShortcutNavigation", m_bTryShortcutNavigation);
		ser.Value("m_bUseLastOpResult", m_bUseLastOpResult);
		ser.Value("m_targetPredictionTime", m_targetPredictionTime);
		ser.Value("m_bPathFound", m_bPathFound);
		ser.Value("m_bInitialized", m_bInitialized);
		ser.Value("m_bConstantSpeed", m_bConstantSpeed);
		ser.Value("m_teleportCurrent", m_teleportCurrent);
		ser.Value("m_teleportEnd", m_teleportEnd);
		ser.Value("m_lastTeleportTime", m_lastTeleportTime);
		ser.Value("m_lastVisibleTime", m_lastVisibleTime);
		ser.Value("m_maxTeleportSpeed", m_maxTeleportSpeed);
		ser.Value("m_pathLengthForTeleport", m_pathLengthForTeleport);
		ser.Value("m_playerDistForTeleport", m_playerDistForTeleport);
		ser.Value("m_bForceReturnPartialPath", m_bForceReturnPartialPath);
		ser.Value("m_bStopOnAnimationStart", m_bStopOnAnimationStart);
		ser.Value("m_lastTargetPosTime", m_lastTargetPosTime);
		ser.Value("m_lastTargetPos", m_lastTargetPos);
		ser.Value("m_smoothedTargetVel", m_smoothedTargetVel);
		ser.Value("m_looseAttentionId", m_looseAttentionId);

		ser.Value("m_stickTargetSafePoints", m_stickTargetSafePoints);
		if (ser.IsReading())
		{
			for (uint8 i = 0, size = m_stickTargetSafePoints.Size(); i < size; ++i)
			{
				m_stickTargetSafePoints[i].Reset();
			}
		}
		ser.Value("m_safePointInterval", m_safePointInterval);

		m_refStickTarget.Serialize(ser, "m_refStickTarget");
		m_refSightTarget.Serialize(ser, "m_refSightTarget");

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pTraceDirective->Serialize(ser);
				ser.EndGroup();
			}
			if (ser.BeginOptionalGroup("PathFindDirective", m_pPathfindDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pPathfindDirective->Serialize(ser);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if (ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathfindDirective);
			if (ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathfindDirective = new COPPathFind("");
				m_pPathfindDirective->Serialize(ser);
				ser.EndGroup();
			}
		}
	}
	ser.EndGroup();
}

//===================================================================
// GetEndDistance
//===================================================================
float COPStick::GetEndDistance(CPipeUser* pPipeUser) const
{
	if (m_fDuration > 0.f)
	{
		float fNormalSpeed, fMinSpeed, fMaxSpeed;
		pPipeUser->GetMovementSpeedRange(pPipeUser->m_State.fMovementUrgency, false,
		                                 fNormalSpeed, fMinSpeed, fMaxSpeed);

		if (fNormalSpeed > 0.f)
			return -fNormalSpeed * m_fDuration;
	}
	return m_fStickDistance;
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::RegeneratePath(CPipeUser* pPipeUser, const Vec3& vDestination)
{
	if (!pPipeUser)
		return;

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPStick::RegeneratePath %s", GetNameSafe(pPipeUser));

	m_pPathfindDirective->Reset(pPipeUser);
	m_pTraceDirective->m_fEndAccuracy = m_fEndAccuracy;
	m_vLastUsedTargetPos = vDestination;
	pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;

	const Vec3 vPipeUserPos = pPipeUser->GetPhysicsPos();
}

//===================================================================
// DebugDraw
//===================================================================
void COPStick::DebugDraw(CPipeUser* pPipeUser) const
{
	CDebugDrawContext dc;
	unsigned nPts = m_stickTargetSafePoints.Size();
	unsigned iPt = 0;
	TStickTargetSafePoints::SConstIterator itEnd = m_stickTargetSafePoints.End();
	for (TStickTargetSafePoints::SConstIterator it = m_stickTargetSafePoints.Begin(); it != itEnd; ++it, ++iPt)
	{
		const SSafePoint& safePoint = *it;
		float frac = ((float) iPt) / nPts;
		ColorB color;
		if (safePoint.safe)
			color.set(0, uint8(255 * frac), uint8(255 * (1.0f - frac)), 255);
		else
			color.set(255, 0, 0, 255);
		dc->DrawSphere(safePoint.pos, 0.2f, color);
	}

	if (m_pPathfindDirective)
		m_pPathfindDirective->DebugDraw(pPipeUser);
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pPipeUser);
}

//===================================================================
// UpdateStickTargetSafePoints
//===================================================================
void COPStick::UpdateStickTargetSafePoints(CPipeUser* pPipeUser)
{
	Vec3 curPos;
	CLeader* pLeader = GetAISystem()->GetLeader(pPipeUser->GetGroupId());
	CAIObject* pOwner = pLeader ? pLeader->GetFormationOwner().GetAIObject() : 0;
	if (pOwner)
		curPos = pOwner->GetPhysicsPos();
	else
		curPos = m_refStickTarget.GetAIObject()->GetPhysicsPos();

	Vec3 opPos = pPipeUser->GetPhysicsPos();
	if (GetAISystem()->WouldHumanBeVisible(opPos, false))
		m_lastVisibleTime = GetAISystem()->GetFrameStartTime();

	if (!m_stickTargetSafePoints.Empty())
	{
		Vec3 delta = curPos - m_stickTargetSafePoints.Front().pos;
		float dist = delta.GetLength();
		if (dist < m_safePointInterval)
			return;
	}

	int maxNumPoints = 1 + (int)(m_pathLengthForTeleport / m_safePointInterval);
	if (m_stickTargetSafePoints.Size() >= maxNumPoints || m_stickTargetSafePoints.Full())
		m_stickTargetSafePoints.PopBack();

	m_stickTargetSafePoints.PushFront(SSafePoint(curPos, *pPipeUser));
}

//===================================================================
// ClearTeleportData
//===================================================================
void COPStick::ClearTeleportData()
{
	m_teleportCurrent.zero();
	m_teleportEnd.zero();
}

//===================================================================
// TryToTeleport
// This works by first detecting if we need to teleport. If we do, then
// we identify and store the teleport destination location. However, we
// don't move pPipeUser there immediately - we continue following our path. But we
// move a "ghost" from the initial position to the destination. If it
// reaches the destination without either the ghost or the real pPipeUser
// being seen by the player then pPipeUser (may) get teleported. If teleporting
// happens then m_stickTargetSafePoints needs to be updated.
//===================================================================
bool COPStick::TryToTeleport(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_stickTargetSafePoints.Size() < 2)
	{
		ClearTeleportData();
		return false;
	}

	if (pPipeUser->m_nPathDecision != PATHFINDER_NOPATH)
	{
		float curPathLen = pPipeUser->m_Path.GetPathLength(false);
		Vec3 stickPos = m_refStickTarget.GetAIObject()->GetPhysicsPos();
		if (pPipeUser->m_Path.Empty())
			curPathLen += Distance::Point_Point(pPipeUser->GetPhysicsPos(), stickPos);
		else
			curPathLen += Distance::Point_Point(pPipeUser->m_Path.GetLastPathPos(), stickPos);
		if (curPathLen < m_pathLengthForTeleport)
		{
			ClearTeleportData();
			return false;
		}
		else
		{
			// Danny/Luc todo issue some readability by sending a signal - the response to that signal
			// should ideally stop the path following
		}
	}

	CTimeValue now(GetAISystem()->GetFrameStartTime());

	if (m_lastVisibleTime == now)
	{
		ClearTeleportData();
		return false;
	}

	if (m_teleportEnd.IsZero() && !m_stickTargetSafePoints.Empty())
	{
		const Vec3 playerPos = GetAISystem()->GetPlayer()->GetPhysicsPos();

		TStickTargetSafePoints::SIterator itEnd = m_stickTargetSafePoints.End();
		for (TStickTargetSafePoints::SIterator it = m_stickTargetSafePoints.Begin(); it != itEnd; ++it)
		{
			const SSafePoint& sp = *it;
			if (!sp.safe)
				return false;

			float playerDistSq = Distance::Point_PointSq(sp.pos, playerPos);
			if (playerDistSq < square(m_playerDistForTeleport))
				continue;

			TStickTargetSafePoints::SIterator itNext = it;
			++itNext;
			if (itNext == itEnd)
			{
				m_teleportEnd = sp.pos;
				break;
			}
			else if (!itNext->safe)
			{
				m_teleportEnd = sp.pos;
				break;
			}
		}
	}

	if (m_teleportEnd.IsZero())
		return false;

	Vec3 curPos = pPipeUser->GetPhysicsPos();
	if (m_teleportCurrent.IsZero())
	{
		m_teleportCurrent = curPos;
		m_lastTeleportTime = now;

		// If player hasn't seen operand for X seconds then move along by that amount
		Vec3 moveDir = m_teleportEnd - m_teleportCurrent;
		float distToEnd = moveDir.NormalizeSafe();

		float dt = (now - m_lastVisibleTime).GetSeconds();
		float dist = dt * m_maxTeleportSpeed;
		if (dist > distToEnd)
			dist = distToEnd;

		m_teleportCurrent += dist * moveDir;

		return false;
	}

	// move the ghost
	Vec3 moveDir = m_teleportEnd - m_teleportCurrent;
	float distToEnd = moveDir.NormalizeSafe();

	float dt = (now - m_lastTeleportTime).GetSeconds();
	m_lastTeleportTime = now;

	float dist = dt * m_maxTeleportSpeed;
	bool reachedEnd = false;
	if (dist > distToEnd)
	{
		reachedEnd = true;
		dist = distToEnd;
	}
	m_teleportCurrent += dist * moveDir;

	if (GetAISystem()->WouldHumanBeVisible(m_teleportCurrent, false))
	{
		ClearTeleportData();
		return false;
	}

	if (reachedEnd)
	{
		AILogEvent("COPStick::TryToTeleport teleporting %s to (%5.2f, %5.2f, %5.2f)",
		           GetNameSafe(pPipeUser), m_teleportEnd.x, m_teleportEnd.y, m_teleportEnd.z);
		Vec3 floorPos = m_teleportEnd;
		GetFloorPos(floorPos, m_teleportEnd, 0.0f, 3.0f, 0.1f, AICE_ALL);
		pPipeUser->GetEntity()->SetPos(floorPos);
		pPipeUser->m_State.fDesiredSpeed = 0;
		pPipeUser->m_State.vMoveDir.zero();
		pPipeUser->ClearPath("Teleport");
		RegeneratePath(pPipeUser, m_refStickTarget.GetAIObject()->GetPos());

		unsigned nPts = m_stickTargetSafePoints.Size();
		unsigned nBack = 10;
		unsigned iPt = 0;
		TStickTargetSafePoints::SIterator itEnd = m_stickTargetSafePoints.End();
		for (TStickTargetSafePoints::SIterator it = m_stickTargetSafePoints.Begin(); it != itEnd; ++it, ++iPt)
		{
			SSafePoint& sp = *it;
			if (iPt == nPts - 1 || sp.pos == m_teleportEnd)
			{
				iPt -= min(iPt, nBack);
				TStickTargetSafePoints::SIterator itFirst = m_stickTargetSafePoints.Begin();
				itFirst += iPt;
				m_stickTargetSafePoints.Erase(itFirst, m_stickTargetSafePoints.End());
				break;
			}
		}
		ClearTeleportData();
		return true;
	}

	return false;
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPStick::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPStick_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Check to see if objects have disappeared since last call
	// Note that at least in the first iteration, they won't be there to start with
	if ((m_refSightTarget.IsSet() && !m_refSightTarget.IsValid()) ||
	    (m_refStickTarget.IsSet() && !m_refStickTarget.IsValid()))
	{
		CCCPOINT(COPStick_Execute_TargetRemoved);

		if (gAIEnv.CVars.DebugPathFinding)
			AILogAlways("COPStick::Execute (%p) resetting due stick/sight target removed", this);

		Reset(NULL);
	}

	// Do not mind the target direction when approaching.
	pPipeUser->m_bPathfinderConsidersPathTargetDirection = false;

	EGoalOpResult eGoalOpResult;

	if (!m_refStickTarget.IsValid())
		if (!GetStickAndSightTargets_CreatePathfindAndTraceGoalOps(pPipeUser, &eGoalOpResult))
			return eGoalOpResult;

	CAIObject* const pStickTarget = m_refStickTarget.GetAIObject();
	const CAIObject* const pSightTarget = m_refSightTarget.GetAIObject();

	SOBJECTSTATE& pipeUserState = pPipeUser->m_State;

	// Special case for formation points, do not stick to disabled points.
	if ((pStickTarget != NULL) && (pStickTarget->GetSubType() == IAIObject::STP_FORMATION) && !pStickTarget->IsEnabled())
	{
		// Wait until the formation becomes active again.
		pipeUserState.vMoveDir.zero();
		return eGOR_IN_PROGRESS;
	}

	if (!m_bContinuous && pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
	{

		if (gAIEnv.CVars.DebugPathFinding)
			AILogAlways("COPStick::Execute (%p) resetting due to non-continuous and no path %s", this, GetNameSafe(pPipeUser));

		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	//make sure the guy looks in correct direction
	if (m_bLookAtLastOp && pSightTarget)
	{
		m_looseAttentionId = pPipeUser->SetLooseAttentionTarget(m_refSightTarget);
	}

	// trace gets deleted when we reach the end and it's not continuous
	if (!m_pTraceDirective)
	{

		if (gAIEnv.CVars.DebugPathFinding)
			AILogAlways("COPStick::Execute (%p) returning true due to no trace directive %s", this, GetNameSafe(pPipeUser));

		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	bool b2D = !pPipeUser->IsUsing3DNavigation();

	EGoalOpResult eTraceResult = eGOR_IN_PROGRESS;

	///////// TRACE //////////////////////////////////////////////////////////
	// actually trace the path - continue doing this even whilst regenerating (etc) the path
	if (m_bPathFound)
	{
		if (!m_bAlignBodyBeforeMove || m_bBodyIsAligned)
		{
			if (!Trace(pPipeUser, pStickTarget, &eTraceResult))
				return eTraceResult;
		}
		else
		{
			//
			// Align body towards the move target before starting to move
			//

			// We need to perform a trace to get the move target.
			// The movement data set in there will be cleared out later
			// to prevent the movement to actually take place.
			if (!Trace(pPipeUser, pStickTarget, &eTraceResult))
				return eTraceResult;

			if (!pipeUserState.vMoveTarget.IsZero())
			{
				// Align body towards the move target
				Vec3 dirToMoveTarget = pipeUserState.vMoveTarget - pPipeUser->GetPhysicsPos();
				dirToMoveTarget.z = 0.0f;
				dirToMoveTarget.Normalize();
				pPipeUser->SetBodyTargetDir(dirToMoveTarget);

				// If animation body direction is within the angle threshold,
				// wait for some time and then mark the agent as ready to move
				Vec3 actualBodyDir = pPipeUser->GetBodyInfo().vAnimBodyDir;
				const bool lookingTowardsMoveTarget = (actualBodyDir.Dot(dirToMoveTarget) > cosf(DEG2RAD(17.0f)));
				if (lookingTowardsMoveTarget)
					m_fCorrectBodyDirTime += gEnv->pTimer->GetFrameTime();
				else
					m_fCorrectBodyDirTime = 0.0f;

				const float timeSpentAligning = m_fTimeSpentAligning + gEnv->pTimer->GetFrameTime();
				m_fTimeSpentAligning = timeSpentAligning;

				if (m_fCorrectBodyDirTime > 0.2f || timeSpentAligning > 8.0f)
				{
					pPipeUser->ResetBodyTargetDir();
					m_bBodyIsAligned = true;
				}
			}

			// Clear the movement information so that the agent doesn't move
			pipeUserState.vMoveDir.zero();
			pipeUserState.vMoveTarget.zero();
			pipeUserState.vInflectionPoint.zero();
			pipeUserState.fDesiredSpeed = 0.0f;
			pipeUserState.fDistanceToPathEnd = 0.0f;
			pipeUserState.predictedCharacterStates.nStates = 0;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	// We should never get asserted here! If this assert hits, then the m_pStickTarget was changed during trace,
	// which should never happen!
	// (MATT) That might not be true since the ref refactor {2009/03/23}
	AIAssert(pStickTarget);
	// Adding anti-crash "if" [6/6/2010 evgeny]
	if (!pStickTarget)
		return eGOR_IN_PROGRESS;

	// Cache some values [6/6/2010 evgeny]
	Vec3 vStickTargetPos = pStickTarget->GetPhysicsPos();
	const Vec3 vPipeUserPos = pPipeUser->GetPhysicsPos();
	const float fPathDistanceLeft = pPipeUser->m_Path.GetPathLength(false);

	// Hijack
	if (m_eTraceEndMode == eTEM_MinimumDistance)
		if (HandleHijack(pPipeUser, vStickTargetPos, fPathDistanceLeft, eTraceResult))
			return eGOR_SUCCEEDED;

	// Teleport
	if (m_maxTeleportSpeed > 0.f && pPipeUser->m_movementAbility.teleportEnabled)
	{
		UpdateStickTargetSafePoints(pPipeUser);
		TryToTeleport(pPipeUser);
	}

	// Target prediction
	m_targetPredictionTime = (pPipeUser->GetType() == AIOBJECT_VEHICLE) ? 2.f : 0.f;
	if (m_targetPredictionTime > 0.f)
	{
		HandleTargetPrediction(pPipeUser, vStickTargetPos);
	}
	else
	{
		m_smoothedTargetVel.zero();
		m_lastTargetPos = vStickTargetPos;
	}
	Vec3 vPredictedTargetOffset = m_smoothedTargetVel * m_targetPredictionTime;

	// ensure offset doesn't cross forbidden
	int buildingID = -1;
	IAISystem::ENavigationType ePipeUserLastNavNodeType = gAIEnv.pNavigation->CheckNavigationType(pPipeUser->GetPos(),
	                                                                                              buildingID, pPipeUser->GetMovementAbility().pathfindingProperties.navCapMask);

	m_pPathfindDirective->SetTargetOffset(vPredictedTargetOffset);
	vStickTargetPos += vPredictedTargetOffset;

	Vec3 vToStickTarget = ((m_bForceReturnPartialPath && (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND))
	                       ? pPipeUser->m_Path.GetLastPathPos()
	                       : vStickTargetPos) - vPipeUserPos;

	if (b2D)
		vToStickTarget.z = 0.f;

	IAIObject* pAttTarget = pPipeUser->GetAttentionTarget();
	if (pAttTarget && (pAttTarget != pStickTarget) && pipeUserState.vMoveDir.IsZero(.05f))
		pPipeUser->m_bLooseAttention = false;

	//////////////////////////////////////////////////////////////////////////
	{
		CRY_PROFILE_REGION(PROFILE_AI, "OPStick::Execute(): Final Path Processing!")

		int nPathDecision = pPipeUser->m_nPathDecision;

		// If the target pos moves substantially update
		// If we're not already close to the target but it's moved a bit then update.
		// Be careful about forcing an update too often - especially if we're nearly there.
		if (nPathDecision != PATHFINDER_STILLFINDING)
		{
			if ((m_pTraceDirective->m_Maneuver == COPTrace::eMV_None) && !m_pTraceDirective->m_passingStraightNavSO)
			{
				EActorTargetPhase eCurrentActorTargetPhase = pipeUserState.curActorTargetPhase;
				if ((eCurrentActorTargetPhase == eATP_None) || (eCurrentActorTargetPhase == eATP_Error))
				{
					const SNavPathParams& navPathParams = pPipeUser->m_Path.GetParams();
					if (!navPathParams.precalculatedPath && !navPathParams.inhibitPathRegeneration)
					{
						if (IsTargetDirty(pPipeUser, vPipeUserPos, b2D, vStickTargetPos, ePipeUserLastNavNodeType))
						{
							RegeneratePath(pPipeUser, vStickTargetPos);
						}
					}
				}
			}
		}

		// check pathfinder status
		return HandlePathDecision(pPipeUser, nPathDecision, b2D);
	}
}

//===================================================================
// ExecuteDry
// Note - it is very important we don't call Reset on ourself from here
// else we might restart ourself subsequently
//===================================================================
void COPStick::ExecuteDry(CPipeUser* pPipeUser)
{
	CAIObject* const pStickTarget = m_refStickTarget.GetAIObject();

	if (m_pTraceDirective && pStickTarget)
	{
		bool bAdjustSpeed = !m_bConstantSpeed && (m_pTraceDirective->m_Maneuver == COPTrace::eMV_None);

		if (bAdjustSpeed && (pPipeUser->GetType() == AIOBJECT_ACTOR) && !pPipeUser->IsUsing3DNavigation())
		{
			pPipeUser->m_State.fMovementUrgency = AISPEED_SPRINT;
		}

		if (m_bPathFound && (!m_bAlignBodyBeforeMove || m_bBodyIsAligned))
		{
			m_pTraceDirective->ExecuteTrace(pPipeUser, false);
		}

		if (bAdjustSpeed)
			if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
				pPuppet->AdjustSpeed(pStickTarget, m_fStickDistance);
	}
}

bool COPStick::GetStickAndSightTargets_CreatePathfindAndTraceGoalOps(CPipeUser* pPipeUser, EGoalOpResult* peGoalOpResult)
{
	// first time = lets stick to target or special named if passed in
	if (m_sLocateName.empty())
	{
		m_refStickTarget = GetWeakRef(static_cast<CAIObject*>(pPipeUser->GetAttentionTarget()));
	}
	else if (!m_bUseLastOpResult) // Don't bother looking up the special AI object if we're going to look at the last op next anyway
	{
		CAIObject* pAIObject = pPipeUser->GetSpecialAIObject(m_sLocateName, 0.0f);
		m_refStickTarget = GetWeakRef(pAIObject);
	}

	if (m_bUseLastOpResult || !m_refStickTarget.IsValid())
	{
		if (pPipeUser->m_refLastOpResult.IsValid())
		{
			m_refStickTarget = pPipeUser->m_refLastOpResult;
		}
		else
		{
			if (gAIEnv.CVars.DebugPathFinding)
				AILogAlways("COPStick::Execute resetting due to no stick target %s", GetNameSafe(pPipeUser));

			// no target, nothing to stick to
			Reset(pPipeUser);
			*peGoalOpResult = eGOR_FAILED;
			return false;
		}
	}

	// keep last op. result as sight target
	if (m_bLookAtLastOp && !m_refSightTarget.IsValid() && pPipeUser->m_refLastOpResult.IsValid())
		m_refSightTarget = pPipeUser->m_refLastOpResult;

	CAIObject* const pStickTarget = m_refStickTarget.GetAIObject();
	if (pStickTarget == pPipeUser)
	{
		AILogAlways("COPStick::Execute sticking to self %s ", GetNameSafe(pPipeUser));
		Reset(pPipeUser);
		*peGoalOpResult = eGOR_SUCCEEDED;
		return false;
	}

	const CAIObject* const pSightTarget = m_refSightTarget.GetAIObject();

	if (m_fStickDistance > 0.f && (pStickTarget->GetSubType() == CAIObject::STP_ANIM_TARGET))
	{
		AILogAlways("COPStick::Execute resetting stick distance from %.1f to zero because the stick target is anim target. %s",
		            m_fStickDistance, GetNameSafe(pPipeUser));
		m_fStickDistance = 0.f;
	}

	// Create pathfinder operation
	{
		CRY_PROFILE_REGION(PROFILE_AI, "OPStick::Execute(): Create Pathfinder/Tracer!");

		Vec3 vStickPos = pStickTarget->GetPhysicsPos();

		if (gAIEnv.CVars.DebugPathFinding)
			AILogAlways("COPStick::Execute (%p) Creating pathfind/trace directives to (%5.2f, %5.2f, %5.2f) %s", this,
			            vStickPos.x, vStickPos.y, vStickPos.z, GetNameSafe(pPipeUser));

		float fEndTolerance = (m_bForceReturnPartialPath || m_fEndAccuracy < 0.f)
		                      ? std::numeric_limits<float>::max()
		                      : m_fEndAccuracy;
		// override end distance if a duration has been set
		m_pPathfindDirective = new COPPathFind("", pStickTarget, fEndTolerance, GetEndDistance(pPipeUser));

		bool bExactFollow = !pSightTarget && !pPipeUser->m_bLooseAttention;
		m_pTraceDirective = new COPTrace(bExactFollow, m_fEndAccuracy, m_bForceReturnPartialPath, m_bStopOnAnimationStart, m_eTraceEndMode);

		RegeneratePath(pPipeUser, vStickPos);

		// TODO: This is hack to prevent the Alien Scouts not to use the speed control.
		// Use better test to use the speed control _only_ when it is really needed (human formations).
		CPuppet* pPuppet = pPipeUser->CastToCPuppet();
		if ((pPuppet != NULL) && !m_bInitialized && !pPuppet->m_movementAbility.b3DMove)
		{
			pPuppet->ResetSpeedControl();
			m_bInitialized = true;
		}
	}

	return true;
}

bool COPStick::Trace(CPipeUser* pPipeUser, CAIObject* pStickTarget, EGoalOpResult* peTraceResult)
{
	CRY_ASSERT_MESSAGE(m_pTraceDirective, "m_pTraceDirective should really be set here (set by the calling code)");
	if (!m_pTraceDirective)
	{
		*peTraceResult = eGOR_FAILED;
		return false;
	}

	CRY_PROFILE_REGION(PROFILE_AI, "OPStick::Trace(): Tracer Execute!");

	// if using AdjustSpeed then force sprint at this point - will get overridden later
	bool bAdjustSpeed = !m_bConstantSpeed && (m_pTraceDirective->m_Maneuver == COPTrace::eMV_None);

	if (bAdjustSpeed && (pPipeUser->GetType() == AIOBJECT_ACTOR) && !pPipeUser->IsUsing3DNavigation())
	{
		pPipeUser->m_State.fMovementUrgency = AISPEED_SPRINT;
	}

	*peTraceResult = m_pTraceDirective->Execute(pPipeUser);

	if (bAdjustSpeed)
		if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
			pPuppet->AdjustSpeed(pStickTarget, m_fStickDistance);

	// If the path has been traced, finish the operation if the operand is not sticking continuously.
	if (*peTraceResult != eGOR_IN_PROGRESS)
	{
		if (!m_bContinuous && (m_eTraceEndMode != eTEM_MinimumDistance))
		{

			if (gAIEnv.CVars.DebugPathFinding)
				AILogAlways("COPStick::Execute (%p) finishing due to non-continuous and finished tracing %s", this, GetNameSafe(pPipeUser));

			Reset(pPipeUser);
			return false;
		}

		m_pPathfindDirective->m_bWaitingForResult = false;
	}

	// m_pStickTarget might be set to NULL as result of passing thru a navigation smart objects
	// on the path by executing an AI action which inserts a goal pipe which calls Reset() for
	// all active goals including this one... in this case it would be fine if we just end this
	// Execute() cycle and wait for the next one
	if (!pStickTarget)
	{
		*peTraceResult = eGOR_IN_PROGRESS;
		return false;
	}

	return true;
}

bool COPStick::HandleHijack(CPipeUser* pPipeUser, const Vec3& vStickTargetPos, float fPathDistanceLeft, EGoalOpResult eTraceResult)
{
	const float HIJACK_DISTANCE = 0.2f;

	const Vec3 vToStickTarget = vStickTargetPos - pPipeUser->GetPhysicsPos();
	const float fToStickTargetLength2D = vStickTargetPos.GetLength2D();

	if (((eTraceResult != eGOR_IN_PROGRESS) && !m_bContinuous) ||
	    (m_bPathFound && (fPathDistanceLeft < HIJACK_DISTANCE)))
	{
		float fFrameStartTimeInSeconds = GetAISystem()->GetFrameStartTimeSeconds();

		if (m_fApproachTime == -1.f)
		{
			const float MIN_APPROACH_TIME = 0.3f;
			m_fApproachTime = fFrameStartTimeInSeconds + MIN_APPROACH_TIME;
			m_fHijackDistance = fToStickTargetLength2D;
		}
		else if (m_fApproachTime <= fFrameStartTimeInSeconds)
		{
			const float fOldDistance = (vStickTargetPos - pPipeUser->GetLastPosition()).GetLength2D();
			// Replaced GetPos with GetPhysicsPos [6/6/2010 evgeny]
			//const float fNewDistance = (vStickTargetPos - pPipeUser->GetPos()).GetLength2D();
			const float& fNewDistance = fToStickTargetLength2D;

			// Ideally, this check should be framerate dependent
			const float MOVEMENT_STOPPED_SECOND_EPSILON = 0.05f;
			if (fNewDistance >= fOldDistance - MOVEMENT_STOPPED_SECOND_EPSILON)
			{
				// We're done!
				m_fApproachTime = -1.f;
				m_fHijackDistance = -1.f;

				if (gAIEnv.CVars.DebugPathFinding)
					AILogAlways("COPStick::Execute (%p) finishing due to non-continuous and finished tracing %s", this, GetNameSafe(pPipeUser));

				Reset(pPipeUser);
				return true;
			}
		}

		// Ignore pathfinding and force movement in the direction of our target.
		// This will get us as close to a target as physically possible.
		SOBJECTSTATE& pipeUserState = pPipeUser->m_State;
		float fNormalSpeed, fMinSpeed, fMaxSpeed;
		pPipeUser->GetMovementSpeedRange(pipeUserState.fMovementUrgency, pipeUserState.allowStrafing,
		                                 fNormalSpeed, fMinSpeed, fMaxSpeed);
		const float& fRemainingDistance = fToStickTargetLength2D;
		CRY_ASSERT(m_fHijackDistance > 0.f);
		// slow down as we approach the target
		pipeUserState.fDesiredSpeed = fNormalSpeed * fRemainingDistance / m_fHijackDistance;
		pipeUserState.vMoveDir = vToStickTarget.GetNormalizedSafe(Vec3Constants<float>::fVec3_Zero);
	}

	return false;
}

void COPStick::HandleTargetPrediction(CPipeUser* pPipeUser, const Vec3& vStickTargetPos)
{
	CTimeValue now = GetAISystem()->GetFrameStartTime();

	if (m_lastTargetPos.IsZero())
	{
		m_lastTargetPos = vStickTargetPos;
		m_lastTargetPosTime = now;
		m_smoothedTargetVel.zero();
	}
	else
	{
		int64 dt = (now - m_lastTargetPosTime).GetMilliSecondsAsInt64();
		if (dt > 0)
		{
			Vec3 targetVel = vStickTargetPos - m_lastTargetPos;

			if (targetVel.GetLengthSquared() > 5.0f) // try to catch sudden jumps
				targetVel.zero();
			else
				targetVel /= (static_cast<float>(dt) * 0.001f);

			// Danny todo make this time timestep independent (exp dependency on dt)
			const float frac = 0.1f;
			m_smoothedTargetVel = frac * targetVel + (1.f - frac) * m_smoothedTargetVel;
			m_lastTargetPos = vStickTargetPos;
			m_lastTargetPosTime = now;
		}
	}
}

bool COPStick::IsTargetDirty(CPipeUser* pPipeUser, const Vec3& vPipeUserPos, bool b2D, const Vec3& vStickTargetPos, IAISystem::ENavigationType ePipeUserLastNavNodeType)
{
	// check if need to regenerate path
	const Vec3 vFromStickTargetToLastUsedTarget = m_vLastUsedTargetPos - vStickTargetPos;
	float fTargetMoveDist = b2D
	                        ? vFromStickTargetToLastUsedTarget.GetLength2D()
	                        : vFromStickTargetToLastUsedTarget.GetLength();

	// [AlexMcC] If the target hasn't moved, there's no good reason to update our path.
	// Mikko's comment below explains why the path can be updated too frequently otherwise.
	// If we don't check to see if the target has moved, we could update our path every
	// frame and never move.
	if (fTargetMoveDist <= m_fTrhDistance)
		return false;

	if (pPipeUser->m_Path.Empty())
		return true;

	// If the target is moving approximately to the same direction, do not update the path so often.

	// Use the stored destination point instead of the last path node since the path may be cut because of navSO.
	const Vec3& vPathEndPos = pPipeUser->m_PathDestinationPos;
	Vec3 vDirToPathEnd = vPathEndPos - vPipeUserPos;
	if (b2D)
		vDirToPathEnd.z = 0.f;
	vDirToPathEnd.NormalizeSafe();

	Vec3 vDirFromPathEndToStickTarget = vStickTargetPos - vPathEndPos;
	if (b2D)
		vDirFromPathEndToStickTarget.z = 0.f;
	vDirFromPathEndToStickTarget.NormalizeSafe();

	float fRegenerateDist = m_fTrhDistance;
	if (vDirFromPathEndToStickTarget.Dot(vDirToPathEnd) < cosf(DEG2RAD(8.f)))
		fRegenerateDist *= 5.f;

	if (fTargetMoveDist > fRegenerateDist)
		return true;

	// when near the path end force more frequent updates
	float fPathDistLeft = pPipeUser->m_Path.GetPathLength(false);
	const Vec3 vFromStickTargetToPathEnd = vPathEndPos - vStickTargetPos;
	float fPathEndError = b2D ? vFromStickTargetToPathEnd.GetLength2D() : vFromStickTargetToPathEnd.GetLength();

	// TODO/HACK! [Mikko] This prevent the path to regenerated every frame in some special cases in Crysis Core level
	// where quite a few behaviors are sticking to a long distance (7-10m).
	// The whole stick regeneration logic is flawed mostly because pPipeUser->m_PathDestinationPos is not always
	// the actual target position. The pathfinder may adjust the end location and not keep the requested end pos
	// if the target is not reachable. I'm sure there are other really nasty cases about this path invalidation logic too.
	if (ePipeUserLastNavNodeType == IAISystem::NAV_VOLUME)
		fPathEndError = max(0.0f, fPathEndError - GetEndDistance(pPipeUser));

	return (fPathEndError > 0.1f) && (fPathDistLeft < 2.f * fPathEndError);
}

EGoalOpResult COPStick::HandlePathDecision(CPipeUser* pPipeUser, int nPathDecision, bool b2D)
{
	switch (nPathDecision)
	{
	case PATHFINDER_STILLFINDING:
		{
			m_pPathfindDirective->Execute(pPipeUser);
			return eGOR_IN_PROGRESS;
		}

	case PATHFINDER_NOPATH:
		pPipeUser->m_State.vMoveDir.zero();
		if (m_bContinuous)
		{
			return eGOR_IN_PROGRESS;
		}
		else
		{
			if (gAIEnv.CVars.DebugPathFinding)
				AILogAlways("COPStick::Execute (%p) resetting due to no path %s", this, GetNameSafe(pPipeUser));

			Reset(pPipeUser);
			return eGOR_FAILED;
		}

	case PATHFINDER_PATHFOUND:
		{
			// do not spam the AI with the OnPathFound signal (stick goal regenerates the path frequently)
			if (!m_bPathFound)
			{
				m_bPathFound = true;
				TPathPoints::const_reference lastPathNode = pPipeUser->m_OrigPath.GetPath().back();
				const Vec3& vLastPos = lastPathNode.vPos;
				const Vec3& vRequestedLastNodePos = pPipeUser->m_Path.GetParams().end;
				// send signal to AI
				float fDistance = b2D
				                  ? Distance::Point_Point2D(vLastPos, vRequestedLastNodePos)
				                  : Distance::Point_Point(vLastPos, vRequestedLastNodePos);

				if ((lastPathNode.navType != IAISystem::NAV_SMARTOBJECT) &&
				    (fDistance > m_fStickDistance + C_MaxDistanceForPathOffset))
				{
					AISignalExtraData* pData = new AISignalExtraData;
					pData->fValue = fDistance - m_fStickDistance;
					pPipeUser->SetSignal(0, "OnEndPathOffset", pPipeUser->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnEndPathOffset);
				}
				else
				{
					pPipeUser->SetSignal(0, "OnPathFound", NULL, 0, gAIEnv.SignalCRCs.m_nOnPathFound);
				}

				return Execute(pPipeUser);
			}
		}
	}

	return eGOR_IN_PROGRESS;
}
