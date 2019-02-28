// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   UnitAction.cpp
   $Id$
   Description: Refactoring CLeadeActions; adding Serialization support

   -------------------------------------------------------------------------
   History:
   - 22/05/2006 : Moved from UnitAction by Mikko

 *********************************************************************/

#include "StdAfx.h"
#include "CAISystem.h"
#include "UnitImg.h"
#include "UnitAction.h"
#include "AILog.h"
#include "Leader.h"
#include "Formation/AIFormationDescriptor.h"

CUnitImg::CUnitImg() :
	m_TagPoint(0, 0, 0),
	m_Group(0),
	m_FormationPointIndex(0),
	m_SoldierClass(UNIT_CLASS_INFANTRY),
	m_properties(UPR_ALL),
	m_pCurrentAction(0),
	m_bTaskSuspended(false),
	m_flags(0),
	m_fHeight(1.6f),
	m_fWidth(0.6f),
	m_fDistance(0.f),
	m_fDistance2(0.f),
	m_lastReinforcementTime(0.f)
{
	// Empty
}

//
//----------------------------------------------------------------------------------------------------
CUnitImg::CUnitImg(CWeakRef<CAIActor> refUnit) :
	m_refUnit(refUnit),
	m_TagPoint(0, 0, 0),
	m_Group(0),
	m_FormationPointIndex(0),
	m_SoldierClass(UNIT_CLASS_INFANTRY),
	m_properties(UPR_ALL),
	m_pCurrentAction(0),
	m_bTaskSuspended(false),
	m_flags(0),
	m_fHeight(1.6f),
	m_fWidth(0.6f),
	m_fDistance(0.f),
	m_fDistance2(0.f),
	m_lastReinforcementTime(0.f)
{
	m_lastMoveTime = GetAISystem()->GetFrameStartTime();

	CCCPOINT(CUnitImg_CUnitImg);
}

//
//----------------------------------------------------------------------------------------------------
CUnitImg::~CUnitImg()
{
	ClearPlanning();
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::TaskExecuted()
{

	if (m_pCurrentAction)
	{
		delete m_pCurrentAction;
		m_pCurrentAction = NULL;

		if (!m_Plan.empty())//this test should be useless
			m_Plan.pop_front();

		CCCPOINT(CUnitImg_TaskExecuted);
	}
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ExecuteTask()
{
	bool bContinue = true;

	while (bContinue && !m_Plan.empty())
	{
		CUnitAction* const nextAction = m_Plan.front();

		bool bIsBlocked = nextAction->IsBlocked();
		bContinue = !nextAction->m_BlockingPlan && !bIsBlocked;

		CAIActor* const pUnit = m_refUnit.GetAIObject();

		if (!bIsBlocked)
		{
			CCCPOINT(CUnitImg_ExecuteTask);

			m_pCurrentAction = nextAction;
			IAISignalExtraData* pData = NULL;
			switch (m_pCurrentAction->m_Action)
			{
			case UA_SIGNAL:
				GetAISystem()->SendSignal(0, 1, m_pCurrentAction->m_SignalText, pUnit, new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;
			case UA_ACQUIRETARGET:
				GetAISystem()->SendSignal(0, 1, "ORDER_ACQUIRE_TARGET", pUnit, new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;
			case UA_SEARCH:
				{
					const char* signal = "ORDER_SEARCH";
					if (m_pCurrentAction->m_Point.IsZero())
					{
						Vec3 hidePosition(0, 0, 0);
						CLeader* pMyLeader = GetAISystem()->GetLeader(pUnit);
						if (pMyLeader)
						{
							if (pMyLeader->m_pFormation)
							{
								CAIObject* pFormPoint = pMyLeader->GetNewFormationPoint(m_refUnit).GetAIObject();
								if (pFormPoint)
									hidePosition = pFormPoint->GetPos();
							}
						}
						if (hidePosition.IsZero())
							hidePosition = pUnit->GetPos();
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = hidePosition;
						pData->iValue = m_pCurrentAction->m_Tag;
						GetAISystem()->SendSignal(0, 10, signal, pUnit, pData);

						CCCPOINT(CUnitImg_TaskExecuted_A);
					}
					else
					{
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = m_pCurrentAction->m_Point;
						pData->point2 = m_pCurrentAction->m_Direction;
						pData->iValue = m_pCurrentAction->m_Tag;
						GetAISystem()->SendSignal(0, 10, signal, pUnit, pData);
					}
					bContinue = false; // force blocking
				}
				break;
			default:
				// unrecognized action type, skip it
				AIAssert(0);
				bContinue = true;
				break;
			}
		}

		if (bContinue)
			TaskExecuted();
	}
}

//
//----------------------------------------------------------------------------------------------------
bool CUnitImg::IsBlocked() const
{
	if (m_bTaskSuspended) // consider the unit's plan blocked if he's suspended his task for
		return true;        // something more prioritary

	if (m_Plan.empty())
		return false;
	else
		return m_Plan.front()->IsBlocked();
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ClearPlanning(int priority)
{
	if (priority)
	{
		TActionList::iterator itAction = m_Plan.begin();
		while (itAction != m_Plan.end())
		{
			if ((*itAction)->m_Priority <= priority)
			{
				if (*itAction == m_pCurrentAction)
				{
					m_pCurrentAction = NULL;
					m_bTaskSuspended = false;
				}

				CUnitAction* pAction = *itAction;
				m_Plan.erase(itAction++);
				delete pAction;
			}
			else
				++itAction;
		}
	}
	else // unconditioned if priority==0
	{
		for (TActionList::iterator itAction = m_Plan.begin(); itAction != m_Plan.end(); ++itAction)
			delete (*itAction);
		m_Plan.clear();
		m_pCurrentAction = NULL;
		m_bTaskSuspended = false;
	}

}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ClearUnitAction(EUnitAction action)
{
	TActionList::iterator itAction = m_Plan.begin();
	while (itAction != m_Plan.end())
	{
		if ((*itAction)->m_Action == action)
		{
			if (*itAction == m_pCurrentAction)
			{
				m_pCurrentAction = NULL;
				m_bTaskSuspended = false;
			}

			CUnitAction* pAction = *itAction;
			m_Plan.erase(itAction++);
			delete pAction;
		}
		else
			++itAction;
	}
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::SuspendTask()
{
	m_bTaskSuspended = true;
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ResumeTask()
{
	m_bTaskSuspended = false;
}

bool CUnitImg::IsPlanFinished() const
{
	return m_Plan.empty() || !m_refUnit.GetAIObject()->IsEnabled();
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::Reset()
{
	m_lastReinforcementTime.SetSeconds(0.f);
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::Serialize(TSerialize ser)
{
	m_refUnit.Serialize(ser, "m_refUnit");

	CRY_ASSERT_MESSAGE(m_Plan.empty(), "Todo: implement serialization of m_Plan");
	//SerializeListOfPointers(ser, objectTracker, "Plan", m_Plan);
	ser.Value("m_TagPoint", m_TagPoint);
	ser.Value("m_flags", m_flags);
	ser.Value("m_Group", m_Group);
	ser.Value("m_FormationPointIndex", m_FormationPointIndex);
	ser.Value("m_SoldierClass", m_SoldierClass);
	ser.Value("m_bTaskSuspended", m_bTaskSuspended);
	ser.Value("m_lastMoveTime", m_lastMoveTime);
	ser.Value("m_fDistance", m_fDistance);
	ser.Value("m_fDistance2", m_fDistance2);
	ser.Value("m_lastReinforcementTime", m_lastReinforcementTime);

	if (ser.BeginOptionalGroup("CurrentAction", m_pCurrentAction != NULL))
	{
		if (ser.IsReading())
			m_pCurrentAction = m_Plan.empty() ? NULL : m_Plan.front();
		ser.EndGroup();
	}
}

//
//----------------------------------------------------------------------------------------------------
void CUnitAction::Serialize(TSerialize ser)
{
	ser.EnumValue("m_Action", m_Action, UA_NONE, UA_LAST);
	ser.Value("m_BlockingPlan", m_BlockingPlan);

	CRY_ASSERT_MESSAGE(false, "Todo: implement serialization of blocked actions");
	//ser.Value("BlockingActions", m_BlockingActions);
	//ser.Value("BlockedActions", m_BlockedActions);

	ser.Value("m_Priority", m_Priority);
	ser.Value("m_Point", m_Point);
	ser.Value("m_SignalText", m_SignalText);
	m_SignalData.Serialize(ser);
	ser.Value("m_Tag", m_Tag);
}

//
//----------------------------------------------------------------------------------------------------
