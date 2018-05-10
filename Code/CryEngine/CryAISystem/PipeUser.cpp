// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PipeUser.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   -

 *********************************************************************/
#include "StdAfx.h"
#include "CAISystem.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include "GoalOp.h"
#include "PipeUser.h"
#include "Puppet.h"
#include "Leader.h"
#include "AIActions.h"
#include "PathFollower.h"
#include "SmartPathFollower.h"
#include <CryNetwork/ISerialize.h>
#include "ObjectContainer.h"
#include "TargetSelection/TargetTrackManager.h"
#include "Formation/FormationManager.h"

#include "Cover/CoverSystem.h"
#include "AIObject/PipeUserMovement/MovementBlock_InstallPipeUserInCover.h"
#include "AIObject/PipeUserMovement/MovementBlock_SetupPipeUserCoverInformation.h"
#include "AIObject/PipeUserMovement/MovementBlock_UninstallPipeUserFromCover.h"

#include "DebugDrawContext.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"
#include <CryAISystem/MovementStyle.h>
#include <CryAISystem/MovementRequest.h>

#pragma warning(push)
#pragma warning(disable:4355) // 'this': used in base member initializer list
CPipeUser::CPipeUser()
	: m_fTimePassed(0)
	, m_adjustingAim(false)
	, m_pPathFollower(0)
	, m_bPathfinderConsidersPathTargetDirection(true)
	, m_vLastMoveDir(ZERO)
	, m_bKeepMoving(false)
	, m_nPathDecision(PATHFINDER_NOPATH)
	, m_queuedPathId(0)
	, m_bFirstUpdate(true)
	, m_bBlocked(false)
	, m_pCurrentGoalPipe(NULL)
	, m_fireMode(FIREMODE_OFF)
	, m_fireModeUpdated(false)
	, m_bLooseAttention(false)
	, m_IsSteering(false)
#ifdef _DEBUG
	, m_DEBUGmovementReason(AIMORE_UNKNOWN)
#endif
	, m_AttTargetPersistenceTimeout(0.0f)
	, m_AttTargetThreat(AITHREAT_NONE)
	, m_AttTargetExposureThreat(AITHREAT_NONE)
	, m_AttTargetType(AITARGET_NONE)
	, m_bPathToFollowIsSpline(false)
	, m_lastLiveTargetPos(ZERO)
	, m_timeSinceLastLiveTarget(-1.0f)
	, m_refShape(0)
	, m_looseAttentionId(0)
	, m_aimState(AI_AIM_NONE)
	, m_pActorTargetRequest(0)
	, m_PathDestinationPos(ZERO)
	, m_eNavSOMethod(nSOmNone)
	, m_idLastUsedSmartObject(0)
	, m_actorTargetReqId(1)
	, m_spreadFireTime(0.0f)
	, m_lastExecutedGoalop(eGO_LAST)
	, m_paused(0)
	, m_bEnableUpdateLookTarget(true)
	, m_adjustpath(0)
	, m_pipeExecuting(false)
	, m_cutPathAtSmartObject(true)
	, m_considerActorsAsPathObstacles(false)
	, m_movementActorAdapter(*this)
	, m_bLastActionSucceed(false)
	, m_pCoverUser(nullptr)
	, m_pCoverEyesTargetObject(nullptr)
{
	CCCPOINT(CPipeUser_CPipeUser);

	_fastcast_CPipeUser = true;

	// (MATT) SetName could be called to ensure we're consistent, rather than the code above {2009/02/03}

	m_CurrentHideObject.m_HideSmartObject.pChainedUserEvent = NULL;
	m_CurrentHideObject.m_HideSmartObject.pChainedObjectEvent = NULL;

	m_callbacksForPipeuser.queuePathRequestFunction = functor(*this, &CPipeUser::RequestPathTo);
	m_callbacksForPipeuser.checkOnPathfinderStateFunction = functor(*this, &CPipeUser::GetPathfinderState);
	m_callbacksForPipeuser.getPathFollowerFunction = functor(*this, &CPipeUser::GetPathFollower);
	m_callbacksForPipeuser.getPathFunction = functor(*this, &CPipeUser::GetINavPath);

	m_coverMovementAbility.addStartMovementBlocksCallback = functor(*this, &CPipeUser::CreateMovementPlanCoverStartBlocks);
	m_coverMovementAbility.addEndMovementBlocksCallback = functor(*this, &CPipeUser::CreateMovementPlanCoverEndBlocks);
	m_coverMovementAbility.prePathFollowingUpdateCallback = functor(*this, &CPipeUser::PrePathFollowUpdate);
}
#pragma warning(pop)

static bool bNotifyListenersLock = false;

CPipeUser::~CPipeUser(void)
{
	CCCPOINT(CPipeUser_Destructor);

	// (MATT) Can't a call to Reset be included here? {2009/02/04}

	if (m_pCurrentGoalPipe)
	{
		ResetCurrentPipe(true);
	}

	while (!m_mapGoalPipeListeners.empty())
		UnRegisterGoalPipeListener(m_mapGoalPipeListeners.begin()->second.first, m_mapGoalPipeListeners.begin()->first);

	// Delete special AI objects, and reset array.
	for (unsigned i = 0; i < COUNT_AISPECIAL; ++i)
		m_refSpecialObjects[i].Release();

	SAFE_RELEASE(m_pPathFollower);
	SAFE_DELETE(m_pActorTargetRequest);

	gAIEnv.pMovementSystem->UnregisterEntity(GetEntityID());

	if (m_pCoverUser)
	{
		gAIEnv.pCoverSystem->UnregisterEntity(GetEntityID());
	}
}

void CPipeUser::Event(unsigned short int eType, SAIEVENT* pAIEvent)
{
	CAIActor::Event(eType, pAIEvent);

	switch (eType)
	{
	case AIEVENT_CLEAR:
		{
			CCCPOINT(CPuppet_Event_Clear);

			ClearActiveGoals();
			m_bLooseAttention = false;
			gAIEnv.pFormationManager->FreeFormationPoint(GetWeakRef(this));
			SetAttentionTarget(NILREF);
			m_bBlocked = false;
		}
		break;
	case AIEVENT_CLEARACTIVEGOALS:
		ClearActiveGoals();
		m_bBlocked = false;
		break;
	case AIEVENT_AGENTDIED:
		SetNavSOFailureStates();
		CRY_ASSERT(m_pCoverUser);
		if (m_pCoverUser && !m_pCoverUser->GetState().IsEmpty())
		{
			m_pCoverUser->Reset();
		}

		m_Path.Clear("Agent Died");
		break;
	case AIEVENT_ADJUSTPATH:
		m_adjustpath = pAIEvent->nType;
		break;
	}
}

void CPipeUser::ParseParameters(const AIObjectParams& params, bool bParseMovementParams)
{
	CAIActor::ParseParameters(params, bParseMovementParams);

	// Reset the path follower parameters
	if (m_pPathFollower)
	{
		PathFollowerParams pathFollowerParams;
		GetPathFollowerParams(pathFollowerParams);

		m_pPathFollower->SetParams(pathFollowerParams);
		m_pPathFollower->Reset();
	}
}

void CPipeUser::CreateRefPoint()
{
	if (m_refRefPoint.IsSet() && m_refRefPoint.GetAIObject())
		return;

	gAIEnv.pAIObjectManager->CreateDummyObject(m_refRefPoint, string(GetName()) + "_RefPoint", STP_REFPOINT);
	CAIObject* pRefPoint = m_refRefPoint.GetAIObject();
	assert(pRefPoint);

	if (pRefPoint)
	{
		pRefPoint->SetPos(GetPos(), GetMoveDir());
	}
}

void CPipeUser::CreateLookAtTarget()
{
	if (m_refLookAtTarget.IsSet() && m_refLookAtTarget.GetAIObject())
	{
		return;
	}

	gAIEnv.pAIObjectManager->CreateDummyObject(m_refLookAtTarget, string(GetName()) + "_LookAtTarget", STP_LOOKAT);
	CAIObject* pLookAtTarget = m_refLookAtTarget.GetAIObject();
	pLookAtTarget->SetPos(GetPos() + GetMoveDir());
}

//
//
void CPipeUser::Reset(EObjectResetType type)
{
	CCCPOINT(CPipeUser_Reset);

	m_notAllowedSubpipes.clear();
	SelectPipe(0, "_first_", NILREF, 0, true);
	m_Path.Clear("CPipeUser::Reset m_Path");

	CAIActor::Reset(type);

	ClearPath("Reset");

	m_adjustingAim = false;

	m_bLastNearForbiddenEdge = false;

	SetAttentionTarget(NILREF);
	m_PathDestinationPos.zero();
	m_refPathFindTarget.Reset();
	m_IsSteering = false;
	m_fireMode = FIREMODE_OFF;
	SetFireTarget(NILREF);
	m_fireModeUpdated = false;
	m_outOfAmmoSent = false;
	m_lowAmmoSent = false;
	m_wasReloading = false;
	m_actorTargetReqId = 1;

	ResetLookAt();
	ResetBodyTargetDir();
	ResetDesiredBodyDirectionAtTarget();
	ResetMovementContext();

	m_aimState = AI_AIM_NONE;

	m_posLookAtSmartObject.zero();

	m_eNavSOMethod = nSOmNone;
	m_pendingNavSOStates.Clear();
	m_currentNavSOStates.Clear();

	m_CurrentNodeNavType = IAISystem::NAV_UNSET;
	m_idLastUsedSmartObject = 0;

	m_CurrentHideObject.m_HideSmartObject.Clear();
	m_bFirstUpdate = true;

	m_lastLiveTargetPos.zero();
	m_timeSinceLastLiveTarget = -1.0f;
	m_spreadFireTime = 0.0f;

	m_recentUnreachableHideObjects.clear();

	m_bPathToFollowIsSpline = false;

	m_refShapeName.clear();
	m_refShape = 0;

	m_paused = 0;
	m_bEnableUpdateLookTarget = true;

	SAFE_DELETE(m_pActorTargetRequest);

	// Delete special AI objects, and reset array.
	for (unsigned i = 0; i < COUNT_AISPECIAL; ++i)
		m_refSpecialObjects[i].Release();

	m_adjustpath = 0;
	m_pipeExecuting = false;

	m_pathAdjustmentObstacles.Reset();

	switch (type)
	{
	case AIOBJRESET_INIT:
	{
		if (!gAIEnv.pMovementSystem->IsEntityRegistered(GetEntityID()))
		{
			gAIEnv.pMovementSystem->RegisterEntity(GetEntityID(), m_callbacksForPipeuser, m_movementActorAdapter);
			gAIEnv.pMovementSystem->AddActionAbilityCallbacks(GetEntityID(), m_coverMovementAbility);
		}

		ICoverUser::Params params;
		params.distanceToCover = m_Parameters.distanceToCover;
		params.inCoverRadius = m_Parameters.inCoverRadius;
		params.minEffectiveCoverHeight = m_Parameters.effectiveCoverHeight;
		params.userID = GetEntityID();
		params.fillCoverEyesCustomMethod = functor(*this, &CPipeUser::FillCoverEyes);
		params.activeCoverInvalidateCallback = functor(*this, &CPipeUser::SetCoverInvalidated);
		params.activeCoverCompromisedCallback = functor(*this, &CPipeUser::SetCoverInvalidated);
		m_pCoverUser = gAIEnv.pCoverSystem->RegisterEntity(GetEntityID(), params);
		m_pCoverUser->Reset();
		break;
	}
	case AIOBJRESET_SHUTDOWN:
	{
		gAIEnv.pMovementSystem->UnregisterEntity(GetEntityID());
		if (m_pCoverUser)
		{
			gAIEnv.pCoverSystem->UnregisterEntity(GetEntityID());
			m_pCoverUser = nullptr;
		}
		break;
	}
	default:
		assert(0);
		break;
	}
}

void CPipeUser::SetName(const char* pName)
{
	CCCPOINT(CPipeUser_SetName);

	CAIObject::SetName(pName);
	char name[256];

	CAIObject* pRefPoint = m_refRefPoint.GetAIObject();
	if (pRefPoint)
	{
		cry_sprintf(name, "%s_RefPoint", pName);
		pRefPoint->SetName(name);
	}

	CAIObject* pLookAtTarget = m_refLookAtTarget.GetAIObject();
	if (pLookAtTarget)
	{
		cry_sprintf(name, "%s_LookAtTarget", pName);
		pLookAtTarget->SetName(name);
	}
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::ProcessBranchGoal(QGoal& Goal, bool& blocking)
{
	if (Goal.op != eGO_BRANCH)
		return false;

	blocking = Goal.bBlocking;

	bool bNot = (Goal.params.nValue & NOT) != 0;

	if (GetBranchCondition(Goal) ^ bNot)
	{
		// string labels are already converted to integer relative offsets
		// TODO: cut the string version of Jump in a few weeks
		if (Goal.params.str.empty())
			blocking |= m_pCurrentGoalPipe->Jump(Goal.params.nValueAux);
		else
			blocking |= m_pCurrentGoalPipe->Jump(Goal.params.str);
	}

	return true;
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::GetBranchCondition(QGoal& Goal)
{
	int branchType = Goal.params.nValue & (~NOT);

	// Fetch attention target, because it's used in many cases
	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();

	switch (branchType)
	{
	case IF_RANDOM:
		{
			if (cry_random(0.0f, 1.0f) <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_NO_PATH:
		{
			if (m_nPathDecision == PATHFINDER_NOPATH)
				return true;
		}
		break;
	case IF_PATH_STILL_FINDING:
		{
			if (m_nPathDecision == PATHFINDER_STILLFINDING)
				return true;
		}
		break;
	case IF_IS_HIDDEN:  // branch if already at hide spot
		{
			if (!m_CurrentHideObject.IsValid())
				return false;
			Vec3 diff(m_CurrentHideObject.GetLastHidePos() - GetPos());
			diff.z = 0.f;
			if (diff.len2() < Goal.params.fValue * Goal.params.fValue)
				return true;
		}
		break;
	case IF_CAN_HIDE: // branch if hide spot was found
		{
			if (m_CurrentHideObject.IsValid())
				return true;
		}
		break;
	case IF_CANNOT_HIDE:
		{
			if (!m_CurrentHideObject.IsValid())
				return true;
		}
		break;
	case IF_STANCE_IS:  // branch if stance is equal to params.fValue
		{
			if (m_State.bodystate == Goal.params.fValue)
				return true;
		}
		break;
	case IF_HAS_FIRED:  // jumps if the PipeUser just fired - fire flag passed to actor
		{
			if (m_State.fire)
				return true;
		}
		break;
	case IF_FIRE_IS:  // branch if last "firecmd" argument was equal to params.fValue
		{
			bool state = Goal.params.fValue > 0.5f;
			if (AllowedToFire() == state)
				return true;
		}
		break;
	case IF_NO_LASTOP:
		{
			if (m_refLastOpResult.IsNil() || !m_refLastOpResult.GetAIObject())
				return true;
		}
		break;
	case IF_SEES_LASTOP:
		{
			CAIObject* pLastOpResult = m_refLastOpResult.GetAIObject();
			if (pLastOpResult && GetAISystem()->CheckObjectsVisibility(this, pLastOpResult, Goal.params.fValue))
				return true;
		}
		break;
	case IF_SEES_TARGET:
		{
			if (Goal.params.fValueAux >= 0.0f)
			{
				if (pAttentionTarget)
				{
					CCCPOINT(CPipeUser_GetBranchCondition_IF_SEES_TARGET);
					SAIBodyInfo bi;

					if (GetProxy() && GetProxy()->QueryBodyInfo(SAIBodyInfoQuery((EStance)(int)Goal.params.fValueAux, 0.0f, 0.0f, false), bi))
					{
						const Vec3& pos = bi.vEyePos;
						Vec3 dir = pAttentionTarget->GetPos() - pos;
						if (dir.GetLengthSquared() > sqr(Goal.params.fValue))
							dir.SetLength(Goal.params.fValue);

						if (CanSee(pAttentionTarget->GetVisionID()))
							return true;
					}
				}
			}
			{
				if (pAttentionTarget && GetAISystem()->CheckObjectsVisibility(this, pAttentionTarget, Goal.params.fValue))
					return true;
			}
		}
		break;
	case IF_TARGET_LOST_TIME_MORE:
		{
			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet && pPuppet->m_targetLostTime > Goal.params.fValue)
				return true;
		}
		break;

	case IF_TARGET_LOST_TIME_LESS:
		{
			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet && pPuppet->m_targetLostTime <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_EXPOSED_TO_TARGET:
		{
			if (pAttentionTarget)
			{
				CCCPOINT(CPipeUser_GetBranchCondition_IF_EXPOSED_TO_TARGET);
				Vec3 pos = GetPos();
				SAIBodyInfo bi;
				if (GetProxy() && GetProxy()->QueryBodyInfo(SAIBodyInfoQuery(STANCE_CROUCH, 0.0f, 0.0f, false), bi))
					pos = bi.vEyePos;

				Vec3 dir(pAttentionTarget->GetPos() - pos);
				dir.SetLength(Goal.params.fValue);
				float dist;
				bool exposed = !IntersectSweptSphere(0, dist, Lineseg(pos, pos + dir), Goal.params.fValueAux, AICE_ALL);
				if (exposed)
					return true;
			}
		}
		break;
	case IF_CAN_SHOOT_TARGET_PRONED:
		{
			CCCPOINT(CPipeUser_GetbranchCondition_IF_CAN_SHOOT_TARGET_PRONED);

			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet->CanFireInStance(STANCE_PRONE))
				return true;
		}
		break;
	case IF_CAN_SHOOT_TARGET_CROUCHED:
		{
			CCCPOINT(CPipeUser_GetbranchCondition_IF_CAN_SHOOT_TARGET_CROUCHED);

			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet->CanFireInStance(STANCE_CROUCH))
				return true;
		}
		break;
	case IF_CAN_SHOOT_TARGET_STANDING:
		{
			CCCPOINT(CPipeUser_GetbranchCondition_IF_CAN_SHOOT_TARGET_STANDING);

			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet->CanFireInStance(STANCE_STAND))
				return true;
		}
		break;
	case IF_CAN_SHOOT_TARGET:
		{
			Vec3 pos, dir;
			CPuppet* pPuppet = CastToCPuppet();
			bool aimOK = GetAimState() != AI_AIM_OBSTRUCTED;
			if (pAttentionTarget && pPuppet && pPuppet->GetFirecommandHandler() &&
			    aimOK &&
			    pPuppet->CanAimWithoutObstruction(pAttentionTarget->GetPos()) &&
			    pPuppet->GetFirecommandHandler()->ValidateFireDirection(pAttentionTarget->GetPos() - GetFirePos(), false))
			{
				CCCPOINT(CPipeUser_GetBranchCondition_IF_CAN_SHOOT_TARGET);
				return true;
			}
		}
		break;
	case IF_CAN_MELEE:
		{
			SAIWeaponInfo weaponInfo;
			GetProxy()->QueryWeaponInfo(weaponInfo);
			if (weaponInfo.canMelee)
				return true;
		}
		break;
	case IF_TARGET_DIST_LESS:
	case IF_TARGET_DIST_GREATER:
	case IF_TARGET_IN_RANGE:
	case IF_TARGET_OUT_OF_RANGE:
		{
			CCCPOINT(CPipeUser_GetBranchCondition_IF_TARGET_A);
			if (pAttentionTarget)
			{
				const Vec3& vPos = GetPos();
				const Vec3& vTargetPos = pAttentionTarget->GetPos();
				float fDist = Distance::Point_Point(vPos, vTargetPos);
				if (branchType == IF_TARGET_DIST_LESS)
				{
					return (fDist <= Goal.params.fValue);
				}
				if (branchType == IF_TARGET_DIST_GREATER)
				{
					return (fDist > Goal.params.fValue);
				}
				if (branchType == IF_TARGET_IN_RANGE)
				{
					return (fDist <= m_Parameters.m_fAttackRange);
				}
				if (branchType == IF_TARGET_OUT_OF_RANGE)
				{
					return (fDist > m_Parameters.m_fAttackRange);
				}
			}
		}
		break;
	case IF_TARGET_MOVED_SINCE_START:
	case IF_TARGET_MOVED:
		{
			CGoalPipe* pLastPipe(m_pCurrentGoalPipe->GetLastSubpipe());
			if (!pLastPipe) // this should NEVER happen
			{
				AIError("CPipeUser::ProcessBranchGoal can get pipe. User: %s", GetName());
				return true;
			}

			if (pAttentionTarget)
			{
				CCCPOINT(CPipeUser_GetBranchCondition_IF_TARGET_MOVED);
				bool ret = Distance::Point_Point(pLastPipe->GetAttTargetPosAtStart(), pAttentionTarget->GetPos()) > Goal.params.fValue;
				if (branchType == IF_TARGET_MOVED)
					pLastPipe->SetAttTargetPosAtStart(pAttentionTarget->GetPos());
				return ret;
			}
		}
		break;
	case IF_NO_ENEMY_TARGET:
		{
			CCCPOINT(CPipeUser_GetBranchCondition_IF_NO_ENEMY_TARGET);
			if (!pAttentionTarget || !IsHostile(pAttentionTarget))
				return true;
		}
		break;
	case IF_PATH_LONGER:  // branch if current path is longer than params.fValue
		{
			float dbgPathLength(m_Path.GetPathLength(false));
			if (dbgPathLength > Goal.params.fValue)
				return true;
		}
		break;
	case IF_PATH_SHORTER:
		{
			float dbgPathLength(m_Path.GetPathLength(false));
			if (dbgPathLength <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_PATH_LONGER_RELATIVE: // branch if current path is longer than (params.fValue) times the distance to destination
		{
			Vec3 pathDest(m_Path.GetParams().end);
			float pathLength = m_Path.GetPathLength(false);
			float dist = Distance::Point_Point(GetPos(), pathDest);
			if (pathLength >= dist * Goal.params.fValue)
				return true;
		}
		break;
	case IF_COVER_COMPROMISED:
	case IF_COVER_NOT_COMPROMISED:  // jumps if the current cover cannot be used for hiding or if the hide spots does not exists.
		{
			CCCPOINT(CPipeUser_GetBranchCondition_IF_COVER_COMPROMISED);
			bool bCompromised = true;
			if (pAttentionTarget)
				bCompromised = m_CurrentHideObject.IsCompromised(this, pAttentionTarget->GetPos());

			bool bResult = bCompromised;
			if (Goal.params.nValue == IF_COVER_NOT_COMPROMISED)
				bResult = !bResult;

			if (bResult)
				return true;
		}
		break;
	case IF_COVER_FIRE_ENABLED: // branch if cover firemode is  enabled
		{
			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet)
				return !pPuppet->IsCoverFireEnabled();
		}
		break;
	case IF_COVER_SOFT:
		{
			bool isEmptyCover = m_CurrentHideObject.IsCoverPathComplete() && m_CurrentHideObject.GetCoverWidth(true) < 0.1f;
			bool soft = !m_CurrentHideObject.IsObjectCollidable() || isEmptyCover;
			if (soft)
				return true;
		}
		break;
	case IF_COVER_NOT_SOFT:
		{
			bool isEmptyCover = m_CurrentHideObject.IsCoverPathComplete() && m_CurrentHideObject.GetCoverWidth(true) < 0.1f;
			bool soft = !m_CurrentHideObject.IsObjectCollidable() || isEmptyCover;
			if (soft)
				return true;
		}
		break;

	case IF_LASTOP_FAILED:
		{
			if (m_pCurrentGoalPipe && m_pCurrentGoalPipe->GetLastResult() == eGOR_FAILED)
				return true;
		}
		break;

	case IF_LASTOP_SUCCEED:
		{
			if (m_pCurrentGoalPipe && m_pCurrentGoalPipe->GetLastResult() == eGOR_SUCCEEDED)
				return true;
		}
		break;

	case IF_LASTOP_DIST_LESS:
		{
			CAIObject* pLastOpResult = m_refLastOpResult.GetAIObject();
			if (pLastOpResult && (Distance::Point_Point(GetPos(), pLastOpResult->GetPos()) < Goal.params.fValue))
			{
				return true;
			}
		}
		break;

	case IF_LASTOP_DIST_LESS_ALONG_PATH:
		{
			CAIObject* pLastOpResult = m_refLastOpResult.GetAIObject();
			if (pLastOpResult && (m_nPathDecision == PATHFINDER_PATHFOUND))
			{
				CPuppet* pPuppet = CastToCPuppet();
				if (pPuppet)
				{
					const Vec3& vMyPos = GetPos();
					float dist = m_movementAbility.b3DMove ? Distance::Point_Point(vMyPos, pLastOpResult->GetPos())
					             : Distance::Point_Point2D(vMyPos, pLastOpResult->GetPos());
					float distPath = pPuppet->GetDistanceAlongPath(pLastOpResult->GetPos(), true);
					if (dist < distPath)
					{
						dist = distPath;
					}
					if (dist < Goal.params.fValue)
					{
						return true;
					}
				}
			}
		}
		break;

	case BRANCH_ALWAYS: // branches always
		return true;
		break;

	case IF_ACTIVE_GOALS_HIDE:
		if (!m_vActiveGoals.empty() || !m_CurrentHideObject.IsValid())
			return true;
		break;

	default://IF_ACTIVE_GOALS
		if (!m_vActiveGoals.empty())
			return true;
		break;
	}

	return false;
}

void CPipeUser::Update(EUpdateType type)
{
	IAIActorProxy* pAIActorProxy = GetProxy();

	if (!CastToCPuppet())
	{
		if (!IsEnabled())
		{
			AIWarning("CPipeUser::Update: Trying to update disabled Puppet: %s", GetName());
			AIAssert(0);
			return;
		}

		// There should never be Pipe Users without proxies.
		if (!pAIActorProxy)
		{
			AIWarning("CPipeUser::Update: Pipe User does not have proxy: %s", GetName());
			AIAssert(0);
			return;
		}
		// There should never be Pipe Users without physics.
		if (!GetPhysics())
		{
			AIWarning("CPipeUser::Update: Pipe User does not have physics: %s", GetName());
			AIAssert(0);
			return;
		}
		// dead Pipe Users should never be updated
		if (pAIActorProxy->IsDead())
		{
			AIWarning("CPipeUser::Update: Trying to update dead Pipe User: %s ", GetName());
			AIAssert(0);
			return;
		}
	}

	CAIActor::Update(type);

	UpdateCovers(type, gEnv->pTimer->GetFrameTime());

	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();

	if (type == EUpdateType::Full)
	{
		if (m_CurrentHideObject.IsValid())
		{
			m_CurrentHideObject.Update(this);

			if (pAttentionTarget && m_CurrentHideObject.IsCompromised(this, pAttentionTarget->GetPos()))
				SetCoverCompromised();
		}

		// Update some special objects if they have been recently used.

		Vec3 vProbTargetPos = ZERO;

		CAIObject* pSpecialAIObject = m_refSpecialObjects[AISPECIAL_PROBTARGET_IN_TERRITORY].GetAIObject();
		if (pSpecialAIObject && pSpecialAIObject->m_bTouched)
		{
			pSpecialAIObject->m_bTouched = false;
			if (vProbTargetPos.IsZero())
				vProbTargetPos = GetProbableTargetPosition();
			Vec3 vPos = vProbTargetPos;
			// Clamp the point to the territory shape.
			if (SShape* pShape = GetTerritoryShape())
				pShape->ConstrainPointInsideShape(vPos, true);

			pSpecialAIObject->SetPos(vPos);
		}

		pSpecialAIObject = m_refSpecialObjects[AISPECIAL_PROBTARGET_IN_REFSHAPE].GetAIObject();
		if (pSpecialAIObject && pSpecialAIObject->m_bTouched)
		{
			pSpecialAIObject->m_bTouched = false;
			if (vProbTargetPos.IsZero())
				vProbTargetPos = GetProbableTargetPosition();
			Vec3 vPos = vProbTargetPos;
			// Clamp the point to ref shape
			if (SShape* pShape = GetRefShape())
				pShape->ConstrainPointInsideShape(vPos, true);

			pSpecialAIObject->SetPos(vPos);
		}

		pSpecialAIObject = m_refSpecialObjects[AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE].GetAIObject();
		if (pSpecialAIObject && pSpecialAIObject->m_bTouched)
		{
			pSpecialAIObject->m_bTouched = false;
			if (vProbTargetPos.IsZero())
				vProbTargetPos = GetProbableTargetPosition();
			Vec3 vPos = vProbTargetPos;
			// Clamp the point to ref shape
			if (SShape* pShape = GetRefShape())
				pShape->ConstrainPointInsideShape(vPos, true);
			// Clamp the point to the territory shape.
			if (SShape* pShape = GetTerritoryShape())
				pShape->ConstrainPointInsideShape(vPos, true);

			pSpecialAIObject->SetPos(vPos);
		}

		pSpecialAIObject = m_refSpecialObjects[AISPECIAL_ATTTARGET_IN_REFSHAPE].GetAIObject();

		if (pSpecialAIObject && pSpecialAIObject->m_bTouched)
		{
			pSpecialAIObject->m_bTouched = false;
			if (pAttentionTarget)
			{
				Vec3 vPos = pAttentionTarget->GetPos();
				// Clamp the point to ref shape
				if (SShape* pShape = GetRefShape())
					pShape->ConstrainPointInsideShape(vPos, true);
				// Update pos
				pSpecialAIObject->SetPos(vPos);
			}
			else
			{
				pSpecialAIObject->SetPos(GetPos());
			}
		}

		pSpecialAIObject = m_refSpecialObjects[AISPECIAL_ATTTARGET_IN_TERRITORY].GetAIObject();
		if (pSpecialAIObject && pSpecialAIObject->m_bTouched)
		{
			pSpecialAIObject->m_bTouched = false;
			if (pAttentionTarget)
			{
				Vec3 vPos = pAttentionTarget->GetPos();
				// Clamp the point to ref shape
				if (SShape* pShape = GetTerritoryShape())
					pShape->ConstrainPointInsideShape(vPos, true);
				// Update pos
				pSpecialAIObject->SetPos(vPos);
			}
			else
			{
				pSpecialAIObject->SetPos(GetPos());
			}
		}

		pSpecialAIObject = m_refSpecialObjects[AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE].GetAIObject();
		if (pSpecialAIObject && pSpecialAIObject->m_bTouched)
		{
			pSpecialAIObject->m_bTouched = false;
			if (pAttentionTarget)
			{
				Vec3 vPos = pAttentionTarget->GetPos();
				// Clamp the point to ref shape
				if (SShape* pShape = GetRefShape())
					pShape->ConstrainPointInsideShape(vPos, true);
				// Clamp the point to the territory shape.
				if (SShape* pShape = GetTerritoryShape())
					pShape->ConstrainPointInsideShape(vPos, true);

				pSpecialAIObject->SetPos(vPos);
			}
			else
			{
				pSpecialAIObject->SetPos(GetPos());
			}
		}
	}

	// make sure to update direction when entity is not moved
	const SAIBodyInfo& bodyInfo = GetBodyInfo();

	SetPos(bodyInfo.vEyePos);
	SetBodyDir(bodyInfo.GetBodyDir());
	SetEntityDir(bodyInfo.vEntityDir);
	SetMoveDir(bodyInfo.vMoveDir);
	SetViewDir(bodyInfo.GetEyeDir());

	// Handle navigational smart object start and end states before executing goalpipes.
	if (m_eNavSOMethod == nSOmSignalAnimation || m_eNavSOMethod == nSOmActionAnimation)
	{
		if (m_currentNavSOStates.IsEmpty())
		{
			if (m_State.curActorTargetPhase == eATP_Started || m_State.curActorTargetPhase == eATP_StartedAndFinished)
			{
				// Copy the set state to the currently used state.
				m_currentNavSOStates = m_pendingNavSOStates;
				m_pendingNavSOStates.Clear();

				// keep track of last used smart object
				m_idLastUsedSmartObject = m_currentNavSOStates.objectEntId;

				SetSignal(1, "OnEnterNavSO");
			}
		}

		if (m_State.curActorTargetPhase == eATP_StartedAndFinished || m_State.curActorTargetPhase == eATP_Finished)
		{
			// modify smart object states
			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_currentNavSOStates.objectEntId);
			gAIEnv.pSmartObjectManager->ModifySmartObjectStates(GetEntity(), m_currentNavSOStates.sAnimationDoneUserStates);
			if (pObjectEntity)
				gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pObjectEntity, m_currentNavSOStates.sAnimationDoneObjectStates);

			m_currentNavSOStates.Clear();
			m_eNavSOMethod = nSOmNone;

			SetSignal(1, "OnLeaveNavSO");
		}
	}

	if (!CastToCPuppet())
	{
		//m_State.Reset();

		if (type != EUpdateType::Full)
		{
			//if approaching then always update
			for (size_t i = 0; i < m_vActiveGoals.size(); i++)
			{
				QGoal& Goal = m_vActiveGoals[i];
				Goal.pGoalOp->ExecuteDry(this);
			}
		}
		else
		{
			GetStateFromActiveGoals(m_State);

			m_lightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);
		}

		HandleNavSOFailure();
		SyncActorTargetPhaseWithAIProxy();

		m_State.vBodyTargetDir = m_vBodyTargetDir;
		m_State.vDesiredBodyDirectionAtTarget = m_vDesiredBodyDirectionAtTarget;
		m_State.movementContext = m_movementContext;

		CAIObject* pAttTarget = m_refAttentionTarget.GetAIObject();
		if (pAttTarget && pAttTarget->IsEnabled())
		{
			if (CanSee(pAttTarget->GetVisionID()))
			{
				m_AttTargetType = m_State.eTargetType = AITARGET_VISUAL;
				m_State.nTargetType = pAttTarget->GetType();
				m_State.bTargetEnabled = true;
			}
			else
			{
				switch (m_State.eTargetType)
				{
				case AITARGET_VISUAL:
					m_AttTargetThreat = m_State.eTargetThreat = AITHREAT_AGGRESSIVE;
					m_AttTargetType = m_State.eTargetType = AITARGET_MEMORY;
					m_State.nTargetType = pAttTarget->GetType();
					m_State.bTargetEnabled = true;
					m_stimulusStartTime = GetAISystem()->GetFrameStartTimeSeconds();
					break;

				case AITARGET_MEMORY:
				case AITARGET_SOUND:
					if (GetAISystem()->GetFrameStartTimeSeconds() - m_stimulusStartTime >= 5.f)
					{
						m_State.nTargetType = -1;
						m_State.bTargetEnabled = false;
						m_State.eTargetThreat = AITHREAT_NONE;
						m_AttTargetType = m_State.eTargetType = AITARGET_NONE;

						SetAttentionTarget(NILREF);
					}
					break;
				}
			}
		}
		else
		{
			if ((m_State.nTargetType != -1) ||
			    m_State.bTargetEnabled ||
			    (m_State.eTargetThreat != AITHREAT_NONE) ||
			    (m_State.eTargetType != AITARGET_NONE))
			{
				SetAttentionTarget(NILREF);
			}

			m_State.nTargetType = -1;
			m_State.bTargetEnabled = false;
			m_State.eTargetThreat = AITHREAT_NONE;
			m_State.eTargetType = AITARGET_NONE;
		}

		//--------------------------------------------------------
		// Update the look logic, unless AI is paused for things like communications.
		if (m_bEnableUpdateLookTarget)
		{
			if (!m_paused)
			{
				UpdateLookTarget(pAttentionTarget);
			}
		}
		else
		{
			m_State.vLookTargetPos.zero();
		}
	}
}

bool CPipeUser::IsAdjustingAim() const
{
	return m_adjustingAim;
}

void CPipeUser::SetAdjustingAim(bool adjustingAim)
{
	m_adjustingAim = adjustingAim;
}

void CPipeUser::SetCoverState(const ICoverUser::StateFlags& state)
{
	if (!m_pCoverUser)
		return;
	
	ICoverUser::StateFlags currentState = m_pCoverUser->GetState();
	m_pCoverUser->SetState(state);
	
	if (state.Check(ICoverUser::EStateFlags::InCover))
	{
		CRY_ASSERT(currentState.Check(ICoverUser::EStateFlags::MovingToCover));
		if (!currentState.Check(ICoverUser::EStateFlags::InCover))
		{
			SetSignal(1, "OnEnterCover", 0, 0, gAIEnv.SignalCRCs.m_nOnEnterCover);
		}
	}
	else if (state.Check(ICoverUser::EStateFlags::MovingToCover))
	{
		CRY_ASSERT(!currentState.Check(ICoverUser::EStateFlags::InCover));
		if (!currentState.Check(ICoverUser::EStateFlags::MovingToCover))
		{
			SetSignal(1, "OnMovingToCover", 0, 0, gAIEnv.SignalCRCs.m_nOnMovingToCover);
			if (state.Check(ICoverUser::EStateFlags::InCover))
			{
				SetSignal(1, "OnMovingInCover", 0, 0, gAIEnv.SignalCRCs.m_nOnMovingInCover);
			}
		}
	}
	else if (state.IsEmpty())
	{
		if (currentState.Check(ICoverUser::EStateFlags::InCover))
		{
			SetCoverID(CoverID());
			ResetBodyTargetDir();
			SetSignal(1, "OnLeaveCover", 0, 0, gAIEnv.SignalCRCs.m_nOnLeaveCover);
		}
	}
}

bool CPipeUser::IsInCover() const
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return false;

	return m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::InCover);
}

bool CPipeUser::IsMovingToCover() const
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return false;

	return m_pCoverUser->GetState().Check(ICoverUser::EStateFlags::MovingToCover);
}

bool CPipeUser::IsMovingInCover() const
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return false;

	return m_pCoverUser->GetState().CheckAll({ ICoverUser::EStateFlags::MovingToCover, ICoverUser::EStateFlags::InCover });
}

void CPipeUser::SetMovingToCover(bool bMovingInCover)
{
	ICoverUser::StateFlags state = ICoverUser::EStateFlags::MovingToCover;
	if (bMovingInCover)
	{
		state.Add(ICoverUser::EStateFlags::InCover);
	}
	SetCoverState(state);
}

void CPipeUser::SetInCover(bool inCover)
{
	SetCoverState(inCover ? ICoverUser::EStateFlags::InCover : ICoverUser::EStateFlags::None);
}

void CPipeUser::SetCoverCompromised()
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return;

	if (!m_pCoverUser->GetState().IsEmpty())
	{
		SetSignal(1, "OnCoverCompromised", 0, 0, gAIEnv.SignalCRCs.m_nOnCoverCompromised);

		if (CoverID coverID = m_pCoverUser->GetCoverID())
			SetCoverBlacklisted(coverID, true, 10.0f);

		SetCoverState(ICoverUser::EStateFlags::None);

		m_pCoverUser->SetCoverID(CoverID());
	}
}

void CPipeUser::SetCoverInvalidated(CoverID coverID, ICoverUser* pCoverUser)
{
	CRY_ASSERT(m_pCoverUser == pCoverUser);
	if (!m_pCoverUser)
		return;
	
	SetSignal(1, "OnCoverCompromised", 0, 0, gAIEnv.SignalCRCs.m_nOnCoverCompromised);

	SetCoverState(ICoverUser::EStateFlags::None);

	if (m_pCoverUser->GetCoverID() == coverID)
	{
		m_pCoverUser->SetCoverID(CoverID());
	}
	else if (m_pCoverUser->GetNextCoverID() == coverID)
	{
		m_pCoverUser->SetNextCoverID(CoverID());
	}

	SetCoverBlacklisted(coverID, true, 10.0f);
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::IsCoverCompromised() const
{
	if (gAIEnv.CVars.CoverSystem)
	{
		return m_pCoverUser ? m_pCoverUser->IsCompromised() : false;
	}
	else
	{
		CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
		if (!pAttentionTarget)
			return false;

		if (m_CurrentHideObject.IsValid())
			return m_CurrentHideObject.IsCompromised(this, pAttentionTarget->GetPos());

		return true;
	}
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::IsTakingCover(float distanceThreshold) const
{
	if (IsInCover())
		return true;

	if ((distanceThreshold > 0.0f) && (IsMovingToCover()))
		return m_State.fDistanceToPathEnd < distanceThreshold;

	return false;
}

void CPipeUser::SetCoverID(const CoverID& coverID)
{
	CRY_ASSERT(m_pCoverUser);
	if (m_pCoverUser)
	{
		m_pCoverUser->SetCoverID(coverID);
	}
}

const CoverID& CPipeUser::GetCoverID() const
{
	CRY_ASSERT(m_pCoverUser);
	if (m_pCoverUser)
	{
		return m_pCoverUser->GetCoverID();
	}
	else
	{
		static CoverID invalidCoverId;
		return invalidCoverId;
	}
}

void CPipeUser::SetCoverRegister(const CoverID& coverID)
{
	CRY_ASSERT(m_pCoverUser);
	if (m_pCoverUser)
	{
		m_pCoverUser->SetNextCoverID(coverID);
	}
}

const CoverID& CPipeUser::GetCoverRegister() const
{
	CRY_ASSERT(m_pCoverUser);
	if (m_pCoverUser)
	{
		return m_pCoverUser->GetNextCoverID();
	}
	else
	{
		static CoverID invalidCoverId;
		return invalidCoverId;
	}
}

inline bool HasPointInRangeSq(const Vec3* points, uint32 pointCount, const Vec3& pos, float rangeSq)
{
	for (uint32 i = 0; i < pointCount; ++i)
	{
		if (Distance::Point_PointSq(points[i], pos) < rangeSq)
			return true;
	}

	return false;
}

void CPipeUser::UpdateCoverEyesWithTarget(const CAIObject* pTarget, const Vec3& targetPosition)
{
	CRY_ASSERT(m_pCoverUser);

	m_pCoverEyesTargetObject = pTarget;
	m_coverEyesTargetPosition = targetPosition;

	m_pCoverUser->UpdateCoverEyes();
}

void CPipeUser::FillCoverEyes(DynArray<Vec3>& eyesContainer)
{
	const uint32 kMaxEyesCount = 8;

	const CAIObject* pTarget = m_refAttentionTarget.GetAIObject();
	Vec3 targetPosition = ZERO;
	if (m_pCoverEyesTargetObject || !m_coverEyesTargetPosition.IsZero())
	{
		pTarget = m_pCoverEyesTargetObject;
		targetPosition = m_coverEyesTargetPosition;

		m_pCoverEyesTargetObject = nullptr;
		m_coverEyesTargetPosition = ZERO;
	}
	else if(pTarget)
	{
		targetPosition = pTarget->GetPos();
	}
	else
	{
		return;
	}

	if (!targetPosition.IsZero())
		eyesContainer.push_back(targetPosition);

	if (!pTarget || eyesContainer.size() >= kMaxEyesCount)
		return;

	const bool prediction = gAIEnv.CVars.CoverPredictTarget > 0.001f;
	const float RangeThresholdSq = sqr(0.5f);

	if (prediction && pTarget->IsAgent())
	{
		const Vec3 targetVelocity = pTarget->GetVelocity();
		const Vec3 futureLocation = eyesContainer.back() + targetVelocity * gAIEnv.CVars.CoverPredictTarget;

		if (!HasPointInRangeSq(eyesContainer.data(), eyesContainer.size(), futureLocation, RangeThresholdSq))
		{
			eyesContainer.push_back(futureLocation);
		}
	}

	if (eyesContainer.size() >= kMaxEyesCount)
		return;

	const CPuppet* pPuppet = CastToCPuppet();
	if (!pPuppet)
		return;

	tAIObjectID targetID = pTarget->GetAIObjectID();
	tAIObjectID targetAssociationID = pTarget->GetAssociation().GetObjectID();

	const uint32 MaxTargets = 8;
	tAIObjectID targets[MaxTargets];

	uint32 targetCount = pPuppet->GetBestTargets(targets, MaxTargets);

	for (uint32 i = 0; i < targetCount; ++i)
	{
		tAIObjectID nextTargetID = targets[i];
		if(nextTargetID == targetID || nextTargetID == targetAssociationID)
			continue;

		IAIObject* nextTargetObject = gAIEnv.pObjectContainer->GetAIObject(nextTargetID);
		if(!nextTargetObject || !CanSee(nextTargetObject->GetVisionID()))
			continue;

		Vec3 location = nextTargetObject->GetPos();
		if (!HasPointInRangeSq(eyesContainer.data(), eyesContainer.size(), location, RangeThresholdSq))
		{
			eyesContainer.push_back(location);
			if (eyesContainer.size() >= kMaxEyesCount)
				return;
		}

		if (prediction && nextTargetObject->IsAgent())
		{
			Vec3 targetVelocity = nextTargetObject->GetVelocity();
			Vec3 futureLocation = eyesContainer.back() + targetVelocity * gAIEnv.CVars.CoverPredictTarget;

			if (!HasPointInRangeSq(eyesContainer.data(), eyesContainer.size(), futureLocation, RangeThresholdSq))
			{
				eyesContainer.push_back(futureLocation);
				if (eyesContainer.size() >= kMaxEyesCount)
					return;
			}
		}
	}
}

void CPipeUser::UpdateCovers(EUpdateType type, float updateTime)
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return;

	if (!m_pCoverUser->GetState().IsEmpty() && m_pCoverUser->GetCoverID())
	{
		const Vec3& coverNormal = m_pCoverUser->GetCoverNormal(GetPhysicsPos());
		const Vec3& coverPos = m_pCoverUser->GetCoverLocation(m_pCoverUser->GetCoverID());
		SetBodyTargetDir(-coverNormal); // TODO: Remove this if old cover alignment method is not needed anymore
		m_State.coverRequest.SetCoverLocation(coverPos, -coverNormal);
	}
}

void CPipeUser::SetCoverBlacklisted(const CoverID& coverID, bool blacklist, float time)
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return;

	m_pCoverUser->SetCoverBlacklisted(coverID, blacklist, time);
}

bool CPipeUser::IsCoverBlacklisted(const CoverID& coverID) const
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return false;
	
	return m_pCoverUser->IsCoverBlackListed(coverID);
}

CoverHeight CPipeUser::CalculateEffectiveCoverHeight() const
{
	CRY_ASSERT(m_pCoverUser);
	if (!m_pCoverUser)
		return HighCover;

	const float effectiveHeight = m_pCoverUser->CalculateEffectiveHeightAt(GetPhysicsPos(), m_pCoverUser->GetCoverID());
	return (effectiveHeight >= m_Parameters.effectiveHighCoverHeight) ? HighCover : LowCover;
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::ProcessRandomGoal(QGoal& Goal, bool& blocking)
{
	if (Goal.op != eGO_RANDOM)
		return false;

	blocking = true;
	if (Goal.params.fValue > cry_random(0, 99)) // 0 - never jump // 100 - always jump
		m_pCurrentGoalPipe->Jump(Goal.params.nValue);

	return true;
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::ProcessClearGoal(QGoal& Goal, bool& blocking)
{
	if (Goal.op != eGO_CLEAR)
		return false;

	ClearActiveGoals();

	//Luciano - CLeader should manage the release of formation
	//					GetAISystem()->FreeFormationPoint(m_Parameters.m_nGroup,this);
	if (Goal.params.fValue)
	{
		SetAttentionTarget(NILREF);
		ClearPotentialTargets();
	}

	blocking = false;

	return true;
}

void CPipeUser::SetNavSOFailureStates()
{
	if (m_eNavSOMethod != nSOmNone)
	{
		if (!m_currentNavSOStates.IsEmpty())
		{
			gAIEnv.pSmartObjectManager->ModifySmartObjectStates(GetEntity(), m_currentNavSOStates.sAnimationFailUserStates);
			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_currentNavSOStates.objectEntId);
			if (pObjectEntity)
				gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pObjectEntity, m_currentNavSOStates.sAnimationFailObjectStates);
			m_currentNavSOStates.Clear();
		}

		if (!m_pendingNavSOStates.IsEmpty())
		{
			gAIEnv.pSmartObjectManager->ModifySmartObjectStates(GetEntity(), m_pendingNavSOStates.sAnimationFailUserStates);
			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_pendingNavSOStates.objectEntId);
			if (pObjectEntity)
				gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pObjectEntity, m_pendingNavSOStates.sAnimationFailObjectStates);
			m_pendingNavSOStates.Clear();
		}
	}

	AIAssert(m_pendingNavSOStates.IsEmpty() && m_currentNavSOStates.IsEmpty());

	// Stop exact positioning.
	m_State.actorTargetReq.Reset();

	m_eNavSOMethod = nSOmNone;
}

void CPipeUser::ClearPath(const char* dbgString)
{
	SetNavSOFailureStates();

	// Don't change the pathfinder result since it has to reflect the result of
	// the last pathfind. If it is set to NoPath then flowgraph AIGoto nodes will
	// fail at the end of the path.
	m_Path.Clear(dbgString);
	m_OrigPath.Clear(dbgString);
	m_refPathFindTarget.Reset();
}

//
//--------------------------------------------------------------------------------------------------------
void CPipeUser::GetStateFromActiveGoals(SOBJECTSTATE& state)
{

#ifdef _DEBUG
	// Reset the movement reason at each update, it will be setup by the correct goal operation.
	m_DEBUGmovementReason = AIMORE_UNKNOWN;
#endif

	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_bFirstUpdate)
	{
		m_bFirstUpdate = false;
		SetLooseAttentionTarget(NILREF);
	}

	if (IsPaused())
		return;

	m_DeferredActiveGoals.clear();

	m_pipeExecuting = true;

	if (m_pCurrentGoalPipe)
	{
		if (m_pCurrentGoalPipe->CountSubpipes() >= 10)
		{
			AIWarning("%s has too many (%d) subpipes. Pipe <%s>", GetName(), m_pCurrentGoalPipe->CountSubpipes(),
			          m_pCurrentGoalPipe->GetName());
		}

		if (!m_bBlocked)  // if goal queue not blocked
		{
			// (MATT) Track whether we will add this to the active goals, executed in subsequent frames {2009/10/14}
			bool doNotExecuteAgain = false;

			QGoal Goal;

			EPopGoalResult pgResult = m_pCurrentGoalPipe->PeekPopGoalResult();
			bool isLoop = m_pCurrentGoalPipe->IsLoop();

			if ((pgResult == ePGR_AtEnd))
			{
				if (isLoop)
				{
					m_pCurrentGoalPipe->Reset();

					ClearActiveGoals();
					if (GetAttentionTarget())
						m_pCurrentGoalPipe->SetAttTargetPosAtStart(GetAttentionTarget()->GetPos());
				}
				else
				{
					NotifyListeners(m_pCurrentGoalPipe, ePN_Exiting);
				}
			}

			while ((pgResult = m_pCurrentGoalPipe->PopGoal(Goal, this)) == ePGR_Succeed)
			{
				// Each Process instruction first checks to see if this goal is of the right type, immediately returning false if not
				bool blocking = false;

				if (ProcessBranchGoal(Goal, blocking))
				{
					// Now, having either taken the branch or not:
					if (blocking)
					{
						doNotExecuteAgain = true;
						break; // blocking - don't execute the next instruction until next frame
					}
					continue; // non-blocking - continue with execution immediately
				}

				if (ProcessRandomGoal(Goal, blocking))  // RandomJump would be a better name
				{
					doNotExecuteAgain = blocking;
					break;
				}

				if (ProcessClearGoal(Goal, blocking))
				{
					doNotExecuteAgain = blocking;
					break;
				}

				EGoalOpResult result = Goal.pGoalOp->Execute(this);
				doNotExecuteAgain = false;

				if (result == eGOR_IN_PROGRESS)
				{
					if (Goal.bBlocking)
					{
						break;
					}
					else
					{
						m_DeferredActiveGoals.push_back(Goal);
					}
				}
				else
				{
					if (result == eGOR_DONE)
					{
						Goal.pGoalOp->Reset(this);
					}
					else
					{
						m_pCurrentGoalPipe->SetLastResult(result);
					}

					doNotExecuteAgain = (result == eGOR_FAILED) || (result == eGOR_DONE);
				}
			}

			if (pgResult != ePGR_BreakLoop)
			{
				if (!Goal.pGoalOp)
				{
					if (isLoop)
					{
						m_pCurrentGoalPipe->Reset();
						ClearActiveGoals();
						if (GetAttentionTarget())
							m_pCurrentGoalPipe->SetAttTargetPosAtStart(GetAttentionTarget()->GetPos());
					}
				}
				else
				{
					if (!doNotExecuteAgain)
					{
						//FIXME
						//todo: remove this, currently happens coz "lookat" never finishes - actor can't rotate sometimes
						if (m_DeferredActiveGoals.size() < 20)
						{
							m_DeferredActiveGoals.push_back(Goal);
						}
						else
						{
							// (MATT) Test if the lookat problem still occurs {2009/10/20}
							assert(false);
						}
						m_bBlocked = Goal.bBlocking;
					}
				}
			}
		}
	}
	if (m_vActiveGoals.size() >= 10)
	{
		const QGoal& Goal = m_vActiveGoals.back();
		AIWarning("%s has too many (%" PRISIZE_T ") active goals. Pipe <%s>; last goal <%s>", GetName(), m_vActiveGoals.size(),
		          m_pCurrentGoalPipe ? m_pCurrentGoalPipe->GetName() : "_no_pipe_",
		          Goal.op == eGO_LAST ? Goal.sPipeName.c_str() : m_pCurrentGoalPipe->GetGoalOpName(Goal.op));
		assert(m_vActiveGoals.size() < 100);
	}

	if (!m_vActiveGoals.empty())
	{
		for (size_t i = 0; i < m_vActiveGoals.size(); i++)
		{
			QGoal& Goal = m_vActiveGoals[i];

			m_lastExecutedGoalop = Goal.op;

			/*
			   ITimer *pTimer = gEnv->pTimer;
			   int val = gAIEnv.CVars.ProfileGoals;

			   CTimeValue timeLast;

			   if (val)
			   timeLast = pTimer->GetAsyncTime();
			 */
			EGoalOpResult result = Goal.pGoalOp->Execute(this);
			/*if (val)
			   {
			   CTimeValue timeCurr = pTimer->GetAsyncTime();
			   float f = (timeCurr-timeLast).GetSeconds();
			   timeLast=timeCurr;

			   const char* goalName = Goal.op == eGO_LAST ? Goal.sPipeName.c_str() : m_pCurrentGoalPipe->GetGoalOpName(Goal.op);
			   GetAISystem()->m_mapDEBUGTimingGOALS[CONST_TEMP_STRING(goalName)] = f;
			   }
			 */
			if (result != eGOR_IN_PROGRESS)
			{
				if (Goal.bBlocking)
				{
					if (result != eGOR_DONE)
						m_pCurrentGoalPipe->SetLastResult(result);
					m_bBlocked = false;
				}

				RemoveActiveGoal(i);
				if (!m_vActiveGoals.empty())
					--i;
			}
		}
	}

	m_vActiveGoals.insert(m_vActiveGoals.end(), m_DeferredActiveGoals.begin(), m_DeferredActiveGoals.end());

	if (m_bKeepMoving && m_State.vMoveDir.IsZero(.01f) && !m_vLastMoveDir.IsZero(.01f))
		m_State.vMoveDir = m_vLastMoveDir;
	else if (!m_State.vMoveDir.IsZero(.01f))
		m_vLastMoveDir = m_State.vMoveDir;

	assert(m_pipeExecuting);
	m_pipeExecuting = false;

	if (!m_delayedPipeSelection.name.empty())
	{
		const DelayedPipeSelection& delayed = m_delayedPipeSelection;
		SelectPipe(delayed.mode, delayed.name.c_str(), delayed.refArgument, delayed.goalPipeId,
		           delayed.resetAlways /*, &delayed.params*/);

		m_delayedPipeSelection = DelayedPipeSelection();
	}
}

void CPipeUser::SetLastOpResult(CWeakRef<CAIObject> refObject)
{
	m_refLastOpResult = refObject;
}

void CPipeUser::SetAttentionTarget(CWeakRef<CAIObject> refTarget)
{
	CCCPOINT(CPipeUser_SetAttentionTarget);
	CAIObject* pTarget = refTarget.GetAIObject();

	if (!pTarget)
	{
		m_AttTargetThreat = AITHREAT_NONE;
		m_AttTargetExposureThreat = AITHREAT_NONE;
		m_AttTargetType = AITARGET_NONE;
	}
	else if (m_refAttentionTarget != refTarget && m_pCurrentGoalPipe)
	{
		AIAssert(m_pCurrentGoalPipe);
		CGoalPipe* pLastPipe(m_pCurrentGoalPipe->GetLastSubpipe());
		if (pLastPipe && pTarget)
			pLastPipe->SetAttTargetPosAtStart(pTarget->GetPos());
	}

	m_refAttentionTarget = refTarget;

#ifdef CRYAISYSTEM_DEBUG
	RecorderEventData recorderEventData(pTarget != NULL ? pTarget->GetName() : "<none>");
	RecordEvent(IAIRecordable::E_ATTENTIONTARGET, &recorderEventData);
#endif
}

//====================================================================
// RequestPathTo
//====================================================================
void CPipeUser::RequestPathTo(
  const Vec3& pos
  , const Vec3& dir
  , bool allowDangerousDestination
  , int forceTargetBuildingId
  , float endTol
  , float endDistance
  , CAIObject* pTargetObject
  , const bool cutPathAtSmartObject
  , const MNMDangersFlags dangersFlags
  , const bool considerActorsAsPathObstacles)
{
	CCCPOINT(CPipeUser_RequestPathTo);

	m_cutPathAtSmartObject = cutPathAtSmartObject;
	m_considerActorsAsPathObstacles = considerActorsAsPathObstacles;
	m_OrigPath.Clear("CPipeUser::RequestPathTo m_OrigPath");
	m_nPathDecision = PATHFINDER_STILLFINDING;
	m_refPathFindTarget = GetWeakRef(pTargetObject);
	// add a slight offset to avoid starting below the floor
	Vec3 myPos = GetPhysicsPos() + Vec3(0, 0, 0.05f);
	Vec3 endDir(ZERO);
	// Check the type of the way the movement to the target, 0 = strict 1 = fastest, forget end direction.
	if (m_bPathfinderConsidersPathTargetDirection)
		endDir = dir;

	CancelRequestedPath(false);

	assert(m_queuedPathId == 0);

	const MNMPathRequest request(myPos, pos, endDir, forceTargetBuildingId, endTol, endDistance,
	                             allowDangerousDestination, functor(*this, &CPipeUser::OnMNMPathResult), GetNavigationTypeID(),
	                             dangersFlags);
	m_queuedPathId = gAIEnv.pMNMPathfinder->RequestPathTo(GetEntityID(), request);

	if (m_queuedPathId == 0)
	{
		MNMPathRequestResult result;
		HandlePathDecision(result);
	}
}

void CPipeUser::RequestPathTo(MNMPathRequest& request)
{
	CCCPOINT(CPipeUser_RequestPathTo);

	m_cutPathAtSmartObject = false;
	m_considerActorsAsPathObstacles = false;
	m_OrigPath.Clear("CPipeUser::RequestPathTo m_OrigPath");
	m_nPathDecision = PATHFINDER_STILLFINDING;
	m_refPathFindTarget.Reset();
	// add a slight offset to avoid starting below the floor

	CancelRequestedPath(false);

	assert(m_queuedPathId == 0);

	request.resultCallback = functor(*this, &CPipeUser::OnMNMPathResult);
	m_queuedPathId = gAIEnv.pMNMPathfinder->RequestPathTo(GetEntityID(), request);

	if (m_queuedPathId == 0)
	{
		MNMPathRequestResult result;
		HandlePathDecision(result);
	}
}

//===================================================================
// RequestPathInDirection
//===================================================================
void CPipeUser::RequestPathInDirection(const Vec3& pos, float distance, CWeakRef<CAIObject> refTargetObject, float endDistance)
{
	AIWarning("CPipeUser::RequestPathInDirection is currently not supported for the MNM Navigation System.");
}

//====================================================================
// GetGoalPipe
//====================================================================
CGoalPipe* CPipeUser::GetGoalPipe(const char* name)
{
	CGoalPipe* pPipe = (CGoalPipe*) gAIEnv.pPipeManager->OpenGoalPipe(name);

	if (pPipe)
		return pPipe;
	else
		return 0;
}

//====================================================================
// IsUsingPipe
//====================================================================
bool CPipeUser::IsUsingPipe(const char* name) const
{
	const CGoalPipe* pExecutingPipe = m_pCurrentGoalPipe;

	if (!pExecutingPipe)
		return false;

	while (pExecutingPipe->IsInSubpipe())
	{
		if (pExecutingPipe->GetNameAsString() == name)
			return true;
		pExecutingPipe = pExecutingPipe->GetSubpipe();
	}
	return pExecutingPipe->GetNameAsString() == name;
}

bool CPipeUser::IsUsingPipe(int goalPipeId) const
{
	const CGoalPipe* pExecutingPipe = m_pCurrentGoalPipe;
	while (pExecutingPipe)
	{
		if (pExecutingPipe->GetEventId() == goalPipeId)
			return true;
		pExecutingPipe = pExecutingPipe->GetSubpipe();
	}
	return false;
}

//====================================================================
// RemoveActiveGoal
//====================================================================
void CPipeUser::RemoveActiveGoal(int nOrder)
{
	if (m_vActiveGoals.empty())
		return;
	int size = (int)m_vActiveGoals.size();

	if (size == 1)
	{
		m_vActiveGoals.front().pGoalOp->Reset(this);
		m_vActiveGoals.clear();
		return;
	}

	m_vActiveGoals[nOrder].pGoalOp->Reset(this);

	if (nOrder != size - 1)
		m_vActiveGoals[nOrder] = m_vActiveGoals[size - 1];

	m_vActiveGoals.pop_back();
}

bool CPipeUser::AbortActionPipe(int goalPipeId)
{
	CGoalPipe* pPipe = m_pCurrentGoalPipe;
	while (pPipe && pPipe->GetEventId() != goalPipeId)
		pPipe = pPipe->GetSubpipe();
	if (!pPipe)
		return false;

	// high priority pipes are already aborted or not allowed to be aborted
	if (pPipe->IsHighPriority())
		return false;

	// aborted actions become high priority actions
	pPipe->HighPriority();

	// cancel all pipes inserted after this one unless another action is found
	while (pPipe && pPipe->GetSubpipe())
	{
		if (pPipe->GetSubpipe()->GetNameAsString() == "_action_")
			break;
		else if (pPipe->GetSubpipe()->GetEventId())
			CancelSubPipe(pPipe->GetSubpipe()->GetEventId());
		pPipe = pPipe->GetSubpipe();
	}
	return true;
}

bool CPipeUser::SelectPipe(int mode, const char* name, CWeakRef<CAIObject> refArgument, int goalPipeId, bool resetAlways, const GoalParams* node)
{
	if (m_pipeExecuting)
	{
		assert(!node); // don't support parameterized goal pipe selections
		m_delayedPipeSelection = DelayedPipeSelection(mode, name, refArgument, goalPipeId, resetAlways);

		return true;
	}

	if (IEntity* pEntity = GetEntity())
		gAIEnv.pAIActionManager->AbortAIAction(pEntity);

	refArgument.ValidateOrReset();

	m_lastExecutedGoalop = eGO_LAST;

	if (m_pCurrentGoalPipe && !resetAlways)
	{
		if (m_pCurrentGoalPipe->GetNameAsString() == name)
		{
			CCCPOINT(CPipeUser_SelectPipe_A);
			NotifyListeners(m_pCurrentGoalPipe->GetEventId(), ePN_Deselected);
			m_pCurrentGoalPipe->SetEventId(goalPipeId);
			if (refArgument.IsSet())
				m_pCurrentGoalPipe->SetRefArgument(refArgument);
			m_pCurrentGoalPipe->SetLoop((mode & AIGOALPIPE_RUN_ONCE) == 0);
			return true;
		}
	}

	CGoalPipe* pHigherPriorityOwner = NULL;
	CGoalPipe* pHigherPriority = m_pCurrentGoalPipe;
	while (pHigherPriority && !pHigherPriority->IsHighPriority())
	{
		pHigherPriorityOwner = pHigherPriority;
		pHigherPriority = pHigherPriority->GetSubpipe();
	}

	if (!pHigherPriority && refArgument.IsSet())
	{
		SetLastOpResult(refArgument);
	}

	if (!pHigherPriority || resetAlways)
	{
		CCCPOINT(CPipeUser_SelectPipe_B);

		// reset some stuff with every goal-pipe change -----------------------------------------------------------------------------------
		// strafing dist should be reset
		CPuppet* pPuppet = CastToCPuppet();
		if (pPuppet)
		{
			pPuppet->SetAllowedStrafeDistances(0, 0, false);
			pPuppet->SetAdaptiveMovementUrgency(0, 0, 0);
			pPuppet->SetDelayedStance(STANCE_NULL);
		}

		// reset some stuff here
		if (!(mode & AIGOALPIPE_DONT_RESET_AG))
		{
			if (pPuppet)
				pPuppet->GetState().coverRequest.ClearCoverAction(); //Argh!

			if (IAIActorProxy* pProxy = GetProxy())
				pProxy->ResetAGInput(AIAG_ACTION);
		}

		SetNavSOFailureStates();
	}

	CGoalPipe* pPipe = gAIEnv.pPipeManager->IsGoalPipe(name);
	if (pPipe)
	{
		IAIObject* pAttTarget = GetAttentionTarget();
		pPipe->SetAttTargetPosAtStart(pAttTarget ? pAttTarget->GetPos() : Vec3Constants<float>::fVec3_Zero);
		pPipe->SetRefArgument(refArgument);
	}
	else
	{
		AIError("SelectPipe: Goalpipe '%s' does not exists. Selecting default goalpipe '_first_' instead.", name);
		pPipe = gAIEnv.pPipeManager->IsGoalPipe("_first_");
		AIAssert(pPipe);
		pPipe->GetRefArgument().Reset();
	}

	if (!pHigherPriority || resetAlways)
		ResetCurrentPipe(resetAlways);

	CGoalPipe* prevGoalPipe = m_pCurrentGoalPipe;
	m_pCurrentGoalPipe = pPipe;   // this might be too slow, in which case we will go back to registration
	m_pCurrentGoalPipe->Reset();
	m_pCurrentGoalPipe->SetEventId(goalPipeId);
	if (node)
	{
		m_pCurrentGoalPipe->ParseParams(*node);
	}

	if (pHigherPriority && !resetAlways)
	{
		if (pHigherPriorityOwner)
		{
			pHigherPriorityOwner->SetSubpipe(NULL);

			delete prevGoalPipe;
		}

		m_pCurrentGoalPipe->SetSubpipe(pHigherPriority);
	}

	m_pCurrentGoalPipe->SetLoop((mode & AIGOALPIPE_RUN_ONCE) == 0);

	GetAISystem()->Record(this, IAIRecordable::E_GOALPIPESELECTED, name);

#ifdef CRYAISYSTEM_DEBUG
	if (pPipe)
	{
		RecorderEventData recorderEventData(name);
		RecordEvent(IAIRecordable::E_GOALPIPESELECTED, &recorderEventData);
	}
	else
	{
		string str(name);
		str += "[Not Found!]";
		RecorderEventData recorderEventData(str.c_str());
		RecordEvent(IAIRecordable::E_GOALPIPESELECTED, &recorderEventData);
	}
#endif  // #ifdef CRYAISYSTEM_DEBUG

	CCCPOINT(CPipeUser_SelectPipe_End);
	return true;
}

void CPipeUser::Pause(bool pause)
{
	if (pause)
	{
		++m_paused;
	}
	else
	{
		assert(m_paused > 0);
		if (m_paused > 0)
			--m_paused;
	}

	assert(m_paused <= 8);
}

bool CPipeUser::IsPaused() const
{
	return m_paused > 0;
}

// used by action system to remove "_action_" inserted goal pipe
// which would notify goal pipe listeners and resume previous goal pipe
bool CPipeUser::RemoveSubPipe(int goalPipeId, bool keepInserted)
{
	if (!goalPipeId)
		return false;

	if (!m_pCurrentGoalPipe)
	{
		m_notAllowedSubpipes.insert(goalPipeId);
		return false;
	}

	// find the last goal pipe in the list
	CGoalPipe* pTargetPipe = NULL;
	CGoalPipe* pPrevPipe = m_pCurrentGoalPipe;
	while (pPrevPipe->GetSubpipe())
	{
		pPrevPipe = pPrevPipe->GetSubpipe();
		if (pPrevPipe->GetEventId() == goalPipeId)
			pTargetPipe = pPrevPipe;
	}

	if (keepInserted == false && pTargetPipe != NULL)
	{
		while (CGoalPipe* pSubpipe = pTargetPipe->GetSubpipe())
		{
			if (pSubpipe->IsHighPriority())
				break;
			if (pSubpipe->GetEventId())
				CancelSubPipe(pSubpipe->GetEventId());
			else
				pTargetPipe = pSubpipe;
		}
	}

	if (m_pCurrentGoalPipe->RemoveSubpipe(this, goalPipeId, keepInserted, true))
	{
		NotifyListeners(goalPipeId, ePN_Removed);

		// find the last goal pipe in the list
		CGoalPipe* pCurrent = m_pCurrentGoalPipe;
		while (pCurrent->GetSubpipe())
			pCurrent = pCurrent->GetSubpipe();

		// only if the removed pipe was the last one in the list
		if (pCurrent != pPrevPipe)
		{
			// remove goals
			ClearActiveGoals();

			// and resume the new last goal pipe
			SetLastOpResult(pCurrent->GetRefArgument());
			NotifyListeners(pCurrent, ePN_Resumed);
		}
		return true;
	}
	else
	{
		m_notAllowedSubpipes.insert(goalPipeId);
		return false;
	}
}

// used by ai flow graph nodes to cancel inserted goal pipe
// which would notify goal pipe listeners that operation was failed
bool CPipeUser::CancelSubPipe(int goalPipeId)
{
	if (!m_pCurrentGoalPipe)
	{
		if (goalPipeId != 0)
			m_notAllowedSubpipes.insert(goalPipeId);
		return false;
	}

	CGoalPipe* pCurrent = m_pCurrentGoalPipe;
	while (pCurrent->GetSubpipe())
		pCurrent = pCurrent->GetSubpipe();
	bool bClearNeeded = !goalPipeId || pCurrent->GetEventId() == goalPipeId;

	if (CPuppet* pPuppet = CastToCPuppet())
		// enable this since it might be disabled from canceled goal pipe (and planed to enable it again at pipe end)
		pPuppet->m_bCanReceiveSignals = true;

	if (m_pCurrentGoalPipe->RemoveSubpipe(this, goalPipeId, true, true))
	{
		NotifyListeners(goalPipeId, ePN_Deselected);

		if (bClearNeeded)
		{
			ClearActiveGoals();

			// resume the parent goal pipe
			pCurrent = m_pCurrentGoalPipe;
			while (pCurrent->GetSubpipe())
				pCurrent = pCurrent->GetSubpipe();
			SetLastOpResult(pCurrent->GetRefArgument());
			NotifyListeners(pCurrent, ePN_Resumed);
		}

		return true;
	}
	else
	{
		if (goalPipeId != 0)
			m_notAllowedSubpipes.insert(goalPipeId);
		return false;
	}
}

IGoalPipe* CPipeUser::InsertSubPipe(int mode, const char* name, CWeakRef<CAIObject> refArgument, int goalPipeId, const GoalParams* node)
{
	if (!m_pCurrentGoalPipe)
		return NULL;

	if (goalPipeId != 0)
	{
		VectorSet<int>::iterator itFind = m_notAllowedSubpipes.find(goalPipeId);
		if (itFind != m_notAllowedSubpipes.end())
		{
			CCCPOINT(CPipeUser_InsertSubPipe_NotAllowed);

			m_notAllowedSubpipes.erase(itFind);
			return NULL;
		}
	}

	if (m_pCurrentGoalPipe->CountSubpipes() >= 8)
	{
		AIWarning("%s has too many (%d) subpipes. Pipe <%s>; inserting <%s>", GetName(), m_pCurrentGoalPipe->CountSubpipes(),
		          m_pCurrentGoalPipe->GetName(), name);
	}

	bool bExclusive = (mode & AIGOALPIPE_NOTDUPLICATE) != 0;
	bool bHighPriority = (mode & AIGOALPIPE_HIGHPRIORITY) != 0; // it will be not removed when a goal pipe is selected
	bool bSamePriority = (mode & AIGOALPIPE_SAMEPRIORITY) != 0; // sets the priority to be the same as active goal pipe
	bool bKeepOnTop = (mode & AIGOALPIPE_KEEP_ON_TOP) != 0;     // will keep this pipe at the top of the stack

	//	if (m_pCurrentGoalPipe->m_sName == name && !goalPipeId)
	//		return NULL;

	// first lets find the goalpipe
	CGoalPipe* pPipe = gAIEnv.pPipeManager->IsGoalPipe(name);
	if (!pPipe)
	{
		AIWarning("InsertSubPipe: Goalpipe '%s' does not exists. No goalpipe inserted.", name);

#ifdef CRYAISYSTEM_DEBUG
		string str(name);
		str += "[Not Found!]";
		RecorderEventData recorderEventData(str.c_str());
		RecordEvent(IAIRecordable::E_GOALPIPEINSERTED, &recorderEventData);
#endif  // #ifdef CRYAISYSTEM_DEBUG

		return NULL;
	}

	if (bHighPriority)
		pPipe->HighPriority();

	if (bKeepOnTop)
		pPipe->m_bKeepOnTop = true;

	// now find the innermost pipe
	CGoalPipe* pExecutingPipe = m_pCurrentGoalPipe;
	int i = 0;
	while (pExecutingPipe->IsInSubpipe())
	{
		CCCPOINT(CPipeUser_InsertSubPipe_IsInSubpipe);

		if (bExclusive && pExecutingPipe->GetNameAsString() == name)
		{
			delete pPipe;
			return NULL;
		}
		if (pExecutingPipe->GetSubpipe()->m_bKeepOnTop)
			break;
		if (!bSamePriority && !bHighPriority)
		{
			if (pExecutingPipe->GetSubpipe()->IsHighPriority())
				break;
		}
		pExecutingPipe = pExecutingPipe->GetSubpipe();
		++i;
	}

	if (i > 10)
	{
		CGoalPipe* pCurrentPipe = m_pCurrentGoalPipe;
		AIWarning("%s: More than 10 subpipes", GetName());
		int j = 0;
		while (pCurrentPipe->IsInSubpipe())
		{
			const string& sDebugName = pCurrentPipe->GetDebugName();
			AILogAlways("%d) %s", j++, sDebugName.empty() ? pCurrentPipe->GetName() : sDebugName.c_str());
			pCurrentPipe = pCurrentPipe->GetSubpipe();
		}
	}
	CCCPOINT(CPipeUser_InsertSubPipe_A);

	if (bExclusive && pExecutingPipe->GetNameAsString() == name)
	{
		delete pPipe;
		return NULL;
	}
	if (!pExecutingPipe->GetSubpipe())
	{
		if (!m_vActiveGoals.empty())
		{
			ClearActiveGoals();

			// but make sure we end up executing it again
			pExecutingPipe->ReExecuteGroup();
		}

		NotifyListeners(pExecutingPipe, ePN_Suspended);

		// (MATT) Do we actually need to check? {2009/03/13}
		if (refArgument.IsSet())
			SetLastOpResult(refArgument);

		CCCPOINT(CPipeUser_InsertSubPipe_Unblock);

		// unblock current pipe
		m_bBlocked = false;
	}

	pExecutingPipe->SetSubpipe(pPipe);

	if (GetAttentionTarget())
		pPipe->SetAttTargetPosAtStart(GetAttentionTarget()->GetPos());
	else
		pPipe->SetAttTargetPosAtStart(Vec3Constants<float>::fVec3_Zero);
	pPipe->SetRefArgument(refArgument);
	pPipe->SetEventId(goalPipeId);

	if (node)
		pPipe->ParseParams(*node);

	NotifyListeners(pPipe, ePN_Inserted);

	GetAISystem()->Record(this, IAIRecordable::E_GOALPIPEINSERTED, name);

#ifdef CRYAISYSTEM_DEBUG
	RecorderEventData recorderEventData(name);
	RecordEvent(IAIRecordable::E_GOALPIPEINSERTED, &recorderEventData);
#endif

	CCCPOINT(CPipeUser_InsertSubPipe_End);

	return pPipe;
}

void CPipeUser::ClearActiveGoals()
{
	if (!m_vActiveGoals.empty())
	{
		VectorOGoals::iterator li, liEnd = m_vActiveGoals.end();
		for (li = m_vActiveGoals.begin(); li != liEnd; ++li)
			li->pGoalOp->Reset(this);

		m_vActiveGoals.clear();
	}
	m_bBlocked = false;
}

void CPipeUser::ResetCurrentPipe(bool resetAlways)
{
	assert(!m_pipeExecuting);

	GetAISystem()->Record(this, IAIRecordable::E_GOALPIPERESETED,
	                      m_pCurrentGoalPipe ? m_pCurrentGoalPipe->GetName() : 0);

#ifdef CRYAISYSTEM_DEBUG
	RecorderEventData recorderEventData(m_pCurrentGoalPipe ? m_pCurrentGoalPipe->GetName() : 0);
	RecordEvent(IAIRecordable::E_GOALPIPERESETED, &recorderEventData);
#endif

	CGoalPipe* pPipe = m_pCurrentGoalPipe;
	while (pPipe)
	{
		NotifyListeners(pPipe, ePN_Deselected);
		if (resetAlways || !pPipe->GetSubpipe() || !pPipe->GetSubpipe()->IsHighPriority())
			pPipe = pPipe->GetSubpipe();
		else
			break;
	}

	if (!pPipe)
		ClearActiveGoals();
	else
		pPipe->SetSubpipe(NULL);

	if (m_pCurrentGoalPipe)
	{
		m_pCurrentGoalPipe->ResetGoalops(this);
		delete m_pCurrentGoalPipe;
		m_pCurrentGoalPipe = 0;
	}

	if (!pPipe)
	{
		if (CPuppet* pPuppet = CastToCPuppet())
			pPuppet->m_bCanReceiveSignals = true;

		m_bKeepMoving = false;
		ResetLookAt();
		ResetBodyTargetDir();
		ResetDesiredBodyDirectionAtTarget();
		ResetMovementContext();
	}
}

int CPipeUser::GetGoalPipeId() const
{
	const CGoalPipe* pPipe = m_pCurrentGoalPipe;
	if (!pPipe)
		return -1;
	while (pPipe->GetSubpipe())
		pPipe = pPipe->GetSubpipe();
	return pPipe->GetEventId();
}

void CPipeUser::ResetLookAt()
{
	m_bPriorityLookAtRequested = false;
	if (m_bLooseAttention)
	{
		SetLooseAttentionTarget(NILREF);
	}
}

bool CPipeUser::SetLookAtPointPos(const Vec3& point, bool priority)
{
	if (!priority && m_bPriorityLookAtRequested)
		return false;
	m_bPriorityLookAtRequested = priority;

	CCCPOINT(CPipeUser_SetLookAtPoint);

	CreateLookAtTarget();
	m_refLooseAttentionTarget = m_refLookAtTarget;
	CAIObject* pLookAtTarget = m_refLookAtTarget.GetAIObject();
	pLookAtTarget->SetPos(point);
	m_bLooseAttention = true;

	// check only in 2D
	Vec3 desired(point - GetPos());
	desired.z = 0;
	desired.NormalizeSafe();
	const SAIBodyInfo& bodyInfo = GetBodyInfo();
	Vec3 current(bodyInfo.vEyeDirAnim);
	current.z = 0;
	current.NormalizeSafe();

	// cos( 11.5deg ) ~ 0.98
	return 0.98f <= current.Dot(desired);
}

bool CPipeUser::SetLookAtDir(const Vec3& dir, bool priority)
{
	if (!priority && m_bPriorityLookAtRequested)
		return false;
	m_bPriorityLookAtRequested = priority;

	CCCPOINT(CPipeUser_SetLookAtDir);

	Vec3 vDir = dir;
	if (vDir.NormalizeSafe())
	{
		CreateLookAtTarget();
		m_refLooseAttentionTarget = m_refLookAtTarget;
		CAIObject* pLookAtTarget = m_refLookAtTarget.GetAIObject();
		pLookAtTarget->SetPos(GetPos() + vDir * 100.0f);
		m_bLooseAttention = true;

		// check only in 2D
		Vec3 desired(vDir);
		desired.z = 0;
		desired.NormalizeSafe();
		const SAIBodyInfo& bodyInfo = GetBodyInfo();
		Vec3 current(bodyInfo.vEyeDirAnim);
		current.z = 0;
		current.NormalizeSafe();

		// cos( 11.5deg ) ~ 0.98
		if (0.98f <= current.Dot(desired))
		{
			SetLooseAttentionTarget(NILREF);
			return true;
		}
		else
			return false;
	}
	else
		return true;
}

void CPipeUser::ResetBodyTargetDir()
{
	m_vBodyTargetDir.zero();
}

void CPipeUser::SetBodyTargetDir(const Vec3& dir)
{
	m_vBodyTargetDir = dir;
}

void CPipeUser::ResetDesiredBodyDirectionAtTarget()
{
	m_vDesiredBodyDirectionAtTarget.zero();
}

void CPipeUser::SetDesiredBodyDirectionAtTarget(const Vec3& dir)
{
	m_vDesiredBodyDirectionAtTarget = dir;
}

void CPipeUser::ResetMovementContext()
{
	m_movementContext = 0;
}

void CPipeUser::ClearMovementContext(unsigned int movementContext)
{
	m_movementContext &= ~movementContext;
}

void CPipeUser::SetMovementContext(unsigned int movementContext)
{
	m_movementContext |= movementContext;
}

Vec3 CPipeUser::GetLooseAttentionPos()
{
	CCCPOINT(CPipeUser_GetLooseAttentionPos);

	if (m_bLooseAttention)
	{
		CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
		if (pLooseAttentionTarget)
		{
			return pLooseAttentionTarget->GetPos();
		}
	}

	return ZERO;
}

void DeleteLookTarget(Vec3* lookTarget)
{
	delete lookTarget;
}

std::shared_ptr<Vec3> CPipeUser::CreateLookTarget()
{
	std::shared_ptr<Vec3> lookTarget(new Vec3(ZERO), DeleteLookTarget);
	m_lookTargets.push_back(lookTarget);
	return lookTarget;
}

CAIObject* CPipeUser::GetSpecialAIObject(const char* objName, float range)
{
	if (!objName) return NULL;

	CCCPOINT(CPipeUser_GetSpecialAIObject);

	CAIObject* pObject = NULL;

	CWeakRef<CPipeUser> refThis = GetWeakRef(this);

	if (range <= 0.0f)
		range = 20.0f;

	CAISystem* pAISystem = GetAISystem();

	if (strcmp(objName, "player") == 0)
	{
		pObject = pAISystem->GetPlayer();
	}
	else if (strcmp(objName, "self") == 0)
	{
		pObject = this;
	}
	else if (strcmp(objName, "beacon") == 0)
	{
		pObject = static_cast<CAIObject*>(pAISystem->GetBeacon(GetGroupId()));
	}
	else if (strcmp(objName, "refpoint") == 0)
	{
		CPipeUser* pPipeUser = CastToCPipeUser();
		if (pPipeUser)
			pObject = pPipeUser->GetRefPoint();
	}
	else if (strcmp(objName, "formation") == 0)
	{
		CLeader* pLeader = pAISystem->GetLeader(GetGroupId());
		// is leader managing my formation?
		{
			CAIObject* pFormationOwner = (pLeader ? pLeader->GetFormationOwner().GetAIObject() : 0);
			if (pFormationOwner && pFormationOwner->m_pFormation)
			{
				// check if I already have one
				pObject = pLeader->GetFormationPoint(refThis).GetAIObject();
				if (!pObject)
					pObject = pLeader->GetNewFormationPoint(refThis).GetAIObject();
			}
			else
			{
				CCCPOINT(CPipeUser_GetSpecialAIObject_A);

				CAIObject* pLastOp = GetLastOpResult();
				// check if Formation point is already acquired to lastOpResult
				if (pLastOp && pLastOp->GetSubType() == CAIObject::STP_FORMATION)
					pObject = pLastOp;
				else
				{
					IAIObject* pBoss = pAISystem->GetNearestObjectOfTypeInRange(this, AIOBJECT_ACTOR, 0, range, AIFAF_HAS_FORMATION | AIFAF_INCLUDE_DEVALUED | AIFAF_SAME_GROUP_ID);
					if (!pBoss)
						pBoss = pAISystem->GetNearestObjectOfTypeInRange(this, AIOBJECT_VEHICLE, 0, range, AIFAF_HAS_FORMATION | AIFAF_INCLUDE_DEVALUED | AIFAF_SAME_GROUP_ID);
					if (CAIObject* pTheBoss = static_cast<CAIObject*>(pBoss))
					{
						// check if I already have one
						pObject = pTheBoss->m_pFormation->GetFormationPoint(refThis);
						if (!pObject)
							pObject = pTheBoss->m_pFormation->GetNewFormationPoint(refThis);
					}
				}
				if (!pObject)
				{
					// lets send a NoFormationPoint event
					SetSignal(1, "OnNoFormationPoint", 0, 0, gAIEnv.SignalCRCs.m_nOnNoFormationPoint);
					if (pLeader)
						pLeader->SetSignal(1, "OnNoFormationPoint", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnNoFormationPoint);
				}
			}

		}

	}
	else if (strcmp(objName, "formation_id") == 0)
	{

		CAIObject* pLastOp = GetLastOpResult();
		// check if Formation point is already aquired to lastOpResult

		if (pLastOp && pLastOp->GetSubType() == CAIObject::STP_FORMATION)
			pObject = pLastOp;
		else  // if not - find available formation point
		{
			int groupid((int)range);
			CAIObject* pBoss(NULL);
			AIObjects::iterator ai;
			if ((ai = GetAISystem()->m_mapGroups.find(groupid)) != GetAISystem()->m_mapGroups.end())
			{
				for (; ai != GetAISystem()->m_mapGroups.end() && !pBoss; )
				{
					if (ai->first != groupid)
						break;
					CAIObject* pObj = ai->second.GetAIObject();
					if (pObj && pObj != this && pObj->m_pFormation)
					{
						pBoss = pObj;
					}
					++ai;
				}
			}
			if (pBoss && pBoss->m_pFormation)
			{
				// check if I already have one

				pObject = pBoss->m_pFormation->GetFormationPoint(refThis);
				if (!pObject)
					pObject = pBoss->m_pFormation->GetNewFormationPoint(refThis);
			}

			if (!pObject)
			{
				// lets send a NoFormationPoint event
				SetSignal(1, "OnNoFormationPoint", 0, 0, gAIEnv.SignalCRCs.m_nOnNoFormationPoint);
			}
		}

	}
	else if (strcmp(objName, "formation_special") == 0)
	{
		CLeader* pLeader = GetAISystem()->GetLeader(GetGroupId());
		if (pLeader)
			pObject = pLeader->GetNewFormationPoint(refThis, SPECIAL_FORMATION_POINT).GetAIObject();

		if (!pObject)
		{
			// lets send a NoFormationPoint event
			SetSignal(1, "OnNoFormationPoint", 0, 0, gAIEnv.SignalCRCs.m_nOnNoFormationPoint);
			if (pLeader)
				pLeader->SetSignal(1, "OnNoFormationPoint", GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnNoFormationPoint);
		}
	}
	else if (strcmp(objName, "atttarget") == 0)
	{
		CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
		pObject = pAttentionTarget;
	}
	else if (strcmp(objName, "last_hideobject") == 0)
	{
		if (m_CurrentHideObject.IsValid())
		{
			pObject = GetOrCreateSpecialAIObject(AISPECIAL_LAST_HIDEOBJECT);
			pObject->SetPos(m_CurrentHideObject.GetObjectPos());
		}
	}
	else if (strcmp(objName, "lookat_target") == 0)
	{
		if (m_bLooseAttention && m_refLooseAttentionTarget.IsValid())
			pObject = m_refLooseAttentionTarget.GetAIObject();
	}
	else if (strcmp(objName, "probtarget") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET);
		pObject->SetPos(GetProbableTargetPosition());
	}
	else if (strcmp(objName, "probtarget_in_territory") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET_IN_TERRITORY);
		Vec3 pos = GetProbableTargetPosition();
		// Clamp the point to the territory shape.
		if (SShape* pShape = GetTerritoryShape())
			pShape->ConstrainPointInsideShape(pos, true);

		pObject->SetPos(pos);
	}
	else if (strcmp(objName, "probtarget_in_refshape") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET_IN_REFSHAPE);
		Vec3 pos = GetProbableTargetPosition();
		// Clamp the point to ref shape
		if (GetRefShape())
			GetRefShape()->ConstrainPointInsideShape(pos, true);
		pObject->SetPos(pos);
	}
	else if (strcmp(objName, "probtarget_in_territory_and_refshape") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE);
		Vec3 pos = GetProbableTargetPosition();
		// Clamp the point to ref shape
		if (GetRefShape())
			GetRefShape()->ConstrainPointInsideShape(pos, true);
		// Clamp the point to the territory shape.
		if (SShape* pShape = GetTerritoryShape())
			pShape->ConstrainPointInsideShape(pos, true);

		pObject->SetPos(pos);
	}
	else if (strcmp(objName, "atttarget_in_territory") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_ATTTARGET_IN_TERRITORY);
		if (GetAttentionTarget())
		{
			Vec3 pos = GetAttentionTarget()->GetPos();
			// Clamp the point to the territory shape.
			if (SShape* pShape = GetTerritoryShape())
				pShape->ConstrainPointInsideShape(pos, true);

			pObject->SetPos(pos);
		}
		else
		{
			pObject->SetPos(GetPos());
		}
	}
	else if (strcmp(objName, "atttarget_in_refshape") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_ATTTARGET_IN_REFSHAPE);
		if (GetAttentionTarget())
		{
			Vec3 pos = GetAttentionTarget()->GetPos();
			// Clamp the point to ref shape
			if (GetRefShape())
				GetRefShape()->ConstrainPointInsideShape(pos, true);
			// Update pos
			pObject->SetPos(pos);
		}
		else
		{
			pObject->SetPos(GetPos());
		}
	}
	else if (strcmp(objName, "atttarget_in_territory_and_refshape") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE);
		if (GetAttentionTarget())
		{
			Vec3 pos = GetAttentionTarget()->GetPos();
			// Clamp the point to ref shape
			if (GetRefShape())
				GetRefShape()->ConstrainPointInsideShape(pos, true);
			// Clamp the point to the territory shape.
			if (SShape* pShape = GetTerritoryShape())
				pShape->ConstrainPointInsideShape(pos, true);

			pObject->SetPos(pos);
		}
		else
		{
			pObject->SetPos(GetPos());
		}
	}
	else if (strcmp(objName, "animtarget") == 0)
	{
		if (m_pActorTargetRequest)
		{
			pObject = GetOrCreateSpecialAIObject(AISPECIAL_ANIM_TARGET);
			pObject->SetPos(m_pActorTargetRequest->approachLocation, m_pActorTargetRequest->approachDirection);
		}
	}
	else if (strcmp(objName, "group_tac_pos") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_GROUP_TAC_POS);
	}
	else if (strcmp(objName, "group_tac_look") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_GROUP_TAC_LOOK);
	}
	else if (strcmp(objName, "vehicle_avoid") == 0)
	{
		pObject = GetOrCreateSpecialAIObject(AISPECIAL_VEHICLE_AVOID_POS);
	}
	else
	{
		pObject = gAIEnv.pAIObjectManager->GetAIObjectByName(objName);
	}
	return pObject;
}

//====================================================================
// GetTargetScore
//====================================================================
static int GetTargetScore(CAIObject* pAI, CAIObject* pTarget)
{
	// Returns goodness value based on the target type.
	int score = 0;

	if (pAI->IsHostile(pTarget)) score += 10;
	if (pTarget->GetSubType() == IAIObject::STP_SOUND) score += 1;
	if (pTarget->GetSubType() == IAIObject::STP_MEMORY) score += 2;
	if (pTarget->IsAgent()) score += 5;

	return score;
}

//====================================================================
// GetProbableTargetPosition
//====================================================================
Vec3 CPipeUser::GetProbableTargetPosition()
{
	if (m_timeSinceLastLiveTarget >= 0.0f && m_timeSinceLastLiveTarget < 2.0f)
	{
		// The current target is fresh, use it.
		return m_lastLiveTargetPos;
	}
	else
	{
		CCCPOINT(CPipeUser_GetProbableTargetPosition);
		// - Choose the nearest target that is more fresh than ours
		//   among the units sharing the same combat class in the group
		// - If no fresh targets are not available, try to choose the most
		//   important attention target
		// - If no attention target is available, try beacon
		// - If there is no beacon, just return the position of self.
		bool foundLive = false;
		bool foundAny = false;

		Vec3 nearestKnownTarget;
		CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
		if (pAttentionTarget)
			nearestKnownTarget = pAttentionTarget->GetPos();
		else
			nearestKnownTarget = GetPos();

		float nearestLiveDist = FLT_MAX;
		Vec3 nearestLivePos = nearestKnownTarget;

		float nearestAnyDist = FLT_MAX;
		Vec3 nearestAnyPos = nearestKnownTarget;
		int nearestAnyScore = 0;

		int combatClass = m_Parameters.m_CombatClass;

		int groupId = GetGroupId();

		AIObjects::iterator gi = GetAISystem()->m_mapGroups.find(groupId);
		AIObjects::iterator gend = GetAISystem()->m_mapGroups.end();

		for (; gi != gend; ++gi)
		{
			if (gi->first != groupId) break;
			CAIActor* pAIActor = CastToCAIActorSafe(gi->second.GetAIObject());
			if (!pAIActor || pAIActor->GetParameters().m_CombatClass != combatClass) continue;
			if (!pAIActor->IsEnabled()) continue;

			CPuppet* pPuppet = pAIActor->CastToCPuppet();
			if (pPuppet)
			{
				// Find nearest fresh live target.
				float otherTargetTime = pPuppet->GetTimeSinceLastLiveTarget();
				if (otherTargetTime >= 0.0f && otherTargetTime < 2.0f && otherTargetTime < m_timeSinceLastLiveTarget)
				{
					float dist = Distance::Point_PointSq(nearestKnownTarget, pPuppet->GetLastLiveTargetPosition());
					if (dist < nearestLiveDist)
					{
						nearestLiveDist = dist;
						nearestLivePos = pPuppet->GetLastLiveTargetPosition();
						foundLive = true;
					}
				}

				// Fall back.
				if (!foundLive && !pAttentionTarget && pPuppet->GetAttentionTarget())
				{
					const Vec3& targetPos = pPuppet->GetAttentionTarget()->GetPos();
					float dist = Distance::Point_PointSq(nearestAnyPos, targetPos);
					int score = GetTargetScore(this, pPuppet);
					if (score >= nearestAnyScore && dist < nearestAnyDist)
					{
						nearestAnyDist = dist;
						nearestAnyPos = targetPos;
						nearestAnyScore = score;
						foundAny = true;
					}
				}
			}
		}

		if (foundLive)
		{
			return nearestLivePos;
		}
		else if (pAttentionTarget)
		{
			return pAttentionTarget->GetPos();
		}
		else if (foundAny)
		{
			return nearestAnyPos;
		}
		else
		{
			IAIObject* beacon = GetAISystem()->GetBeacon(GetGroupId());
			if (beacon)
				return beacon->GetPos();
			else
				return GetPos();
		}
	}
}

//====================================================================
// GetOrCreateSpecialAIObject
//====================================================================
CAIObject* CPipeUser::GetOrCreateSpecialAIObject(ESpecialAIObjects type)
{
	//fix for /analyze warning
	assert(type < COUNT_AISPECIAL);

	CStrongRef<CAIObject>& refSpecialObj = m_refSpecialObjects[type];

	if (!refSpecialObj.IsNil())
		return refSpecialObj.GetAIObject();

	CCCPOINT(CPipeUser_GetOrCreateSpecialAIObject);

	ESubType subType = STP_SPECIAL;

	string name = GetName();
	switch (type)
	{
	case AISPECIAL_LAST_HIDEOBJECT:
		name += "_*LastHideObj";
		break;
	case AISPECIAL_PROBTARGET:
		name += "_*ProbTgt";
		break;
	case AISPECIAL_PROBTARGET_IN_TERRITORY:
		name += "_*ProbTgtInTerr";
		break;
	case AISPECIAL_PROBTARGET_IN_REFSHAPE:
		name += "_*ProbTgtInRefShape";
		break;
	case AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE:
		name += "_*ProbTgtInTerrAndRefShape";
		break;
	case AISPECIAL_ATTTARGET_IN_TERRITORY:
		name += "_*AttTgtInTerr";
		break;
	case AISPECIAL_ATTTARGET_IN_REFSHAPE:
		name += "_*AttTgtInRefShape";
		break;
	case AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE:
		name += "_*AttTgtInTerrAndRefShape";
		break;
	case AISPECIAL_ANIM_TARGET:
		name += "_*AnimTgt";
		subType = STP_ANIM_TARGET;
		break;
	case AISPECIAL_GROUP_TAC_POS:
		name += "_GroupTacPos";
		subType = STP_FORMATION;
		break;
	case AISPECIAL_GROUP_TAC_LOOK:
		name += "_GroupTacLook";
		break;
	case AISPECIAL_VEHICLE_AVOID_POS:
		name += "_VehicleAvoid";
		break;
	default:
		{
			char buf[64];
			cry_sprintf(buf, "_*Special%02d", (int)type);
			name += buf;
			break;
		}
	}

	gAIEnv.pAIObjectManager->CreateDummyObject(refSpecialObj, name, subType);
	return refSpecialObj.GetAIObject();
}

//====================================================================
// PathIsInvalid
//====================================================================
void CPipeUser::PathIsInvalid()
{
	ClearPath("CPipeUser::PathIsInvalid m_Path");
	m_State.vMoveDir.zero();
	m_State.fDesiredSpeed = 0.0f;
}

void CPipeUser::Serialize(TSerialize ser)
{
	CCCPOINT(CPipeUser_Serialize);

	// m_mapGoalPipeListeners must not change here! there's no need to serialize it!

	if (ser.IsReading())
		ResetCurrentPipe(true);

	CAIActor::Serialize(ser);

	ser.Value("m_bLastNearForbiddenEdge", m_bLastNearForbiddenEdge);
	ser.Value("m_bLastActionSucceed", m_bLastActionSucceed);

	// serialize members
	m_refRefPoint.Serialize(ser, "m_refRefPoint");
	m_refLookAtTarget.Serialize(ser, "m_refLookAtTarget");

	ser.Value("m_bEnableUpdateLookTarget", m_bEnableUpdateLookTarget);

	ser.Value("m_AttTargetPersistenceTimeout", m_AttTargetPersistenceTimeout);
	ser.EnumValue("m_AttTargetThreat", m_AttTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
	ser.EnumValue("m_AttTargetExposureThreat", m_AttTargetExposureThreat, AITHREAT_NONE, AITHREAT_LAST);
	ser.EnumValue("m_AttTargetType", m_AttTargetType, AITARGET_NONE, AITARGET_LAST);

	m_refLastOpResult.Serialize(ser, "m_refLastOpResult");

	ser.Value("m_bPathfinderConsidersPathTargetDirection", m_bPathfinderConsidersPathTargetDirection);
	ser.Value("m_fTimePassed", m_fTimePassed);

#ifdef SERIALIZE_COVER_INFORMATION
	ser.Value("m_inCover", m_inCover);
	ser.Value("m_movingToCover", m_movingToCover);
	ser.Value("m_movingInCover", m_movingInCover);
#else
	if (ser.IsReading())
	{
		m_vBodyTargetDir.zero();
		if (m_pCoverUser)
		{
			m_pCoverUser->Reset();
		}
	}
#endif // SERIALIZE_COVER_INFORMATION

	ser.Value("LooseAttention", m_bLooseAttention);

	m_refLooseAttentionTarget.Serialize(ser, "m_refLooseAttentionTarget");

	ser.Value("m_looseAttentionId", m_looseAttentionId);

	m_CurrentHideObject.Serialize(ser);

	ser.Value("m_nPathDecision", m_nPathDecision);
	ser.Value("m_adjustpath", m_adjustpath);
	ser.Value("m_IsSteering", m_IsSteering);

	if (ser.IsReading())
	{
		m_fireMode = FIREMODE_OFF;
	}

	m_refFireTarget.Serialize(ser, "m_refFireTarget");

	ser.Value("m_fireModeUpdated", m_fireModeUpdated);
	ser.Value("m_outOfAmmoSent", m_outOfAmmoSent);
	ser.Value("m_lowAmmoSent", m_lowAmmoSent);
	ser.Value("m_wasReloading", m_wasReloading);
	ser.Value("m_bFirstUpdate", m_bFirstUpdate);
	ser.Value("m_pathToFollowName", m_pathToFollowName);
	ser.Value("m_bPathToFollowIsSpline", m_bPathToFollowIsSpline);
	ser.EnumValue("m_CurrentNodeNavType", m_CurrentNodeNavType, IAISystem::NAV_UNSET, IAISystem::NAV_MAX_VALUE);

	// Serialize special objects.
	char specialName[64];
	for (unsigned i = 0; i < COUNT_AISPECIAL; ++i)
	{
		cry_sprintf(specialName, "m_pSpecialObjects%d", i);
		m_refSpecialObjects[i].Serialize(ser, specialName);
	}

#ifdef SERIALIZE_EXECUTING_GOALPIPE
	// serialize executing pipe
	if (ser.IsReading())
	{
		string goalPipeName;
		ser.Value("PipeName", goalPipeName);

		// always create a new goal pipe
		CGoalPipe* pPipe = NULL;
		if (goalPipeName != "none")
		{
			pPipe = gAIEnv.pPipeManager->IsGoalPipe(goalPipeName);
		}

		m_pCurrentGoalPipe = pPipe;   // this might be too slow, in which case we will go back to registration
		if (m_pCurrentGoalPipe)
		{
			m_pCurrentGoalPipe->Reset();
			m_pCurrentGoalPipe->Serialize(ser, m_vActiveGoals);
		}

		m_notAllowedSubpipes.clear();
	}
	else
	{
		if (m_pCurrentGoalPipe)
		{
			ser.Value("PipeName", m_pCurrentGoalPipe->GetNameAsString());

			m_pCurrentGoalPipe->Serialize(ser, m_vActiveGoals);
		}
		else
		{
			ser.Value("PipeName", string("none"));
		}
	}
#else
	if (ser.IsReading())
	{
		ResetCurrentPipe(true);
		CRY_ASSERT(m_pCurrentGoalPipe == NULL);
		m_notAllowedSubpipes.clear();
		SelectPipe(0, "_first_", NILREF, 0, true);   // Select the first goalpipe as when it's reset
	}
#endif // SERIALIZE_EXECUTING_GOALPIPE

	// this stuff can get reset when selecting pipe - so, serialize it after
	ser.Value("Blocked", m_bBlocked);
	ser.Value("KeepMoving", m_bKeepMoving);
	ser.Value("LastMoveDir", m_vLastMoveDir);

	ser.Value("m_PathDestinationPos", m_PathDestinationPos);

	m_Path.Serialize(ser);
	m_OrigPath.Serialize(ser);
	if (ser.IsWriting())
	{
		if (ser.BeginOptionalGroup("m_pPathFollower", m_pPathFollower != NULL))
		{
			m_pPathFollower->Serialize(ser);
			m_pPathFollower->AttachToPath(&m_Path);
			ser.EndGroup();
		}
	}
	else
	{
		SAFE_DELETE(m_pPathFollower);
		if (ser.BeginOptionalGroup("m_pPathFollower", true))
		{
			m_pPathFollower = CreatePathFollower(PathFollowerParams());
			m_pPathFollower->Serialize(ser);
			m_pPathFollower->AttachToPath(&m_Path);
			ser.EndGroup();
		}
	}
	ser.Value("m_posLookAtSmartObject", m_posLookAtSmartObject);

	ser.BeginGroup("UnreachableHideObjectList");
	{
		int count = m_recentUnreachableHideObjects.size();
		ser.Value("UnreachableHideObjectList_size", count);
		if (ser.IsReading())
			m_recentUnreachableHideObjects.clear();
		TimeOutVec3List::iterator it = m_recentUnreachableHideObjects.begin();
		float time(0);
		Vec3 point(0);
		for (int i = 0; i < count; i++)
		{
			char name[32];
			if (ser.IsWriting())
			{
				time = it->first;
				point = it->second;
				++it;
			}
			cry_sprintf(name, "time_%d", i);
			ser.Value(name, time);
			cry_sprintf(name, "point_%d", i);
			ser.Value(name, point);
			if (ser.IsReading())
				m_recentUnreachableHideObjects.push_back(std::make_pair(time, point));
		}
		ser.EndGroup();
	}

	ser.EnumValue("m_eNavSOMethod", m_eNavSOMethod, nSOmNone, nSOmLast);
	ser.Value("m_idLastUsedSmartObject", m_idLastUsedSmartObject);

	m_currentNavSOStates.Serialize(ser);
	m_pendingNavSOStates.Serialize(ser);

	ser.Value("m_actorTargetReqId", m_actorTargetReqId);

	ser.Value("m_refShapeName", m_refShapeName);
	if (ser.IsReading())
		m_refShape = GetAISystem()->GetGenericShapeOfName(m_refShapeName.c_str());

	if (ser.IsWriting())
	{
		if (ser.BeginOptionalGroup("m_pActorTargetRequest", m_pActorTargetRequest != 0))
		{
			m_pActorTargetRequest->Serialize(ser);
			ser.EndGroup();
		}
	}
	else
	{
		if (ser.BeginOptionalGroup("m_pActorTargetRequest", true))
		{
			if (!m_pActorTargetRequest)
				m_pActorTargetRequest = new SAIActorTargetRequest;
			m_pActorTargetRequest->Serialize(ser);
			ser.EndGroup();
		}
	}
}

void CPipeUser::PostSerialize()
{
	CAIActor::PostSerialize();

	const EntityId entityId = GetEntityID();
	
	gAIEnv.pMovementSystem->RegisterEntity(entityId, m_callbacksForPipeuser, m_movementActorAdapter);
	gAIEnv.pMovementSystem->AddActionAbilityCallbacks(entityId, m_coverMovementAbility);

	ICoverUser::Params params;
	params.distanceToCover = m_Parameters.distanceToCover;
	params.inCoverRadius = m_Parameters.inCoverRadius;
	params.minEffectiveCoverHeight = m_Parameters.effectiveCoverHeight;
	params.userID = entityId;
	params.activeCoverInvalidateCallback = functor(*this, &CPipeUser::SetCoverInvalidated);
	params.activeCoverCompromisedCallback = functor(*this, &CPipeUser::SetCoverInvalidated);
	m_pCoverUser = gAIEnv.pCoverSystem->RegisterEntity(entityId, params);
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
bool CPipeUser::SetCharacter(const char* character, const char* behaviour)
{
	return GetProxy()->SetCharacter(character, behaviour);
}
#endif

void CPipeUser::NotifyListeners(CGoalPipe* pPipe, EGoalPipeEvent event, bool includeSubPipes)
{
	if (pPipe && pPipe->GetEventId())
		NotifyListeners(pPipe->GetEventId(), event);
	if (includeSubPipes && pPipe)
		NotifyListeners(pPipe->GetSubpipe(), event, true);
}

void CPipeUser::NotifyListeners(int goalPipeId, EGoalPipeEvent event)
{
	if (!goalPipeId)
		return;

	// recursion?
	if (bNotifyListenersLock)
		return;

	bNotifyListenersLock = true;

	typedef int                                                    GoalPipeID;
	typedef std::vector<std::pair<GoalPipeID, IGoalPipeListener*>> PendingRemoval;
	PendingRemoval pendingRemoval;

	// Notify listeners
	{
		TMapGoalPipeListeners::iterator it, itEnd = m_mapGoalPipeListeners.end();
		for (it = m_mapGoalPipeListeners.find(goalPipeId); it != itEnd && it->first == goalPipeId; ++it)
		{
			std::pair<IGoalPipeListener*, const char*>& second = it->second;

			bool unregisterListenerAfterEvent = false;

			second.first->OnGoalPipeEvent(this, event, goalPipeId, unregisterListenerAfterEvent);

			if (unregisterListenerAfterEvent)
			{
				pendingRemoval.push_back(std::make_pair(goalPipeId, second.first));
			}
		}
	}

	bNotifyListenersLock = false;

	// Remove pending removals
	{
		PendingRemoval::const_iterator it = pendingRemoval.begin();
		PendingRemoval::const_iterator end = pendingRemoval.end();

		for (; it != end; ++it)
		{
			UnRegisterGoalPipeListener(it->second, it->first);
		}
	}
}

void CPipeUser::RegisterGoalPipeListener(IGoalPipeListener* pListener, int goalPipeId, const char* debugClassName)
{
	// you can't add listeners while notifying them
	AIAssert(!bNotifyListenersLock);

	// zero means don't listen it
	AIAssert(goalPipeId);

	m_mapGoalPipeListeners.insert(std::make_pair(goalPipeId, std::make_pair(pListener, debugClassName)));

	pListener->_vector_registered_pipes.push_back(std::pair<IPipeUser*, int>(this, goalPipeId));
}

void CPipeUser::UnRegisterGoalPipeListener(IGoalPipeListener* pListener, int goalPipeId)
{
	// you can't remove listeners while notifying them
	AIAssert(!bNotifyListenersLock);

	IGoalPipeListener::VectorRegisteredPipes::iterator itFind = std::find(
	  pListener->_vector_registered_pipes.begin(),
	  pListener->_vector_registered_pipes.end(),
	  std::pair<IPipeUser*, int>(this, goalPipeId));
	if (itFind != pListener->_vector_registered_pipes.end())
		pListener->_vector_registered_pipes.erase(itFind);

	TMapGoalPipeListeners::iterator it, itEnd = m_mapGoalPipeListeners.end();
	for (it = m_mapGoalPipeListeners.find(goalPipeId); it != itEnd && it->first == goalPipeId; ++it)
	{
		if (it->second.first == pListener)
		{
			m_mapGoalPipeListeners.erase(it);

			// listeners shouldn't register them twice!
#ifdef _DEBUG
			itEnd = m_mapGoalPipeListeners.end();
			for (TMapGoalPipeListeners::iterator it2 = m_mapGoalPipeListeners.find(goalPipeId); it2 != itEnd && it2->first == goalPipeId; ++it2)
				AIAssert(it2->second.first != pListener);
#endif

			return;
		}
	}
	//assert( !"Trying to unregister not registered listener" );
}

//
//-------------------------------------------------------------------------------------------
bool CPipeUser::IsUsing3DNavigation()
{
	bool can3D = (m_movementAbility.pathfindingProperties.navCapMask & (IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE)) != 0;
	bool can2D = (m_movementAbility.pathfindingProperties.navCapMask & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_FREE_2D | IAISystem::NAV_WAYPOINT_HUMAN)) != 0;

	if (can3D && !can2D)
		return true;
	if (can2D && !can3D)
		return false;

	bool use3D = false;
	if (!m_Path.Empty() && m_movementAbility.b3DMove)
	{
		const PathPointDescriptor* current = m_Path.GetPrevPathPoint();
		const PathPointDescriptor* next = m_Path.GetNextPathPoint();
		bool current3D = true;
		bool next3D = true;
		if (current)
		{
			m_CurrentNodeNavType = current->navType;
			if ((current->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_ROAD))
			    || current->navType == IAISystem::NAV_UNSET)
				current3D = false;
		}
		if (next)
		{
			if ((next->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_ROAD))
			    || next->navType == IAISystem::NAV_UNSET)
				next3D = false;
		}
		use3D = (current3D || next3D);
	}
	else
	{
		if (m_CurrentNodeNavType == IAISystem::NAV_UNSET)
		{
			int nBuildingID;
			m_CurrentNodeNavType = gAIEnv.pNavigation->CheckNavigationType(GetPhysicsPos(), nBuildingID, m_movementAbility.pathfindingProperties.navCapMask);
		}

		if (!((m_CurrentNodeNavType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_ROAD | IAISystem::NAV_FREE_2D))
		      || m_CurrentNodeNavType == IAISystem::NAV_UNSET))
			use3D = true;
	}
	return use3D;
}
//
//-------------------------------------------------------------------------------------------
void CPipeUser::DebugDrawGoals()
{
	// Debug draw all active goals.
	for (size_t i = 0; i < m_vActiveGoals.size(); i++)
	{
		if (m_vActiveGoals[i].pGoalOp)
			m_vActiveGoals[i].pGoalOp->DebugDraw(this);
	}
}

//====================================================================
// SetPathToFollow
//====================================================================
void CPipeUser::SetPathToFollow(const char* pathName)
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("CPipeUser::SetPathToFollow %s path = %s", GetName(), pathName);
	m_pathToFollowName = pathName;
	m_bPathToFollowIsSpline = false;
}

void CPipeUser::SetPathAttributeToFollow(bool bSpline)
{
	m_bPathToFollowIsSpline = bSpline;
}

//===================================================================
// GetPathEntryPoint
//===================================================================
bool CPipeUser::GetPathEntryPoint(Vec3& entryPos, bool reverse, bool startNearest) const
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("CPipeUser::GetPathEntryPoint %s path = %s", GetName(), m_pathToFollowName.c_str());

	if (m_pathToFollowName.length() == 0)
		return false;

	SShape path1;
	if (!gAIEnv.pNavigation->GetDesignerPath(m_pathToFollowName.c_str(), path1))
	{
		AIWarning("CPipeUser::GetPathEntryPoint %s unable to find path %s - check path is not marked as a road and that all AI has been generated and exported", GetName(), m_pathToFollowName.c_str());
		return false;
	}
	if (path1.shape.empty())
		return false;

	if (startNearest)
	{
		float d;
		path1.NearestPointOnPath(GetPhysicsPos(), false, d, entryPos);
	}
	else if (reverse)
	{
		entryPos = *path1.shape.rbegin();
	}
	else
	{
		entryPos = *path1.shape.begin();
	}

	return true;
}

//====================================================================
// UsePathToFollow
//====================================================================
bool CPipeUser::UsePathToFollow(bool reverse, bool startNearest, bool loop)
{
	if (m_pathToFollowName.length() == 0)
		return false;

	ClearPath("CPipeUser::UsePathToFollow m_Path");

	SShape path1;
	if (!gAIEnv.pNavigation->GetDesignerPath(m_pathToFollowName.c_str(), path1))
		return false;

	if (path1.shape.empty())
		return false;

	IAISystem::ENavigationType navType(path1.navType);

	if (startNearest)
	{
		float d;
		Vec3 nearestPoint;
		ListPositions::const_iterator nearest = path1.NearestPointOnPath(GetPhysicsPos(), false, d, nearestPoint);

		m_Path.PushBack(PathPointDescriptor(navType, nearestPoint));

		if (reverse)
		{
			ListPositions::const_reverse_iterator rnearest(nearest);
			ListPositions::const_reverse_iterator rbegin(path1.shape.rbegin());
			ListPositions::const_reverse_iterator rend(path1.shape.rend());
			for (ListPositions::const_reverse_iterator it = rnearest; it != rend; ++it)
			{
				const Vec3& pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
			if (loop)
			{
				for (ListPositions::const_reverse_iterator it = rbegin; it != rnearest; ++it)
				{
					const Vec3& pos = *it;
					m_Path.PushBack(PathPointDescriptor(navType, pos));
				}
				m_Path.PushBack(PathPointDescriptor(navType, nearestPoint));
			}
		}
		else
		{
			for (ListPositions::const_iterator it = nearest; it != path1.shape.end(); ++it)
			{
				const Vec3& pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
			if (loop)
			{
				for (ListPositions::const_iterator it = path1.shape.begin(); it != nearest; ++it)
				{
					const Vec3& pos = *it;
					m_Path.PushBack(PathPointDescriptor(navType, pos));
				}
				m_Path.PushBack(PathPointDescriptor(navType, nearestPoint));
			}
		}
	}
	else
	{
		if (reverse)
		{
			for (ListPositions::reverse_iterator it = path1.shape.rbegin(); it != path1.shape.rend(); ++it)
			{
				const Vec3& pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
		}
		else
		{
			for (ListPositions::const_iterator it = path1.shape.begin(); it != path1.shape.end(); ++it)
			{
				const Vec3& pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
		}
	}

	// convert ai path to be splined
	// this spline is not enough one and allows a possibility that a conversion fails
	// 29/10/2006 Tetsuji
	if (m_bPathToFollowIsSpline)
	{
		ConvertPathToSpline(navType);
		return true;
	}

	SNavPathParams params;
	params.precalculatedPath = true;
	params.meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(GetNavigationTypeID(), path1.shape[0]);

	m_Path.SetParams(params);

	m_nPathDecision = PATHFINDER_PATHFOUND;
	m_Path.PushFront(PathPointDescriptor(navType, GetPhysicsPos()));
	m_OrigPath = m_Path;
	return true;
}

//===================================================================
// ConvertPathToSpline
//===================================================================
bool CPipeUser::ConvertPathToSpline(IAISystem::ENavigationType navType)
{
	const TPathPoints& path = m_Path.GetPath();
	size_t nPoints = path.size();

	if (nPoints == 0)
		return false;

	std::vector<Vec3> pointListLocal;
	pointListLocal.reserve(nPoints);

	TPathPoints::const_iterator li, liend;
	li = path.begin();
	liend = path.end();

	while (li != liend)
	{
		pointListLocal.push_back(li->vPos);
		++li;
	}

	SNavPathParams params = m_Path.GetParams();
	m_Path.Clear("CPipeUser::ConvertPathToSpline");
	SetPointListToFollow(pointListLocal, navType, true);

	m_OrigPath.Clear("CPipeUser::ConvertPathBySpline");

	params.precalculatedPath = true;
	m_Path.SetParams(params);

	m_nPathDecision = PATHFINDER_PATHFOUND;
	m_OrigPath = m_Path;

	return true;

}

//-------------------------------------------------------------------------------------------------

Vec3 CPipeUser::SetPointListToFollowSub(Vec3 a, Vec3 b, Vec3 c, std::vector<Vec3>& newPointList, float step)
{
	float ideallength = 4.0f;

	if (m_bPathToFollowIsSpline == true)
		ideallength = 1.0f;

	// calculate normalized spline(2) ( means each line has the same length )
	// x(1-t)^2 + 2yt(1-t)+ zt^2

	// when we suppose x=(0,0,0) then spline(2) is below
	// 2yt(1-t) +zt^2 =s
	// (z-2y)t^t + 2yt - s = 0;

	// to get t from a length
	// t = -2y +- sqrt( 4y^2 - 4(z-2y)s ) / 2(z-2y)

	// these 3 points (a,b,c) makes a spline(2)
	// if there is a problem,
	// point a will be skipped then ( b, c, d ) will be calculated next time.

	Vec3 oa = (a + b) / 2.0f;
	Vec3 ob = b;
	Vec3 oc = (b + c) / 2.0f;

	Vec3 s = ob - oa; // src
	Vec3 d = oc - oa; // destination

	// when points are ideantical
	if (s.GetLength() < 0.0001f)
		return b;

	// when (0,0,0),d and s make line, we just pass these point to the path system

	float y, z, len;
	Vec3 nl = s * ideallength / s.GetLength();

	for (int i = 0; i < 3; i++)
	{
		// select a stable element for the calculation.
		if (i == 0)
			y = s.x, z = d.x, len = nl.x;
		if (i == 1)
			y = s.y, z = d.y, len = nl.y;
		if (i == 2)
			y = s.z, z = d.z, len = nl.z;

		// solve a formula for get t from length (ax*x + bx + c = 0)
		float ca = (z - 2.0f * y);
		float cb = 2.0f * y;
		float cc = -len;

		float sq = cb * cb - 4.0f * ca * cc;
		if (sq >= 0.0f && ca != 0.0f)
		{
			sq = sqrtf(sq);
			float t1 = (-cb + sq) / (2.0f * ca);
			float t2 = (-cb - sq) / (2.0f * ca);
			float t = (t1 > 0.0f && t1 < step) ? t1 : t2;

			if (t > 0.0f && t < step)
			{
				for (float u = 0.0f; u < step; u += t)
				{
					Vec3 np = 2.0f * u * (1.0f - u) * s + u * u * d;
					np += oa;
					newPointList.push_back(np);
				}
				if (!newPointList.empty())
				{
					Vec3 ret = newPointList.back();
					ret = (ret - c) * 2.0f + c;
					newPointList.pop_back();
					return ret;
				}
			}
		}
	}
	newPointList.push_back(oa);
	return b;

}

//===================================================================
// SetPointListToFollow
//===================================================================
void CPipeUser::SetPointListToFollow(const std::vector<Vec3>& pointList, IAISystem::ENavigationType navType, bool bSpline)
{
	ClearPath("CPipeUser::SetPointListToFollow m_Path");

	static bool debugsw = false;

	if (debugsw == true)
	{
		for (std::vector<Vec3>::const_iterator it = pointList.begin(); it != pointList.end(); ++it)
		{
			const Vec3& pos = *it;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
		return;
	}

	// To guarantee at least 3 points for B-Spline(2)
	int howmanyPoints = pointList.size();

	if (howmanyPoints < 2)
		return;

	std::vector<Vec3>::const_iterator itLocal;

	if (howmanyPoints == 2 || bSpline == false)
	{
		for (itLocal = pointList.begin(); itLocal != pointList.end(); ++itLocal)
		{
			const Vec3& pos = *itLocal;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
		return;
	}

	std::vector<Vec3>::const_iterator itX = pointList.begin();
	std::vector<Vec3>::const_iterator itY = itX;
	std::vector<Vec3>::const_iterator itZ = itX;

	++itY;
	++itZ, ++itZ;

	Vec3 nextStart = *itX;
	Vec3 mid = *itY;
	Vec3 end = *itZ;

	nextStart = (nextStart - mid) * 2.0f + mid;

	std::vector<Vec3> pointListLocal;

	// divde each line

	for (int i = 0; i < howmanyPoints - 3; ++itX, ++itY, ++itZ, i++)
	{
		pointListLocal.clear();
		nextStart = SetPointListToFollowSub(nextStart, *itY, *itZ, pointListLocal, 1.0f);

		for (itLocal = pointListLocal.begin(); itLocal != pointListLocal.end(); ++itLocal)
		{
			const Vec3& pos = *itLocal;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
	}

	// at the last point, we need special treatment to make the curve reach the end point.
	mid = *itY;
	end = *itZ;
	end = (end - mid) * 2.0f + mid;

	pointListLocal.clear();
	nextStart = SetPointListToFollowSub(nextStart, *itY, end, pointListLocal, 1.0f);
	if (pointListLocal.empty())
	{
		const Vec3& pos = nextStart;
		m_Path.PushBack(PathPointDescriptor(navType, pos));
	}
	else
	{
		for (itLocal = pointListLocal.begin(); itLocal != pointListLocal.end(); ++itLocal)
		{
			const Vec3& pos = *itLocal;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
	}

	// terminate the end point ( skip if the final point of the spline is close to the end )
	{
		const Vec3& pos = *itZ;
		if ((pos - nextStart).GetLength() > 1.0f)
			m_Path.PushBack(PathPointDescriptor(navType, pos));
	}

	//m_Path.PushFront(PathPointDescriptor(navType, GetPhysicsPos()));
}

//===================================================================
// UsePointListToFollow
//===================================================================
bool CPipeUser::UsePointListToFollow(void)
{
	m_OrigPath.Clear("CPipeUser::UsePointListToFollow m_OrigPath");

	SNavPathParams params;
	params.precalculatedPath = true;
	m_Path.SetParams(params);

	m_nPathDecision = PATHFINDER_PATHFOUND;
	m_OrigPath = m_Path;

	return true;

}
//-------------------------------------------------------------------------------------------------
void CPipeUser::SetFireMode(EFireMode mode)
{
	//	m_fireAtLastOpResult = shootLastOpResult;
	m_outOfAmmoSent = false;
	m_lowAmmoSent = false;
	m_wasReloading = false;
	if (m_fireMode == mode)
		return;
	m_fireMode = mode;
	m_fireModeUpdated = true;
	m_spreadFireTime = cry_random(0.0f, 10.0f);
}

//-------------------------------------------------------------------------------------------------
void CPipeUser::SetFireTarget(CWeakRef<CAIObject> refTargetObject)
{
	m_refFireTarget = refTargetObject;
}

//-------------------------------------------------------------------------------------------------
void CPipeUser::SetRefPointPos(const Vec3& pos)
{
	CCCPOINT(CPipeUser_SetRefPointPos);

	CreateRefPoint();
	CAIObject* pRefPoint = m_refRefPoint.GetAIObject();
	AIAssert(pRefPoint);

	static bool bSetRefPointPosLock = false;
	bool bNotify = !bSetRefPointPosLock && m_pCurrentGoalPipe && !pRefPoint->GetPos().IsEquivalent(pos, 0.001f);
	pRefPoint->SetPos(pos, GetMoveDir());
	pRefPoint->SetEntityID(0);

	if (GetAISystem()->IsRecording(this, IAIRecordable::E_REFPOINTPOS))
	{
		static char buffer[32];
		cry_sprintf(buffer, "<%.0f %.0f %.0f>", pos.x, pos.y, pos.z);
		GetAISystem()->Record(this, IAIRecordable::E_REFPOINTPOS, buffer);
	}

#ifdef CRYAISYSTEM_DEBUG
	RecorderEventData recorderEventData(pos);
	RecordEvent(IAIRecordable::E_REFPOINTPOS, &recorderEventData);
#endif

	if (bNotify)
	{
		bSetRefPointPosLock = true;
		NotifyListeners(m_pCurrentGoalPipe->GetLastSubpipe(), ePN_RefPointMoved);
		bSetRefPointPosLock = false;
	}
}

void CPipeUser::SetRefPointPos(const Vec3& pos, const Vec3& dir)
{
	CCCPOINT(CPipeUser_SetRefPointPos_Dir);

	CreateRefPoint();
	CAIObject* pRefPoint = m_refRefPoint.GetAIObject();
	AIAssert(pRefPoint);

	static bool bSetRefPointPosDirLock = false;
	bool bNotify = !bSetRefPointPosDirLock && m_pCurrentGoalPipe && !pRefPoint->GetPos().IsEquivalent(pos, 0.001f);
	pRefPoint->SetPos(pos, dir);

	if (GetAISystem()->IsRecording(this, IAIRecordable::E_REFPOINTPOS))
	{
		static char buffer[32];
		cry_sprintf(buffer, "<%.0f %.0f %.0f>", pos.x, pos.y, pos.z);
		GetAISystem()->Record(this, IAIRecordable::E_REFPOINTPOS, buffer);
	}

#ifdef CRYAISYSTEM_DEBUG
	RecorderEventData recorderEventData(pos);
	RecordEvent(IAIRecordable::E_REFPOINTPOS, &recorderEventData);
#endif

	if (bNotify)
	{
		bSetRefPointPosDirLock = true;
		NotifyListeners(m_pCurrentGoalPipe->GetLastSubpipe(), ePN_RefPointMoved);
		bSetRefPointPosDirLock = false;
	}
}

//
//---------------------------------------------------------------------------------------------------------------------------
void CPipeUser::IgnoreCurrentHideObject(float timeOut)
{
	const Vec3& pos(m_CurrentHideObject.GetObjectPos());

	// Check if the object is already in the list (should not).
	TimeOutVec3List::const_iterator end(m_recentUnreachableHideObjects.end());
	for (TimeOutVec3List::const_iterator it = m_recentUnreachableHideObjects.begin(); it != end; ++it)
	{
		if (Distance::Point_PointSq(it->second, pos) < sqr(0.1f))
			return;
	}

	// Add the object pos to the list.
	m_recentUnreachableHideObjects.push_back(FloatVecPair(timeOut, pos));

	while (m_recentUnreachableHideObjects.size() > 5)
		m_recentUnreachableHideObjects.pop_front();
}

//
//---------------------------------------------------------------------------------------------------------------------------
bool CPipeUser::WasHideObjectRecentlyUnreachable(const Vec3& pos) const
{
	TimeOutVec3List::const_iterator end(m_recentUnreachableHideObjects.end());
	for (TimeOutVec3List::const_iterator it = m_recentUnreachableHideObjects.begin(); it != end; ++it)
	{
		if (Distance::Point_PointSq(it->second, pos) < sqr(0.1f))
			return true;
	}
	return false;
}

//
//---------------------------------------------------------------------------------------------------------------------------
int CPipeUser::SetLooseAttentionTarget(CWeakRef<CAIObject> refObject, int id)
{

	CAIObject* pObject = refObject.GetAIObject();
	if (pObject || id == m_looseAttentionId || id == -1)
	{
		m_bLooseAttention = pObject != NULL;
		if (m_bLooseAttention)
		{
			m_refLooseAttentionTarget = refObject;
			++m_looseAttentionId;
		}
		else
		{
			m_refLooseAttentionTarget.Reset();
		}
	}
	return m_looseAttentionId;
}

//
//---------------------------------------------------------------------------------------------------------------------------
void CPipeUser::SetLookStyle(ELookStyle eLookTargetStyle)
{
	m_State.eLookStyle = eLookTargetStyle;
}

//---------------------------------------------------------------------------------------------------------------------------
ELookStyle CPipeUser::GetLookStyle()
{
	return m_State.eLookStyle;
}

//---------------------------------------------------------------------------------------------------------------------------
void CPipeUser::AllowLowerBodyToTurn(bool bAllowLowerBodyToTurn)
{
	m_State.bAllowLowerBodyToTurn = bAllowLowerBodyToTurn;
}

bool CPipeUser::IsAllowingBodyTurn()
{
	return m_State.bAllowLowerBodyToTurn;
}

//
//---------------------------------------------------------------------------------------------------------------------------
int CPipeUser::CountGroupedActiveGoals()
{
	int count(0);

	for (size_t i = 0; i < m_vActiveGoals.size(); i++)
	{
		QGoal& goal = m_vActiveGoals[i];
		if (goal.eGrouping == IGoalPipe::eGT_GROUPED && goal.op != eGO_WAIT)
			++count;
	}
	return count;
}

//
//---------------------------------------------------------------------------------------------------------------------------
void CPipeUser::ClearGroupedActiveGoals()
{
	for (size_t i = 0; i < m_vActiveGoals.size(); i++)
	{
		QGoal& goal = m_vActiveGoals[i];
		if (goal.eGrouping == IGoalPipe::eGT_GROUPED && goal.op != eGO_WAIT)
		{
			RemoveActiveGoal(i);
			if (!m_vActiveGoals.empty())
				--i;
		}
	}
}

//
//----------------------------------------------------------------------------------------------
void CPipeUser::SetRefShapeName(const char* shapeName)
{
	// Set and resolve the shape name.
	m_refShapeName = shapeName;
	m_refShape = GetAISystem()->GetGenericShapeOfName(m_refShapeName.c_str());
	if (!m_refShape)
		m_refShapeName.clear();
}

//
//----------------------------------------------------------------------------------------------
const char* CPipeUser::GetRefShapeName() const
{
	return m_refShapeName.c_str();
}

//
//----------------------------------------------------------------------------------------------
SShape* CPipeUser::GetRefShape()
{
	return m_refShape;
}

//
//----------------------------------------------------------------------------------------------
void CPipeUser::RecordSnapshot()
{
	// Currently not used
}

//
//----------------------------------------------------------------------------------------------
void CPipeUser::RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData)
{
#ifdef CRYAISYSTEM_DEBUG
	CRecorderUnit* pRecord = (CRecorderUnit*)GetAIDebugRecord();
	if (pRecord != NULL)
	{
		pRecord->RecordEvent(event, pEventData);
	}
#endif //CRYAISYSTEM_DEBUG
}

//
//----------------------------------------------------------------------------------------------
void CPipeUser::OnMNMPathResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result)
{
	assert(requestId == m_queuedPathId);

	m_queuedPathId = 0;

	HandlePathDecision(result);
}

//
//----------------------------------------------------------------------------------------------
void CPipeUser::CancelRequestedPath(bool actorRemoved)
{
	if (m_queuedPathId)
	{
		gAIEnv.pMNMPathfinder->CancelPathRequest(m_queuedPathId);

		m_queuedPathId = 0;
	}
}

//
//----------------------------------------------------------------------------------------------
void CPipeUser::HandlePathDecision(MNMPathRequestResult& result)
{
	CNavPath pNavPath;
	if (result.HasPathBeenFound())
	{
		result.pPath->CopyTo(&pNavPath);

		if (gAIEnv.CVars.DebugPathFinding)
		{
			const Vec3& vEnd = pNavPath.GetLastPathPos();
			const Vec3& vStart = pNavPath.GetPrevPathPoint()->vPos;
			const Vec3& vStoredPathEnd = pNavPath.GetParams().end;
			const Vec3& vNext = pNavPath.GetNextPathPos();

			AILogAlways("CPipeUser::HandlePathDecision %s found path to (%5.2f, %5.2f, %5.2f) from (%5.2f, %5.2f, %5.2f). Stored path end is (%5.2f, %5.2f, %5.2f). Next is (%5.2f, %5.2f, %5.2f)",
			            GetName(),
			            vEnd.x, vEnd.y, vEnd.z,
			            vStart.x, vStart.y, vStart.z,
			            vStoredPathEnd.x, vStoredPathEnd.y, vStoredPathEnd.z,
			            vNext.x, vNext.y, vNext.z);
		}

		m_nPathDecision = PATHFINDER_PATHFOUND;

		// Check if there is a pending NavSO which has not started playing yet and cancel it.
		// Path regeneration while the animation is playing is ok, but should be inhibited while preparing for actor target.
		// This may happen if some goal operation or such requests path regeneration
		// while the actor target has been requested. This should not happen.
		if (m_eNavSOMethod != nSOmNone && m_State.curActorTargetPhase == eATP_Waiting)
		{
			if (gAIEnv.CVars.DebugPathFinding)
			{
				if (m_State.actorTargetReq.animation.empty())
				{
					AILogAlways("CPipeUser::HandlePathDecision %s got new path during actor target request (phase: %d, vehicle: %s, seat: %d). Please investigate, this case should not happen.",
					            GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.vehicleName.c_str(), m_State.actorTargetReq.vehicleSeat);
				}
				else
				{
					AILogAlways("CPipeUser::HandlePathDecision %s got new path during actor target request (phase: %d, anim: %s). Please investigate, this case should not happen.",
					            GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.animation.c_str());
				}
			}
			SetNavSOFailureStates();
		}

		// Check if there is a pending end-of-path actor target request which has not started playing yet and cancel it.
		// Path regeneration should be inhibited while preparing for actor target.
		if (GetActiveActorTargetRequest() && m_State.curActorTargetPhase == eATP_Waiting)
		{
			if (gAIEnv.CVars.DebugPathFinding)
			{
				if (m_State.actorTargetReq.animation.empty())
				{
					AILogAlways("CPuppet::HandlePathDecision %s got new path during actor target request (phase:%d, vehicle:%s, seat:%d). Please investigate, this case should not happen.",
					            GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.vehicleName.c_str(), m_State.actorTargetReq.vehicleSeat);
				}
				else
				{
					AILogAlways("CPuppet::HandlePathDecision %s got new path during actor target request (phase:%d, anim:%s). Please investigate, this case should not happen.",
					            GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.animation.c_str());
				}
			}
			// Stop exact positioning.
			m_State.actorTargetReq.Reset();
		}

		// lets get it from the ai system
		// [Mikko] The AIsystem working path version does not always match the
		// pipeuser path version. When new path is found, for it to be used.
		// In some weird cases the new path version ca even be the same as the
		// current one. Make sure the new path version is different than the current.
		int oldVersion = m_Path.GetVersion();
		m_Path = pNavPath;
		m_Path.SetVersion(oldVersion + 1);

		// Store the path destination point for later checks.
		if (!m_Path.Empty())
			m_PathDestinationPos = m_Path.GetLastPathPos();

		// Cut the path end
		float trimLength = m_Path.GetParams().endDistance;
		if (fabsf(trimLength) > 0.0001f)
			m_Path.TrimPath(trimLength, IsUsing3DNavigation());

		// Trim the path so that it will only reach until the first navSO animation.
		m_OrigPath = m_Path;

		if (m_cutPathAtSmartObject)
		{
			m_Path.PrepareNavigationalSmartObjectsForMNM(GetEntity());
		}

		m_State.fDistanceToPathEnd = m_Path.GetPathLength(!IsUsing3DNavigation());
		// AdjustPath may have failed
		if (m_nPathDecision == PATHFINDER_PATHFOUND && m_OrigPath.GetPath().size() < 2)
		{
			AIWarning("CPipeUser::HandlePathDecision (%5.2f, %5.2f, %5.2f) original path size = %" PRISIZE_T " for %s",
			          GetPos().x, GetPos().y, GetPos().z, m_OrigPath.GetPath().size(), GetName());
		}
	}
	else
	{
		if (gAIEnv.CVars.DebugPathFinding)
		{
			AILogAlways("CPipeUser::HandlePathDecision %s No path", GetName());
		}

		ClearPath("CPipeUser::HandlePathDecision m_Path");
		m_nPathDecision = PATHFINDER_NOPATH;
	}

	if (!CastToCPuppet())
	{
		AdjustPath(); // CPuppets have some stuff to do before AdjustPath
	}
}

//===================================================================
// AdjustPath
//===================================================================
void CPipeUser::AdjustPath()
{
	const AgentPathfindingProperties& pathProps = m_movementAbility.pathfindingProperties;

	if (pathProps.avoidObstacles)
	{
		if (!AdjustPathAroundObstacles())
		{
			AIWarning("CPuppet::AdjustPath Unable to adjust path around obstacles for %s", GetName());
			ClearPath("CPuppet::AdjustPath m_Path");
			m_nPathDecision = PATHFINDER_NOPATH;
			return;
		}
	}

	if (m_adjustpath > 0)
	{
		IAISystem::tNavCapMask navCapMask = pathProps.navCapMask;
		int nBuildingID;
		IAISystem::ENavigationType currentNavType = gAIEnv.pNavigation->CheckNavigationType(GetPos(), nBuildingID, navCapMask);
		ConvertPathToSpline(currentNavType);
	}
}

//===================================================================
// AdjustPathAroundObstacles
//===================================================================
bool CPipeUser::AdjustPathAroundObstacles()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (gAIEnv.CVars.AdjustPathsAroundDynamicObstacles != 0)
	{
		CalculatePathObstacles();

		return m_Path.AdjustPathAroundObstacles(m_pathAdjustmentObstacles, m_movementAbility.pathfindingProperties.navCapMask, nullptr);
	}

	return true;
}

//===================================================================
// GetPathFollower (non-virtual, non-const)
//===================================================================
IPathFollower* CPipeUser::GetPathFollower()
{
	// only create this if appropriate (human - set by config etc)
	if (!m_pPathFollower && m_movementAbility.usePredictiveFollowing)
	{
		// Set non-default values (most get overridden during movement anyhow)
		PathFollowerParams params;
		GetPathFollowerParams(params);

		m_pPathFollower = CreatePathFollower(params);
		assert(m_pPathFollower);
		if (m_pPathFollower)
		{
			m_pPathFollower->AttachToPath(&m_Path);
		}
	}
	return m_pPathFollower;
}

//===================================================================
// GetPathFollower (virtual, const)
//===================================================================
IPathFollower* CPipeUser::GetPathFollower() const
{
	return m_pPathFollower;
}

//===================================================================
// GetPathFollowerParams
//===================================================================
void CPipeUser::GetPathFollowerParams(PathFollowerParams& outParams) const
{
	outParams.navCapMask = m_movementAbility.pathfindingProperties.navCapMask;
	outParams.passRadius = m_movementAbility.pathfindingProperties.radius;
	outParams.pathRadius = m_movementAbility.pathRadius;
	outParams.stopAtEnd = true;
	outParams.use2D = !m_movementAbility.b3DMove;

	outParams.endAccuracy = 0.2f;

	// Obsolete
	outParams.pathLookAheadDist = m_movementAbility.pathLookAhead;
	outParams.maxSpeed = 0.0f;
}

EntityId CPipeUser::GetPendingSmartObjectID() const
{
	return 0;
}

//===================================================================
// CreatePathFollower
//===================================================================
IPathFollower* CPipeUser::CreatePathFollower(const PathFollowerParams& params)
{
	IPathFollower* pResult = NULL;

	if (gAIEnv.CVars.UseSmartPathFollower == 1)
	{
		CSmartPathFollower* pSmartPathFollower = new CSmartPathFollower(params, m_pathAdjustmentObstacles);
		pResult = pSmartPathFollower;
	}
	else
	{
		pResult = new CPathFollower(params);
	}

	assert(pResult);
	return pResult;
}

void CPipeUser::CreateMovementPlanCoverStartBlocks(DynArray<Movement::BlockPtr>& blocks, const MovementRequest& request)
{
	if (IsInCover())
	{
		blocks.push_back(Movement::BlockPtr(new Movement::MovementBlocks::UninstallPipeUserFromCover(*this, request.style.GetStance())));
	}
	if (request.style.IsMovingToCover() && GetCoverRegister())
	{
		blocks.push_back(Movement::BlockPtr(new Movement::MovementBlocks::SetupPipeUserCoverInformation(*this)));
	}
}

void CPipeUser::CreateMovementPlanCoverEndBlocks(DynArray<Movement::BlockPtr>& blocks, const MovementRequest& request)
{
	if (request.style.IsMovingToCover() && GetCoverRegister())
	{
		blocks.push_back(Movement::BlockPtr(new Movement::MovementBlocks::InstallPipeUserInCover(*this)));
	}
}

void CPipeUser::PrePathFollowUpdate(const MovementUpdateContext& context, bool bIsLastFollowBlock)
{
	if (!bIsLastFollowBlock || !IsMovingToCover())
		return;
	
	CoverID coverID = GetCoverID();
	IF_UNLIKELY(coverID == 0)
		return;

	Vec3 coverNormal(ZERO);
	const Vec3 coverPosition = gAIEnv.pCoverSystem->GetCoverLocation(coverID, m_Parameters.distanceToCover, 0, &coverNormal);

	SAICoverRequest& coverRequest = m_State.coverRequest;

	coverRequest.SetCoverLocation(coverPosition, -coverNormal);

	const Vec3 agentPosition = GetPhysicsPos();

	if (IsMovingInCover())
	{
		SetDesiredBodyDirectionAtTarget(-coverNormal);

		// Turn body to the direction we're moving
		const Vec3 directionAgentIsMovingIn = (coverPosition - agentPosition).GetNormalized();
		coverRequest.SetCoverBodyDirection(coverNormal, directionAgentIsMovingIn);
	}
	else
	{
		// Give hints to the animation system for sliding into cover
		SetDesiredBodyDirectionAtTarget(-coverNormal); // TODO: Change this to something like DesiredCoverLocation when old cover alignment method is not used anymore

		// Update the desired cover body direction while we're further
		// away from the cover than a certain threshold.
		// The threshold is there to prevent us from setting the cover
		// body direction while being very close, because we could
		// by accident overshoot slightly and set the wrong direction.

		if (agentPosition.GetSquaredDistance(coverPosition) > square(1.0f) ||
			coverRequest.coverBodyDirection == eCoverBodyDirection_Unspecified)
		{
			// Turn the body towards the direction we're coming from.
			// This is because we know that the direction we're coming from
			// was traversable and therefor makes somewhat sense to look
			// towards. If we look in the direction we're running towards,
			// or in the direction of our target we might end up with
			// characters looking straight into the wall.

			const Vec3 directionAgentIsComingFrom = (agentPosition - coverPosition).GetNormalized();
			coverRequest.SetCoverBodyDirection(coverNormal, directionAgentIsComingFrom);
		}

		enum
		{
			MovingIntoLowCover = 1,
		};

		SetMovementContext(MovingIntoLowCover);
	}
}

//
//----------------------------------------------------------------------------------------------
EAimState CPipeUser::GetAimState() const
{
	return m_aimState;
}

void CPipeUser::SetActorTargetRequest(const SAIActorTargetRequest& req)
{
	CAIObject* pTarget = GetOrCreateSpecialAIObject(AISPECIAL_ANIM_TARGET);

	if (!m_pActorTargetRequest)
		m_pActorTargetRequest = new SAIActorTargetRequest;
	*m_pActorTargetRequest = req;
	pTarget->SetPos(m_pActorTargetRequest->approachLocation, m_pActorTargetRequest->approachDirection);
}

const SAIActorTargetRequest* CPipeUser::GetActiveActorTargetRequest() const
{
	CAIObject* pPathFindTarget = m_refPathFindTarget.GetAIObject();
	if (pPathFindTarget && pPathFindTarget->GetSubType() == STP_ANIM_TARGET && m_pActorTargetRequest)
		return m_pActorTargetRequest;
	return 0;
}

void CPipeUser::ClearActorTargetRequest()
{
	SAFE_DELETE(m_pActorTargetRequest);
}

void CPipeUser::HandleVisualStimulus(SAIEVENT* pAIEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	const float fGlobalVisualPerceptionScale = gEnv->pAISystem->GetGlobalVisualScale(this);
	const float fVisualPerceptionScale = m_Parameters.m_PerceptionParams.perceptionScale.visual * fGlobalVisualPerceptionScale;
	if (gAIEnv.CVars.IgnoreVisualStimulus != 0 || m_Parameters.m_bAiIgnoreFgNode || fVisualPerceptionScale <= 0.0f)
		return;

	if (gAIEnv.pTargetTrackManager->IsEnabled())
	{
		// Check if in range (using perception scale)
		if (eFOV_Outside != IsPointInFOV(pAIEvent->vPosition, fVisualPerceptionScale))
		{
			gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pAIEvent, TargetTrackHelpers::eEST_Visual);

			IEntity* pEventOwnerEntity = gEnv->pEntitySystem->GetEntity(pAIEvent->sourceId);
			if (!pEventOwnerEntity)
				return;

			IAIObject* pEventOwnerAI = pEventOwnerEntity->GetAI();
			if (!pEventOwnerAI)
				return;

			if (IsHostile(pEventOwnerAI))
			{
				m_State.nTargetType = static_cast<CAIObject*>(pEventOwnerAI)->GetType();
				m_stimulusStartTime = GetAISystem()->GetFrameStartTimeSeconds();

				m_AttTargetThreat = m_State.eTargetThreat = AITHREAT_AGGRESSIVE;
				m_AttTargetType = m_State.eTargetType = AITARGET_VISUAL;

				CWeakRef<CAIObject> refAttentionTarget = GetWeakRef(static_cast<CAIObject*>(pEventOwnerAI));
				if (refAttentionTarget != m_refAttentionTarget)
				{
					SetAttentionTarget(refAttentionTarget);
				}
			}
		}
	}
}

void CPipeUser::HandleSoundEvent(SAIEVENT* pAIEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fGlobalAudioPerceptionScale = gEnv->pAISystem->GetGlobalAudioScale(this);
	const float fAudioPerceptionScale = m_Parameters.m_PerceptionParams.perceptionScale.audio * fGlobalAudioPerceptionScale;
	if (gAIEnv.CVars.IgnoreSoundStimulus != 0 || m_Parameters.m_bAiIgnoreFgNode || fAudioPerceptionScale <= 0.0f)
		return;

	if (gAIEnv.pTargetTrackManager->IsEnabled())
	{
		// Check if in range (using perception scale)
		const Vec3& vMyPos = GetPos();
		const float fSoundDistance = vMyPos.GetDistance(pAIEvent->vPosition) * (1.0f / fAudioPerceptionScale);
		if (fSoundDistance <= pAIEvent->fThreat)
		{
			gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pAIEvent, TargetTrackHelpers::eEST_Sound);

			IEntity* pEventOwnerEntity = gEnv->pEntitySystem->GetEntity(pAIEvent->sourceId);
			if (!pEventOwnerEntity)
				return;

			IAIObject* pEventOwnerAI = pEventOwnerEntity->GetAI();
			if (!pEventOwnerAI)
				return;

			if (IsHostile(pEventOwnerAI))
			{
				if ((m_AttTargetType != AITARGET_MEMORY) && (m_AttTargetType != AITARGET_VISUAL))
				{
					m_State.nTargetType = static_cast<CAIObject*>(pEventOwnerAI)->GetType();
					m_stimulusStartTime = GetAISystem()->GetFrameStartTimeSeconds();

					m_AttTargetThreat = m_State.eTargetThreat = AITHREAT_AGGRESSIVE;
					m_AttTargetType = m_State.eTargetType = AITARGET_SOUND;

					SetAttentionTarget(GetWeakRef(static_cast<CAIObject*>(pEventOwnerAI)));
				}
			}
		}
	}
}

void CPipeUser::CalculatePathObstacles()
{
	m_pathAdjustmentObstacles.CalculateObstaclesAroundActor(this);
}

void CPipeUser::SetLastActionStatus(bool bSucceed)
{
	m_bLastActionSucceed = bSucceed;
}

void CPipeUser::UpdateLookTarget(CAIObject* pTarget)
{
	CCCPOINT(CPipeUser_UpdateLookTarget);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Don't look at targets that aren't at least suspicious
	if (pTarget && (pTarget == m_refAttentionTarget.GetAIObject()) && (GetAttentionTargetThreat() <= AITHREAT_SUSPECT))
	{
		pTarget = NULL;
	}

	// If not moving, allow to look at target.
	bool bLookAtTarget = (m_State.fDesiredSpeed < 0.01f) || m_Path.Empty();

	if (m_bLooseAttention)
	{
		CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
		if (pLooseAttentionTarget)
		{
			pTarget = pLooseAttentionTarget;
		}
	}

	Vec3 vLookTargetPos(ZERO);

	if (m_fireMode == FIREMODE_MELEE || m_fireMode == FIREMODE_MELEE_FORCED)
	{
		if (pTarget)
		{
			vLookTargetPos = pTarget->GetPos();
			vLookTargetPos.z = GetPos().z;
			bLookAtTarget = true;
		}
	}

	bool use3DNav = IsUsing3DNavigation();
	bool isMoving = (m_State.fDesiredSpeed > 0.f) && (m_State.curActorTargetPhase == eATP_None) && !m_State.vMoveDir.IsZero();

	float distToTarget = FLT_MAX;
	if (pTarget)
	{
		Vec3 dirToTarget = pTarget->GetPos() - GetPos();
		distToTarget = dirToTarget.GetLength();
		if (distToTarget > 0.0001f)
		{
			dirToTarget /= distToTarget;
		}

		// Allow to look at the target when it is almost at the movement direction or very close.
		if (isMoving)
		{
			Vec3 vMoveDir = m_State.vMoveDir;
			if (!use3DNav)
			{
				vMoveDir.z = 0.f;
			}
			vMoveDir.NormalizeSafe(Vec3Constants<float>::fVec3_Zero);
			if (distToTarget < 2.5f || (vMoveDir.Dot(dirToTarget) > cosf(DEG2RAD(60))))
			{
				bLookAtTarget = true;
			}
		}
	}

	if (bLookAtTarget && pTarget)
	{
		Vec3 vTargetPos = pTarget->GetPos();

		const float maxDeviation = distToTarget * sinf(DEG2RAD(15));

		if (distToTarget > GetParameters().m_fPassRadius)
		{
			vLookTargetPos = vTargetPos;
			Limit(vLookTargetPos.z, GetPos().z - maxDeviation, GetPos().z + maxDeviation);
		}

		// Clamp the lookat height when the target is close.
		unsigned short int nTargetType = pTarget->GetType();
		if ((distToTarget < 1.f) ||
		    (nTargetType == AIOBJECT_DUMMY || nTargetType == AIOBJECT_HIDEPOINT || nTargetType == AIOBJECT_WAYPOINT ||
		     nTargetType > AIOBJECT_PLAYER) && (distToTarget < 5.f)) // anchors & dummy objects
		{
			if (!use3DNav)
			{
				vLookTargetPos = vTargetPos;
				Limit(vLookTargetPos.z, GetPos().z - maxDeviation, GetPos().z + maxDeviation);
			}
		}
	}
	else if (isMoving && (gAIEnv.configuration.eCompatibilityMode != ECCM_CRYSIS2))
	{
		// Look forward or to the movement direction
		Vec3 vLookAheadPoint;
		float lookAheadDist = 2.5f;

		if (m_pPathFollower)
		{
			float junk;
			vLookAheadPoint = m_pPathFollower->GetPathPointAhead(lookAheadDist, junk);
		}
		else
		{
			if (!m_Path.GetPosAlongPath(vLookAheadPoint, lookAheadDist, !m_movementAbility.b3DMove, true))
			{
				vLookAheadPoint = GetPhysicsPos();
			}
		}

		// Since the path height is not guaranteed to follow terrain, do not even try to look up or down.
		vLookTargetPos = vLookAheadPoint;

		// Make sure the lookahead position is far enough so that the catchup logic in the path following
		// together with look-ik does not get flipped.
		Vec3 delta = vLookTargetPos - GetPhysicsPos();
		delta.z = 0.f;
		float dist = delta.GetLengthSquared();
		if (dist < 1.f)
		{
			float u = 1.f - sqrtf(dist);
			Vec3 safeDir = GetEntityDir();
			safeDir.z = 0;
			delta = delta + (safeDir - delta) * u;
		}
		delta.Normalize();

		vLookTargetPos = GetPhysicsPos() + delta * 40.f;
		vLookTargetPos.z = GetPos().z;
	}
	else
	{
		// Disable look target.
		vLookTargetPos.zero();
	}

	if (!m_posLookAtSmartObject.IsZero())
	{
		// The SO lookat should override the lookat target in case not requesting to fire and not using lookat goalop.
		if (!m_bLooseAttention && (m_fireMode == FIREMODE_OFF))
		{
			vLookTargetPos = m_posLookAtSmartObject;
		}
	}

	if (!vLookTargetPos.IsZero())
	{
		float distSq = Distance::Point_Point2DSq(vLookTargetPos, GetPos());
		if (distSq < sqr(2.f))
		{
			Vec3 fakePos = GetPos() + GetEntityDir() * 2.f;
			if (distSq < sqr(0.7f))
			{
				vLookTargetPos = fakePos;
			}
			else
			{
				float speed = m_State.vMoveDir.GetLength();
				speed = clamp_tpl(speed, 0.f, 10.f);
				float d = sqrtf(distSq);
				float u = 1.f - (d - 0.7f) / (2.f - 0.7f);
				vLookTargetPos += speed / 10.f * u * (fakePos - vLookTargetPos);
			}
		}
	}

	// for the in-vehicle gunners
	if (GetProxy())
	{
		const SAIBodyInfo& bodyInfo = GetBodyInfo();

		if (IEntity* pLinkedVehicleEntity = bodyInfo.GetLinkedVehicleEntity())
		{
			if (GetProxy()->GetActorIsFallen())
			{
				vLookTargetPos.zero();
			}
			else
			{
				CAIObject* pUnit = static_cast<CAIObject*>(pLinkedVehicleEntity->GetAI());
				if (pUnit)
				{
					if (pUnit->CastToCAIVehicle())
					{
						vLookTargetPos.zero();
						CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
						if (m_bLooseAttention && pLooseAttentionTarget)
						{
							pTarget = pLooseAttentionTarget;
						}
						if (pTarget)
						{
							vLookTargetPos = pTarget->GetPos();
							m_State.allowStrafing = false;
						}
					}
				}
			}
		}
	}

	m_State.vLookTargetPos = vLookTargetPos;
}

void CPipeUser::EnableUpdateLookTarget(bool bEnable /*= true*/)
{
	m_bEnableUpdateLookTarget = bEnable;
}

void CPipeUser::OnAIHandlerSentSignal(const char* szText, uint32 crcCode)
{
	assert(crcCode != 0);

	ListWaitGoalOps::iterator it = m_listWaitGoalOps.begin();
	while (it != m_listWaitGoalOps.end())
	{
		COPWaitSignal* pGoalOp = *it;
		if (pGoalOp->NotifySignalReceived(this, szText, NULL))
			it = m_listWaitGoalOps.erase(it);
		else
			++it;
	}

	CAIActor::OnAIHandlerSentSignal(szText, crcCode);
}

Movement::PathfinderState CPipeUser::GetPathfinderState()
{
	switch (m_nPathDecision)
	{
	case PATHFINDER_STILLFINDING:
		return Movement::StillFinding;

	case PATHFINDER_PATHFOUND:
		return Movement::FoundPath;

	case PATHFINDER_NOPATH:
		return Movement::CouldNotFindPath;

	default:
		assert(false);
		return Movement::CouldNotFindPath;
	}
}

void CPipeUser::HandleNavSOFailure()
{
	// Handle navigational smart object failure case. The start and finish states are handled
	// before executing the goalpipes, but since trace goalop can set the error too, the error is handled
	// here right after the goalops are executed. If this was done later, the state syncing code below would
	// clear the flag.
	if (m_eNavSOMethod == nSOmSignalAnimation || m_eNavSOMethod == nSOmActionAnimation)
	{
		if (m_State.curActorTargetPhase == eATP_Error)
		{
			// modify smart object states
			if (!m_currentNavSOStates.IsEmpty())
			{
				gAIEnv.pSmartObjectManager->ModifySmartObjectStates(GetEntity(), m_currentNavSOStates.sAnimationFailUserStates);
				IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_currentNavSOStates.objectEntId);
				if (pObjectEntity)
					gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pObjectEntity, m_currentNavSOStates.sAnimationFailObjectStates);
				m_currentNavSOStates.Clear();
			}

			if (!m_pendingNavSOStates.IsEmpty())
			{
				gAIEnv.pSmartObjectManager->ModifySmartObjectStates(GetEntity(), m_pendingNavSOStates.sAnimationFailUserStates);
				IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_pendingNavSOStates.objectEntId);
				if (pObjectEntity)
					gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pObjectEntity, m_pendingNavSOStates.sAnimationFailObjectStates);
				m_pendingNavSOStates.Clear();
			}

			m_eNavSOMethod = nSOmNone;
		}
	}
}

void CPipeUser::SyncActorTargetPhaseWithAIProxy()
{
	//
	// Sync the actor target phase with the AIProxy
	// The syncing should only happen here. The Trace goalop is allowed to
	// make a request and set error state if necessary.
	//

	bool animationStarted = false;

	switch (m_State.curActorTargetPhase)
	{
	case eATP_Waiting:
	case eATP_Starting:
		break;

	case eATP_Started:
		//m_State.actorTargetPhase = eATP_Playing;
		animationStarted = true;
		break;

	case eATP_StartedAndFinished:
		animationStarted = true;
		//		m_State.actorTargetPhase = eATP_None;
		m_State.actorTargetReq.Reset();
		break;

	case eATP_Finished:
		m_State.curActorTargetPhase = eATP_None;
		m_State.actorTargetReq.Reset();
		break;

	case eATP_Error:
		// m_State.actorTargetReq shouldn't get reset here but only when path
		// is cleared after COPTrace has processed the error, otherwise
		// COPTrace may finish with AIGOR_SUCCEED instead of AIGOR_FAILED.
		break;
	}

	// Inform the listeners of current goal pipe that an animation has been started.
	if (animationStarted && m_eNavSOMethod == nSOmNone)
	{
		CGoalPipe* pCurrent = m_pCurrentGoalPipe;
		if (pCurrent)
		{
			while (pCurrent->GetSubpipe())
				pCurrent = pCurrent->GetSubpipe();
			NotifyListeners(pCurrent, ePN_AnimStarted);
		}
	}
}
