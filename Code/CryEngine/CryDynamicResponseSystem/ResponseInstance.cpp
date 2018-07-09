// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ResponseInstance.h"
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include <CryString/StringUtils.h>
#include "Response.h"
#include "ResponseSystem.h"
#include "DelayActionAction.h"

using namespace CryDRS;

CResponseInstance::CResponseInstance(SSignal& signal, CResponse* pResponse)
	: m_pSender(signal.m_pSender)
	, m_pActiveActor(signal.m_pSender)
	, m_pSignalContext(signal.m_pSignalContext)
	, m_pCurrentlyExecutedSegment(nullptr)
	, m_signalName(signal.m_signalName)
	, m_id(signal.m_id)
	, m_pResponse(pResponse)
{}

//--------------------------------------------------------------------------------------------------
CResponseInstance::~CResponseInstance()
{
#if defined(DRS_COLLECT_DEBUG_DATA)
	for (auto& currentActionInstance : m_activeActions)
	{
		DRS_DEBUG_DATA_ACTION(AddActionFinished(currentActionInstance.get()));
	}
#endif
	m_activeActions.clear();
}

//--------------------------------------------------------------------------------------------------
bool CResponseInstance::Update()
{
	DRS_DEBUG_DATA_ACTION(SetCurrentResponseInstance(this));

	int blockingActions = 0;

	for (ActionInstanceList::iterator it = m_activeActions.begin(); it != m_activeActions.end(); )
	{
		const DRS::IResponseActionInstance::eCurrentState state = (*it)->Update();
		if (state != DRS::IResponseActionInstance::CS_RUNNING)
		{
			if (state != DRS::IResponseActionInstance::CS_RUNNING_NON_BLOCKING)
			{
				DRS_DEBUG_DATA_ACTION(AddActionFinished(it->get()));
				it = m_activeActions.erase(it);  //delete finished action instance
			}
			else
			{
				++it;
			}
		}
		else
		{
			blockingActions++;
			++it;
		}
	}

	if (blockingActions == 0 && m_pCurrentlyExecutedSegment)
	{
		DRS_DEBUG_DATA_ACTION(IncrementSegmentHierarchyLevel());
		ExecuteSegment(m_pCurrentlyExecutedSegment->GetNextResponseSegment(this));

		if (m_pCurrentlyExecutedSegment == nullptr && !m_activeActions.empty())  //if the response instance is done we cancel all still running actions
		{
			for (auto& currentActionInstance : m_activeActions)
			{
				currentActionInstance->Cancel();
			}
			return true;
		}
	}

	if (m_pCurrentlyExecutedSegment == nullptr) //canceled or finished
	{
		if (m_pResponse)
		{
			m_pResponse->SetLastEndTime(CResponseSystem::GetInstance()->GetCurrentDrsTime());
		}
		return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
void CResponseInstance::Cancel()
{
	DRS_DEBUG_DATA_ACTION(SetCurrentResponseInstance(this));
	DRS_DEBUG_DATA_ACTION(AddResponseInstanceFinished(CResponseSystemDebugDataProvider::eER_Canceled));
	for (auto& currentActionInstance : m_activeActions)
	{
		currentActionInstance->Cancel();
	}
	m_pCurrentlyExecutedSegment = nullptr; //this will quit the instance in the next update
}

//--------------------------------------------------------------------------------------------------
void CResponseInstance::ExecuteSegment(CResponseSegment* pSegment)
{
	m_pCurrentlyExecutedSegment = pSegment;

	if (pSegment)
	{
		DRS_DEBUG_DATA_ACTION(AddResponseSegmentStarted(pSegment));
#if !defined(_RELEASE)
		const string currentName = string("Response for '") + GetSignalName().GetText() + "', Segment: '" + pSegment->GetName() + "' CurrentActor: " + (GetCurrentActor() ? GetCurrentActor()->GetName() : "None");
#endif
		for (CResponseSegment::ActionsInfo& actionInfo : pSegment->GetActions())
		{
			if (m_pCurrentlyExecutedSegment)
			{
				CRY_ASSERT(actionInfo.m_pAction);
				DRS::IResponseActionInstanceUniquePtr pAction = nullptr;
				if (actionInfo.m_delay > FLT_EPSILON)
				{
					pAction = DRS::IResponseActionInstanceUniquePtr(new DelayActionActionInstance(actionInfo.m_delay, actionInfo.m_pAction, this));
					DRS_DEBUG_DATA_ACTION(AddActionStarted(actionInfo.m_pAction->GetVerboseInfoWithType() + ", delay: " + CryStringUtils::toString(actionInfo.m_delay) + "s ", pAction.get(), GetCurrentActor(), pSegment));
				}
				else
				{
					SET_DRS_USER_SCOPED(currentName);
					pAction = actionInfo.m_pAction->Execute(this);
					DRS_DEBUG_DATA_ACTION(AddActionStarted(actionInfo.m_pAction->GetVerboseInfoWithType(), (pAction) ? pAction.get() : nullptr, GetCurrentActor(), pSegment));
				}

				if (pAction)
				{
					m_activeActions.push_back(std::move(pAction));
				}
				else
				{
					DRS_DEBUG_DATA_ACTION(AddActionFinished(nullptr));
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseInstance::Execute()
{
	ExecuteSegment(&m_pResponse->GetBaseSegment());
}
