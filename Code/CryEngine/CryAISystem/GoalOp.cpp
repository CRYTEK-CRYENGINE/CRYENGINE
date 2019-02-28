// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GoalOp.h"
#include "GoalOpTrace.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "AILog.h"
#include <CrySystem/IConsole.h>
#include "AICollision.h"
#include "PipeUser.h"
#include "Leader.h"
#include "DebugDrawContext.h"
#include "PathFollower.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CryPhysics/IPhysics.h>
#include <CryMath/Cry_Math.h>
#include <CrySystem/ILog.h>
#include <CryNetwork/ISerialize.h>
#include "TacticalPointSystem/TacticalPointSystem.h"

#include "Communication/CommunicationManager.h"

#include "GameSpecific/GoalOp_Crysis2.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

// Ugly
#define C_MaxDistanceForPathOffset 2 // threshold (in m) used in COPStick and COPApproach, to detect if the returned path
// is bringing the agent too far from the expected destination

CGoalOpParallel::~CGoalOpParallel()
{
	SAFE_DELETE(m_NextConcurrentPipe);
	SAFE_DELETE(m_ConcurrentPipe);
}

void CGoalOpParallel::ParseParams(const GoalParams& node)  //(const XmlNodeRef &node)
{
	uint32 count = node.GetChildCount();//node->getChildCount();
	for (uint32 loop = 0; loop < count; ++loop)
	{
		const GoalParams& child = node.GetChild(loop);
		ParseParam(child.GetName(), child);
	}

	if (m_NextConcurrentPipe)
	{
		m_NextConcurrentPipe->ParseParams(node);
	}
	else if (m_ConcurrentPipe)
	{
		m_ConcurrentPipe->ParseParams(node);
	}
}

void CGoalOpParallel::SetConcurrentPipe(CGoalPipe* goalPipe)
{
	if (m_ConcurrentPipe)
	{
		m_NextConcurrentPipe = goalPipe;
	}
	else
	{
		m_ConcurrentPipe = goalPipe;

		if (m_ConcurrentPipe)
		{
			m_OpInfos.resize(0);
			uint32 count = m_ConcurrentPipe->GetNumGoalOps();

			m_OpInfos.resize(count);

			for (uint32 loop = 0; loop < count; ++loop)
			{
				m_OpInfos[loop].blocking = m_ConcurrentPipe->IsGoalBlocking(loop);
			}
		}
	}
}

void CGoalOpParallel::ReleaseConcurrentPipe(CPipeUser* pPipeUser, bool clearNextPipe)
{
	if (m_ConcurrentPipe)
	{
		m_ConcurrentPipe->ResetGoalops(pPipeUser);
		pPipeUser->NotifyListeners(m_ConcurrentPipe, ePN_Finished);
		delete m_ConcurrentPipe;
		m_ConcurrentPipe = 0;
	}

	if (clearNextPipe && m_NextConcurrentPipe)
	{
		m_NextConcurrentPipe->ResetGoalops(pPipeUser);
		pPipeUser->NotifyListeners(m_NextConcurrentPipe, ePN_Finished);
		delete m_NextConcurrentPipe;
		m_NextConcurrentPipe = 0;
	}
}

EGoalOpResult CGoalOpParallel::Execute(CPipeUser* pPipeUser)
{
	uint32 executed = 0;

	if (m_NextConcurrentPipe)
	{
		ReleaseConcurrentPipe(pPipeUser);
		SetConcurrentPipe(m_NextConcurrentPipe);
		m_NextConcurrentPipe = 0;
	}

	if (m_ConcurrentPipe)
	{
		uint32 count = m_ConcurrentPipe->GetNumGoalOps();

		for (uint32 loop = 0; loop < count; ++loop)
		{
			CGoalOp* op = m_ConcurrentPipe->GetGoalOp(loop);

			if ((op != NULL) && (!m_OpInfos[loop].done))
			{
				EGoalOpResult result = op->Execute(pPipeUser);
				++executed;

				if (result != eGOR_IN_PROGRESS)
				{
					--executed;
					m_OpInfos[loop].done = true;
				}
				else if (m_OpInfos[loop].blocking)
				{
					break;
				}
			}
		}
	}

	return (m_ConcurrentPipe == 0) ? eGOR_NONE : (executed > 0) ? eGOR_IN_PROGRESS : eGOR_DONE;
}

void CGoalOpParallel::ExecuteDry(CPipeUser* pPipeUser)
{
};

CGoalOpXMLReader CGoalOp::s_xml;

CGoalOpXMLReader::CGoalOpXMLReader()
{
	m_dictAIObjectType.Add("None", AIOBJECT_NONE);
	m_dictAIObjectType.Add("Dummy", AIOBJECT_DUMMY);
	m_dictAIObjectType.Add("Actor", AIOBJECT_ACTOR);
	m_dictAIObjectType.Add("Vehicle", AIOBJECT_VEHICLE);
	m_dictAIObjectType.Add("Target", AIOBJECT_TARGET);
	m_dictAIObjectType.Add("Aware", AIOBJECT_AWARE);
	m_dictAIObjectType.Add("Attribute", AIOBJECT_ATTRIBUTE);
	m_dictAIObjectType.Add("WayPoint", AIOBJECT_WAYPOINT);
	m_dictAIObjectType.Add("HidePoint", AIOBJECT_HIDEPOINT);
	m_dictAIObjectType.Add("SoundSupressor", AIOBJECT_SNDSUPRESSOR);
	m_dictAIObjectType.Add("Helicopter", AIOBJECT_HELICOPTER);
	m_dictAIObjectType.Add("Infected", AIOBJECT_INFECTED);
	m_dictAIObjectType.Add("AlienTick", AIOBJECT_ALIENTICK);
	m_dictAIObjectType.Add("Car", AIOBJECT_CAR);
	m_dictAIObjectType.Add("Boat", AIOBJECT_BOAT);
	m_dictAIObjectType.Add("Airplane", AIOBJECT_AIRPLANE);
	m_dictAIObjectType.Add("2DFly", AIOBJECT_2D_FLY);
	m_dictAIObjectType.Add("MountedWeapon", AIOBJECT_MOUNTEDWEAPON);
	m_dictAIObjectType.Add("GlobalAlertness", AIOBJECT_GLOBALALERTNESS);
	m_dictAIObjectType.Add("Leader", AIOBJECT_LEADER);
	m_dictAIObjectType.Add("Order", AIOBJECT_ORDER);
	m_dictAIObjectType.Add("Player", AIOBJECT_PLAYER);
	m_dictAIObjectType.Add("Grenade", AIOBJECT_GRENADE);
	m_dictAIObjectType.Add("RPG", AIOBJECT_RPG);

	m_dictAnimationMode.Add("Signal", AIANIM_SIGNAL);
	m_dictAnimationMode.Add("Action", AIANIM_ACTION);

	m_dictBools.Add("false", false);
	m_dictBools.Add("true", true);

	m_dictCoverLocation.Add("None", eCUL_None);
	m_dictCoverLocation.Add("Automatic", eCUL_Automatic);
	m_dictCoverLocation.Add("Left", eCUL_Left);
	m_dictCoverLocation.Add("Right", eCUL_Right);
	m_dictCoverLocation.Add("Center", eCUL_Center);

	m_dictFireMode.Add("Off", FIREMODE_OFF);
	m_dictFireMode.Add("Burst", FIREMODE_BURST);
	m_dictFireMode.Add("Continuous", FIREMODE_CONTINUOUS);
	m_dictFireMode.Add("Forced", FIREMODE_FORCED);
	m_dictFireMode.Add("Aim", FIREMODE_AIM);
	m_dictFireMode.Add("Secondary", FIREMODE_SECONDARY);
	m_dictFireMode.Add("SecondarySmoke", FIREMODE_SECONDARY_SMOKE);
	m_dictFireMode.Add("Melee", FIREMODE_MELEE);
	m_dictFireMode.Add("Kill", FIREMODE_KILL);
	m_dictFireMode.Add("BurstWhileMoving", FIREMODE_BURST_WHILE_MOVING);
	m_dictFireMode.Add("PanicSpread", FIREMODE_PANIC_SPREAD);
	m_dictFireMode.Add("BurstDrawFire", FIREMODE_BURST_DRAWFIRE);
	m_dictFireMode.Add("MeleeForced", FIREMODE_MELEE_FORCED);
	m_dictFireMode.Add("BurstSnipe", FIREMODE_BURST_SNIPE);
	m_dictFireMode.Add("AimSweep", FIREMODE_AIM_SWEEP);
	m_dictFireMode.Add("BurstOnce", FIREMODE_BURST_ONCE);

	m_dictLook.Add("Look", AILOOKMOTIVATION_LOOK);
	m_dictLook.Add("Glance", AILOOKMOTIVATION_GLANCE);
	m_dictLook.Add("Startle", AILOOKMOTIVATION_STARTLE);
	m_dictLook.Add("DoubleTake", AILOOKMOTIVATION_DOUBLETAKE);

	m_dictRegister.Add("LastOp", AI_REG_LASTOP);
	m_dictRegister.Add("RefPoint", AI_REG_REFPOINT);
	m_dictRegister.Add("AttTarget", AI_REG_ATTENTIONTARGET);
	m_dictRegister.Add("Path", AI_REG_PATH);
	m_dictRegister.Add("Cover", AI_REG_COVER);

	m_dictSignalFilter.Add("Sender", SIGNALFILTER_SENDER);
	m_dictSignalFilter.Add("LastOp", SIGNALFILTER_LASTOP);
	m_dictSignalFilter.Add("GroupOnly", SIGNALFILTER_GROUPONLY);
	m_dictSignalFilter.Add("FactionOnly", SIGNALFILTER_FACTIONONLY);
	m_dictSignalFilter.Add("AnyoneInComm", SIGNALFILTER_ANYONEINCOMM);
	m_dictSignalFilter.Add("Target", SIGNALFILTER_TARGET);
	m_dictSignalFilter.Add("SuperGroup", SIGNALFILTER_SUPERGROUP);
	m_dictSignalFilter.Add("SuperFaction", SIGNALFILTER_SUPERFACTION);
	m_dictSignalFilter.Add("SuperTarget", SIGNALFILTER_SUPERTARGET);
	m_dictSignalFilter.Add("NearestGroup", SIGNALFILTER_NEARESTGROUP);
	m_dictSignalFilter.Add("NearestSpecies", SIGNALFILTER_NEARESTSPECIES);
	m_dictSignalFilter.Add("NearestInComm", SIGNALFILTER_NEARESTINCOMM);
	m_dictSignalFilter.Add("HalfOfGroup", SIGNALFILTER_HALFOFGROUP);
	m_dictSignalFilter.Add("Leader", SIGNALFILTER_LEADER);
	m_dictSignalFilter.Add("GroupOnlyExcept", SIGNALFILTER_GROUPONLY_EXCEPT);
	m_dictSignalFilter.Add("AnyoneInCommExcept", SIGNALFILTER_ANYONEINCOMM_EXCEPT);
	m_dictSignalFilter.Add("LeaderEntity", SIGNALFILTER_LEADERENTITY);
	m_dictSignalFilter.Add("NearestInCommFaction", SIGNALFILTER_NEARESTINCOMM_FACTION);
	m_dictSignalFilter.Add("NearestInCommLooking", SIGNALFILTER_NEARESTINCOMM_LOOKING);
	m_dictSignalFilter.Add("Formation", SIGNALFILTER_FORMATION);
	m_dictSignalFilter.Add("FormationExcept", SIGNALFILTER_FORMATION_EXCEPT);
	m_dictSignalFilter.Add("Readability", SIGNALFILTER_READABILITY);
	m_dictSignalFilter.Add("ReadabilityAnticipation", SIGNALFILTER_READABILITYAT);
	m_dictSignalFilter.Add("ReadabilityResponse", SIGNALFILTER_READABILITYRESPONSE);

	m_dictStance.Add("Null", STANCE_NULL);
	m_dictStance.Add("Stand", STANCE_STAND);
	m_dictStance.Add("Crouch", STANCE_CROUCH);
	m_dictStance.Add("Prone", STANCE_PRONE);
	m_dictStance.Add("Relaxed", STANCE_RELAXED);
	m_dictStance.Add("Stealth", STANCE_STEALTH);
	m_dictStance.Add("Alerted", STANCE_ALERTED);
	m_dictStance.Add("LowCover", STANCE_LOW_COVER);
	m_dictStance.Add("HighCover", STANCE_HIGH_COVER);
	m_dictStance.Add("Swim", STANCE_SWIM);
	m_dictStance.Add("ZeroG", STANCE_ZEROG);

	m_dictUrgency.Add("Zero", AISPEED_ZERO);
	m_dictUrgency.Add("Slow", AISPEED_SLOW);
	m_dictUrgency.Add("Walk", AISPEED_WALK);
	m_dictUrgency.Add("Run", AISPEED_RUN);
	m_dictUrgency.Add("Sprint", AISPEED_SPRINT);
}

bool CGoalOpXMLReader::GetBool(const XmlNodeRef& node, const char* szAttrName, bool bDefaultValue)
{
	bool bValue = false;
	if (m_dictBools.Get(node, szAttrName, bValue))
	{
		return bValue;
	}

	return bDefaultValue;
}

bool CGoalOpXMLReader::GetMandatoryBool(const XmlNodeRef& node, const char* szAttrName)
{
	bool bValue;
	return m_dictBools.Get(node, szAttrName, bValue, true) ? bValue : false;
}

const char* CGoalOpXMLReader::GetMandatoryString(const XmlNodeRef& node, const char* szAttrName)
{
	const char* sz;
	if (!node->getAttr(szAttrName, &sz))
	{
		AIError("Unable to get mandatory string attribute '%s' of node '%s'.",
		        szAttrName, node->getTag());
		sz = "";
	}

	return sz;
}

//#pragma optimize("", off)
//#pragma inline_depth(0)

// The end point accuracy to use when tracing and we don't really care. 0 results in exact positioning being used
// a small value (0.1) allows some sloppiness. Both should work.
static float defaultTraceEndAccuracy = 0.1f;

EGoalOpResult COPAcqTarget::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPAcqTarget_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	pPipeUser->m_bLooseAttention = false;

	CAIObject* pTargetObject = m_sTargetName.empty() ? 0 : pPipeUser->GetSpecialAIObject(m_sTargetName);

	pPipeUser->SetAttentionTarget(pTargetObject ? GetWeakRef(pTargetObject) : pPipeUser->m_refLastOpResult);

	return eGOR_DONE;
}

COPApproach::COPApproach(float fEndDistance, float fEndAccuracy, float fDuration,
                         bool bUseLastOpResult, bool bLookAtLastOp,
                         bool bForceReturnPartialPath, bool bStopOnAnimationStart,
                         const char* szNoPathSignalText) :
	m_fLastDistance(0.0f),
	m_fInitialDistance(0.0f),
	m_bUseLastOpResult(bUseLastOpResult),
	m_bLookAtLastOp(bLookAtLastOp),
	m_fEndDistance(fEndDistance),
	m_fEndAccuracy(fEndAccuracy),
	m_fDuration(fDuration),
	m_bForceReturnPartialPath(bForceReturnPartialPath),
	m_stopOnAnimationStart(bStopOnAnimationStart),
	m_looseAttentionId(0),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_bPathFound(false)
{
	CCCPOINT(COPApproach_COPApproach);
}

COPApproach::COPApproach(const XmlNodeRef& node) :
	m_fLastDistance(0.f),
	m_fInitialDistance(0.f),
	m_bUseLastOpResult(s_xml.GetBool(node, "useLastOp")),
	m_bLookAtLastOp(s_xml.GetBool(node, "lookAtLastOp")),
	m_fEndDistance(0.f),
	m_fEndAccuracy(0.f),
	m_fDuration(0.f),
	m_bForceReturnPartialPath(s_xml.GetBool(node, "requestPartialPath")),
	m_stopOnAnimationStart(s_xml.GetBool(node, "stopOnAnimationStart")),
	m_looseAttentionId(0),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_bPathFound(false)
{
	if (node->getAttr("time", m_fDuration))
	{
		m_fDuration = fabsf(m_fDuration);
	}
	else
	{
		s_xml.GetMandatory(node, "distance", m_fEndDistance);
	}

	node->getAttr("endAccuracy", m_fEndAccuracy);
}

void COPApproach::DebugDraw(CPipeUser* pPipeUser) const
{
	// TODO evgeny Consider an abstract class to avoid retyping m_pTraceDirective/m_pPathfindDirective
	if (m_pTraceDirective)
	{
		m_pTraceDirective->DebugDraw(pPipeUser);
	}

	if (m_pPathfindDirective)
	{
		m_pPathfindDirective->DebugDraw(pPipeUser);
	}
}

EGoalOpResult COPApproach::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPApproach_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	int debugPathfinding = gAIEnv.CVars.DebugPathFinding;

	CAIObject* pTarget = static_cast<CAIObject*>(pPipeUser->GetAttentionTarget());

	// Move strictly to the target point
	pPipeUser->m_bPathfinderConsidersPathTargetDirection = true;

	if (!pTarget || m_bUseLastOpResult)
	{
		CAIObject* pLastOpResult = pPipeUser->m_refLastOpResult.GetAIObject();
		if (pLastOpResult)
		{
			pTarget = pLastOpResult;
		}
		else
		{
			// no target, nothing to approach to
			if (debugPathfinding)
				AILogAlways("COPApproach::Execute %s no target; resetting goalop", GetNameSafe(pPipeUser));
			Reset(pPipeUser);
			return eGOR_FAILED;
		}
	}

	// luciano - added check for formation points
	if (pTarget && !pTarget->IsEnabled())
	{
		if (debugPathfinding)
			AILogAlways("COPApproach::Execute %s target %s not enabled", GetNameSafe(pPipeUser), pTarget->GetName());
		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	Vec3 mypos = pPipeUser->GetPos();

	Vec3 targetpos;
	Vec3 targetdir;
	if ((pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND) && m_bForceReturnPartialPath)
	{
		targetpos = pPipeUser->m_Path.GetLastPathPos();
		targetdir = pPipeUser->m_Path.GetEndDir();
	}
	else
	{
		targetpos = pTarget->GetPhysicsPos();
		targetdir = pTarget->GetMoveDir();
	}

	CWeakRef<CAIObject>& refLastOpResult = pPipeUser->m_refLastOpResult;
	if (!m_looseAttentionId && m_bLookAtLastOp && refLastOpResult.IsValid())
	{
		m_looseAttentionId = pPipeUser->SetLooseAttentionTarget(refLastOpResult);
	}

	if (pPipeUser->GetSubType() == CAIObject::STP_HELI)
	{
		// Make sure helicopter gets up first, and then starts moving
		if (CAIVehicle* pHeli = pPipeUser->CastToCAIVehicle())
			if (pHeli->HandleVerticalMovement(targetpos))
				return eGOR_IN_PROGRESS;
	}

	if (!pPipeUser->m_movementAbility.bUsePathfinder)
	{
		Vec3 projectedDist = mypos - targetpos;
		float dist = pPipeUser->m_movementAbility.b3DMove ? projectedDist.GetLength() : projectedDist.GetLength2D();

		float endDistance = GetEndDistance(pPipeUser);

		if (dist < endDistance)
		{
			if (debugPathfinding)
				AILogAlways("COPApproach::Execute %s No pathfinder and reached end", GetNameSafe(pPipeUser));
			m_fInitialDistance = 0;
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}

		// no pathfinding - just approach
		pPipeUser->m_State.vMoveDir = targetpos - pPipeUser->GetPhysicsPos();
		pPipeUser->m_State.vMoveDir.Normalize();

#ifdef _DEBUG
		// Update the debug movement reason.
		pPipeUser->m_DEBUGmovementReason = CPipeUser::AIMORE_SMARTOBJECT;
#endif

		m_fLastDistance = dist;
		return eGOR_IN_PROGRESS;
	}

	if (!m_bPathFound && !m_pPathfindDirective)
	{
		// generate path to target
		float endTol = m_bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy;

		// override end distance if a duration has been set
		float endDistance = GetEndDistance(pPipeUser);
		m_pPathfindDirective = new COPPathFind("", pTarget, endTol, endDistance);
		pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;

		if (m_pPathfindDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS)
		{
			if (pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
			{
				if (debugPathfinding)
					AILogAlways("COPApproach::Execute %s pathfinder no path", GetNameSafe(pPipeUser));
				// If special nopath signal is specified, send the signal.
				if (m_noPathSignalText.size() > 0)
					pPipeUser->SetSignal(0, m_noPathSignalText.c_str(), NULL);
				pPipeUser->m_State.vMoveDir.zero();
				Reset(pPipeUser);
				return eGOR_FAILED;
			}
		}
		return eGOR_IN_PROGRESS;
	}

	// trace/pathfinding gets deleted when we reach the end
	if (!m_pPathfindDirective)
	{
		if (debugPathfinding)
		{
			AILogAlways("COPApproach::Execute (%p) returning true due to no pathfinding directive %s", this, GetNameSafe(pPipeUser));
		}
		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	// actually trace the path - continue doing this even whilst regenerating (etc) the path
	EGoalOpResult doneTracing = eGOR_IN_PROGRESS;
	if (m_bPathFound)
	{
		if (!m_pTraceDirective)
		{
			if ((pTarget->GetSubType() == CAIObject::STP_ANIM_TARGET) && (m_fEndDistance > 0.0f || m_fDuration > 0.0f))
			{
				AILogAlways("COPApproach::Execute resetting approach distance from (endDist=%.1f duration=%.1f) to zero because the approach target is anim target. %s",
				            m_fEndDistance, m_fDuration, GetNameSafe(pPipeUser));
				m_fEndDistance = 0.0f;
				m_fDuration = 0.0f;
			}

			TPathPoints::const_reference lastPathNode = pPipeUser->m_OrigPath.GetPath().back();
			Vec3 lastPos = lastPathNode.vPos;
			Vec3 requestedLastNodePos = pPipeUser->m_Path.GetParams().end;
			float dist = Distance::Point_Point(lastPos, requestedLastNodePos);
			float endDistance = GetEndDistance(pPipeUser);
			if (lastPathNode.navType != IAISystem::NAV_SMARTOBJECT && dist > endDistance + C_MaxDistanceForPathOffset)// && pPipeUser->m_Path.GetPath().size() == 1 )
			{
				AISignalExtraData* pData = new AISignalExtraData;
				pData->fValue = dist - endDistance;
				pPipeUser->SetSignal(0, "OnEndPathOffset", pPipeUser->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnEndPathOffset);
			}
			else
			{
				pPipeUser->SetSignal(0, "OnPathFound", NULL, 0, gAIEnv.SignalCRCs.m_nOnPathFound);
			}

			bool bExact = false;
			//      m_pTraceDirective = new COPTrace(bExact, endDistance, m_fEndAccuracy, m_fDuration, m_bForceReturnPartialPath, m_stopOnAnimationStart);
			m_pTraceDirective = new COPTrace(bExact, m_fEndAccuracy, m_bForceReturnPartialPath, m_stopOnAnimationStart);
		}

		doneTracing = m_pTraceDirective->Execute(pPipeUser);

		// If this goal gets reseted during m_pTraceDirective->Execute it means that
		// a smart object was used for navigation which inserts a goal pipe which
		// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
		// we should just report that this goal pipe isn't finished since it will be
		// reexecuted after finishing the inserted goal pipe
		if (!m_pTraceDirective)
			return eGOR_IN_PROGRESS;

		// If the path has been traced, finish the operation
		if (doneTracing != eGOR_IN_PROGRESS)
		{
			if (debugPathfinding)
				AILogAlways("COPApproach::Execute (%p) finishing due to finished tracing %s", this, GetNameSafe(pPipeUser));
			Reset(pPipeUser);
			return doneTracing;
		}
	}

	// check pathfinder status
	switch (pPipeUser->m_nPathDecision)
	{
	case PATHFINDER_STILLFINDING:
		m_pPathfindDirective->Execute(pPipeUser);
		return eGOR_IN_PROGRESS;

	case PATHFINDER_NOPATH:
		pPipeUser->m_State.vMoveDir.zero();
		if (debugPathfinding)
			AILogAlways("COPApproach::Execute (%p) resetting due to no path %s", this, GetNameSafe(pPipeUser));
		// If special nopath signal is specified, send the signal.
		if (m_noPathSignalText.size() > 0)
			pPipeUser->SetSignal(0, m_noPathSignalText.c_str(), NULL);
		Reset(pPipeUser);
		return eGOR_FAILED;

	case PATHFINDER_PATHFOUND:
		if (!m_bPathFound)
		{
			m_bPathFound = true;
			return Execute(pPipeUser);
		}
		break;
	}

	return eGOR_IN_PROGRESS;
}

void COPApproach::Reset(CPipeUser* pPipeUser)
{
	if (gAIEnv.CVars.DebugPathFinding)
	{
		AILogAlways("COPApproach::Reset %s", GetNameSafe(pPipeUser));
	}

	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	m_bPathFound = false;

	m_fInitialDistance = 0;

	if (pPipeUser)
	{
		pPipeUser->ClearPath("COPApproach::Reset m_Path");
		if (m_bLookAtLastOp)
		{
			pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
}

void COPApproach::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPApproach");
	{
		ser.Value("m_fLastDistance", m_fLastDistance);
		ser.Value("m_fInitialDistance", m_fInitialDistance);
		ser.Value("m_bUseLastOpResult", m_bUseLastOpResult);
		ser.Value("m_bLookAtLastOp", m_bLookAtLastOp);
		ser.Value("m_fEndDistance", m_fEndDistance);
		ser.Value("m_fEndAccuracy", m_fEndAccuracy);
		ser.Value("m_fDuration", m_fDuration);
		ser.Value("m_bForceReturnPartialPath", m_bForceReturnPartialPath);
		ser.Value("m_stopOnAnimationStart", m_stopOnAnimationStart);
		ser.Value("m_noPathSignalText", m_noPathSignalText);
		ser.Value("m_looseAttentionId", m_looseAttentionId);
		ser.Value("m_bPathFound", m_bPathFound);

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
float COPApproach::GetEndDistance(CPipeUser* pPipeUser) const
{
	if (m_fDuration > 0.0f)
	{
		//		float normalSpeed = pPipeUser->GetNormalMovementSpeed(pPipeUser->m_State.fMovementUrgency, false);
		float normalSpeed, smin, smax;
		pPipeUser->GetMovementSpeedRange(pPipeUser->m_State.fMovementUrgency, false, normalSpeed, smin, smax);
		if (normalSpeed > 0.0f)
			return -normalSpeed * m_fDuration;
	}
	return m_fEndDistance;
}

//===================================================================
// ExecuteDry
//===================================================================
void COPApproach::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_pTraceDirective && m_bPathFound)
		m_pTraceDirective->ExecuteTrace(pPipeUser, false);
}

// COPFollowPath
//====================================================================
COPFollowPath::COPFollowPath(bool pathFindToStart, bool reverse, bool startNearest, int loops, float fEndAccuracy, bool bUsePointList, bool bControlSpeed, float desiredSpeedOnPath) :
	m_pathFindToStart(pathFindToStart),
	m_reversePath(reverse),
	m_startNearest(startNearest),
	m_bUsePointList(bUsePointList),
	m_bControlSpeed(bControlSpeed),
	m_loops(loops),
	m_loopCounter(0),
	m_notMovingTimeMs(0),
	m_returningToPath(false),
	m_fDesiredSpeedOnPath(desiredSpeedOnPath)
{
	m_pTraceDirective = 0;
	m_pPathFindDirective = 0;
	m_fEndAccuracy = fEndAccuracy;
}

COPFollowPath::COPFollowPath(const XmlNodeRef& node) :
	m_pathFindToStart(s_xml.GetBool(node, "pathFindToStart", true)),
	m_reversePath(s_xml.GetBool(node, "reversePath", true)),
	m_startNearest(s_xml.GetBool(node, "startNearest", true)),
	m_bUsePointList(s_xml.GetBool(node, "usePointList")),
	m_loops(0),
	m_loopCounter(0),
	m_notMovingTimeMs(0),
	m_returningToPath(false),
	m_pTraceDirective(0),
	m_pPathFindDirective(0),
	m_fEndAccuracy(0),
	m_bControlSpeed(true),
	m_fDesiredSpeedOnPath(0.0f)
{
	s_xml.GetMandatory(node, "loops", m_loops);
	node->getAttr("endAccuracy", m_fEndAccuracy);

	s_xml.GetMandatory(node, "speed", m_fDesiredSpeedOnPath);
}

COPFollowPath::COPFollowPath(const COPFollowPath& rhs)
	: m_pTraceDirective(nullptr)
	, m_pPathFindDirective(nullptr)
	, m_refPathStartPoint()
	, m_pathFindToStart(rhs.m_pathFindToStart)
	, m_reversePath(rhs.m_reversePath)
	, m_startNearest(rhs.m_startNearest)
	, m_bUsePointList(rhs.m_bUsePointList)
	, m_bControlSpeed(rhs.m_bControlSpeed)
	, m_loops(rhs.m_loops)
	, m_loopCounter(0)
	, m_notMovingTimeMs(0)
	, m_returningToPath(false)
	, m_fEndAccuracy(rhs.m_fEndAccuracy)
	, m_fDesiredSpeedOnPath(rhs.m_fDesiredSpeedOnPath)
{
}

//====================================================================
// Reset
//====================================================================
void COPFollowPath::Reset(CPipeUser* pPipeUser)
{
	CCCPOINT(COPFollowPath_Reset);

	// (MATT) Apparently we hang onto the path start dummy {2009/02/17}
	delete m_pTraceDirective;
	m_pTraceDirective = 0;
	delete m_pPathFindDirective;
	m_pPathFindDirective = 0;
	m_loopCounter = 0;
	m_notMovingTimeMs = 0;
	m_returningToPath = false;
	m_fEndAccuracy = defaultTraceEndAccuracy;
	m_fDesiredSpeedOnPath = 0.0f;
	if (pPipeUser)
		pPipeUser->ClearPath("COPFollowPath::Reset m_Path");
}

//====================================================================
// Serialize
//====================================================================
void COPFollowPath::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPFollowPath");
	{
		ser.Value("m_pathFindToStart", m_pathFindToStart);
		ser.Value("m_reversePath", m_reversePath);
		ser.Value("m_startNearest", m_startNearest);
		ser.Value("m_loops", m_loops);
		ser.Value("m_loopCounter", m_loopCounter);
		ser.Value("m_TraceEndAccuracy", m_fEndAccuracy);
		ser.Value("m_fDesiredSpeedOnPath", m_fDesiredSpeedOnPath);
		ser.Value("m_notMovingTimeMs", m_notMovingTimeMs);
		ser.Value("m_returningToPath", m_returningToPath);
		ser.Value("m_bUsePointList", m_bUsePointList);
		ser.Value("m_bControlSpeed", m_bControlSpeed);

		m_refPathStartPoint.Serialize(ser, "m_refPathStartPoint");

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pTraceDirective->Serialize(ser);
				ser.EndGroup();
			}
			if (ser.BeginOptionalGroup("PathFindDirective", m_pPathFindDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pPathFindDirective->Serialize(ser);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if (ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				m_pTraceDirective->Serialize(ser);
				m_pTraceDirective->SetControlSpeed(m_bControlSpeed);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathFindDirective);
			if (ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathFindDirective = new COPPathFind("");
				m_pPathFindDirective->Serialize(ser);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}
}

//====================================================================
// Execute
//====================================================================
EGoalOpResult COPFollowPath::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CAIObject* pTarget = (CAIObject*)pPipeUser->GetAttentionTarget();
	CAISystem* pSystem = GetAISystem();

	// for a temporary, until all functionality have made.
	if (m_bUsePointList == true && !(m_pathFindToStart == false && m_reversePath == false && m_startNearest == false))
	{
		AIWarning("COPFollowPath:: bUsePointList only support false,false,false in first 3 parameters for %s", GetNameSafe(pPipeUser));
		return eGOR_FAILED;
	}

	CCCPOINT(COPFollowPath_Execute);

	// if we have a path, trace it. Once we get going (i.e. pathfind is finished, if we use it) we'll
	// always have a trace
	if (!m_pTraceDirective)
	{
		if (m_pPathFindDirective)
		{
			CCCPOINT(COPFollowPath_Execute_A);

			EGoalOpResult finishedPathFind = m_pPathFindDirective->Execute(pPipeUser);
			if (finishedPathFind != eGOR_IN_PROGRESS)
			{
				pPipeUser->m_State.fDesiredSpeed = m_fDesiredSpeedOnPath;
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				if (!m_bControlSpeed)
					m_pTraceDirective->SetControlSpeed(false);

				m_lastTime = pSystem->GetFrameStartTime();
			}
			else
				return eGOR_IN_PROGRESS;
		}
		else // no pathfind directive
		{
			//      m_pathFindToStart = pPipeUser->GetPathFindToStartOfPathToFollow(pathStartPos);
			if (m_pathFindToStart)
			{
				CCCPOINT(COPFollowPath_Execute_B);

				// Create the PathStartPoint if needed
				if (m_refPathStartPoint.IsNil() || !m_refPathStartPoint->GetAIObjectID())
					gAIEnv.pAIObjectManager->CreateDummyObject(m_refPathStartPoint, string(GetNameSafe(pPipeUser)) + "_PathStartPoint", CAIObject::STP_REFPOINT);

				Vec3 entryPos;
				if (!pPipeUser->GetPathEntryPoint(entryPos, m_reversePath, m_startNearest))
				{
					AIWarning("COPFollowPath::Unable to find path entry point for %s - check path is not marked as a road and that all AI has been generated and exported", GetNameSafe(pPipeUser));
					return eGOR_FAILED;
				}
				m_refPathStartPoint->SetPos(entryPos);
				m_pPathFindDirective = new COPPathFind("FollowPath", m_refPathStartPoint.GetAIObject());

				return eGOR_IN_PROGRESS;
			}
			else
			{
				CCCPOINT(COPFollowPath_Execute_C);

				if (m_bUsePointList == true)
				{
					bool pathOK = pPipeUser->UsePointListToFollow();
					if (!pathOK)
					{
						AIWarning("COPFollowPath::Execute Unable to use point list %s", GetNameSafe(pPipeUser));
						return eGOR_FAILED;
					}
				}
				else
				{
					bool pathOK = pPipeUser->UsePathToFollow(m_reversePath, m_startNearest, m_loops != 0);
					if (!pathOK)
					{
						AIWarning("COPFollowPath::Execute Unable to use follow path for %s - check path is not marked as a road and that all AI has been generated and exported", GetNameSafe(pPipeUser));
						return eGOR_FAILED;
					}
				}
				pPipeUser->m_State.fDesiredSpeed = m_fDesiredSpeedOnPath;
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				if (!m_bControlSpeed)
					m_pTraceDirective->SetControlSpeed(false);

				m_lastTime = pSystem->GetFrameStartTime();
			} // m_pathFindToStart
		}   // path find directive
	}
	AIAssert(m_pTraceDirective);
	EGoalOpResult done = m_pTraceDirective->Execute(pPipeUser);

	// HACK: The following code tries to put a lost or stuck agent back on track.
	// It works together with a piece of in ExecuteDry which tracks the speed relative
	// to the requested speed and if it drops dramatically for certain time, this code
	// will trigger and try to move the agent back on the path. [Mikko]

	int timeout = 700;
	if (pPipeUser->GetType() == AIOBJECT_VEHICLE)
		timeout = 7000;

	if (pPipeUser->GetSubType() == CAIObject::STP_2D_FLY)
		m_notMovingTimeMs = 0;

	if (m_notMovingTimeMs > timeout)
	{
		CCCPOINT(COPFollowPath_Execute_Stuck);

		// Stuck or lost, move to the nearest point on path.
		AIWarning("COPFollowPath::Entity %s has not been moving fast enough for %.1fs it might be stuck, find back to path.", GetNameSafe(pPipeUser), m_notMovingTimeMs * 0.001f);

		// Create the PathStartPoint if needed
		// (MATT) And if definately is, sometimes. But fix this code duplication. {2009/02/18}
		if (m_refPathStartPoint.IsNil() || !m_refPathStartPoint->GetAIObjectID())
			gAIEnv.pAIObjectManager->CreateDummyObject(m_refPathStartPoint, string(GetNameSafe(pPipeUser)) + "_PathStartPoint", CAIObject::STP_REFPOINT);

		Vec3 entryPos;
		if (!pPipeUser->GetPathEntryPoint(entryPos, m_reversePath, true))
		{
			AIWarning("COPFollowPath::Unable to find path entry point for %s - check path is not marked as a road and that all AI has been generated and exported", GetNameSafe(pPipeUser));
			return eGOR_FAILED;
		}
		m_refPathStartPoint->SetPos(entryPos);
		m_pPathFindDirective = new COPPathFind("FollowPath", m_refPathStartPoint.GetAIObject());
		delete m_pTraceDirective;
		m_pTraceDirective = 0;
		m_notMovingTimeMs = 0;
		m_returningToPath = true;
	}

	if (done != eGOR_IN_PROGRESS || !m_pTraceDirective)
	{
		if ((m_pathFindToStart || m_returningToPath) && m_pPathFindDirective)
		{
			CCCPOINT(COPFollowPath_Execute_D);

			pPipeUser->SetSignal(1, "OnPathFindAtStart", 0, 0, gAIEnv.SignalCRCs.m_nOnPathFindAtStart);

			delete m_pPathFindDirective;
			m_pPathFindDirective = 0;
			delete m_pTraceDirective;
			bool pathOK = pPipeUser->UsePathToFollow(m_reversePath, (m_startNearest || m_returningToPath), false);
			if (!pathOK)
			{
				AIWarning("COPFollowPath::Execute Unable to use follow path for %s - check path is not marked as a road and that all AI has been generated and exported", GetNameSafe(pPipeUser));
				return eGOR_FAILED;
			}
			pPipeUser->m_State.fDesiredSpeed = m_fDesiredSpeedOnPath;
			m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
			if (!m_bControlSpeed)
				m_pTraceDirective->SetControlSpeed(false);

			m_lastTime = pSystem->GetFrameStartTime();
			m_returningToPath = false;
			return eGOR_IN_PROGRESS;
		}
		else
		{
			if (m_loops != 0)
			{
				CCCPOINT(COPFollowPath_Execute_E);

				m_loopCounter++;
				if (m_loops != -1 && m_loopCounter >= m_loops)
				{
					Reset(pPipeUser);
					return eGOR_SUCCEEDED;
				}

				delete m_pTraceDirective;
				bool pathOK = pPipeUser->UsePathToFollow(m_reversePath, m_startNearest, m_loops != 0);
				if (!pathOK)
				{
					AIWarning("COPFollowPath::Execute Unable to use follow path for %s - check path is not marked as a road and that all AI has been generated and exported", GetNameSafe(pPipeUser));
					return eGOR_FAILED;
				}

				pPipeUser->m_State.fDesiredSpeed = m_fDesiredSpeedOnPath;

				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				if (!m_bControlSpeed)
					m_pTraceDirective->SetControlSpeed(false);
				m_lastTime = pSystem->GetFrameStartTime();
				// For seamless operation, update trace already.
				m_pTraceDirective->Execute(pPipeUser);
				return eGOR_IN_PROGRESS;
			}
		}

		Reset(pPipeUser);
		return eGOR_SUCCEEDED;
	}

	return eGOR_IN_PROGRESS;
}

//====================================================================
// ExecuteDry
//====================================================================
void COPFollowPath::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CAISystem* pSystem = GetAISystem();

	if (m_pTraceDirective)
	{
		CCCPOINT(COPFollowPath_ExecuteDry);

		m_pTraceDirective->ExecuteTrace(pPipeUser, false);

		// HACK: The following code together with some logic in the execute tries to keep track
		// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
		CTimeValue time(pSystem->GetFrameStartTime());
		int64 dt((time - m_lastTime).GetMilliSecondsAsInt64());

		float speed = pPipeUser->GetVelocity().GetLength();
		float desiredSpeed = pPipeUser->m_State.fDesiredSpeed;

		if (desiredSpeed > 0.0f)
		{
			float ratio = clamp_tpl(speed / desiredSpeed, 0.0f, 1.0f);
			if (ratio < 0.1f)
				m_notMovingTimeMs += (int)dt;
			else
				m_notMovingTimeMs -= (int)dt;

			if (m_notMovingTimeMs < 0)
				m_notMovingTimeMs = 0;
		}
		m_lastTime = time;
	}
}

//===================================================================
// COPBackoff
//===================================================================
COPBackoff::COPBackoff(float distance, float duration, int filter, float minDistance)
{
	m_fDistance = distance;
	m_fDuration = duration;
	m_iDirection = filter & (AI_MOVE_FORWARD | AI_MOVE_BACKWARD | AI_MOVE_LEFT | AI_MOVE_RIGHT | AI_MOVE_TOWARDS_GROUP | AI_MOVE_BACKLEFT | AI_MOVE_BACKRIGHT);
	m_fMinDistance = minDistance;
	if (m_iDirection == 0)
		m_iDirection = AI_MOVE_BACKWARD;

	//m_iAvailableDirections = m_iDirection;

	m_bUseLastOp = (filter & AILASTOPRES_USE) != 0;
	m_bLookForward = (filter & AI_LOOK_FORWARD) != 0;
	m_bUseTargetPosition = (filter & AI_BACKOFF_FROM_TARGET) != 0;
	m_bRandomOrder = (filter & AI_RANDOM_ORDER) != 0;
	m_bCheckSlopeDistance = (filter & AI_CHECK_SLOPE_DISTANCE) != 0;

	if (m_fDistance < 0.f)
	{
		m_fDistance = -m_fDistance;
	}
	m_pPathfindDirective = 0;
	m_pTraceDirective = 0;
	m_currentDirMask = AI_MOVE_BACKWARD;
	m_vBestFailedBackoffPos.zero();
	m_fMaxDistanceFound = 0.f;
	m_bTryingLessThanMinDistance = false;
	m_looseAttentionId = 0;
	ResetMoveDirections();
}

COPBackoff::COPBackoff(const XmlNodeRef& node) :
	m_fDistance(0.f),
	m_fDuration(0.f),
	m_fMinDistance(0.f),
	m_pPathfindDirective(0),
	m_pTraceDirective(0),
	m_currentDirMask(AI_MOVE_BACKWARD),
	m_vBestFailedBackoffPos(ZERO),
	m_fMaxDistanceFound(0.f),
	m_bTryingLessThanMinDistance(false),
	m_looseAttentionId(0),
	m_bUseLastOp(s_xml.GetBool(node, "useLastOp")),
	m_bLookForward(s_xml.GetBool(node, "lookForward")),
	m_bUseTargetPosition(s_xml.GetBool(node, "backOffFromTarget")),
	m_bRandomOrder(s_xml.GetBool(node, "randomOrder")),
	m_bCheckSlopeDistance(s_xml.GetBool(node, "checkSlopeDistance"))
{
	s_xml.GetMandatory(node, "distance", m_fDistance);
	if (m_fDistance < 0.f)
	{
		m_fDistance = -m_fDistance;
	}

	node->getAttr("maxDuration", m_fDuration);

	m_iDirection = (s_xml.GetBool(node, "moveForward") ? AI_MOVE_FORWARD : 0) |
	               (s_xml.GetBool(node, "moveBackward") ? AI_MOVE_BACKWARD : 0) |
	               (s_xml.GetBool(node, "moveLeft") ? AI_MOVE_LEFT : 0) |
	               (s_xml.GetBool(node, "moveRight") ? AI_MOVE_RIGHT : 0) |
	               (s_xml.GetBool(node, "moveBackLeft") ? AI_MOVE_BACKLEFT : 0) |
	               (s_xml.GetBool(node, "moveBackRight") ? AI_MOVE_BACKRIGHT : 0) |
	               (s_xml.GetBool(node, "moveTowardsGroup") ? AI_MOVE_TOWARDS_GROUP : 0);
	if (m_iDirection == 0)
	{
		m_iDirection = AI_MOVE_BACKWARD;
	}

	node->getAttr("minDistance", m_fMinDistance);

	ResetMoveDirections();
}

COPBackoff::COPBackoff(const COPBackoff& rhs)
	: m_bUseTargetPosition(rhs.m_bUseTargetPosition)
	, m_iDirection(rhs.m_iDirection)
	, m_iCurrentDirectionIndex(0)
	, m_fDistance(rhs.m_fDistance)
	, m_fDuration(rhs.m_fDuration)
	, m_bUseLastOp(rhs.m_bUseLastOp)
	, m_bLookForward(rhs.m_bLookForward)
	, m_moveStart(ZERO)
	, m_moveEnd(ZERO)
	, m_currentDirMask(AI_MOVE_BACKWARD)
	, m_fMinDistance(rhs.m_fMinDistance)
	, m_vBestFailedBackoffPos(ZERO)
	, m_fMaxDistanceFound(0.f)
	, m_bTryingLessThanMinDistance(false)
	, m_looseAttentionId(0)
	, m_bRandomOrder(rhs.m_bRandomOrder)
	, m_bCheckSlopeDistance(rhs.m_bCheckSlopeDistance)
	, m_pTraceDirective(nullptr)
	, m_pPathfindDirective(nullptr)
	, m_refBackoffPoint()
{
	ResetMoveDirections();
}

//===================================================================
// Reset
//===================================================================
void COPBackoff::Reset(CPipeUser* pPipeUser)
{
	ResetNavigation(pPipeUser);
	m_fMaxDistanceFound = 0;
	m_bTryingLessThanMinDistance = false;
	m_vBestFailedBackoffPos.zero();
	m_currentDirMask = AI_MOVE_BACKWARD;
	ResetMoveDirections();
}

//===================================================================
// SetMoveDirections
//===================================================================
void COPBackoff::ResetMoveDirections()
{
	m_iCurrentDirectionIndex = 0;
	m_MoveDirections.clear();

	int mask = (AI_MOVE_FORWARD | AI_MOVE_BACKWARD | AI_MOVE_LEFT | AI_MOVE_RIGHT | AI_MOVE_TOWARDS_GROUP | AI_MOVE_BACKLEFT | AI_MOVE_BACKRIGHT);
	mask &= m_iDirection;

	int currentdir = 1;
	while (currentdir <= mask)
	{
		if ((mask & currentdir) != 0)
			m_MoveDirections.push_back(currentdir);
		currentdir <<= 1;
	}
	if (m_bRandomOrder)
	{
		int size = m_MoveDirections.size();
		for (int i = 0; i < size; ++i)
		{
			int other = cry_random(0, size - 1);
			int temp = m_MoveDirections[i];
			m_MoveDirections[i] = m_MoveDirections[other];
			m_MoveDirections[other] = temp;
		}
	}
}

//===================================================================
// ResetNavigation
//===================================================================
void COPBackoff::ResetNavigation(CPipeUser* pPipeUser)
{
	if (m_pPathfindDirective)
		delete m_pPathfindDirective;
	m_pPathfindDirective = 0;
	if (m_pTraceDirective)
		delete m_pTraceDirective;
	m_pTraceDirective = 0;

	if (pPipeUser)
		pPipeUser->ClearPath("COPBackoff::Reset m_Path");

	if (m_refBackoffPoint.IsNil())
	{
		m_refBackoffPoint.Release();
		if (m_bLookForward && pPipeUser)
		{
			pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
		}
	}
	m_looseAttentionId = 0;
}

//===================================================================
// Execute
//===================================================================
void COPBackoff::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pPipeUser, false);
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPBackoff::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPBackoff_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_fLastUpdateTime = GetAISystem()->GetFrameStartTime();

	if (!m_pPathfindDirective)
	{
		CAIObject* pTarget = NULL;

		if (m_bUseLastOp)
		{
			pTarget = pPipeUser->m_refLastOpResult.GetAIObject();
			if (!pTarget)
			{
				Reset(pPipeUser);
				return eGOR_FAILED;
			}
		}
		else
		{
			pTarget = (CAIObject*)pPipeUser->GetAttentionTarget();
			if (!pTarget)
			{
				if (!(pTarget = pPipeUser->m_refLastOpResult.GetAIObject()))
				{
					Reset(pPipeUser);
					return eGOR_FAILED;
				}
			}
		}

		int currentDir = m_iCurrentDirectionIndex >= (int) m_MoveDirections.size() ? 0 : m_MoveDirections[m_iCurrentDirectionIndex];
		if (m_iCurrentDirectionIndex < (int)m_MoveDirections.size())
			m_iCurrentDirectionIndex++;

		if (currentDir == 0)
		{
			if (m_fMaxDistanceFound > 0.f)
			{
				// a path less than mindistance required has been found anyway
				CAIObject* pBackoffPoint = m_refBackoffPoint.GetAIObject();
				m_pPathfindDirective = new COPPathFind("Backoff", pBackoffPoint, std::numeric_limits<float>::max(), 0.0f, m_fDistance);
				pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;
				m_bTryingLessThanMinDistance = true;
				if (m_pPathfindDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS)
				{
					if (pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
					{
						pPipeUser->m_State.vMoveDir.zero();
						pPipeUser->SetSignal(1, "OnBackOffFailed", 0, 0, gAIEnv.SignalCRCs.m_nOnBackOffFailed);
						Reset(pPipeUser);
						return eGOR_FAILED;
					}
				}
			}
			else
			{
				pPipeUser->m_State.vMoveDir.zero();
				pPipeUser->SetSignal(1, "OnBackOffFailed", 0, 0, gAIEnv.SignalCRCs.m_nOnBackOffFailed);
				Reset(pPipeUser);
				return eGOR_FAILED;
			}
		}

		Vec3 mypos = pPipeUser->GetPhysicsPos();
		Vec3 tgpos = pTarget->GetPhysicsPos();

		// evil hack
		CAIActor* pTargetActor = pTarget->CastToCAIActor();
		if ((pTargetActor != NULL) && (pTarget->GetType() == AIOBJECT_VEHICLE))
		{
			tgpos += pTargetActor->m_State.vMoveDir * 10.f;
		}

		Vec3 vMoveDir = tgpos - mypos;
		// zero the z value because for human backoffs it seems that when the z values differ
		// by a large amount at close range the direction tracing doesn't work so well
		if (!pPipeUser->IsUsing3DNavigation())
			vMoveDir.z = 0.0f;

		Vec3 avoidPos;

		if (m_bUseTargetPosition)
			avoidPos = tgpos;
		else
			avoidPos = mypos;

		// This check assumes that the reason of the backoff
		// if to move away to specified direction away from a point to avoid.
		// If the agent is already far enough, stop immediately.
		if (Distance::Point_Point(avoidPos, mypos) > m_fDistance)
		{
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}

		switch (currentDir)
		{
		case AI_MOVE_FORWARD:
			break;
		case AI_MOVE_RIGHT:
			vMoveDir.Set(vMoveDir.y, -vMoveDir.x, 0.0f);
			break;
		case AI_MOVE_LEFT:
			vMoveDir.Set(-vMoveDir.y, vMoveDir.x, 0.0f);
			break;
		case AI_MOVE_BACKLEFT:
			vMoveDir = vMoveDir.GetRotated(Vec3Constants<float>::fVec3_OneZ, gf_PI / 4);
			break;
		case AI_MOVE_BACKRIGHT:
			vMoveDir = vMoveDir.GetRotated(Vec3Constants<float>::fVec3_OneZ, -gf_PI / 4);
			break;
		case AI_MOVE_TOWARDS_GROUP:
			{
				// Average direction from the target towards the group.
				vMoveDir.zero();
				int groupId = pPipeUser->GetGroupId();
				AIObjects::iterator it = GetAISystem()->m_mapGroups.find(groupId);
				AIObjects::iterator end = GetAISystem()->m_mapGroups.end();
				for (; it != end && it->first == groupId; ++it)
				{
					CAIObject* pObject = it->second.GetAIObject();
					if (!pObject || !pObject->IsEnabled())
						continue;
					if (pObject->GetType() != AIOBJECT_ACTOR)
						continue;
					vMoveDir += pObject->GetPos() - tgpos;
				}
				if (!pPipeUser->IsUsing3DNavigation())
					vMoveDir.z = 0.0f;
			}
			break;
		case AI_MOVE_BACKWARD:
		default://
			vMoveDir = -vMoveDir;
			break;
		}
		vMoveDir.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
		vMoveDir *= m_fDistance;
		Vec3 backoffPos = avoidPos + vMoveDir;

		m_moveStart = mypos;
		m_moveEnd = backoffPos;

		Vec3 offset(0, 0, 0.5f);
		GetAISystem()->AddDebugSphere(m_moveStart + offset, 0.1f, 0, 255, 255, 5);
		GetAISystem()->AddDebugSphere(backoffPos + offset, 0.2f, 0, 255, 255, 5);
		GetAISystem()->AddDebugLine(m_moveStart + offset, backoffPos + offset, 0, 255, 255, 5);

		// (MATT) Really crucial part! {2009/02/04}
		// create a dummy object to pathfind towards
		if (m_refBackoffPoint.IsNil())
		{
			gAIEnv.pAIObjectManager->CreateDummyObject(m_refBackoffPoint, string(GetNameSafe(pPipeUser)) + "_BackoffPoint");
			m_refBackoffPoint.GetAIObject()->SetPos(backoffPos);
		}

		if (m_bLookForward)
			m_looseAttentionId = pPipeUser->SetLooseAttentionTarget(m_refBackoffPoint);

		// start pathfinding
		CAIObject* pBackoffPoint = m_refBackoffPoint.GetAIObject();
		m_pPathfindDirective = new COPPathFind("Backoff", pBackoffPoint, std::numeric_limits<float>::max(), 0.0f, m_fDistance);
		pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;
		if (m_pPathfindDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS)
		{
			if (pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
			{
				ResetNavigation(pPipeUser);
				return eGOR_IN_PROGRESS;// check next direction
			}
		}
		return eGOR_IN_PROGRESS;
	} // !m_pPathfindDirective

	if (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND)
	{
		if (m_fMinDistance > 0)
		{
			float dist = Distance::Point_Point(pPipeUser->GetPos(), pPipeUser->m_OrigPath.GetLastPathPos());
			if (dist < m_fMinDistance)
			{
				if (m_fMaxDistanceFound < dist)
				{
					m_fMaxDistanceFound = dist;
					CAIObject* pBackoffPoint = m_refBackoffPoint.GetAIObject();
					m_vBestFailedBackoffPos = pBackoffPoint->GetPos();
				}
				ResetNavigation(pPipeUser);
				return eGOR_IN_PROGRESS;// check next direction
			}
		}
		if (!m_pTraceDirective)
		{
			m_pTraceDirective = new COPTrace(false, defaultTraceEndAccuracy);
			pPipeUser->m_Path.GetParams().precalculatedPath = true; // prevent path regeneration
			pPipeUser->SetSignal(0, "OnPathFound", NULL, 0, gAIEnv.SignalCRCs.m_nOnPathFound);
			m_fInitTime = GetAISystem()->GetFrameStartTime();
		}

		if (m_fDuration > 0 && (GetAISystem()->GetFrameStartTime() - m_fInitTime).GetSeconds() > m_fDuration)
		{
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}

		// keep tracing
		EGoalOpResult done = m_pTraceDirective->Execute(pPipeUser);
		if (done != eGOR_IN_PROGRESS)
		{
			Reset(pPipeUser);
			return done;
		}
		// If this goal gets reseted during m_pTraceDirective->Execute it means that
		// a smart object was used for navigation which inserts a goal pipe which
		// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
		// we should just report that this goal pipe isn't finished since it will be
		// reexecuted after finishing the inserted goal pipe
		if (!m_pTraceDirective)
			return eGOR_IN_PROGRESS;
	}
	else if (pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
	{
		if (m_bTryingLessThanMinDistance)
			pPipeUser->SetSignal(1, "OnBackOffFailed", 0, 0, gAIEnv.SignalCRCs.m_nOnBackOffFailed);

		//redo next time for next direction...
		if (m_bTryingLessThanMinDistance)
		{
			ResetNavigation(pPipeUser);
			return eGOR_IN_PROGRESS;
		}
		else
		{
			//... unless the less-than-min distance path has already been tried
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

	}
	else
	{
		m_pPathfindDirective->Execute(pPipeUser);
	}

	return eGOR_IN_PROGRESS;
}

void COPBackoff::Serialize(TSerialize ser)
{

	ser.BeginGroup("COPBackoff");
	{
		ser.Value("m_bUseTargetPosition", m_bUseTargetPosition);
		ser.Value("m_iDirection", m_iDirection);
		ser.Value("m_iCurrentDirectionIndex", m_iCurrentDirectionIndex);
		ser.Value("m_MoveDirections", m_MoveDirections);
		ser.Value("m_fDistance", m_fDistance);
		ser.Value("m_fDuration", m_fDuration);
		ser.Value("m_fInitTime", m_fInitTime);
		ser.Value("m_fLastUpdateTime", m_fLastUpdateTime);
		ser.Value("m_bUseLastOp", m_bUseLastOp);
		ser.Value("m_bLookForward", m_bLookForward);
		ser.Value("m_moveStart", m_moveStart);
		ser.Value("m_moveEnd", m_moveEnd);
		ser.Value("m_currentDirMask", m_currentDirMask);
		ser.Value("m_fMinDistance", m_fMinDistance);
		ser.Value("m_vBestFailedBackoffPos", m_vBestFailedBackoffPos);
		ser.Value("m_fMaxDistanceFound", m_fMaxDistanceFound);
		ser.Value("m_bTryingLessThanMinDistance", m_bTryingLessThanMinDistance);
		ser.Value("m_looseAttentionId", m_looseAttentionId);
		ser.Value("m_bRandomOrder", m_bRandomOrder);

		m_refBackoffPoint.Serialize(ser, "m_refBackoffPoint");
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
		ser.EndGroup();

	}
}

void COPBackoff::DebugDraw(CPipeUser* pPipeUser) const
{
	Vec3 dir(m_moveEnd - m_moveStart);
	dir.NormalizeSafe();
	CDebugDrawContext dc;
	ColorB color(255, 255, 255);
	dc->DrawLine(m_moveStart + Vec3(0, 0, 0.25f), color, m_moveEnd + Vec3(0, 0, 0.25f), color);
	dc->DrawCone(m_moveEnd + Vec3(0, 0, 0.25f), dir, 0.3f, 0.8f, color);
}

COPTimeout::COPTimeout(float fIntervalMin, float fIntervalMax) :
	m_fIntervalMin(fIntervalMin),
	m_fIntervalMax((fIntervalMax > 0.f) ? fIntervalMax : fIntervalMin)
{
	Reset(0);
}

COPTimeout::COPTimeout(const XmlNodeRef& node) :
	m_fIntervalMin(0.f),
	m_fIntervalMax(-1.f)
{
	if (!node->getAttr("interval", m_fIntervalMin))
	{
		s_xml.GetMandatory(node, "intervalMin", m_fIntervalMin);
	}

	node->getAttr("intervalMax", m_fIntervalMax);
	if (m_fIntervalMax <= 0.f)
	{
		m_fIntervalMax = m_fIntervalMin;
	}

	Reset(0);
}

void COPTimeout::Reset(CPipeUser* pPipeUser)
{
	m_actualIntervalMs = 0;
	m_startTime.SetValue(0);
}

void COPTimeout::Serialize(TSerialize ser)
{
	uint64 timeElapsed = 0;
	if (ser.IsWriting())
	{
		CTimeValue time = GetAISystem()->GetFrameStartTime();
		timeElapsed = (time - m_startTime).GetMilliSecondsAsInt64();
	}
	ser.Value("timeElapsed", timeElapsed);
	if (ser.IsReading())
	{
		CTimeValue time = GetAISystem()->GetFrameStartTime();
		m_startTime.SetMilliSeconds(time.GetMilliSecondsAsInt64() - timeElapsed);
	}
	ser.Value("m_actualIntervalMs", m_actualIntervalMs);
}

EGoalOpResult COPTimeout::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CTimeValue time = GetAISystem()->GetFrameStartTime();
	if (m_startTime.GetMilliSecondsAsInt64() <= 0)
	{
		m_actualIntervalMs = (int)(1000.0f * (m_fIntervalMin + ((m_fIntervalMax - m_fIntervalMin) * cry_random(0.0f, 1.0f))));
		m_startTime = time;
	}

	CTimeValue timeElapsed = time - m_startTime;
	if (timeElapsed.GetMilliSecondsAsInt64() > m_actualIntervalMs)
	{
		Reset(pPipeUser);
		return eGOR_DONE;
	}

	return eGOR_IN_PROGRESS;
}

COPStrafe::COPStrafe(float fDistanceStart, float fDistanceEnd, bool bStrafeWhileMoving) :
	m_fDistanceStart(fDistanceStart),
	m_fDistanceEnd(fDistanceEnd),
	m_bStrafeWhileMoving(bStrafeWhileMoving)
{
}

COPStrafe::COPStrafe(const XmlNodeRef& node) :
	m_fDistanceStart(0.f),
	m_fDistanceEnd(0.0f),
	m_bStrafeWhileMoving(s_xml.GetBool(node, "strafeWhileMoving"))
{
	if (!node->getAttr("distance", m_fDistanceStart))
	{
		node->getAttr("distanceStart", m_fDistanceStart);
	}

	if (!node->getAttr("distanceEnd", m_fDistanceEnd))
	{
		m_fDistanceEnd = m_fDistanceStart;
	}
}

EGoalOpResult COPStrafe::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
	{
		pPuppet->SetAllowedStrafeDistances(m_fDistanceStart, m_fDistanceEnd, m_bStrafeWhileMoving);
	}
	else
	{
		assert(!!!"Goalop 'Strafe' is applicable only to puppets.");
	}
	return eGOR_DONE;
}

COPFireCmd::COPFireCmd(EFireMode eFireMode, bool bUseLastOp, float fIntervalMin, float fIntervalMax) :
	m_eFireMode(eFireMode),
	m_bUseLastOp(bUseLastOp),
	m_fIntervalMin(fIntervalMin),
	m_fIntervalMax(fIntervalMax),
	m_actualIntervalMs(-1000)
{
	m_startTime.SetValue(0);
}

COPFireCmd::COPFireCmd(const XmlNodeRef& node) :
	m_eFireMode(FIREMODE_OFF),
	m_bUseLastOp(s_xml.GetBool(node, "useLastOp")),
	m_fIntervalMin(-1.f),
	m_fIntervalMax(-1.f),
	m_actualIntervalMs(-1000)
{
	s_xml.GetFireMode(node, "mode", m_eFireMode, CGoalOpXMLReader::MANDATORY);

	if (!node->getAttr("timeout", m_fIntervalMin))
	{
		node->getAttr("timeoutMin", m_fIntervalMin);
	}

	node->getAttr("timeoutMax", m_fIntervalMax);

	m_startTime.SetValue(0);
}

void COPFireCmd::Reset(CPipeUser* pPipeUser)
{
	m_actualIntervalMs = -1000;
	m_startTime.SetValue(0);
	pPipeUser->SetFireTarget(NILREF);
}

EGoalOpResult COPFireCmd::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPFireCmd_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CTimeValue time = GetAISystem()->GetFrameStartTime();
	// if this is a first time
	if (m_startTime.GetMilliSecondsAsInt64() <= 0)
	{
		pPipeUser->SetFireMode(m_eFireMode);
		pPipeUser->SetFireTarget(m_bUseLastOp ? pPipeUser->m_refLastOpResult : NILREF);

		m_startTime = time;

		if (m_fIntervalMin < 0.0f)
			m_actualIntervalMs = -1000;
		else if (m_fIntervalMax > 0.0f)
			m_actualIntervalMs = (int)(cry_random(m_fIntervalMin, m_fIntervalMax) * 1000.0f);
		else
			m_actualIntervalMs = (int)(m_fIntervalMin * 1000.0f);
	}

	CTimeValue timeElapsed = time - m_startTime;

	if ((m_actualIntervalMs < 0) || (m_actualIntervalMs < timeElapsed.GetMilliSecondsAsInt64()))
	{
		// stop firing if was timed
		if (m_actualIntervalMs > 0)
			pPipeUser->SetFireMode(FIREMODE_OFF);

		Reset(pPipeUser);
		return eGOR_DONE;
	}

	return eGOR_IN_PROGRESS;
}

COPBodyCmd::COPBodyCmd(EStance nBodyStance, bool bDelayed) :
	m_nBodyStance(nBodyStance),
	m_bDelayed(bDelayed)
{
}

COPBodyCmd::COPBodyCmd(const XmlNodeRef& node) :
	m_nBodyStance(STANCE_NULL),
	m_bDelayed(s_xml.GetBool(node, "delayed"))
{
	s_xml.GetStance(node, "id", m_nBodyStance, CGoalOpXMLReader::MANDATORY);
}

EGoalOpResult COPBodyCmd::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CPuppet* pPuppet = pPipeUser->CastToCPuppet();
	if (m_bDelayed)
	{
		if (pPuppet)
		{
			pPuppet->SetDelayedStance(m_nBodyStance);
		}
		else
		{
			assert(!!!"COPBodyCmd::Execute: Unable to set a delayed stance on a non-puppet pipe user.");
		}
	}
	else
	{
		pPipeUser->m_State.bodystate = m_nBodyStance;

		if (pPuppet)
		{
			pPuppet->SetDelayedStance(STANCE_NULL);
		}
	}

	return eGOR_DONE;
}

COPRunCmd::COPRunCmd(float fMaxUrgency, float fMinUrgency, float fScaleDownPathLength) :
	m_fMaxUrgency(ConvertUrgency(fMaxUrgency)),
	m_fMinUrgency(ConvertUrgency(fMinUrgency)),
	m_fScaleDownPathLength(fScaleDownPathLength)
{
}

COPRunCmd::COPRunCmd(const XmlNodeRef& node) :
	m_fMaxUrgency(0.f),
	m_fMinUrgency(0.f),
	m_fScaleDownPathLength(0.f)
{
	if (!s_xml.GetUrgency(node, "urgency", m_fMaxUrgency))
	{
		if (!s_xml.GetUrgency(node, "maxUrgency", m_fMaxUrgency))
		{
			s_xml.GetUrgency(node, "id", m_fMaxUrgency);
		}
	}

	if ((gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS) || (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2))
	{
		s_xml.GetUrgency(node, "minUrgency", m_fMinUrgency);
		node->getAttr("scaleDownPathLength", m_fScaleDownPathLength);
	}

	m_fMaxUrgency = ConvertUrgency(m_fMaxUrgency);
	m_fMinUrgency = ConvertUrgency(m_fMinUrgency);
}

float COPRunCmd::ConvertUrgency(float speed)
{
	// does NOT match CFlowNode_AIBase<TDerived, TBlocking>::SetSpeed
	if (speed < -4.5f)
		return -AISPEED_SPRINT;
	else if (speed < -3.5f)
		return -AISPEED_RUN;
	else if (speed < -2.5f)
		return -AISPEED_WALK;
	else if (speed < -1.5f)
		return -AISPEED_SLOW;
	else if (speed < -0.5f)
		return AISPEED_SLOW;
	else if (speed <= 0.5f)
		return AISPEED_WALK;
	else if (speed + 0.001f < AISPEED_SPRINT)
		return AISPEED_RUN;
	else
		return AISPEED_SPRINT;
}

EGoalOpResult COPRunCmd::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	pPipeUser->m_State.fMovementUrgency = m_fMaxUrgency;

	if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
	{
		if (m_fScaleDownPathLength > 0.00001f)
		{
			pPuppet->SetAdaptiveMovementUrgency(m_fMinUrgency, m_fMaxUrgency, m_fScaleDownPathLength);
		}
		else
		{
			pPuppet->SetAdaptiveMovementUrgency(0.f, 0.f, 0.f);
		}
	}

	return eGOR_DONE;
}

// todo luc fixme
COPLookAt::COPLookAt(float fStartAngle, float fEndAngle, int mode, bool bBodyTurn, bool bUseLastOp)
	: m_fStartAngle(fStartAngle)
	, m_fEndAngle(fEndAngle)
	, m_fLastDot(0.0f)
	, m_fAngleThresholdCos(0.98f)
	, m_eLookStyle(bBodyTurn ? LOOKSTYLE_SOFT : LOOKSTYLE_SOFT_NOLOWER)
	, m_bUseLastOp(bUseLastOp)
	, m_bContinuous((mode & AI_LOOKAT_CONTINUOUS) != 0)
	, m_bInitialized(false)
	, m_bUseBodyDir((mode & AI_LOOKAT_USE_BODYDIR) != 0)
	, m_bYawOnly(false)
{
}

COPLookAt::COPLookAt(const XmlNodeRef& node)
	: m_fStartAngle(0.f)
	, m_fEndAngle(0.f)
	, m_fLastDot(0.0f)
	, m_fAngleThresholdCos(0.98f)
	, m_eLookStyle(s_xml.GetBool(node, "bodyTurn", true) ? LOOKSTYLE_SOFT : LOOKSTYLE_SOFT_NOLOWER)
	, m_bUseLastOp(s_xml.GetBool(node, "useLastOp"))
	, m_bContinuous(s_xml.GetBool(node, "continuous"))
	, m_bInitialized(false)
	, m_bUseBodyDir(s_xml.GetBool(node, "useBodyDir"))
	, m_bYawOnly(s_xml.GetBool(node, "yawOnly"))
{
	node->getAttr("startAngle", m_fStartAngle);
	node->getAttr("endAngle", m_fEndAngle);

	float fAngleThreshold;
	if (node->getAttr("angleThreshold", fAngleThreshold))
		m_fAngleThresholdCos = cosf(DEG2RAD(fAngleThreshold));
}

void COPLookAt::ResetLooseTarget(CPipeUser* pPipeUser, bool bForceReset)
{
	if (bForceReset || !m_bContinuous)
	{
		pPipeUser->SetLookStyle(LOOKSTYLE_DEFAULT);
		pPipeUser->SetLooseAttentionTarget(NILREF);
	}
}

void COPLookAt::Reset(CPipeUser* pPipeUser)
{
	ResetLooseTarget(pPipeUser);
	m_bInitialized = false;
}

EGoalOpResult COPLookAt::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPLookAt_Execute)
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_fStartAngle < -450.f) // stop looking at - disable looseAttentionTarget
	{
		ResetLooseTarget(pPipeUser, true);
		return eGOR_SUCCEEDED;
	}

	// first time
	if (!m_bInitialized)
	{
		m_bInitialized = true;

		m_startTime = GetAISystem()->GetFrameStartTime();

		if (!m_bUseLastOp && !m_fStartAngle && !m_fEndAngle)
		{
			return eGOR_SUCCEEDED;
		}

		pPipeUser->SetLookStyle(m_eLookStyle);

		if (!m_fStartAngle && !m_fEndAngle)
		{
			CAIObject* pLastOp = pPipeUser->m_refLastOpResult.GetAIObject();
			if (!pLastOp)
			{
				Reset(pPipeUser);
				return eGOR_FAILED;
			}

			pPipeUser->SetLooseAttentionTarget(GetWeakRef(pLastOp));

			if (m_bContinuous) // will keep the look at target after this goalop ends
			{
				Reset(pPipeUser);// Luc 1-aug-07: this reset was missing and it broke non continuous lookat with looping goalpipes (never finishing in the second loop)
				return eGOR_SUCCEEDED;
			} // otherwise, wait for orienting like the loose att target
		}
		else  // if (!m_fStartAngle && !m_fEndAngle)
		{
			Vec3 vMoveDir = pPipeUser->GetMoveDir();
			vMoveDir.z = 0;

			float fActualAngle = cry_random(m_fStartAngle, m_fEndAngle);
			Matrix33 m = Matrix33::CreateRotationZ(DEG2RAD(fActualAngle));
			vMoveDir = m * vMoveDir;

			pPipeUser->SetLooseAttentionTarget(pPipeUser->GetPos() + 100.f * vMoveDir);
		}
	}
	else  // keep on looking, orient like the loose att target
	{
		//m_bInitialized

		if (m_bContinuous)
			return eGOR_SUCCEEDED;

		// Time out if angle threshold isn't reached for some time (to avoid deadlocks)
		float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
		if (elapsedTime > 8.0f)
		{
			AIWarning("LookAt goal op timed out. Didn't reach wanted angle threshold for %f seconds.", elapsedTime);
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}

		Vec3 vLookAtPos = pPipeUser->GetLooseAttentionPos();
		if (vLookAtPos.IsZero(1.f))
		{
			if (IAIObject* pAttTarget = pPipeUser->GetAttentionTarget())
			{
				vLookAtPos = pAttTarget->GetPos();
			}
		}

		Vec3 vToLookTarget = vLookAtPos - pPipeUser->GetPos();
		Vec3 vToLookTargetDir = vToLookTarget;
		Vec3 vMyDir = m_bUseBodyDir ? pPipeUser->GetBodyDir() : pPipeUser->GetViewDir();
		const bool b2D = !pPipeUser->GetMovementAbility().b3DMove;
		const bool yawOnly = (m_bYawOnly) || (b2D && m_bUseBodyDir);
		if (yawOnly)
		{
			vMyDir.z = 0;
			vMyDir.NormalizeSafe();
			vToLookTargetDir.z = 0;
		}
		vToLookTargetDir.NormalizeSafe();

		float f = vMyDir.Dot(vToLookTargetDir);
		if (f > m_fAngleThresholdCos)
		{
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}

		if (b2D)
		{
			vToLookTarget.z = 0;
		}

		// To avoid getting stuck
		if (vToLookTarget.len() < 0.5f)
		{
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}
	}
	return eGOR_IN_PROGRESS;
}

//
//----------------------------------------------------------------------------------------------------------
void COPLookAt::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPLookAt");
	{
		ser.Value("m_fStartAngle", m_fStartAngle);
		ser.Value("m_fEndAngle", m_fEndAngle);
		ser.Value("m_fLastDot", m_fLastDot);
		ser.Value("m_bUseLastOp", m_bUseLastOp);
		ser.Value("m_bContinuous", m_bContinuous);
		ser.Value("m_bUseBodyDir", m_bUseBodyDir);
		ser.Value("m_bInitialized", m_bInitialized);

		ser.EndGroup();
	}
}

//
//----------------------------------------------------------------------------------------------------------
COPLookAround::COPLookAround(float lookAtRange, float scanIntervalRange, float intervalMin, float intervalMax, bool bBodyTurn, bool breakOnLiveTarget, bool bUseLastOp, bool checkForObstacles)
	: m_fLastDot(0.0f)
	, m_fLookAroundRange(lookAtRange)
	, m_fIntervalMin(intervalMin)
	, m_fIntervalMax(intervalMax)
	, m_fScanIntervalRange(scanIntervalRange)
	, m_scanTimeOutMs(0)
	, m_timeOutMs(0)
	, m_eLookStyle(LOOKSTYLE_DEFAULT)
	, m_breakOnLiveTarget(breakOnLiveTarget)
	, m_useLastOp(bUseLastOp)
	, m_lookAngle(0.0f)
	, m_lookZOffset(0.0f)
	, m_lastLookAngle(0.0f)
	, m_lastLookZOffset(0.0f)
	, m_looseAttentionId(0)
	, m_bInitialized(false)
	, m_initialDir(1, 0, 0)
	, m_checkForObstacles(checkForObstacles)
{
	if (m_fIntervalMax < m_fIntervalMin)
		m_fIntervalMax = m_fIntervalMin;

	RandomizeScanTimeout();
	RandomizeTimeout();

	m_eLookStyle = (bBodyTurn ? LOOKSTYLE_SOFT : LOOKSTYLE_SOFT_NOLOWER);
}

COPLookAround::COPLookAround(const XmlNodeRef& node)
	: m_fLastDot(0.0f)
	, m_fLookAroundRange(0.f)
	, m_fIntervalMin(-1.f)
	, m_fIntervalMax(-1.f)
	, m_fScanIntervalRange(-1.f)
	, m_scanTimeOutMs(0)
	, m_timeOutMs(0)
	, m_eLookStyle(s_xml.GetBool(node, "bodyTurn", true) ? LOOKSTYLE_SOFT : LOOKSTYLE_SOFT_NOLOWER)
	, m_breakOnLiveTarget(s_xml.GetBool(node, "breakOnLiveTarget"))
	, m_useLastOp(s_xml.GetBool(node, "useLastOp"))
	, m_lookAngle(0.f)
	, m_lookZOffset(0.f)
	, m_lastLookAngle(0.f)
	, m_lastLookZOffset(0.f)
	, m_looseAttentionId(0)
	, m_bInitialized(false)
	, m_initialDir(1.f, 0.f, 0.f)
	, m_checkForObstacles(false)
{
	s_xml.GetMandatory(node, "lookAroundRange", m_fLookAroundRange);
	node->getAttr("scanRange", m_fScanIntervalRange);

	if (node->getAttr("interval", m_fIntervalMin))
	{
		m_fIntervalMax = m_fIntervalMin;
	}
	node->getAttr("intervalMin", m_fIntervalMin);
	node->getAttr("intervalMax", m_fIntervalMax);
	if (m_fIntervalMax < m_fIntervalMin)
	{
		m_fIntervalMax = m_fIntervalMin;
	}

	RandomizeScanTimeout();
	RandomizeTimeout();

	node->getAttr("checkForObstacles", m_checkForObstacles);
}

void COPLookAround::UpdateLookAtTarget(CPipeUser* pPipeUser)
{
	m_lastLookAngle = m_lookAngle;
	m_lastLookZOffset = m_lookZOffset;

	if (!m_checkForObstacles)
	{
		SetRandomLookDir();
	}
	else
	{
		const Vec3& opPos = pPipeUser->GetPos();
		Vec3 dir;

		uint32 limitRays = 4;
		for (uint32 i = 0; i < limitRays; ++i)
		{
			SetRandomLookDir();
			dir = GetLookAtDir(pPipeUser, m_lookAngle, m_lookZOffset);
			// TODO : Defer this ray cast
			if (GetAISystem()->CheckPointsVisibility(opPos, opPos + dir, 5.0f))
				break;
		}
	}
}

void COPLookAround::SetRandomLookDir()
{
	// Random angle within range.
	float range(gf_PI);
	if (m_fLookAroundRange > 0)
		range = DEG2RAD(m_fLookAroundRange);
	range = clamp_tpl(range, 0.0f, gf_PI);
	m_lookAngle = cry_random(-1.0f, 1.0f) * range;
	m_lookZOffset = cry_random(-3.0f, 0.3f);  // Look more down then up
}

Vec3 COPLookAround::GetLookAtDir(CPipeUser* pPipeUser, float angle, float dz) const
{
	Vec3 forward = m_initialDir;
	if (!pPipeUser->GetState().vMoveDir.IsZero())
		forward = pPipeUser->GetState().vMoveDir;
	if (m_useLastOp)
	{
		CAIObject* pLastOpResult = pPipeUser->m_refLastOpResult.GetAIObject();
		if (pLastOpResult)
			forward = (pLastOpResult->GetPos() - pPipeUser->GetPos()).GetNormalizedSafe();
	}
	forward.z = 0.0f;
	forward.NormalizeSafe();
	Vec3 right(forward.y, -forward.x, 0.0f);

	const float lookAtDist = 20.0f;
	float dx = (float)sin_tpl(angle) * lookAtDist;
	float dy = (float)cos_tpl(angle) * lookAtDist;

	return right * dx + forward * dy + Vec3(0, 0, dz);
}

EGoalOpResult COPLookAround::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!m_bInitialized)
	{
		m_initialDir = pPipeUser->GetEntityDir();
		m_bInitialized = true;
	}

	if (!m_looseAttentionId)
	{
		m_fLastDot = 10.0f;

		UpdateLookAtTarget(pPipeUser);
		Vec3 dir = GetLookAtDir(pPipeUser, m_lookAngle, m_lookZOffset);
		Vec3 pos(pPipeUser->GetPos() + dir);
		m_looseAttentionId = pPipeUser->SetLooseAttentionTarget(pos);
		m_startTime = m_scanStartTime = GetAISystem()->GetFrameStartTime();

		pPipeUser->SetLookStyle(m_eLookStyle);
	}
	else
	{
		int id = pPipeUser->GetLooseAttentionId();
		if (id && id != m_looseAttentionId)
		{
			//something else is using the operand loose target; aborting
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		if (m_breakOnLiveTarget && pPipeUser->GetAttentionTarget() && pPipeUser->GetAttentionTarget()->IsAgent())
		{
			pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
			m_looseAttentionId = 0;
			Reset(pPipeUser);
			return eGOR_SUCCEEDED;
		}

		// (MATT) Test for possible div by 0. Unpretty but should work. {2009/04/28}
		if (pPipeUser->GetAttentionTarget() && (pPipeUser->GetAttentionTarget()->GetPos().GetDistance(pPipeUser->GetPos()) < 0.0001f))
			return eGOR_FAILED;

		ExecuteDry(pPipeUser);

		if (m_timeOutMs < 0)
		{
			// If no time out is specified, we bail out once the target is reached.
			const Vec3& opPos = pPipeUser->GetPos();
			const Vec3& opViewDir = pPipeUser->GetViewDir();

			Vec3 dirToTarget = pPipeUser->GetLooseAttentionPos() - opPos;
			dirToTarget.NormalizeSafe();

			float f = opViewDir.Dot(dirToTarget);
			if (f > 0.99f || fabsf(f - m_fLastDot) < 0.001f)
			{
				Reset(pPipeUser);

				return eGOR_SUCCEEDED;
			}
			else
				m_fLastDot = f;
		}
		else
		{
			// If time out is specified, keep looking around until the time out finishes.
			int64 elapsed = (GetAISystem()->GetFrameStartTime() - m_startTime).GetMilliSecondsAsInt64();

			if (elapsed > m_timeOutMs)
			{
				Reset(pPipeUser);

				return eGOR_SUCCEEDED;
			}
		}
	}

	return eGOR_IN_PROGRESS;
}

void COPLookAround::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (!m_bInitialized)
		return;

	CTimeValue now(GetAISystem()->GetFrameStartTime());
	int64 scanElapsedMs = (now - m_scanStartTime).GetMilliSecondsAsInt64();

	if (pPipeUser->GetAttentionTarget())
	{
		// Interpolate the initial dir towards the current attention target slowly.
		Vec3 reqDir = pPipeUser->GetAttentionTarget()->GetPos() - pPipeUser->GetPos();
		reqDir.Normalize();

		const float maxRatePerSec = DEG2RAD(25.0f);
		const float maxRate = maxRatePerSec * GetAISystem()->GetFrameDeltaTime();
		const float thr = cosf(maxRate);

		float cosAngle = m_initialDir.Dot(reqDir);
		if (cosAngle > thr)
		{
			m_initialDir = reqDir;
		}
		else
		{
			float angle = acos_tpl(cosAngle);
			float t = 0.0f;
			if (angle != 0.0f)
				t = maxRate / angle;
			Quat curView;
			curView.SetRotationVDir(m_initialDir);
			Quat reqView;
			reqView.SetRotationVDir(reqDir);
			Quat view;
			view.SetSlerp(curView, reqView, t);
			m_initialDir = view * FORWARD_DIRECTION;
		}
	}

	// Smooth transition from last value to new value if scanning, otherwise just as fast as possible.
	float t = 1.0f;
	if (m_scanTimeOutMs > 0)
		t = (1.0f - cosf(clamp_tpl(static_cast<float>(scanElapsedMs) / static_cast<float>(m_scanTimeOutMs), 0.0f, 1.0f) * gf_PI)) * 0.5f;

	Vec3 dir = GetLookAtDir(pPipeUser, m_lastLookAngle + (m_lookAngle - m_lastLookAngle) * t, m_lastLookZOffset + (m_lookZOffset - m_lastLookZOffset) * t);
	Vec3 pos = pPipeUser->GetPos() + dir;

	m_looseAttentionId = pPipeUser->SetLooseAttentionTarget(pos);

	// Once one sweep is finished, start another.
	if (scanElapsedMs > m_scanTimeOutMs)
	{
		UpdateLookAtTarget(pPipeUser);

		m_scanStartTime = now;
		RandomizeScanTimeout();
	}
}

void COPLookAround::Reset(CPipeUser* pPipeUser)
{
	m_bInitialized = false;
	if (pPipeUser)
	{
		pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
		m_looseAttentionId = 0;
	}

	RandomizeScanTimeout();
	RandomizeTimeout();

	m_lookAngle = 0.0f;
	m_lookZOffset = 0.0f;
	m_lastLookAngle = 0.0f;
	m_lastLookZOffset = 0.0f;

	m_fLastDot = 0;
	//	m_fTimeOut = 0;
	//m_fScanTimeOut = 0;
	if (pPipeUser)
		pPipeUser->SetLookStyle(LOOKSTYLE_DEFAULT);
}

//===================================================================
// DebugDraw
//===================================================================
void COPLookAround::DebugDraw(CPipeUser* pPipeUser) const
{
	int64 scanElapsed = (GetAISystem()->GetFrameStartTime() - m_scanStartTime).GetMilliSecondsAsInt64();

	float t(1.0f);
	if (m_scanTimeOutMs > 0)
		t = (1.0f - cosf(clamp_tpl(scanElapsed / (m_scanTimeOutMs * 0.001f), 0.0f, 1.0f) * gf_PI)) / 2.0f;
	Vec3 dir = GetLookAtDir(pPipeUser, m_lastLookAngle + (m_lookAngle - m_lastLookAngle) * t, m_lastLookZOffset + (m_lookZOffset - m_lastLookZOffset) * t);

	CDebugDrawContext dc;

	Vec3 pos = pPipeUser->GetPos() - Vec3(0, 0, 1);

	// Initial dir
	dc->DrawLine(pos, ColorB(255, 255, 255), pos + m_initialDir * 3.0f, ColorB(255, 255, 255, 128));

	// Current dir
	dc->DrawLine(pos, ColorB(255, 0, 0), pos + dir * 3.0f, ColorB(255, 0, 0, 128), 3.0f);
}

void COPLookAround::Serialize(TSerialize ser)
{

	ser.BeginGroup("COPLookAround");
	{
		ser.Value("m_fLastDot", m_fLastDot);
		ser.Value("m_fLookAroundRange", m_fLookAroundRange);
		ser.Value("m_fIntervalMin", m_fIntervalMin);
		ser.Value("m_fIntervalMax", m_fIntervalMax);
		ser.Value("m_fTimeOutMs", m_timeOutMs);
		ser.Value("m_fScanIntervalRange", m_fScanIntervalRange);
		ser.Value("m_scanTimeOutMs", m_scanTimeOutMs);
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_scanStartTime", m_scanStartTime);
		ser.Value("m_breakOnLiveTarget", m_breakOnLiveTarget);
		ser.Value("m_useLastOp", m_useLastOp);
		ser.Value("m_lookAngle", m_lookAngle);
		ser.Value("m_lookZOffset", m_lookZOffset);
		ser.Value("m_lastLookAngle", m_lastLookAngle);
		ser.Value("m_lastLookZOffset", m_lastLookZOffset);
		ser.Value("m_looseAttentionId", m_looseAttentionId);

		ser.EndGroup();
	}

}
//
//-------------------------------------------------------------------------------------------------------------------------------
COPPathFind::COPPathFind(
  const char* szTargetName,
  CAIObject* pTarget,
  float fEndTolerance,
  float fEndDistance,
  float fDirectionOnlyDistance) :

	m_sTargetName(szTargetName),
	m_refTarget(GetWeakRef(pTarget)),
	m_fDirectionOnlyDistance(fDirectionOnlyDistance),
	m_fEndTolerance(fEndTolerance),
	m_fEndDistance(fEndDistance),
	m_vTargetPos(ZERO),
	m_vTargetOffset(ZERO),
	m_nForceTargetBuildingID(-1),
	m_bWaitingForResult(false)
{
}

//
//-------------------------------------------------------------------------------------------------------------------------------
COPPathFind::COPPathFind(const XmlNodeRef& node) :
	m_sTargetName(s_xml.GetMandatoryString(node, "target")),
	m_refTarget(m_sTargetName.empty() ? NILREF : GetWeakRef(gAIEnv.pAIObjectManager->GetAIObjectByName(m_sTargetName.c_str()))),
	m_fDirectionOnlyDistance(0.f),
	m_fEndTolerance(0.f),
	m_fEndDistance(0.f),
	m_vTargetPos(ZERO),
	m_vTargetOffset(ZERO),
	m_nForceTargetBuildingID(-1),
	m_bWaitingForResult(false)
{
	node->getAttr("directionOnlyDistance", m_fDirectionOnlyDistance);
}

//
//-------------------------------------------------------------------------------------------------------------------------------
void COPPathFind::Reset(CPipeUser* pPipeUser)
{
	CCCPOINT(COPPathFind_Reset);

	m_bWaitingForResult = false;
	m_nForceTargetBuildingID = -1;
	// (MATT) Note that apparently we don't reset the target here {2009/02/17}
}

//
//-------------------------------------------------------------------------------------------------------------------------------
EGoalOpResult COPPathFind::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPPathFind_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!m_bWaitingForResult)
	{
		CAIObject* pTarget = 0;

		if (!m_sTargetName.empty())
		{
			pTarget = pPipeUser->GetSpecialAIObject(m_sTargetName.c_str());
		}

		if (!pTarget)
		{
			pTarget = m_refTarget.GetAIObject();

			if (!pTarget)
			{
				pTarget = pPipeUser->m_refLastOpResult.GetAIObject();

				if (!pTarget)
				{
					pPipeUser->SetSignal(0, "OnNoPathFound", 0, 0, gAIEnv.SignalCRCs.m_nOnNoPathFound);
					pPipeUser->m_nPathDecision = PATHFINDER_NOPATH;
					Reset(pPipeUser);
					return eGOR_FAILED;
				}
			}
		}

		CCCPOINT(COPPathFind_Execute_RequestPath);

		m_vTargetPos = pTarget->GetPhysicsPos() + m_vTargetOffset;

		if (m_fDirectionOnlyDistance > 0.f)
		{
			pPipeUser->RequestPathInDirection(m_vTargetPos, m_fDirectionOnlyDistance, NILREF, m_fEndDistance);
		}
		else
		{
			pPipeUser->RequestPathTo(m_vTargetPos, pTarget->GetMoveDir(),
			                         pTarget->GetSubType() != CAIObject::STP_FORMATION,
			                         m_nForceTargetBuildingID, m_fEndTolerance, m_fEndDistance, pTarget);
		}

		m_bWaitingForResult = true;
	}

	switch (pPipeUser->m_nPathDecision)
	{
	case PATHFINDER_STILLFINDING:
		return eGOR_IN_PROGRESS;

	case PATHFINDER_NOPATH:
		pPipeUser->SetSignal(0, "OnNoPathFound", 0, 0, gAIEnv.SignalCRCs.m_nOnNoPathFound);
		Reset(pPipeUser);
		return eGOR_FAILED;

	default:
		if (pPipeUser->m_Path.GetPath().empty())
		{
			pPipeUser->SetSignal(0, "OnNoPathFound", 0, 0, gAIEnv.SignalCRCs.m_nOnNoPathFound);
		}

		Reset(pPipeUser);
		return eGOR_SUCCEEDED;
	}
}

void COPPathFind::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPPathFind");
	{
		m_refTarget.Serialize(ser, "m_refTarget");
		ser.Value("m_sTargetName", m_sTargetName);
		ser.Value("m_fDirectionOnlyDistance", m_fDirectionOnlyDistance);
		ser.Value("m_fEndTolerance", m_fEndTolerance);
		ser.Value("m_fEndDistance", m_fEndDistance);
		ser.Value("m_vTargetPos", m_vTargetPos);
		ser.Value("m_vTargetOffset", m_vTargetOffset);
		ser.Value("m_nForceTargetBuildingID", m_nForceTargetBuildingID);

		ser.Value("m_bWaitingForResult", m_bWaitingForResult);

		ser.EndGroup();
	}
}

COPLocate::COPLocate(const char* szName, unsigned short int nObjectType, float fRange) :
	m_sName(szName), m_nObjectType(nObjectType), m_fRange(fRange), m_bWarnIfFailed(true)
{
}

COPLocate::COPLocate(const XmlNodeRef& node) :
	m_sName(node->getAttr("name")),
	m_nObjectType(AIOBJECT_DUMMY),
	m_fRange(0.f),
	m_bWarnIfFailed(s_xml.GetBool(node, "warnIfFailed", true))
{
	if (m_sName.empty())
	{
		s_xml.GetAIObjectType(node, "type", m_nObjectType, CGoalOpXMLReader::MANDATORY);
	}

	node->getAttr("range", m_fRange);
}

EGoalOpResult COPLocate::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPLocate_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_sName.empty())
	{
		IAIObject* pAIObject = GetAISystem()->GetNearestObjectOfTypeInRange(
		  pPipeUser->GetPos(), m_nObjectType, 0, (m_fRange > 0.001f) ? m_fRange : 50.f);

		pPipeUser->SetLastOpResult(GetWeakRef(static_cast<CAIObject*>(pAIObject)));
		return pAIObject ? eGOR_SUCCEEDED : eGOR_FAILED;
	}

	CAIObject* pAIObject = pPipeUser->GetSpecialAIObject(m_sName, m_fRange);

	// don't check the range if m_fRange doesn't really represent a range :-/
	if ((pAIObject != NULL) && (m_fRange > 0.001f) && (strcmp(m_sName.c_str(), "formation_id") != 0))
	{
		// check with m_fRange optional parameter
		if ((pPipeUser->GetPos() - pAIObject->GetPos()).GetLengthSquared() > m_fRange * m_fRange)
		{
			pAIObject = 0;
		}
	}

	// always set lastOpResult even if pAIObject is NULL!!!
	pPipeUser->SetLastOpResult(GetWeakRef(pAIObject));

	CCCPOINT(COPLocate_Execute_A);
	if (!pAIObject)
	{
		if (m_bWarnIfFailed)
		{
			if (m_sName == "formation")
				AIWarning("No free formation point for %s", pPipeUser->GetName());
			else
				gEnv->pLog->LogError("Failed to locate '%s'", m_sName.c_str());
		}
		return eGOR_FAILED;
	}

	return eGOR_SUCCEEDED;
}

EGoalOpResult COPIgnoreAll::Execute(CPipeUser* pPipeUser)
{
	pPipeUser->MakeIgnorant(m_bIgnoreAll);
	return eGOR_DONE;
}

COPSignal::COPSignal(const XmlNodeRef& node) :
	m_nSignalID(1),
	m_sSignal(node->getAttr("name")),
	m_cFilter(SIGNALFILTER_SENDER),
	m_bSent(false),
	m_iDataValue(0)
{
	node->getAttr("id", m_nSignalID);
	s_xml.GetSignalFilter(node, "filter", m_cFilter);
	node->getAttr("data", m_iDataValue);
}

EGoalOpResult COPSignal::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPSignal_Execute);

	if (m_bSent)
	{
		m_bSent = false;
		return eGOR_DONE;
	}

	AISignalExtraData* pData = new AISignalExtraData;
	pData->iValue = m_iDataValue;

	switch (m_cFilter)
	{
	case SIGNALFILTER_SENDER:
		pPipeUser->SetSignal(m_nSignalID, m_sSignal.c_str(), pPipeUser->GetEntity(), pData);
		m_bSent = true;
		return eGOR_IN_PROGRESS;

	case SIGNALFILTER_LASTOP:
		{
			CAIObject* pLastOpResult = pPipeUser->m_refLastOpResult.GetAIObject();
			if (pLastOpResult)
			{
				CAIActor* pOperandActor = pLastOpResult->CastToCAIActor();
				if (pOperandActor)
					pOperandActor->SetSignal(m_nSignalID, m_sSignal.c_str(), pPipeUser->GetEntity(), pData);
			}
			break;
		}

	default:
		// signal to species, group or anyone within comm range
		GetAISystem()->SendSignal(m_cFilter, m_nSignalID, m_sSignal.c_str(), pPipeUser, pData);
	}

	m_bSent = true;
	return eGOR_IN_PROGRESS;
}

COPScript::COPScript(const string& scriptCode)
{
	if (!scriptCode.empty())
	{
		m_scriptCode = scriptCode;
	}
	else
	{
		AIError("Goalop Script: Given empty string for Lua code");
	}
}

COPScript::COPScript(const XmlNodeRef& node)
{
	string sCode = node->getAttr("code");
	if (!sCode.empty())
	{
		m_scriptCode = sCode;
	}
	else
	{
		AIError("Goalop '%s': Given empty string for Lua code", node->getTag());
	}

}

EGoalOpResult COPScript::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(CCOPScript_Execute);

	if (!m_scriptCode)
		return eGOR_FAILED;

	bool result = false;
	IScriptSystem* scriptSystem = gEnv->pScriptSystem;

	const char* bufferDesc = "Script GoalOp";

	SmartScriptFunction scriptPtr(scriptSystem,
	                              scriptSystem->CompileBuffer(m_scriptCode.c_str(), m_scriptCode.length(), bufferDesc));

	if (!scriptPtr)
	{
		AIError("Goalop Script: Failed to compile Lua code when executing goalop: %s", m_scriptCode.c_str());
		return eGOR_FAILED;
	}

	// TODO(Márcio): Set the function environment here, instead of setting globals
	if (IEntity* entity = pPipeUser->GetEntity())
	{
		scriptSystem->SetGlobalValue("entity", entity->GetScriptTable());

		if (Script::CallReturn(gEnv->pScriptSystem, scriptPtr, result))
		{
			scriptSystem->SetGlobalToNull("entity");
			return result ? eGOR_SUCCEEDED : eGOR_FAILED;
		}
	}

	scriptSystem->SetGlobalToNull("entity");

	return eGOR_FAILED;
}

COPDeValue::COPDeValue(int nPuppetsAlso, bool bClearDevalued) :
	m_bDevaluePuppets(nPuppetsAlso != 0),
	m_bClearDevalued(bClearDevalued)
{
}

COPDeValue::COPDeValue(const XmlNodeRef& node) :
	m_bDevaluePuppets(s_xml.GetBool(node, "devaluePuppets")),
	m_bClearDevalued(s_xml.GetBool(node, "clearDevalued"))
{
}

EGoalOpResult COPDeValue::Execute(CPipeUser* pPipeUser)
{
	if (m_bClearDevalued)
		pPipeUser->ClearDevalued();
	else if (pPipeUser->GetAttentionTarget())
	{
		if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
			pPuppet->Devalue(pPipeUser->GetAttentionTarget(), m_bDevaluePuppets);
	}
	return eGOR_DONE;
}

////////////////////////////////////////////////////////////////////////////////////

EGoalOpResult COPTacticalPos::Execute(CPipeUser* pPipeUser)
{
	EGoalOpResult ret = eGOR_FAILED;

	CAISystem* pAISystem = GetAISystem();

	switch (m_state)
	{
	case eTPS_QUERY_INIT:
		{
			// Start an async TPS query

			// Store our current hidepos
			m_vLastHidePos = pPipeUser->m_CurrentHideObject.GetObjectPos();

			// Set up context
			QueryContext context;
			InitQueryContextFromActor(pPipeUser, context);
			m_queryInstance.SetContext(context);

			// Start the query
			m_queryInstance.Execute(eTPQF_LockResults);
			m_state = eTPS_QUERY;
		}
	// Fall through! Status may have changed immediately.
	case eTPS_QUERY:
		{
			// Wait for results from the TPS query

			ETacticalPointQueryState eTPSState = m_queryInstance.GetStatus();
			switch (eTPSState)
			{
			case eTPSQS_InProgress:
				// Query is still being processed
				{
					ret = eGOR_IN_PROGRESS;
				}
				break;
			case eTPSQS_Error:
			// Got an error - abort this op.
			case eTPSQS_Fail:
				{
					// We found no points so we have nowhere to go - but there were no errors
					Reset(pPipeUser);

					// This doesn't quite make sense if we're going to set a refpoint

					// Wrapping with old methods...
					//pPipeUser->SetInCover(false);
					//pPipeUser->m_CurrentHideObject.Invalidate();

					SendStateSignal(pPipeUser, eTPGOpState_NoPointFound);

					ret = eGOR_FAILED;
				}
				break;

			case eTPSQS_Success:
				{
					m_queryInstance.UnlockResults();
					// We found a point successfully, so we can continue with the op
					m_iOptionUsed = m_queryInstance.GetOptionUsed();

					if (m_iOptionUsed < 0)
						SendStateSignal(pPipeUser, eTPGOpState_NoPointFound);
					else
						SendStateSignal(pPipeUser, eTPGOpState_PointFound);

					STacticalPointResult point = m_queryInstance.GetBestResult();
					assert(point.IsValid());

					// Wrapping old methods...
					pPipeUser->m_CurrentHideObject.Invalidate();
					switch (m_nReg)
					{
					case AI_REG_REFPOINT:
						if (point.flags & eTPDF_AIObject)
						{
							if (CAIObject* pAIObject = gAIEnv.pObjectContainer->GetAIObject(point.aiObjectId))
							{
								pPipeUser->SetRefPointPos(pAIObject->GetPos(), pAIObject->GetEntityDir());

								Reset(pPipeUser);
								return eGOR_SUCCEEDED;
							}
						}
						else if (point.flags & eTPDF_CoverID)
						{
							pPipeUser->SetRefPointPos(point.vPos, Vec3Constants<float>::fVec3_OneY);

							Reset(pPipeUser);
							return eGOR_SUCCEEDED;
						}

						// we can expect a position. vObjDir may not be set if this is not a hidespot, but should be zero.
						pPipeUser->SetRefPointPos(point.vPos, point.vObjDir);
						Reset(pPipeUser);
						return eGOR_SUCCEEDED;
					case AI_REG_LASTOP:
						{
							if (CAIObject* pAIObject = gAIEnv.pObjectContainer->GetAIObject(point.aiObjectId))
							{
								pPipeUser->SetLastOpResult(pAIObject->GetSelfReference());
								Reset(pPipeUser);
								return eGOR_SUCCEEDED;
							}
						}
						return eGOR_FAILED;
					case AI_REG_COVER:
						{
							if (point.flags & eTPDF_Hidespot)
							{
								assert(!GetAISystem()->IsHideSpotOccupied(pPipeUser, point.vObjPos));

								SHideSpot hs(SHideSpotInfo::eHST_ANCHOR, point.vObjPos, point.vObjDir);
								hs.pAnchorObject = gAIEnv.pObjectContainer->GetAIObject(point.aiObjectId);

								pPipeUser->m_CurrentHideObject.Set(&hs, point.vObjPos, point.vObjDir);
								Reset(pPipeUser);
								return eGOR_SUCCEEDED;
							}
							else if (point.flags & eTPDF_CoverID)
							{
								assert(!gAIEnv.pCoverSystem->IsCoverOccupied(point.coverID) ||
								       (gAIEnv.pCoverSystem->GetCoverOccupant(point.coverID) == pPipeUser->GetEntityID()));

								pPipeUser->SetCoverRegister(point.coverID);

								Reset(pPipeUser);
								return eGOR_SUCCEEDED;
							}
						}
						return eGOR_FAILED;
					default:
						if (point.flags & eTPDF_Mask_AbstractHidespot)
						{
							// Wrapping old methods...
							// This doesn't make us go there, more provides debugging and setup when we arrive.

							// (MATT) I'm having to disable this for now, which might be a mistake, until I can expose it cleanly {2009/07/31}
							//pPipeUser->m_CurrentHideObject.Set(&(point.hidespot), point.vPos, point.vObjDir);
						}
					}

					// Have we chosen the same hidespot again?
					if (pPipeUser->m_CurrentHideObject.IsValid() && m_vLastHidePos.IsEquivalent(pPipeUser->m_CurrentHideObject.GetObjectPos()))
						pPipeUser->SetSignal(1, "OnSameHidespotAgain", 0, 0, 0);  // TODO: Add CRC

					// TODO : (MATT)  {2007/05/23:11:58:45} Hack - Switching urgency in the hide op isn't good.
					// If it's a short distance to the hidepoint, running generally looks bad, so force to walk
					Vec3 vHideVec = point.vPos - pPipeUser->GetPos();
					float fHideDist = vHideVec.GetLength();

					if (fHideDist < 5.0f)
						pPipeUser->m_State.fMovementUrgency = AISPEED_WALK;
					if (fHideDist < 0.1f) // TODO: Follow all this execution path thru properly. And simplify.
						SendStateSignal(pPipeUser, eTPGOpState_DestinationReached);

					gAIEnv.pAIObjectManager->CreateDummyObject(m_refHideTarget, string(GetNameSafe(pPipeUser)) + "_HideTarget");
					m_refHideTarget.GetAIObject()->SetPos(point.vPos);
					pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;

					ret = eGOR_IN_PROGRESS;
					m_state = eTPS_PATHFIND_INIT;
				}
			}
		}
		break;

	case eTPS_PATHFIND_INIT:
		pPipeUser->ClearPath("COPTacticalPos::Execute");

		if (!m_pPathFindDirective)
		{
			CAIObject* pHideTarget = m_refHideTarget.GetAIObject();
			assert(pHideTarget);

			if (gAIEnv.CVars.DebugPathFinding)
			{

				const Vec3& vHidePos = pHideTarget->GetPos();
				AILogAlways("COPHide::Execute %s pathfinding to (%5.2f, %5.2f, %5.2f)", GetNameSafe(pPipeUser),
				            vHidePos.x, vHidePos.y, vHidePos.z);
			}

			// request the path
			m_pPathFindDirective = new COPPathFind("", pHideTarget);
			ret = eGOR_IN_PROGRESS;
		}
		m_state = eTPS_PATHFIND;
	//fallthrough
	case eTPS_PATHFIND:
		ret = eGOR_IN_PROGRESS; // Always in progress if we are at the pathfind state! (Let path finder do its job)
		if (m_pPathFindDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS)
		{
			CAIObject* pHideTarget = m_refHideTarget.GetAIObject();
			if (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND)
			{
				if (gAIEnv.CVars.DebugPathFinding)
				{
					// We should have hide target
					const Vec3& vHidePos = pHideTarget->GetPos();
					AILogAlways("COPHide::Execute %s Creating trace to hide target (%5.2f, %5.2f, %5.2f)", GetNameSafe(pPipeUser),
					            vHidePos.x, vHidePos.y, vHidePos.z);
				}

				m_state = eTPS_TRACE_INIT;
			}
			else
			{
				// Could not reach the point, mark it ignored so that we do not try to pick it again.
				pPipeUser->IgnoreCurrentHideObject(10.0f);

				Reset(pPipeUser);
				SendStateSignal(pPipeUser, eTPGOpState_DestinationReached);

				ret = eGOR_SUCCEEDED;
			}
		}
		break;
	case eTPS_TRACE_INIT:
		{
			if (!m_pTraceDirective)
				m_pTraceDirective = new COPTrace(m_bLookAtHide, defaultTraceEndAccuracy);
			m_state = eTPS_TRACE;
		}
	//falltrough
	case eTPS_TRACE:
		{
			ret = eGOR_IN_PROGRESS;
			if (m_pTraceDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS)
			{
				Reset(pPipeUser);
				SendStateSignal(pPipeUser, eTPGOpState_DestinationReached);

				ret = eGOR_SUCCEEDED;
			}
			break;
		}
	}
	return ret;
}

void COPTacticalPos::QueryDryUpdate(CPipeUser* pPipeUser)
{
	ETacticalPointQueryState eTPSState = m_queryInstance.GetStatus();

	switch (eTPSState)
	{
	case eTPSQS_Success:
		{
			STacticalPointResult point = m_queryInstance.GetBestResult();
			assert(point.IsValid());

			switch (m_nReg)
			{
			case AI_REG_COVER:
				{
					if (point.flags & eTPDF_Hidespot)
					{
						assert(!GetAISystem()->IsHideSpotOccupied(pPipeUser, point.vObjPos));

						SHideSpot hs(SHideSpotInfo::eHST_ANCHOR, point.vObjPos, point.vObjDir);
						hs.pAnchorObject = gAIEnv.pObjectContainer->GetAIObject(point.aiObjectId);

						pPipeUser->m_CurrentHideObject.Set(&hs, point.vObjPos, point.vObjDir);
					}
					else if (point.flags & eTPDF_CoverID)
					{
						assert(!gAIEnv.pCoverSystem->IsCoverOccupied(point.coverID) ||
						       (gAIEnv.pCoverSystem->GetCoverOccupant(point.coverID) == pPipeUser->GetEntityID()));

						pPipeUser->SetCoverRegister(point.coverID);
					}
				}
			default:
				break;
			}
		}
	}
}

void COPTacticalPos::ExecuteDry(CPipeUser* pPipeUser)
{
	QueryDryUpdate(pPipeUser);

	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pPipeUser, false);
}

bool COPTacticalPos::IsBadHiding(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IAIObject* pTarget = pPipeUser->GetAttentionTarget();

	if (!pTarget)
		return false;

	Vec3 ai_pos = pPipeUser->GetPos();
	Vec3 target_pos = pTarget->GetPos();
	ray_hit hit;
	IPhysicalEntity* skipList[4];
	int skipCount(1);
	skipList[0] = pPipeUser->GetProxy()->GetPhysics();
	if (pTarget->GetProxy() && pTarget->GetProxy()->GetPhysics())
	{
		skipList[skipCount++] = pTarget->GetProxy()->GetPhysics();
		if (pTarget->GetProxy()->GetPhysics(true))
		{
			PREFAST_SUPPRESS_WARNING(6386)
			skipList[skipCount++] = pTarget->GetProxy()->GetPhysics(true);
		}
	}
	int rayresult(0);
	rayresult = gAIEnv.pWorld->RayWorldIntersection(
	  target_pos, ai_pos - target_pos, COVER_OBJECT_TYPES,
	  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
	  &hit, 1, &skipList[0], skipCount);
	if (rayresult)
	{
		// check possible leaning direction
		if ((hit.pt - ai_pos).GetLengthSquared() < 9.f)
		{
			Vec3 dir = ai_pos - target_pos;
			float zcross = dir.y * hit.n.x - dir.x * hit.n.y;
			if (zcross < 0)
				pPipeUser->SetSignal(1, "OnRightLean", 0, 0, gAIEnv.SignalCRCs.m_nOnRightLean);
			else
				pPipeUser->SetSignal(1, "OnLeftLean", 0, 0, gAIEnv.SignalCRCs.m_nOnLeftLean);
		}
		return false;
	}

	// try lowering yourself 1 meter
	// no need to lover self - just check if the hide is as high as you are; if not - it is a lowHideSpot
	ai_pos = pPipeUser->GetPos();
	//	ai_pos.z-=1.f;
	rayresult = gAIEnv.pWorld->RayWorldIntersection(
	  ai_pos, target_pos - ai_pos, COVER_OBJECT_TYPES,
	  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
	  &hit, 1, &skipList[0], skipCount);
	if (!rayresult)
	{
		// try lowering yourself 1 meter - see if this hides me
		ai_pos.z -= 1.f;
		rayresult = gAIEnv.pWorld->RayWorldIntersection(
		  ai_pos, target_pos - ai_pos,
		  COVER_OBJECT_TYPES, AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
		  &hit, 1, &skipList[0], skipCount);
		//		if (rayresult)
		{
			pPipeUser->SetSignal(1, "OnLowHideSpot", 0, 0, gAIEnv.SignalCRCs.m_nOnLowHideSpot);
			return false;
		}
		return true;
	}
	return false;
}

COPTacticalPos::COPTacticalPos(int tacQueryID, EAIRegister nReg) :
	m_nReg(nReg),
	m_pPathFindDirective(0),
	m_pTraceDirective(0),
	m_iOptionUsed(-1),
	m_bLookAtHide(false)
{
	Reset(NULL);
	m_queryInstance.SetQueryID(tacQueryID);
}

COPTacticalPos::COPTacticalPos(const XmlNodeRef& node) :
	m_nReg(AI_REG_NONE),
	m_pPathFindDirective(0),
	m_pTraceDirective(0),
	m_iOptionUsed(-1),
	m_bLookAtHide(false)
{
	CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
	int tacQueryID = 0;

	const char* szQueryName = node->getAttr("name");
	if (stricmp(szQueryName, ""))
	{
		tacQueryID = pTPS->GetQueryID(szQueryName);
		if (!tacQueryID)
		{
			AIWarning("Goalop 'TacticalPos': Query '%s' does not exist (yet).", szQueryName);
		}
	}
	else
	{
		if (s_xml.GetMandatory(node, "id", tacQueryID))
		{
			if (!pTPS->GetQueryName(tacQueryID))
			{
				AIError("Goalop 'TacticalPos': Query with id %d could not be found.", tacQueryID);
			}
		}
		else
		{
			AIError("Goalop 'TacticalPos': Neither query name nor id was provided.");
		}
	}

	if (tacQueryID)
	{
		s_xml.GetRegister(node, "register", m_nReg);
	}

	Reset(NULL);
	m_queryInstance.SetQueryID(tacQueryID);
}

COPTacticalPos::COPTacticalPos(const COPTacticalPos& rhs) :
	m_nReg(rhs.m_nReg),
	m_pPathFindDirective(0),
	m_pTraceDirective(0),
	m_iOptionUsed(-1),
	m_bLookAtHide(false),
	m_queryInstance(rhs.m_queryInstance)
{
	Reset(NULL);
}

void COPTacticalPos::Reset(CPipeUser* pPipeUser)
{
	m_state = eTPS_QUERY_INIT;

	m_queryInstance.UnlockResults();
	m_queryInstance.Cancel();

	m_vLastHidePos.zero();

	SAFE_DELETE(m_pPathFindDirective);
	SAFE_DELETE(m_pTraceDirective);

	if (pPipeUser)
	{
		pPipeUser->SetMovingToCover(false);
		if (m_nReg != AI_REG_REFPOINT && m_nReg != AI_REG_LASTOP && m_nReg != AI_REG_COVER)
			pPipeUser->ClearPath("COPTacticalPos::Reset m_Path");
	}

	m_refHideTarget.Release();
}

////////////////////////////////////////////////////////////////////////////////////

void COPTacticalPos::Serialize(TSerialize ser)
{
	// TODO Tactical Positioning System is not yet serialized, so until that is fixed, do not serialize any state here.
	//	Instead, the query will be rerun and reprocessed completely, which is not ideal but prevents the GoalOp from stalling waiting for a return on a previously sent query, which TPS has already forgotten about.

	if (ser.IsReading())
	{
		Reset(NULL);
	}

	/*ser.BeginGroup("COPTacticalPos");
	   {
	   m_refHideTarget.Serialize(ser, "m_refHideTarget");

	   ser.Value("m_bLookAtHide",m_bLookAtHide);
	   if(ser.IsWriting())
	   {
	    if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
	    {
	      PREFAST_SUPPRESS_WARNING(6011) m_pTraceDirective->Serialize(ser, objectTracker);
	      ser.EndGroup();
	    }
	    if(ser.BeginOptionalGroup("PathFindDirective", m_pPathFindDirective!=NULL))
	    {
	      PREFAST_SUPPRESS_WARNING(6011) m_pPathFindDirective->Serialize(ser, objectTracker);
	      ser.EndGroup();
	    }
	   }
	   else
	   {
	    SAFE_DELETE(m_pTraceDirective);
	    if(ser.BeginOptionalGroup("TraceDirective", true))
	    {
	      m_pTraceDirective = new COPTrace(true);
	      m_pTraceDirective->Serialize(ser, objectTracker);
	      ser.EndGroup();
	    }
	    SAFE_DELETE(m_pPathFindDirective);
	    if(ser.BeginOptionalGroup("PathFindDirective", true))
	    {
	      m_pPathFindDirective = new COPPathFind("");
	      m_pPathFindDirective->Serialize(ser, objectTracker);
	      ser.EndGroup();
	    }
	   }

	   ser.EndGroup();
	   }*/
}

////////////////////////////////////////////////////////////////////////////////////

void COPTacticalPos::SendStateSignal(CPipeUser* pPipeUser, int nState)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
	IAISignalExtraData* pEData = gEnv->pAISystem->CreateSignalExtraData();  // no leak - this will be deleted inside SetSignal

	int queryID = m_queryInstance.GetQueryID();
	pEData->string1 = pTPS->GetQueryName(queryID);
	if (nState != eTPGOpState_NoPointFound)
	{
		pEData->string2 = pTPS->GetOptionLabel(m_queryInstance.GetQueryID(), m_iOptionUsed);
	}

	switch (nState)
	{
	case eTPGOpState_NoPointFound:
		pPipeUser->SetSignal(1, "OnTPSDestNotFound", 0, pEData, gAIEnv.SignalCRCs.m_nOnTPSDestinationNotFound);
		break;
	case eTPGOpState_PointFound:
		pPipeUser->SetSignal(1, "OnTPSDestFound", 0, pEData, gAIEnv.SignalCRCs.m_nOnTPSDestinationFound);
		break;
	case eTPGOpState_DestinationReached:
		pPipeUser->SetSignal(1, "OnTPSDestReached", 0, pEData, gAIEnv.SignalCRCs.m_nOnTPSDestinationReached);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////////

COPLook::COPLook(int lookMode, bool bBodyTurn, EAIRegister nReg)
	: m_eLookThere(LOOKSTYLE_DEFAULT)
	, m_eLookBack(LOOKSTYLE_DEFAULT)
	, m_nReg(nReg)
	, m_nLookID(0)
	, m_bInitialised(false)
	, m_fLookTime(0.0f)
	, m_fTimeLeft(0.0f)
{
	// Pick the soft/hard lookstyles that match the bodyturn parameter
	ELookStyle eSoft, eHard;
	if (bBodyTurn)
	{
		eSoft = LOOKSTYLE_SOFT;
		eHard = LOOKSTYLE_HARD;
	}
	else
	{
		eSoft = LOOKSTYLE_SOFT_NOLOWER;
		eHard = LOOKSTYLE_HARD_NOLOWER;
	}

	// Convert into lookstyles

	switch (lookMode)
	{
	case AILOOKMOTIVATION_LOOK:
		m_fLookTime = 1.5f;
		m_eLookThere = eSoft;
		m_eLookBack = eSoft;
		break;
	case AILOOKMOTIVATION_GLANCE:
		m_fLookTime = 0.8f;
		m_eLookThere = eHard;
		m_eLookBack = eHard;
		break;
	case AILOOKMOTIVATION_STARTLE:
		m_fLookTime = 0.8f;
		m_eLookThere = eHard;
		m_eLookBack = eSoft;
		break;
	case AILOOKMOTIVATION_DOUBLETAKE:
		m_fLookTime = 1.2f;
		m_eLookThere = eSoft;
		m_eLookBack = eHard;
		break;
	default:
		assert(false && "Unknown look motivation");
	}
}

COPLook::COPLook(const XmlNodeRef& node)
	: m_eLookThere(LOOKSTYLE_DEFAULT)
	, m_eLookBack(LOOKSTYLE_DEFAULT)
	, m_nReg(AI_REG_LASTOP)
	, m_nLookID(0)
	, m_bInitialised(false)
	, m_fLookTime(0.0f)
	, m_fTimeLeft(0.0f)
{
	s_xml.GetRegister(node, "register", m_nReg);

	// Pick the soft/hard look styles that match the body turn parameter
	ELookStyle eSoft, eHard;
	if (s_xml.GetBool(node, "bodyTurn", true))
	{
		eSoft = LOOKSTYLE_SOFT;
		eHard = LOOKSTYLE_HARD;
	}
	else
	{
		eSoft = LOOKSTYLE_SOFT_NOLOWER;
		eHard = LOOKSTYLE_HARD_NOLOWER;
	}

	// Convert into look styles
	ELookMotivation eLookMode = AILOOKMOTIVATION_LOOK;
	s_xml.GetLook(node, "id", eLookMode);
	switch (eLookMode)
	{
	case AILOOKMOTIVATION_LOOK:
		m_fLookTime = 1.5f;
		m_eLookThere = eSoft;
		m_eLookBack = eSoft;
		break;
	case AILOOKMOTIVATION_GLANCE:
		m_fLookTime = 0.8f;
		m_eLookThere = eHard;
		m_eLookBack = eHard;
		break;
	case AILOOKMOTIVATION_STARTLE:
		m_fLookTime = 0.8f;
		m_eLookThere = eHard;
		m_eLookBack = eSoft;
		break;
	case AILOOKMOTIVATION_DOUBLETAKE:
		m_fLookTime = 1.2f;
		m_eLookThere = eSoft;
		m_eLookBack = eHard;
		break;
	default:
		assert(false && "Unknown look motivation");
	}
}

EGoalOpResult COPLook::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPLook_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Initialisation
	if (!m_bInitialised)
	{
		// Get the look target
		CWeakRef<CAIObject> refLookTarget;
		switch (m_nReg)
		{
		case AI_REG_LASTOP:
			refLookTarget = GetWeakRef(pPipeUser->GetLastOpResult());
			break;
		case AI_REG_REFPOINT:
			refLookTarget = GetWeakRef(pPipeUser->GetRefPoint());
			break;
		default:
			AIError("COPLook failed - unknown AI register");
		}

		if (!refLookTarget.IsValid()) return eGOR_FAILED;

		m_nLookID = pPipeUser->SetLooseAttentionTarget(refLookTarget, m_nLookID);
		pPipeUser->SetLookStyle(m_eLookThere);
		pPipeUser->SetLookAtPointPos(refLookTarget.GetAIObject()->GetPos(), true /*priority: overrides other possible look targets*/);
		m_fTimeLeft = m_fLookTime;
		m_bInitialised = true;
	}

	// Just wait for time to expire
	// Should probably sit in ExecuteDry

	// We can now check pPuppet->GetLookIKStatus() to decide when to end
	// A value of 1.0f means that we're now looking in the right direction
	// A value < 0 means that Look IK will not arrive at desired target (for various possible reasons)
	// So we should quite on this condition also.
	//CryLogAlways( "look IK status [%f]", pPuppet->GetLookIKStatus() );

	//m_fTimeLeft -= pAISystem->GetFrameDeltaTime(); Not accurate
	m_fTimeLeft -= GetAISystem()->GetUpdateInterval();
	if (m_fTimeLeft < 0.0f)
	{
		Reset(pPipeUser);
		return eGOR_SUCCEEDED;
	}

	return eGOR_IN_PROGRESS;
}

void COPLook::ExecuteDry(CPipeUser* pPipeUser)
{
}

void COPLook::Reset(CPipeUser* pPipeUser)
{
	pPipeUser->SetLooseAttentionTarget(NILREF, m_nLookID);
	pPipeUser->SetLookStyle(m_eLookBack);
	pPipeUser->ResetLookAt();
	m_bInitialised = false;
}

void COPLook::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPLook");
	{
		// TODO : (MATT) Insert serialisation code here {2007/09/14:14:55:01}
		assert(false && "Insert code here");
		ser.EndGroup();
	}
}

#if 0
// deprecated and won't compile at all...

//
//----------------------------------------------------------------------------------------------------------

EGoalOpResult COPForm::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	pPipeUser->CreateFormation(m_sName.c_str());
	return eGOR_DONE;
}
#endif // 0

EGoalOpResult COPClear::Execute(CPipeUser* pPipeUser)
{
	return eGOR_DONE;
}

#if 0
// deprecated and won't compile at all...
//
//----------------------------------------------------------------------------------------------------------
COPContinuous::COPContinuous(bool bKeeoMoving) :
	m_bKeepMoving(bKeeoMoving)
{

}

//
//----------------------------------------------------------------------------------------------------------
COPContinuous::COPContinuous(const XmlNodeRef& node) :
	m_bKeepMoving(s_xml.GetBool(node, "keepMoving"))
{

}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPContinuous::Execute(CPipeUser* pPipeUser)
{
	pPipeUser->m_bKeepMoving = m_bKeepMoving;
	return eGOR_DONE;
}

//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
COPSteer::COPSteer(float fSteerDistance, float fPathLenLimit)
	: m_vLastUsedTargetPos(0, 0, 0),
	m_fMinEndDistance(0.f),
	m_fMaxEndDistance(0.f),
	m_pTraceDirective(NULL),
	m_pPathfindDirective(NULL),
	m_bFirstExec(true),
	m_fPathLenLimit(fPathLenLimit),
	m_fEndAccuracy(0.f)
{
	m_fSteerDistanceSqr = fSteerDistance * fSteerDistance;
}

//
//----------------------------------------------------------------------------------------------------------
COPSteer::COPSteer(const XmlNodeRef& node) :
	m_vLastUsedTargetPos(ZERO),
	m_fMinEndDistance(0.f),
	m_fMaxEndDistance(0.f),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_bFirstExec(true),
	m_fPathLenLimit(0.f),
	m_fEndAccuracy(0.f)
{
	float fSteerDistance;
	if (!s_xml.GetMandatory(node, "distance", fSteerDistance))
	{
		fSteerDistance = 0.f;
	}

	m_fSteerDistanceSqr = fSteerDistance * fSteerDistance;

	s_xml.GetMandatory(node, "pathLengthLimit", m_fPathLenLimit);
}

//
//----------------------------------------------------------------------------------------------------------
COPSteer::~COPSteer()
{
	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPSteer::~COPSteer %p", this);
}

//
//----------------------------------------------------------------------------------------------------------
void COPSteer::DebugDraw(CPipeUser* pPipeUser) const
{

}

//
//----------------------------------------------------------------------------------------------------------
void COPSteer::Reset(CPipeUser* pPipeUser)
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPSteer::Reset %s", GetNameSafe(pPipeUser));

	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	m_bNeedHidespot = true;
	m_vLastUsedTargetPos.zero();

	if (pPipeUser)
	{
		pPipeUser->ClearPath("COPSteer::Reset m_Path");
	}
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPSteer::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (pPipeUser->m_nPathDecision == PATHFINDER_STILLFINDING && m_pPathfindDirective != NULL)
	{
		m_pPathfindDirective->Execute(pPipeUser);
		return eGOR_IN_PROGRESS;
	}

	if (m_pPosDummy == NULL)
	{
		pPipeUser->SetSignal(0, "OnSteerFailed", pPipeUser->GetEntity(), NULL, 0);
		AILogAlways("COPSteer::Execute - Error - No point to steer to: %s", GetNameSafe(pPipeUser));

		return eGOR_FAILED;
	}

	if (pPipeUser->GetPos().GetSquaredDistance2D(m_pPosDummy->GetPos()) < 0.3f)
	{
		return eGOR_SUCCEEDED;
	}
	else
	{
		CAIObject* pTarget = m_pPosDummy;

		bool bForceReturnPartialPath = false;

		//if (pPipeUser->m_nPathDecision != PATHFINDER_PATHFOUND) m_fLastTime = -1.0f;

		// Check for unsuitable agents
		if (pPipeUser->GetSubType() == CAIObject::STP_HELI
		    || !pPipeUser->m_movementAbility.bUsePathfinder)
		{
			AILogAlways("COPSteer::Execute - Error - Unsuitable agent: %s", GetNameSafe(pPipeUser));
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		if (pTarget->GetSubType() == CAIObject::STP_ANIM_TARGET)
		{
			AILogAlways("COPSteer::Execute - Error - Unsuitable target: %s", GetNameSafe(pPipeUser));
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		// Move strictly to the target point.
		// MTJ: Not sure this should stay
		pPipeUser->m_bPathfinderConsidersPathTargetDirection = true;

		pPipeUser->SetInCover(false);
		pPipeUser->m_CurrentHideObject.Invalidate();

		// If we have a hidespot, that's our target
		// Otherwise it's the attention target
		if (m_pPosDummy)
			pTarget = m_pPosDummy;

		switch (m_pPathfindDirective ? pPipeUser->m_nPathDecision : PATHFINDER_MAXVALUE)
		{
		case PATHFINDER_MAXVALUE:
			{
				// Generate path to target
				float endTol = (bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy);
				m_pPathfindDirective = new COPPathFind("", pTarget, endTol, 0.0f, m_fPathLenLimit);
				pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;
				if (m_pPathfindDirective->Execute(pPipeUser) && pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
				{
					// If special nopath signal is specified, send the signal.
					pPipeUser->m_State.vMoveDir.zero();
					return eGOR_FAILED;
				}
				break;
			}
		case PATHFINDER_PATHFOUND:
			{
				// if we have a path, trace it
				if (pPipeUser->m_OrigPath.Empty() || pPipeUser->m_OrigPath.GetPathLength(true) < 0.01f)
				{
					AILogAlways("COPSteer::Execute - Origpath is empty: %s. Regenerating.", GetNameSafe(pPipeUser));
					/*RegeneratePath( pPipeUser, m_pPosDummy->GetPos() );
					   pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;*/
					return eGOR_FAILED;
				}
				else
				{
					// If we need a tracer, create one
					if (!m_pTraceDirective)
					{
						// We create another goalop to achieve this (sucky approach)
						bool bExact = true;
						bForceReturnPartialPath = false;

						m_pTraceDirective = new COPTrace(bExact, m_fEndAccuracy, bForceReturnPartialPath);
						TPathPoints::const_reference lastPathNode = pPipeUser->m_OrigPath.GetPath().back();
						Vec3 vLastPos = lastPathNode.vPos;
						Vec3 vRequestedLastNodePos = pPipeUser->m_Path.GetParams().end;
						float dist = Distance::Point_Point(vLastPos, vRequestedLastNodePos);

						// MTJ: What is this for?
						if (lastPathNode.navType != IAISystem::NAV_SMARTOBJECT && dist > C_MaxDistanceForPathOffset)// && pPipeUser->m_Path.GetPath().size() == 1 )
						{
							AISignalExtraData* pData = new AISignalExtraData;
							pData->fValue = dist;// - m_fMinEndDistance;
							pPipeUser->SetSignal(0, "OnEndPathOffset", pPipeUser->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnEndPathOffset);
						}
						else
						{
							pPipeUser->SetSignal(0, "OnPathFound", NULL, 0, gAIEnv.SignalCRCs.m_nOnPathFound);
						}
					}

					// Keep tracing - previous code will stop us when close enough
					// MTJ: What previous code?
					bool bDone = (m_pTraceDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS);

					if (bDone)
					{
						pPipeUser->SetSignal(0, "OnFormationPointReached", pPipeUser->GetEntity(), NULL, 0); // MTJ: Provide CRC
						return eGOR_SUCCEEDED;
					}
				}

				break;
			}
		case PATHFINDER_NOPATH:
			{
				pPipeUser->SetSignal(0, "OnGetToFormationPointFailed", pPipeUser->GetEntity(), NULL, 0); // MTJ: Provide CRC
				return eGOR_FAILED;
			}
		default:
			{
				// MTJ: Still pathfinding I guess?
				m_pPathfindDirective->Execute(pPipeUser);
				break;
			}
		}
	}
	// Run me again next tick
	return eGOR_IN_PROGRESS;
}

void COPSteer::ExecuteDry(CPipeUser* pPipeUser)
{
	if (pPipeUser->m_nPathDecision == PATHFINDER_STILLFINDING && m_pPathfindDirective != NULL)
	{
		m_pPathfindDirective->Execute(pPipeUser);
		return;
	}

	if (m_pTraceDirective && m_pPosDummy)
	{
		bool bTargetDirty = false;

		if (pPipeUser->GetPos().GetSquaredDistance2D(m_pPosDummy->GetPos()) < m_fSteerDistanceSqr)
		{

		}
		else
		{
			Vec3 targetVector;
			if (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND /*&& m_bForceReturnPartialPath*/)
				targetVector = (pPipeUser->m_Path.GetLastPathPos() - pPipeUser->GetPhysicsPos());
			else
				targetVector = (m_pPosDummy->GetPhysicsPos() - pPipeUser->GetPhysicsPos());

			{
				Vec3 targetPos = m_pPosDummy->GetPos();
				float targetMoveDist = (m_vLastUsedTargetPos - targetPos).GetLength2D();

				// Use the stored destination point instead of the last path node since the path may be cut because of navSO.
				Vec3 pathEnd = pPipeUser->m_PathDestinationPos;
				Vec3 dir(pathEnd - pPipeUser->GetPos());

				dir.z = 0;
				dir.NormalizeSafe();

				Vec3 dirToTarget(targetPos - pathEnd);

				dirToTarget.z = 0;
				dirToTarget.NormalizeSafe();

				float regenDist = 0.3f;
				if (dirToTarget.Dot(dir) < cosf(DEG2RAD(8.0f)))
					regenDist *= 5.0f;

				if (targetMoveDist > regenDist)
					bTargetDirty = true;

				// when near the path end force more frequent updates
				//float pathDistLeft = pPipeUser->m_Path.GetPathLength(false);
				//float pathEndError = (pathEnd - targetPos).GetLength2D();

				//// TODO/HACK! [Mikko] This prevent the path to regenerated every frame in some special cases in Crysis Core level
				//// where quite a few behaviors are sticking to a long distance (7-10m).
				//// The whole stick regeneration logic is flawed mostly because pPipeUser->m_PathDestinationPos is not always
				//// the actual target position. The pathfinder may adjust the end location and not keep the requested end pos
				//// if the target is not reachable. I'm sure there are other really nasty cases about this path invalidation logic too.
				//const GraphNode* pLastNode = gAIEnv.pGraph->GetNode(pPipeUser->m_lastNavNodeIndex);
				//if (pLastNode && pLastNode->navType == IAISystem::NAV_VOLUME)
				//	pathEndError = max(0.0f, pathEndError - pathDistLeft/*GetEndDistance(pPipeUser)*/);

				//if (pathEndError > 0.1f && pathDistLeft < 2.0f * pathEndError)
				//{
				//	//bTargetDirty = true;
				//}
			}

			//if(!pPipeUser->m_movementAbility.b3DMove)
			//	targetVector.z = 0.0f;

			if (/*pPipeUser->m_nPathDecision != PATHFINDER_STILLFINDING && */ bTargetDirty)
				RegeneratePath(pPipeUser, m_pPosDummy->GetPos());
			else
				m_pTraceDirective->ExecuteTrace(pPipeUser, false);

		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPSteer::RegeneratePath(CPipeUser* pPipeUser, const Vec3& destination)
{
	if (!pPipeUser)
		return;

	CTimeValue time = GetAISystem()->GetFrameStartTime();

	if ((time - m_fLastRegenTime).GetMilliSeconds() < 100.f)
		return;

	m_fLastRegenTime = time;

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPSteer::RegeneratePath %s", GetNameSafe(pPipeUser));
	m_pPathfindDirective->Reset(pPipeUser);
	if (m_pTraceDirective)
		m_pTraceDirective->m_fEndAccuracy = m_fEndAccuracy;
	m_vLastUsedTargetPos = destination;
	pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;

	const Vec3 opPos = pPipeUser->GetPhysicsPos();

	// Check for direct connection if distance is less than fixed value
	if (opPos.GetSquaredDistance(destination) < 2500.f)
	{
		int nbid;
		IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(opPos, nbid, pPipeUser->m_movementAbility.pathfindingProperties.navCapMask);
		CNavRegion* pRegion = gAIEnv.pNavigation->GetNavRegion(navType, gAIEnv.pGraph);
		if (pRegion)
		{
			Vec3 from = opPos;
			Vec3 to = destination;

			NavigationBlockers navBlocker;
			if (pRegion->CheckPassability(from, to, pPipeUser->GetParameters().m_fPassRadius, navBlocker, pPipeUser->GetMovementAbility().pathfindingProperties.navCapMask))
			{
				pPipeUser->ClearPath("COPSteer::RegeneratePath m_Path");

				if (navType == IAISystem::NAV_TRIANGULAR)
				{
					// Make sure not to enter forbidden area.
					if (gAIEnv.pNavigation->IsPathForbidden(opPos, destination))
						return;
					if (gAIEnv.pNavigation->IsPointForbidden(destination, pPipeUser->GetParameters().m_fPassRadius))
						return;
				}

				PathPointDescriptor pt;
				pt.navType = navType;

				pt.vPos = from;
				pPipeUser->m_Path.PushBack(pt);

				float endDistance = 0.5f;//m_fStickDistance;
				if (fabsf(endDistance) > 0.0001f)
				{
					// Handle end distance.
					float dist;
					if (pPipeUser->IsUsing3DNavigation())
						dist = Distance::Point_Point(from, to);
					else
						dist = Distance::Point_Point2D(from, to);

					float d;
					if (endDistance > 0.0f)
						d = dist - endDistance;
					else
						d = -endDistance;
					float u = clamp_tpl(d / dist, 0.0001f, 1.0f);

					pt.vPos = from + u * (to - from);

					pPipeUser->m_Path.PushBack(pt);
				}
				else
				{
					pt.vPos = to;
					pPipeUser->m_Path.PushBack(pt);
				}

				pPipeUser->m_Path.SetParams(SNavPathParams(from, to, Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero, -1, false, endDistance, true));

				pPipeUser->m_OrigPath = pPipeUser->m_Path;
				pPipeUser->m_nPathDecision = PATHFINDER_PATHFOUND;
			}
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPSteer::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPSteer");
	{
		ser.Value("m_vLastUsedTargetPos", m_vLastUsedTargetPos);
		ser.Value("m_fLastDistance", m_fLastDistance);
		ser.Value("m_fMinEndDistance", m_fMinEndDistance);
		ser.Value("m_fMaxEndDistance", m_fMaxEndDistance);
		ser.Value("m_bNeedHidespot", m_bNeedHidespot);
		ser.Value("m_fEndAccuracy", m_fEndAccuracy);

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pTraceDirective->Serialize(ser, objectTracker);
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
#endif // 0

//
//----------------------------------------------------------------------------------------------------------
COPWaitSignal::COPWaitSignal(const XmlNodeRef& node) :
	m_sSignal(s_xml.GetMandatoryString(node, "name")),
	m_edMode(edNone),
	m_intervalMs(0)
{
	float fInterval = 0.0f;
	node->getAttr("timeout", fInterval);
	m_intervalMs = (int)(fInterval * 1000.0f);

	Reset(NULL);
}

//
//----------------------------------------------------------------------------------------------------------
COPWaitSignal::COPWaitSignal(const char* sSignal, float fInterval /*= 0*/)
{
	m_edMode = edNone;
	m_sSignal = sSignal;
	m_intervalMs = (int)(fInterval * 1000.0f);
	Reset(NULL);
}

COPWaitSignal::COPWaitSignal(const char* sSignal, const char* sObjectName, float fInterval /*= 0*/)
{
	m_edMode = edString;
	m_sObjectName = sObjectName;

	m_sSignal = sSignal;
	m_intervalMs = (int)(fInterval * 1000.0f);
	Reset(NULL);
}

COPWaitSignal::COPWaitSignal(const char* sSignal, int iValue, float fInterval /*= 0*/)
{
	m_edMode = edInt;
	m_iValue = iValue;

	m_sSignal = sSignal;
	m_intervalMs = (int)(fInterval * 1000.0f);
	Reset(NULL);
}

COPWaitSignal::COPWaitSignal(const char* sSignal, EntityId nID, float fInterval /*= 0*/)
{
	m_edMode = edId;
	m_nID = nID;

	m_sSignal = sSignal;
	m_intervalMs = (int)(fInterval * 1000.0f);
	Reset(NULL);
}

bool COPWaitSignal::NotifySignalReceived(CAIObject* pPipeUser, const char* szText, IAISignalExtraData* pData)
{
	CCCPOINT(COPWaitSignal_NotifySignalReceived);

	if (!m_bSignalReceived && szText && m_sSignal == szText)
	{
		if (m_edMode == edNone)
			m_bSignalReceived = true;
		else if (pData)
		{
			CCCPOINT(OPWaitSignal_NotifySignalReceived_Data);
			// (MATT) Note that it appears all the data forms could be removed {2008/08/09}
			if (m_edMode == edString)
			{
				const char* objectName = pData->GetObjectName();

				if (objectName && m_sObjectName == objectName)
					m_bSignalReceived = true;
			}
			else if (m_edMode == edInt)
			{
				if (m_iValue == pData->iValue)
					m_bSignalReceived = true;
			}
			else if (m_edMode == edId)
			{
				if (m_nID == pData->nID.n)
					m_bSignalReceived = true;
			}
		}
	}

	return m_bSignalReceived;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPWaitSignal::Reset(CPipeUser* pPipeUser)
{
	if (pPipeUser)
	{
		CPipeUser::ListWaitGoalOps& rGoalOpVec = pPipeUser->m_listWaitGoalOps;
		const CPipeUser::ListWaitGoalOps::iterator cGoalOpEnd = rGoalOpVec.end();
		CPipeUser::ListWaitGoalOps::iterator it = std::find(rGoalOpVec.begin(), cGoalOpEnd, this);
		if (it != cGoalOpEnd)
			rGoalOpVec.erase(it);
	}
	else
	{
		// only reset this on initialize!
		// later it may happen that this is called from signal handler (by inserting a goal pipe)
		m_bSignalReceived = false;
	}
	m_startTime.SetSeconds(0.0f);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPWaitSignal::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_bSignalReceived)
	{
		m_bSignalReceived = false;
		return eGOR_DONE;
	}

	if (m_intervalMs <= 0)
	{
		if (!m_startTime.GetValue())
		{
			pPipeUser->m_listWaitGoalOps.insert(pPipeUser->m_listWaitGoalOps.begin(), this); //push_front
			m_startTime.SetMilliSeconds(1);
		}
		return eGOR_IN_PROGRESS;
	}

	CTimeValue time = GetAISystem()->GetFrameStartTime();
	if (!m_startTime.GetValue())
	{
		m_startTime = time;
		pPipeUser->m_listWaitGoalOps.insert(pPipeUser->m_listWaitGoalOps.begin(), this); //push_front
	}

	CTimeValue timeElapsed = time - m_startTime;
	if (timeElapsed.GetMilliSecondsAsInt64() > m_intervalMs)
		return eGOR_DONE;

	return eGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPWaitSignal::Serialize(TSerialize ser)
{
	// please ask me when you want to change [tetsuji]
	ser.BeginGroup("COPWaitSignal");
	{
		ser.Value("m_bSignalReceived", m_bSignalReceived);
		if (ser.IsReading())
		{
			m_startTime.SetSeconds(0.0f);
		}
		ser.EndGroup();
	}
	// please ask me when you want to change [dejan]
}

//----------------------------------------------------------------------------------------------------------
// COPAnimAction
//----------------------------------------------------------------------------------------------------------
COPAnimation::COPAnimation(EAnimationMode mode, const char* value, const bool isUrgent)
	: m_eMode(mode)
	, m_sValue(value)
	, m_bIsUrgent(isUrgent)
	, m_bAGInputSet(false)
{
}

//
//----------------------------------------------------------------------------------------------------------
COPAnimation::COPAnimation(const XmlNodeRef& node)
	: m_sValue(s_xml.GetMandatoryString(node, "name")),
	m_bIsUrgent(s_xml.GetBool(node, "urgent", false)),
	m_bAGInputSet(false),
	m_eMode(AIANIM_INVALID)
{
	s_xml.GetAnimationMode(node, "mode", m_eMode, CGoalOpXMLReader::MANDATORY);
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAnimation::Reset(CPipeUser* pPipeUser)
{
	m_bAGInputSet = false;
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPAnimation::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	AIAssert(pPipeUser);
	if (!pPipeUser->GetProxy())
		return eGOR_FAILED;

	if (m_bAGInputSet)
	{
		switch (m_eMode)
		{
		case AIANIM_SIGNAL:
			return pPipeUser->GetProxy()->IsSignalAnimationPlayed(m_sValue) ? eGOR_SUCCEEDED : eGOR_IN_PROGRESS;
		case AIANIM_ACTION:
			return pPipeUser->GetProxy()->IsActionAnimationStarted(m_sValue) ? eGOR_SUCCEEDED : eGOR_IN_PROGRESS;
		}
		AIAssert(!"Setting an invalid AG input has returned true!");
		return eGOR_FAILED;
	}

	// set the value here
	if (!pPipeUser->GetProxy()->SetAGInput(m_eMode == AIANIM_ACTION ? AIAG_ACTION : AIAG_SIGNAL, m_sValue, m_bIsUrgent))
	{
		AIWarning("Can't set value '%s' on AG input '%s' of PipeUser '%s'", m_sValue.c_str(), m_eMode == AIANIM_ACTION ? "Action" : "Signal", GetNameSafe(pPipeUser));
		return eGOR_FAILED;
	}
	m_bAGInputSet = true;

	return eGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAnimation::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPAnimation");
	{
		ser.EnumValue("m_eMode", m_eMode, AIANIM_SIGNAL, AIANIM_ACTION);
		ser.Value("m_sValue", m_sValue);
		ser.Value("m_bIsUrgent", m_bIsUrgent);
		ser.Value("m_bAGInputSet", m_bAGInputSet);
		ser.EndGroup();
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAnimTarget::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPAnimTarget");
	{
		ser.Value("m_bSignal", m_bSignal);
		ser.Value("m_sAnimName", m_sAnimName);
		ser.Value("m_fStartWidth", m_fStartWidth);
		ser.Value("m_fDirectionTolerance", m_fDirectionTolerance);
		ser.Value("m_fStartArcAngle", m_fStartArcAngle);
		ser.Value("m_vApproachPos", m_vApproachPos);
		ser.Value("m_bAutoAlignment", m_bAutoAlignment);
		ser.EndGroup();
	}
}

//----------------------------------------------------------------------------------------------------------
// COPAnimTarget
//----------------------------------------------------------------------------------------------------------
COPAnimTarget::COPAnimTarget(bool signal, const char* animName, float startWidth, float dirTolerance, float startArcAngle, const Vec3& approachPos, bool autoAlignment)
	: m_bSignal(signal)
	, m_sAnimName(animName)
	, m_fStartWidth(startWidth)
	, m_fDirectionTolerance(dirTolerance)
	, m_fStartArcAngle(startArcAngle)
	, m_vApproachPos(approachPos)
	, m_bAutoAlignment(autoAlignment)
{
}

//
//----------------------------------------------------------------------------------------------------------
COPAnimTarget::COPAnimTarget(const XmlNodeRef& node)
	: m_sAnimName(s_xml.GetMandatoryString(node, "name")),
	m_fStartWidth(0.f),
	m_fDirectionTolerance(0.f),
	m_fStartArcAngle(0.f),
	m_vApproachPos(ZERO),
	m_bAutoAlignment(s_xml.GetBool(node, "autoAlignment"))
{
	EAnimationMode eAnimationMode;
	if (!s_xml.GetAnimationMode(node, "mode", eAnimationMode, CGoalOpXMLReader::MANDATORY))
	{
		eAnimationMode = AIANIM_SIGNAL;
	}
	m_bSignal = (eAnimationMode == AIANIM_SIGNAL);

	s_xml.GetMandatory(node, "startWidth", m_fStartWidth);
	s_xml.GetMandatory(node, "directionTolerance", m_fDirectionTolerance);
	s_xml.GetMandatory(node, "startArcAngle", m_fStartArcAngle);

	float fApproachRadius;
	if (node->getAttr("approachRadius", fApproachRadius))
	{
		m_vApproachPos = Vec3(fApproachRadius);
	}
}

EGoalOpResult COPAnimTarget::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	IAIObject* pTarget = pPipeUser->GetLastOpResult();
	if (pTarget)
	{
		SAIActorTargetRequest req;
		if (!m_vApproachPos.IsZero())
		{
			req.approachLocation = m_vApproachPos;
			req.approachDirection = pTarget->GetPos() - m_vApproachPos;
			req.approachDirection.NormalizeSafe(Vec3(0, 1, 0));
		}
		else
		{
			req.approachLocation = pTarget->GetPos();
			req.approachDirection = pTarget->GetMoveDir();
		}
		req.animLocation = pTarget->GetPos();
		req.animDirection = pTarget->GetMoveDir();
		if (pPipeUser->IsUsing3DNavigation() == false)
		{
			req.animDirection.z = 0.0f;
			req.animDirection.NormalizeSafe();
		}
		req.directionTolerance = DEG2RAD(m_fDirectionTolerance);
		req.startArcAngle = m_fStartArcAngle;
		req.startWidth = m_fStartWidth;
		req.signalAnimation = m_bSignal;
		req.useAssetAlignment = m_bAutoAlignment;
		req.animation = m_sAnimName;

		pPipeUser->SetActorTargetRequest(req);
	}
	return eGOR_DONE;
}

//
//-------------------------------------------------------------------------------------------------------------
COPWait::COPWait(int waitType, int blockCount) :
	m_WaitType(static_cast<EOPWaitType>(waitType)),
	m_BlockCount(blockCount)
{

}

//
//-------------------------------------------------------------------------------------------------------------
COPWait::COPWait(const XmlNodeRef& node) :
	m_WaitType(WAIT_ALL),
	m_BlockCount(0)
{
	const char* szWaitType = node->getAttr("for");
	if (!stricmp(szWaitType, "Any"))
	{
		m_WaitType = WAIT_ANY;
	}
	else
	{
		if (!stricmp(szWaitType, "Any2"))
		{
			m_WaitType = WAIT_ANY_2;
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPWait::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	int currentBlockCount = pPipeUser->CountGroupedActiveGoals();
	bool done = false;
	switch (m_WaitType)
	{
	case WAIT_ALL:
		done = currentBlockCount == 0;
		break;
	case WAIT_ANY:
		done = currentBlockCount < m_BlockCount;
		break;
	case WAIT_ANY_2:
		done = currentBlockCount + 1 < m_BlockCount;
		break;
	default:
		done = true;
	}
	if (done)
		pPipeUser->ClearGroupedActiveGoals();

	return done ? eGOR_DONE : eGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPWait::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPWait");
	{
		ser.EnumValue("m_WaitType", m_WaitType, WAIT_ALL, WAIT_LAST);
		ser.Value("m_BlockCount", m_BlockCount);
		ser.EndGroup();
	}
}

#if 0
// deprecated and won't compile at all...

//===================================================================
// COPProximity
//===================================================================
COPProximity::COPProximity(float radius, const string& signalName, bool signalWhenDisabled, bool visibleTargetOnly) :
	m_radius(radius),
	m_signalName(signalName),
	m_signalWhenDisabled(signalWhenDisabled),
	m_visibleTargetOnly(visibleTargetOnly),
	m_triggered(false)
{
}

COPProximity::COPProximity(const XmlNodeRef& node) :
	m_radius(0.f),
	m_signalName(s_xml.GetMandatoryString(node, "signal")),
	m_signalWhenDisabled(s_xml.GetBool(node, "signalWhenDisabled")),
	m_visibleTargetOnly(s_xml.GetBool(node, "visibleTargetOnly")),
	m_triggered(false)
{
	s_xml.GetMandatory(node, "radius", m_radius);
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPProximity::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_triggered)
		return eGOR_SUCCEEDED;

	CAIObject* pTarget = (CAIObject*)pPipeUser->GetAttentionTarget();
	if (!pTarget)
	{
		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	CCCPOINT(COPProximity_Execute);

	if (!m_refProxObject.IsValid())
	{
		CCCPOINT(COPProximity_Execute_NotValid);

		m_refProxObject = GetWeakRef(pPipeUser->GetLastOpResult());
		if (!m_refProxObject.IsValid())
		{
			Reset(pPipeUser);
			return eGOR_FAILED;
		}
		if (m_radius < 0.0f)
			m_radius = m_refProxObject.GetAIObject()->GetRadius();
	}

	CAIObject* const pProxObject = m_refProxObject.GetAIObject();
	CPipeUser* const pTargetPipeuser = pProxObject->CastToCPipeUser();
	bool trigger = false;

	bool b3D = (pTargetPipeuser && pTargetPipeuser->IsUsing3DNavigation()) || (pTargetPipeuser && pTargetPipeuser->IsUsing3DNavigation());
	float distSqr = b3D ? Distance::Point_PointSq(pTarget->GetPos(), pProxObject->GetPos()) :
	                Distance::Point_Point2DSq(pTarget->GetPos(), pProxObject->GetPos());

	if (m_signalWhenDisabled && !pProxObject->IsEnabled())
		trigger = true;

	if (!m_visibleTargetOnly || (m_visibleTargetOnly && pPipeUser->GetAttentionTargetType() == AITARGET_VISUAL))
	{
		if (distSqr < sqr(m_radius))
			trigger = true;
	}

	if (trigger)
	{
		CCCPOINT(COPProximity_Execute_Trigger);

		AISignalExtraData* pData = new AISignalExtraData;
		pData->fValue = sqrtf(distSqr);
		pPipeUser->SetSignal(0, m_signalName.c_str(), pPipeUser->GetEntity(), pData);
		m_triggered = true;
		return eGOR_SUCCEEDED;
	}

	return eGOR_IN_PROGRESS;
}

//===================================================================
// Reset
//===================================================================
void COPProximity::Reset(CPipeUser* pPipeUser)
{
	m_triggered = false;
	m_refProxObject.Reset();
}

//===================================================================
// Serialize
//===================================================================
void COPProximity::Serialize(TSerialize ser)
{
	ser.BeginGroup("AICOPProximity");
	{
		ser.Value("m_radius", m_radius);
		ser.Value("m_triggered", m_triggered);
		ser.Value("m_signalName", m_signalName);
		ser.Value("m_signalWhenDisabled", m_signalWhenDisabled);
		ser.Value("m_visibleTargetOnly", m_visibleTargetOnly);

		m_refProxObject.Serialize(ser, "m_ref_ProxObject");
	}
	ser.EndGroup();
}

//===================================================================
// DebugDraw
//===================================================================
void COPProximity::DebugDraw(CPipeUser* pPipeUser) const
{
	CDebugDrawContext dc;

	const CAIObject* const pProxObject = m_refProxObject.GetAIObject();
	if (pProxObject)
	{
		dc->DrawWireSphere(pProxObject->GetPos(), m_radius, ColorB(255, 255, 255));

		CPuppet* pOperandPuppet = pPipeUser->CastToCPuppet();
		if (!pOperandPuppet)
			return;
		CAIObject* pTarget = (CAIObject*)pPipeUser->GetAttentionTarget();
		if (pTarget)
		{
			Vec3 delta = pTarget->GetPos() - pProxObject->GetPos();
			delta.NormalizeSafe();
			delta *= m_radius;
			ColorB color;
			if (pPipeUser->GetAttentionTargetType() == AITARGET_VISUAL)
				color.Set(255, 255, 255, 255);
			else
				color.Set(255, 255, 255, 128);
			dc->DrawSphere(pProxObject->GetPos() + delta, 0.25f, color);
			dc->DrawLine(pProxObject->GetPos() + delta, color, pTarget->GetPos(), color);
		}
	}
}

//===================================================================
// COPMoveTowards
//===================================================================
COPMoveTowards::COPMoveTowards(float distance, float duration) :
	m_distance(distance),
	m_duration(duration),
	m_pPathfindDirective(0),
	m_pTraceDirective(0),
	m_looseAttentionId(0),
	m_moveDist(0),
	m_moveSearchRange(0),
	m_moveStart(0, 0, 0),
	m_moveEnd(0, 0, 0)
{
}

COPMoveTowards::COPMoveTowards(const XmlNodeRef& node) :
	m_distance(0.f),
	m_duration(0.f),
	m_pPathfindDirective(0),
	m_pTraceDirective(0),
	m_looseAttentionId(0),
	m_moveDist(0.f),
	m_moveSearchRange(0.f),
	m_moveStart(ZERO),
	m_moveEnd(ZERO)
{
	if (node->getAttr("time", m_duration))
	{
		m_duration = fabsf(m_duration);
	}
	else
	{
		s_xml.GetMandatory(node, "distance", m_distance);
	}

	float variation = 0.f;
	node->getAttr("variation", variation);

	// Random variation on the end distance.
	if (variation > 0.01f)
	{
		float u = cry_random(0, 9) / 9.0f;
		m_distance = (m_distance > 0.0f) ? max(0.f, m_distance - u * variation)
		             : min(0.f, m_distance + u * variation);
	}
}

//===================================================================
// Reset
//===================================================================
void COPMoveTowards::Reset(CPipeUser* pPipeUser)
{
	ResetNavigation(pPipeUser);
}

//===================================================================
// ResetNavigation
//===================================================================
void COPMoveTowards::ResetNavigation(CPipeUser* pPipeUser)
{
	if (m_pPathfindDirective)
		delete m_pPathfindDirective;
	m_pPathfindDirective = 0;
	if (m_pTraceDirective)
		delete m_pTraceDirective;
	m_pTraceDirective = 0;

	if (pPipeUser)
	{
		pPipeUser->CancelRequestedPath(false);
		pPipeUser->ClearPath("COPBackoff::Reset m_Path");
	}

	if (m_looseAttentionId)
	{
		if (pPipeUser)
			pPipeUser->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
		m_looseAttentionId = 0;
	}
}

//===================================================================
// Execute
//===================================================================
void COPMoveTowards::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pPipeUser, false);
}

//===================================================================
// GetEndDistance
//===================================================================
float COPMoveTowards::GetEndDistance(CPipeUser* pPipeUser) const
{
	if (m_duration > 0.0f)
	{
		float normalSpeed, minSpeed, maxSpeed;
		pPipeUser->GetMovementSpeedRange(pPipeUser->m_State.fMovementUrgency, false, normalSpeed, minSpeed, maxSpeed);
		//		float normalSpeed = pPipeUser->GetNormalMovementSpeed(pPipeUser->m_State.fMovementUrgency, false);
		if (normalSpeed > 0.0f)
			return normalSpeed * m_duration;
	}
	return m_distance;
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPMoveTowards::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPMoveTowards_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!m_pPathfindDirective)
	{
		CAIObject* pTarget = pPipeUser->m_refLastOpResult.GetAIObject();
		if (!pTarget)
		{
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		Vec3 operandPos = pPipeUser->GetPhysicsPos();
		Vec3 targetPos = pTarget->GetPhysicsPos();

		float moveDist = GetEndDistance(pPipeUser);
		float searchRange = Distance::Point_Point(operandPos, targetPos);
		//		moveDist = min(moveDist, searchRange);
		searchRange = max(searchRange, moveDist);
		searchRange *= 2.0f;

		// start pathfinding
		pPipeUser->CancelRequestedPath(false);
		m_pPathfindDirective = new COPPathFind("MoveTowards", pTarget, std::numeric_limits<float>::max(),
		                                       -moveDist, searchRange);
		pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;

		// Store params for debug drawing.
		m_moveStart = operandPos;
		m_moveEnd = targetPos;
		m_moveDist = moveDist;
		m_moveSearchRange = searchRange;

		if (m_pPathfindDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS)
		{
			if (pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
			{
				ResetNavigation(pPipeUser);
				return eGOR_FAILED;
			}
		}
		return eGOR_IN_PROGRESS;
	}

	if (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND)
	{
		if (pPipeUser->m_Path.GetPath().size() == 1)
		{
			AIWarning("COPMoveTowards::Entity %s Path has only one point.", GetNameSafe(pPipeUser));
			return eGOR_FAILED;
		}

		if (!m_pTraceDirective)
		{
			m_pTraceDirective = new COPTrace(false, defaultTraceEndAccuracy);
			//			pPipeUser->m_Path.GetParams().precalculatedPath = true; // prevent path regeneration
		}

		// keep tracing
		EGoalOpResult done = m_pTraceDirective->Execute(pPipeUser);
		if (done != eGOR_IN_PROGRESS)
		{
			Reset(pPipeUser);
			return done;
		}
		// If this goal gets reseted during m_pTraceDirective->Execute it means that
		// a smart object was used for navigation which inserts a goal pipe which
		// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
		// we should just report that this goal pipe isn't finished since it will be
		// reexecuted after finishing the inserted goal pipe
		if (!m_pTraceDirective)
			return eGOR_IN_PROGRESS;
	}
	else if (pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
	{
		Reset(pPipeUser);
		return eGOR_FAILED;
	}
	else
	{
		m_pPathfindDirective->Execute(pPipeUser);
	}

	return eGOR_IN_PROGRESS;
}

//===================================================================
// Serialize
//===================================================================
void COPMoveTowards::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPMoveTowards");
	{
		ser.Value("m_distance", m_distance);
		ser.Value("m_duration", m_duration);
		ser.Value("m_moveStart", m_moveStart);
		ser.Value("m_moveEnd", m_moveEnd);
		ser.Value("m_moveDist", m_moveDist);
		ser.Value("m_moveSearchRange", m_moveSearchRange);

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if (ser.BeginOptionalGroup("PathFindDirective", m_pPathfindDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pPathfindDirective->Serialize(ser, objectTracker);
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
			SAFE_DELETE(m_pPathfindDirective);
			if (ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathfindDirective = new COPPathFind("");
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
	}
	ser.EndGroup();
}

//===================================================================
// DebugDraw
//===================================================================
void COPMoveTowards::DebugDraw(CPipeUser* pPipeUser) const
{
	Vec3 dir(m_moveEnd - m_moveStart);
	dir.Normalize();

	Vec3 start = m_moveStart;
	Vec3 end = m_moveEnd;
	start.z += 0.5f;
	end.z += 0.5f;

	CDebugDrawContext dc;

	dc->DrawRangeArc(start, dir, DEG2RAD(60.0f), m_moveDist, m_moveDist - 0.5f, ColorB(255, 255, 255, 16), ColorB(255, 255, 255), true);
	dc->DrawRangeArc(start, dir, DEG2RAD(60.0f), m_moveSearchRange, 0.2f, ColorB(255, 0, 0, 128), ColorB(255, 0, 0), true);

	dc->DrawLine(start, ColorB(255, 255, 255), end, ColorB(255, 255, 255));
}

//====================================================================
// COPDodge
//====================================================================
COPDodge::COPDodge(float distance, bool useLastOpAsBackup) :
	m_distance(distance),
	m_useLastOpAsBackup(useLastOpAsBackup),
	m_notMovingTime(0.0f)
{
	m_pTraceDirective = 0;
	m_basis.SetIdentity();
	m_targetPos.zero();
	m_targetView.zero();
}

COPDodge::COPDodge(const XmlNodeRef& node) :
	m_distance(0),
	m_useLastOpAsBackup(s_xml.GetBool(node, "useLastOpAsBackup")),
	m_endAccuracy(0.f),
	m_notMovingTime(0.0f),
	m_pTraceDirective(0),
	m_targetPos(ZERO),
	m_targetView(ZERO)
{
	s_xml.GetMandatory(node, "radius", m_distance);

	m_basis.SetIdentity();
}

//====================================================================
// Reset
//====================================================================
void COPDodge::Reset(CPipeUser* pPipeUser)
{
	delete m_pTraceDirective;
	m_pTraceDirective = 0;
	m_notMovingTime = 0;
	m_basis.SetIdentity();
	m_targetPos.zero();
	m_targetView.zero();
	if (pPipeUser)
		pPipeUser->ClearPath("COPDodge::Reset");
}

//====================================================================
// Serialize
//====================================================================
void COPDodge::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPDodge");
	{
		ser.Value("m_distance", m_distance);
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);
		ser.Value("m_lastTime", m_lastTime);
		ser.Value("m_notMovingTime", m_notMovingTime);
		ser.Value("m_endAccuracy", m_endAccuracy);
		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				PREFAST_SUPPRESS_WARNING(6011) m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if (ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(false, m_endAccuracy);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}
}

//====================================================================
// CheckSegmentAgainstAvoidPos
//====================================================================
bool COPDodge::OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos)
{
	Lineseg movement(from, to);
	float t;
	for (unsigned i = 0, ni = avoidPos.size(); i < ni; ++i)
	{
		const float radSq = sqr(avoidPos[i].second + rad);
		if (Distance::Point_LinesegSq(avoidPos[i].first, movement, t) < radSq)
			return true;
	}
	return false;
}

//====================================================================
// GetNearestActors
//====================================================================
void COPDodge::GetNearestActors(CAIActor* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions)
{
	const float radiusSq = sqr(radius);
	const CAISystem::AIActorSet& enabledAIActorsSet = GetAISystem()->GetEnabledAIActorSet();
	for (CAISystem::AIActorSet::const_iterator it = enabledAIActorsSet.begin(), itend = enabledAIActorsSet.end(); it != itend; ++it)
	{
		CAIActor* pAIActor = it->GetAIObject();
		if (!pAIActor || pAIActor == pSelf)
			continue;
		Vec3 delta = pAIActor->GetPhysicsPos() - pos;
		if (delta.GetLengthSquared() < radiusSq)
		{
			const float passRadius = pAIActor->GetParameters().m_fPassRadius;
			positions.push_back(std::make_pair(pAIActor->GetPos(), passRadius));

			if (CPipeUser* pPipeUser = pAIActor->CastToCPipeUser())
			{
				const Vec3& vPipeUserLastPathPos = pPipeUser->m_Path.GetLastPathPos();
				if (Distance::Point_Point2DSq(vPipeUserLastPathPos, pos) < radiusSq)
				{
					positions.push_back(std::make_pair(vPipeUserLastPathPos, passRadius));
				}
			}
		}
	}
}

//====================================================================
// Execute
//====================================================================
EGoalOpResult COPDodge::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPDodge_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!m_pTraceDirective)
	{
		CRY_PROFILE_REGION(PROFILE_AI, "Dodge/CalculatePathTree");

		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		CAIObject* pLastOpResult = pPipeUser->m_refLastOpResult.GetAIObject();
		if (!pTarget && m_useLastOpAsBackup)
			pTarget = pLastOpResult;

		if (!pTarget)
		{
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		Vec3 center = pPipeUser->GetPhysicsPos();
		m_targetPos = pTarget->GetPos();
		m_targetView = pTarget->GetViewDir();

		Vec3 dir = m_targetPos - center;
		dir.Normalize();
		m_basis.SetRotationVDir(dir);

		IAISystem::tNavCapMask navCap = pPipeUser->m_movementAbility.pathfindingProperties.navCapMask;
		const float passRadius = pPipeUser->GetParameters().m_fPassRadius;

		int nBuildingID;
		IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(center, nBuildingID, navCap);

		m_avoidPos.clear();
		GetNearestActors(pPipeUser->CastToCPuppet(), pPipeUser->GetPhysicsPos(), m_distance + 2.0f, m_avoidPos);

		// Calc path.
		const int iters = 7;
		Vec3 pos[iters];

		for (int i = 0; i < iters; ++i)
		{
			float a = ((float)i / (float)iters) * gf_PI2;
			float x = cosf(a);
			float y = sinf(a);
			float d = m_distance * cry_random(0.9f, 1.1f);

			Vec3 delta = x * m_basis.GetColumn0() + y * m_basis.GetColumn2() + 0.5f * m_basis.GetColumn1();
			delta.Normalize();
			pos[i] = center + delta * d;

			Vec3 hitPos;
			float hitDist;
			if (IntersectSweptSphere(&hitPos, hitDist, Lineseg(center, pos[i]), passRadius * 1.1f, AICE_ALL))
				pos[i] = center + delta * hitDist;

			m_DEBUG_testSegments.push_back(center);
			m_DEBUG_testSegments.push_back(pos[i]);
		}

		//Lineseg	targetLOS(m_targetPos, m_targetPos + m_targetView * Distance::Point_Point(center, m_targetPos)*2.0f);
		//float t;
		//Distance::Point_Lineseg(center, targetLOS, t);
		//Vec3 nearestPointOnTargetView = targetLOS.GetPoint(t);

		float bestDist = 0.0f;
		int bestIdx = -1;
		for (int i = 0; i < iters; ++i)
		{
			float d = Distance::Point_PointSq(center, pos[i]);
			if (d > bestDist)
			{
				if (!OverlapSegmentAgainstAvoidPos(center, pos[i], passRadius, m_avoidPos))
				{
					bestDist = d;
					bestIdx = i;
				}
			}
		}

		if (bestIdx == -1)
		{
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		m_bestPos = pos[bestIdx];

		// Set the path
		pPipeUser->CancelRequestedPath(false);
		pPipeUser->ClearPath("COPDodge::Execute generate path");

		SNavPathParams params(center, m_bestPos);
		params.precalculatedPath = true;
		pPipeUser->m_Path.SetParams(params);

		pPipeUser->m_nPathDecision = PATHFINDER_PATHFOUND;
		pPipeUser->m_Path.PushBack(PathPointDescriptor(navType, center));
		pPipeUser->m_Path.PushBack(PathPointDescriptor(navType, m_bestPos));

		pPipeUser->m_OrigPath = pPipeUser->m_Path;

		if (CPuppet* pPuppet = pPipeUser->CastToCPuppet())
		{
			// Update adaptive urgency control before the path gets processed further
			pPuppet->AdjustMovementUrgency(pPuppet->m_State.fMovementUrgency,
			                               pPuppet->m_Path.GetPathLength(!pPuppet->IsUsing3DNavigation()));
		}

		m_pTraceDirective = new COPTrace(false, m_endAccuracy);
		m_lastTime = GetAISystem()->GetFrameStartTime();
	}

	AIAssert(m_pTraceDirective);
	EGoalOpResult done = m_pTraceDirective->Execute(pPipeUser);
	if (m_pTraceDirective == NULL)
		return eGOR_IN_PROGRESS;

	AIAssert(m_pTraceDirective);

	// HACK: The following code tries to put a lost or stuck agent back on track.
	// It works together with a piece of in ExecuteDry which tracks the speed relative
	// to the requested speed and if it drops dramatically for certain time, this code
	// will trigger and try to move the agent back on the path. [Mikko]

	float timeout = 1.5f;
	if (pPipeUser->GetType() == AIOBJECT_VEHICLE)
		timeout = 7.0f;

	if (m_notMovingTime > timeout)
	{
		// Stuck or lost, move to the nearest point on path.
		AIWarning("COPDodge::Entity %s has not been moving fast enough for %.1fs it might be stuck, abort.", GetNameSafe(pPipeUser), m_notMovingTime);
		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	if (done != eGOR_IN_PROGRESS)
	{
		Reset(pPipeUser);
		return done;
	}
	if (!m_pTraceDirective)
	{
		Reset(pPipeUser);
		return eGOR_FAILED;
	}

	return eGOR_IN_PROGRESS;
}

//====================================================================
// ExecuteDry
//====================================================================
void COPDodge::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CAISystem* pSystem = GetAISystem();

	if (m_pTraceDirective)
	{
		m_pTraceDirective->ExecuteTrace(pPipeUser, false);

		// HACK: The following code together with some logic in the execute tries to keep track
		// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
		CTimeValue time(pSystem->GetFrameStartTime());
		float dt((time - m_lastTime).GetSeconds());
		float speed = pPipeUser->GetVelocity().GetLength();
		float desiredSpeed = pPipeUser->m_State.fDesiredSpeed;
		if (desiredSpeed > 0.0f)
		{
			float ratio = clamp_tpl(speed / desiredSpeed, 0.0f, 1.0f);
			if (ratio < 0.1f)
				m_notMovingTime += dt;
			else
				m_notMovingTime -= dt;
			if (m_notMovingTime < 0.0f)
				m_notMovingTime = 0.0f;
		}
		m_lastTime = time;
	}
}

//===================================================================
// DebugDraw
//===================================================================
void COPDodge::DebugDraw(CPipeUser* pPipeUser) const
{
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pPipeUser);

	const float passRadius = pPipeUser->GetParameters().m_fPassRadius;

	CDebugDrawContext dc;

	for (unsigned i = 0, ni = m_DEBUG_testSegments.size(); i < ni; i += 2)
	{
		dc->DrawLine(m_DEBUG_testSegments[i], ColorB(255, 255, 255), m_DEBUG_testSegments[i + 1], ColorB(255, 255, 255));
		dc->DrawWireSphere(m_DEBUG_testSegments[i + 1], passRadius * 1.1f, ColorB(255, 255, 255));
	}

	dc->DrawSphere(m_bestPos, 0.2f, ColorB(255, 0, 0));

	for (unsigned i = 0, ni = m_avoidPos.size(); i < ni; ++i)
		dc->DrawWireSphere(m_avoidPos[i].first, m_avoidPos[i].second, ColorB(255, 0, 0));
}

//
//-------------------------------------------------------------------------------------------------------------
COPCompanionStick::COPCompanionStick(float fNotReactingDistance, float fForcedMoveDistance, float fLeaderInfluenceRange)
	: m_vLastUsedTargetPos(0, 0, 0),
	m_vCurrentTargetPos(0, 0, 0),
	m_fMinEndDistance(0.f),
	m_fMaxEndDistance(0.f),
	m_pTraceDirective(NULL),
	m_pPathfindDirective(NULL),
	m_bFirstExec(true),
	m_fPathLenLimit(20),
	m_fEndAccuracy(0.01f),
	m_fMoveWillingness(0.0f),
	m_fLeaderInfluenceRange(5.0f),
	m_fNotReactingDistance(1.0f),
	m_fForcedMoveDistance(2.0f)
{
	m_fSteerDistanceSqr = 100;//fSteerDistance * fSteerDistance;

	if (fNotReactingDistance > FLT_EPSILON)
		m_fNotReactingDistance = fNotReactingDistance;

	if (fForcedMoveDistance > FLT_EPSILON)
		m_fForcedMoveDistance = fForcedMoveDistance;

	if (fLeaderInfluenceRange > FLT_EPSILON)
		m_fLeaderInfluenceRange = fLeaderInfluenceRange;
}

//
//-------------------------------------------------------------------------------------------------------------
COPCompanionStick::COPCompanionStick(const XmlNodeRef& node)
	: m_vLastUsedTargetPos(ZERO),
	m_vCurrentTargetPos(ZERO),
	m_fMinEndDistance(0.f),
	m_fMaxEndDistance(0.f),
	m_pTraceDirective(0),
	m_pPathfindDirective(0),
	m_bFirstExec(true),
	m_fPathLenLimit(20),
	m_fSteerDistanceSqr(100.f),
	m_fEndAccuracy(0.01f),
	m_fMoveWillingness(0.f),
	m_fLeaderInfluenceRange(5.f),
	m_fNotReactingDistance(1.f),
	m_fForcedMoveDistance(2.f)
{
	s_xml.GetMandatory(node, "leaderInfluenceRange", m_fLeaderInfluenceRange);
	s_xml.GetMandatory(node, "notReactingDistance", m_fNotReactingDistance);
	s_xml.GetMandatory(node, "forcedMoveDistance", m_fForcedMoveDistance);
}

//
//-------------------------------------------------------------------------------------------------------------
COPCompanionStick::~COPCompanionStick()
{
	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPSteer::~COPSteer %p", this);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPCompanionStick::Execute(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (pPipeUser->m_nPathDecision == PATHFINDER_STILLFINDING && m_pPathfindDirective != NULL)
	{
		m_pPathfindDirective->Execute(pPipeUser);
		return eGOR_IN_PROGRESS;
	}

	CPuppet* pPuppet = pPipeUser->CastToCPuppet();

	if (m_pPosDummy == NULL)
	{
		pPipeUser->SetSignal(0, "OnSteerFailed", pPipeUser->GetEntity(), NULL, 0);
		AILogAlways("COPSteer::Execute - Error - No point to steer to: %s", GetNameSafe(pPipeUser));

		return eGOR_FAILED;
	}

	if (pPuppet->GetPos().GetSquaredDistance2D(m_pPosDummy->GetPos()) < 0.3f)
	{
		return eGOR_SUCCEEDED;
	}
	else
	{
		CAIObject* pTarget = m_pPosDummy;

		bool bForceReturnPartialPath = false;

		//if (pPipeUser->m_nPathDecision != PATHFINDER_PATHFOUND) m_fLastTime = -1.0f;

		// Check for unsuitable agents
		if (pPipeUser->GetSubType() == CAIObject::STP_HELI
		    || !pPipeUser->m_movementAbility.bUsePathfinder)
		{
			AILogAlways("COPSteer::Execute - Error - Unsuitable agent: %s", GetNameSafe(pPipeUser));
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		if (pTarget->GetSubType() == CAIObject::STP_ANIM_TARGET)
		{
			AILogAlways("COPSteer::Execute - Error - Unsuitable target: %s", GetNameSafe(pPipeUser));
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		// Move strictly to the target point.
		// MTJ: Not sure this should stay
		pPipeUser->m_bPathfinderConsidersPathTargetDirection = true;

		pPipeUser->SetInCover(false);
		pPipeUser->m_CurrentHideObject.Invalidate();

		// If we have a hidespot, that's our target
		// Otherwise it's the attention target
		if (m_pPosDummy)
			pTarget = m_pPosDummy;

		switch (m_pPathfindDirective ? pPipeUser->m_nPathDecision : PATHFINDER_MAXVALUE)
		{
		case PATHFINDER_MAXVALUE:
			{
				// Generate path to target
				float endTol = (bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy);
				m_pPathfindDirective = new COPPathFind("", pTarget, endTol, 0.0f, m_fPathLenLimit);
				pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;
				if (m_pPathfindDirective->Execute(pPipeUser) && pPipeUser->m_nPathDecision == PATHFINDER_NOPATH)
				{
					// If special nopath signal is specified, send the signal.
					pPipeUser->m_State.vMoveDir.zero();
					return eGOR_FAILED;
				}
				break;
			}
		case PATHFINDER_PATHFOUND:
			{
				// if we have a path, trace it
				if (pPipeUser->m_OrigPath.Empty() || pPipeUser->m_OrigPath.GetPathLength(true) < 0.01f)
				{
					AILogAlways("COPSteer::Execute - Origpath is empty: %s. Regenerating.", GetNameSafe(pPipeUser));
					/*RegeneratePath( pPipeUser, m_pPosDummy->GetPos() );
					   pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;*/
					return eGOR_FAILED;
				}
				else
				{
					// If we need a tracer, create one
					if (!m_pTraceDirective)
					{
						// We create another goalop to achieve this (sucky approach)
						bool bExact = true;
						bForceReturnPartialPath = false;

						m_pTraceDirective = new COPTrace(bExact, m_fEndAccuracy, bForceReturnPartialPath);
						m_pTraceDirective->SetControlSpeed(false);
						TPathPoints::const_reference lastPathNode = pPipeUser->m_OrigPath.GetPath().back();
						Vec3 vLastPos = lastPathNode.vPos;
						Vec3 vRequestedLastNodePos = pPipeUser->m_Path.GetParams().end;
						float dist = Distance::Point_Point(vLastPos, vRequestedLastNodePos);

						// MTJ: What is this for?
						if (lastPathNode.navType != IAISystem::NAV_SMARTOBJECT && dist > C_MaxDistanceForPathOffset)// && pPipeUser->m_Path.GetPath().size() == 1 )
						{
							AISignalExtraData* pData = new AISignalExtraData;
							pData->fValue = dist;// - m_fMinEndDistance;
							pPipeUser->SetSignal(0, "OnEndPathOffset", pPipeUser->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnEndPathOffset);
						}
						else
						{
							pPipeUser->SetSignal(0, "OnPathFound", NULL, 0, gAIEnv.SignalCRCs.m_nOnPathFound);
						}
					}

					// Keep tracing - previous code will stop us when close enough
					// MTJ: What previous code?
					bool bDone = (m_pTraceDirective->Execute(pPipeUser) != eGOR_IN_PROGRESS);

					if (bDone)
					{
						pPipeUser->SetSignal(0, "OnFormationPointReached", pPipeUser->GetEntity(), NULL, 0); // MTJ: Provide CRC
						return eGOR_SUCCEEDED;
					}
				}

				break;
			}
		case PATHFINDER_NOPATH:
			{
				pPipeUser->SetSignal(0, "OnGetToFormationPointFailed", pPipeUser->GetEntity(), NULL, 0); // MTJ: Provide CRC
				return eGOR_FAILED;
			}
		default:
			{
				// MTJ: Still pathfinding I guess?
				m_pPathfindDirective->Execute(pPipeUser);
				break;
			}
		}
	}

	// Run me again next tick
	return eGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCompanionStick::ExecuteDry(CPipeUser* pPipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (pPipeUser->m_nPathDecision == PATHFINDER_STILLFINDING && m_pPathfindDirective != NULL)
	{
		m_pPathfindDirective->Execute(pPipeUser);
		return;
	}

	if (m_pTraceDirective && m_pPosDummy)
	{
		bool bTargetDirty = false;

		//if( pPipeUser->GetPos().GetSquaredDistance2D( m_pPosDummy->GetPos() ) < m_fSteerDistanceSqr )
		//{

		//}
		//else
		//{
		Vec3 targetVector;
		if (pPipeUser->m_nPathDecision == PATHFINDER_PATHFOUND /*&& m_bForceReturnPartialPath*/)
			targetVector = (pPipeUser->m_Path.GetLastPathPos() - pPipeUser->GetPhysicsPos());
		else
			targetVector = (m_pPosDummy->GetPhysicsPos() - pPipeUser->GetPhysicsPos());

		{
			Vec3 targetPos = m_pPosDummy->GetPos();
			float targetMoveDist = (m_vLastUsedTargetPos - targetPos).GetLength2D();

			// Use the stored destination point instead of the last path node since the path may be cut because of navSO.
			Vec3 pathEnd = pPipeUser->m_PathDestinationPos;
			Vec3 dir(pathEnd - pPipeUser->GetPos());

			dir.z = 0;
			dir.NormalizeSafe();

			Vec3 dirToTarget(targetPos - pathEnd);

			dirToTarget.z = 0;
			dirToTarget.NormalizeSafe();

			float regenDist = 0.3f;
			if (dirToTarget.Dot(dir) < cosf(DEG2RAD(8.0f)))
				regenDist *= 5.0f;

			if (targetMoveDist > regenDist)
				bTargetDirty = true;

			// when near the path end force more frequent updates
			//float pathDistLeft = pPipeUser->m_Path.GetPathLength(false);
			//float pathEndError = (pathEnd - targetPos).GetLength2D();

			//// TODO/HACK! [Mikko] This prevent the path to regenerated every frame in some special cases in Crysis Core level
			//// where quite a few behaviors are sticking to a long distance (7-10m).
			//// The whole stick regeneration logic is flawed mostly because pPipeUser->m_PathDestinationPos is not always
			//// the actual target position. The pathfinder may adjust the end location and not keep the requested end pos
			//// if the target is not reachable. I'm sure there are other really nasty cases about this path invalidation logic too.
			//const GraphNode* pLastNode = gAIEnv.pGraph->GetNode(pPipeUser->m_lastNavNodeIndex);
			//if (pLastNode && pLastNode->navType == IAISystem::NAV_VOLUME)
			//	pathEndError = max(0.0f, pathEndError - pathDistLeft/*GetEndDistance(pPipeUser)*/);

			//if (pathEndError > 0.1f && pathDistLeft < 2.0f * pathEndError)
			//{
			//	//bTargetDirty = true;
			//}
		}

		if (!pPipeUser->m_movementAbility.b3DMove)
			targetVector.z = 0.0f;

		const Vec3& vDummyPos = m_pPosDummy->GetPos();
		m_vCurrentTargetPos = vDummyPos;

		if (/*pPipeUser->m_nPathDecision != PATHFINDER_STILLFINDING && */ bTargetDirty)
			RegeneratePath(pPipeUser, vDummyPos);
		else
			m_pTraceDirective->ExecuteTrace(pPipeUser, false);

		AdjustSpeed(pPipeUser);

	}
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCompanionStick::Reset(CPipeUser* pPipeUser)
{
	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPSteer::Reset %s", GetNameSafe(pPipeUser));

	SAFE_DELETE(m_pPathfindDirective);
	SAFE_DELETE(m_pTraceDirective);

	m_bNeedHidespot = true;
	m_vLastUsedTargetPos.zero();
	m_vCurrentTargetPos.zero();

	if (pPipeUser)
	{
		pPipeUser->ClearPath("COPSteer::Reset m_Path");
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void COPCompanionStick::Serialize(TSerialize ser)
{

}

//
//-------------------------------------------------------------------------------------------------------------
void COPCompanionStick::DebugDraw(CPipeUser* pPipeUser) const
{
	CPuppet* pPuppet = pPipeUser->CastToCPuppet();

	float normalSpeed, minSpeed, maxSpeed;
	pPuppet->GetMovementSpeedRange(pPuppet->m_State.fMovementUrgency, pPuppet->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

	float fMaxSpeed = 6.0f;//maxSpeed;

	// if companion out of camera increase its max speed
	CCamera& cam = GetISystem()->GetViewCamera();
	Sphere s(pPuppet->GetPos(), pPuppet->GetRadius() * 2.0f);
	if (cam.IsSphereVisible_FH(s) == CULL_EXCLUSION)
	{
		fMaxSpeed *= 2.0f;
	}

	//float fDesiredSpeed = 3.0f;
	float fDistanceToLeaderRatio = 1.0f; // 1 - means far, 0 - means very close
	float fLeaderSpeed = 0.0f;
	float fDistanceToTarget = 0.0f;
	Vec3 vLabelPos = m_vLastUsedTargetPos;

	// tune speed depending on target distance
	{
		fDistanceToTarget = pPuppet->GetPos().GetDistance(m_vLastUsedTargetPos);
		fDistanceToLeaderRatio = fDistanceToTarget / 10;
		if (fDistanceToLeaderRatio > 1.0f)
			fDistanceToLeaderRatio = 1.0f;
		//fMaxSpeed *= fDistToTargetRation;
	}

	CDebugDrawContext dc;

	dc->Draw3dLabel(vLabelPos, 1.0f, "\nSpeed %.2f\nDesired Speed %.2f\nMaxSpeed %.f\ndestDist %.2f\nLeader speed %.2f\n\nWillingness %.3f"
	                , pPuppet->CastToCAIActor()->GetVelocity().GetLength()
	                , pPuppet->m_State.fDesiredSpeed
	                , fMaxSpeed
	                , fDistanceToTarget
	                , fLeaderSpeed
	                , m_fMoveWillingness
	                );

	dc->DrawSphere(m_vLastUsedTargetPos, 0.55f, ColorB(0, 128, 255, 150));
}

//
//----------------------------------------------------------------------------------------------------------
void COPCompanionStick::RegeneratePath(CPipeUser* pPipeUser, const Vec3& destination)
{
	if (!pPipeUser)
		return;

	CTimeValue time = GetAISystem()->GetFrameStartTime();

	if ((time - m_fLastRegenTime).GetMilliSeconds() < 100.f)
		return;

	m_fLastRegenTime = time;

	if (gAIEnv.CVars.DebugPathFinding)
		AILogAlways("COPCompanionStick::RegeneratePath %s", GetNameSafe(pPipeUser));
	m_pPathfindDirective->Reset(pPipeUser);
	if (m_pTraceDirective)
		m_pTraceDirective->m_fEndAccuracy = m_fEndAccuracy;
	m_vLastUsedTargetPos = destination;
	pPipeUser->m_nPathDecision = PATHFINDER_STILLFINDING;

	const Vec3 opPos = pPipeUser->GetPhysicsPos();

	// Check for direct connection if distance is less than fixed value
	if (opPos.GetSquaredDistance(destination) < 2500.f)
	{
		int nbid;
		IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(opPos, nbid, pPipeUser->m_movementAbility.pathfindingProperties.navCapMask);
		CNavRegion* pRegion = gAIEnv.pNavigation->GetNavRegion(navType, gAIEnv.pGraph);
		if (pRegion)
		{
			Vec3 from = opPos;
			Vec3 to = destination;

			NavigationBlockers navBlocker;
			if (pRegion->CheckPassability(from, to, pPipeUser->GetParameters().m_fPassRadius, navBlocker, pPipeUser->GetMovementAbility().pathfindingProperties.navCapMask))
			{
				pPipeUser->ClearPath("COPCompanionStick::RegeneratePath m_Path");

				if (navType == IAISystem::NAV_TRIANGULAR)
				{
					// Make sure not to enter forbidden area.
					if (gAIEnv.pNavigation->IsPathForbidden(opPos, destination))
						return;
					if (gAIEnv.pNavigation->IsPointForbidden(destination, pPipeUser->GetParameters().m_fPassRadius))
						return;
				}

				PathPointDescriptor pt;
				pt.navType = navType;

				pt.vPos = from;
				pPipeUser->m_Path.PushBack(pt);

				float endDistance = 0.5f;//m_fStickDistance;
				if (fabsf(endDistance) > 0.0001f)
				{
					// Handle end distance.
					float dist;
					if (pPipeUser->IsUsing3DNavigation())
						dist = Distance::Point_Point(from, to);
					else
						dist = Distance::Point_Point2D(from, to);

					float d;
					if (endDistance > 0.0f)
						d = dist - endDistance;
					else
						d = -endDistance;
					float u = clamp_tpl(d / dist, 0.0001f, 1.0f);

					pt.vPos = from + u * (to - from);

					pPipeUser->m_Path.PushBack(pt);
				}
				else
				{
					pt.vPos = to;
					pPipeUser->m_Path.PushBack(pt);
				}

				pPipeUser->m_Path.SetParams(SNavPathParams(from, to, Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero, -1, false, endDistance, true));

				pPipeUser->m_OrigPath = pPipeUser->m_Path;
				pPipeUser->m_nPathDecision = PATHFINDER_PATHFOUND;
			}
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPCompanionStick::AdjustSpeed(CPipeUser* pPipeUser)
{
	CPuppet* pPuppet = pPipeUser->CastToCPuppet();

	float normalSpeed, minSpeed, maxSpeed;
	pPuppet->GetMovementSpeedRange(pPuppet->m_State.fMovementUrgency, pPuppet->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

	float fMaxSpeed = 6.0f;//maxSpeed;

	// if companion out of camera increase its max speed
	CCamera& cam = GetISystem()->GetViewCamera();
	Sphere s(pPuppet->GetPos(), pPuppet->GetRadius() * 2.0f);
	if (cam.IsSphereVisible_FH(s) == CULL_EXCLUSION)
	{
		fMaxSpeed *= 2.0f;
	}

	//float fDesiredSpeed = 3.0f;
	float fLeaderInfluence = 1.0f; // 1 - means far, 0 - means very close
	float fLeaderSpeed = 0.0f;
	float fDistanceToTarget = 0.0f;

	// match speed to group leader
	CGroupMember* pMember = gAIEnv.pGroupSystem->GetGroupMember(pPipeUser->CastToCAIActor()->GetEntityID());
	const CGroupMember* pLeader = pMember->GetLeader();
	if (pLeader != NULL)
	{
		const CAIActor* pActor = pLeader->GetActor();
		fLeaderSpeed = pActor->GetVelocity().GetLength2D();
	}

	// tune speed depending on target distance
	fDistanceToTarget = pPuppet->GetPos().GetDistance(m_vCurrentTargetPos);
	fLeaderInfluence = fDistanceToTarget / m_fLeaderInfluenceRange;
	if (fLeaderInfluence > 1.0f)
		fLeaderInfluence = 1.0f;

	// determine puppet's willingness to move:

	// if pipe user stopped close to his final destination (not the point that was used
	// to generate the path - m_vLastUsedTargetPos, which is important for moving targets)
	// than zero its willingness to move
	if (pPuppet->m_State.fDesiredSpeed < 0.01f && fDistanceToTarget < m_fNotReactingDistance)
	{
		m_fMoveWillingness = 0.0f;
	}
	// if pipe user is already moving then its willingness to move is set to 1
	else if (pPuppet->m_State.fDesiredSpeed > 0.0f)
	{
		m_fMoveWillingness = 1.0f;
	}
	else
	{
		// if destination is more than 2 meters away, than be willing to move
		if (fDistanceToTarget > m_fForcedMoveDistance)
			m_fMoveWillingness = 1.0f;
		else  // else be more willing with time.
			m_fMoveWillingness += GetAISystem()->GetFrameDeltaTime() * 0.2f * fDistanceToTarget;
	}

	if (m_fMoveWillingness < 1.0f)
	{
		pPuppet->m_State.fDesiredSpeed = 0.0f;
	}
	else
	{
		if (fDistanceToTarget > m_fLeaderInfluenceRange)
			pPuppet->m_State.fDesiredSpeed = fMaxSpeed;
		else if (fDistanceToTarget < 3.0f)
			pPuppet->m_State.fDesiredSpeed = max(1.0f, min(fMaxSpeed, fLeaderSpeed));
		else
			pPuppet->m_State.fDesiredSpeed = fMaxSpeed * fLeaderInfluence + fLeaderSpeed * (1.0f - fLeaderInfluence);
	}
}
#endif // 0

COPCommunication::COPCommunication(const char* commName, const char* channelName, const char* ordering, float expirity, float minSilence, bool ignoreSound, bool ignoreAnim)
	:
	m_ordering(SCommunicationRequest::Unordered)
	, m_expirity(expirity)
	, m_minSilence(minSilence)
	, m_ignoreSound(ignoreSound)
	, m_ignoreAnim(ignoreAnim)
	, m_bInitialized(false)
	, m_commFinished(false)
	, m_timeout(8.0f)
	, m_playID(0)
	, m_waitUntilFinished(true)
{
	m_commID = gAIEnv.pCommunicationManager->GetCommunicationID(commName);
	if (!gAIEnv.pCommunicationManager->GetCommunicationName(m_commID))
	{
		AIWarning("Unknown communication name '%s'", commName);
		m_commID = CommID(0);
	}

	m_channelID = gAIEnv.pCommunicationManager->GetChannelID(channelName);
	if (!m_channelID)
	{
		AIWarning("Unknown channel name '%s'", channelName);
	}

	if (!stricmp(ordering, "ordered"))
		m_ordering = SCommunicationRequest::Ordered;
	else if (!stricmp(ordering, "unordered"))
		m_ordering = SCommunicationRequest::Unordered;
	else
	{
		AIWarning("Invalid ordering '%s' for 'Communication'", ordering);
	}

}

COPCommunication::COPCommunication(const XmlNodeRef& node) :
	m_channelID(0)
	, m_commID(0)
	, m_ordering(SCommunicationRequest::Unordered)
	, m_expirity(0.0f)
	, m_minSilence(-1.0f)
	, m_ignoreSound(false)
	, m_ignoreAnim(false)
	, m_bInitialized(false)
	, m_commFinished(false)
	, m_timeout(8.0f)
	, m_playID(0)
	, m_waitUntilFinished(s_xml.GetBool(node, "waitUntilFinished", true))
{
	const char* commName;
	if (node->getAttr("name", &commName))
	{
		m_commID = gAIEnv.pCommunicationManager->GetCommunicationID(commName);
		if (!gAIEnv.pCommunicationManager->GetCommunicationName(m_commID))
		{
			AIWarning("Unknown communication name '%s'", commName);
			m_commID = CommID(0);
		}
	}
	else
	{
		AIWarning("Missing 'name' attribute for 'Communication' goal op");
	}

	const char* channelName;
	if (node->getAttr("channel", &channelName))
	{
		m_channelID = gAIEnv.pCommunicationManager->GetChannelID(channelName);
		if (!m_channelID)
		{
			AIWarning("Unknown channel name '%s'", channelName);
		}
	}
	else
	{
		AIWarning("Missing 'channel' attribute for 'Communication'");
	}

	if (node->haveAttr("timeout"))
		node->getAttr("timeout", m_timeout);

	if (node->haveAttr("expirity"))
		node->getAttr("expirity", m_expirity);

	if (node->haveAttr("minSilence"))
		node->getAttr("minSilence", m_minSilence);

	if (node->haveAttr("ignoreSound"))
		node->getAttr("ignoreSound", m_ignoreSound);

	if (node->haveAttr("ignoreAnim"))
		node->getAttr("ignoreAnim", m_ignoreAnim);
}

void COPCommunication::Reset(CPipeUser* pPipeUser)
{
	m_bInitialized = false;

	//If this goalop was listening, then remove the listener now
	if (m_playID)
	{
		gAIEnv.pCommunicationManager->RemoveInstanceListener(m_playID);
	}
}

void COPCommunication::OnCommunicationEvent(ICommunicationManager::ECommunicationEvent event, EntityId actorID,
                                            const CommPlayID& playID)
{
	switch (event)
	{
	case ICommunicationManager::CommunicationCancelled:
	case ICommunicationManager::CommunicationFinished:
	case ICommunicationManager::CommunicationExpired:
		{
			if (m_playID == playID)
			{
				m_commFinished = true;
			}
			else
			{
				AIWarning("Communication goal op received event for a communication not matching requested playID.");
			}
		}
		break;
	default:
		break;
	}
}

EGoalOpResult COPCommunication::Execute(CPipeUser* pPipeUser)
{
	CCCPOINT(COPCommunication_Execute)
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// first time	so start the communication
	if (!m_bInitialized)
	{
		m_bInitialized = true;

		m_startTime = GetAISystem()->GetFrameStartTime();

		CCommunicationManager& commManager = *gAIEnv.pCommunicationManager;

		SCommunicationRequest request;
		request.channelID = m_channelID;
		request.commID = m_commID;
		request.contextExpiry = m_expirity;
		request.minSilence = m_minSilence;
		request.ordering = m_ordering;
		request.skipCommAnimation = m_ignoreAnim;
		request.skipCommSound = m_ignoreSound;

		if (m_waitUntilFinished)
			request.eventListener = this;

		IAIActorProxy* proxy = pPipeUser->GetProxy();
		if (!proxy)
		{
			AIWarning("Communication goal op failed, target did not have a valid AI proxy.");
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		request.configID = commManager.GetConfigID(proxy->GetCommunicationConfigName());
		request.actorID = pPipeUser->GetEntityID();

		m_playID = commManager.PlayCommunication(request);

		if (!m_playID)
		{
			AIWarning("Communication goal op failed, manager could not start specified communication.");
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

		if (!m_waitUntilFinished)
		{
			return eGOR_SUCCEEDED;
		}
	}
	else
	{
		//If we've received a callback, then return success
		if (m_commFinished)
		{
			return eGOR_SUCCEEDED;
		}

		// Time out if angle threshold isn't reached for some time (to avoid deadlocks)
		float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
		if (elapsedTime > m_timeout)
		{
			AIWarning("Communication goal op timed out, never received return from manager. Waited %.2f seconds.", elapsedTime);
			Reset(pPipeUser);
			return eGOR_FAILED;
		}

	}
	return eGOR_IN_PROGRESS;
}

//
//----------------------------------------------------------------------------------------------------------
void COPCommunication::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPCommunication");
	ser.Value("m_commID", m_commID);
	ser.Value("m_channelID", m_channelID);
	ser.Value("m_expirity", m_expirity);
	ser.Value("m_minSilence", m_minSilence);
	ser.Value("m_ignoreSound", m_ignoreSound);
	ser.Value("m_ignoreAnim", m_ignoreAnim);

	int ordering = SCommunicationRequest::Unordered;

	if (ser.IsWriting())
	{
		ordering = (int) m_ordering;
	}
	ser.Value("m_ordering", ordering);
	if (ser.IsReading())
	{
		m_ordering = (SCommunicationRequest::EOrdering)ordering;
	}
	ser.EndGroup();
}

EGoalOpResult COPDummy::Execute(CPipeUser* pPipeUser)
{
	// This goalop does nothing except for returning AIGOR_DONE
	return eGOR_DONE;
}
