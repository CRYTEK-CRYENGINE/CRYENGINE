// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIGroup.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 2006				: Created by Mikko Mononen

 *********************************************************************/

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include "CAISystem.h"
#include "AIPlayer.h"
#include "AIGroup.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include <float.h>
#include "DebugDrawContext.h"
#include "Formation/AIFormationDescriptor.h"

// Serialises a container of AI references
// Perhaps this is too heavy on the templating and it could be virtualised
template<class T>
bool SerializeWeakRefVector(TSerialize ser, const char* containerName, std::vector<CWeakRef<T>>& container)
{
	ser.BeginGroup(containerName);
	unsigned numItems = container.size();
	ser.Value("numItems", numItems);
	if (ser.IsReading())
		container.resize(numItems);

	for (typename std::vector<CWeakRef<T>>::iterator itr(container.begin()); itr != container.end(); ++itr)
	{
		ser.BeginGroup("i");
		itr->Serialize(ser, "v");
		ser.EndGroup();
	}
	ser.EndGroup();
	return true;
}

//====================================================================
// CAIGroup
//====================================================================
CAIGroup::CAIGroup(int groupID) :
	m_groupID(groupID),
	m_pLeader(0),
	m_bUpdateEnemyStats(true),
	m_vEnemyPositionKnown(0, 0, 0),
	m_vAverageEnemyPosition(0, 0, 0),
	m_vAverageLiveEnemyPosition(0, 0, 0),
	m_countMax(0),
	m_countEnabledMax(0),
	m_reinforcementSpotsAcquired(false),
	m_reinforcementState(REINF_NONE),
	m_countDead(0),
	m_countCheckedDead(0),
	m_count(0),
	m_countEnabled(0),
	m_reinforcementSpotsAllAlerted(0),
	m_reinforcementSpotsCombat(0),
	m_reinforcementSpotsBodyCount(0),
	m_pendingDecSpotsAllAlerted(false),
	m_pendingDecSpotsCombat(false),
	m_pendingDecSpotsBodyCount(false)
{
	CCCPOINT(CAIGroup_CAIGroup)
}

//====================================================================
// ~CAIGroup
//====================================================================
CAIGroup::~CAIGroup()
{
	CCCPOINT(CAIGroup_Destructor)
}

//====================================================================
// SetLeader
//====================================================================
void CAIGroup::SetLeader(CLeader* pLeader)
{
	CCCPOINT(CAIGroup_SetLeader)
	m_pLeader = pLeader;
	if (pLeader)
		pLeader->SetAIGroup(this);
}

//====================================================================
// GetLeader
//====================================================================
CLeader* CAIGroup::GetLeader()
{
	return m_pLeader;
}

//====================================================================
// AddMember
//====================================================================
void CAIGroup::AddMember(CAIActor* pMember)
{
	CCCPOINT(CAIGroup_AddMember)

	// everywhere assumes that pMember is non-null
	AIAssert(pMember);
	// check if it's already a member of team
	if (GetUnit(pMember) != m_Units.end())
		return;
	CUnitImg newUnit;
	newUnit.m_refUnit = GetWeakRef(pMember);
	SetUnitClass(newUnit);

	IEntity* pEntity = pMember->GetEntity();
	if (pEntity)
	{
		IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
		if (pPhysEntity)
		{
			CCCPOINT(CAIGroup_AddMember_Phys)

			float radius;
			pe_status_pos status;
			pPhysEntity->GetStatus(&status);
			radius = fabsf(status.BBox[1].x - status.BBox[0].x);
			float radius2 = fabsf(status.BBox[1].y - status.BBox[0].y);
			if (radius < radius2)
				radius = radius2;
			float myHeight = status.BBox[1].z - status.BBox[0].z;
			newUnit.SetWidth(radius);
			newUnit.SetHeight(myHeight);
		}
	}
	m_Units.push_back(newUnit);

	if (m_pLeader)
	{
		m_pLeader->AddUnitNotify(pMember);
	}
}

//====================================================================
// RemoveMember
//====================================================================
bool CAIGroup::RemoveMember(CAIActor* pMember)
{
	if (!pMember)
		return false;

	CCCPOINT(CAIGroup_RemoveMember);

	// (MATT) This will need cleanup {2009/02/12}
	if (m_refReinfPendingUnit == pMember)
		NotifyReinfDone(pMember, false);

	if (pMember->GetProxy() && pMember->GetProxy()->IsDead())
		m_countDead++;

	TUnitList::iterator theUnit = std::find(m_Units.begin(), m_Units.end(), pMember);

	if (m_pLeader)
	{
		CAIActor* pMemberActor = pMember->CastToCAIActor();
		if (m_pLeader->GetFormationOwner() == pMember && pMemberActor->GetGroupId() == m_pLeader->GetGroupId())
			m_pLeader->ReleaseFormation();

		if (m_pLeader->GetAssociation() == pMember)
			m_pLeader->SetAssociation(NILREF);
	}

	LeaveAllGroupScopesForActor(pMember);

	if (theUnit != m_Units.end())
	{
		m_Units.erase(theUnit);
		if (m_pLeader)
		{
			// if empty group - remove the leader; nobody owns it, will hang there forever
			// ok, don't remove leader here - all serialization issues are taken care of; make sure chain-loading is fine
			//			if(m_Units.empty())
			//			{
			//				if(deleting)
			//				{
			//					GetAISystem()->RemoveObject(m_pLeader);
			//					m_pLeader = 0;
			//				}
			//			}
			//			else
			if (!m_Units.empty())
			{
				m_pLeader->DeadUnitNotify(pMember);
			}
		}
		m_bUpdateEnemyStats = true;

		return true;
	}
	return false;
}

//====================================================================
// NotifyBodyChecked
//====================================================================
void CAIGroup::NotifyBodyChecked(const CAIObject* pObject)
{
	m_countCheckedDead++;
}

//====================================================================
// GetUnit
//====================================================================
TUnitList::iterator CAIGroup::GetUnit(const CAIActor* pAIObject)
{
	CCCPOINT(CAIGroup_GetUnit);
	return std::find(m_Units.begin(), m_Units.end(), pAIObject);
}

//====================================================================
// SetUnitClass
//====================================================================
void CAIGroup::SetUnitClass(CUnitImg& UnitImg) const
{
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	CAIObject* const pObject = UnitImg.m_refUnit.GetAIObject();
	IEntity* const pEntity = (pObject ? pObject->GetEntity() : NULL);
	if (pEntity)
	{
		IScriptTable* pTable = pEntity->GetScriptTable();
		SmartScriptTable pEntityProperties;
		if (pTable && pTable->GetValue("Properties", pEntityProperties))
		{
			const char* sCharacter = NULL;
			pEntityProperties->GetValue("aicharacter_character", sCharacter);
			IScriptSystem* pSS = pTable->GetScriptSystem();
			if (pSS)
			{
				SmartScriptTable pGlobalCharacterTable;
				if (pSS->GetGlobalValue("AICharacter", pGlobalCharacterTable))
				{
					SmartScriptTable pMyCharacterTable;
					if (pGlobalCharacterTable->GetValue(sCharacter, pMyCharacterTable))
					{
						int soldierClass;
						if (pMyCharacterTable->GetValue("Class", soldierClass))
						{
							CCCPOINT(CAIGroup_SetUnitClass);

							UnitImg.SetClass(static_cast<int>(soldierClass));
							return;
						}
					}
				}
			}
		}
	}
#endif
	UnitImg.SetClass(UNIT_CLASS_UNDEFINED);
}

//====================================================================
// IsMember
//====================================================================
bool CAIGroup::IsMember(const IAIObject* pMember) const
{
	const CAIActor* pActor = pMember->CastToCAIActor();
	for (TUnitList::const_iterator itUnit = m_Units.begin(); itUnit != m_Units.end(); ++itUnit)
		if ((*itUnit) == pActor)
			return true;
	return false;
}

//====================================================================
// SetUnitProperties
//====================================================================
void CAIGroup::SetUnitProperties(const IAIObject* obj, uint32 properties)
{
	const CAIActor* pActor = obj->CastToCAIActor();
	TUnitList::iterator senderUnit = std::find(m_Units.begin(), m_Units.end(), pActor);

	if (senderUnit != m_Units.end())
		senderUnit->SetProperties(properties);
}

//====================================================================
// GetAveragePosition
//====================================================================
Vec3 CAIGroup::GetAveragePosition(IAIGroup::eAvPositionMode mode, uint32 unitClassMask) const
{
	// (MATT) Restructured this {2009/02/13}

	int count = 0;
	Vec3 average(ZERO);
	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		CAIObject* const pUnit = it->m_refUnit.GetAIObject();
		if (!pUnit->IsEnabled())
			continue;
		bool bInclude = false;

		switch (mode)
		{
		case AVMODE_ANY:
			bInclude = true;
			break;
		case AVMODE_CLASS:
			bInclude = (it->GetClass() & unitClassMask) != 0;
			break;
		case AVMODE_PROPERTIES:
			bInclude = (it->GetProperties() & unitClassMask) != 0;
			break;
		default:
			break;
		}

		CCCPOINT(CAIGroup_GetAveragePosition);

		if (bInclude)
		{
			++count;
			average += pUnit->GetPos();
		}
	}
	return (count ? average / float(count) : average);
}

//====================================================================
// GetUnitCount
//====================================================================
int CAIGroup::GetUnitCount(uint32 unitPropMask) const
{

	int count = 0;
	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
		if (it->m_refUnit.GetAIObject()->IsEnabled() && (it->GetProperties() & unitPropMask))
		{
			CCCPOINT(CAIGroup_GetUnitCount);
			++count;
		}
	return count;
}

//====================================================================
// GetAttentionTarget
//====================================================================
// Suppressed passedByValue warnings for smart pointers like CWeakRef<>
// cppcheck-suppress passedByValue
CWeakRef<CAIObject> CAIGroup::GetAttentionTarget(bool bHostileOnly, bool bLiveOnly, const CWeakRef<CAIObject> refSkipTarget) const
{
	CCCPOINT(CAIGroup_GetAttentionTarget);

	// to do: give more priority to closer targets?
	CWeakRef<CAIObject> refSelectedTarget;
	bool bHostileFound = false;

	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		CAIActor* pAIActor = CastToCAIActorSafe(it->m_refUnit.GetAIObject());
		if (pAIActor)
		{
			CAIObject* attentionTarget = static_cast<CAIObject*>(pAIActor->GetAttentionTarget());
			if (attentionTarget && (attentionTarget->IsEnabled() ||
			                        (attentionTarget->GetType() == AIOBJECT_VEHICLE) && refSkipTarget != attentionTarget))
			{
				CWeakRef<CAIObject> refAttentionTarget = GetWeakRef(attentionTarget);
				bool bLive = attentionTarget->IsAgent();
				if (pAIActor->IsHostile(attentionTarget))
				{
					if (bLive)
						return refAttentionTarget;//give priority to live hostile targets
					else if (!bLiveOnly)
					{
						bHostileFound = true;
						refSelectedTarget = refAttentionTarget; // then to hostile non live targets
						CCCPOINT(CAIGroup_GetAttentionTarget_A);
					}
				}
				else if (!bHostileFound && !bHostileOnly && (bLive || !bLiveOnly))
					refSelectedTarget = refAttentionTarget; // then to non hostile targets
			}
		}
	}
	if (m_pLeader && m_pLeader->IsPlayer())
	{
		CAIPlayer* pPlayer = CastToCAIPlayerSafe(m_pLeader->GetAssociation().GetAIObject());
		if (pPlayer)
		{
			CAIObject* pPlayerAttTarget = static_cast<CAIObject*>(pPlayer->GetAttentionTarget());
			CWeakRef<CAIObject> refPlayerAttTarget = GetWeakRef(pPlayerAttTarget);
			if (pPlayerAttTarget && pPlayerAttTarget->IsEnabled() && refPlayerAttTarget != refSkipTarget) // nothing else to check, assuming that player has only live enemy targets
			{
				bool bLive = pPlayerAttTarget->IsAgent();
				if (pPlayer->IsHostile(pPlayerAttTarget))
				{
					if (bLive)
						return refPlayerAttTarget;//give priority to live hostile targets
					else if (!bLiveOnly)
						refSelectedTarget = refPlayerAttTarget; // then to hostile non live targets
				}
				else if (!bHostileFound && !bHostileOnly && (bLive || !bLiveOnly))
					refSelectedTarget = refPlayerAttTarget; // then to non hostile targets

				CCCPOINT(CAIGroup_GetAttentionTarget_B);
			}
		}
	}

	return refSelectedTarget;
}

//====================================================================
// GetTargetCount
//====================================================================
int CAIGroup::GetTargetCount(bool bHostileOnly, bool bLiveOnly) const
{
	CCCPOINT(CAIGroup_GetTargetCount);
	int n = 0;

	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		CAIActor* pAIActor = CastToCAIActorSafe(it->m_refUnit.GetAIObject());
		if (pAIActor)
		{
			CAIObject* attentionTarget = static_cast<CAIObject*>(pAIActor->GetAttentionTarget());
			if (attentionTarget && attentionTarget->IsEnabled())
			{
				bool bLive = attentionTarget->IsAgent();
				bool bHostile = pAIActor->IsHostile(attentionTarget);
				if (!(bLiveOnly && !bLive || bHostileOnly && !bHostile))
					n++;
			}
		}
	}
	if (m_pLeader && m_pLeader->IsPlayer())
	{
		CAIPlayer* pPlayer = (CAIPlayer*)m_pLeader->GetAssociation().GetAIObject();
		;
		if (pPlayer)
		{
			IAIObject* pAttentionTarget = pPlayer->GetAttentionTarget();
			if (pAttentionTarget && pAttentionTarget->IsEnabled()) // nothing else to check, assuming that player has only live enemy targets
			{
				bool bLive = pAttentionTarget->IsAgent();
				bool bHostile = pPlayer->IsHostile(pAttentionTarget);
				if (!(bLiveOnly && !bLive || bHostileOnly && !bHostile))
					n++;
			}
		}
	}

	return n;
}

//====================================================================
// GetEnemyPosition
//====================================================================
Vec3 CAIGroup::GetEnemyPosition(const CAIObject* pUnit) const
{
	Vec3 enemyPos(0, 0, 0);
	const CAIActor* pUnitAIActor = CastToCAIActorSafe(pUnit);
	if (pUnitAIActor)
	{
		IAIObject* pAttTarget = pUnitAIActor->GetAttentionTarget();
		if (pAttTarget)
		{
			CCCPOINT(CAIGroup_GetEnemyPosition_A);

			switch (((CAIObject*)pAttTarget)->GetType())
			{
			case AIOBJECT_NONE:
				break;
			case AIOBJECT_ACTOR:
			case AIOBJECT_DUMMY:
			case AIOBJECT_PLAYER:
			case AIOBJECT_WAYPOINT: // Actually this should be Beacon
			default:
				return pAttTarget->GetPos();
			}
		}
		else
			return m_vAverageEnemyPosition;
	}
	else
	{
		Vec3 averagePos = GetAveragePosition();

		int memory = 0;
		bool realTarget = false;
		float distance = 10000.f;

		TUnitList::const_iterator it, itEnd = m_Units.end();
		for (it = m_Units.begin(); it != itEnd; ++it)
		{
			CAIActor* const pAIActor = CastToCAIActorSafe(it->m_refUnit.GetAIObject());
			if (!pAIActor || !pAIActor->IsEnabled())
				continue;

			CAIObject* const pTarget = (CAIObject*) pAIActor->GetAttentionTarget();
			if (pTarget && !pAIActor->IsHostile(pTarget))
			{
				const SOBJECTSTATE& state = pAIActor->GetState();

				if (!realTarget)
				{
					if (state.eTargetType == AITARGET_MEMORY)
					{
						enemyPos += pTarget->GetPos();
						++memory;
					}
					else
					{
						realTarget = true;
						enemyPos = pTarget->GetPos();
						distance = (enemyPos - averagePos).GetLengthSquared();
					}
				}
				else if (state.eTargetType != AITARGET_MEMORY)
				{
					float thisDistance = (pAIActor->GetPos() - averagePos).GetLengthSquared();
					if (distance > thisDistance)
					{
						enemyPos = pTarget->GetPos();
						distance = thisDistance;
					}
				}
			}
		}

		if (!realTarget && m_pLeader && m_pLeader->IsPlayer())
		{
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(m_pLeader->GetAssociation().GetAIObject());
			if (pPlayer)
			{
				IAIObject* pTarget = pPlayer->GetAttentionTarget();
				if (pTarget && pTarget->IsEnabled()) // nothing else to check, assuming that player has only live enemy targets
				{
					realTarget = true;
					enemyPos = pTarget->GetPos();
				}
			}
		}

		if (!realTarget && memory)
			enemyPos /= (float) memory;
	}

	CCCPOINT(CAIGroup_GetEnemyPosition_B);

	return enemyPos;
}

//====================================================================
// GetEnemyAverageDirection
//====================================================================
Vec3 CAIGroup::GetEnemyAverageDirection(bool bLiveTargetsOnly, bool bSmart) const
{
	Vec3 avgDir(0, 0, 0);
	//	TVecTargets FoundTargets;
	//	TVecTargets::iterator itf;
	TSetAIObjects::const_iterator it, itEnd = m_Targets.end();
	int count = 0;
	for (it = m_Targets.begin(); it != itEnd; ++it)
	{
		const CAIObject* pTarget = it->GetAIObject();
		if (pTarget)
		{
			bool bLiveTarget = pTarget->IsAgent();
			if ((!bLiveTargetsOnly || bLiveTarget) && !pTarget->GetMoveDir().IsZero())
			{
				// add only not null (significative) directions
				avgDir += pTarget->GetMoveDir();
				//			FoundTargets.push_back(pTarget);
				count++;
			}
		}

	}
	if (count)
	{
		avgDir /= float(count);
		/*if(bSmart)// clear the average if it's small enough
		   //(means that the direction vectors are spread in many opposite directions)
		   {

		   // compute variance
		   float fVariance = 0;
		   for (itf = FoundTargets.begin(); itf != FoundTargets.end(); ++itf)
		   {	CAIObject* pTarget = *itf;
		   fVariance+= (pTarget->GetMoveDir() - avgDir).GetLengthSquared();
		   }

		   if(fVariance>0.7)
		   avgDir.Set(0,0,0);

		   }*/
		// either normalize or clear
		float fAvgSize = avgDir.GetLength();
		if (fAvgSize > (bSmart ? 0.4f : 0.1f))
			avgDir /= fAvgSize;
		else
			avgDir.Set(0, 0, 0);
	}
	return avgDir;
}

//====================================================================
// OnObjectRemoved
//====================================================================
void CAIGroup::OnObjectRemoved(CAIObject* pObject)
{
	if (!pObject)
		return;

	CAIActor* pActor = pObject->CastToCAIActor();

	TUnitList::iterator it = std::find(m_Units.begin(), m_Units.end(), pActor);
	if (it != m_Units.end())
		RemoveMember(pActor);

	// (MATT) If UpdateEnemyStats touches targets, this must happen after other cleanup has happened... {2009/03/17}
	TSetAIObjects::iterator ittgt = std::find(m_Targets.begin(), m_Targets.end(), pObject);
	if (ittgt != m_Targets.end())
	{
		m_bUpdateEnemyStats = true;
	}

	for (unsigned i = 0; i < m_reinforcementSpots.size(); )
	{
		if (m_reinforcementSpots[i].pAI == pObject)
		{
			m_reinforcementSpots[i] = m_reinforcementSpots.back();
			m_reinforcementSpots.pop_back();
		}
		else
			++i;
	}
}

//====================================================================
// GetForemostUnit
//====================================================================
CAIObject* CAIGroup::GetForemostUnit(const Vec3& direction, uint32 unitProp) const
{
	Vec3 vFurthestPos(0, 0, 0);
	float maxLength = 0;
	CAIObject* pForemost = NULL;

	Vec3 vAvgPos = GetAveragePosition(AVMODE_PROPERTIES, unitProp);

	for (TUnitList::const_iterator it = m_Units.begin(); it != m_Units.end(); ++it)
	{
		const CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_refUnit.GetAIObject();
		if (pUnit->IsEnabled() && (unit.GetProperties() & unitProp))
		{
			CCCPOINT(CAIGroup_ForemostUnit);

			Vec3 vDisplace = pUnit->GetPos() - vAvgPos;
			float fNorm = vDisplace.GetLength();
			if (fNorm > 0.001f)//consider an epsilon
			{
				Vec3 vNorm = vDisplace / fNorm;
				float cosine = vNorm.Dot(direction);
				if (cosine >= 0) // exclude negative cosine (opposite directions)
				{
					float fProjection = cosine * fNorm;
					if (maxLength < fProjection)
					{
						maxLength = fProjection;
						pForemost = pUnit;
					}
				}
			}
			else if (!pForemost)
			{
				// the unit is exactly at the average position and no one better has been found yet
				pForemost = pUnit;
			}
		}
	}
	return (pForemost);
}

//====================================================================
// GetForemostEnemyPosition
//====================================================================
Vec3 CAIGroup::GetForemostEnemyPosition(const Vec3& direction, bool bLiveOnly) const
{
	const CAIObject* pForemost = GetForemostEnemy(direction, bLiveOnly);
	return (pForemost ? pForemost->GetPos() : ZERO);
}

//====================================================================
// GetForemostEnemy
//====================================================================
const CAIObject* CAIGroup::GetForemostEnemy(const Vec3& direction, bool bLiveOnly) const
{
	Vec3 vFurthestPos(0, 0, 0);
	float maxLength = 0;
	const CAIObject* pForemost = NULL;
	Vec3 vDirNorm = direction.GetNormalizedSafe();
	TSetAIObjects::const_iterator itend = m_Targets.end();
	for (TSetAIObjects::const_iterator it = m_Targets.begin(); it != itend; ++it)
	{
		const CAIObject* pTarget = it->GetAIObject();
		if (!bLiveOnly || pTarget->IsAgent())
		{
			Vec3 vDisplace = pTarget->GetPos() - m_vAverageEnemyPosition;
			float fNorm = vDisplace.GetLength();
			if (fNorm > 0.001f)//consider an epsilon
			{
				Vec3 vNorm = vDisplace / fNorm;
				float cosine = vNorm.Dot(vDirNorm);
				if (cosine >= 0) // exclude negative cosine (opposite directions)
				{
					float fProjection = cosine * fNorm;
					if (maxLength < fProjection)
					{
						maxLength = fProjection;
						pForemost = pTarget;
					}
				}
			}
			else if (!pForemost)
			{
				// the target is exactly at the average position and no one better has been found yet
				pForemost = pTarget;
			}
		}
	}

	CAISystem* pAISystem(GetAISystem());
	if (!pForemost && !bLiveOnly)
		pForemost = static_cast<CAIObject*>(pAISystem->GetBeacon(m_groupID));

	return pForemost;
}

//====================================================================
// GetEnemyPositionKnown
//====================================================================
Vec3 CAIGroup::GetEnemyPositionKnown() const
{
	// (MATT) Only used in one place and candidates for its replacement are noted {2009/02/10}
	CCCPOINT(CAIGroup_GetEnemyPositionKnown);

	CAISystem* pAISystem(GetAISystem());
	Vec3 enemyPos;
	bool realTarget = false;
	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		CAIActor* pAIActor = it->m_refUnit.GetAIObject();
		if (!pAIActor || !pAIActor->IsEnabled())
			continue;

		CAIObject* const pTarget = (CAIObject*) pAIActor->GetAttentionTarget();
		if (!pTarget || !pTarget->IsEnabled() || !pTarget->IsAgent())
			continue;

		if (pAIActor->GetAttentionTargetType() == AITARGET_MEMORY || !pAIActor->IsHostile(pTarget))
			continue;

		realTarget = true;
		enemyPos = pTarget->GetPos();
		break;
	}

	if (!realTarget)
	{
		CCCPOINT(CAIGroup_GetEnemyPositionKnown_A);

		if (m_pLeader && m_pLeader->IsPlayer())
		{
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(m_pLeader->GetAssociation().GetAIObject());
			if (pPlayer)
			{
				IAIObject* pTarget = pPlayer->GetAttentionTarget();
				if (pTarget && pTarget->IsEnabled()) // nothing else to check, assuming that player has only live enemy targets
				{
					realTarget = true;
					enemyPos = pTarget->GetPos();
				}
			}
		}
		if (!realTarget)
		{
			if (pAISystem->GetBeacon(m_groupID))
				enemyPos = pAISystem->GetBeacon(m_groupID)->GetPos();
			else
				enemyPos.Set(0, 0, 0);
		}
	}
	return enemyPos;
}

//====================================================================
// Update
//====================================================================
void CAIGroup::Update()
{
	CCCPOINT(CAIGroup_Update);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CAISystem* pAISystem(GetAISystem());

	UpdateReinforcementLogic();
}

//====================================================================
// UpdateReinforcementLogic
//====================================================================
// (MATT) This code looks rather complex - does it really help? {2009/02/12}
void CAIGroup::UpdateReinforcementLogic()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (/*m_reinforcementState == REINF_ALERTED_COMBAT_PENDING ||*/
	  m_reinforcementState == REINF_DONE_PENDING)
		return;

	// Get reinforcement points for this group if not acquired yet.
	if (!m_reinforcementSpotsAcquired)
	{
		m_reinforcementSpots.clear();

		m_reinforcementSpotsAllAlerted = 0;
		m_reinforcementSpotsCombat = 0;
		m_reinforcementSpotsBodyCount = 0;

		for (AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIANCHOR_REINFORCEMENT_SPOT)); it->GetObject(); it->Next())
		{
			CAIObject* pAI = static_cast<CAIObject*>(it->GetObject());
			if (pAI->GetGroupId() == m_groupID && pAI->GetEntity())
			{
				// Look up the extra properties.
				IScriptTable* pTable = pAI->GetEntity()->GetScriptTable();
				if (pTable)
				{
					CCCPOINT(CAIGroup_UpdateReinforcementLogic_A);

					SmartScriptTable props;
					if (pTable->GetValue("Properties", props))
					{
						m_reinforcementSpots.resize(m_reinforcementSpots.size() + 1);

						int whenAllAlerted = 0;
						int whenInCombat = 0;
						int groupBodyCount = 0;
						float avoidWhenTargetInRadius = 0.0f;
						int type = 0;
						props->GetValue("bWhenAllAlerted", whenAllAlerted);
						props->GetValue("bWhenInCombat", whenInCombat);
						props->GetValue("iGroupBodyCount", groupBodyCount);
						props->GetValue("eiReinforcementType", type);
						props->GetValue("AvoidWhenTargetInRadius", avoidWhenTargetInRadius);

						m_reinforcementSpots.back().whenAllAlerted = whenAllAlerted > 0;
						m_reinforcementSpots.back().whenInCombat = whenInCombat > 0;
						m_reinforcementSpots.back().groupBodyCount = groupBodyCount;
						m_reinforcementSpots.back().avoidWhenTargetInRadius = avoidWhenTargetInRadius;
						m_reinforcementSpots.back().type = type;
						m_reinforcementSpots.back().pAI = pAI;

						if (m_reinforcementSpots.back().whenAllAlerted)
							m_reinforcementSpotsAllAlerted++;
						if (m_reinforcementSpots.back().whenInCombat)
							m_reinforcementSpotsCombat++;
						if (m_reinforcementSpots.back().groupBodyCount > 0)
							m_reinforcementSpotsBodyCount++;

					}
				}
			}
		}

		m_reinforcementSpotsAcquired = true;
	}

	// Check if reinforcements should be called.
	if (!m_reinforcementSpots.empty() && m_reinforcementState != REINF_DONE)
	{
		CCCPOINT(CAIGroup_UpdateReinforcementLogic_B);

		int totalCount = 0;
		int enabledCount = 0;
		int alertedCount = 0;
		int combatCount = 0;
		int liveTargetCount = 0;
		for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
		{
			CAIActor* pAIActor = it->m_refUnit.GetAIObject();
			if (!pAIActor) continue;

			totalCount++;

			if (!pAIActor->IsEnabled()) continue;
			if (pAIActor->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			if (CPuppet* pPuppet = pAIActor->CastToCPuppet())
			{
				if (pPuppet->GetAlertness() >= 1)
					alertedCount++;
				if (pPuppet->GetAlertness() >= 2)
					combatCount++;
			}

			if (pAIActor->GetAttentionTargetType() == AITARGET_VISUAL &&
			    pAIActor->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
			{
				liveTargetCount++;
			}

			enabledCount++;
		}

		bool matchWhenAllAlerted = false;
		bool matchWhenInCombat = false;
		bool matchWhenGroupSizeLess = false;

		bool tryToCall = false;

		if (m_reinforcementState == REINF_NONE)
		{
			if (m_reinforcementSpotsAllAlerted)
			{
				if ((enabledCount <= 2 && alertedCount > 0) ||
				    (enabledCount > 2 && alertedCount > 1))
				{
					matchWhenAllAlerted = true;
					matchWhenInCombat = false;
					matchWhenGroupSizeLess = false;
					tryToCall = true;
				}
			}

			if (m_reinforcementSpotsCombat)
			{
				if ((enabledCount <= 2 && liveTargetCount > 0 && combatCount > 0) ||
				    (enabledCount > 2 && liveTargetCount > 1 && combatCount > 1))
				{
					matchWhenAllAlerted = false;
					matchWhenInCombat = true;
					matchWhenGroupSizeLess = false;
					tryToCall = true;
				}
			}
		}
		/*
		    if (m_reinforcementState == REINF_ALERTED_COMBAT)
		    {
		      if (m_reinforcementSpotsBodyCount && m_countDead > 0)
		      {
		        for (unsigned i = 0, n = m_reinforcementSpots.size(); i < n; ++i)
		        {
		          if (m_countDead >= m_reinforcementSpots[i].groupBodyCount)
		          {
		            matchWhenAllAlerted = false;
		            matchWhenInCombat = false;
		            matchWhenGroupSizeLess = true;
		            tryToCall = true;
		            break;
		          }
		        }
		      }
		    }
		 */
		if (tryToCall)
		{
			CCCPOINT(CAIGroup_UpdateReinforcementLogic_C);

			float nearestDist = FLT_MAX;
			SAIReinforcementSpot* nearestSpot = 0;
			CUnitImg* pNearestCallerImg = 0;

			Vec3 playerPos(0, 0, 0), playerViewDir(0, 0, 0);
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
			if (pPlayer)
			{
				playerPos = pPlayer->GetPos();
				playerViewDir = pPlayer->GetViewDir();
			}

			size_t reinfSpotCount = m_reinforcementSpots.size();

			for (size_t i = 0; i < reinfSpotCount; ++i)
			{
				const SAIReinforcementSpot& spot = m_reinforcementSpots[i];
				if (!spot.pAI->IsEnabled())
					continue;

				if (matchWhenAllAlerted && !spot.whenAllAlerted) continue;
				if (matchWhenInCombat && !spot.whenInCombat) continue;
				if (matchWhenGroupSizeLess && m_countDead >= spot.groupBodyCount) continue;

				const Vec3& reinfSpot = spot.pAI->GetPos();
				const float reinfRadiusSqr = sqr(spot.pAI->GetRadius());
				const float avoidWhenTargetInRadiusSqr = sqr(spot.avoidWhenTargetInRadius);

				for (TUnitList::iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
				{
					CAIActor* const pAIActor = CastToCPuppetSafe(it->m_refUnit.GetAIObject());
					if (!pAIActor) continue;
					if (!pAIActor->IsEnabled()) continue;
					if ((GetAISystem()->GetFrameStartTime() - it->m_lastReinforcementTime).GetSeconds() < 3.f) continue;
					IAIActorProxy* pProxy = pAIActor->GetProxy();
					if (!pProxy || pProxy->IsCurrentBehaviorExclusive()) continue;

					if (CPuppet* const pPuppet = pAIActor->CastToCPuppet())
					{
						if (matchWhenAllAlerted && pPuppet->GetAlertness() < 1) continue;
						if (matchWhenInCombat && pPuppet->GetAlertness() < 2) continue;
					}

					IAIObject* pAttTarget = pAIActor->GetAttentionTarget();
					EAITargetType eAttTargetType = pAIActor->GetAttentionTargetType();
					EAITargetThreat eAttTargetThreat = pAIActor->GetAttentionTargetThreat();

					// Skip reinforcement spots that are too close the attention target.
					if (pAttTarget)
					{
						if ((eAttTargetType == AITARGET_VISUAL || eAttTargetType == AITARGET_MEMORY) &&
						    eAttTargetThreat == AITHREAT_AGGRESSIVE &&
						    Distance::Point_PointSq(reinfSpot, pAttTarget->GetPos()) < avoidWhenTargetInRadiusSqr)
						{
							continue;
						}
					}

					const Vec3& vPos = pAIActor->GetPos();
					float distSqr = Distance::Point_PointSq(vPos, reinfSpot);

					CCCPOINT(CAIGroup_UpdateReinforcementLogic_D);

					if (distSqr < reinfRadiusSqr)
					{
						float dist = sqrtf(distSqr);

						if (!pAttTarget || pProxy->GetLinkedVehicleEntityId())
						{
							dist += 100.0f;
						}
						else
						{
							const float preferredCallDist = 25.0f;
							float bestDist = fabsf(Distance::Point_Point(vPos, pAttTarget->GetPos()) - preferredCallDist);

							if (eAttTargetType == AITARGET_VISUAL && eAttTargetThreat == AITHREAT_AGGRESSIVE)
								bestDist *= 0.5f;
							else if (eAttTargetThreat == AITHREAT_THREATENING)
								bestDist *= 0.75f;

							dist += bestDist;
						}

						// Prefer puppet visible to the player.
						Vec3 dirPlayerToPuppet = vPos - playerPos;
						dirPlayerToPuppet.NormalizeSafe();
						float dot = playerViewDir.Dot(dirPlayerToPuppet);
						dist += (1 - sqr((dot + 1) / 2)) * 25.0f;

						if (dist < nearestDist)
						{
							nearestDist = dist;
							nearestSpot = &m_reinforcementSpots[i];
							//							nearestCaller = pPuppet;
							pNearestCallerImg = &(*it);
						}
					}
				}
			}

			if (pNearestCallerImg && nearestSpot)
			{
				// Change state
				if (m_reinforcementState == REINF_NONE)
					/*					m_reinforcementState = REINF_ALERTED_COMBAT_PENDING;
					        else if (m_reinforcementState == REINF_ALERTED_COMBAT)*/
					m_reinforcementState = REINF_DONE_PENDING;

				// Tell the agent to call reinforcements.
				CAIActor* const pUnit = CastToCAIActorSafe(pNearestCallerImg->m_refUnit.GetAIObject());
				AISignalExtraData* pData = new AISignalExtraData;
				pData->nID = nearestSpot->pAI->GetEntityID();
				pData->iValue = nearestSpot->type;
				pUnit->SetSignal(1, "OnCallReinforcements", pUnit->GetEntity(), pData);
				pNearestCallerImg->m_lastReinforcementTime = GetAISystem()->GetFrameStartTime();

#ifdef CRYAISYSTEM_DEBUG
				m_DEBUG_reinforcementCalls.push_back(SAIRefinforcementCallDebug(pUnit->GetPos() + Vec3(0, 0, 0.3f), nearestSpot->pAI->GetPos() + Vec3(0, 0, 0.3f), 7.0f, nearestSpot->pAI->GetName()));
#endif
				m_refReinfPendingUnit = StaticCast<CPuppet>(pNearestCallerImg->m_refUnit);
				// Remove the spot (when confirmed)
				m_pendingDecSpotsAllAlerted = nearestSpot->whenAllAlerted;
				m_pendingDecSpotsCombat = nearestSpot->whenInCombat;
				m_pendingDecSpotsBodyCount = nearestSpot->groupBodyCount > 0;

				CCCPOINT(CAIGroup_UpdateReinforcementLogic_E);
			}
		}
	}

#ifdef CRYAISYSTEM_DEBUG
	for (unsigned i = 0; i < m_DEBUG_reinforcementCalls.size(); )
	{
		m_DEBUG_reinforcementCalls[i].t -= gEnv->pTimer->GetFrameTime();
		if (m_DEBUG_reinforcementCalls[i].t < 0.0f)
		{
			m_DEBUG_reinforcementCalls[i] = m_DEBUG_reinforcementCalls.back();
			m_DEBUG_reinforcementCalls.pop_back();
		}
		else
			++i;
	}
#endif
}

//====================================================================
// UpdateGroupCountStatus
//====================================================================
void CAIGroup::UpdateGroupCountStatus()
{
	CAISystem* pAISystem(GetAISystem());
	// Count number of active and non active members.
	int count = 0;
	int countEnabled = 0;
	for (AIObjects::iterator it = pAISystem->m_mapGroups.find(m_groupID); it != pAISystem->m_mapGroups.end() && it->first == m_groupID; )
	{
		CAIObject* pObject = it->second.GetAIObject();

		// (MATT) These are weak refs so objects may have been removed {2009/03/25}
		if (!pObject)
		{
			pAISystem->m_mapGroups.erase(it++);
			continue;
		}

		int type = pObject->GetType();
		if (type == AIOBJECT_ACTOR)
		{
			if (pObject->IsEnabled())
				countEnabled++;
			count++;
		}
		else if (type == AIOBJECT_VEHICLE)
		{
			CAIVehicle* pVehicle = pObject->CastToCAIVehicle();
			if (pVehicle->IsEnabled() && pVehicle->IsDriverInside())
				countEnabled++;
			count++;
		}

		++it;
	}

	// Store the current state
	m_count = count;
	m_countEnabled = countEnabled;
	// Keep track on the maximum
	m_countMax = max(m_countMax, count);
	m_countEnabledMax = max(m_countEnabledMax, countEnabled);

	// add smart objects state "GroupHalved" to alive group members
	// (MATT) Rather specific :/ Does anyone use it?  {2009/03/25}
	if (m_countEnabled == m_countEnabledMax / 2)
	{
		AIObjects::iterator it, itEnd = pAISystem->m_mapGroups.end();
		for (it = pAISystem->m_mapGroups.lower_bound(m_groupID); it != itEnd && it->first == m_groupID; ++it)
		{
			// Objects will now already be validated or removed
			CAIObject* pObject = it->second.GetAIObject();
			if (pObject->IsEnabled() && pObject->GetType() != AIOBJECT_LEADER)
			{
				if (IEntity* pEntity = pObject->GetEntity())
					gAIEnv.pSmartObjectManager->AddSmartObjectState(pEntity, "GroupHalved");
			}
		}
	}
}

//====================================================================
// GetGroupCount
//====================================================================
int CAIGroup::GetGroupCount(int flags)
{
	if (flags == 0 || flags == IAISystem::GROUP_ALL)
		return m_count;
	else if (flags == IAISystem::GROUP_ENABLED)
		return m_countEnabled;
	else if (flags == IAISystem::GROUP_MAX)
		return m_countMax;
	else if (flags == (IAISystem::GROUP_ENABLED | IAISystem::GROUP_MAX))
		return m_countEnabledMax;
	return 0;
}

//====================================================================
// Reset
//====================================================================
void CAIGroup::Reset()
{
	CCCPOINT(CAIGroup_Reset);

	m_count = 0;
	m_countMax = 0;
	m_countEnabled = 0;
	m_countEnabledMax = 0;
	m_countDead = 0;
	m_countCheckedDead = 0;
	m_bUpdateEnemyStats = true;

	for (TUnitList::iterator itrUnit(m_Units.begin()); itrUnit != m_Units.end(); ++itrUnit)
		itrUnit->Reset();

	stl::free_container(m_groupScopes);

	stl::free_container(m_reinforcementSpots);
	m_reinforcementSpotsAcquired = false;
	m_reinforcementState = REINF_NONE;
	m_refReinfPendingUnit.Reset();

#ifdef CRYAISYSTEM_DEBUG
	m_DEBUG_reinforcementCalls.clear();
#endif
}

//====================================================================
// Serialize
//====================================================================
void CAIGroup::Serialize(TSerialize ser)
{
	CCCPOINT(CAIGroup_Serialize);

	// TODO: serialize and create tactical evals
	char Name[256];
	cry_sprintf(Name, "AIGroup-%d", m_groupID);

	ser.BeginGroup(Name);

	ser.Value("AIgroup_GroupID", m_groupID);

	m_bUpdateEnemyStats = true;

	ser.Value("m_count", m_count);
	ser.Value("m_countMax", m_countMax);
	ser.Value("m_countEnabled", m_countEnabled);
	ser.Value("m_countEnabledMax", m_countEnabledMax);
	ser.Value("m_countDead", m_countDead);
	ser.Value("m_countCheckedDead", m_countCheckedDead);

	//	TSetAIObjects	m_Targets;
	ser.Value("m_vEnemyPositionKnown", m_vEnemyPositionKnown);
	ser.Value("m_vAverageEnemyPosition", m_vAverageEnemyPosition);
	ser.Value("m_vAverageLiveEnemyPosition", m_vAverageLiveEnemyPosition);

	//see below for m_pLeader Serialization
	{
		if (ser.BeginOptionalGroup("m_Units", true))
		{
			if (ser.IsWriting())
			{
				uint32 count = m_Units.size();
				ser.Value("Size", count);
				for (TUnitList::iterator iter = m_Units.begin(); iter != m_Units.end(); ++iter)
				{
					ser.BeginGroup("i");
					CUnitImg& value = *iter;
					ser.Value("v", value);
					ser.EndGroup();
				}
			}
			else
			{
				m_Units.clear();
				uint32 count = 0;
				ser.Value("Size", count);
				while (count--)
				{
					ser.BeginGroup("i");
					m_Units.push_back(CUnitImg());
					ser.Value("v", m_Units.back());
					ser.EndGroup();
				}
			}
			ser.EndGroup();
		}
	}

	ser.Value("m_groupScopes", m_groupScopes);

	if (ser.IsReading())
	{
		// Clear reinforcement spots.
		m_reinforcementSpotsAcquired = false;
		m_reinforcementSpots.clear();
	}

	SerializeWeakRefVector<const CAIObject>(ser, "m_Targets", m_Targets);

	ser.EnumValue("m_reinforcementState", m_reinforcementState, REINF_NONE, LAST_REINF);

	ser.Value("m_pendingDecSpotsAllAlerted", m_pendingDecSpotsAllAlerted);
	ser.Value("m_pendingDecSpotsCombat", m_pendingDecSpotsCombat);
	ser.Value("m_pendingDecSpotsBodyCount", m_pendingDecSpotsBodyCount);
	m_refReinfPendingUnit.Serialize(ser, "m_refReinfPendingUnit");

	ser.EndGroup(); // AIGroup
}

//====================================================================
// DebugDraw
//====================================================================
void CAIGroup::DebugDraw()
{
	// Debug draw reinforcement logic.
	if (gAIEnv.CVars.DebugDrawReinforcements == m_groupID)
	{

		int totalCount = 0;
		int enabledCount = 0;
		int alertedCount = 0;
		int combatCount = 0;
		int liveTargetCount = 0;
		for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
		{
			CAIActor* pAIActor = it->m_refUnit.GetAIObject();
			if (!pAIActor) continue;
			totalCount++;

			if (!pAIActor->IsEnabled()) continue;
			if (pAIActor->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			if (CPuppet* pPuppet = pAIActor->CastToCPuppet())
			{
				if (pPuppet->GetAlertness() >= 1)
					alertedCount++;
				if (pPuppet->GetAlertness() >= 2)
					combatCount++;
			}

			if (pAIActor->GetAttentionTargetType() == AITARGET_VISUAL &&
			    pAIActor->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
			{
				liveTargetCount++;
			}

			enabledCount++;
		}

		CDebugDrawContext dc;

		for (unsigned i = 0, n = m_reinforcementSpots.size(); i < n; ++i)
		{
			if (m_reinforcementSpots[i].pAI->IsEnabled())
			{
				string text;
				char msg[64];

				if (m_reinforcementState == REINF_NONE)
				{
					if (m_reinforcementSpots[i].whenAllAlerted)
					{
						cry_sprintf(msg, "ALERTED: Alerted >= Enabled | %d >= %d", alertedCount, enabledCount);
						text += msg;
						if (alertedCount >= enabledCount)
							text += " Active!\n";
						else
							text += "\n";
					}

					if (m_reinforcementSpots[i].whenInCombat)
					{
						if (enabledCount > 1)
							cry_sprintf(msg, "COMBAT: InCombat > 0 && LiveTarget > 1 | %d > 0 && %d > 1", combatCount, liveTargetCount);
						else
							cry_sprintf(msg, "COMBAT: InCombat > 0 && LiveTarget > 0 | %d > 0 && %d > 0", combatCount, liveTargetCount);
						text += msg;

						if (combatCount > 0 && ((enabledCount > 1 && liveTargetCount > 1) || (enabledCount == 1 && liveTargetCount > 0)))
							text += " Active!\n";
						else
							text += "\n";
					}
				}

				if (m_reinforcementState == REINF_NONE /*|| m_reinforcementState == REINF_ALERTED_COMBAT*/)
				{
					if (m_reinforcementSpots[i].groupBodyCount > 0)
					{
						cry_sprintf(msg, "SIZE: Bodies < Count | %d < %d",
						            m_reinforcementSpots[i].groupBodyCount, m_countDead);
						text += msg;

						if (m_countDead >= m_reinforcementSpots[i].groupBodyCount)
							text += " >>>\n";
						else
							text += "\n";
					}
				}
				dc->Draw3dLabel(m_reinforcementSpots[i].pAI->GetPos() + Vec3(0, 0, 1), 1.5f, "%s", text.c_str());
			}
			else
			{
				dc->Draw3dLabel(m_reinforcementSpots[i].pAI->GetPos() + Vec3(0, 0, 1), 1.0f, "Disabled");
			}

			ColorB color, colorTrans;
			int c = m_reinforcementSpots[i].type % 3;
			if (c == 0)
				color.set(255, 64, 64, 255);
			else if (c == 1)
				color.set(64, 255, 64, 255);
			else if (c == 2)
				color.set(64, 64, 255, 255);
			colorTrans = color;
			colorTrans.a = 64;

			if (!m_reinforcementSpots[i].pAI->IsEnabled())
			{
				color.a /= 4;
				colorTrans.a /= 4;
			}

			dc->DrawCylinder(m_reinforcementSpots[i].pAI->GetPos() + Vec3(0, 0, 0.5f), Vec3(0, 0, 1), 0.2f, 1.0f, color);
			dc->DrawRangeCircle(m_reinforcementSpots[i].pAI->GetPos(), m_reinforcementSpots[i].pAI->GetRadius(), 1.0f, colorTrans, color, true);

			if (m_reinforcementSpots[i].pAI->IsEnabled())
			{
				for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
				{
					CPuppet* pPuppet = CastToCPuppetSafe(it->m_refUnit.GetAIObject());
					if (!pPuppet) continue;
					totalCount++;
					if (!pPuppet->IsEnabled()) continue;
					if (pPuppet->GetProxy()->IsCurrentBehaviorExclusive()) continue;

					if (Distance::Point_PointSq(pPuppet->GetPos(), m_reinforcementSpots[i].pAI->GetPos()) < sqr(m_reinforcementSpots[i].pAI->GetRadius()))
						dc->DrawLine(pPuppet->GetPos(), color + Vec3(0, 0, 0.5f), m_reinforcementSpots[i].pAI->GetPos() + Vec3(0, 0, 0.5f), color);
				}
			}
		}

#ifdef CRYAISYSTEM_DEBUG
		for (unsigned i = 0, n = m_DEBUG_reinforcementCalls.size(); i < n; ++i)
		{
			const Vec3& pos = m_DEBUG_reinforcementCalls[i].from;
			Vec3 dir = m_DEBUG_reinforcementCalls[i].to - m_DEBUG_reinforcementCalls[i].from;
			dc->DrawArrow(pos, dir, 0.5f, ColorB(255, 255, 255));

			float len = dir.NormalizeSafe();
			dir *= min(2.0f, len);

			dc->Draw3dLabel(pos + dir, 1.0f, "%s", m_DEBUG_reinforcementCalls[i].performer.c_str());
		}
#endif
	}
}

//====================================================================
// NotifyReinfDone
//====================================================================
void CAIGroup::NotifyReinfDone(const IAIObject* obj, bool isDone)
{
	CAIObject* pReinfPendingUnit = m_refReinfPendingUnit.GetAIObject();
	if (!pReinfPendingUnit)
		return;

	if (pReinfPendingUnit != obj)
	{
		AIWarning("CAIGroup::NotifyReinfDone - pending unit missMatch (internal %s ; passed %s)", pReinfPendingUnit->GetName(), obj->GetName());
		AIAssert(0);
		return;
	}
	if (/*m_reinforcementState != REINF_ALERTED_COMBAT_PENDING &&*/
	  m_reinforcementState != REINF_DONE_PENDING)
	{
		AIWarning("CAIGroup::NotifyReinfDone - not pending state (Puppet %s ; state %d)", obj->GetName(), m_reinforcementState);
		AIAssert(0);
		return;
	}
	if (isDone)
	{
		// Change state
		/*if (m_reinforcementState == REINF_ALERTED_COMBAT_PENDING)
		   m_reinforcementState = REINF_ALERTED_COMBAT;
		   else */if (m_reinforcementState == REINF_DONE_PENDING)
			m_reinforcementState = REINF_DONE;

		// Remove the spot
		if (m_pendingDecSpotsAllAlerted)
			m_reinforcementSpotsAllAlerted--;
		if (m_pendingDecSpotsCombat)
			m_reinforcementSpotsCombat--;
		if (m_pendingDecSpotsBodyCount)
			m_reinforcementSpotsBodyCount--;
	}
	else
	{
		// Change state
		//if (m_reinforcementState == REINF_ALERTED_COMBAT_PENDING)
		m_reinforcementState = REINF_NONE;
		//else if (m_reinforcementState == REINF_DONE_PENDING)
		//	m_reinforcementState = REINF_ALERTED_COMBAT;
	}
	m_refReinfPendingUnit.Reset();
}

bool CAIGroup::EnterGroupScope(const CAIActor* pMember, const GroupScopeID scopeID, const uint32 allowedConcurrentUsers)
{
	TUnitList::iterator theUnit = std::find(m_Units.begin(), m_Units.end(), pMember);
	IF_UNLIKELY (theUnit == m_Units.end())
		return false;

	IF_UNLIKELY (GetAmountOfActorsInScope(scopeID) >= allowedConcurrentUsers)
	{
		return false;
	}

	const tAIObjectID memberId = pMember->GetAIObjectID();
	IF_UNLIKELY (m_groupScopes.find(memberId) != m_groupScopes.end())
	{
		return false;
	}

	m_groupScopes[scopeID].insert(memberId);
	return true;
}

void CAIGroup::LeaveGroupScope(const CAIActor* pMember, const GroupScopeID scopeID)
{
	GroupScopes::iterator scopeIt = m_groupScopes.find(scopeID);
	if (scopeIt == m_groupScopes.end())
	{
		return;
	}

	const tAIObjectID memberId = pMember->GetAIObjectID();
	MembersInScope& membersInScope = scopeIt->second;
	MembersInScope::iterator memberInScopeIt = membersInScope.find(memberId);
	if (memberInScopeIt == membersInScope.end())
	{
		return;
	}

	membersInScope.erase(memberInScopeIt);
	if (membersInScope.empty())
	{
		m_groupScopes.erase(scopeIt);
	}
}

void CAIGroup::LeaveAllGroupScopesForActor(const CAIActor* pMember)
{
	const tAIObjectID memberId = pMember->GetAIObjectID();

	GroupScopes::iterator scopeIt = m_groupScopes.begin();
	while (scopeIt != m_groupScopes.end())
	{
		MembersInScope& membersInScope = scopeIt->second;
		MembersInScope::iterator memberInScopeIt = membersInScope.find(memberId);
		if (memberInScopeIt != membersInScope.end())
		{
			membersInScope.erase(memberInScopeIt);
			if (membersInScope.empty())
			{
				m_groupScopes.erase(scopeIt++); // Delete current entry and move on to the next.
			}
			else
			{
				++scopeIt;
			}
		}
		else
		{
			++scopeIt;
		}
	}
}

uint32 CAIGroup::GetAmountOfActorsInScope(const GroupScopeID scopeID) const
{
	const GroupScopes::const_iterator scopeIt = m_groupScopes.find(scopeID);
	if (scopeIt == m_groupScopes.end())
	{
		// No-one has entered the scope, so it doesn't exist.
		return 0;
	}

	const MembersInScope& membersInScope = scopeIt->second;
	return membersInScope.size();
}

GroupScopeID CAIGroup::GetGroupScopeId(const char* scopeName)
{
	IF_UNLIKELY (scopeName == NULL)
	{
		assert(false);
		return (GroupScopeID)0;
	}
	return (GroupScopeID)(CCrc32::ComputeLowercase(scopeName));
}
