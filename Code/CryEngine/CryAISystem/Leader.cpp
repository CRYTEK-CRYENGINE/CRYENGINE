// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Leader.cpp

   Description: Class Implementation for CLeader

   -------------------------------------------------------------------------
   History:
   Created by Kirill Bulatsev
   - 01:06:2005   : serialization support added

 *********************************************************************/

#include "StdAfx.h"
#include "Leader.h"
#include "LeaderAction.h"
#include "CAISystem.h"
#include <CrySystem/ISystem.h>
#include "Puppet.h"
#include "AIPlayer.h"
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/IPhysics.h>
#include <set>
#include <limits>
#include <CrySystem/IConsole.h>
#include <CryNetwork/ISerialize.h>
#include "Formation/AIFormationDescriptor.h"

//
//----------------------------------------------------------------------------------------------------
CLeader::CLeader(int iGroupID) :
	m_pCurrentAction(NULL)
	, m_vForcedPreferredPos(ZERO)
	, m_bKeepEnabled(false)
	, m_vMarkedPosition(0, 0, 0)
{
	_fastcast_CLeader = true;

	m_groupId = iGroupID;
	SetName("TeamLeader");
	SetAssociation(NILREF);
	m_bLeaderAlive = true;
	m_pGroup = GetAISystem()->GetAIGroup(iGroupID);
}

//
//----------------------------------------------------------------------------------------------------
CLeader::~CLeader(void)
{

	AbortExecution();
	ReleaseFormation();
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::SetAssociation(CWeakRef<CAIObject> refAssociation)
{
	CCCPOINT(CLeader_SetAssociation);

	CAIObject::SetAssociation(refAssociation);
	CAIObject* pObject = refAssociation.GetAIObject();
	if (pObject && pObject->IsEnabled())
		m_bLeaderAlive = true;
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::Reset(EObjectResetType type)
{

	CAIObject::Reset(type);
	AbortExecution();
	ReleaseFormation();
	m_szFormationDescriptor.clear();
	m_refEnemyTarget.Reset();
	m_vForcedPreferredPos.Set(0, 0, 0);
	m_bLeaderAlive = true;
	m_State.ClearSignals();
}

//
//----------------------------------------------------------------------------------------------------
bool CLeader::IsPlayer() const
{
	CAIObject* pObject = GetAssociation().GetAIObject();
	if (pObject)
		return pObject->GetType() == AIOBJECT_PLAYER;
	return false;
}

//
//----------------------------------------------------------------------------------------------------
Vec3 CLeader::GetPreferedPos() const
{
	if (!m_vForcedPreferredPos.IsZero())
		return m_vForcedPreferredPos;

	CCCPOINT(CLeader_GetPreferedPos);

	CAIObject* pLeaderObject = GetAssociation().GetAIObject();
	if (!pLeaderObject)
		return m_pGroup->GetAveragePosition();
	else if (pLeaderObject->GetType() == AIOBJECT_PLAYER)
	{
		return m_pGroup->GetAveragePosition();
	}
	else
		return pLeaderObject->GetPos();
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::Update(EUpdateType type)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!GetAssociation().ValidateOrReset() && !m_bKeepEnabled)
		return;

	UpdateEnemyStats();

	while (!m_State.vSignals.empty())
	{
		AISIGNAL sstruct = m_State.vSignals.back();
		m_State.vSignals.pop_back();
		ProcessSignal(sstruct);
	}

	for (TUnitList::iterator unitItr = m_pGroup->GetUnits().begin(); unitItr != m_pGroup->GetUnits().end(); ++unitItr)
	{
		CCCPOINT(CLeader_Update);

		CUnitImg& curUnit = (*unitItr);
		if (!curUnit.IsPlanFinished())
		{
			if (curUnit.Idle() && !curUnit.IsBlocked())
				curUnit.ExecuteTask();
		}
	}

	CLeaderAction::eActionUpdateResult eUpdateResult;
	if (m_pCurrentAction && (eUpdateResult = m_pCurrentAction->Update()) != CLeaderAction::ACTION_RUNNING)
	{
		// order completed or not issued
		bool search = m_pCurrentAction->GetType() == LA_ATTACK && !IsPlayer() && !m_pGroup->GetAttentionTarget(true, true).IsValid();
		ELeaderAction iActionType = m_pCurrentAction->GetType();
		ELeaderActionSubType iActionSubType = m_pCurrentAction->GetSubType();
		// get previous leaderAction's unit properties
		uint32 unitProperties = m_pCurrentAction->GetUnitProperties();
		AbortExecution();
		// inform the puppet that the order is done
		CWeakRef<CAIObject> refTarget = m_pGroup->GetAttentionTarget(true);
		CAIObject* pTarget = refTarget.GetAIObject();
		AISignalExtraData* pData = new AISignalExtraData;
		pData->iValue = iActionType;
		pData->iValue2 = iActionSubType;
		if (pTarget)
		{
			// target id
			IEntity* pTargetEntity = NULL;
			if (CPuppet* pPuppet = pTarget->CastToCPuppet())
				pTargetEntity = pPuppet->GetEntity();
			else if (CAIPlayer* pPlayer = pTarget->CastToCAIPlayer())
				pTargetEntity = pPlayer->GetEntity();
			//target name
			if (pTargetEntity)
				pData->nID = pTargetEntity->GetId();
			pData->SetObjectName(pTarget->GetName());
			//target distance
			Vec3 avgPos = m_pGroup->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, unitProperties);
			pData->fValue = (avgPos - m_pGroup->GetEnemyAveragePosition()).GetLength();
			pData->point = m_pGroup->GetEnemyAveragePosition();

			CCCPOINT(CLeader_Update_A);
		}

		int sigFilter;
		CAIActor* pDestActor = CastToCAIActorSafe(GetAssociation().GetAIObject());
		if (pDestActor && pDestActor->IsEnabled())
			sigFilter = SIGNALFILTER_SENDER;
		else
		{
			sigFilter = SIGNALFILTER_SUPERGROUP;
			if (!m_pGroup->GetUnits().empty())
			{
				TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();
				pDestActor = itUnit->m_refUnit.GetAIObject();
			}
		}

		if (pDestActor)
		{
			if (eUpdateResult == CLeaderAction::ACTION_DONE)
				GetAISystem()->SendSignal(sigFilter, 10, "OnLeaderActionCompleted", pDestActor, pData);
			else
				GetAISystem()->SendSignal(sigFilter, 10, "OnLeaderActionFailed", pDestActor, pData);
		}

	}

}

//
//----------------------------------------------------------------------------------------------------
void CLeader::ProcessSignal(AISIGNAL& signal)
{
	if (m_pCurrentAction && m_pCurrentAction->ProcessSignal(signal))
		return;

	CCCPOINT(CLeader_ProcessSignal);

	AISignalExtraData data;
	if (signal.pEData)
	{
		data = *(AISignalExtraData*)signal.pEData;
		delete (AISignalExtraData*)signal.pEData;
		signal.pEData = NULL;
	}

	if (signal.Compare(gAIEnv.SignalCRCs.m_nAIORD_ATTACK))
	{
		ELeaderActionSubType attackType = static_cast<ELeaderActionSubType>(data.iValue);
		LeaderActionParams params;
		params.type = LA_ATTACK;
		params.subType = attackType;
		params.unitProperties = data.iValue2;
		params.id = data.nID;
		params.pAIObject = gAIEnv.pAIObjectManager->GetAIObjectByName(data.GetObjectName());
		params.fDuration = data.fValue;
		params.vPoint = data.point;
		params.name = data.GetObjectName();
		Attack(&params);
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nAIORD_SEARCH))
	{
		Vec3 startPoint = data.point;
		Search((startPoint.IsZero() ? m_pGroup->GetEnemyPositionKnown() : startPoint), data.fValue, (data.iValue ? data.iValue : UPR_ALL), data.iValue2);
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnScaleFormation))
	{
		CCCPOINT(CLeader_ProcessSignal_ScaleFormation);

		float fScale = data.fValue;
		CAIObject* pFormationOwner = m_refFormationOwner.GetAIObject();
		if (pFormationOwner)
			pFormationOwner->m_pFormation->SetScale(fScale);
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nRPT_LeaderDead))
	{
		if (!m_pCurrentAction) // if there is a current action, the OnLeaderDied signal will be sent after the action finishes
		{
			CAIObject* pAILeader = GetAssociation().GetAIObject();
			if (pAILeader && m_bLeaderAlive)
			{
				GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnLeaderDied", pAILeader);
				m_bLeaderAlive = false;
			}
		}
	}
	else if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(signal.senderID))
	{
		if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitDied))
		{
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			if (m_pCurrentAction)
				m_pCurrentAction->DeadUnitNotify(pActor);

			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
			if (senderUnit != m_pGroup->GetUnits().end())
				m_pGroup->RemoveMember(pActor);
			// TO DO: remove use action from list if it's owned by this unit
		}
		else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitBusy))
		{
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

			if (senderUnit != m_pGroup->GetUnits().end())
			{
				if (m_pCurrentAction)
					m_pCurrentAction->BusyUnitNotify(*senderUnit);

				// TODO: busy units will be removed by now...
				// Now it isn't so bad because they will stay busy forever.
				// However, it's only a temporary solution!
				// They should still be there but marked as busy.
				m_pGroup->RemoveMember(pActor);
			}
		}
		else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitSuspended))
		{
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

			if (senderUnit != m_pGroup->GetUnits().end())
			{
				if (m_pCurrentAction)
					m_pCurrentAction->BusyUnitNotify(*senderUnit);
			}
		}
		else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnUnitResumed))
		{
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

			if (senderUnit != m_pGroup->GetUnits().end())
			{
				if (m_pCurrentAction)
					m_pCurrentAction->ResumeUnit(*senderUnit);
			}
		}
		else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnSetUnitProperties))
		{
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

			if (senderUnit != m_pGroup->GetUnits().end())
				senderUnit->SetProperties(data.iValue);
		}
	}

	if (m_pCurrentAction)
	{
		if (signal.Compare(gAIEnv.SignalCRCs.m_nAIORD_REPORTDONE))
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(signal.senderID))
			{
				CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
				TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
				if (senderUnit != m_pGroup->GetUnits().end())
				{
					(*senderUnit).TaskExecuted();
					(*senderUnit).ExecuteTask();
				}
			}
		}
		else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnAbortAction))
		{
			AbortExecution();
			CLeader* pAILeader = CastToCLeaderSafe(GetAssociation().GetAIObject());
			if (pAILeader)
				pAILeader->SetSignal(1, "OnAbortAction", 0, 0, gAIEnv.SignalCRCs.m_nOnAbortAction);
		}
		else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnFormationPointReached)) /*if(!strcmp(signal.strText, "OnFormationPointReached"))*/
		{
			/*
			   // TO DO: what to do with this?
			   IEntity* pEntity = (IEntity*)signal.pSender;
			   assert(pEntity);
			   CAIObject* pAIObj = (CAIObject*)pEntity->GetAI();
			   assert(pAIObj);
			   TUnitList::iterator itEnd =  m_pGroup->GetUnits().end();
			 */
		}
	}
	else if (signal.Compare(gAIEnv.SignalCRCs.m_nOnKeepEnabled))
		m_bKeepEnabled = true;
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::AbortExecution(int priority)
{

	ClearAllPlannings(UPR_ALL, priority);

	if (m_pCurrentAction)
	{
		CCCPOINT(CLeader_AbortExecution);

		delete m_pCurrentAction;
		m_pCurrentAction = NULL;
	}
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::ClearAllPlannings(uint32 unitProp, int priority)
{
	for (TUnitList::iterator unitItr = m_pGroup->GetUnits().begin(); unitItr != m_pGroup->GetUnits().end(); ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		CAIObject* pUnit = unit.m_refUnit.GetAIObject();
		if (pUnit->IsEnabled() && (unit.GetProperties() & unitProp))
		{
			CUnitImg& curUnit = (*unitItr);
			curUnit.ClearPlanning(priority);

			CCCPOINT(CLeader_ClearAllPlannings);
		}
	}
}

//
//----------------------------------------------------------------------------------------------------
bool CLeader::IsIdle() const
{
	for (TUnitList::const_iterator itUnit = m_pGroup->GetUnits().begin(); itUnit != m_pGroup->GetUnits().end(); ++itUnit)
		if (!itUnit->Idle() && itUnit->m_refUnit.GetAIObject()->IsEnabled())
			return false;
	return true;
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::Attack(const LeaderActionParams* pParams)
{

	AbortExecution(PRIORITY_NORMAL);
	LeaderActionParams defaultParams(LA_ATTACK, LAS_DEFAULT);
	m_pCurrentAction = CreateAction(pParams ? pParams : &defaultParams);
	m_pCurrentAction->SetPriority(PRIORITY_NORMAL);
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::Search(const Vec3& targetPos, float searchDistance, uint32 unitProperties, int searchAIObjectType)
{
	if (m_pCurrentAction && m_pCurrentAction->GetType() == LA_SEARCH)
		return;
	AbortExecution(PRIORITY_NORMAL);
	LeaderActionParams params(LA_SEARCH, LAS_DEFAULT);
	params.unitProperties = unitProperties;
	params.fSize = searchDistance;
	params.vPoint = targetPos;
	params.iValue = searchAIObjectType;
	m_pCurrentAction = CreateAction(&params);
	m_pCurrentAction->SetPriority(PRIORITY_NORMAL);
}

void CLeader::OnEnemyStatsUpdated(const Vec3& avgEnemyPos, const Vec3& oldAvgEnemyPos)
{
	if (!IsPlayer())
	{
		if (avgEnemyPos.IsZero() && !oldAvgEnemyPos.IsZero())
		{
			CLeader* pAILeader = CastToCLeaderSafe(GetAssociation().GetAIObject());
			if (pAILeader)
				pAILeader->SetSignal(1, "OnNoGroupTarget", 0, 0, gAIEnv.SignalCRCs.m_nOnNoGroupTarget);
		}
		else if (!avgEnemyPos.IsZero() && oldAvgEnemyPos.IsZero())
		{
			CAIObject* pAILeader = GetAssociation().GetAIObject();
			CWeakRef<CAIObject> refTarget = m_pGroup->GetAttentionTarget(true, true);
			CAIObject* pTarget = refTarget.GetAIObject();

			if (pTarget && pAILeader)
			{
				IEntity* pEntity = pTarget->GetEntity();
				if (pEntity)
				{
					AISignalExtraData* pData = new AISignalExtraData;
					pData->nID = pEntity->GetId();
					GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 0, "OnGroupTarget", pAILeader, pData);
				}
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------
void CLeader::UpdateEnemyStats()
{
	// update beacon
	if (m_pCurrentAction)
		m_pCurrentAction->UpdateBeacon();
	else
	{
		CWeakRef<CAIObject> refBeaconUser = GetAssociation();
		if (!refBeaconUser.IsValid() || IsPlayer())
			for (TUnitList::const_iterator it = m_pGroup->GetUnits().begin(); it != m_pGroup->GetUnits().end(); ++it)
			{
				CAIObject* pUnit = it->m_refUnit.GetAIObject();
				if (pUnit && it->m_refUnit != refBeaconUser && pUnit->IsEnabled())
				{
					refBeaconUser = it->m_refUnit;
					break;
				}
			}

		if (refBeaconUser.IsNil())
			return;

		const Vec3& avgEnemyPos(m_pGroup->GetEnemyAveragePosition());
		const Vec3& avgLiveEnemyPos(m_pGroup->GetLiveEnemyAveragePosition());

		if (!avgLiveEnemyPos.IsZero())
			GetAISystem()->UpdateBeacon(GetGroupId(), avgLiveEnemyPos, refBeaconUser.GetAIObject());
		else if (!avgEnemyPos.IsZero())
			GetAISystem()->UpdateBeacon(GetGroupId(), avgEnemyPos, refBeaconUser.GetAIObject());
		Vec3 vEnemyDir = m_pGroup->GetEnemyAverageDirection(true, false);
		if (!vEnemyDir.IsZero())
		{
			IAIObject* pBeacon = GetAISystem()->GetBeacon(GetGroupId());
			if (pBeacon)
			{
				pBeacon->SetEntityDir(vEnemyDir);
				pBeacon->SetMoveDir(vEnemyDir);
			}
		}

		CCCPOINT(CLeader_UpdateEnemyStats);
	}
}

//
//--------------------------------------------------------------------------------------------
void CLeader::OnObjectRemoved(CAIObject* pObject)
{
	CAIActor::OnObjectRemoved(pObject);
}

//---------------------------------------------------------------------------------------
bool CLeader::LeaderCreateFormation(const char* szFormationDescr, const Vec3& vTargetPos, bool bIncludeLeader, uint32 unitProp, CAIObject* pOwner)
{
	// (MATT) Note that this code seems to imply that pOwner must be an Actor - can the argument change? {2009/02/13}

	ReleaseFormation();
	int unitCount = m_pGroup->GetUnitCount(unitProp);
	if (unitCount == 0)
		return false;

	// Select formation owner
	CWeakRef<CAIObject> refAILeader = GetAssociation();

	CAIActor* pLeaderActor = CastToCAIActorSafe(refAILeader.GetAIObject());

	TUnitList::iterator itLeader = m_pGroup->GetUnit(pLeaderActor);

	if (IsPlayer())
		m_refFormationOwner = refAILeader;
	else if (pOwner)
		m_refFormationOwner = GetWeakRef(pOwner);
	else if (vTargetPos.IsZero() && bIncludeLeader && itLeader != m_pGroup->GetUnits().end() && (itLeader->GetProperties() & unitProp))
	{
		m_refFormationOwner = GetAssociation();  // get Leader puppet by default
		if (!m_refFormationOwner.IsValid())
			m_refFormationOwner = m_pGroup->GetUnits().begin()->m_refUnit;
	}
	else
	{
		float fMinDist = std::numeric_limits<float>::max();
		for (TUnitList::iterator itUnit = m_pGroup->GetUnits().begin(); itUnit != m_pGroup->GetUnits().end(); ++itUnit)
		{
			if (!bIncludeLeader && itUnit->m_refUnit == refAILeader)
				continue;
			if (!(itUnit->GetProperties() & unitProp))
				continue;
			CAIObject* pUnit = itUnit->m_refUnit.GetAIObject();
			if (!pUnit)
				continue;
			float fDist = (pUnit->GetPos() - vTargetPos).GetLengthSquared();
			if (fDist < fMinDist)
			{
				m_refFormationOwner = itUnit->m_refUnit;
				fMinDist = fDist;
			}
		}
	}
	if (!m_refFormationOwner.IsValid())
		return false;

	CCCPOINT(CLeader_LeaderCreateFormation);

	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();
	if (pFormationOwner->CreateFormation(szFormationDescr, vTargetPos))
	{
		m_szFormationDescriptor = szFormationDescr;
	}

	if (pFormationOwner->m_pFormation)
	{
		//assign places to team members
		pFormationOwner->m_pFormation->GetNewFormationPoint(StaticCast<CAIActor>(m_refFormationOwner), 0);

		AssignFormationPoints(bIncludeLeader, unitProp);

		m_refPrevFormationOwner = m_refFormationOwner;
		return true;
	}
	m_refFormationOwner.Reset();
	return false;
}

bool CLeader::ReleaseFormation()
{
	CAIObject* pFormationOwner = m_refFormationOwner.GetAIObject();
	if (pFormationOwner && pFormationOwner->m_pFormation)
	{
		CAIActor* pOwnerActor = pFormationOwner->CastToCAIActor();
		int iOwnerGroup = GetGroupId();
		if (pOwnerActor)
			iOwnerGroup = pOwnerActor->GetGroupId();
		else if (pFormationOwner->GetSubType() == IAIObject::STP_BEACON)
			iOwnerGroup = GetAISystem()->GetBeaconGroupId(pFormationOwner);

		for (TUnitList::iterator itUnit = m_pGroup->GetUnits().begin(); itUnit != m_pGroup->GetUnits().end(); ++itUnit)
			FreeFormationPoint(itUnit->m_refUnit);
	}

	// force all unit to release their possible formations
	for (TUnitList::iterator itUnit = m_pGroup->GetUnits().begin(); itUnit != m_pGroup->GetUnits().end(); ++itUnit)
	{
		itUnit->m_FormationPointIndex = -1;
	}

	bool ret = false;
	if (pFormationOwner && pFormationOwner->m_pFormation)
		ret = pFormationOwner->ReleaseFormation();
	m_refFormationOwner.Reset();

	return ret;
}

CWeakRef<CAIObject> CLeader::GetNewFormationPoint(CWeakRef<CAIObject> refRequester, int iPointType)
{
	// (MATT) Code strongly implied requester must be an actor (GetClosestPoint) {2009/02/14}
	CAIObject* pFormationOwner = m_refFormationOwner.GetAIObject();
	CAIObject* pFormationPoint = NULL;
	CAIActor* const pRequester = static_cast<CAIActor*>(refRequester.GetAIObject());
	if (pFormationOwner && pFormationOwner->m_pFormation)
	{
		if (iPointType <= 0)
		{
			int iFPIndex = GetFormationPointIndex(pRequester);
			if (iFPIndex >= 0)
				pFormationPoint = pFormationOwner->m_pFormation->GetNewFormationPoint(StaticCast<CAIActor>(refRequester), iFPIndex);
		}
		else
			pFormationPoint = pFormationOwner->m_pFormation->GetClosestPoint(pRequester, true, iPointType);
	}
	return GetWeakRef(pFormationPoint);
}

int CLeader::GetFormationPointIndex(const CAIObject* pAIObject)  const
{
	if (pAIObject->GetType() == AIOBJECT_PLAYER)
		return 0;
	const CAIActor* pActor = pAIObject->CastToCAIActor();
	TUnitList::const_iterator itUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

	if (itUnit != m_pGroup->GetUnits().end())
		return (*itUnit).m_FormationPointIndex;

	return -1;
}

//-----------------------------------------------------------------------------------------------------------
CWeakRef<CAIObject> CLeader::GetFormationPointSight(const CPipeUser* pRequester) const
{
	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();
	if (pFormationOwner)
	{
		CFormation* pFormation = pFormationOwner->m_pFormation;
		if (pFormation)
			return GetWeakRef(pFormation->GetFormationDummyTarget(GetWeakRef(pRequester)));
	}
	return NILREF;
}

//-----------------------------------------------------------------------------------------------------------
CWeakRef<CAIObject> CLeader::GetFormationPoint(CWeakRef<CAIObject> refRequester) const
{
	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();
	if (pFormationOwner)
	{
		CFormation* pFormation = pFormationOwner->m_pFormation;
		if (pFormation)
			return GetWeakRef(pFormation->GetFormationPoint(refRequester));
	}
	return NILREF;
}

//-----------------------------------------------------------------------------------------------------------
void CLeader::FreeFormationPoint(CWeakRef<CAIObject> refRequester) const
{
	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();
	if (pFormationOwner && pFormationOwner->m_pFormation)
		pFormationOwner->m_pFormation->FreeFormationPoint(refRequester);

}

//-----------------------------------------------------------------------------------------------------------
int CLeader::AssignFormationPoints(bool bIncludeLeader, uint32 unitProp)
{
	std::vector<bool> vPointTaken;
	TClassUnitMap mapClassUnits;

	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();
	if (!pFormationOwner)
		return 0;

	CFormation* pFormation = pFormationOwner->m_pFormation;
	if (!pFormation)
		return 0;

	CCCPOINT(CLeader_AssignFormationPoints);

	TUnitList::iterator itUnitEnd = m_pGroup->GetUnits().end();
	for (TUnitList::iterator it = m_pGroup->GetUnits().begin(); it != itUnitEnd; ++it)
		if (it->m_refUnit == m_refFormationOwner)
			it->m_FormationPointIndex = 0;
		else
			it->m_FormationPointIndex = -1;

	int iFreePoints = pFormation->CountFreePoints();
	int iFormationSize = pFormation->GetSize();
	int iTeamSize = m_pGroup->GetUnitCount(unitProp); //m_pGroup->GetUnits().size();
	if (!m_pGroup->IsMember(pFormationOwner))
		iTeamSize++;
	if (iTeamSize > iFreePoints)
		iTeamSize = iFreePoints;

	int building;
	CAIActor* pOwnerActor = pFormationOwner->CastToCAIActor();
	IAISystem::tNavCapMask navMask = (IsPlayer() || !pOwnerActor ? IAISystem::NAVMASK_ALL : pOwnerActor->GetMovementAbility().pathfindingProperties.navCapMask);

	bool bOwnerInTriangularRegion = (gAIEnv.pNavigation->CheckNavigationType(pFormationOwner->GetPos(), building, navMask) == IAISystem::NAV_TRIANGULAR);
	TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();

	std::vector<TUnitList::iterator> AssignedUnit;

	//assigned point 0 to owner
	TUnitList::iterator itOwner = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pOwnerActor);
	AssignedUnit.push_back(itOwner); // could be itUnitEnd if the owner is not in the units (i.e.player, beacon)

	for (int i = 1; i < iFormationSize; i++)
		AssignedUnit.push_back(itUnitEnd);

	CWeakRef<CAIObject> refAILeader = GetAssociation();
	int iAssignedCount = 0;

	for (int k = 0; k < 3; k++) // this index determines the type of test
	{
		for (int i = 1; i < iFormationSize; i++)
		{
			//formation points are sorted by distance to point 0
			//try to assign closest points to owner first, depending on their class
			CAIObject* pPoint = pFormation->GetFormationPoint(i);
			float fMinDist = 987654321.f;
			int index = -1;
			TUnitList::iterator itSelected = itUnitEnd;

			for (itUnit = m_pGroup->GetUnits().begin(); itUnit != itUnitEnd; ++itUnit)
			{
				CUnitImg& unit = (*itUnit);
				if ((unit.GetProperties() & unitProp) && unit.m_refUnit != m_refFormationOwner
				    && unit.m_FormationPointIndex < 0 &&
				    (k == 0 && (unit.GetClass() == pFormation->GetPointClass(i)) || ///first give priority to points that have ONLY the requestor's class
				     k == 1 && AssignedUnit[i] == itUnitEnd && (unit.GetClass() & pFormation->GetPointClass(i)) ||// then consider points that have ALSO the requestor's class
				     k == 2 && AssignedUnit[i] == itUnitEnd && pFormation->GetPointClass(i) != SPECIAL_FORMATION_POINT)) // finally don't consider the class, excluding SPECIAL_FORMATION_POINT anyway
				{
					if (unit.m_refUnit == refAILeader && !bIncludeLeader)
						continue;
					Vec3 endPos(pPoint->GetPos());
					Vec3 otherPos(unit.m_refUnit.GetAIObject()->GetPos());
					if (bOwnerInTriangularRegion)
						endPos.z = gEnv->p3DEngine->GetTerrainElevation(endPos.x, endPos.y);

					float fDist = Distance::Point_PointSq(pPoint->GetPos(), endPos);
					if (fDist < fMinDist)
					{
						fMinDist = fDist;
						itSelected = itUnit;
						index = i;
					}
				}
			}
			if (index > 0)
			{
				CCCPOINT(CLeader_AssignFormationPoints_A);

				itSelected->m_FormationPointIndex = index;
				pFormationOwner->m_pFormation->GetNewFormationPoint(itSelected->m_refUnit, index);
				iAssignedCount++;
				;
			}
		}
	}
	// then re-assign the members who has found a single class point, but can find a free multiple class point closer to the owner
	for (itUnit = m_pGroup->GetUnits().begin(); itUnit != itUnitEnd; ++itUnit)
	{
		CUnitImg& unit = (*itUnit);
		//formation points are sorted by distance to point 0
		if (unit.m_refUnit == refAILeader && !bIncludeLeader)
			continue;

		//		float fMinDist = 987654321.f;
		for (int i = 1; i < itUnit->m_FormationPointIndex; i++)
		{
			CAIObject* pPoint = pFormation->GetFormationPoint(i);
			if ((unit.GetProperties() & unitProp) && unit.m_refUnit != m_refFormationOwner
			    && (unit.GetClass() & pFormation->GetPointClass(i)) && !pFormation->GetPointOwner(i))
			{
				CCCPOINT(CLeader_AssignFormationPoints_B);

				int currentIndex = itUnit->m_FormationPointIndex;
				pFormation->FreeFormationPoint(unit.m_refUnit);
				pFormation->GetNewFormationPoint(unit.m_refUnit, i);
				itUnit->m_FormationPointIndex = i;
			}
		}
	}

	return iAssignedCount;
}

bool CLeader::AssignFormationPoint(CUnitImg& unit, int teamSize, bool bAnyClass, bool bClosestToOwner)
{
	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();
	CFormation* pFormation;

	CCCPOINT(CLeader_AssignFormationPoint);

	int myPointIndex = -1;
	if (pFormationOwner && (pFormation = pFormationOwner->m_pFormation))
	{
		if (bAnyClass)
			myPointIndex = pFormation->GetClosestPointIndex(unit.m_refUnit, true, teamSize, -1, bClosestToOwner);
		else
		{
			int myClass = unit.m_SoldierClass;
			if (myClass)
			{
				myPointIndex = pFormation->GetClosestPointIndex(unit.m_refUnit, true, teamSize, myClass, bClosestToOwner);
			}
			if (myPointIndex < 0) // if your class is not found, try searching an undefined class point
			{
				myPointIndex = pFormation->GetClosestPointIndex(unit.m_refUnit, true, teamSize, UNIT_CLASS_UNDEFINED, bClosestToOwner);
			}
		}
	}

	if (myPointIndex >= 0)
	{
		unit.m_FormationPointIndex = myPointIndex;
		return true;
	}

	return false;
}

void CLeader::AssignFormationPointIndex(CUnitImg& unit, int index)
{
	unit.m_FormationPointIndex = index;
	CFormation* pFormation;
	CAIObject* const pFormationOwner = m_refFormationOwner.GetAIObject();

	if (pFormationOwner && (pFormation = pFormationOwner->m_pFormation))
	{
		CCCPOINT(CLeader_AssignFormationPointIndex);

		pFormation->FreeFormationPoint(unit.m_refUnit);
		pFormation->GetNewFormationPoint(unit.m_refUnit, index);
	}
}

//
//--------------------------------------------------------------------------------------------------------------
CLeaderAction* CLeader::CreateAction(const LeaderActionParams* params, const char* signalText)
{
	CLeaderAction* pAction(NULL);

	switch (params->type)
	{
	case LA_ATTACK:
		switch (params->subType)
		{
		case LAS_ATTACK_SWITCH_POSITIONS:
			pAction = new CLeaderAction_Attack_SwitchPositions(this, *params);
			break;
		case LAS_DEFAULT:
		default:
			pAction = new CLeaderAction_Attack(this);
			break;
		}
		break;
	case LA_SEARCH:
		pAction = new CLeaderAction_Search(this, *params);
		break;
	default:
		AIAssert(!"Action type not added");
	}
	if (signalText)
	{
		CAIObject* pAILeader = GetAssociation().GetAIObject();
		if (pAILeader)
			GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 1, signalText, pAILeader);
	}

	if (pAction)
		m_vMarkedPosition.Set(0, 0, 0);

	return pAction;
}

//
//--------------------------------------------------------------------------------------------------------------
void CLeader::Serialize(TSerialize ser)
{
	// (MATT) There will be problems serialising the groupid and group pointer  {2009/03/31}

	ser.BeginGroup("AILeader");
	{
		CAIActor::Serialize(ser);

		m_refFormationOwner.Serialize(ser, "m_refFormationOwner");
		m_refFormationOwner.Serialize(ser, "m_refPrevFormationOwner");
		ser.Value("m_szFormationDescriptor", m_szFormationDescriptor);

		ser.Value("m_vForcedPreferredPos", m_vForcedPreferredPos);

		m_refEnemyTarget.Serialize(ser, "m_refEnemyTarget");

		ser.Value("m_vMarkedPosition", m_vMarkedPosition);
		ser.Value("m_bLeaderAlive", m_bLeaderAlive);
		ser.Value("m_bKeepEnabled", m_bKeepEnabled);

		ser.BeginGroup("m_pCurrentAction");
		{
			ELeaderAction actionType = ser.IsWriting() ? (m_pCurrentAction ? m_pCurrentAction->GetType() : LA_NONE) : LA_NONE;
			ELeaderActionSubType subType = ser.IsWriting() ? (m_pCurrentAction ? m_pCurrentAction->GetSubType() : LAS_DEFAULT) : LAS_DEFAULT;
			ser.EnumValue("ActionType", actionType, LA_NONE, LA_LAST);
			ser.EnumValue("ActionSubType", subType, LAS_DEFAULT, LAS_LAST);
			if (ser.IsReading())
			{
				AbortExecution();//clears current action
				if (actionType != LA_NONE)
				{
					LeaderActionParams params(actionType, subType);
					m_pCurrentAction = CreateAction(&params);
				}
			}

			if (m_pCurrentAction)
				m_pCurrentAction->Serialize(ser);
		}
		ser.EndGroup(); // ser.BeginGroup("m_pCurrentAction");
	}
	ser.EndGroup(); // ser.BeginGroup("AILeader");
}

//
//----------------------------------------------------------------------------------------------------------

void CLeader::AssignTargetToUnits(uint32 unitProp, int maxUnitsPerTarget)
{

	CAIGroup::TSetAIObjects::iterator itTarget = m_pGroup->GetTargets().begin();
	std::vector<int> AssignedUnits;

	int indexTarget = 0;
	const CAIObject* pTarget;
	int numUnits = m_pGroup->GetUnitCount(unitProp);
	int numtargets = m_pGroup->GetTargets().size();

	if (numtargets == 0)
		numtargets = 1;
	for (int i = 0; i < numtargets; i++)
		AssignedUnits.push_back(0);

	if (itTarget != m_pGroup->GetTargets().end())
		pTarget = itTarget->GetAIObject();
	else
	{
		pTarget = static_cast<CAIObject*>(GetAISystem()->GetBeacon(GetGroupId()));
		if (!pTarget) // no beacon, no targets
			return;
	}

	int unitsPerTarget = numUnits / numtargets;
	if (unitsPerTarget > maxUnitsPerTarget)
		unitsPerTarget = maxUnitsPerTarget;

	for (TUnitList::iterator it = m_pGroup->GetUnits().begin(); it != m_pGroup->GetUnits().end(); ++it)
	{
		CUnitImg& unit = *it;
		if (unit.GetProperties() & unitProp)
		{
			AISignalExtraData data;
			unit.ClearPlanning();
			IEntity* pTargetEntity = NULL;

			// (MATT) This casting is horrible at best, at worst might not work {2009/02/10}
			if (const CPuppet* pPuppet = pTarget->CastToCPuppet())
				pTargetEntity = (IEntity*)pPuppet->GetAssociation().GetAIObject();
			else if (const CAIPlayer* pPlayer = pTarget->CastToCAIPlayer())
				pTargetEntity = (IEntity*)pPlayer->GetAssociation().GetAIObject();

			if (pTargetEntity)
				data.nID = pTargetEntity->GetId();
			data.SetObjectName(pTarget->GetName());

			CUnitAction* pAcquire = new CUnitAction(UA_ACQUIRETARGET, true, "", data);
			unit.m_Plan.push_back(pAcquire);
			unit.ExecuteTask();
			AssignedUnits[indexTarget] = AssignedUnits[indexTarget] + 1;
			if (AssignedUnits[indexTarget] >= unitsPerTarget)
			{
				indexTarget++;
				if (indexTarget >= numtargets)
					break;
				if (itTarget != m_pGroup->GetTargets().end())
					++itTarget;
				if (itTarget == m_pGroup->GetTargets().end())
					break;
				else
					pTarget = itTarget->GetAIObject();
			}

		}
	}
	UpdateEnemyStats();
}

int CLeader::GetActiveUnitPlanCount(uint32 unitProp) const
{
	int active = 0;
	for (TUnitList::const_iterator it = m_pGroup->GetUnits().begin(); it != m_pGroup->GetUnits().end(); ++it)
	{
		const CUnitImg& unit = *it;
		if ((unit.GetProperties() & unitProp) && !unit.m_Plan.empty())
			active++;
	}
	return active;
}

//
//-------------------------------------------------------------------------------------

CAIObject* CLeader::GetEnemyLeader() const
{
	CWeakRef<CAIObject> refTarget = m_pGroup->GetAttentionTarget(true);
	CAIActor* pTarget = refTarget.GetIAIObject()->CastToCAIActor();
	CAIObject* pAILeader = NULL;
	if (pTarget)
	{
		CLeader* pLeader = GetAISystem()->GetLeader(pTarget);
		if (pLeader)
			pAILeader = pLeader->GetAssociation().GetAIObject();
	}
	return pAILeader;
}

void CLeader::DeadUnitNotify(CAIActor* pAIObj)
{
	if (pAIObj && GetAssociation() == pAIObj)
	{
		GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnLeaderDied", pAIObj);
		m_bLeaderAlive = false;
	}

	if (m_pCurrentAction) m_pCurrentAction->DeadUnitNotify(pAIObj);

}
