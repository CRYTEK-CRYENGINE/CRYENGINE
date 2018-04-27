// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name: GoalOp_Crysis2.cpp
   Description: Crysis2 goalops
   These should move into GameDLL when interfaces allow!
   -------------------------------------------------------------------------
   History:
   - 18:11:2009 - Created by Márcio Martins
 *********************************************************************/

#include "StdAfx.h"
#include "GoalOp_Crysis2.h"
#include "GoalOpTrace.h"

#include "PipeUser.h"
#include "Puppet.h"
#include "DebugDrawContext.h"
#include "HideSpot.h"
#include "Cover/CoverSystem.h"
#include "GenericAStarSolver.h"
#include "ObjectContainer.h"

#include "Communication/CommunicationManager.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "AIPlayer.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

namespace COPCrysis2Utils
{
//
//-------------------------------------------------------------------------------------------------------------
// Turns body to the left when 'targetPos' is left of 'pos' (with hideDir a vector pointing out of the cover object)
static void TurnBodyToTargetInCover(CPipeUser* pPipeUser, const Vec3& pos, const Vec3& hideDir, const Vec3& targetPos)
{
	SOBJECTSTATE& state = pPipeUser->GetState();
	state.coverRequest.SetCoverBodyDirection(hideDir, targetPos - pos);
}
}

//
//-------------------------------------------------------------------------------------------------------------
IGoalOp* CGoalOpFactoryCrysis2::GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int firstParam, GoalParameters& params) const
{
	EGoalOperations op = CGoalPipe::GetGoalOpEnum(sGoalOpName);

	switch (op)
	{
	case eGO_ADJUSTAIM:
		{
			params.nValue = 0;
			params.fValue = 0.0f; // timeout
			int hide = 0;
			int useLastOpResultAsBackup = 0;
			int allowProne = 0;

			pH->GetParam(firstParam, hide);
			if (pH->GetParamCount() > firstParam)
				pH->GetParam(firstParam + 1, params.fValue); // timeout
			if (pH->GetParamCount() > firstParam + 1)
				pH->GetParam(firstParam + 2, useLastOpResultAsBackup);
			if (pH->GetParamCount() > firstParam + 2)
				pH->GetParam(firstParam + 3, allowProne);

			if (hide)
				params.nValue |= 0x1;

			if (useLastOpResultAsBackup)
				params.nValue |= 0x2;

			if (allowProne)
				params.nValue |= 0x4;
		}
		break;
	case eGO_PEEK:
		{
			params.nValue = 0;
			params.fValue = 0.0f; // timeout
			int useLastOpResultAsBackup = 0;

			pH->GetParam(firstParam, params.fValue); // timeout
			if (pH->GetParamCount() > firstParam)
				pH->GetParam(firstParam + 1, useLastOpResultAsBackup);

			if (useLastOpResultAsBackup)
				params.nValue |= 0x1;
		}
		break;
	case eGO_HIDE:
		{
			params.nValue = AI_REG_COVER;
			params.bValue = true;

			pH->GetParam(firstParam, params.nValue); // location
			if (pH->GetParamCount() > firstParam)
				pH->GetParam(firstParam + 1, params.bValue); // exact
		}
		break;
	case eGO_STICKPATH:
		{
			const int paramCount = pH->GetParamCount();

			// finishInRange
			bool bFinishInRange = true;
			pH->GetParam(firstParam, bFinishInRange);
			params.nValue |= (bFinishInRange ? 0x02 : 0);

			// stick distance max
			pH->GetParam(firstParam + 1, params.vPos.y);

			if (paramCount > firstParam + 1)
			{
				// stick distance min
				pH->GetParam(firstParam + 2, params.vPos.x);

				if (paramCount > firstParam + 2)
				{
					// canReverse
					bool bCanReverse = true;
					pH->GetParam(firstParam + 3, bCanReverse);
					params.nValue |= (bCanReverse ? 0x01 : 0);
				}
			}
		}
		break;
	case eGO_FIREWEAPONS:
		pH->GetParam(firstParam, params.nValue);
		pH->GetParam(firstParam + 1, params.fValue);
		pH->GetParam(firstParam + 2, params.fValueAux);
		pH->GetParam(firstParam + 3, params.bValue);
		pH->GetParam(firstParam + 4, params.nValueAux);
		break;
	case eGO_HOVER:
		pH->GetParam(firstParam, params.nValue);
		pH->GetParam(firstParam + 1, params.bValue);
		break;

	default:
		return NULL;
	}

	return GetGoalOp(op, params);
}

//
//-------------------------------------------------------------------------------------------------------------
IGoalOp* CGoalOpFactoryCrysis2::GetGoalOp(EGoalOperations op, GoalParameters& params) const
{
	IGoalOp* pResult = NULL;

	switch (op)
	{
	case eGO_ADJUSTAIM:
		{
			pResult = new COPCrysis2AdjustAim((params.nValue & 0x02) != 0, (params.nValue & 0x04) != 0, params.fValue);
		}
		break;
	case eGO_PEEK:
		{
			pResult = new COPCrysis2Peek((params.nValue & 0x02) != 0, params.fValue);
		}
		break;
	case eGO_HIDE:
		{
			pResult = new COPCrysis2Hide(static_cast<EAIRegister>(params.nValue), params.bValue);
		}
		break;
	case eGO_COMMUNICATE:
		{
			pResult = new COPCrysis2Communicate(CommID(params.nValue), CommChannelID(params.nValueAux), params.fValue, static_cast<EAIRegister>(int(params.fValueAux)),
			                                    params.bValue ? SCommunicationRequest::Unordered : SCommunicationRequest::Ordered);
		}
		break;
	case eGO_STICKPATH:
		{
			// nValue & 0x01 == canReverse
			// nValue & 0x02 == finishInRange
			pResult = new COPCrysis2StickPath((params.nValue & 0x02) != 0, params.vPos.y, params.vPos.x, (params.nValue & 0x01) != 0);
		}
		break;
	case eGO_HOVER:
		{
			pResult = new COPCrysis2Hover(static_cast<EAIRegister>(params.nValue), params.bValue);
		}
		break;
	case eGO_FLY:
		{
			pResult = new COPCrysis2Fly();
		}
		break;
	case eGO_ACQUIREPOSITION:
		{
			pResult = new COPAcquirePosition();
		}
		break;
	case eGO_CHASETARGET:
		{
			pResult = new COPCrysis2ChaseTarget();
		}
		break;
	case eGO_FIREWEAPONS:
		{
			pResult = new COPCrysis2FlightFireWeapons(static_cast<EAIRegister>(params.nValue), params.fValue, params.fValueAux, params.bValue, static_cast<uint32>(params.nValueAux));
		}
		break;
	default:
		return NULL;
	}

	return pResult;
}

//
//-------------------------------------------------------------------------------------------------------------
COPCrysis2AdjustAim::COPCrysis2AdjustAim(bool useLastOpAsBackup, bool allowProne, float timeout) :
	m_useLastOpAsBackup(useLastOpAsBackup),
	m_allowProne(allowProne),
	m_timeoutMs((int)(timeout * 1000.0f)),
	m_timeoutRandomness(0.0f),
	m_bestPostureID(-1),
	m_queryID(0)
{
	m_startTime.SetValue(0);
	m_lastGood.SetValue(0);

	m_nextUpdateMs = RandomizeTimeInterval();
	m_runningTimeoutMs = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
COPCrysis2AdjustAim::COPCrysis2AdjustAim(const XmlNodeRef& node) :
	m_useLastOpAsBackup(s_xml.GetBool(node, "useLastOpResultAsBackup")),
	m_allowProne(s_xml.GetBool(node, "allowProne")),
	m_timeoutRandomness(0.0f),
	m_timeoutMs(0),
	m_bestPostureID(-1),
	m_queryID(0)
{
	float timeout = 0.0f;
	node->getAttr("timeout", timeout);
	m_timeoutMs = (int)(timeout * 1000.0f);

	node->getAttr("timeoutRandomness", m_timeoutRandomness);

	m_startTime.SetValue(0);
	m_lastGood.SetValue(0);

	m_nextUpdateMs = RandomizeTimeInterval();
	m_runningTimeoutMs = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
int COPCrysis2AdjustAim::RandomizeTimeInterval() const
{
	return cry_random(250, 500);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPCrysis2AdjustAim::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPCrysis2AdjustAim_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CPuppet* pPuppet = pPipeUser->CastToCPuppet();
	if (!pPuppet)
		return eGOR_FAILED;

	CTimeValue now(GetAISystem()->GetFrameStartTime());

	if (!m_startTime.GetValue())
	{
		m_runningTimeoutMs = m_timeoutMs;

		if (m_timeoutRandomness > 0.0001f)
			m_runningTimeoutMs += (int)cry_random(0.0f, m_timeoutRandomness * 1000.0f);

		m_startTime = now;
		m_lastGood = now;

		pPipeUser->SetAdjustingAim(true);
	}

	CAIObject* pLastOpResult = pPipeUser->m_refLastOpResult.GetAIObject();

	Vec3 target(ZERO);

	if (pPipeUser->GetAttentionTarget())
		target = pPipeUser->GetAttentionTarget()->GetPos();
	else if (m_useLastOpAsBackup && (pLastOpResult != NULL))
		target = pLastOpResult->GetPos();
	else
	{
		Reset(pPipeUser);

		return eGOR_FAILED;
	}

	bool isMoving = (pPipeUser->m_State.fDesiredSpeed >= 0.001f) && !pPipeUser->m_State.vMoveDir.IsZero();
	int64 elapsedMs = (now - m_startTime).GetMilliSecondsAsInt64();

	if ((elapsedMs >= m_nextUpdateMs) && isMoving)
		m_nextUpdateMs += 150;
	else if (elapsedMs >= m_nextUpdateMs)
	{
		PostureManager::PostureID postureID = -1;
		PostureManager::PostureInfo* posture;

		PostureManager& postureManager = pPuppet->GetPostureManager();

		if (!m_queryID)
		{
			uint32 checks = PostureManager::DefaultChecks;
			if (pPuppet->GetFireMode() == FIREMODE_OFF)
				checks &= ~PostureManager::CheckAimability;

			PostureManager::PostureQuery query;
			query.actor = pPuppet;
			query.allowLean = true;
			query.allowProne = m_allowProne;
			query.checks = checks;

			if (pPuppet->IsInCover())
				query.coverID = pPuppet->GetCoverID();

			query.distancePercent = 0.85f;
			query.hintPostureID = m_bestPostureID;
			query.position = pPuppet->GetPhysicsPos();
			query.target = target;
			query.type = PostureManager::AimPosture;

			m_queryID = postureManager.QueryPosture(query);
			if (!m_queryID)
			{
				if (!ProcessQueryResult(pPipeUser, AsyncFailed, 0))
				{
					Reset(pPipeUser);

					return eGOR_FAILED;
				}
			}
		}
		else
		{
			AsyncState queryStatus = postureManager.GetPostureQueryResult(m_queryID, &m_bestPostureID, &posture);

			if (queryStatus != AsyncInProgress)
			{
				m_queryID = 0;
				if (!ProcessQueryResult(pPipeUser, queryStatus, posture))
				{
					Reset(pPipeUser);

					return eGOR_FAILED;
				}
			}
		}

		m_nextUpdateMs += RandomizeTimeInterval();
		if (pPipeUser->m_State.bodystate == STANCE_PRONE)
			m_nextUpdateMs += 2000;
	}

	bool finished = (m_runningTimeoutMs > 0) && (elapsedMs >= m_runningTimeoutMs);
	if (!finished)
	{
		int timeToNextShotMs = (int)(pPuppet->GetTimeToNextShot() * 1000.0f);

		finished = (timeToNextShotMs > 0) && (timeToNextShotMs > (m_runningTimeoutMs - elapsedMs));
	}

	if (finished)
	{
		Reset(pPipeUser);

		return eGOR_SUCCEEDED;
	}

	return eGOR_IN_PROGRESS;
}

bool COPCrysis2AdjustAim::ProcessQueryResult(CPipeUser* pipeUser, AsyncState queryStatus,
                                             PostureManager::PostureInfo* postureInfo)
{
	CRY_ASSERT(pipeUser);

	CTimeValue now(GetAISystem()->GetFrameStartTime());

	if (queryStatus != AsyncFailed)
	{
		CRY_ASSERT(postureInfo);

		pipeUser->m_State.bodystate = postureInfo->stance;
		pipeUser->m_State.lean = postureInfo->lean;
		pipeUser->m_State.peekOver = postureInfo->peekOver;

		const bool coverAction = pipeUser->IsInCover();
		if (coverAction)
		{
			const char* const coverActionName = postureInfo->agInput.c_str();
			pipeUser->m_State.coverRequest.SetCoverAction(coverActionName, postureInfo->lean);
		}
		else
		{
			if (!postureInfo->agInput.empty())
				pipeUser->GetProxy()->SetAGInput(AIAG_ACTION, postureInfo->agInput.c_str());
			else
				pipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);
		}

		m_lastGood = now;
	}
	else
	{
		if ((now - m_lastGood).GetMilliSecondsAsInt64() > 500)
		{
			pipeUser->SetSignal(1, "OnNoAimPosture", 0, 0, gAIEnv.SignalCRCs.m_nOnNoAimPosture);

			return false;
		}

		// Could not find new pose, keep stance and remove peek.
		pipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);
		pipeUser->m_State.lean = 0;
		pipeUser->m_State.peekOver = 0;
		pipeUser->m_State.coverRequest.ClearCoverAction();
	}

	return true;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCrysis2AdjustAim::Reset(CPipeUser* pPipeUser)
{
	pPipeUser->m_State.lean = 0.0f;
	pPipeUser->m_State.peekOver = 0.0f;
	pPipeUser->m_State.coverRequest.ClearCoverAction();
	pPipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);
	pPipeUser->SetAdjustingAim(false);

	if (m_queryID)
	{
		pPipeUser->CastToCPuppet()->GetPostureManager().CancelPostureQuery(m_queryID);
		m_queryID = 0;
	}

	m_startTime.SetValue(0);
	m_lastGood.SetValue(0);
	m_nextUpdateMs = RandomizeTimeInterval();
	m_bestPostureID = -1;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCrysis2AdjustAim::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCrysis2AdjustAim");
	{
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_nextUpdateMs", m_nextUpdateMs);
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);
	}
	ser.EndGroup();
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCrysis2AdjustAim::DebugDraw(CPipeUser* pPipeUser) const
{
	if (!pPipeUser)
		return;

	// m_selector.DebugDraw(pPipeUser);

	Vec3 basePos = pPipeUser->GetPhysicsPos();
	Vec3 targetPos = pPipeUser->GetProbableTargetPosition();

	CDebugDrawContext dc;

	/* for(unsigned i = 0; i < 8; ++i)
	   {
	   if(!m_postures[i].valid) continue;
	   dc->DrawCone(m_postures[i].weapon, m_postures[i].weaponDir, 0.05f, m_postures[i].targetAim ? 0.4f : 0.2f, ColorB(255,0,0, m_postures[i].targetAim ? 255 : 128));
	   dc->DrawSphere(m_postures[i].eye, 0.1f, ColorB(255,255,255, m_postures[i].targetVis ? 255 : 128));
	   dc->DrawLine(basePos, ColorB(255,255,255), m_postures[i].eye, ColorB(255,255, 255));

	   if((int)i == m_bestPosture)
	   {
	   dc->DrawLine(m_postures[i].weapon, ColorB(255,0,0), targetPos, ColorB(255,0,0,0));
	   dc->DrawLine(m_postures[i].eye, ColorB(255, 255, 255), targetPos, ColorB(255, 255, 255, 0));
	   }
	   }
	 */

	Vec3 toeToHead = pPipeUser->GetPos() - pPipeUser->GetPhysicsPos();
	float len = toeToHead.GetLength();
	if (len > 0.0001f)
		toeToHead *= (len - 0.3f) / len;
	dc->DrawSphere(pPipeUser->GetPhysicsPos() + toeToHead, 0.4f, ColorB(200, 128, 0, 90));

	dc->DrawLine(pPipeUser->GetFirePos(), ColorB(255, 0, 0), targetPos, ColorB(255, 0, 0, 64));
	dc->DrawLine(pPipeUser->GetPos(), ColorB(255, 255, 255), targetPos, ColorB(255, 255, 255, 64));

	CPuppet* pPuppet = pPipeUser->CastToCPuppet();
	if (!pPuppet)
		return;

	static string text;

	text = "AIM ";

	if (m_bestPostureID > -1)
	{
		PostureManager::PostureInfo posture;
		pPuppet->GetPostureManager().GetPosture(m_bestPostureID, &posture);

		text += posture.name;
	}
	else
		text += "<$4No Posture$o>";

	const Vec3 agentHead = pPipeUser->GetPos();
	const ColorB white(255, 255, 255, 255);
	const float fontSize = 1.25f;

	float x, y, z;
	if (!dc->ProjectToScreen(agentHead.x, agentHead.y, agentHead.z, &x, &y, &z))
		return;

	if ((z < 0.0f) || (z > 1.0f))
		return;

	x *= dc->GetWidth() * 0.01f;
	y *= dc->GetHeight() * 0.01f;

	y += 100.0f;
	dc->Draw2dLabel(x, y, fontSize, white, true, "%s", text.c_str());
}

//
//-------------------------------------------------------------------------------------------------------------
COPCrysis2Peek::COPCrysis2Peek(bool useLastOpAsBackup, float timeout) :
	m_useLastOpAsBackup(useLastOpAsBackup),
	m_timeoutMs((int)(timeout * 1000.0f)),
	m_timeoutRandomness(0.0f),
	m_bestPostureID(-1),
	m_queryID(0)
{
	m_startTime.SetValue(0);
	m_lastGood.SetValue(0);

	m_nextUpdateMs = RandomizeTimeInterval();
	m_runningTimeoutMs = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
COPCrysis2Peek::COPCrysis2Peek(const XmlNodeRef& node) :
	m_useLastOpAsBackup(s_xml.GetBool(node, "useLastOpResultAsBackup")),
	m_timeoutRandomness(0.0f),
	m_timeoutMs(0),
	m_bestPostureID(-1),
	m_queryID(0)
{
	float timeout = 0.0f;
	node->getAttr("timeout", timeout);
	m_timeoutMs = (int)(timeout * 1000.0f);

	node->getAttr("timeoutRandomness", m_timeoutRandomness);

	m_startTime.SetValue(0);
	m_lastGood.SetValue(0);

	m_nextUpdateMs = RandomizeTimeInterval();
	m_runningTimeoutMs = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
int COPCrysis2Peek::RandomizeTimeInterval() const
{
	return cry_random(250, 500);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPCrysis2Peek::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPCrysis2Peek_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CPuppet* pPuppet = pPipeUser->CastToCPuppet();
	if (!pPuppet)
		return eGOR_FAILED;

	if (!pPuppet->IsInCover())
	{
		Reset(pPipeUser);

		return eGOR_FAILED;
	}

	CTimeValue now(GetAISystem()->GetFrameStartTime());

	if (!m_startTime.GetValue())
	{
		m_runningTimeoutMs = m_timeoutMs;

		if (m_timeoutRandomness > 0.0001f)
			m_runningTimeoutMs += (int)cry_random(0.0f, m_timeoutRandomness * 1000.0f);

		m_startTime = now;
		m_lastGood = now;
	}

	CAIObject* pLastOpResult = pPipeUser->m_refLastOpResult.GetAIObject();

	Vec3 target;

	if (pPipeUser->GetAttentionTarget())
		target = pPipeUser->GetAttentionTarget()->GetPos();
	else if (m_useLastOpAsBackup && (pLastOpResult != NULL))
		target = pLastOpResult->GetPos();
	else
	{
		Reset(pPipeUser);

		return eGOR_FAILED;
	}

	int64 elapsedMs = (now - m_startTime).GetMilliSecondsAsInt64();

	if (elapsedMs >= m_nextUpdateMs)
	{
		PostureManager::PostureID postureID = -1;
		PostureManager::PostureInfo* posture;

		PostureManager& postureManager = pPuppet->GetPostureManager();

		if (!m_queryID)
		{
			uint32 checks = PostureManager::CheckVisibility | PostureManager::CheckLeanability;

			PostureManager::PostureQuery query;
			query.actor = pPuppet;
			query.allowLean = true;
			query.allowProne = false;
			query.checks = checks;
			query.coverID = pPuppet->GetCoverID();
			query.distancePercent = 0.25f;
			query.hintPostureID = m_bestPostureID;
			query.position = pPuppet->GetPhysicsPos();
			query.target = target;
			query.type = PostureManager::PeekPosture;

			m_queryID = postureManager.QueryPosture(query);
			if (!m_queryID)
			{
				if (!ProcessQueryResult(pPipeUser, AsyncFailed, 0))
				{
					Reset(pPipeUser);

					return eGOR_FAILED;
				}
			}
		}
		else
		{
			AsyncState queryStatus = postureManager.GetPostureQueryResult(m_queryID, &m_bestPostureID, &posture);

			if (queryStatus != AsyncInProgress)
			{
				m_queryID = 0;
				if (!ProcessQueryResult(pPipeUser, queryStatus, posture))
				{
					Reset(pPipeUser);

					return eGOR_FAILED;
				}
			}
		}

		m_nextUpdateMs += RandomizeTimeInterval();
	}

	bool finished = (m_runningTimeoutMs > 0) && (elapsedMs >= m_runningTimeoutMs);

	if (finished)
	{
		Reset(pPipeUser);

		return eGOR_SUCCEEDED;
	}

	return eGOR_IN_PROGRESS;
}

bool COPCrysis2Peek::ProcessQueryResult(CPipeUser* pipeUser, AsyncState queryStatus,
                                        PostureManager::PostureInfo* postureInfo)
{
	assert(pipeUser);

	CTimeValue now(GetAISystem()->GetFrameStartTime());

	if (queryStatus != AsyncFailed)
	{
		assert(postureInfo);

		pipeUser->m_State.bodystate = postureInfo->stance;
		pipeUser->m_State.lean = postureInfo->lean;
		pipeUser->m_State.peekOver = postureInfo->peekOver;

		const bool coverAction = pipeUser->IsInCover();
		if (coverAction)
		{
			const char* const coverActionName = postureInfo->agInput.c_str();
			pipeUser->m_State.coverRequest.SetCoverAction(coverActionName, postureInfo->lean);
		}
		else
		{
			if (!postureInfo->agInput.empty())
				pipeUser->GetProxy()->SetAGInput(AIAG_ACTION, postureInfo->agInput.c_str());
			else
				pipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);
		}

		m_lastGood = now;
	}
	else
	{
		if ((now - m_lastGood).GetMilliSecondsAsInt64() > 500)
		{
			pipeUser->SetSignal(1, "OnNoPeekPosture", 0, 0, gAIEnv.SignalCRCs.m_nOnNoPeekPosture);

			return false;
		}

		// Could not find new pose, keep stance and remove peek.
		pipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);
		pipeUser->m_State.lean = 0;
		pipeUser->m_State.peekOver = 0;
		pipeUser->m_State.coverRequest.ClearCoverAction();
	}

	return true;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCrysis2Peek::Reset(CPipeUser* pPipeUser)
{
	pPipeUser->m_State.lean = 0.0f;
	pPipeUser->m_State.peekOver = 0.0f;
	pPipeUser->m_State.coverRequest.ClearCoverAction();
	pPipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);

	if (m_queryID)
	{
		pPipeUser->CastToCPuppet()->GetPostureManager().CancelPostureQuery(m_queryID);
		m_queryID = 0;
	}

	m_startTime.SetValue(0);
	m_lastGood.SetValue(0);
	m_nextUpdateMs = RandomizeTimeInterval();
	m_bestPostureID = -1;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCrysis2Peek::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCrysis2Peek");
	{
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_nextUpdateMs", m_nextUpdateMs);
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);
	}
	ser.EndGroup();
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCrysis2Peek::DebugDraw(CPipeUser* pPipeUser) const
{
	if (!pPipeUser)
		return;

	CPuppet* pPuppet = pPipeUser->CastToCPuppet();
	if (!pPuppet)
		return;

	CDebugDrawContext dc;

	stack_string text;

	text = "AIM ";

	if (m_bestPostureID > -1)
	{
		PostureManager::PostureInfo posture;
		pPuppet->GetPostureManager().GetPosture(m_bestPostureID, &posture);

		text += posture.name;
	}
	else
		text += "<$4No Posture$o>";

	const Vec3 agentHead = pPipeUser->GetPos();
	const ColorB white(255, 255, 255, 255);
	const float fontSize = 1.25f;

	float x, y, z;
	if (!dc->ProjectToScreen(agentHead.x, agentHead.y, agentHead.z, &x, &y, &z))
		return;

	if ((z < 0.0f) || (z > 1.0f))
		return;

	x *= dc->GetWidth() * 0.01f;
	y *= dc->GetHeight() * 0.01f;

	y += 100.0f;
	dc->Draw2dLabel(x, y, fontSize, white, true, "%s", text.c_str());
}

COPCrysis2Hide::COPCrysis2Hide(EAIRegister location, bool exact)
	: m_location(location)
	, m_exact(exact)
	, m_pPathfinder(0)
	, m_pTracer(0)
{
}

COPCrysis2Hide::COPCrysis2Hide(const XmlNodeRef& node) :
	m_location(AI_REG_NONE),
	m_exact(s_xml.GetBool(node, "exact", true)),
	m_pPathfinder(0),
	m_pTracer(0)
{
	s_xml.GetRegister(node, "register", m_location, CGoalOpXMLReader::MANDATORY);
}

COPCrysis2Hide::COPCrysis2Hide(const COPCrysis2Hide& rhs) :
	m_location(rhs.m_location),
	m_exact(rhs.m_exact),
	m_pPathfinder(0),
	m_pTracer(0)
{
}

COPCrysis2Hide::~COPCrysis2Hide()
{
	m_refHideTarget.Release();

	SAFE_DELETE(m_pPathfinder);
	SAFE_DELETE(m_pTracer);
}

EGoalOpResult COPCrysis2Hide::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPCrysis2Hide_Execute);

	if (m_refHideTarget.IsNil())
	{
		CCCPOINT(COPCrysis2Hide_Execute_NoHideTarget);

		pPipeUser->ClearPath("COPCrysis2Hide::Execute");

		if (m_location == AI_REG_REFPOINT)
		{
			CCCPOINT(COPCrysis2Hide_Execute_RefPoint);

			if (IAIObject* pRefPoint = pPipeUser->GetRefPoint())
			{
				// Setup the hide object.
				Vec3 pos = pRefPoint->GetPos();
				Vec3 dir = pRefPoint->GetEntityDir();

				if (dir.IsZero())
				{
					Vec3 target(pPipeUser->GetAttentionTarget() ? pPipeUser->GetAttentionTarget()->GetPos() : pos);
					dir = (target - pos).GetNormalizedSafe();
				}

				SHideSpot hideSpot(SHideSpotInfo::eHST_ANCHOR, pos, dir);
				CAIHideObject& currentHideObject = pPipeUser->m_CurrentHideObject;
				currentHideObject.Set(&hideSpot, hideSpot.info.pos, hideSpot.info.dir);

				if (!currentHideObject.IsValid() ||
				    !GetAISystem()->IsHideSpotOccupied(pPipeUser, hideSpot.info.pos))
				{
					Reset(pPipeUser);

					pPipeUser->SetCoverState(ICoverUser::EStateFlags::None);

					return eGOR_FAILED;
				}

				CreateHideTarget(pPipeUser, pos);
			}
		}
		else // AI_REG_COVER
		{
			CCCPOINT(COPCrysis2Hide_Execute_HideObject);

			if (CoverID coverID = pPipeUser->GetCoverRegister())
			{
				float distanceToCover = pPipeUser->GetParameters().distanceToCover;

				Vec3 normal(1.f, 0, 0);
				Vec3 pos = gAIEnv.pCoverSystem->GetCoverLocation(coverID, distanceToCover, 0, &normal);

				CreateHideTarget(pPipeUser, pos, -normal);

				if (gAIEnv.CVars.CoverExactPositioning)
				{
					SAIActorTargetRequest req;
					req.approachLocation = pos + normal * 0.25f;
					req.approachDirection = -normal;
					req.animLocation = pos;
					req.animDirection = -normal;

					req.directionTolerance = DEG2RAD(5.0f);
					req.startArcAngle = 0.5f;
					req.startWidth = 0.5f;
					req.signalAnimation = false;

					pPipeUser->SetActorTargetRequest(req);
				}

#ifdef CRYAISYSTEM_DEBUG
				if (gAIEnv.CVars.DebugDrawCover)
					GetAISystem()->AddDebugCylinder(pos + CoverUp * 0.015f, CoverUp, pPipeUser->GetParameters().m_fPassRadius, 0.025f, Col_Red, 3.5f);
#endif
			}
			else if (pPipeUser->m_CurrentHideObject.IsValid())
			{
				Vec3 pos = pPipeUser->m_CurrentHideObject.GetLastHidePos();

				CreateHideTarget(pPipeUser, pos);
			}
			else
			{
				Reset(pPipeUser);

				return eGOR_FAILED;
			}
		}

		pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;
	}

	if (!m_refHideTarget.IsNil())
	{
		if (!m_pPathfinder)
		{
			if (gAIEnv.CVars.DebugPathFinding)
			{
				const Vec3& vPos = m_refHideTarget->GetPos();
				AILogAlways("COPCrysis2Hide::Execute %s pathfinding to (%5.2f, %5.2f, %5.2f)", pPipeUser->GetName(),
				            vPos.x, vPos.y, vPos.z);
			}

			CAIObject* targetObject = 0;
			if (gAIEnv.CVars.CoverSystem && gAIEnv.CVars.CoverExactPositioning)
				targetObject = pPipeUser->GetOrCreateSpecialAIObject(CPipeUser::AISPECIAL_ANIM_TARGET);
			else
				targetObject = m_refHideTarget.GetAIObject();

			m_pPathfinder = new COPPathFind("", targetObject);
		}

		if (!m_pTracer && (m_pPathfinder->Execute(pPipeUser) != eGOR_IN_PROGRESS))
		{
			if (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND)
			{
				if (gAIEnv.CVars.DebugPathFinding)
				{
					const Vec3& vPos = m_refHideTarget->GetPos();
					AILogAlways("COPCrysis2Hide::Execute %s Creating trace to hide target (%5.2f, %5.2f, %5.2f)", pPipeUser->GetName(),
					            vPos.x, vPos.y, vPos.z);
				}

				ICoverUser::StateFlags coverState = ICoverUser::EStateFlags::MovingToCover;
				if (CoverID nextCoverID = pPipeUser->GetCoverRegister())
				{
					if (CoverID currCoverID = pPipeUser->GetCoverID())
					{
						CCoverSystem& coverSystem = *gAIEnv.pCoverSystem;
						if (coverSystem.GetSurfaceID(currCoverID) == coverSystem.GetSurfaceID(nextCoverID))
						{
							coverState.Add(ICoverUser::EStateFlags::InCover);
						}
						else
						{
							// Check if surfaces are neighbors (edges are close to each other)
							const CoverSurface& currSurface = coverSystem.GetCoverSurface(currCoverID);
							const CoverSurface& nextSurface = coverSystem.GetCoverSurface(nextCoverID);

							const float neighborDistSq = 0.3f * 0.3f;

							ICoverSystem::SurfaceInfo currSurfaceInfo;
							ICoverSystem::SurfaceInfo nextSurfaceInfo;

							if (currSurface.GetSurfaceInfo(&currSurfaceInfo) && nextSurface.GetSurfaceInfo(&nextSurfaceInfo))
							{
								const Vec3& currLeft = currSurfaceInfo.samples[0].position;
								const Vec3& currRight = currSurfaceInfo.samples[currSurfaceInfo.sampleCount - 1].position;
								const Vec3& nextLeft = nextSurfaceInfo.samples[0].position;
								const Vec3& nextRight = nextSurfaceInfo.samples[nextSurfaceInfo.sampleCount - 1].position;

								if (Distance::Point_Point2DSq(currLeft, nextRight) < neighborDistSq ||
								    Distance::Point_Point2DSq(currRight, nextLeft) < neighborDistSq)
								{
									coverState.Add(ICoverUser::EStateFlags::InCover);
								}
							}
						}
					}

					pPipeUser->SetCoverRegister(CoverID());
					pPipeUser->SetCoverID(nextCoverID);
				}
				pPipeUser->SetCoverState(coverState);

				m_pTracer = new COPTrace(m_exact, 0.0f);
			}
			else
			{
				CCCPOINT(COPCrysis2Hide_Execute_Unreachable);

				// Could not reach the point, mark it ignored so that we do not try to pick it again.
				if (CoverID coverID = pPipeUser->GetCoverRegister())
				{
					pPipeUser->SetCoverState(ICoverUser::EStateFlags::None);

					pPipeUser->SetCoverRegister(CoverID());
					pPipeUser->SetCoverBlacklisted(coverID, true, 10.0f);
				}
				else
					pPipeUser->IgnoreCurrentHideObject(10.0f);

				Reset(pPipeUser);

				return eGOR_FAILED;
			}
		}

		if (m_pTracer)
		{
			EGoalOpResult result = m_pTracer->Execute(pPipeUser);

			if (result == eGOR_SUCCEEDED)
			{
				CCCPOINT(COPCrysis2Hide_Execute_A);

				Reset(pPipeUser);

				if (pPipeUser->GetCoverID())
				{
					pPipeUser->SetInCover(true);
					pPipeUser->SetSignal(1, "OnCoverReached", 0, 0, gAIEnv.SignalCRCs.m_nOnCoverReached);
				}

				return eGOR_SUCCEEDED;
			}
			else if (result == eGOR_IN_PROGRESS)
			{
				CCCPOINT(COPCrysis2Hide_Execute_B);

				pPipeUser->SetMovingToCover(true);
				UpdateMovingToCoverAnimation(pPipeUser);
			}
			else
			{
				Reset(pPipeUser);

				return eGOR_FAILED;
			}
		}
	}

	return eGOR_IN_PROGRESS;
}

void COPCrysis2Hide::ExecuteDry(CPipeUser* pPipeUser)
{
	CCCPOINT(COPCrysis2Hide_ExecuteDry);

	if (m_pTracer)
		m_pTracer->ExecuteTrace(pPipeUser, false);
}

void COPCrysis2Hide::Reset(CPipeUser* pPipeUser)
{
	CCCPOINT(COPCrysis2Hide_Reset);

	SAFE_DELETE(m_pPathfinder);
	SAFE_DELETE(m_pTracer);

	if (pPipeUser)
	{
		pPipeUser->SetCoverState(ICoverUser::EStateFlags::None);

		pPipeUser->ClearPath("COPCrysis2Hide::Reset");

		pPipeUser->ClearMovementContext(1);
		pPipeUser->ResetDesiredBodyDirectionAtTarget();
	}

	m_refHideTarget.Release();
}

void COPCrysis2Hide::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCrysis2Hide");
	{
		m_refHideTarget.Serialize(ser, "m_refHideTarget");

		ser.EnumValue("m_location", m_location, AI_REG_NONE, AI_REG_LAST);
		ser.Value("m_exact", m_exact);

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("Tracer", m_pTracer != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pTracer->Serialize(ser);
				ser.EndGroup();
			}
			if (ser.BeginOptionalGroup("Pathfinder", m_pPathfinder != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pPathfinder->Serialize(ser);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTracer);

			if (ser.BeginOptionalGroup("Tracer", true))
			{
				m_pTracer = new COPTrace(m_exact);
				m_pTracer->Serialize(ser);
				ser.EndGroup();
			}

			SAFE_DELETE(m_pPathfinder);

			if (ser.BeginOptionalGroup("Pathfinder", true))
			{
				m_pPathfinder = new COPPathFind("");
				m_pPathfinder->Serialize(ser);
				ser.EndGroup();
			}
		}

		ser.EndGroup();
	}
}

void COPCrysis2Hide::CreateHideTarget(CPipeUser* pPipeUser, const Vec3& pos, const Vec3& dir)
{
	CCCPOINT(COPCrysis2Hide_CreateHideTarget);

	assert(m_refHideTarget.IsNil() || !m_refHideTarget.GetAIObject());

	if (m_refHideTarget.IsNil() || !m_refHideTarget.GetAIObject())
		gAIEnv.pAIObjectManager->CreateDummyObject(m_refHideTarget);

	IAIObject* hideTarget = m_refHideTarget.GetAIObject();
	assert(hideTarget);

	if (const NavigationAgentTypeID agentTypeID = pPipeUser->GetNavigationTypeID())
	{
		if (const NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(agentTypeID, pos))
		{
			Vec3 fixedLoc;

			const float radius = pPipeUser->GetPathAgentPassRadius(); // TODO: fix - this should use agent type settings from new navigation system
			if (gAIEnv.pNavigationSystem->GetClosestMeshLocation(meshID, pos, 2.0f, radius + 0.25f, nullptr, &fixedLoc, nullptr))
			{
				hideTarget->SetPos(fixedLoc);
				hideTarget->SetEntityDir(dir);

				return;
			}
		}
	}

	hideTarget->SetPos(pos);
	hideTarget->SetEntityDir(dir);
}

void COPCrysis2Hide::DebugDraw(CPipeUser* pPipeUser) const
{
	if (!m_refHideTarget.IsNil())
	{
		CDebugDrawContext dc;

		dc->DrawCylinder(m_refHideTarget.GetAIObject()->GetPos() + Vec3Constants<float>::fVec3_OneZ * 0.15f * 0.5f,
		                 Vec3Constants<float>::fVec3_OneZ, pPipeUser->GetParameters().m_fPassRadius, 0.15f, Col_DarkGreen);
	}
}

void COPCrysis2Hide::UpdateMovingToCoverAnimation(CPipeUser* pPipeUser) const
{
	// Give hints to animation system for sliding into cover
	if (pPipeUser->IsMovingToCover() && !pPipeUser->IsInCover()) // GoalOpHide is used both when moving into cover and *in* cover..
	{
		Vec3 hidePos, hideNormal;
		float hideHeight;
		if (CoverID coverID = pPipeUser->GetCoverID())
		{
			float radius = pPipeUser->GetParameters().distanceToCover;
			hidePos = gAIEnv.pCoverSystem->GetCoverLocation(coverID, radius, &hideHeight, &hideNormal);
			//hidePos = gAIEnv.pCoverSystem->GetCoverSurface(coverID)->GetCoverOcclusionAt(radius, &hideHeight, &hideNormal);
			hideHeight = pPipeUser->GetCoverUser()->GetLocationEffectiveHeight();
		}
		else
		{
			hidePos = pPipeUser->m_CurrentHideObject.GetLastHidePos();
			hideNormal = -pPipeUser->m_CurrentHideObject.GetObjectDir();
			hideHeight = pPipeUser->m_CurrentHideObject.HasLowCover() ? 0 : HighCoverMaxHeight; // old system: assume low cover when low cover is available, otherwise assume high cover
		}

		if (hideHeight < LowCoverMaxHeight) // only slides into low cover are supported
		{
			pPipeUser->SetDesiredBodyDirectionAtTarget(-hideNormal);

			SOBJECTSTATE& state = pPipeUser->GetState();
			state.coverRequest.SetCoverLocation(hidePos, -hideNormal);

			// when coming from the left, turn body to left (and vice versa)
			COPCrysis2Utils::TurnBodyToTargetInCover(pPipeUser, hidePos, hideNormal, pPipeUser->GetPhysicsPos());

			pPipeUser->SetMovementContext(1); // 1 ~ "moving into low cover"
		}
	}
}

COPCrysis2Communicate::COPCrysis2Communicate(
  CommID commID, CommChannelID channelID, float expirity, EAIRegister target,
  SCommunicationRequest::EOrdering ordering)
	: m_commID(commID)
	, m_channelID(channelID)
	, m_ordering(ordering)
	, m_expirity(expirity)
	, m_target(target)
{
}

COPCrysis2Communicate::COPCrysis2Communicate(const XmlNodeRef& node) :
	m_expirity(0.f),
	m_target(AI_REG_NONE)
{
	const char* szName = s_xml.GetMandatoryString(node, "name");
	CCommunicationManager* pCommunicationManager = gAIEnv.pCommunicationManager;
	m_commID = pCommunicationManager->GetCommunicationID(szName);
	m_channelID = pCommunicationManager->GetChannelID(szName);

	m_ordering = s_xml.GetBool(node, "ordered") ? SCommunicationRequest::Ordered : SCommunicationRequest::Unordered;
	s_xml.GetMandatory(node, "expiry", m_expirity);
	s_xml.GetRegister(node, "register", m_target);
}

COPCrysis2Communicate::~COPCrysis2Communicate()
{
}

EGoalOpResult COPCrysis2Communicate::Execute(CPipeUser* pPipeUser)
{
	SCommunicationRequest request;
	request.actorID = pPipeUser->GetEntityID();
	request.commID = m_commID;
	request.channelID = m_channelID;
	request.configID = gAIEnv.pCommunicationManager->GetConfigID(pPipeUser->GetProxy()->GetCommunicationConfigName());
	request.contextExpiry = m_expirity;
	request.ordering = m_ordering;

	IAIObject* target = 0;

	switch (m_target)
	{
	case AI_REG_LASTOP:
		target = pPipeUser->GetLastOpResult();
		break;
	case AI_REG_ATTENTIONTARGET:
		target = pPipeUser->GetAttentionTarget();
		break;
	case AI_REG_REFPOINT:
		target = pPipeUser->GetRefPoint();
		break;
	case AI_REG_COVER:
		if (CoverID coverID = pPipeUser->GetCoverRegister())
			request.target = gAIEnv.pCoverSystem->GetCoverLocation(coverID);
		else
			request.target = pPipeUser->m_CurrentHideObject.GetObjectPos();
		break;
	default:
		break;
	}

	if (target)
	{
		request.targetID = target->GetEntityID();
		if (!request.targetID)
			request.target = target->GetPos();
	}

	gAIEnv.pCommunicationManager->PlayCommunication(request);

	return eGOR_DONE;
}

//////////////////////////////////////////////////////////////////////////
COPCrysis2StickPath::COPCrysis2StickPath(bool bFinishInRange, float fMaxStickDist, float fMinStickDist, bool bCanReverse)
	: m_State(eState_FIRST)
	, m_fPathLength(0.0f)
	, m_fMinStickDistSq(sqr(fMinStickDist))
	, m_fMaxStickDistSq(sqr(fMaxStickDist))
	, m_bFinishInRange(bFinishInRange)
	, m_bCanReverse(bCanReverse)
{

}

//////////////////////////////////////////////////////////////////////////
COPCrysis2StickPath::COPCrysis2StickPath(const XmlNodeRef& node)
	: m_State(eState_FIRST)
	, m_fPathLength(0.0f)
	, m_fMinStickDistSq(0.0f)
	, m_fMaxStickDistSq(0.0f)
	, m_bFinishInRange(false)
	, m_bCanReverse(s_xml.GetBool(node, "canReverse", true))
{
	float fMaxStickDist = 0.0f;
	s_xml.GetMandatory(node, "maxStickDist", fMaxStickDist);
	m_fMaxStickDistSq = sqr(fMaxStickDist);

	float fMinStickDist = s_xml.Get(node, "minStickDist", 0.0f);
	m_fMinStickDistSq = sqr(fMinStickDist);

	m_bFinishInRange = s_xml.GetMandatoryBool(node, "finishInRange");
}

//////////////////////////////////////////////////////////////////////////
COPCrysis2StickPath::COPCrysis2StickPath(const COPCrysis2StickPath& rhs)
	: m_State(rhs.m_State)
	, m_fPathLength(rhs.m_fPathLength)
	, m_fMinStickDistSq(rhs.m_fMinStickDistSq)
	, m_fMaxStickDistSq(rhs.m_fMaxStickDistSq)
	, m_bFinishInRange(rhs.m_bFinishInRange)
	, m_bCanReverse(rhs.m_bCanReverse)
{

}

//////////////////////////////////////////////////////////////////////////
COPCrysis2StickPath::~COPCrysis2StickPath()
{
	m_refNavTarget.Release();
}

//////////////////////////////////////////////////////////////////////////
void COPCrysis2StickPath::Reset(CPipeUser* pPipeUser)
{
	// Note: pPipeUser can be NULL!

	if (pPipeUser)
	{
		SAIEVENT aiEvent;
		aiEvent.vForcedNavigation.zero();
		pPipeUser->Event(AIEVENT_FORCEDNAVIGATION, &aiEvent);
	}

	m_State = eState_FIRST;

	m_refTarget.Reset();
	m_refNavTarget.Release();

	m_fPathLength = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void COPCrysis2StickPath::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCrysis2StickPath");
	{
		ser.Value("m_fMinStickDistSq", m_fMinStickDistSq);
		ser.Value("m_fMaxStickDistSq", m_fMaxStickDistSq);
		ser.Value("m_bFinishInRange", m_bFinishInRange);
		ser.Value("m_bCanReverse", m_bCanReverse);

		m_refNavTarget.Serialize(ser, "m_refNavTarget");
	}
	ser.EndGroup();

	if (ser.IsReading())
	{
		Reset(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void COPCrysis2StickPath::ProjectTargetOntoPath(Vec3& outPos, float& outNearestDistance, float& outDistanceAlongPath) const
{
	assert(m_refTarget.IsValid());

	const Vec3 vTargetPos = m_refTarget.GetAIObject()->GetPos();
	m_Path.NearestPointOnPath(vTargetPos, false, outNearestDistance, outPos, &outDistanceAlongPath);
}

//////////////////////////////////////////////////////////////////////////
COPCrysis2StickPath::ETargetRange COPCrysis2StickPath::GetTargetRange(const Vec3& vMyPos) const
{
	assert(m_refTarget.IsValid());

	ETargetRange result = eTR_TooFar;

	Vec3 vNearestTargetPoint(ZERO);
	float fNearestTargetDist = 0.0f;
	float fTargetDistAlongPath = 0.0f;
	ProjectTargetOntoPath(vNearestTargetPoint, fNearestTargetDist, fTargetDistAlongPath);

	const float fDistSq = vMyPos.GetSquaredDistance(vNearestTargetPoint);
	if (fDistSq < m_fMaxStickDistSq)
	{
		result = (fDistSq > m_fMinStickDistSq ? eTR_InRange : eTR_TooClose);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
COPCrysis2StickPath::ETargetRange COPCrysis2StickPath::GetTargetRange(const Vec3& vMyPos, const Vec3& vTargetPos) const
{
	ETargetRange result = eTR_TooFar;

	const float fDistSq = vMyPos.GetSquaredDistance(vTargetPos);
	if (fDistSq < m_fMaxStickDistSq)
	{
		result = (fDistSq > m_fMinStickDistSq ? eTR_InRange : eTR_TooClose);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
Vec3 COPCrysis2StickPath::GetProjectedPos(CPuppet* pPuppet) const
{
	assert(pPuppet);

	return pPuppet->GetPos() + pPuppet->GetMoveDir() * pPuppet->GetState().fDesiredSpeed;
}

//////////////////////////////////////////////////////////////////////////
EGoalOpResult COPCrysis2StickPath::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	return ExecuteCurrentState(pPipeUser, false);
}

//////////////////////////////////////////////////////////////////////////
void COPCrysis2StickPath::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	ExecuteCurrentState(pPipeUser, true);
}

//////////////////////////////////////////////////////////////////////////
EGoalOpResult COPCrysis2StickPath::ExecuteCurrentState(CPipeUser* pPipeUser, bool bDryUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	EGoalOpResult eGoalOpResult = eGOR_FAILED;

	CPuppet* pPuppet = CastToCPuppetSafe(pPipeUser);
	if (pPuppet)
	{
		switch (m_State)
		{
		case eState_Prepare:
			{
				eGoalOpResult = ExecuteState_Prepare(pPuppet, bDryUpdate) ? eGOR_IN_PROGRESS : eGOR_FAILED;
			}
			break;

		case eState_Navigate:
			{
				eGoalOpResult = ExecuteState_Navigate(pPuppet, bDryUpdate) ? eGOR_IN_PROGRESS : eGOR_FAILED;
			}
			break;

		case eState_Wait:
			{
				if (m_bFinishInRange)
					eGoalOpResult = eGOR_SUCCEEDED;
				else
					eGoalOpResult = ExecuteState_Wait(pPuppet, bDryUpdate) ? eGOR_IN_PROGRESS : eGOR_FAILED;
			}
			break;

		default:
			CRY_ASSERT_MESSAGE(false, "COPCrysis2StickPath::Execute Unhandled state");
			break;
		}
	}

	if (eGoalOpResult == eGOR_FAILED)
		Reset(pPipeUser);

	return eGoalOpResult;
}

//////////////////////////////////////////////////////////////////////////
bool COPCrysis2StickPath::ExecuteState_Prepare(CPuppet* pPuppet, bool bDryUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pPuppet);

	if (!bDryUpdate)
	{
		const char* szPathName = pPuppet->GetPathToFollow();
		if (!gAIEnv.pNavigation->GetDesignerPath(szPathName, m_Path))
		{
			pPuppet->SetSignal(0, "OnNoPathFound", 0, 0, gAIEnv.SignalCRCs.m_nOnNoPathFound);
			return false;
		}

		Vec3 vPathEndPos(pPuppet->GetPos());
		m_fPathLength = 0.0f;
		ListPositions::const_iterator itPathCurrent(m_Path.shape.begin());
		ListPositions::const_iterator itPathEnd(m_Path.shape.end());
		ListPositions::const_iterator itPathNext(itPathCurrent);
		++itPathNext;
		while (itPathNext != itPathEnd)
		{
			Lineseg seg(*itPathCurrent, *itPathNext);
			m_fPathLength += Distance::Point_Point(seg.start, seg.end);
			vPathEndPos = seg.end;
			itPathCurrent = itPathNext;
			++itPathNext;
		}

		// Get desired target to stick towards
		IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(pPuppet->GetVehicleStickTarget());
		CAIObject* pTarget = pTargetEntity ? static_cast<CAIObject*>(pTargetEntity->GetAI()) : NULL;
		if (pTarget)
		{
			m_refTarget = gAIEnv.pObjectContainer->GetWeakRef(pTarget);
		}
		else if (m_refNavTarget.IsNil())
		{
			// Obtain a NavTarget
			stack_string name("navTarget_");
			name += GetNameSafe(pPuppet);

			gAIEnv.pAIObjectManager->CreateDummyObject(m_refNavTarget, name, CAIObject::STP_REFPOINT);
			m_refNavTarget.GetAIObject()->SetPos(vPathEndPos);

			m_refTarget = m_refNavTarget.GetWeakRef();

			// Follow path to end means we always finish when in range.
			// (Since we have no target to stick to, there's nothing else for us to do once we get to the end)
			m_bFinishInRange = true;
		}
		else
		{
			return false;
		}

		assert(m_refTarget.IsValid());

		m_fMinStickDistSq = min(m_fMinStickDistSq, m_fMaxStickDistSq);

		// Set to correct state based on current target distance
		if (eTR_InRange == GetTargetRange(pPuppet->GetPos()))
			m_State = eState_Wait;
		else
			m_State = eState_Navigate;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool COPCrysis2StickPath::ExecuteState_Navigate(CPuppet* pPuppet, bool bDryUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pPuppet);
	assert(m_refTarget.IsValid());

	Vec3 vNearestTargetPoint(ZERO);
	float fNearestTargetDist = 0.0f;
	float fTargetDistAlongPath = 0.0f;
	ProjectTargetOntoPath(vNearestTargetPoint, fNearestTargetDist, fTargetDistAlongPath);

	// Check if close enough to target yet
	ETargetRange targetRange = GetTargetRange(pPuppet->GetPos(), vNearestTargetPoint);
	if (eTR_InRange == targetRange || (eTR_TooClose == targetRange && !m_bCanReverse))
	{
		m_State = eState_Wait;
		return true;
	}

	if (!bDryUpdate)
	{
		CAIObject* pTarget = m_refTarget.GetAIObject();
		assert(pTarget);

		const Vec3 vMyPos = pPuppet->GetPos();

		Vec3 vNearestMyPoint(ZERO);
		float fNearestMyDist = 0.0f;
		float fMyDistAlongPath = 0.0f;
		m_Path.NearestPointOnPath(vMyPos, false, fNearestMyDist, vNearestMyPoint, &fMyDistAlongPath);

		// Determine movement direction to get to the target as quickly as possible along the path
		bool bMoveForward = true;
		if (m_bCanReverse)
		{
			if (m_Path.closed)
			{
				// Move forward if the absolute distance between my projection and the target's projection is less than
				// half of the path's total length, keeping in mind the path loop connection
				const float fDistBetween = fTargetDistAlongPath - fMyDistAlongPath;
				bMoveForward = (fDistBetween >= 0.0f ? fDistBetween<m_fPathLength*0.5f : -fDistBetween> m_fPathLength * 0.5f);
			}
			else
			{
				// Move forward as long as the target's projection is further along the path than my own
				bMoveForward = (fTargetDistAlongPath >= fMyDistAlongPath);
			}

			// Go in opposite direction determined if too close
			if (eTR_TooClose == targetRange)
			{
				bMoveForward = !bMoveForward;
			}
		}

		// On non-closed paths, just don't move anymore if we have to reverse and we can't
		bool bCanMove = true;
		if (!m_Path.closed && !m_bCanReverse && fTargetDistAlongPath < fMyDistAlongPath && eTR_TooFar == targetRange)
		{
			bCanMove = false;
		}

		if (bCanMove)
		{
			const float fDesiredSpeed = pPuppet->GetState().fDesiredSpeed;
			const float fSpeed = (bMoveForward ? fDesiredSpeed : -fDesiredSpeed);

			// Get the nearest path point to use from my position towards target
			// Where I will be in one second if nothing goes wrong
			bool bBeyondPathLength = false;
			float fClosestPointDist = fMyDistAlongPath + fSpeed;
			if (fClosestPointDist >= m_fPathLength)
			{
				fClosestPointDist -= m_fPathLength;
				bBeyondPathLength = true;
			}
			else if (fClosestPointDist < 0.0f)
			{
				fClosestPointDist += m_fPathLength;
			}

			const Vec3 vClosestPathPoint = m_Path.GetPointAlongPath(fClosestPointDist);
			const Vec3 vDirection = (vClosestPathPoint - vMyPos).GetNormalizedSafe(pPuppet->GetMoveDir()) * fSpeed;

			pPuppet->SetForcedNavigation(vDirection, fSpeed);

			// Special case: If navigating to end of path, switch to waiting once we project beyond the path length
			if (bBeyondPathLength && m_refNavTarget.IsSet())
			{
				m_State = eState_Wait;
			}
		}
		else
		{
			pPuppet->ClearForcedNavigation();
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool COPCrysis2StickPath::ExecuteState_Wait(CPuppet* pPuppet, bool bDryUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(pPuppet);
	assert(m_refTarget.IsValid());

	if (!bDryUpdate)
	{
		pPuppet->ClearForcedNavigation();

		// Navigate again if target moves too far away
		ETargetRange targetRange = GetTargetRange(pPuppet->GetPos());
		if (eTR_TooFar == targetRange || (eTR_TooClose == targetRange && m_bCanReverse))
		{
			m_State = eState_Navigate;
		}
	}

	return true;
}

COPAcquirePosition::COPAcquirePosition() : m_target(AI_REG_NONE), /*m_Graph(0),*/ m_State(C2AP_INIT), m_SubState(C2APCS_GATHERSPANS)
{

}

COPAcquirePosition::COPAcquirePosition(const XmlNodeRef& node) : m_target(AI_REG_NONE), /*m_Graph(0),*/ m_State(C2AP_INIT), m_SubState(C2APCS_GATHERSPANS)
{
	s_xml.GetRegister(node, "register", m_target);
	s_xml.GetRegister(node, "output", m_output);
}

COPAcquirePosition::~COPAcquirePosition()
{

}

void COPAcquirePosition::Reset(CPipeUser* pPipeUser)
{
	m_State = C2AP_INIT;
	m_SubState = C2APCS_GATHERSPANS;
	m_target = AI_REG_NONE;
	m_output = AI_REG_NONE;
	//m_Graph = 0;
}

bool COPAcquirePosition::GetTarget(CPipeUser* pPipeUser, Vec3& target)
{
	bool ret = false;

	switch (m_target)
	{
	case AI_REG_REFPOINT:
		{
			CAIObject* refPoint = pPipeUser->GetRefPoint();
			if (refPoint)
			{
				target = refPoint->GetPos();
				ret = true;
			}
		}
		break;

	case AI_REG_ATTENTIONTARGET:
		{
			IAIObject* refTarget = pPipeUser->GetAttentionTarget();
			if (refTarget)
			{
				target = refTarget->GetPos();
				ret = true;
			}
		}
	}

	return ret;
}

bool COPAcquirePosition::SetOutput(CPipeUser* pPipeUser, Vec3& target)
{
	bool ret = true;

	switch (m_output)
	{
	case AI_REG_REFPOINT:
		{
			pPipeUser->SetRefPointPos(target);
		}
		break;
	}

	return ret;
}

struct MINUS1
{
	bool operator==(const Vec3i& value)
	{
		return value.z == -1;
	}

	bool operator!=(const Vec3i& value)
	{
		return value.z != -1;
	}
};

bool operator==(const Vec3i& vec, const MINUS1& mone)
{
	return vec.z == -1;
}

bool operator!=(const Vec3i& vec, const MINUS1& mone)
{
	return vec.z != -1;
}

EGoalOpResult COPAcquirePosition::Execute(CPipeUser* pPipeUser)
{
	EGoalOpResult ret = eGOR_FAILED;

	/*if (pPipeUser)
	{
		switch (m_State)
		{
		case C2AP_INIT:
			if (GetTarget(pPipeUser, m_destination))
			{
				m_curPosition = pPipeUser->GetPos();
				ret = eGOR_FAILED;
			}
			break;
		case C2AP_COMPUTING:
			{
				switch (m_SubState)
				{
				case C2APCS_GATHERSPANS:
					{
						m_Coords.clear();
						m_Graph->GetSpansAlongLine(m_curPosition, m_destination, m_Coords);
						m_SubState = C2APCS_CHECKSPANS;
						ret = eGOR_IN_PROGRESS;
					}
					break;
				case C2APCS_CHECKSPANS:
					{
						std::vector<Vec3i>::iterator end = m_Coords.end();
						for (std::vector<Vec3i>::iterator it = m_Coords.begin(); it != end; ++it)
						{
							uint32 x = it->x;
							uint32 y = it->y;
							it->z = -1;

							const CFlightNavRegion2::NavData::Span* span = &m_Graph->GetColumn(x, y);
							if (m_Graph->IsSpanValid(span))
							{
								Vec3 center;
								m_Graph->GetHorCenter(x, y, center);
								Vec3 diff = center - m_destination;
								diff.z = 0.0f;

								float dist = diff.GetLength();
								float diffZ = 0.25f;

								int32 z = 0;
								while (span)
								{
									float minh = span->heightMin + m_Graph->basePos.z;
									float maxh = span->heightMax + m_Graph->basePos.z;

									if (minh > m_destination.z + dist * -diffZ || maxh < m_destination.z + dist * diffZ)
									{
										it->z = z;
										break;
									}

									++z;
									span = span->next;
								}
							}
						}

						m_Coords.erase(std::remove(m_Coords.begin(), m_Coords.end(), MINUS1()), m_Coords.end());

						if (!m_Coords.empty())
						{
							m_SubState = C2APCS_VISTESTS;
							ret = eGOR_IN_PROGRESS;
						}
						else
						{
							return eGOR_FAILED;
						}
					}
					break;
				case C2APCS_VISTESTS:
					{
						uint32 count = 0;
						std::vector<Vec3i>::iterator end = m_Coords.end();
						for (std::vector<Vec3i>::iterator it = m_Coords.begin(); it != end; ++it)
						{
							const CFlightNavRegion2::NavData::Span* span = m_Graph->GetSpan(it->x, it->y, it->z);

							Vec3 center;
							m_Graph->GetHorCenter(it->x, it->y, center);

							Vec3 diff = center - m_destination;
							float dist = diff.GetLength2D();

							float height = m_destination.z + dist * 0.25f;

							center.z = max(m_Graph->basePos.z + span->heightMin, min(height, m_Graph->basePos.z + span->heightMax)); //m_Graph->basePos.z + (span->heightMin + span->heightMax) * 0.5f;
							SetOutput(pPipeUser, center);

							if (count > 0)
							{
								break;
							}

							++count;
						}
						m_State = C2AP_FAILED;
						ret = eGOR_IN_PROGRESS;
					}
					break;

				}
			}
			break;

		case C2AP_FAILED:
			ret = eGOR_FAILED;
			break;
		}
	}*/

	return ret;
}

void COPAcquirePosition::ExecuteDry(CPipeUser* pPipeUser)
{
	Execute(pPipeUser);
}

//COPCrysis2Fly::SolverAllocator COPCrysis2Fly::m_Solvers;

COPCrysis2Fly::COPCrysis2Fly()
	: m_State(C2F_INVALID)
	, m_target(AI_REG_NONE)
	//, m_Graph(0)
	, m_lookAheadDist(10.0f)
	//, m_Solver(0)
	, m_CurSegmentDir(ZERO)
	, m_destination(ZERO)
	, m_nextDestination(ZERO)
	, m_Length(0xffffffff)
	, m_PathSizeFinish(1)
	, m_minDistance(-1.0f)
	, m_maxDistance(-1.0f)
	, m_TargetEntity(0)
	, m_Circular(false)
	, m_desiredSpeed(50.0f)
	, m_currentSpeed(50.0f)
	, m_Timeout(0.0f)
{
}

COPCrysis2Fly::COPCrysis2Fly(const XmlNodeRef& node)
	: m_State(C2F_INVALID)
	, m_target(AI_REG_NONE)
	//, m_Graph(0)
	, m_lookAheadDist(10.0f)
	//, m_Solver(0)
	, m_CurSegmentDir(ZERO)
	, m_destination(ZERO)
	, m_nextDestination(ZERO)
	, m_Length(0xffffffff)
	, m_PathSizeFinish(1)
	, m_minDistance(-1.0f)
	, m_maxDistance(-1.0f)
	, m_TargetEntity(0)
	, m_Circular(false)
	, m_desiredSpeed(50.0f)
	, m_currentSpeed(50.0f)
	, m_Timeout(0.0f)
{
	s_xml.GetRegister(node, "register", m_target);
	node->getAttr("lookahead", m_lookAheadDist);
}

COPCrysis2Fly::COPCrysis2Fly(const COPCrysis2Fly& rhs)
{
	m_State = rhs.m_State;
	m_target = rhs.m_target;
	m_CurSegmentDir = rhs.m_CurSegmentDir;
	m_destination = rhs.m_destination;
	m_nextDestination = rhs.m_nextDestination;
	m_Length = rhs.m_Length;
	m_Timeout = rhs.m_Timeout;
	m_lookAheadDist = rhs.m_lookAheadDist;
	m_PathSizeFinish = rhs.m_PathSizeFinish;
	m_Circular = rhs.m_Circular;
	m_minDistance = rhs.m_minDistance;
	m_maxDistance = rhs.m_maxDistance;
	m_TargetEntity = rhs.m_TargetEntity;
	m_desiredSpeed = rhs.m_desiredSpeed;
	m_currentSpeed = rhs.m_currentSpeed;

	//m_Graph = 0; //rhs.m_Graph;
	//m_Solver = 0;
}

COPCrysis2Fly::~COPCrysis2Fly()
{
}

void COPCrysis2Fly::ExecuteDry(CPipeUser* pPipeUser)
{
	Execute(pPipeUser);
}

void COPCrysis2Fly::Reset(CPipeUser* pPipeUser)
{
	m_State = C2F_INVALID;
	//m_Graph = 0;
	m_Length = 0xffffffff;
	m_desiredSpeed = 50.0f;
	m_currentSpeed = 50.0f;
	m_PathSizeFinish = 1;
	m_Circular = false;

	m_minDistance = -1.0f;
	m_maxDistance = -1.0f;
	m_TargetEntity = 0;

	m_Threat.Reset();

	std::vector<Vec3> temp;
	m_PathOut.swap(temp);
	std::vector<Vec3> temp2;
	m_Reversed.swap(temp2);

	Base::Reset(pPipeUser);
}

void COPCrysis2Fly::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCrysis2Fly");
	{
		ser.Value("m_state", *(alias_cast<int*>(&m_State)));
		ser.Value("m_target", *(alias_cast<int*>(&m_target)));
		ser.Value("m_CurSegmentDir", m_CurSegmentDir);
		ser.Value("m_destination", m_destination);
		ser.Value("m_nextDestination", m_nextDestination);
		ser.Value("m_Length", m_Length);
		ser.Value("m_PathSizeFinish", m_PathSizeFinish);
		ser.Value("m_Timeout", m_Timeout);
		ser.Value("m_lookAheadDist", m_lookAheadDist);
		ser.Value("m_desiredSpeed", m_desiredSpeed);
		ser.Value("m_currentSpeed", m_currentSpeed);

		ser.Value("m_minDistance", m_minDistance);
		ser.Value("m_maxDistance", m_maxDistance);
		ser.Value("m_TargetEntity", m_TargetEntity);

		ser.Value("m_ThreatthreatId", m_Threat.threatId);
		ser.Value("m_ThreatthreatPos", m_Threat.threatPos);
		ser.Value("m_ThreatthreatDir", m_Threat.threatDir);

		ser.Value("m_Circular", m_Circular);

		ser.Value("m_PathOut", m_PathOut);
		ser.Value("m_Reversed", m_Reversed);
	}
	ser.EndGroup();
}

COPCrysis2Fly::TargetResult COPCrysis2Fly::GetTarget(CPipeUser* pPipeUser, Vec3& target) const
{
	TargetResult ret = C2F_NO_TARGET;

	switch (m_target)
	{
	case AI_REG_REFPOINT:
		{
			CAIObject* refPoint = pPipeUser->GetRefPoint();
			if (refPoint)
			{
				target = refPoint->GetPos();
				assert(target.IsValid());
				ret = C2F_TARGET_FOUND;
			}
		}
		break;

	case AI_REG_ATTENTIONTARGET:
		{
			IAIObject* refTarget = pPipeUser->GetAttentionTarget();
			if (refTarget)
			{
				target = refTarget->GetPos();
				assert(target.IsValid());
				ret = C2F_TARGET_FOUND;
			}
		}
		break;

	case AI_REG_PATH:
		{
			if (m_State != C2F_FOLLOWINGPATH)
				ret = C2F_SET_PATH;
		}
		break;
	}

	return ret;
}

void COPCrysis2Fly::ParseParam(const char* param, const GoalParams& value)
{
	if (!stricmp("desiredSpeed", param))
	{
		value.GetValue(m_desiredSpeed);
		m_currentSpeed = m_desiredSpeed;
	}
	else if (!stricmp("concurrent", param))
	{
		const char* str;
		value.GetValue(str);

		CGoalPipe* pPipe = gAIEnv.pPipeManager->IsGoalPipe(str);

		if (value.GetChildCount() > 0)
		{
			int32 id;
			if (value.GetChild(0).GetValue(id))
			{
				pPipe->SetEventId(id);
			}
		}

		if (pPipe)
		{
			SetConcurrentPipe(pPipe);
		}
	}
	else if (!stricmp("continuous", param))
	{
		value.GetValue(m_Circular);
	}
	else if (!stricmp("minDistance", param))
	{
		value.GetValue(m_minDistance);
	}
	else if (!stricmp("maxDistance", param))
	{
		value.GetValue(m_maxDistance);
	}
	else if (!stricmp("targetEntity", param))
	{
		value.GetValue(m_TargetEntity);
	}
	else if (!stricmp("impulse", param))
	{
		if (!m_Threat.threatId)
		{
			if (value.GetChildCount() == 2)
			{
				value.GetValue(m_Threat.threatPos);
				value.GetChild(0).GetValue(m_Threat.threatDir);
				value.GetChild(1).GetValue(m_Threat.threatId);
			}
		}
	}
}

EGoalOpResult COPCrysis2Fly::CalculateTarget(CPipeUser* pPipeUser)
{
	EGoalOpResult result = eGOR_FAILED;

	/*switch (GetTarget(pPipeUser, m_destination))
	{
	case C2F_TARGET_FOUND:
		{
			m_nextDestination = m_destination;
			const Vec3& currentPos = pPipeUser->GetPos();

			if (m_destination.GetSquaredDistance(currentPos) > 0.25f)
			{
				if (m_Graph)
				{
					m_Solver = AllocateSolver();

					if (m_Solver)
					{
						m_Solver->StartPathFind(m_Graph, m_Graph, m_Graph, currentPos, m_destination);
						m_State = C2F_PATHFINDING;
					}
					else
					{
						return eGOR_FAILED;
					}
				}
			}
		}
		result = eGOR_IN_PROGRESS;
		break;
	case C2F_SET_PATH:
		{
			const char* pathToFollow = pPipeUser->GetPathToFollow();

			SShape path;
			if (gAIEnv.pNavigation->GetDesignerPath(pathToFollow, path))
			{
				if (!path.shape.empty())
				{
					uint32 index = 0;

					uint32 startIndex;
					float dist;
					Vec3 nearestPt;
					path.NearestPointOnPath(pPipeUser->GetPos(), m_Circular, dist, nearestPt, 0, 0, &startIndex);

					m_PathOut.clear();

					size_t pathSize = path.shape.size();

					if (m_Circular && !path.closed)
					{
						m_PathOut.push_back(path.shape.front());
						++pathSize;
						++index;
					}

					if (path.closed || m_Circular)
					{
						m_PathSizeFinish = 2;
						m_Circular = true;

						if (startIndex == pathSize - 2)
						{
							startIndex = 0;
						}
					}

					m_PathOut.resize(pathSize + 1 - startIndex);
					std::vector<Vec3>::const_reverse_iterator end = path.shape.rend() - startIndex;
					for (std::vector<Vec3>::const_reverse_iterator it = path.shape.rbegin(); end != it; ++it)
					{
						m_PathOut[index] = *it;
						++index;
					}

					m_PathOut[index] = pPipeUser->GetPos();

					m_Reversed.clear();
					m_Reversed.reserve(m_PathOut.size());

					m_destination = (m_Circular) ? m_PathOut[1] : m_PathOut[0];

					m_State = C2F_FOLLOWINGPATH;
					result = eGOR_IN_PROGRESS;
				}
			}
		}
		break;
	default:
		break;
	}*/

	return result;
}

bool COPCrysis2Fly::HandleThreat(CPipeUser* pPipeUser, const Vec3& moveDir)
{
	bool ret = false;
	/*
	if (pPipeUser->IsPerceptionEnabled())
	{
		if (m_Threat.threatId)
		{
			if (IEntity* entity = gEnv->pEntitySystem->GetEntity(m_Threat.threatId))
			{
				Vec3 dir = pPipeUser->GetPos() - entity->GetPos();
				dir.Normalize();

				if (m_Threat.threatDir.dot(dir) > 0.5f)
				{
					if (!moveDir.IsEquivalent(ZERO))
					{
						Vec3 pos = pPipeUser->GetPos();
						Lineseg moveSeg(pos + moveDir * 50.0f, pos - moveDir * 50.0f);
						Lineseg threSeg(m_Threat.threatPos, m_Threat.threatPos + m_Threat.threatDir * 5.0f);

						float moveFrac, threadFrac;
						if (Intersect::Lineseg_Lineseg2D(moveSeg, threSeg, moveFrac, threadFrac))
						{
							if (moveFrac < 0.5f)
							{
								if (m_currentSpeed < 5.0f)
								{
									m_PathOut.swap(m_Reversed);
									m_PathOut.push_back(pPipeUser->GetPos());
									m_currentSpeed = m_desiredSpeed;
								}
								else
								{
									m_currentSpeed = 0.0f;
								}
							}
							else
							{
								m_currentSpeed = m_desiredSpeed;
							}

							ret = true;
						}
					}
				}
				else
				{
					m_Threat.Reset();
				}
			}
			else
			{
				m_Threat.Reset();
			}
		}
	}
	*/
	return ret;
}

void COPCrysis2Fly::SendEvent(CPipeUser* pPipeUser, const char* eventName)
{
	if (pPipeUser && eventName)
	{
		SEntityEvent event(ENTITY_EVENT_SCRIPT_EVENT);
		event.nParam[0] = (INT_PTR)eventName;
		event.nParam[1] = IEntityClass::EVT_BOOL;
		bool bValue = true;
		event.nParam[2] = (INT_PTR)&bValue;
		pPipeUser->GetEntity()->SendEvent(event);
	}
}

float COPCrysis2Fly::CheckTargetEntity(const CPipeUser* pPipeUser, const Vec3& lookAheadPos)
{
	float ret = m_desiredSpeed;

	if (m_TargetEntity && (m_minDistance > 0.0f) && (m_maxDistance > 0.0f))
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_TargetEntity);

		if (pEntity)
		{
			float frameTime = gEnv->pTimer->GetFrameTime();

			Vec3 targetEntityPos = pEntity->GetPos();

			const char* pathToFollow = pPipeUser->GetPathToFollow();

			SShape path;
			if (gAIEnv.pNavigation->GetDesignerPath(pathToFollow, path))
			{
				float dist;
				path.NearestPointOnPath(pEntity->GetPos(), false, dist, targetEntityPos);
			}

			Vec3 currentPos = pPipeUser->GetPos();

			Vec3 currentDir = lookAheadPos - currentPos;
			currentDir.Normalize();

			Vec3 diff = targetEntityPos - currentPos;
			float dist = diff.GetLength();

			diff *= 1.0f / dist;

			pe_status_dynamics vel;
			float targetSpeed = 0.f;
			if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
			{
				pPhysics->GetStatus(&vel);
				targetSpeed = vel.v.GetLength();
			}

			Vec3 targetDir = pEntity->GetForwardDir();

			bool behind = targetDir.dot(diff) > 0.0f && currentDir.dot(targetDir) > 0.0f;

			if (dist < m_minDistance && !behind)
			{
				//Speed up or so
				if (currentDir.dot(diff) > 0.0f)
				{
					if (m_currentSpeed > 1.0f)
					{
						m_currentSpeed = m_currentSpeed - m_currentSpeed * frameTime;
						ret = m_currentSpeed;
					}
					else
					{
						m_PathOut.swap(m_Reversed);
						m_PathOut.push_back(currentPos);
					}
				}
				else
				{
					m_currentSpeed = max(min(targetSpeed, m_desiredSpeed), m_desiredSpeed * 0.25f);
					ret = m_currentSpeed;
				}
			}
			else if (dist > m_maxDistance && !behind)
			{
				//slow down or approach
				//Speed up or so
				if (currentDir.dot(diff) < 0.0f)
				{
					if (m_currentSpeed > 1.0f)
					{
						m_currentSpeed = m_currentSpeed - m_currentSpeed * frameTime;
						ret = m_currentSpeed;
					}
					else
					{
						m_PathOut.swap(m_Reversed);
						m_PathOut.push_back(currentPos);
					}
				}
				else
				{
					m_currentSpeed = max(min(targetSpeed, m_desiredSpeed), m_desiredSpeed * 0.25f);
					ret = m_currentSpeed;
				}
			}
			else if (!behind)
			{
				m_currentSpeed = m_currentSpeed + (targetSpeed - m_currentSpeed) * frameTime;
				ret = m_currentSpeed;
			}
			else
			{
				m_currentSpeed = m_desiredSpeed;
				ret = m_currentSpeed;
			}
		}
	}

	return ret;
}

EGoalOpResult COPCrysis2Fly::Execute(CPipeUser* pPipeUser)
{
	EGoalOpResult result = eGOR_FAILED;

	/*if (Base::Execute(pPipeUser) == eGOR_DONE)
	{
		Base::Reset(pPipeUser);
	}

	switch (m_State)
	{
	case C2F_INVALID:
		{
			CRY_PROFILE_REGION(PROFILE_AI, "COPCrysis2Fly: SETUP ASTAR" );

			result = CalculateTarget(pPipeUser);

			//break here if failed or designer placed path set
			if (eGOR_FAILED == result)
			{
				SendEvent(pPipeUser, "GoToFailed");
				break;
			}
			else if (C2F_FOLLOWINGPATH == m_State)
			{
				break;
			}
		}
	case C2F_PATHFINDING:
		{
			CRY_PROFILE_REGION(PROFILE_AI, "COPCrysis2Fly: ASTAR UPDATE";

			if (m_Solver)
			{
				if (m_Solver->Update(10))
				{
					std::vector<Vec3i> pathRaw;
					pathRaw.clear();

					m_Solver->GetPathReversed(pathRaw);
					DestroySolver(m_Solver);
					m_Solver = 0;

					m_PathOut.clear();
					if (m_Graph)
					{
						m_Graph->BeautifyPath(pPipeUser->GetPos(), m_destination, pathRaw, m_PathOut);

						if (m_PathOut.size() < 2)
						{
							SendEvent(pPipeUser, "GoToSucceeded");
							return eGOR_DONE;
						}

						m_Reversed.clear();
						m_Reversed.reserve(m_PathOut.size());

						Vec3& curPos = m_PathOut.back();
						curPos = pPipeUser->GetPos();

						Vec3& nextPos = *(&curPos - 1);
						Vec3 moveDir = nextPos - curPos;
						moveDir.Normalize();

						m_CurSegmentDir = moveDir;

						pPipeUser->m_State.predictedCharacterStates.nStates = 0;
						pPipeUser->m_State.vMoveDir = moveDir;
						pPipeUser->m_State.vLookTargetPos = curPos + 100.0f * moveDir;
						pPipeUser->m_State.fDesiredSpeed = 0.0f;

						m_State = C2F_FOLLOWINGPATH;
					}
				}

				result = eGOR_IN_PROGRESS;

			}
			else
			{
				result = eGOR_IN_PROGRESS;
			}
		}
		break;

	case C2F_FOLLOW_AND_SWITCH:
	case C2F_FOLLOW_AND_CALC_NEXT:
		switch (m_State)
		{
		case C2F_FOLLOW_AND_CALC_NEXT:
			if (m_Solver)
			{
				if (m_Solver->Update(2))
				{
					m_State = C2F_FOLLOW_AND_SWITCH;
					m_Length = 0xffffffff;
				}
			}
			break;
		case C2F_FOLLOW_AND_SWITCH:
			{
				if (m_Solver)
				{
					std::vector<Vec3i> pathRaw;
					pathRaw.clear();

					m_Solver->GetPathReversed(pathRaw);
					DestroySolver(m_Solver);
					m_Solver = 0;

					m_PathOut.clear();
					if (m_Graph)
					{
						m_Graph->BeautifyPath(m_destination, m_nextDestination, pathRaw, m_PathOut);
						m_PathOut.push_back(pPipeUser->GetPos());

						m_Length = m_PathOut.size();
						if (m_PathOut.size() < 2)
						{
							SendEvent(pPipeUser, "GoToSucceeded");
							return eGOR_DONE;
						}

					}
				}

				if (m_PathOut.size() < m_Length)
				{
					SendEvent(pPipeUser, "GoToSucceeded");

					m_destination = m_nextDestination;
					m_State = C2F_FOLLOWINGPATH;
					m_Length = 0xffffffff;
				}
			}
			break;
		}

	case C2F_FOLLOWINGPATH:
		{
			if ((m_State != C2F_FOLLOW_AND_CALC_NEXT) && (m_State != C2F_FOLLOW_AND_SWITCH) && (GetTarget(pPipeUser, m_nextDestination) != C2F_NO_TARGET))
			{
				if (!m_destination.IsEquivalent(m_nextDestination))
				{
					m_Solver = AllocateSolver();
					m_Solver->StartPathFind(m_Graph, m_Graph, m_Graph, m_destination, m_nextDestination);
					m_State = C2F_FOLLOW_AND_CALC_NEXT;
				}
			}

			if (m_PathSizeFinish == m_PathOut.size() && m_Circular)
			{
				Vec3 target;
				m_State = C2F_INVALID;
				CalculateTarget(pPipeUser);
			}

			if (m_PathOut.size() > m_PathSizeFinish)
			{
				Vec3 curPos = pPipeUser->GetPos();

				ColorB colour(0, 255, 0);
				Vec3 nextPos;
				float dist = GetNextPathPoint(m_PathOut, curPos, nextPos, &m_Reversed);

				if (FLT_EPSILON > dist)
				{
					SendEvent(pPipeUser, "GoToFailed");

					pPipeUser->m_State.predictedCharacterStates.nStates = 0;
					pPipeUser->m_State.vMoveDir = ZERO;
					pPipeUser->m_State.fDesiredSpeed = 0.0f;

					m_State = C2F_FAILED;
					result = eGOR_FAILED;
				}

				if (gAIEnv.CVars.DebugPathFinding)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(nextPos + Vec3(0.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1.0f, 2.0f, colour);
				}

				colour.r = 255;
				colour.g = 0;
				Vec3 lookAheadPos;

				float lookDist = m_lookAheadDist;
				GetLookAheadPoint(m_PathOut, lookDist, lookAheadPos);

				if (gAIEnv.CVars.DebugPathFinding)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(lookAheadPos + Vec3(0.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1.0f, 2.0f, colour);
				}

				UpdatePathPos(m_PathOut, curPos);

				float accel = 1.0f;

				if ((m_State != C2F_FOLLOW_AND_SWITCH) && (m_PathOut.size() == 2))
				{
					if (dist < 20.0f)
					{
						accel = dist / 30.0f;
					}
				}

				if (m_PathOut.size() == m_PathSizeFinish && dist < 50.0f * accel)
				{
					SendEvent(pPipeUser, "GoToCloseToDest");
				}

				Vec3 moveDir = lookAheadPos - curPos;

				if (moveDir.GetLengthSquared() < 1.0f)
				{
					lookAheadPos = pPipeUser->GetEntity()->GetForwardDir() * 10.0f + curPos;
				}

				moveDir.NormalizeSafe(pPipeUser->GetEntity()->GetForwardDir());

				pPipeUser->m_State.predictedCharacterStates.nStates = 0;
				pPipeUser->m_State.vMoveDir = moveDir;
				pPipeUser->m_State.fDistanceToPathEnd = dist;
				pPipeUser->m_State.vLookTargetPos = lookAheadPos;

				float speed = m_desiredSpeed;

				if (!HandleThreat(pPipeUser, moveDir))
				{
					speed = CheckTargetEntity(pPipeUser, lookAheadPos);
				}
				else
				{
					speed = m_currentSpeed;
				}

				GetLookAheadPoint(m_PathOut, lookDist * 2.0f, lookAheadPos);

				if (gAIEnv.CVars.DebugPathFinding)
				{
					colour.r = 0;
					colour.b = 255;
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(lookAheadPos + Vec3(0.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1.0f, 2.0f, colour);
				}

				pPipeUser->SetBodyTargetDir(lookAheadPos - curPos);
				pPipeUser->m_State.fDesiredSpeed = speed * accel;

				result = eGOR_IN_PROGRESS;
			}
			else
			{
				if (m_TargetEntity && (m_PathOut.size() > m_PathSizeFinish || m_Reversed.size() > m_PathSizeFinish))
				{
					if (!(m_PathOut.size() > m_PathSizeFinish))
					{
						m_PathOut.swap(m_Reversed);
						m_PathOut.push_back(pPipeUser->GetPos());
					}
					result = eGOR_IN_PROGRESS;
				}
				else
				{
					SendEvent(pPipeUser, "GoToSucceeded");

					pPipeUser->m_State.predictedCharacterStates.nStates = 0;
					pPipeUser->m_State.vMoveDir = ZERO;
					pPipeUser->m_State.fDesiredSpeed = 0.0f;

					m_State = C2F_FAILED;
					result = eGOR_DONE;
				}
			}
		}
		break;
	case C2F_FAILED:

		pPipeUser->m_State.predictedCharacterStates.nStates = 0;
		pPipeUser->m_State.vMoveDir = ZERO;
		pPipeUser->m_State.fDesiredSpeed = 0.0f;
		result = eGOR_FAILED;

		break;
	default:
		result = eGOR_IN_PROGRESS;

		m_State = C2F_INVALID;
		break;
	}

	if (gAIEnv.CVars.DebugPathFinding)
	{
		CFlightNavRegion2::DrawPath(m_PathOut);

		CFlightNavRegion2::DrawPath(m_Reversed);
	}*/

	return result;
}

static const float drawPath = 0.0f;

float GetNextPathPoint(std::vector<Vec3>& pathOut, const Vec3& curPos, Vec3& pos, std::vector<Vec3>* reversePath = 0)
{
	float ret = 0.0f;

	pos = curPos;

	uint32 size = pathOut.size();
	if (size > 1)
	{
		std::vector<Vec3>::reverse_iterator it = pathOut.rbegin();
		Vec3 lastPos = *it;

		// current target
		++it;
		pos = *it;

		Vec3 lastDir = pos - lastPos;

		Vec3 curDir = pos - curPos;
		float curMagnitude = curDir.GetLength();

		lastDir.Normalize();
		curDir.Normalize();

		if (lastDir.dot(curDir) < 0.98f || curMagnitude < 0.5f)
		{
			pathOut.pop_back();
			it = pathOut.rbegin();

			if (reversePath)
			{
				reversePath->push_back(*it);
			}

			--size;

			if (size > 1)
			{
				*it = curPos;
				++it;
				pos = *it;
			}

			curDir = pos - curPos;
		}

		if (size > 2)
		{
			++it;

			Vec3 nextPos = *it;
			Vec3 nextDir = nextPos - pos;
			nextDir.Normalize();

			curDir = pos - curPos;
			curDir.Normalize();

			if (nextDir.dot(curDir) < -0.3f)
			{
				pathOut.pop_back();
				it = pathOut.rbegin();

				if (reversePath)
				{
					reversePath->push_back(*it);
				}

				--size;

				*it = curPos;
				++it;

				pos = *it;

				curDir = pos - curPos;
			}
		}

		ret = pos.GetDistance(curPos);
	}

	return ret;
}

void UpdatePathPos(std::vector<Vec3>& pathOut, const Vec3& curPos)
{
	uint32 size = pathOut.size();
	if (size > 1)
	{
		std::vector<Vec3>::reverse_iterator it = pathOut.rbegin();
		*it = curPos;
	}
}

float GetLookAheadPoint(std::vector<Vec3>& pathOut, float dist, Vec3& pos)
{
	float tempDist = dist;
	float ret = 0.0f;

	Vec3 lastDir = ZERO;

	uint32 size = pathOut.size();
	if (size > 1)
	{
		std::vector<Vec3>::reverse_iterator it = pathOut.rbegin();
		Vec3 startPos = *it;

		while (++it != pathOut.rend())
		{
			Vec3 endPos = *it;

			Vec3 segment = endPos - startPos;
			segment.Normalize();

			float temp = startPos.GetDistance(endPos);
			float bonus = 1.0f - fabs(lastDir.Dot(segment));
			bonus *= 10.0f;

			temp += bonus;
			lastDir = segment;

			if (temp > tempDist)
			{
				Vec3 dir = (endPos - startPos);
				dir.Normalize();

				pos = startPos + tempDist * dir;
				ret += tempDist;

				tempDist = 0.0f;
				break;
			}

			startPos = endPos;
			tempDist -= temp;
			ret += temp;
		}

		if (0.0f < tempDist)
		{
			pos = startPos;
		}

	}
	else
	{
		pos = pathOut.back();
	}

	return ret;
}

COPCrysis2ChaseTarget::COPCrysis2ChaseTarget()
{
}

COPCrysis2ChaseTarget::COPCrysis2ChaseTarget(const XmlNodeRef& node)
	: m_State(C2F_INVALID)
	, m_target(AI_REG_NONE)
	, m_lookAheadDist(10.0f)
	, m_desiredSpeed(0.0f)
	, m_destination(ZERO)
	, m_nextDestination(ZERO)
	, m_distanceMin(0.0f)
	, m_distanceMax(0.0f)
	, m_bestLocation(ZERO)
	, m_testLocation(ZERO)
{
	s_xml.GetRegister(node, "register", m_target);
	node->getAttr("lookahead", m_lookAheadDist);

	m_visCheckRayID[0] = 0;
	m_visCheckRayID[1] = 0;
	m_visCheckResultCount = 0;

	m_firePauseParams.SetName("params");

	GoalParams temp;
	temp.SetName("firePause");
	m_firePauseParams.AddChild(temp);
}

COPCrysis2ChaseTarget::COPCrysis2ChaseTarget(const COPCrysis2ChaseTarget& rhs)
{
	m_State = rhs.m_State;
	m_target = rhs.m_target;
	m_destination = rhs.m_destination;
	m_nextDestination = rhs.m_nextDestination;
	m_lookAheadDist = rhs.m_lookAheadDist;
	m_desiredSpeed = rhs.m_desiredSpeed;

	m_distanceMin = rhs.m_distanceMin;
	m_distanceMax = rhs.m_distanceMax;

	m_visCheckRayID[0] = 0;
	m_visCheckRayID[1] = 0;
	m_visCheckResultCount = 0;

	m_firePauseParams.SetName("params");

	GoalParams temp;
	temp.SetName("firePause");
	m_firePauseParams.AddChild(temp);
}

COPCrysis2ChaseTarget::~COPCrysis2ChaseTarget()
{
}

void COPCrysis2ChaseTarget::ExecuteDry(CPipeUser* pPipeUser)
{
	Execute(pPipeUser);
}

void COPCrysis2ChaseTarget::Reset(CPipeUser* pPipeUser)
{
	m_State = C2F_INVALID;
	m_desiredSpeed = 50.0f;
	m_distanceMin = 0.0f;
	m_distanceMax = 0.0f;

	if (m_visCheckRayID[0])
		gAIEnv.pRayCaster->Cancel(m_visCheckRayID[0]);
	if (m_visCheckRayID[1])
		gAIEnv.pRayCaster->Cancel(m_visCheckRayID[1]);

	m_visCheckRayID[0] = 0;
	m_visCheckRayID[1] = 0;

	ListPositions().swap(m_PathOut);

	Base::Reset(pPipeUser);
}

COPCrysis2ChaseTarget::TargetResult COPCrysis2ChaseTarget::GetTarget(CPipeUser* pPipeUser, Vec3& target, IAIObject** targetObject) const
{
	TargetResult ret = C2F_NO_TARGET;

	CPuppet* pPuppet = pPipeUser->CastToCPuppet();
	if (pPuppet)
	{
		if (EntityId entityID = pPuppet->GetVehicleStickTarget())
		{
			if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID))
			{
				if (IAIObject* object = entity->GetAI())
				{
					target = object->GetPos(); //entity->GetWorldPos();

					if (targetObject)
						*targetObject = entity->GetAI();

					return C2F_TARGET_FOUND;
				}
			}
		}
	}

	switch (m_target)
	{
	case AI_REG_REFPOINT:
		{
			CAIObject* refPoint = pPipeUser->GetRefPoint();

			if (refPoint)
			{
				target = refPoint->GetPos();
				ret = C2F_TARGET_FOUND;
			}
		}
		break;

	case AI_REG_ATTENTIONTARGET:
		{
			IAIObject* refTarget = pPipeUser->GetAttentionTarget();

			if (refTarget)
			{
				target = refTarget->GetPos();

				if (targetObject)
					*targetObject = refTarget;

				ret = C2F_TARGET_FOUND;
			}
		}
		break;
	}

	return ret;
}

void COPCrysis2ChaseTarget::ParseParam(const char* param, const GoalParams& value)
{
	if (!stricmp("desiredSpeed", param))
	{
		value.GetValue(m_desiredSpeed);
	}
	else if (!stricmp("distanceMin", param))
	{
		value.GetValue(m_distanceMin);
	}
	else if (!stricmp("distanceMax", param))
	{
		value.GetValue(m_distanceMax);
	}
	else if (!stricmp("concurrent", param))
	{
		const char* str;
		value.GetValue(str);

		CGoalPipe* pPipe = gAIEnv.pPipeManager->IsGoalPipe(str);

		if (value.GetChildCount() > 0)
		{
			int32 id;
			if (value.GetChild(0).GetValue(id))
			{
				pPipe->SetEventId(id);
			}
		}

		if (pPipe)
		{
			SetConcurrentPipe(pPipe);
		}
	}
}

size_t COPCrysis2ChaseTarget::GatherCandidateLocations(float currentLocationAlong, const ListPositions& path, float pathLength,
                                                       bool closed, float spacing, const Vec3& target, Candidates& candidates)
{
	size_t size = path.size();
	size_t candidatesStart = candidates.size();
	size_t current = 0;
	size_t next = 1;

	float pathLen = 0.0f;
	float remainingLen = 0.0f;

	const float distanceMinSq = sqr(m_distanceMin);
	const float distanceMaxSq = sqr(m_distanceMax);

	bool hasMax = distanceMaxSq > 0.0f;
	bool hasMin = distanceMaxSq > 0.0f;

	const float distanceOffset = 0.0f; //cry_random(-30.0f, 30.0f);

	while (next < size)
	{
		const Vec3 left = path[current];
		const Vec3 right = path[next];

		const float segLen = Distance::Point_Point(left, right);
		if (segLen <= 0.00001f)
			continue;

		const float segLenInv = 1.0f / segLen;
		float prev = pathLen - remainingLen;
		pathLen += segLen;
		remainingLen += segLen;

		while (remainingLen >= spacing)
		{
			CandidateLocation location;

			float distance = prev + spacing;
			float fraction = 1.0f - (pathLen - distance) * segLenInv;
			Vec3 point = left + (right - left) * fraction;

			const float distanceToTargetSq = Distance::Point_PointSq(target, point);
			;

			if ((!hasMax || (distanceToTargetSq <= distanceMaxSq)) &&
			    (!hasMin || (distanceToTargetSq >= distanceMinSq)))
			{
				float distanceAbs = 0.0f;

				if (closed)
					distanceAbs = min((pathLength - distance) + currentLocationAlong, fabs_tpl(distance - currentLocationAlong));
				else
					distanceAbs = fabs_tpl(distance - currentLocationAlong);

				if (distanceAbs <= 0.001f)
					distanceAbs = 0.001f;

				float candidateDistance = fabs_tpl(distanceAbs - distanceOffset);
				if (candidateDistance < 1.0f)
					candidateDistance = 1.0f;

				float distanceScore = 1.0f / candidateDistance;
				float distanceTargetScore = 1.0f / distanceToTargetSq;

				location.score = distanceScore * 50.0f - distanceTargetScore * 25.0f;
				location.point = point;
				location.fraction = fraction;
				location.index = current;

				candidates.push_back(location);
			}

			remainingLen -= spacing;
			prev += spacing;
		}

		current = next;
		++next;
	}

	return candidates.size() - candidatesStart;
}

EGoalOpResult COPCrysis2ChaseTarget::Chase(CPipeUser* pPipeUser)
{
	IAIObject* targetObject = 0;
	Vec3 target;
	if (GetTarget(pPipeUser, target, &targetObject) != C2F_TARGET_FOUND)
		return eGOR_IN_PROGRESS;

	SShape path;

	if (gAIEnv.pNavigation->GetDesignerPath(pPipeUser->GetPathToFollow(), path))
	{
		if (!path.shape.empty())
		{
			m_candidates.clear();

			Vec3 point;
			uint32 startIndex;
			float distancePath;
			float distanceEuclidean;
			float pathLength;
			float segmentFraction;
			path.NearestPointOnPath(pPipeUser->GetPos(), false, distanceEuclidean, point, &distancePath, 0, &startIndex,
			                        &pathLength, &segmentFraction);

			size_t candidateCount = GatherCandidateLocations(distancePath, path.shape, pathLength, path.closed, 15.0f, target, m_candidates);

			if (candidateCount > 0)
				std::sort(m_candidates.begin(), m_candidates.end());

			bool cloaked = false;

			if (targetObject)
			{
				if (CAIPlayer* player = targetObject->CastToCAIPlayer())
				{
					cloaked = player->IsCloakEffective(pPipeUser->GetPos());
				}
			}

			if (cloaked)
			{
				if (!m_candidates.empty())
				{
					size_t off = cry_random((size_t)0, m_candidates.size() - 1);
					m_bestLocation = m_candidates[off >> 1];

					if (gAIEnv.pNavigation->GetDesignerPath(pPipeUser->GetPathToFollow(), path))
					{
						if (!path.shape.empty())
						{
							path.NearestPointOnPath(pPipeUser->GetPos(), false, distanceEuclidean, point, &distancePath, 0, &startIndex,
							                        0, &segmentFraction);

							m_PathOut.clear();
							CalculateShortestPathTo(path.shape, path.closed, startIndex, segmentFraction,
							                        m_bestLocation.index, m_bestLocation.fraction, m_PathOut);
						}
					}

					m_State = C2F_CHASING;
				}
			}
			else
			{
				m_State = C2F_FINDINGLOCATION;
			}
			return eGOR_IN_PROGRESS;
		}
	}

	return eGOR_IN_PROGRESS;
}

void COPCrysis2ChaseTarget::TestLocation(const CandidateLocation& location, CPipeUser* pipeUser, const Vec3& target)
{
	assert(!m_visCheckRayID[0] && !m_visCheckRayID[1]);

	Vec3 point = location.point - Vec3(0.0f, 0.0f, 2.5f);
	Vec3 dir = (target - point);

	Vec3 right = dir.Cross(Vec3(0.0f, 0.0f, 1.0f)).GetNormalized();

	const float ShipHalfWidth = 15.0f * 0.5f;
	const float TargetHalfWidth = 0.75f * 0.5f;

	Vec3 pointLeft = point + right * -ShipHalfWidth;
	Vec3 targetLeft = target + right * -TargetHalfWidth;

	Vec3 pointRight = point + right * ShipHalfWidth;
	Vec3 targetRight = target + right * TargetHalfWidth;

	PhysSkipList skipList;
	pipeUser->GetPhysicalSkipEntities(skipList);

	m_visCheckRayID[0] = gAIEnv.pRayCaster->Queue(RayCastRequest::HighPriority,
	                                              RayCastRequest(pointLeft, (targetLeft - pointLeft) * 0.985f, COVER_OBJECT_TYPES,
	                                                             AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
	                                                             &skipList.at(0), skipList.size()), functor(*this, &COPCrysis2ChaseTarget::VisCheckRayComplete));

	m_visCheckRayID[1] = gAIEnv.pRayCaster->Queue(RayCastRequest::HighPriority,
	                                              RayCastRequest(pointRight, (targetRight - pointRight) * 0.985f, COVER_OBJECT_TYPES,
	                                                             AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
	                                                             &skipList.at(0), skipList.size()), functor(*this, &COPCrysis2ChaseTarget::VisCheckRayComplete));

	m_testLocation = location;
	m_visCheckResultCount = 0;
}

void COPCrysis2ChaseTarget::VisCheckRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	assert(rayID == m_visCheckRayID[0] || rayID == m_visCheckRayID[1]);

	if (result)
	{
		assert(m_State == C2F_WAITING_LOCATION_RESULT);
		m_State = C2F_FINDINGLOCATION;

		if ((m_visCheckRayID[0] == rayID) && m_visCheckRayID[1])
		{
			gAIEnv.pRayCaster->Cancel(m_visCheckRayID[1]);
			m_visCheckRayID[1] = 0;
		}
		else if ((m_visCheckRayID[1] == rayID) && m_visCheckRayID[0])
		{
			gAIEnv.pRayCaster->Cancel(m_visCheckRayID[0]);
			m_visCheckRayID[0] = 0;
		}
	}
	else
	{
		++m_visCheckResultCount;
		if (m_visCheckResultCount == 2)
		{
			m_bestLocation = m_testLocation;
			m_State = C2F_PREPARING;
		}
	}

	if (rayID == m_visCheckRayID[0])
		m_visCheckRayID[0] = 0;

	if (rayID == m_visCheckRayID[1])
		m_visCheckRayID[1] = 0;
}

ILINE Vec3 GetSegmentPoint(const ListPositions& path, size_t segment, float fraction)
{
	Vec3 start = path[segment];
	Vec3 end = path[segment + 1];

	return start + (end - start) * fraction;
}

ILINE float GetSubpathTo(const ListPositions& path, size_t startIndex, float startSegmentFraction, size_t endIndex,
                         float endSegmentFraction, ListPositions& pathOut)
{
	float distance = 0.0f;
	Vec3 node;
	Vec3 lastNode;

	if (endIndex == startIndex)
	{
		if (fabs_tpl(endSegmentFraction - startSegmentFraction) < 0.0001f)
			return 0.0f;

		lastNode = GetSegmentPoint(path, startIndex, startSegmentFraction);
		pathOut.push_back(lastNode);

		node = GetSegmentPoint(path, endIndex, endSegmentFraction);
		pathOut.push_back(node);

		return Distance::Point_Point(lastNode, node);
	}
	else if (endIndex > startIndex)
	{
		lastNode = GetSegmentPoint(path, startIndex, startSegmentFraction);
		pathOut.push_back(lastNode);

		for (size_t i = startIndex + 1; i <= endIndex; ++i)
		{
			node = path[i];
			pathOut.push_back(node);

			distance += Distance::Point_Point(lastNode, node);
			lastNode = node;
		}

		if (endSegmentFraction > 0.001f)
		{
			node = GetSegmentPoint(path, endIndex, endSegmentFraction);
			pathOut.push_back(node);
			distance += Distance::Point_Point(lastNode, node);
		}
	}
	else
	{
		lastNode = GetSegmentPoint(path, startIndex, startSegmentFraction);
		pathOut.push_back(lastNode);

		size_t startOffset = (startSegmentFraction >= 0.001f ? 0 : 1);
		size_t endOffset = (endSegmentFraction >= 0.001f ? 0 : 1);

		for (size_t i = startIndex - startOffset; i > endIndex; --i)
		{
			node = path[i];
			pathOut.push_back(node);

			distance += Distance::Point_Point(lastNode, node);
			lastNode = node;
		}

		node = GetSegmentPoint(path, endIndex, endSegmentFraction);

		pathOut.push_back(node);
		distance += Distance::Point_Point(lastNode, node);
	}

	return distance;
}

float COPCrysis2ChaseTarget::CalculateShortestPathTo(const ListPositions& path, bool closed, size_t startIndex,
                                                     float startSegmentFraction, size_t endIndex,
                                                     float endSegmentFraction, ListPositions& pathOut)
{
	if (!closed)
		return GetSubpathTo(path, endIndex, endSegmentFraction, startIndex, startSegmentFraction, pathOut);
	else
	{
		float distanceAround = GetSubpathTo(path, endIndex, endSegmentFraction, startIndex, startSegmentFraction, pathOut);
		float distanceThroughOrigin = FLT_MAX;

		m_throughOriginBuf.clear();

		if (endIndex > startIndex)
		{
			float toEnd = GetSubpathTo(path, endIndex, endSegmentFraction, path.size() - 2, 1.0f, m_throughOriginBuf);
			m_throughOriginBuf.pop_back();

			float toOrigin = GetSubpathTo(path, 0, 0.0f, startIndex, startSegmentFraction, m_throughOriginBuf);

			distanceThroughOrigin = toEnd + toOrigin;
		}
		else if (endIndex < startIndex)
		{
			float toOrigin = GetSubpathTo(path, endIndex, endSegmentFraction, 0, 0.0f, m_throughOriginBuf);
			m_throughOriginBuf.pop_back();

			float throughEnd = GetSubpathTo(path, path.size() - 2, 1.0f, startIndex, startSegmentFraction, m_throughOriginBuf);

			distanceThroughOrigin = throughEnd + toOrigin;
		}

		if (distanceAround > distanceThroughOrigin)
		{
			pathOut.swap(m_throughOriginBuf);

			return distanceThroughOrigin;
		}

		return distanceAround;
	}
}

EGoalOpResult COPCrysis2ChaseTarget::Execute(CPipeUser* pPipeUser)
{
	EGoalOpResult result = eGOR_IN_PROGRESS;

	Vec3 target(ZERO);
	IAIObject* targetObject = 0;

	GetTarget(pPipeUser, target, &targetObject);

	bool canShoot = false;
	if (targetObject && pPipeUser->CanSee(targetObject->GetVisionID()))
		canShoot = true;

	m_firePauseParams.GetChild(0).SetValue(!canShoot);
	Base::ParseParams(m_firePauseParams);

	if (Base::Execute(pPipeUser) == eGOR_DONE)
		Base::Reset(pPipeUser);

	switch (m_State)
	{
	case C2F_FINDINGLOCATION:
		{
			if (!m_candidates.empty())
			{
				TestLocation(m_candidates.back(), pPipeUser, target);
				m_candidates.pop_back();
				m_State = C2F_WAITING_LOCATION_RESULT;
			}
			else
			{
				m_State = C2F_INVALID;

				if (m_visCheckRayID[0])
					gAIEnv.pRayCaster->Cancel(m_visCheckRayID[0]);
				if (m_visCheckRayID[1])
					gAIEnv.pRayCaster->Cancel(m_visCheckRayID[1]);

				m_visCheckRayID[0] = 0;
				m_visCheckRayID[1] = 0;

				ListPositions().swap(m_PathOut);

				m_State = C2F_INVALID;
				result = eGOR_IN_PROGRESS;
			}
		}
		break;
	case C2F_WAITING_LOCATION_RESULT:
		break;
	case C2F_PREPARING:
		{
			m_destination = m_bestLocation.point;
			m_nextDestination = m_bestLocation.point;

			if ((m_destination - pPipeUser->GetPos()).GetLengthSquared() <= sqr(0.5f))
			{
				m_State = C2F_INVALID;
				break;
			}

			m_State = C2F_CHASING;
			result = eGOR_IN_PROGRESS;

			m_PathOut.clear();

			SShape path;

			if (gAIEnv.pNavigation->GetDesignerPath(pPipeUser->GetPathToFollow(), path))
			{
				if (!path.shape.empty())
				{
					Vec3 point;
					uint32 startIndex;
					float distancePath;
					float distanceEuclidean;
					float segmentFraction;
					path.NearestPointOnPath(pPipeUser->GetPos(), false, distanceEuclidean, point, &distancePath, 0, &startIndex,
					                        0, &segmentFraction);

					m_PathOut.clear();
					CalculateShortestPathTo(path.shape, path.closed, startIndex, segmentFraction,
					                        m_bestLocation.index, m_bestLocation.fraction, m_PathOut);

					assert(Distance::Point_Point(m_PathOut.front(), m_bestLocation.point) < 0.01f);

#ifdef CRYAISYSTEM_DEBUG
					for (size_t i = 0; i < m_PathOut.size(); ++i)
					{
						if (i > 0)
							GetAISystem()->AddDebugLine(m_PathOut[i - 1], m_PathOut[i], Col_Blue, 10.0f, 10.0f);

						GetAISystem()->AddDebugSphere(m_PathOut[i], 1.0f, 255, 255, 255, 10.0f);
					}
#endif

					break;
				}
			}

			result = eGOR_FAILED;
			m_State = C2F_INVALID;
		}
		break;
	case C2F_CHASING:
		{
			if (m_PathOut.size() > 1)
			{
				Vec3 curPos = pPipeUser->GetPos();

				ColorB colour(0, 255, 0);
				Vec3 nextPos;
				GetNextPathPoint(m_PathOut, curPos, nextPos);

				if (gAIEnv.CVars.DebugPathFinding)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(nextPos + Vec3(0.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1.0f, 2.0f, colour);
				}

				colour.r = 255;
				colour.g = 0;
				Vec3 lookAheadPos;
				Vec3 lookAheadPos2;

				float lookDist = m_lookAheadDist;
				GetLookAheadPoint(m_PathOut, lookDist, lookAheadPos);

				if (gAIEnv.CVars.DebugPathFinding)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(lookAheadPos + Vec3(0.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1.0f, 2.0f, colour);
				}

				UpdatePathPos(m_PathOut, curPos);

				float distanceToEnd = GetLookAheadPoint(m_PathOut, lookDist * 2.0f, lookAheadPos2);

				float accel = 1.0f;
				if (distanceToEnd < m_desiredSpeed)
					accel = distanceToEnd / m_desiredSpeed;

				Vec3 moveDir = lookAheadPos - curPos;
				moveDir.NormalizeSafe(pPipeUser->GetEntity()->GetForwardDir());

				pPipeUser->m_State.predictedCharacterStates.nStates = 0;
				pPipeUser->m_State.vMoveDir = moveDir;
				pPipeUser->m_State.fDistanceToPathEnd = distanceToEnd;
				pPipeUser->m_State.vLookTargetPos = lookAheadPos2;

				if (gAIEnv.CVars.DebugPathFinding)
				{
					colour.r = 0;
					colour.b = 255;
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(lookAheadPos + Vec3(0.0f, 0.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1.0f, 2.0f, colour);
				}

				pPipeUser->SetBodyTargetDir(lookAheadPos2 - curPos);
				pPipeUser->m_State.fDesiredSpeed = m_desiredSpeed * accel;

				result = eGOR_IN_PROGRESS;
			}
			else
			{
				pPipeUser->m_State.predictedCharacterStates.nStates = 0;
				pPipeUser->m_State.vMoveDir = ZERO;
				pPipeUser->m_State.fDesiredSpeed = 0.0f;

				m_State = C2F_INVALID;
			}
		}
		break;
	case C2F_FAILED:
		pPipeUser->m_State.predictedCharacterStates.nStates = 0;
		pPipeUser->m_State.vMoveDir = ZERO;
		pPipeUser->m_State.fDesiredSpeed = 0.0f;
		result = eGOR_FAILED;
		break;
	default:
		m_State = C2F_INVALID;
		result = Chase(pPipeUser);
		break;
	}

	return result;
}

COPCrysis2FlightFireWeapons::COPCrysis2FlightFireWeapons() : m_State(eFP_START), m_NextState(eFP_STOP), m_InitCoolDown(0.0f), m_WaitTime(-1.0f), m_minTime(0.0f), m_maxTime(0.0f), m_PausedTime(0.0f), m_PauseOverrideTime(0.0f), m_target(AI_REG_NONE), m_targetId(0), m_Rotation(0.0f), m_OnlyLookAt(false), m_LastTarget(ZERO), m_FirePrimary(false), m_SecondaryWeapons(0)
{

};

COPCrysis2FlightFireWeapons::COPCrysis2FlightFireWeapons(const XmlNodeRef& node) : m_State(eFP_START), m_NextState(eFP_STOP), m_InitCoolDown(0.0f), m_WaitTime(-1.0f), m_PausedTime(0.0f), m_PauseOverrideTime(0.0f), m_FirePrimary(false), m_SecondaryWeapons(0), m_minTime(0.0f), m_maxTime(0.0f), m_target(AI_REG_NONE), m_targetId(0), m_Rotation(0.0f), m_OnlyLookAt(false), m_LastTarget(ZERO)
{
	node->getAttr("primary", m_FirePrimary);
	node->getAttr("secondary", m_SecondaryWeapons);
	node->getAttr("secondaryAngle", m_Rotation);
	node->getAttr("minFireTime", m_minTime);
	node->getAttr("maxFireTime", m_maxTime);
	s_xml.GetRegister(node, "register", m_target);
}

COPCrysis2FlightFireWeapons::COPCrysis2FlightFireWeapons(EAIRegister reg, float minTime, float maxTime, bool primary, uint32 secondary) : m_State(eFP_START), m_NextState(eFP_STOP), m_InitCoolDown(0.0f), m_WaitTime(-1.0f), m_PausedTime(0.0f), m_PauseOverrideTime(0.0f), m_Rotation(0.0f), m_target(reg), m_minTime(minTime), m_maxTime(maxTime), m_FirePrimary(primary), m_SecondaryWeapons(secondary), m_OnlyLookAt(false), m_LastTarget(ZERO)
{}

COPCrysis2FlightFireWeapons::~COPCrysis2FlightFireWeapons()
{

}

void COPCrysis2FlightFireWeapons::Reset(CPipeUser* pPipeUser)
{
	m_State = eFP_START;
	m_NextState = eFP_STOP;
	m_targetId = 0;
	m_Rotation = 0.0f;
	m_LastTarget = ZERO;

	m_InitCoolDown = 0.0f;

	m_FirePrimary = 0;
	m_SecondaryWeapons = 0;

	m_PausedTime = 0.0f;
	m_PauseOverrideTime = 0.0f;

	m_OnlyLookAt = false;

	pPipeUser->m_State.fire = eAIFS_Off;
	pPipeUser->m_State.fireSecondary = eAIFS_Off;
	pPipeUser->m_State.secondaryIndex = 0;

	pPipeUser->ResetDesiredBodyDirectionAtTarget();
}

void COPCrysis2FlightFireWeapons::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCrysis2FlightFireWeapons");
	{
		ser.Value("m_State", *(alias_cast<int*>(&m_State)));
		ser.Value("m_NextState", *(alias_cast<int*>(&m_NextState)));
		ser.Value("m_InitCoolDown", m_InitCoolDown);
		ser.Value("m_WaitTime", m_WaitTime);
		ser.Value("m_minTime", m_minTime);
		ser.Value("m_maxTime", m_maxTime);
		ser.Value("m_PausedTime", m_PausedTime);
		ser.Value("m_PauseOverrideTime", m_PauseOverrideTime);
		ser.Value("m_Rotation", m_Rotation);
		ser.Value("m_SecondaryWeapons", m_SecondaryWeapons);
		ser.Value("m_targetId", m_targetId);
		{
			union
			{
				EAIRegister* pR;
				int*         pI;
			} u;
			u.pR = &m_target;
			ser.Value("m_target", *u.pI);
		}
		ser.Value("m_LastTarget", m_LastTarget);
		ser.Value("m_FirePrimary", m_FirePrimary);
		ser.Value("m_OnlyLookAt", m_OnlyLookAt);
	}
	ser.EndGroup();
}

void COPCrysis2FlightFireWeapons::ParseParam(const char* param, const GoalParams& value)
{
	if (!stricmp("target", param))
	{
		m_InitCoolDown = 0.0f;
		m_target = static_cast<EAIRegister>(AI_REG_INTERNAL_TARGET);
		//value->getAttr("value", m_targetId);
		value.GetValue(m_targetId);
	}
	else if (!stricmp("useSecondary", param))
	{
		uint32 tmp;
		if (!value.GetValue(tmp))
		{
			float ohNoh;
			if (value.GetValue(ohNoh))
			{
				tmp = static_cast<uint32>(ohNoh);
			}
		}

		switch (tmp)
		{
		case 1:
			m_SecondaryWeapons = 17185;
			m_FirePrimary = false;
			m_Rotation = 0.0f;
			break;

		case 2:
			m_Rotation = -gf_PI * 0.5f;
			m_SecondaryWeapons = 1;
			m_FirePrimary = false;
			break;

		case 3:
			m_Rotation = gf_PI * 0.5f;
			m_SecondaryWeapons = 2;
			m_FirePrimary = false;
			break;

		default:
			m_SecondaryWeapons = 0;
			m_FirePrimary = true;
			break;
		}
	}
	else if (!stricmp("fireDuration", param))
	{
		value.GetValue(m_minTime);
		m_maxTime = m_minTime;
	}
	else if (!stricmp("onlyLookAt", param))
	{
		value.GetValue(m_OnlyLookAt);
	}
	else if (!stricmp("firePause", param))
	{
		bool paused;
		value.GetValue(paused);

		if (paused && (m_State != eFP_PAUSED) && (m_State != eFP_PAUSED_OVERRIDE))
		{
			m_NextState = m_State;
			m_State = eFP_PAUSED;
			m_PausedTime = 0.0f;
			m_PauseOverrideTime = 0.0f;
		}
		else if (!paused && ((m_State == eFP_PAUSED) || (m_State == eFP_PAUSED_OVERRIDE)))
		{
			m_State = m_NextState;
			m_NextState = eFP_STOP;

			m_PausedTime = 0.0f;
			m_PauseOverrideTime = 0.0f;
		}
	}
}

void COPCrysis2FlightFireWeapons::ExecuteDry(CPipeUser* pPipeUser)
{
	Execute(pPipeUser);
}

bool COPCrysis2FlightFireWeapons::CalculateHitPos(CPipeUser* pPipeUser, CAIObject* targetObject, Vec3& target)
{
	bool ret = false;

	if (targetObject)
	{
		target = targetObject->GetPos();
		m_LastTarget = target;
		ret = true;
	}

	return ret;
}

bool COPCrysis2FlightFireWeapons::CalculateHitPosPlayer(CPipeUser* pPipeUser, CAIPlayer* targetPlayer, Vec3& target)
{
	bool ret = false;

	if (targetPlayer)
	{
		target = targetPlayer->GetPos();
		bool cloaked = targetPlayer->IsCloakEffective(pPipeUser->GetPos());

		if (cloaked)
		{
			if (m_LastTarget.IsEquivalent(ZERO))
			{
				target = targetPlayer->GetPos() + cry_random(0.5f, 1.0f) * Vec3(3.0f, 3.0f, 0.0f);
				m_LastTarget = target;
			}
			else
			{
				target = m_LastTarget + cry_random(0.25f, 0.5f) * Vec3(2.0f, 2.0f, 0.0f);
			}

			ret = true;
		}
		else if (CPuppet* puppet = pPipeUser->CastToCPuppet())
		{
			puppet->UpdateTargetTracking(GetWeakRef(targetPlayer), target + Vec3(0.0f, 0.0f, 1.0f));
			puppet->m_Parameters.m_fAccuracy = 1.0f;
			bool canDamage = puppet->CanDamageTarget(targetPlayer);
			bool allowedToHit = true; //puppet->IsAllowedToHitTarget();

			ret = puppet->AdjustFireTarget(targetPlayer, target, allowedToHit && canDamage, 1.0f, DEG2RAD(5.5f), &target);
		}
	}

	return ret;
}

bool COPCrysis2FlightFireWeapons::GetTarget(CPipeUser* pPipeUser, Vec3& target)
{
	bool ret = false;

	switch (m_target)
	{
	case AI_REG_REFPOINT:
		{
			CAIObject* refPoint = pPipeUser->GetRefPoint();
			if (refPoint)
			{
				ret = CalculateHitPos(pPipeUser, refPoint, target);
			}
		}
		break;

	case AI_REG_ATTENTIONTARGET:
		{
			IAIObject* refTarget = pPipeUser->GetAttentionTarget();
			if (refTarget)
			{
				target = refTarget->GetPos();

				if (CAIPlayer* player = refTarget->CastToCAIPlayer())
				{
					ret = CalculateHitPosPlayer(pPipeUser, player, target);
				}
				else
				{
					ret = CalculateHitPos(pPipeUser, static_cast<CAIObject*>(refTarget), target);
				}
			}
		}
		break;

	case AI_REG_INTERNAL_TARGET:
		if (m_targetId != 0)
		{
			IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(m_targetId);

			if (pTargetEntity)
			{
				target = pTargetEntity->GetPos();

				if (IAIObject* object = pTargetEntity->GetAI())
				{
					pPipeUser->SetAttentionTarget(GetWeakRef((CAIObject*)object));

					if (CAIPlayer* player = object->CastToCAIPlayer())
					{
						ret = CalculateHitPosPlayer(pPipeUser, player, target);
					}
					else
					{
						ret = CalculateHitPos(pPipeUser, static_cast<CAIObject*>(object), target);
					}
				}
			}
		}
		break;

	case AI_REG_NONE:
		target = pPipeUser->GetPos() + pPipeUser->GetEntity()->GetForwardDir() * 100.0f;
		m_LastTarget = target;
		ret = true;
		break;
	}

	return ret;
}

void COPCrysis2FlightFireWeapons::ExecuteShoot(CPipeUser* pPipeUser, const Vec3& targetPos)
{
	pPipeUser->m_State.aimTargetIsValid = true;
	pPipeUser->m_State.vAimTargetPos = targetPos;

	Vec3 dir = targetPos - pPipeUser->GetPos();

	if (!m_FirePrimary && m_SecondaryWeapons > 0)
	{
		Matrix33 rot;
		rot.SetRotationZ(m_Rotation);
		dir = dir * rot;
	}

	pPipeUser->SetDesiredBodyDirectionAtTarget(dir);
	pPipeUser->SetFireMode(FIREMODE_VEHICLE);

	if (!m_OnlyLookAt)
	{
		dir.z = 0.0f;
		dir.NormalizeSafe();

		Vec3 forward2D = pPipeUser->GetEntity()->GetForwardDir();
		forward2D.z = 0.0f;

		if (m_FirePrimary && forward2D.dot(dir) > 0.8f)
		{
			pPipeUser->m_State.fire = eAIFS_On;
			if (m_State != eFP_WAIT && m_State != eFP_PAUSED_OVERRIDE)
			{
				m_State = eFP_WAIT_INIT;
			}
		}

		if (m_SecondaryWeapons && forward2D.dot(dir) > 0.75f)
		{
			pPipeUser->m_State.fireSecondary = eAIFS_On;
			pPipeUser->m_State.secondaryIndex = m_SecondaryWeapons;
			if (m_State != eFP_WAIT && m_State != eFP_PAUSED_OVERRIDE)
			{
				m_State = eFP_WAIT_INIT;
			}
		}

		pPipeUser->m_State.fireSecondary = eAIFS_On;
		pPipeUser->m_State.secondaryIndex = m_SecondaryWeapons;
		if (m_State != eFP_WAIT && m_State != eFP_PAUSED_OVERRIDE)
		{
			m_State = eFP_WAIT_INIT;
		}
	}
	else
	{
		pPipeUser->m_State.fire = eAIFS_Off;
		pPipeUser->m_State.fireSecondary = eAIFS_Off;
		pPipeUser->m_State.secondaryIndex = 0;

		if (m_State != eFP_WAIT && m_State != eFP_PAUSED_OVERRIDE)
		{
			m_State = eFP_WAIT_INIT;
		}
	}
};

EGoalOpResult COPCrysis2FlightFireWeapons::Execute(CPipeUser* pPipeUser)
{
	EGoalOpResult ret = eGOR_IN_PROGRESS;

	Vec3 forward = pPipeUser->GetEntity()->GetForwardDir();
	Vec3 playerPos;// = GetAISystem()->GetPlayer()->GetPos();

	if (!GetTarget(pPipeUser, playerPos))
	{
		pPipeUser->m_State.fire = eAIFS_Off;
		pPipeUser->m_State.fireSecondary = eAIFS_Off;
		return eGOR_FAILED;
	}

	switch (m_State)
	{
	case eFP_START:

		if (m_FirePrimary || m_SecondaryWeapons)
		{
			m_State = eFP_PREPARE;
			m_NextState = eFP_STOP;
			ret = eGOR_IN_PROGRESS;
		}
		else
		{
			m_State = eFP_WAIT_INIT;
			m_NextState = eFP_STOP;
			ret = eGOR_IN_PROGRESS;
		}
		break;
	case eFP_STOP:

		if (m_FirePrimary)
		{
			pPipeUser->m_State.fire = eAIFS_Off;
		}

		if (m_SecondaryWeapons)
		{
			pPipeUser->m_State.fireSecondary = eAIFS_Off;
			pPipeUser->m_State.secondaryIndex = 0;
		}

		m_State = eFP_DONE;
		ret = eGOR_DONE;
		break;

	case eFP_WAIT_INIT:
		if (m_maxTime > 0.0f && m_minTime > 0.0f)
		{
			m_WaitTime = gEnv->pTimer->GetCurrTime() + cry_random(m_minTime, m_maxTime);
			m_State = eFP_WAIT;
		}
		else
		{
			m_WaitTime = -1.0f;
			m_State = eFP_RUN_INDEFINITELY;
		}
		m_NextState = eFP_STOP;

	case eFP_WAIT:
	case eFP_PREPARE:
		{
			if (m_State == eFP_WAIT && gEnv->pTimer->GetCurrTime() > m_WaitTime)
			{
				pPipeUser->m_State.fire = eAIFS_Off;
				pPipeUser->m_State.fireSecondary = eAIFS_Off;
				m_State = m_NextState;
				ret = eGOR_IN_PROGRESS;
				break;
			}

		case eFP_RUN_INDEFINITELY:

			ExecuteShoot(pPipeUser, playerPos);

			ret = eGOR_IN_PROGRESS;
		}
		break;
	case eFP_DONE:
		m_State = eFP_START;
		ret = eGOR_DONE;
		break;

	case eFP_PAUSED_OVERRIDE:

		m_PauseOverrideTime -= gEnv->pTimer->GetFrameTime();

		ExecuteShoot(pPipeUser, playerPos);

		if (0.0f > m_PauseOverrideTime)
		{
			m_State = eFP_PAUSED;
			m_PauseOverrideTime = 0.0f;
			m_PausedTime = 0.0f;
		}

	case eFP_PAUSED:
		{
			if (eFP_RUN_INDEFINITELY != m_NextState)
			{
				float delta = gEnv->pTimer->GetFrameTime();
				m_WaitTime += delta;
				m_PausedTime += delta;
			}

			if (!(m_PauseOverrideTime > FLT_EPSILON) && m_PausedTime > 3.0f)
			{
				if (cry_random(0.0f, 1.0f) > 0.9f)
				{
					m_State = eFP_PAUSED_OVERRIDE;
					m_PauseOverrideTime = cry_random(2.0f, 4.0f);
				}
			}

			if (eFP_PAUSED_OVERRIDE != m_State)
			{
				pPipeUser->m_State.fire = eAIFS_Off;
				pPipeUser->m_State.fireSecondary = eAIFS_Off;
				pPipeUser->m_State.secondaryIndex = 0;
			}

			Vec3 dir = playerPos - pPipeUser->GetPos();

			if (!m_FirePrimary && m_SecondaryWeapons > 0)
			{
				Matrix33 rot;
				rot.SetRotationZ(m_Rotation);
				dir = dir * rot;
			}
			pPipeUser->SetDesiredBodyDirectionAtTarget(dir);

			ret = eGOR_IN_PROGRESS;
		}
		break;

	default:
		m_State = eFP_START;
		break;
	}

	return ret;
}

COPCrysis2Hover::COPCrysis2Hover() : m_State(CHS_INVALID), m_target(AI_REG_NONE), m_Continuous(false), m_NextState(CHS_INVALID)
{

}

COPCrysis2Hover::COPCrysis2Hover(EAIRegister reg, bool continuous) : m_State(CHS_INVALID), m_target(reg), m_Continuous(continuous), m_NextState(CHS_INVALID)
{}

COPCrysis2Hover::COPCrysis2Hover(const XmlNodeRef& node) : m_State(CHS_INVALID), m_target(AI_REG_NONE), m_Continuous(false), m_NextState(CHS_INVALID)
{
	s_xml.GetRegister(node, "register", m_target);
	m_Continuous = s_xml.GetBool(node, "continuousUpdate");
}

COPCrysis2Hover::~COPCrysis2Hover()
{

}

void COPCrysis2Hover::Reset(CPipeUser* pPipeUser)
{
	m_State = CHS_INVALID;
	m_NextState = CHS_INVALID;
}

void COPCrysis2Hover::ExecuteDry(CPipeUser* pPipeUser)
{
	Execute(pPipeUser);
}

bool COPCrysis2Hover::GetTarget(CPipeUser* pPipeUser, Vec3& target)
{
	bool ret = false;

	switch (m_target)
	{
	case AI_REG_REFPOINT:
		{
			CAIObject* refPoint = pPipeUser->GetRefPoint();
			if (refPoint)
			{
				target = refPoint->GetPos();
				ret = true;
			}
		}
		break;

	case AI_REG_ATTENTIONTARGET:
		{
			IAIObject* refTarget = pPipeUser->GetAttentionTarget();
			if (refTarget)
			{
				target = refTarget->GetPos();
				ret = true;
			}
		}
		break;
	case AI_REG_NONE:
		{
			Vec3 forward2D = pPipeUser->GetEntity()->GetForwardDir();
			forward2D.z = 0.0f;

			forward2D.Normalize();
			target = pPipeUser->GetPos() + forward2D * 100.0f;
			ret = true;
		}
		break;

	}

	return ret;
}

EGoalOpResult COPCrysis2Hover::Execute(CPipeUser* pPipeUser)
{
	switch (m_State)
	{
	case CHS_INVALID:
		{
			m_NextState = m_Continuous ? CHS_HOVERING : CHS_INVALID;
			if (GetTarget(pPipeUser, m_CurrentTarget))
			{
				//m_CurrentTarget = GetAISystem()->GetPlayer()->GetPos() ;
				Vec3 temp = m_CurrentTarget;
				m_InitialPos = pPipeUser->GetPos();

				Vec3 dir = m_CurrentTarget - m_InitialPos;
				dir.z = 0.0f;
				dir.Normalize();

				m_CurrentTarget -= 20.0f * dir;
				m_CurrentTarget.z += 0.5f;

				m_InitialPos = temp;
				m_InitialForward = m_CurrentTarget - pPipeUser->GetPos(); //ent->GetForwardDir();
				m_InitialForward.Normalize();

				m_InitialTurn = dir; ///Vec3(0.0f, 0.0f, 1.0f); //Vec3(0.0f, 0.0f, 1.0f).Cross(m_InitialForward);
				m_State = CHS_HOVERING;
			}
			else
			{
				pPipeUser->m_State.fDesiredSpeed = 0.0f;
				pPipeUser->m_State.predictedCharacterStates.nStates = 0;
				pPipeUser->m_State.vLookTargetPos = pPipeUser->GetEntity()->GetForwardDir() * 10.0f + pPipeUser->GetPos();

				return eGOR_FAILED;
			}
		}
		break;
	case CHS_HOVERING:
		if (pPipeUser)
		{
			if (m_Continuous && GetTarget(pPipeUser, m_CurrentTarget))
			{
				m_InitialForward = m_CurrentTarget - pPipeUser->GetPos();
				m_InitialForward.Normalize();
			}

			Vec3 currentForward = pPipeUser->GetEntity()->GetForwardDir();
			float dot = m_InitialForward.Dot(currentForward);

			if (dot > 0.99f)
			{
				if (CHS_INVALID != m_NextState)
				{
					m_InitialForward = m_InitialPos - pPipeUser->GetPos();
					m_InitialForward.Normalize();

					m_State = m_NextState;
					return eGOR_IN_PROGRESS;
				}
				else
				{
					return eGOR_DONE;
				}
			}

			Vec3 pos = pPipeUser->GetPos();
			float diffZ = pos.z - m_CurrentTarget.z;
			float speed = static_cast<float>(__fsel(diffZ, -0.05f - diffZ * 0.2f, 0.05f - diffZ * 0.2f));

			pPipeUser->m_State.vMoveDir.x = 0.0f;
			pPipeUser->m_State.vMoveDir.y = 0.0f;
			pPipeUser->m_State.vMoveDir.z = 1.0f;

			pPipeUser->m_State.fDesiredSpeed = speed;
			pPipeUser->m_State.predictedCharacterStates.nStates = 0;

			Vec3 temp = m_CurrentTarget;
			//temp.z = pos.z;
			pPipeUser->m_State.vLookTargetPos = temp; //m_CurrentTarget;

		}
		break;
	case CHS_HOVERING_APPROACH:
		{

			Vec3 currentForward = pPipeUser->GetEntity()->GetForwardDir();
			float dot = m_InitialForward.Dot(currentForward);

			if (dot > 0.95f)
			{
				return eGOR_DONE;
			}

			Vec3 pos = pPipeUser->GetPos();
			float diffZ = pos.z - m_InitialPos.z;
			float speed = static_cast<float>(__fsel(diffZ, -0.05f - diffZ * 0.2f, 0.05f - diffZ * 0.2f));

			pPipeUser->m_State.vMoveDir.x = 0.0f;
			pPipeUser->m_State.vMoveDir.y = 0.0f;
			pPipeUser->m_State.vMoveDir.z = 1.0f;

			pPipeUser->m_State.fDesiredSpeed = speed;
			pPipeUser->m_State.predictedCharacterStates.nStates = 0;
			pPipeUser->m_State.vLookTargetPos = m_InitialPos;
		}

		break;
	}

	return eGOR_IN_PROGRESS;
}
