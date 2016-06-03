// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   LeaderAction.cpp

   Description: Class Implementation for CLeaderAction (and inherited)

   -------------------------------------------------------------------------
   History:
   - 15:11:2004   17:46 : Created by Luciano Morpurgo
   - 2:6:2005   15:23 : Kirill Bulatsev - serialization support added, related refactoring/cleanup


 *********************************************************************/

#include "StdAfx.h"
#include "LeaderAction.h"
#include "AILog.h"
#include "GoalOp.h"
#include "Puppet.h"
#include "Leader.h"
#include "AIPlayer.h"
#include "CAISystem.h"
#include <set>
#include <algorithm>
#include <utility>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>
#include <CryNetwork/ISerialize.h>
#include "AICollision.h"
#include "HideSpot.h"

const float CLeaderAction_Attack::m_CSearchDistance = 30.f;

const float CLeaderAction_Search::m_CSearchDistance = 15.f;
const uint32 CLeaderAction_Search::m_CCoverUnitProperties = UPR_COMBAT_GROUND;
const float CLeaderAction_Attack_SwitchPositions::m_fDefaultMinDistance = 6;

CLeaderAction::CLeaderAction()
	: m_pLeader(nullptr)
	, m_eActionType(LA_NONE)
	, m_eActionSubType(LAS_DEFAULT)
	, m_Priority(0)
	, m_unitProperties(UPR_ALL)
	, m_currentNavType(IAISystem::NAV_UNSET)
	, m_NavProperties()
{
}

CLeaderAction::CLeaderAction(CLeader* pLeader)
	: m_pLeader(pLeader)
	, m_eActionType(LA_NONE)
	, m_eActionSubType(LAS_DEFAULT)
	, m_Priority(0)
	, m_unitProperties(UPR_ALL)
	, m_currentNavType(IAISystem::NAV_UNSET)
	, m_NavProperties()
{
}

CLeaderAction::~CLeaderAction()
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
	{
		CUnitImg& curUnit = (*unitItr);
		curUnit.ClearPlanning(m_Priority);
		curUnit.ClearFlags();
	}
}

TUnitList& CLeaderAction::GetUnitsList() const
{
	return m_pLeader->GetAIGroup()->GetUnits();
}

//////////////////////////////////////////////////////////////////////////
bool CLeaderAction::IsUnitTooFar(const CUnitImg& tUnit, const Vec3& vPos) const
{
	// TODO: check if it was blocked by something or was leading the player
	// through the map.

	CAIActor* pUnitAIActor = tUnit.m_refUnit.GetAIObject();
	assert(pUnitAIActor);

	if (!pUnitAIActor->IsEnabled())
		return false;

	const float fMaxDist = 15.0f;
	if ((pUnitAIActor->GetPos() - vPos).GetLengthSquared() > (fMaxDist * fMaxDist))
		return true;

	CCCPOINT(CLeaderAction_IsUnitTooFar);

	return false;
}

void CLeaderAction::ClearUnitFlags()
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
	{
		CCCPOINT(CLeaderAction_ClearUnitFlags);

		CUnitImg& curUnit = (*unitItr);
		curUnit.ClearFlags();
		curUnit.m_Group = 0;
	}
}

bool CLeaderAction::IsUnitAvailable(const CUnitImg& unit) const
{
	CCCPOINT(CLeaderAction_IsUnitAvailable);

	return (unit.GetProperties() & m_unitProperties) && unit.m_refUnit.GetAIObject()->IsEnabled();
}

void CLeaderAction::DeadUnitNotify(CAIActor* pUnit)
{
	// by default for all Leader actions, the dead unit's remaining actions are deleted
	// and their blocked actions are unlocked (for each Unit Action's destructor)
	TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
	if (itUnit != m_pLeader->GetAIGroup()->GetUnits().end())
		itUnit->ClearPlanning();
}

void CLeaderAction::BusyUnitNotify(CUnitImg& BusyUnit)
{
}

int CLeaderAction::GetNumLiveUnits()  const
{
	// since dead unit were removed from this list just return its size
	return GetUnitsList().size();

}

CUnitImg* CLeaderAction::GetFormationOwnerImg() const
{
	// Return pointer to the unit image of the group formation owner.
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		if (m_pLeader->GetFormationOwner() == unit.m_refUnit)
		{
			CCCPOINT(CLeaderAction_GetFormationOwnerImg)
			return &unit;
		}
	}
	return 0;
}

void CLeaderAction::CheckNavType(CAIActor* pMember, bool bSignal)
{
	int building;
	IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(pMember->GetPos(), building, m_NavProperties);
	if (navType != m_currentNavType && bSignal)
	{
		IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
		pData->iValue = navType;
		GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 1, "OnNavTypeChanged", pMember, pData);

		CCCPOINT(CLeaderAction_CheckNavType);
	}
	m_currentNavType = navType;
}

void CLeaderAction::SetTeamNavProperties()
{
	m_NavProperties = 0;
	CWeakRef<CAIObject> refFormationOwner = m_pLeader->GetFormationOwner();
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_refUnit.GetAIObject();
		CAIActor* pAIActor = pUnit->CastToCAIActor();
		if (pAIActor && (unit.GetProperties() & m_unitProperties) && pUnit->IsEnabled() && unit.m_refUnit != refFormationOwner)
			m_NavProperties |= pAIActor->GetMovementAbility().pathfindingProperties.navCapMask;

		CCCPOINT(CLeaderAction_SetTeamNavProperties);
	}
}

void CLeaderAction::UpdateBeacon() const
{
	if (!m_pLeader->GetAIGroup()->GetLiveEnemyAveragePosition().IsZero())
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), m_pLeader->GetAIGroup()->GetLiveEnemyAveragePosition(), m_pLeader);
	else if (!m_pLeader->GetAIGroup()->GetEnemyAveragePosition().IsZero())
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), m_pLeader->GetAIGroup()->GetEnemyAveragePosition(), m_pLeader);
	Vec3 vEnemyDir = m_pLeader->GetAIGroup()->GetEnemyAverageDirection(true, false);
	if (!vEnemyDir.IsZero())
	{
		IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
		if (pBeacon)
		{
			pBeacon->SetEntityDir(vEnemyDir);
			pBeacon->SetMoveDir(vEnemyDir);
		}
		else
		{
			AIError("Unable to get beacon for group id %d", m_pLeader->GetGroupId());
		}

		CCCPOINT(CLeaderAction_UpdateBeacon);
	}
}

void CLeaderAction::CheckLeaderDistance() const
{
	// Check if some unit is too back
	CAIObject* pAILeader = m_pLeader->GetAssociation().GetAIObject();
	if (pAILeader)
	{
		TUnitList::iterator itEnd = GetUnitsList().end();
		for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		{
			CAIActor* pUnitActor = CastToCAIActorSafe((*unitItr).m_refUnit.GetAIObject());
			if (pUnitActor && pUnitActor->IsEnabled())
			{
				CCCPOINT(CLeaderAction_CheckLeaderDistance);

				static const float maxdist2 = 18 * 18;
				float dist2 = Distance::Point_PointSq(pUnitActor->GetPos(), pAILeader->GetPos());
				if (dist2 > maxdist2 && !unitItr->IsFar())
				{
					pUnitActor->SetSignal(1, "OnLeaderTooFar", (IEntity*)(pAILeader->GetAssociation().GetAIObject()), 0, gAIEnv.SignalCRCs.m_nOnLeaderTooFar);
					unitItr->SetFar();
				}
				else
					unitItr->ClearFar();
			}
		}
	}
}
//-----------------------------------------------------------
//----  SEARCH
//-----------------------------------------------------------

CLeaderAction_Search::CLeaderAction_Search(CLeader* pLeader, const LeaderActionParams& params)
	: CLeaderAction(pLeader)
	, m_timeRunning(0.0f)
{

	m_eActionType = LA_SEARCH;
	m_iSearchSpotAIObjectType = params.iValue ? params.iValue : AIANCHOR_COMBAT_HIDESPOT;
	m_bUseHideSpots = (m_iSearchSpotAIObjectType & AI_USE_HIDESPOTS) != 0;
	m_iSearchSpotAIObjectType &= ~AI_USE_HIDESPOTS;

	m_unitProperties = params.unitProperties;
	m_fSearchDistance = params.fSize > 0 ? params.fSize : m_CSearchDistance;

	// TO DO : use customized m_fSearchDistance (now there's a 15 hardcoded in GetEnclosing()

	Vec3 initPos = params.vPoint;

	if (params.subType == LAS_SEARCH_COVER)
	{
		int numFiringUnits = m_pLeader->GetAIGroup()->GetUnitCount(m_CCoverUnitProperties);
		m_iCoverUnitsLeft = numFiringUnits * 2 / 5;
		if (m_iCoverUnitsLeft == 0 && numFiringUnits > 0)
			m_iCoverUnitsLeft = 1;
	}
	else
		m_iCoverUnitsLeft = 0;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		unitItr->ClearHiding();

	m_vEnemyPos = ZERO;
	m_bInitialized = false;
	PopulateSearchSpotList(initPos);

}

void CLeaderAction_Search::PopulateSearchSpotList(Vec3& initPos)
{
	float maxdist2 = m_fSearchDistance * m_fSearchDistance;
	Vec3 avgPos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, m_unitProperties);
	if (initPos.IsZero())
		initPos = m_pLeader->GetMarkedPosition();
	if (initPos.IsZero())
		initPos = avgPos;

	Vec3 currentEnemyPos(m_pLeader->GetAIGroup()->GetEnemyAveragePosition());

	if (!currentEnemyPos.IsZero())
		m_vEnemyPos = currentEnemyPos;
	else
		m_vEnemyPos = initPos;

	if (m_vEnemyPos.IsZero())
	{
		IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
		if (pBeacon)
			m_vEnemyPos = pBeacon->GetPos();
		if (m_vEnemyPos.IsZero())
			return;
	}

	CCCPOINT(CLeaderAction_PopulateSearchSpotList);

	bool bCleared = false;
	if (m_iSearchSpotAIObjectType > 0)
	{
		for (AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, m_iSearchSpotAIObjectType)); it->GetObject(); it->Next())
		{
			IAIObject* pObjectIt = it->GetObject();
			const Vec3& objPos = pObjectIt->GetPos();
			const Vec3& objDir = pObjectIt->GetViewDir();
			if (!pObjectIt->IsEnabled()) // || pPipeUser->IsDevalued(pObjectIt))
				continue;
			float dist2 = Distance::Point_PointSq(m_vEnemyPos, objPos);
			if (dist2 < maxdist2)
			{
				if (!bCleared)
				{
					// clear the list only if some new spots are actually found around new init position
					m_HideSpots.clear();
					bCleared = true;
				}
				m_HideSpots.insert(std::make_pair(dist2, SSearchPoint(objPos, objDir)));
			}
		}
	}

	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		unit.m_TagPoint = ZERO;
		CAIActor* pAIActor = CastToCAIActorSafe(unit.m_refUnit.GetAIObject());
		if (pAIActor)
		{
			CAIObject* pTarget = (CAIObject*)pAIActor->GetAttentionTarget();
			if (pTarget && pTarget->GetType() == AIOBJECT_DUMMY &&
			    (pTarget->GetSubType() == IAIObject::STP_MEMORY || pTarget->GetSubType() == IAIObject::STP_SOUND))
			{
				bool bAlreadyChecked = false;
				Vec3 memoryPos(pTarget->GetPos());
				// check if some other guy is not already checking a nearby point
				TPointMap::iterator it1 = m_HideSpots.begin(), itend1 = m_HideSpots.end();
				for (; it1 != itend1; ++it1)
				{

					if (Distance::Point_Point2DSq(memoryPos, it1->second.pos) < 1)
					{
						bAlreadyChecked = true;
						break;
					}
				}
				if (!bAlreadyChecked)
				{
					float dist2 = Distance::Point_PointSq(memoryPos, m_vEnemyPos);
					if (!bCleared)
					{
						// clear the list only if some new spots are actually found around new init position
						m_HideSpots.clear();
						bCleared = true;
					}
					m_HideSpots.insert(std::make_pair(dist2, SSearchPoint(memoryPos, pTarget->GetViewDir())));
				}
			}
		}
	}

	if (m_bUseHideSpots)
	{
		TUnitList::iterator itu = GetUnitsList().begin();
		if (itu != itEnd)
		{
			CUnitImg& unit = *itu;
			const CAIActor* pActor = unit.m_refUnit.GetAIObject();

			MultimapRangeHideSpots hidespots;
			MapConstNodesDistance traversedNodes;
			GetAISystem()->GetHideSpotsInRange(hidespots, traversedNodes, m_vEnemyPos, m_fSearchDistance,
			                                   pActor->m_movementAbility.pathfindingProperties.navCapMask, pActor->GetParameters().m_fPassRadius, false);

			MultimapRangeHideSpots::iterator it = hidespots.begin(), ithend = hidespots.end();
			for (; it != ithend; ++it)
			{
				float distance = it->first;
				const SHideSpot& hs = it->second;
				Vec3 pos(hs.info.pos);
				if (hs.info.type == SHideSpotInfo::eHST_TRIANGULAR && hs.pObstacle)
				{
					Vec3 dir = (pos - m_vEnemyPos).GetNormalizedSafe();
					pos += dir * (hs.pObstacle->fApproxRadius + 0.5f + 2 * pActor->m_movementAbility.pathRadius);
				}
				if (!bCleared)
				{
					// clear the list only if some new spots are actually found around new init position
					m_HideSpots.clear();
					bCleared = true;
				}
				m_HideSpots.insert(std::make_pair(distance * distance, SSearchPoint(pos, -hs.info.dir, true)));
			}

		}
	}

	if (!m_vEnemyPos.IsZero())
	{
		// add at least one point
		Vec3 dir(m_vEnemyPos - avgPos);
		m_HideSpots.insert(std::make_pair(dir.GetLengthSquared(), SSearchPoint(m_vEnemyPos, dir.GetNormalizedSafe())));

	}

	// DEBUG
	if (gAIEnv.CVars.DebugDraw == 1)
	{
		TPointMap::iterator it = m_HideSpots.begin(), ithend = m_HideSpots.end();
		for (; it != ithend; ++it)
		{
			GetAISystem()->AddDebugSphere(it->second.pos + Vec3(0, 0, 1), 0.3f, 255, 0, 255, 8);
			GetAISystem()->AddDebugLine(it->second.pos + Vec3(0, 0, 1), it->second.pos + Vec3(0, 0, -3), 255, 0, 255, 8);
		}
		GetAISystem()->AddDebugSphere(m_vEnemyPos + Vec3(0, 0, 2), 0.5f, 255, 128, 255, 8);
		GetAISystem()->AddDebugLine(m_vEnemyPos + Vec3(0, 0, 2), m_vEnemyPos + Vec3(0, 0, -3), 255, 128, 255, 8);
	}
}

CLeaderAction_Search::~CLeaderAction_Search()
{
}

bool CLeaderAction_Search::ProcessSignal(const AISIGNAL& signal)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(signal.senderID);

	if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitMoving))
	{
		CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
		if (pUnit)
		{
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
			if (itUnit != GetUnitsList().end())
			{
				Vec3 hidepos(itUnit->m_TagPoint);
				TPointMap::iterator it = m_HideSpots.begin(), itend = m_HideSpots.end();
				for (; it != itend; ++it)
					if (it->second.pos == hidepos)
					{
						m_HideSpots.erase(it);
						itUnit->SetMoving();
						break;
					}
			}

		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitStop))
	{
		// unit can't reach an assigned hidespot
		CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
		if (pUnit)
		{
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
			if (itUnit != GetUnitsList().end())
			{
				Vec3 hidepos(itUnit->m_TagPoint);
				TPointMap::iterator it = m_HideSpots.begin(), itend = m_HideSpots.end();
				for (; it != itend; ++it)
					if (it->second.pos == hidepos)
					{
						m_HideSpots.erase(it);// assume that no other unit can reach it, just remove it
						break;
					}
				itUnit->ClearPlanning();
			}

		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitDamaged) || signal.Compare(gAIEnv.SignalCRCs.m_nAIORD_SEARCH))
	{
		CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
		if (pUnit)
		{
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
			if (itUnit != GetUnitsList().end())
			{
				Vec3 hidepos(itUnit->m_TagPoint);
				TPointMap::iterator it = m_HideSpots.begin(), itend = m_HideSpots.end();
				for (; it != itend; ++it)
					if (it->second.pos == hidepos)
					{
						it->second.bReserved = false;
						break;
					}
				//create new spot
				if (signal.pEData)
				{
					m_pLeader->ClearAllPlannings();
					PopulateSearchSpotList(signal.pEData->point);
				}
			}

		}
		return true;
	}
	return false;
}

CLeaderAction::eActionUpdateResult CLeaderAction_Search::Update()
{
	FRAME_PROFILER("CLeaderAction_Attack::CLeaderAction_Search", GetISystem(), PROFILE_AI);

	if (m_HideSpots.empty())
	{
		bool bAllunitFinished = true;
		TUnitList::iterator itEnd = GetUnitsList().end();
		for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		{
			CUnitImg& curUnit = (*unitItr);
			if (!curUnit.IsPlanFinished())
			{
				bAllunitFinished = false;
				break;
			}
		}
		if (bAllunitFinished)
			return ACTION_DONE;
	}

	m_timeRunning = GetAISystem()->GetFrameStartTime();

	bool busy = false;

	TUnitList::iterator it, itEnd = GetUnitsList().end();
	for (it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		if (unit.m_refUnit.GetAIObject()->IsEnabled() && (unit.IsPlanFinished() || !m_bInitialized))
		{
			// find cover fire units first
			if (m_iCoverUnitsLeft > 0 && (unit.GetProperties() & m_CCoverUnitProperties))
			{
				AISignalExtraData data;
				data.point = m_vEnemyPos;
				data.point2 = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, m_unitProperties);
				CUnitAction* pAction = new CUnitAction(UA_SIGNAL, true, "ORDER_COVER_SEARCH", data);
				unit.m_Plan.push_back(pAction);
				m_iCoverUnitsLeft--;
				busy = true;

				CCCPOINT(CLeaderAction_Search_Update_A);
			}
			else // search units
			{
				Vec3 unitPos(unit.m_refUnit.GetAIObject()->GetPos());
				TPointMap::iterator oit = m_HideSpots.begin(), oend = m_HideSpots.end();
				Vec3 obstaclePos(ZERO);
				TPointMap::iterator itFound = oend;

				for (; oit != oend; ++oit)
				{
					SSearchPoint& hp = oit->second;
					if (!hp.bReserved && unit.m_TagPoint != hp.pos)
					{
						itFound = oit; // get the first obstacles (closer to enemy position)
						break;
					}
				}

				if (itFound != oend)
				{
					SSearchPoint& hp = itFound->second;
					CUnitAction* action = new CUnitAction(UA_SEARCH, true, hp.pos, hp.dir);
					action->m_Tag = hp.bHideSpot ? 1 : 0;
					unit.m_Plan.push_back(action);
					unit.m_TagPoint = hp.pos;
					unit.ExecuteTask();
					busy = true;
					itFound->second.bReserved = true;

					CCCPOINT(CLeaderAction_Search_Update_B);
				}
			}
		}
		else
			busy = true;
	}

	m_bInitialized = true;

	return (busy ? ACTION_RUNNING : ACTION_DONE);
}

//-----------------------------------------------------------
//----  ATTACK
//-----------------------------------------------------------

CLeaderAction_Attack::CLeaderAction_Attack(CLeader* pLeader)
	: CLeaderAction(pLeader)
	, m_bInitialized(false)
	, m_bStealth(false)
	, m_bApproachWithNoObstacle(false)
	, m_bNoTarget(false)
	, m_timeLimit(40.0f)
	, m_vDefensePoint(ZERO)
	, m_vEnemyPos(ZERO)
{
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_DEFAULT;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		unitItr->ClearHiding();

}

CLeaderAction_Attack::CLeaderAction_Attack()
	: CLeaderAction()
	, m_bInitialized(false)
	, m_bStealth(false)
	, m_bApproachWithNoObstacle(false)
	, m_bNoTarget(false)
	, m_timeRunning()
	, m_timeLimit(0)
	, m_vDefensePoint(ZERO)
	, m_vEnemyPos(ZERO)
{
}

CLeaderAction_Attack::~CLeaderAction_Attack()
{
	GetAISystem()->SendSignal(SIGNALFILTER_GROUPONLY, 1, "OnFireDisabled", m_pLeader);
}

bool CLeaderAction_Attack::ProcessSignal(const AISIGNAL& signal)
{
	return false;
}

//
//----------------------------------------------------------------------------------------------------
#define LEADERACTION_DEFAULT_DISTANCE_TO_TARGET 6.0f

CLeaderAction_Attack_SwitchPositions::CLeaderAction_Attack_SwitchPositions()
	: m_fDistanceToTarget(LEADERACTION_DEFAULT_DISTANCE_TO_TARGET)
	, m_fMinDistanceToTarget(0.0f)
	, m_bVisibilityChecked(false)
	, m_bPointsAssigned(false)
	, m_bAvoidDanger(false)
	, m_fMinDistanceToNextPoint(0.0f)
	, m_vUpdatePointTarget(ZERO)
	, m_fFormationSize(0.0f)
	, m_fFormationScale(0.0f)
	, m_pLiveTarget(nullptr)
{
}

CLeaderAction_Attack_SwitchPositions::CLeaderAction_Attack_SwitchPositions(CLeader* pLeader, const LeaderActionParams& params)
	: m_fDistanceToTarget(LEADERACTION_DEFAULT_DISTANCE_TO_TARGET)
	, m_bVisibilityChecked(false)
	, m_bPointsAssigned(false)
	, m_bAvoidDanger(false)
	, m_fFormationSize(0.0f)
	, m_fFormationScale(0.0f)
	, m_pLiveTarget(NULL)
{
	CCCPOINT(CLeaderAction_Attack_SwitchPositions_CLA_A_SP);

	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_ATTACK_SWITCH_POSITIONS;
	m_bInitialized = false;
	m_timeLimit = params.fDuration;
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_sFormationType = params.name;
	m_unitProperties = params.unitProperties;
	// TO DO: make it variable and tweakable from script
	m_fMinDistanceToNextPoint = m_fDefaultMinDistance;
	m_PointProperties.reserve(50);
	InitNavTypeData();
	m_vUpdatePointTarget = ZERO;
	m_fDistanceToTarget = 6;
	m_fMinDistanceToTarget = 3;// below this distance, position will be discarded
}

void CLeaderAction_Attack_SwitchPositions::UpdateBeaconWithTarget(const CAIObject* pTarget) const
{
	if (pTarget)
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), pTarget->GetPos(), m_pLeader);
	else
		CLeaderAction::UpdateBeacon();
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::InitNavTypeData()
{
	m_TargetData.clear();
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		CAIActor* pAIActor = CastToCAIActorSafe(unit.m_refUnit.GetAIObject());
		if (pAIActor)
			m_TargetData.insert(std::make_pair(unit.m_refUnit, STargetData()));

		CCCPOINT(CLeaderAction_Attack_SwitchPositions_INTD);
	}
}

void CLeaderAction_Attack_SwitchPositions::OnObjectRemoved(CAIObject* pObject)
{
	// (MATT) Caused entries in m_SpecialActions to be erase and values (not keys) in m_TargetData to go NULL {2009/03/13}
}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::CheckNavType(CUnitImg& unit)
{
	// (MATT) Ensure this is a valid pipe user {2009/03/13}
	CAIActor* const pMember = CastToCAIActorSafe(unit.m_refUnit.GetAIObject());
	if (!pMember)
		return;

	// (MATT) Get attention target, if any, and find entry for this unit. Note: ref must have been valid to get this far {2009/03/13}
	CAIObject* pTarget = (CAIObject*)pMember->GetAttentionTarget();
	TMapTargetData::iterator it = m_TargetData.find(unit.m_refUnit);

	if (it == m_TargetData.end())
		return;

	CCCPOINT(CLeaderAction_Attack_SwitchPositions_CheckNavType);

	STargetData& tData = it->second;
	tData.refTarget = GetWeakRefSafe(pTarget);

	int building;
	IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(pMember->GetPos(), building, m_NavProperties);
	if (pTarget && pTarget->GetAssociation().IsValid())
		//kind of cheat, but it's just to let the AI know in which navigation type the enemy went
		if (pTarget->GetSubType() == CAIObject::STP_MEMORY)// || pTarget->GetSubType()==CAIObject::STP_SOUND)
			pTarget = pTarget->GetAssociation().GetAIObject();

	IAISystem::ENavigationType targetNavType = (pTarget ? gAIEnv.pNavigation->CheckNavigationType(pTarget->GetPos(), building, m_NavProperties) :
	                                            IAISystem::NAV_UNSET);

	if (navType == IAISystem::NAV_TRIANGULAR || navType == IAISystem::NAV_WAYPOINT_HUMAN)
	{
		if (navType != tData.navType && (tData.navType == IAISystem::NAV_TRIANGULAR || tData.navType == IAISystem::NAV_WAYPOINT_HUMAN))
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_CheckNavType_A);

			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->iValue = navType;
			pData->iValue2 = targetNavType;
			GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 1, "OnNavTypeChanged", pMember, pData);
		}
		tData.navType = navType;
	}

	if (targetNavType == IAISystem::NAV_TRIANGULAR || targetNavType == IAISystem::NAV_WAYPOINT_HUMAN)
	{

		if (targetNavType != tData.targetNavType && tData.refTarget.IsValid() && (tData.targetNavType == IAISystem::NAV_TRIANGULAR || tData.targetNavType == IAISystem::NAV_WAYPOINT_HUMAN))
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_CheckNavType_B);

			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->iValue = navType;
			pData->iValue2 = targetNavType;
			GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 1, "OnTargetNavTypeChanged", pMember, pData);
		}
		tData.targetNavType = targetNavType;
	}

	CCCPOINT(CLeaderAction_Attack_SwitchPositions_CheckNavType_C);
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_SwitchPositions::STargetData* CLeaderAction_Attack_SwitchPositions::GetTargetData(CUnitImg& unit)
{
	TMapTargetData::iterator it = m_TargetData.find(unit.m_refUnit);
	if (it != m_TargetData.end())
		return &(it->second);
	return NULL;
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_SwitchPositions::~CLeaderAction_Attack_SwitchPositions()
{
	m_pLeader->ReleaseFormation();
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::AddUnitNotify(CAIActor* pUnit)
{
	if (pUnit)
	{
		CAIObject* pTarget = (CAIObject*)pUnit->GetAttentionTarget();

		UpdatePointList(pTarget);

		TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
		if (itUnit != GetUnitsList().end())
		{
			itUnit->ClearFollowing();
			itUnit->m_TagPoint = ZERO;
			itUnit->SetMoving();
		}
	}
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::UpdatePointList(CAIObject* pTarget)
{
	IEntity* pTargetEntity = NULL;
	if (!pTarget)
		pTarget = (CAIObject*)m_pLeader->GetAIGroup()->GetAttentionTarget(true, true).GetAIObject();
	if (pTarget)
		pTargetEntity = pTarget->GetEntity();
	if (!pTargetEntity)
		return;

	// (MATT) We might check for dead references here in m_PointProperties, but logic appears to be simply:
	// "if owner removed, owner is set to NULL" which can be easily done in-place {2009/02/13}

	IEntity* pDummyEntity = NULL;
	if (!m_vUpdatePointTarget.IsEquivalent(pTarget->GetPos()))
	{
		m_vUpdatePointTarget = pTarget->GetPos();
		// target has moved, update additional shoot spot list
		QueryEventMap queryEvents;
		gAIEnv.pSmartObjectManager->TriggerEvent("CheckTargetNear", pTargetEntity, pDummyEntity, &queryEvents);

		CAIObject* pFormationOwner = m_pLeader->GetFormationOwner().GetAIObject();
		int size = m_PointProperties.size();

		const QueryEventMap::const_iterator itEnd = queryEvents.end();

		// (MATT) We might check for dead references here in m_PointProperties, but logic in OnObjectRemoved was simply:
		// "if owner removed, owner is set to NULL", which can be easily done in-place.
		// The code below doesn't actually seem interested in whether the owner is valid or not.
		// It just removes elements if the query is no longer relevant. {2009/02/13}
		TPointPropertiesList::iterator itp = m_PointProperties.begin(), itpEnd = m_PointProperties.end();
		itp += size;
		QueryEventMap::iterator itFound = queryEvents.end();
		for (; itp != itpEnd; )
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdatePointList);

			bool bFound = false;
			for (QueryEventMap::iterator itq = queryEvents.begin(); itq != itEnd; ++itq)
			{
				Vec3 pos;
				const CQueryEvent* pQueryEvent = &itq->second;
				if (pQueryEvent->pRule->pObjectHelper)
					pos = pQueryEvent->pObject->GetHelperPos(pQueryEvent->pRule->pObjectHelper);
				else
					pos = pQueryEvent->pObject->GetPos();

				if (itp->point == pos)
				{
					bFound = true;
					itFound = itq;
					break;
				}
			}
			if (bFound)
			{
				queryEvents.erase(itFound);
				++itp;
				CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdatePointList_A);
			}
			else
			{
				m_PointProperties.erase(itp++);
				CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdatePointList_B);
			}
		}

		// only not found points in queryEvents now
		for (QueryEventMap::const_iterator it = queryEvents.begin(); it != itEnd; ++it)
		{
			const CQueryEvent* pQueryEvent = &it->second;

			Vec3 pos;
			if (pQueryEvent->pRule->pObjectHelper)
				pos = pQueryEvent->pObject->GetHelperPos(pQueryEvent->pRule->pObjectHelper);
			else
				pos = pQueryEvent->pObject->GetPos();

			m_PointProperties.push_back(SPointProperties(pos));
		}

	}

}
//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_SwitchPositions::ProcessSignal(const AISIGNAL& signal)
{
	CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal);

	IEntity* pSenderEntity = gEnv->pEntitySystem->GetEntity(signal.senderID);

	if (signal.Compare(gAIEnv.SignalCRCs.m_nOnFormationPointReached))
	{
		CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_A);
		CAIObject* pUnit;
		if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
		{
			CAIActor* pAIActor = pUnit->CastToCAIActor();
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
			if (itUnit != GetUnitsList().end())
				itUnit->SetFollowing();
		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nAIORD_ATTACK))
	{
		if (signal.pEData && static_cast<ELeaderActionSubType>(signal.pEData->iValue) == m_eActionSubType)
		{
			// unit is requesting this tactic which is active already,
			// just give him instructions to go
			CAIObject* pUnit;
			if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
			{
				CAIActor* pAIActor = pUnit->CastToCAIActor();
				if (pAIActor)
				{
					CAIObject* pTarget = (CAIObject*)pAIActor->GetAttentionTarget();

					UpdatePointList(pTarget);

					TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
					if (itUnit != GetUnitsList().end())
					{
						CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_B);

						itUnit->ClearFollowing();
						itUnit->m_TagPoint = ZERO;
						itUnit->SetMoving();
					}
				}
			}
			return true;
		}
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnRequestUpdate))
	{
		CAIObject* pUnit;
		if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
		{
			CAIActor* pAIActor = pUnit->CastToCAIActor();
			if (pAIActor)
			{
				CAIObject* pTarget = (CAIObject*)pAIActor->GetAttentionTarget();

				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
				if (itUnit != GetUnitsList().end())
				{
					CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_C);

					itUnit->ClearFollowing();
					itUnit->m_TagPoint = ZERO;
					itUnit->SetMoving();

					if (signal.pEData)
					{
						GetBaseSearchPosition(*itUnit, pTarget, signal.pEData->iValue, signal.pEData->fValue);
						if (signal.pEData->iValue2 > 0)
							itUnit->m_fDistance2 = (float)signal.pEData->iValue2;

					}
				}
			}
		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnRequestUpdateAlternative))
	{
		CAIObject* pUnit;
		if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
		{
			CAIActor* pAIActor = pUnit->CastToCAIActor();
			if (pAIActor)
			{
				CAIObject* pTarget = (CAIObject*)pAIActor->GetAttentionTarget();

				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
				if (itUnit != GetUnitsList().end())
				{
					CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_D);

					itUnit->ClearFollowing();
					itUnit->m_TagPoint = ZERO;
					itUnit->SetMoving();

					if (signal.pEData)
					{
						m_ActorForbiddenSpotManager.AddSpot(GetWeakRef(pAIActor), signal.pEData->point);
						GetBaseSearchPosition(*itUnit, pTarget, signal.pEData->iValue, signal.pEData->fValue);
					}
				}
			}
		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnClearSpotList))
	{
		CAIObject* pUnit;
		if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
		{
			CAIActor* pActor = pUnit->CastToCAIActor();
			if (pActor)
			{
				CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_E);
				m_ActorForbiddenSpotManager.RemoveAllSpots(GetWeakRef(pActor));
			}
		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnRequestUpdateTowards))
	{
		CAIObject* pUnit;
		if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
		{
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if (pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
				if (itUnit != GetUnitsList().end())
				{
					CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_F);

					itUnit->ClearFollowing();
					itUnit->SetMoving();
					if (signal.pEData)
					{
						if (signal.pEData->iValue) // unit is asking for a particular direction
							itUnit->m_Group = signal.pEData->iValue;
					}
				}
			}
		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnCheckDeadTarget))
	{
		CAIObject* pUnit;
		if (pSenderEntity && (pUnit = (CAIObject*)(pSenderEntity->GetAI())))
		{
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if (pPiper)
			{
				CAIObject* pTarget = NULL;
				if (signal.pEData)
				{
					IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity((EntityId)signal.pEData->nID.n);
					if (pTargetEntity)
						pTarget = (CAIObject*) pTargetEntity->GetAI();
				}
				CWeakRef<CAIObject> refTarget = GetWeakRef(pTarget);
				CWeakRef<CAIObject> refGroupTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true, true, refTarget);
				CWeakRef<CAIActor> refInvestigator;
				if (!refGroupTarget.IsValid() && pTarget && pTarget->GetType() == AIOBJECT_PLAYER)
				{
					// last touch, no more live targets and the last one was the player, go to investigate him
					float mindist2 = 1000000000.f;
					Vec3 targetPos(pTarget->GetPos());
					TUnitList::iterator itUEnd = GetUnitsList().end();
					for (TUnitList::iterator itUnit = GetUnitsList().begin(); itUnit != itUEnd; ++itUnit)
					{
						CAIActor* pAIActor = itUnit->m_refUnit.GetAIObject();
						if (pAIActor)
						{
							float dist2 = Distance::Point_PointSq(targetPos, pAIActor->GetPos());
							if (dist2 <= mindist2)
							{
								refInvestigator = itUnit->m_refUnit;
								mindist2 = dist2;
							}
						}
					}

				}

				CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_G);

				CAIActor* const pInvestigator = refInvestigator.GetAIObject();
				if (pTarget && refInvestigator.IsValid())
				{
					CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_OCDT);

					IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
					pData->nID = pTarget->GetEntityID();
					pInvestigator->SetSignal(0, "OnCheckDeadBody", pInvestigator->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnCheckDeadBody);
				}
				else
					pPiper->SetSignal(0, "OnNoGroupTarget", 0, 0, gAIEnv.SignalCRCs.m_nOnNoGroupTarget);
			}
		}
		return true;
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nAddDangerPoint))
	{
		IAISignalExtraData* pData = signal.pEData;
		if (pData)
		{
			Vec3 dpoint = pData->point;
			float radius = pData->fValue;
			bool bFound = false;
			TDangerPointList::iterator it = m_DangerPoints.begin(), itEnd = m_DangerPoints.end();
			for (; it != itEnd; ++it)
				if (it->point == dpoint)
				{
					m_DangerPoints.erase(it);
					bFound = true;
					break;
				}

			m_DangerPoints.push_back(SDangerPoint(float(pData->iValue), pData->fValue, dpoint));

			if (!bFound)
			{
				TUnitList::iterator itUEnd = GetUnitsList().end();
				for (TUnitList::iterator itUnit = GetUnitsList().begin(); itUnit != itUEnd; ++itUnit)
				{
					CAIActor* pUnit = itUnit->m_refUnit.GetAIObject();
					if (pUnit && Distance::Point_PointSq(dpoint, pUnit->GetPos()) <= radius * radius)
					{
						itUnit->ClearFollowing();
						itUnit->ClearMoving();
						CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_H);
					}
				}
			}
			m_bPointsAssigned = false;   // reassign all points
			m_bAvoidDanger = true;
		}
		return true;
	}
	else if (!strcmp(signal.strText, "SetDistanceToTarget"))
	{
		if (signal.pEData && pSenderEntity)
		{
			const CAIActor* pAIActor = CastToCAIActorSafe(pSenderEntity->GetAI());
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
			if (itUnit != GetUnitsList().end())
			{
				CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_I);
				itUnit->m_fDistance = signal.pEData->fValue;
			}
		}
		return true;
	}
	else if (!strcmp(signal.strText, "OnExecutingSpecialAction"))
	{
		const CAIActor* pAIActor = CastToCAIActorSafe(pSenderEntity->GetAI());
		TSpecialActionMap::iterator its = m_SpecialActions.begin(), itsEnd = m_SpecialActions.end();
		for (; its != itsEnd; ++its)
		{
			SSpecialAction& action = its->second;
			if (action.refOwner == pAIActor && action.status == AS_WAITING_CONFIRM)
				action.status = AS_ON;
		}

		return true;
	}
	else if (!strcmp(signal.strText, "OnSpecialActionDone"))
	{
		const CAIActor* pAIActor = CastToCAIActorSafe(pSenderEntity->GetAI());
		TSpecialActionMap::iterator its = m_SpecialActions.begin(), itsEnd = m_SpecialActions.end();
		for (; its != itsEnd; ++its)
		{
			SSpecialAction& action = its->second;
			if (action.refOwner == pAIActor)
				action.status = AS_OFF;
		}

		TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
		if (itUnit != GetUnitsList().end())
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_J);

			itUnit->ClearSpecial();
			// will request a new position later
			itUnit->ClearFollowing();
			itUnit->m_TagPoint = ZERO;
			itUnit->SetMoving();
		}

		return true;
	}
	else if (!strcmp(signal.strText, "SetMinDistanceToTarget"))
	{
		if (signal.pEData)
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_ProcessSignal_K);
			m_fMinDistanceToTarget = signal.pEData->fValue;
		}
		return true;
	}
	return false;
}

void CLeaderAction_Attack_SwitchPositions::AssignNewShootSpot(CAIObject* pUnit, int index)
{
	CAIObject* pFormationOwner = m_pLeader->GetFormationOwner().GetAIObject();
	CFormation* pFormation = pFormationOwner ? pFormationOwner->m_pFormation : NULL;
	int fsize = pFormation ? pFormation->GetSize() : 0;
	TPointPropertiesList::iterator it = m_PointProperties.begin(), itEnd = m_PointProperties.end();

	CWeakRef<CAIObject> refUnit = GetWeakRef(pUnit);
	int i = 0;
	for (; it != itEnd; ++it)
		if (i++ >= fsize && it->refOwner == refUnit)
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_ANSS);

			it->refOwner.Reset();
			break;
		}

	m_PointProperties[index].refOwner = refUnit;
}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_SwitchPositions::IsVehicle(const CAIActor* pTarget, IEntity** ppVehicleEntity) const
{
	if (!ppVehicleEntity)
		return false;

	// assuming not null pTarget
	IEntity* pEntity = pTarget->GetEntity();

	if (pTarget->GetType() == AIOBJECT_VEHICLE)
	{
		*ppVehicleEntity = pEntity;
		return true;
	}

	if (pEntity)
	{
		while (pEntity->GetParent())
			pEntity = pEntity->GetParent();

		IAIObject* pAI = pEntity->GetAI();
		if (pAI && (static_cast<CAIObject*>(pAI)->GetType() == AIOBJECT_VEHICLE))
		{
			*ppVehicleEntity = pEntity;
			return true;
		}
	}

	return false;
}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_SwitchPositions::IsSpecialActionConsidered(const CAIActor* pUnit, const CAIActor* pUnitLiveTarget) const
{
	CWeakRef<const CAIActor> refUnitLiveTarget = GetWeakRef(pUnitLiveTarget);
	TSpecialActionMap::const_iterator its = m_SpecialActions.find(refUnitLiveTarget), itEnd = m_SpecialActions.end();
	if (its != itEnd)
	{
		CCCPOINT(CLeaderAction_Attack_SwitchPositions_ISAC);

		// if it's a vehicle, more units can have a special action with it
		if (IsVehicle(pUnitLiveTarget))
		{
			while (its != itEnd && its->first == refUnitLiveTarget)
			{
				if (its->second.refOwner == pUnit)
					return true;
				++its;
			}
		}
		else
			return true;
	}
	return false;
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::UpdateSpecialActions()
{
	CTimeValue Time = GetAISystem()->GetFrameStartTime();
	TUnitList::iterator it = GetUnitsList().begin(), itEnd = GetUnitsList().end();
	int size = m_pLeader->GetAIGroup()->GetGroupCount(IAISystem::GROUP_ENABLED);
	size = max(1, size / 2); // max half of the group can have special action
	// update special action list with new live targets
	for (; it != itEnd; ++it)
	{
		// (MATT)  Whilst updating units list, is this the best place to remove entries with invalid refs? {2009/03/13}
		CAIActor* pAIActor = it->m_refUnit.GetAIObject();
		if (pAIActor && pAIActor->IsEnabled())
		{
			IAIObject* pUnitTarget = pAIActor->GetAttentionTarget();
			const CAIActor* pUnitLiveTarget = CastToCAIActorSafe(pUnitTarget);
			if (pUnitLiveTarget && pUnitLiveTarget->IsHostile(pAIActor) && !IsSpecialActionConsidered(pAIActor, pUnitLiveTarget))
				m_SpecialActions.insert(std::make_pair(GetWeakRef(pUnitLiveTarget), SSpecialAction(it->m_refUnit)));

			CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdateSpecialActions_A);
		}
	}

	TSpecialActionMap::iterator its = m_SpecialActions.begin(), itsEnd = m_SpecialActions.end();
	for (; size > 0 && its != itsEnd; )
	{
		// (MATT)  Whilst updating the special actions map, remove entries with invalid refs {2009/03/13}
		const CAIActor* pTarget = its->first.GetAIObject();
		if (!pTarget)
		{
			m_SpecialActions.erase(its++);
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdateSpecialActions_B);
			continue;
		}

		SSpecialAction& action = its->second;
		if (action.status == AS_OFF && (Time - action.lastTime).GetSeconds() > 2)
		{
			float maxscore = -1000000;
			TUnitList::iterator itUnitSelected = itEnd;
			for (it = GetUnitsList().begin(); it != itEnd; ++it)
			{
				CAIActor* pUnit = CastToCAIActorSafe(it->m_refUnit.GetAIObject());
				if (pUnit && pUnit->IsEnabled())
				{
					IAIObject* pUnitTarget = pUnit->GetAttentionTarget();
					if (pUnitTarget && pUnitTarget == (IAIObject*)pTarget)
					{
						float score = -Distance::Point_PointSq(pTarget->GetPos(), pUnit->GetPos());
						if (action.refOwner != pUnit)
							score += 49; // like it was 7m closer
						if (score > maxscore)
						{
							maxscore = score;
							itUnitSelected = it;
						}
					}
				}
			}
			if (itUnitSelected != itEnd)
			{
				action.status = AS_WAITING_CONFIRM;
				action.lastTime = Time;
				action.refOwner = itUnitSelected->m_refUnit;

				CAIActor* pUnit = itUnitSelected->m_refUnit.GetAIObject();
				pUnit->SetSignal(1, "OnSpecialAction", pUnit->GetEntity(), NULL, gAIEnv.SignalCRCs.m_nOnSpecialAction);
				itUnitSelected->SetSpecial();
				size--;

				CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdateSpecialActions_C);
			}
		}

		switch (action.status)
		{
		case AS_ON:
			action.lastTime = Time;
			break;
		case AS_WAITING_CONFIRM:
			if ((Time - action.lastTime).GetSeconds() > 3)
			{
				// if unit doesn't confirm doing the special action,clear it
				action.status = AS_OFF;
				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(action.refOwner.GetAIObject());
				if (itUnit != GetUnitsList().end())
					itUnit->ClearSpecial();
			}
			break;
		default:
			break;
		}

		CCCPOINT(CLeaderAction_Attack_SwitchPositions_UpdateSpecialActions_D);

		++its;
	}
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction::eActionUpdateResult CLeaderAction_Attack_SwitchPositions::Update()
{
	m_pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true, true).GetAIObject();

	CTimeValue frameTime = GetAISystem()->GetFrameStartTime();
	if (m_pLiveTarget)
		m_timeRunning = frameTime;
	else if ((frameTime - m_timeRunning).GetSeconds() > m_timeLimit)
		return ACTION_DONE;

	CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
	if (!pBeacon)
		UpdateBeacon();
	if (!pBeacon)
		return ACTION_FAILED;

	CCCPOINT(CLeaderAction_Attack_SwitchPositions_Update);

	m_bVisibilityChecked = false;

	bool bFormationUpdated = false;

	TUnitList::iterator itEnd = GetUnitsList().end();
	if (!m_bInitialized)
	{
		if (!m_pLiveTarget)
			return ACTION_FAILED;
		if (!m_pLeader->LeaderCreateFormation(m_sFormationType, ZERO, false, m_unitProperties, pBeacon))
			return ACTION_FAILED;
		CFormation* pFormation = pBeacon->m_pFormation;
		assert(pFormation);
		pFormation->SetOrientationType(CFormation::OT_VIEW);
		pFormation->ForceReachablePoints(true);
		pFormation->GetNewFormationPoint(GetWeakRef(m_pLiveTarget->CastToCAIActor()), 0);

		m_fFormationScale = 1;
		m_fFormationSize = pFormation->GetMaxWidth();
		m_lastScaleUpdateTime.SetSeconds(0.f);
		UpdateFormationScale(pFormation);

		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);

		bFormationUpdated = true;

		m_vEnemyPos = pBeacon->GetPos();
		if (m_vEnemyPos.IsZero())
			return ACTION_FAILED;

		int size = pFormation->GetSize();
		m_PointProperties.resize(size);
		m_bInitialized = true;
		m_bPointsAssigned = false;
		m_bAvoidDanger = false;

		ClearUnitFlags();

		UpdatePointList();
	}

	for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
	{
		CUnitImg& curUnit = (*unitItr);
		CheckNavType(curUnit);

		if (m_bPointsAssigned && !curUnit.IsMoving())
		{
			CAIActor* pUnit = curUnit.m_refUnit.GetAIObject();

			CAIActor* pUnitObstructing = NULL;

			// check Behind status - the unit is behind some other unit in the grioup
			Vec3 pos = pUnit->GetPos();
			CAIActor* pAIActor = pUnit->CastToCAIActor();
			IAIObject* pTarget = pAIActor->GetAttentionTarget();
			Vec3 targetPos = (pTarget && pTarget->GetEntity() ? pTarget->GetPos() : m_vEnemyPos);
			Vec3 dirToEnemy = (targetPos - pos);
			Vec3 proj;

			for (TUnitList::iterator otherItr = GetUnitsList().begin(); otherItr != itEnd; ++otherItr)
			{
				CUnitImg& otherUnit = (*otherItr);
				if (otherUnit != curUnit && !otherUnit.IsMoving())
				{
					Vec3 otherPos = otherUnit.m_refUnit.GetAIObject()->GetPos();
					Vec3 dirToUnit = (otherPos - pos).GetNormalizedSafe();
					if (dirToUnit.Dot(dirToEnemy) > 0.f)
						if (fabs(pos.z - otherPos.z) < otherUnit.GetHeight() * 1.1f)
							if (Distance::Point_Line2D(otherPos, pos, targetPos, proj) < otherUnit.GetWidth() / 2)
							{
								float distanceToEnemy = Distance::Point_PointSq(targetPos, pos);
								float unitToEnemy = Distance::Point_PointSq(otherPos, targetPos);
								if (unitToEnemy > 2 * 2)
								{
									CCCPOINT(CLeaderAction_Attack_SwitchPositions_Update_A);

									pUnitObstructing = otherUnit.m_refUnit.GetAIObject();
									break;
								}
							}
				}
			}

			if (curUnit.IsBehind() && !pUnitObstructing)
			{
				pUnit->SetSignal(1, "OnNotBehind", pUnit->GetEntity());
				curUnit.ClearBehind();
			}
			else if (!curUnit.IsBehind() && pUnitObstructing)
			{
				IAISignalExtraData* pData = NULL;
				IEntity* pOtherEntity = pUnitObstructing->GetEntity();
				if (pOtherEntity)
				{
					pData = GetAISystem()->CreateSignalExtraData();
					pData->nID = pOtherEntity->GetId();
				}
				pUnit->SetSignal(1, "OnBehind", pUnit->GetEntity(), pData);
				curUnit.SetBehind();
			}
		}
	}

	if (!m_bPointsAssigned)
	{
		CFormation* pFormation = pBeacon->m_pFormation;
		int s = pFormation->GetSize();
		for (int i = 1; i < s; i++)
			pFormation->FreeFormationPoint(i);

		ClearUnitFlags();

		for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		{
			CUnitImg& curUnit = (*unitItr);
			curUnit.m_FormationPointIndex = -1;

			STargetData* pUnitTData = GetTargetData(curUnit);
			bool bAssignPoint = !(pUnitTData &&
			                      (pUnitTData->refTarget.IsValid() && (pUnitTData->targetNavType == IAISystem::NAV_WAYPOINT_HUMAN)));

			CAIObject* pUnit = curUnit.m_refUnit.GetAIObject();
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			int iPointIndex = bAssignPoint ? GetFormationPointWithTarget(curUnit) : -1;
			if (pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();
				if (!pTarget)
					pTarget = pBeacon;
				GetBaseSearchPosition(curUnit, pTarget, AI_MOVE_BACKWARD);
				if (iPointIndex > 0)
				{
					if (iPointIndex < s)
					{

						m_pLeader->AssignFormationPointIndex(curUnit, iPointIndex);
						pPiper->SetSignal(1, "OnAttackSwitchPosition");
						curUnit.ClearFollowing();
					}
					else
					{
						CAIActor* pAIActor = pUnit->CastToCAIActor();
						IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
						if (pData)
						{
							pData->point = m_PointProperties[iPointIndex].point;
							AssignNewShootSpot(pUnit, iPointIndex);
							CPipeUser* pPipeUser = pUnit->CastToCPipeUser();
							pPipeUser->SetRefPointPos(pData->point);
							pPipeUser->SetSignal(1, "OnAttackShootSpot", pUnit->GetEntity(), pData);
							curUnit.ClearFollowing();
						}
					}
				}
				else if (m_bAvoidDanger)
				{
					// to do: find a point outside all dangers and send 'em there
					Vec3 worstPoint(ZERO);
					Vec3 unitPos(pUnit->GetPos());
					float mindist = 1000000.f;
					TDangerPointList::iterator it = m_DangerPoints.begin(), itEndDP = m_DangerPoints.end();
					for (; it != itEndDP; ++it)
					{
						float d = Distance::Point_Point2DSq(unitPos, it->point);
						if (d < mindist)
						{
							mindist = d;
							worstPoint = it->point;
						}
					}
					pPiper->SetRefPointPos(worstPoint);

					pPiper->SetSignal(1, "OnAvoidDanger", NULL, NULL, gAIEnv.SignalCRCs.m_nOnAvoidDanger);
				}
			}
			m_pLeader->AssignFormationPointIndex(curUnit, iPointIndex);
			curUnit.ClearFollowing();
			curUnit.ClearSpecial(); // just to avoid sending the OnAttackSwitchPosition/ShootSpot signal now
		}
		m_bAvoidDanger = false;
		m_bPointsAssigned = true;

		return ACTION_RUNNING;
	}

	CFormation* pFormation = pBeacon->m_pFormation;
	if (pFormation)
	{
		int size = pFormation->GetSize();
		for (int i = 0; i < size; i++)
			m_PointProperties[i].bTargetVisible = false;
	}
	else
		return ACTION_FAILED;

	if (UpdateFormationScale(pFormation))
	{
		bFormationUpdated = true;
		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);
		m_vEnemyPos = pBeacon->GetPos();
		for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
			unitItr->SetMoving(); // force all units to move to (new) formation points when target and formation move
	}

	// move the formation if enemy avg position has moved
	if (!bFormationUpdated && Distance::Point_Point2DSq(pBeacon->GetPos(), m_vEnemyPos) > 4 * 4)
	{
		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);
		m_vEnemyPos = pBeacon->GetPos();
		for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
			unitItr->SetMoving(); // force all units to move to (new) formation points when target and formation move
	}

	UpdateSpecialActions();

	if (m_DangerPoints.empty())
	{
		for (TUnitList::iterator unitItr = GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		{
			CUnitImg& curUnit = (*unitItr);
			CAIObject* pUnit = curUnit.m_refUnit.GetAIObject();
			CPuppet* pPuppet = pUnit->CastToCPuppet();
			if (pUnit->IsEnabled() && pPuppet)
			{
				IAIObject* pTarget = pPuppet->GetAttentionTarget();
				if (pTarget)
				{
					const CAIActor* pTargetActor = pTarget->CastToCAIActor();

					IEntity* pVehicleEntity = NULL;
					if (pTargetActor && IsVehicle(pTargetActor, &pVehicleEntity))
					{
						// use refpoint for vehicles
						AABB aabb;
						pVehicleEntity->GetLocalBounds(aabb);
						float x = (aabb.max.x - aabb.min.x) / 2;
						float y = (aabb.max.y - aabb.min.y) / 2;
						float radius = sqrtf(x * x + y * y) + 1.f;
						Vec3 center(pVehicleEntity->GetWorldPos());
						center.z += 1.f;
						Vec3 dir(center - pUnit->GetPos());
						dir.z = 0;
						dir.NormalizeSafe();
						Vec3 hitdir(dir * radius);
						Vec3 pos(center - hitdir);
						IAIActorProxy* pProxy = pPuppet->GetProxy();
						IPhysicalEntity* pPhysics = pProxy ? pProxy->GetPhysics() : NULL;

						ray_hit hit;
						int rayresult = 0;
						IPhysicalWorld* pWorld = gAIEnv.pWorld;
						if (pWorld)
						{
							rayresult = pWorld->RayWorldIntersection(
							  pos, hitdir, COVER_OBJECT_TYPES,
							  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
							  &hit, 1, pPhysics);
							if (rayresult)
								pos = hit.pt - dir;
						}
						pPuppet->SetRefPointPos(pos);
						continue;
					}
				}
				if (!curUnit.IsSpecial() && (curUnit.IsMoving() || curUnit.IsFollowing() && (!pTarget || !pTarget->IsAgent())))
				{
					curUnit.ClearMoving();
					// change position, can't see any live target and I'm not moving
					int iPointIndex = GetFormationPointWithTarget(curUnit);
					if (iPointIndex > 0 && iPointIndex != m_pLeader->GetFormationPointIndex(pUnit))
					{
						if (iPointIndex < pFormation->GetSize())
						{
							m_pLeader->AssignFormationPointIndex(curUnit, iPointIndex);
							CAIActor* pAIActor = pUnit->CastToCAIActor();
							pAIActor->SetSignal(1, "OnAttackSwitchPosition");
						}
						else
						{
							CAIActor* pAIActor = pUnit->CastToCAIActor();
							IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
							if (pData)
							{
								pData->point = m_PointProperties[iPointIndex].point;
								AssignNewShootSpot(pUnit, iPointIndex);
								CPipeUser* pPiper = pUnit->CastToCPipeUser();
								pPiper->SetRefPointPos(pData->point);
								pPiper->SetSignal(1, "OnAttackShootSpot", pUnit->GetEntity(), pData);
								curUnit.ClearFollowing();
							}
						}
					}
				}
			}
		}
	}
	// update danger points
	CTimeValue frameDTime = GetAISystem()->GetFrameDeltaTime();
	TDangerPointList::iterator itp = m_DangerPoints.begin();
	while (itp != m_DangerPoints.end())
	{
		itp->time -= frameDTime.GetSeconds();
		if (itp->time <= 0)
			itp = m_DangerPoints.erase(itp);
		else
			++itp;
	}

	m_fMinDistanceToNextPoint = m_fDefaultMinDistance;

	return ACTION_RUNNING;
}

bool CLeaderAction_Attack_SwitchPositions::UpdateFormationScale(CFormation* pFormation)
{
	// scale the formation depending on enemy occupied area
	float t = (GetAISystem()->GetFrameStartTime() - m_lastScaleUpdateTime).GetSeconds();
	bool ret = false;
	if (t > 2.f)
	{
		m_lastScaleUpdateTime = GetAISystem()->GetFrameStartTime();

		Vec3 pos1(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(Vec3Constants<float>::fVec3_OneX));
		Vec3 pos2(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(Vec3Constants<float>::fVec3_OneY));
		Vec3 pos3(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(-Vec3Constants<float>::fVec3_OneX));
		Vec3 pos4(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(-Vec3Constants<float>::fVec3_OneY));
		float s = Distance::Point_Point(pos1, pos2);
		float s1 = Distance::Point_Point(pos3, pos4);
		if (s < s1)
			s = s1;
		float s2 = Distance::Point_Point(pos2, pos4);
		if (s < s2)
			s = s2;

		if (s > m_fFormationSize)
			s = m_fFormationSize;

		float scale = (m_fFormationSize + s) / m_fFormationSize;
		float scaleRatio = scale / m_fFormationScale;
		if (scaleRatio < 0.8f || scaleRatio > 1.2f)
		{
			m_fFormationScale = scale;
			pFormation->SetScale(scale);
			ret = true;
		}

		// compute distance to target
		int si = pFormation->GetSize();
		float mindist2 = 100000;
		bool bfound = false;
		Vec3 point;
		for (int i = 0; i < si; i++)
		{
			if (pFormation->GetPointOffset(i, point))
			{
				float dist2 = point.GetLengthSquared();
				if (dist2 < mindist2)
				{
					mindist2 = dist2;
					bfound = true;
				}
			}
		}
		if (bfound)
			m_fDistanceToTarget = sqrtf(mindist2);
	}

	return ret;
}

int CLeaderAction_Attack_SwitchPositions::GetFormationPointWithTarget(CUnitImg& unit)
{
	CCCPOINT(CLeaderAction_Attack_SwitchPositions_GFPWT);

	CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());

	CAIActor* pUnit = unit.m_refUnit.GetAIObject();
	if (!pUnit)
		return -1;

	int index = -1;
	Vec3 groundDir(0, 0, -6);
	int myCurrentPointIndex = unit.m_FormationPointIndex;
	if (pBeacon)
	{
		// TO DO: get the formation from the leader directly (beacon will not own the formation after refactoring)
		CFormation* pFormation = pBeacon->m_pFormation;
		if (pFormation)
		{
			CCCPOINT(CLeaderAction_Attack_SwitchPositions_GFPWT_GotBeacon);

			const SAIBodyInfo& bodyInfo = pUnit->GetBodyInfo();
			IEntity* pUnitEntity = pUnit->GetEntity();
			float zDisp = (pUnitEntity ? bodyInfo.vEyePos.z - pUnitEntity->GetWorldPos().z : 1.2f);

			int formationSize = pFormation->GetSize();

			// total size includes all points, including shoot spots (m_PointProperties.size()) only if there's no live target
			// otherwise, only formation points (first <formationSize> points in the m_PointProperties list
			int size = (m_pLiveTarget ? formationSize : m_PointProperties.size());

			ray_hit hit;

			CAIObject* pTarget = (CAIObject*)pUnit->GetAttentionTarget();
			if (!pTarget)
				pTarget = pBeacon;

			IAIActorProxy* pProxy = pTarget->GetProxy();
			IPhysicalEntity* pTargetPhysics = pProxy ? pProxy->GetPhysics() : NULL;

			if (!pTargetPhysics)
			{
				IAIObject* pAssociation = pTarget->GetAssociation().GetAIObject();
				if (pAssociation && pTarget->GetSubType() == IAIObject::STP_MEMORY)
				{
					if ((pProxy = pAssociation->GetProxy()))
						pTargetPhysics = pProxy->GetPhysics();
				}
			}

			Vec3 beaconpos = pTarget->GetPos();

			if (unit.m_TagPoint.IsZero())
				GetBaseSearchPosition(unit, pTarget, unit.m_Group);
			Vec3 unitpos = unit.m_TagPoint;

			float mindist = 100000000.f;
			float fMinDistanceToNextPoint = unit.m_fDistance2 > 0 ? unit.m_fDistance2 : m_fMinDistanceToNextPoint;
			for (int i = 1; i < size; i++)//exclude owner's point
			{
				Vec3 pos;
				if (i < formationSize)
				{
					CAIObject* pPointOwner = pFormation->GetPointOwner(i);
					if (pPointOwner == pUnit)
						myCurrentPointIndex = i;
					if (pPointOwner)
						continue;
					CAIObject* pFormationPoint = pFormation->GetFormationPoint(i);
					if (pFormationPoint)
					{
						pos = pFormationPoint->GetPos();
					}
					else
						continue;
				}
				else
					pos = m_PointProperties[i].point;

				if (Distance::Point_Point2DSq(pos, beaconpos) < m_fMinDistanceToTarget * m_fMinDistanceToTarget)
					continue;

				if (!m_ActorForbiddenSpotManager.IsForbiddenSpot(unit.m_refUnit, pos))
				{
					CCCPOINT(CLeaderAction_Attack_SwitchPositions_GFPWT_A);
					{
						// rough visibility check with raytrace

						// check if the current point is not in a danger region
						bool bDanger = false;
						TDangerPointList::iterator itp = m_DangerPoints.begin(), itpEnd = m_DangerPoints.end();
						for (; itp != itpEnd; ++itp)
						{
							float r = itp->radius;
							r *= r;
							if (Distance::Point_Point2DSq(itp->point, pos) <= r)
							{
								bDanger = true;
								break;
							}
							else if ((pos - unitpos).GetNormalizedSafe().Dot((itp->point - unitpos).GetNormalizedSafe()) > 0.1f)
							{
								bDanger = true;
								break;
							}
						}

						if (bDanger)
							continue;

						// tweak the right Z position for formation point - the same Z as the requestor being at the formation point
						int rayresult2 = 0;
						if (!m_bVisibilityChecked && !unit.IsFar())
						{
							if (i < formationSize)
							{
								rayresult2 = gAIEnv.pWorld->RayWorldIntersection(
								  pos, groundDir, COVER_OBJECT_TYPES,
								  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
								  &hit, 1, pTargetPhysics);
								if (rayresult2)
									pos.z = hit.pt.z;
							}
							Vec3 rayPos(pos.x, pos.y, pos.z + zDisp);
							rayresult2 = gAIEnv.pWorld->RayWorldIntersection(
							  rayPos, pTarget->GetPos() - rayPos, COVER_OBJECT_TYPES | ent_living,
							  AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
							  &hit, 1, pTargetPhysics);
						}
						// TO DO: optimization with m_bVisibilityChecked and m_Targetvisiblep[]
						// works perfectly only if there's one enemy
						if (!rayresult2 || m_PointProperties[i].bTargetVisible)
						{
							float dist = 0;

							if (unit.m_fDistance > 0)
								dist = 2 * fabs(unit.m_fDistance - Distance::Point_Point2D(beaconpos, pos));
							float d1 = Distance::Point_Point2D(pUnit->GetPos(), pos) - fMinDistanceToNextPoint;
							if (d1 < 0)
								dist -= d1;
							else
								dist += d1;

							if (!unit.m_TagPoint.IsZero())
								dist += Distance::Point_Point2D(unit.m_TagPoint, pos);

							if (unit.m_Group == AI_BACKOFF_FROM_TARGET)
							{
								//try to stay in front of target
								Vec3 targetDir(pos - beaconpos);
								targetDir.NormalizeSafe();
								float dot = targetDir.Dot(pTarget->GetViewDir());
								if (dot < 0.3f)
									dist += (1 - dot) * m_fFormationSize;
							}

							if (dist < mindist)
							{
								mindist = dist;
								m_PointProperties[i].bTargetVisible = true;
								index = i;
							}
						}

						CCCPOINT(CLeaderAction_Attack_SwitchPositions_GFPWT_B);
					}
				}
			}
			m_bVisibilityChecked = true;
		}
	}

	unit.m_Group = 0;
	unit.m_fDistance2 = 0;
	unit.ClearFar();

	unit.m_TagPoint = ZERO;

	if (index < 0 && myCurrentPointIndex > 0)
		return myCurrentPointIndex;

	return index;
}

//
//----------------------------------------------------------------------------------------------------
Vec3 CLeaderAction_Attack_SwitchPositions::GetBaseDirection(CAIObject* pTarget, bool bUseTargetMoveDir)
{
	CCCPOINT(CLeaderAction_Attack_SwitchPositions_GetBaseDirection);

	Vec3 vY;
	if (bUseTargetMoveDir)
	{
		vY = pTarget->GetMoveDir();
		vY.z = 0; // 2D only
		vY.NormalizeSafe(pTarget->GetEntityDir());
	}
	else
	{
		vY = pTarget->GetViewDir();
		if (fabs(vY.z) > 0.95f)
			vY = pTarget->GetEntityDir();
		vY.z = 0; // 2D only
		vY.NormalizeSafe();
	}
	return vY;
}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::GetBaseSearchPosition(CUnitImg& unit, CAIObject* pTarget, int method, float distance)
{
	Vec3 unitpos = unit.m_refUnit.GetAIObject()->GetPos();
	if (!pTarget)
	{
		pTarget = (CAIObject*)m_pLeader->GetAIGroup()->GetAttentionTarget().GetAIObject();
		if (!pTarget)
		{
			pTarget = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
			if (!pTarget)
			{
				unit.m_TagPoint = unitpos;
				return;
			}
		}
	}

	Vec3 beaconpos = pTarget->GetPos();

	if (method < 0)
	{
		method = -method;
		unit.SetFar();
	}
	else
		unit.ClearFar();

	bool bUseTargetMoveDir = (method & AI_USE_TARGET_MOVEMENT) != 0;
	method &= ~AI_USE_TARGET_MOVEMENT;

	if (method != AI_BACKOFF_FROM_TARGET)
	{
		if (distance <= 0)
			distance = 2 * m_fMinDistanceToNextPoint;
		else
			distance += m_fMinDistanceToNextPoint;
	}

	unit.m_Group = method;

	switch (method)
	{
	case AI_BACKOFF_FROM_TARGET:
		{
			unit.m_fDistance = distance;
			Vec3 vY(unitpos - beaconpos);
			vY.NormalizeSafe();
			unitpos = beaconpos + vY * distance;
		}
		break;
	case AI_MOVE_BACKWARD: // move back to the target
		{
			Vec3 vY(GetBaseDirection(pTarget, bUseTargetMoveDir));
			unitpos = beaconpos + vY * distance;
		}
		break;
	case AI_MOVE_LEFT: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget, bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0, 0, 1);
			unitpos = beaconpos - vX * distance;
		}
		break;
	case AI_MOVE_RIGHT: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget, bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0, 0, 1);
			unitpos = beaconpos + vX * distance;
		}
		break;
	case AI_MOVE_RIGHT + AI_MOVE_BACKWARD: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget, bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0, 0, 1);
			unitpos = beaconpos + (vX + vY).GetNormalizedSafe() * distance;
		}
		break;
	case AI_MOVE_LEFT + AI_MOVE_BACKWARD: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget, bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0, 0, 1);
			unitpos = beaconpos + (-vY - vX).GetNormalizedSafe() * distance;
		}
		break;
	default:
		break;
	}
	unit.m_TagPoint = unitpos;

	CCCPOINT(CLeaderAction_Attack_SwitchPositions_GetBasesSearchPosition);
}

//-------------------------------------------------------------------------------------
// SERIALIZATION FUNCTIONS
//-------------------------------------------------------------------------------------

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction::Serialize(TSerialize ser)
{
	ser.EnumValue("m_ActionType", m_eActionType, LA_NONE, LA_LAST);
	ser.EnumValue("m_eActionSubType", m_eActionSubType, LAS_DEFAULT, LAS_LAST);
	ser.Value("m_Priority", m_Priority);
	ser.Value("m_unitProperties", m_unitProperties);
	ser.EnumValue("m_currentNavType", m_currentNavType, IAISystem::NAV_UNSET, IAISystem::NAV_MAX_VALUE);
	ser.Value("m_NavProperties", m_NavProperties);
}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack::Serialize(TSerialize ser)
{
	CLeaderAction::Serialize(ser);
	ser.Value("m_bInitialized", m_bInitialized);
	ser.Value("m_bNoTarget", m_bNoTarget);
	ser.Value("m_timeRunning", m_timeRunning);
	ser.Value("m_timeLimit", m_timeLimit);
	ser.Value("m_bApproachWithNoObstacle", m_bApproachWithNoObstacle);
	ser.Value("m_vDefensePoint", m_vDefensePoint);
	ser.Value("m_vEnemyPos", m_vEnemyPos);

}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Search::Serialize(TSerialize ser)
{
	CLeaderAction::Serialize(ser);
	ser.Value("m_timeRunning", m_timeRunning);
	ser.Value("m_iCoverUnitsLeft", m_iCoverUnitsLeft);
	ser.Value("m_vEnemyPos", m_vEnemyPos);
	ser.Value("m_fSearchDistance", m_fSearchDistance);
	ser.Value("m_bInitialized", m_bInitialized);
	ser.Value("m_bUseHideSpots", m_bUseHideSpots);
	ser.Value("m_iSearchSpotAIObjectType", m_iSearchSpotAIObjectType);

	ser.BeginGroup("HideSpots");
	if (ser.IsReading())
		m_HideSpots.clear();
	TPointMap::iterator it = m_HideSpots.begin();
	char name[16];
	int count = m_HideSpots.size();
	SSearchPoint hp;
	float dist2;
	ser.Value("count", count);
	for (int i = 0; i < count; ++i)
	{
		if (ser.IsWriting())
		{
			dist2 = it->first;
			hp.pos = it->second.pos;
			hp.dir = it->second.dir;
			hp.bReserved = it->second.bReserved;
			++it;
		}
		cry_sprintf(name, "HideSpot%d", i);
		ser.BeginGroup(name);
		ser.Value("distance2", dist2);
		hp.Serialize(ser);
		ser.EndGroup();
		if (ser.IsReading())
			m_HideSpots.insert(std::make_pair(dist2, hp));
	}
	ser.EndGroup();
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::Serialize(TSerialize ser)
{
	CLeaderAction_Attack::Serialize(ser);
	ser.Value("m_PointProperties", m_PointProperties);
	ser.Value("m_sFormationType", m_sFormationType);
	ser.Value("m_bVisibilityChecked", m_bVisibilityChecked);
	ser.Value("m_fMinDistanceToNextPoint", m_fMinDistanceToNextPoint);
	ser.Value("m_bPointsAssigned", m_bPointsAssigned);
	ser.Value("m_DangerPoints", m_DangerPoints);
	ser.Value("m_bAvoidDanger", m_bAvoidDanger);
	ser.Value("m_vUpdatePointTarget", m_vUpdatePointTarget);
	ser.Value("m_fFormationSize", m_fFormationSize);
	ser.Value("m_fFormationScale", m_fFormationScale);
	ser.Value("m_fDistanceToTarget", m_fDistanceToTarget);
	ser.Value("m_fMinDistanceToTarget", m_fMinDistanceToTarget);
	ser.Value("m_lastScaleUpdateTime", m_lastScaleUpdateTime);

	ser.BeginGroup("TargetData");
	{
		int size = m_TargetData.size();
		ser.Value("size", size);
		TMapTargetData::iterator it = m_TargetData.begin();
		if (ser.IsReading())
			m_TargetData.clear();

		STargetData targetData;
		CWeakRef<CAIActor> refUnitActor;
		char name[32];

		for (int i = 0; i < size; i++)
		{
			cry_sprintf(name, "target_%d", i);
			ser.BeginGroup(name);
			{
				if (ser.IsWriting())
				{
					refUnitActor = it->first;
					targetData = it->second;
					++it;
				}
				refUnitActor.Serialize(ser, "refUnitActor");
				targetData.Serialize(ser);
				if (ser.IsReading())
					m_TargetData.insert(std::make_pair(refUnitActor, targetData));
			}
			ser.EndGroup();

		}
		ser.EndGroup();
	}

	ser.BeginGroup("SpecialActions");
	{
		int size = m_SpecialActions.size();
		ser.Value("size", size);
		TSpecialActionMap::iterator it = m_SpecialActions.begin();
		if (ser.IsReading())
			m_SpecialActions.clear();

		SSpecialAction specialAction;
		CWeakRef<const CAIActor> refTargetActor;
		char name[32];

		for (int i = 0; i < size; i++)
		{
			cry_sprintf(name, "target_%d", i);
			ser.BeginGroup(name);
			{
				if (ser.IsWriting())
				{
					refTargetActor = it->first;
					specialAction = it->second;
					++it;
				}
				refTargetActor.Serialize(ser, "refTargetActor");
				specialAction.Serialize(ser);
				if (ser.IsReading())
					m_SpecialActions.insert(std::make_pair(refTargetActor, specialAction));
			}
			ser.EndGroup();

		}
		ser.EndGroup();
	}

	m_ActorForbiddenSpotManager.Serialize(ser);
}
