// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   GoalOp_G04.cpp
   Description: Game 04 goalops
             These should move into GameDLL when interfaces allow!
   -------------------------------------------------------------------------
   History:
   - 21:02:2008 - Created by Matthew Jack
 *********************************************************************/

#include "StdAfx.h"
#if 0
// deprecated and won't compile at all...

	#include "GoalOp_G04.h"
	#include "GoalOpTrace.h"

	#include "TacticalPointSystem/TacticalPointSystem.h"
	#include "PipeUser.h"
	#include "NavRegion.h"

	#define RETURN_SUCCEED     { Reset(pOperand); return eGOR_SUCCEEDED; }
	#define RETURN_FAILED      { Reset(pOperand); return eGOR_FAILED; }
	#define RETURN_IN_PROGRESS return eGOR_IN_PROGRESS;

// Ugly
	#define C_MaxDistanceForPathOffset 2 // threshold (in m) used in COPStick and COPApproach, to detect if the returned path
// is bringing the agent too far from the expected destination

IGoalOp* CGoalOpFactoryG04::GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const
{
	// For now, we look up the central enum - but this should become totally localised
	EGoalOperations op = CGoalPipe::GetGoalOpEnum(sGoalOpName);

	// Should really check these params before passing them on
	// So far all ops use similar params, so don't need switch here
	bool bOk = true;

	switch (op)
	{
	case eGO_G4APPROACH:
		bOk &= pH->GetParam(nFirstParam, params.fValue);
		bOk &= pH->GetParam(nFirstParam + 1, params.fValueAux);

		// Optional parameter forces a direct approach
		params.bValue = false;
		if (pH->GetParamCount() > nFirstParam + 1)
			pH->GetParam(nFirstParam + 2, params.bValue);

		// Fetch register parameter, if any
		params.nValueAux = AI_REG_ATTENTIONTARGET;
		if (pH->GetParamCount() > nFirstParam + 2)
			pH->GetParam(nFirstParam + 3, params.nValueAux);

		break;
	default:
		break;
	}

	if (!bOk) return NULL;

	return GetGoalOp(op, params);
}

IGoalOp* CGoalOpFactoryG04::GetGoalOp(EGoalOperations op, GoalParameters& params) const
{
	IGoalOp* pResult = NULL;

	switch (op)
	{
	case eGO_G4APPROACH:
		pResult = new COPG4Approach(params.fValue, params.fValueAux, params.bValue, static_cast<EAIRegister>(params.nValueAux));
		break;
	default:
		// We don't know this goalop, but another factory might
		return NULL;
	}

	return pResult;
}

////////////////////////////////////////////////////////////////////////////
//
//				G4APPROACH - makes agent approach a target using his environment
//
////////////////////////////////////////////////////////////////////////////

COPG4Approach::COPG4Approach(float fMinEndDistance, float fMaxEndDistance, bool bForceDirect, EAIRegister nReg) :
	m_fMinEndDistance(fMinEndDistance)
	, m_fMaxEndDistance(fMaxEndDistance)
	, m_pTraceDirective(NULL)
	, m_pPathfindDirective(NULL)
	, m_fLastTime(-1.0f)
	, m_bForceDirect(bForceDirect)
	, m_nReg(nReg)
{
	m_fEndAccuracy = 0.1f;
	m_bNeedHidespot = true;
	m_eApproachMode = EOPG4AS_Evaluate;
	m_fNotMovingTime = 0.0f;

	CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
	m_iApproachQueryID = pTPS->GetQueryID("G4Approach");
	if (!m_iApproachQueryID)
	{
		AIError("G4Approach TPS query does not exist - failed script load?");
		assert(false); // Pretty major issue, but fix the script, AIReload, and all should be dandy
	}

	m_iRegenerateCurrentQueryID = pTPS->GetQueryID("AdjustCurrent");
	if (!m_iRegenerateCurrentQueryID)
	{
		AIError("AdjustCurrent TPS query does not exist - failed script load?");
		assert(false); // Pretty major issue, but fix the script, AIReload, and all should be dandy
	}
}

COPG4Approach::COPG4Approach(const XmlNodeRef& node) :
	m_fMinEndDistance(0.f),
	m_fMaxEndDistance(0.f),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_fLastTime(-1.f),
	m_bForceDirect(s_xml.GetBool(node, "forceDirect")),
	m_nReg(AI_REG_NONE),
	m_fEndAccuracy(0.1f),
	m_bNeedHidespot(true),
	m_eApproachMode(EOPG4AS_Evaluate),
	m_fNotMovingTime(0.f)
{
	s_xml.GetMandatory(node, "minEndDistance", m_fMinEndDistance);
	s_xml.GetMandatory(node, "maxEndDistance", m_fMaxEndDistance);

	s_xml.GetRegister(node, "register", m_nReg);
	if (m_nReg == AI_REG_NONE)
	{
		m_nReg = AI_REG_ATTENTIONTARGET;
	}

	CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
	m_iApproachQueryID = pTPS->GetQueryID("G4Approach");
	if (!m_iApproachQueryID)
	{
		AIError("G4Approach TPS query does not exist - failed script load?");
		assert(false); // Pretty major issue, but fix the script, AIReload, and all should be dandy
	}

	m_iRegenerateCurrentQueryID = pTPS->GetQueryID("AdjustCurrent");
	if (!m_iRegenerateCurrentQueryID)
	{
		AIError("AdjustCurrent TPS query does not exist - failed script load?");
		assert(false); // Pretty major issue, but fix the script, AIReload, and all should be dandy
	}
}

COPG4Approach::COPG4Approach(const COPG4Approach& rhs) :
	m_fMinEndDistance(rhs.m_fMinEndDistance),
	m_fMaxEndDistance(rhs.m_fMaxEndDistance),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_fLastTime(-1.f),
	m_bForceDirect(rhs.m_bForceDirect),
	m_nReg(rhs.m_nReg),
	m_fEndAccuracy(0.1f),
	m_bNeedHidespot(true),
	m_eApproachMode(EOPG4AS_Evaluate),
	m_fNotMovingTime(0.f),
	m_iApproachQueryID(rhs.m_iApproachQueryID),
	m_iRegenerateCurrentQueryID(rhs.m_iRegenerateCurrentQueryID)
{
}

COPG4Approach::~COPG4Approach()
{
	Reset(NULL);
}

void COPG4Approach::Reset(CPipeUser* pOperand)
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPG4Approach::Reset %s", pOperand ? pOperand->GetName() : "");

	delete m_pPathfindDirective;
	m_pPathfindDirective = NULL;
	delete m_pTraceDirective;
	m_pTraceDirective = NULL;
	m_bNeedHidespot = true;
	m_eApproachMode = EOPG4AS_Evaluate;
	m_fLastTime = -1.0f;
	m_fNotMovingTime = 0.0f;

	m_refHideTarget.Release();

	if (pOperand)
	{
		pOperand->ClearPath("COPG4Approach::Reset m_Path");
	}
}

void COPG4Approach::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPG4Approach");
	{
		ser.Value("m_fLastDistance", m_fLastDistance);
		ser.Value("m_fMinEndDistance", m_fMinEndDistance);
		ser.Value("m_fMaxEndDistance", m_fMaxEndDistance);
		ser.Value("m_bNeedHidespot", m_bNeedHidespot);
		ser.Value("m_fEndAccuracy", m_fEndAccuracy);

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				assert(m_pTraceDirective);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if (ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}
}

EGoalOpResult COPG4Approach::Execute(CPipeUser* pOperand)
{
	CAISystem* pAISystem = GetAISystem();
	bool bForceReturnPartialPath = false;

	// (MATT) Whether it should look this up once and stores it, or will reflect changes, is an open question {2008/08/09}

	// Get the target
	CAIObject* pTarget = NULL;
	switch (m_nReg)
	{
	case AI_REG_LASTOP:
		pTarget = pOperand->GetLastOpResult();
		break;
	case AI_REG_REFPOINT:
		pTarget = pOperand->GetRefPoint();
		break;
	case AI_REG_ATTENTIONTARGET:
		pTarget = static_cast<CAIObject*>(pOperand->GetAttentionTarget());
		break;
	default:
		AIError("G4Approach failed - unknown AI register");
	}

	if (!pTarget)
	{
		pOperand->SetSignal(0, "OnApproachFailed", pOperand->GetEntity(), NULL, 0); // MTJ: Provide CRC
		AILogAlways("COPG4Approach::Execute - Error - No target: %s", pOperand->GetName());
		RETURN_FAILED;
	}

	if (pOperand->m_nPathDecision != PATHFINDER_PATHFOUND) m_fLastTime = -1.0f;

	// Check for unsuitable agents
	if (pOperand->GetSubType() == CAIObject::STP_HELI
	    || !pOperand->m_movementAbility.bUsePathfinder)
	{
		AILogAlways("COPG4Approach::Execute - Error - Unsuitable agent: %s", pOperand->GetName());
		RETURN_FAILED;
	}

	if (pTarget->GetSubType() == CAIObject::STP_ANIM_TARGET)
	{
		AILogAlways("COPG4Approach::Execute - Error - Unsuitable target: %s", pOperand->GetName());
		RETURN_FAILED;
	}

	// Move strictly to the target point.
	// MTJ: Not sure this should stay
	pOperand->m_bPathfinderConsidersPathTargetDirection = true;

	// Where are we heading eventually?
	Vec3 vAgentPosition(pOperand->GetPos());
	float fDistToEndTarget = vAgentPosition.GetDistance(pTarget->GetPos());

	// Should we just approach directly?
	// Either because we have been forced to, or because we are too close to the target
	if (m_bForceDirect || (m_eApproachMode != EOPG4AS_Direct && fDistToEndTarget < m_fMinEndDistance))
	{
		// Could check if we're actually very close to a hidespot
		m_eApproachMode = EOPG4AS_Direct;
		pOperand->SetInCover(false);
		pOperand->m_CurrentHideObject.Invalidate();
	}

	// Examine whether we should choose another hidepoint
	// We only ever go forward, and pass back control to higher levels if that's not appropriate
	if (m_eApproachMode == EOPG4AS_Evaluate)
	{
		// Do we have a current point we are approaching?
		if (m_refHideTarget.IsSet())
		{
			if (fDistToEndTarget < m_fMaxEndDistance)
			{
				// Point is in acceptable range to stop, so we should, ASAP
				// However, we must regenerate the point to arrive at the best location
				m_eApproachMode = EOPG4AS_GoToHidepoint;
				// Fooo.

				// Could be more subtle here, perhaps moving further into the acceptable range
				// or looking for a better hidespot
			}
			else
			{
				// Let the AI know this is a nice moment to shoot etc
				pOperand->SetSignal(0, "OnPassedCover", pOperand->GetEntity(), NULL, 0); // MTJ: Provide CRC
			}

			// Dispose of current point
			m_refHideTarget.Release();
		}

		// Do we need a new point?
		if (m_refHideTarget.IsNil())
		{
			// Can set to false...
			pOperand->SetInCover(false);

			CTacticalPoint point;
			CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;

			QueryContext context;
			InitQueryContextFromActor(pOperand, context);

			if (m_eApproachMode == EOPG4AS_GoToHidepoint)
			{
				// First make a slightly more ambitious query
				int queryID = pTPS->GetQueryID("G4ApproachSuperior");
				// TODO(marcio): fix
				pTPS->SyncQuery(queryID, /*pOperand->CastToCPuppet()*/ context, point);
				if (!point.IsValid())
				{
					// Just regenerating our last spot
					pTPS->SyncQuery(m_iRegenerateCurrentQueryID, /*pOperand->CastToCPuppet()*/ context, point);
				}
			}
			else
			{
				// Script-build query that should exclude current hidespot
				pTPS->SyncQuery(m_iApproachQueryID, /*pOperand->CastToCPuppet()*/ context, point);
				m_eApproachMode = EOPG4AS_GoNearHidepoint;
			}

			if (!point.IsValid())
			{
				// Can't find any hidespot that will work - switch to direct approach
				m_eApproachMode = EOPG4AS_Direct;
				pOperand->SetInCover(false);
				pOperand->m_CurrentHideObject.Invalidate();
			}
			else
			{
				pOperand->m_CurrentHideObject.Set(point.GetHidespot(), point.GetPos(), point.GetHidespot()->info.dir);
				gAIEnv.pAIObjectManager->CreateDummyObject(m_refHideTarget, string(pOperand->GetName()) + "_HideTarget");
				m_refHideTarget.GetAIObject()->SetPos(point.GetPos());
				pOperand->SetInCover(false);
			}
		}
	}

	// If we have a hidespot, that's our target
	// Otherwise it's the register-derived end target
	if (m_refHideTarget.IsSet()) pTarget = m_refHideTarget.GetAIObject();

	switch (m_pPathfindDirective ? pOperand->m_nPathDecision : PATHFINDER_MAXVALUE)
	{
	case PATHFINDER_MAXVALUE:
		{
			// Generate path to target
			float endTol = (bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy);
			m_pPathfindDirective = new COPPathFind("", pTarget, endTol);
			pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;
			if (m_pPathfindDirective->Execute(pOperand) && pOperand->m_nPathDecision == PATHFINDER_NOPATH)
			{
				// If special nopath signal is specified, send the signal.
				pOperand->m_State.vMoveDir.Set(0, 0, 0);
				RETURN_FAILED;
			}
			break;
		}
	case PATHFINDER_PATHFOUND:
		{
			// if we have a path, trace it
			if (pOperand->m_OrigPath.Empty())
			{
				AILogAlways("COPG4Approach::Execute - Origpath is empty: %s", pOperand->GetName());
				RETURN_FAILED;
			}

			// If we need a tracer, create one
			if (!m_pTraceDirective)
			{
				// MTJ: We create another goalop to achieve this? Hmm.
				bool bExact = false;

				m_pTraceDirective = new COPTrace(bExact, m_fEndAccuracy, bForceReturnPartialPath);
				TPathPoints::const_reference lastPathNode = pOperand->m_OrigPath.GetPath().back();
				Vec3 vLastPos = lastPathNode.vPos;
				Vec3 vRequestedLastNodePos = pOperand->m_Path.GetParams().end;
				float dist = Distance::Point_Point(vLastPos, vRequestedLastNodePos);

				// MTJ: What is this for?
				if (lastPathNode.navType != IAISystem::NAV_SMARTOBJECT && dist > C_MaxDistanceForPathOffset)// && pOperand->m_Path.GetPath().size() == 1 )
				{
					AISignalExtraData* pData = new AISignalExtraData;
					pData->fValue = dist - m_fMinEndDistance;
					pOperand->SetSignal(0, "OnEndPathOffset", pOperand->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnEndPathOffset);
				}
				else
				{
					pOperand->SetSignal(0, "OnPathFound", NULL, 0, gAIEnv.SignalCRCs.m_nOnPathFound);
				}
			}

			// Check if we have finished tracing to the end of our path
			bool bTraceDone = (m_pTraceDirective->Execute(pOperand) != eGOR_IN_PROGRESS);

			CTimeValue time(pAISystem->GetFrameStartTime());
			if (m_fLastTime >= 0.0f)
			{
				// HACK: The following code together with some logic in the execute tries to keep track
				// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
				float dt((time - m_fLastTime).GetSeconds());
				float speed = pOperand->GetVelocity().GetLength();
				float desiredSpeed = pOperand->m_State.fDesiredSpeed;
				if (desiredSpeed > 0.0f)
				{
					float ratio = clamp_tpl(speed / desiredSpeed, 0.0f, 1.0f);
					if (ratio < 0.1f)
						m_fNotMovingTime += dt;
					else
						m_fNotMovingTime -= dt;
					if (m_fNotMovingTime < 0.0f)
						m_fNotMovingTime = 0.0f;
				}
				if (m_fNotMovingTime > 0.5f)
					pOperand->SetSignal(0, "OnStuck", NULL, 0, 0);
			}
			m_fLastTime = time;

			// If this goal gets reseted during m_pTraceDirective->Execute it means that
			// a smart object was used for navigation which inserts a goal pipe which
			// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
			// we should just report that this goal pipe isn't finished since it will be
			// reexecuted after finishing the inserted goal pipe
			// MTJ: WTF?
			if (!m_pTraceDirective)
			{
				AILogAlways("COPG4Approach::Execute - Why am I here?: %s", pOperand->GetName());
				RETURN_FAILED;
			}

			Vec3 vTargetPos = pTarget->GetPos();
			float fDistToHopTarget = vAgentPosition.GetDistance(vTargetPos);
			if (m_eApproachMode == EOPG4AS_GoNearHidepoint)
			{
				if (bTraceDone || fDistToHopTarget < 2.0f)
				{
					m_eApproachMode = EOPG4AS_Evaluate; // Check where we are now
					delete m_pPathfindDirective;
					m_pPathfindDirective = NULL;
				}

			}
			else if (m_eApproachMode == EOPG4AS_GoToHidepoint)
			{
				if (bTraceDone)
				{
					pOperand->SetSignal(0, "OnApproachedToCover", pOperand->GetEntity(), NULL, 0); // MTJ: Provide CRC
					RETURN_SUCCEED;
				}
			}
			else if (m_eApproachMode == EOPG4AS_Direct)
			{
				if (bTraceDone || fDistToHopTarget < m_fMinEndDistance)
				{
					pOperand->SetSignal(0, "OnApproachedToOpen", pOperand->GetEntity(), NULL, 0); // MTJ: Provide CRC
					RETURN_SUCCEED;
				}

				// Otherwise, consider regenerating, stick-style
				if (!pOperand->m_Path.Empty())
				{
					// Where does our current path end?
					Vec3 pathEnd = pOperand->m_Path.GetLastPathPoint()->vPos;
					// How far is that from our target?
					float fTolerance = m_fMinEndDistance * 0.8f;
					float fDrift = pathEnd.GetDistance(pTarget->GetPos());
					if (fDrift > fTolerance)
					{
						// Reset for new pathfinding
						m_pPathfindDirective->Reset(pOperand);
						m_pTraceDirective->m_fEndAccuracy = m_fEndAccuracy;
						pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;

						// Try to shortcut that with a direct connection to avoid the 4-frame lag
						bool bOk = true;
						int nbid;
						IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(pOperand->GetPos(), nbid,
						                                                                             pOperand->m_movementAbility.pathfindingProperties.navCapMask);
						CNavRegion* pRegion = gAIEnv.pNavigation->GetNavRegion(navType, gAIEnv.pGraph);
						if (!pRegion) bOk = false;

						NavigationBlockers navBlocker;
						bOk = bOk && pRegion->CheckPassability(vAgentPosition, vTargetPos, pOperand->GetParameters().m_fPassRadius, navBlocker, pOperand->GetMovementAbility().pathfindingProperties.navCapMask);

						if (navType == IAISystem::NAV_TRIANGULAR)
						{
							// Make sure not to enter forbidden area.
							bOk = bOk && !gAIEnv.pNavigation->IsPathForbidden(vAgentPosition, vTargetPos);
							bOk = bOk && !gAIEnv.pNavigation->IsPointForbidden(vTargetPos, pOperand->GetParameters().m_fPassRadius);
						}

						if (bOk)
						{
							pOperand->ClearPath("COPG4Approach::Clearing m_Path for shortcut");

							PathPointDescriptor pt;
							pt.navType = navType;
							pt.vPos = vAgentPosition;
							pOperand->m_Path.PushBack(pt);

							pt.vPos = vTargetPos;
							pOperand->m_Path.PushBack(pt);

							// Double-check that we aren't already at the end, in which case won't have minimal path
							if (pOperand->m_Path.Empty())
								RETURN_FAILED;

							// Effectively cancel our own decision to pathfind, as we've done it ourselves
							// Could perhaps mention end distance, but shoudl really do it everywhere, not just here
							pOperand->m_Path.SetParams(SNavPathParams(vAgentPosition, vTargetPos, Vec3(ZERO), Vec3(ZERO), -1, false, 0.0f, true));
							pOperand->m_OrigPath = pOperand->m_Path;
							pOperand->m_nPathDecision = PATHFINDER_PATHFOUND;
						}
						else
						{
							// Shortcutting failed, but we've already started changing the path, which isn't good
							// At least try to go back to consistent state
							pOperand->ClearPath("COPG4Approach::Clearing m_Path from failed shortcut");
						}
					}
				}
			}
			break;
		}
	case PATHFINDER_NOPATH:
		{
			pOperand->SetSignal(0, "OnApproachFailed", pOperand->GetEntity(), NULL, 0); // MTJ: Provide CRC
			RETURN_FAILED;
		}
	default:
		{
			// MTJ: Still pathfinding I guess?
			m_pPathfindDirective->Execute(pOperand);
			break;
		}
	}
	// Run me again next tick
	RETURN_IN_PROGRESS;
}

void COPG4Approach::DebugDraw(CPipeUser* pOperand) const
{
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pOperand);
	if (m_pPathfindDirective)
		m_pPathfindDirective->DebugDraw(pOperand);
}

void COPG4Approach::ExecuteDry(CPipeUser* pOperand)
{
	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pOperand, false);
}
#endif // 0
