// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BackgroundScheduleManager.h"

namespace BackgroundScheduleManager
{

//-----------------------------------------------------------------------------

CScheduleItem::CScheduleItem(const char* szName)
	: m_name(szName)
	, m_refCount(1)
	, m_state(eScheduleItemState_Pending)
{
}

CScheduleItem::~CScheduleItem()
{
	CRY_ASSERT(m_refCount == 0);

	for (TWorkItems::const_iterator it = m_workItems.begin();
	     it != m_workItems.end(); ++it)
	{
		(*it)->Release();
	}
}

const char* CScheduleItem::GetDescription() const
{
	return m_name.c_str();
}

EScheduleItemState CScheduleItem::GetState() const
{
	return m_state;
}

const float CScheduleItem::GetProgress() const
{
	if (m_workItems.empty())
	{
		return 1.0f;
	}
	else
	{
		float totalProgress = 0.0f;

		for (TWorkItems::const_iterator it = m_workItems.begin();
		     it != m_workItems.end(); ++it)
		{
			totalProgress += (*it)->GetProgress();
		}

		return totalProgress / (float)m_workItems.size();
	}
}

const uint32 CScheduleItem::GetNumWorkItems() const
{
	return m_workItems.size();
}

IBackgroundScheduleItemWork* CScheduleItem::GetWorkItem(const uint32 index) const
{
	return m_workItems[index];
}

void CScheduleItem::AddWorkItem(IBackgroundScheduleItemWork* pWork)
{
	// cannot add new work items when item has finished or failed
	if (m_state == eScheduleItemState_Failed || m_state == eScheduleItemState_Completed)
	{
		CryFatalError("Cannot add new work items when item has finished or failed");
		return;
	}

	pWork->AddRef();

	// add to the work list
	if (m_state == eScheduleItemState_Processing)
	{
		m_addedWorkItems.push_back(pWork);
	}
	else
	{
		m_workItems.push_back(pWork);
	}
}

void CScheduleItem::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CScheduleItem::Release()
{
	const int nCount = CryInterlockedDecrement(&m_refCount);
	assert(nCount >= 0);
	if (nCount == 0)
	{
		delete this;
	}
	else if (nCount < 0)
	{
		assert(0);
		CryFatalError("Deleting Reference Counted Object Twice");
	}
}

void CScheduleItem::RequestStop()
{
	if (m_state == eScheduleItemState_Pending)
	{
		// we can stop right away :)
		m_state = eScheduleItemState_Failed;
	}
	else if (m_state == eScheduleItemState_Processing)
	{
		m_state = eScheduleItemState_Stopping;

		// signal all pending work to stop
		uint32 curIndex = 0;
		while (curIndex < m_processedWorkItems.size())
		{
			IBackgroundScheduleItemWork* pWork = m_processedWorkItems[curIndex];
			if (pWork->OnStop())
			{
				// if the work was stopped remove it from list
				m_processedWorkItems.erase(m_processedWorkItems.begin() + curIndex);
				continue;
			}
			else
			{
				// this work item cannot be stopped this frame
				curIndex += 1;
			}
		}

		// if all pending work has been stopped we can assume the failed state
		if (m_processedWorkItems.empty())
		{
			m_state = eScheduleItemState_Failed;
		}
	}
}

EScheduleWorkItemStatus CScheduleItem::Update()
{
	EScheduleWorkItemStatus retStatus = eScheduleWorkItemStatus_NotFinished;

	switch (m_state)
	{
	// finial state - work failed
	case eScheduleItemState_Failed:
		{
			retStatus = eScheduleWorkItemStatus_Failed;
			break;
		}

	// final state - work completed
	case eScheduleItemState_Completed:
		{
			retStatus = eScheduleWorkItemStatus_Finished;
			break;
		}

	// first update, start all the work items
	case eScheduleItemState_Pending:
		{
			// start all of the tasks
			bool bHasFailedStarts = false;
			for (TWorkItems::const_iterator it = m_workItems.begin();
			     it != m_workItems.end(); ++it)
			{
				IBackgroundScheduleItemWork* pWork = (*it);

				if (pWork->OnStart())
				{
					m_processedWorkItems.push_back(pWork);
				}
				else
				{
					bHasFailedStarts = true;
					break;
				}
			}

			if (bHasFailedStarts)
			{
				m_state = eScheduleItemState_Stopping;
				break;
			}
			else
			{
				m_state = eScheduleItemState_Processing;
				/* FALLS THROUGHT TO PROCESSING STATE */
			}
		}

	// work processing state
	case eScheduleItemState_Processing:
		{
			// process new work items that were added while the schedule was created
			if (!m_addedWorkItems.empty())
			{
				for (TWorkItems::const_iterator it = m_addedWorkItems.begin();
				     it != m_addedWorkItems.end(); ++it)
				{
					IBackgroundScheduleItemWork* pWork = (*it);
					pWork->OnStart();
					m_processedWorkItems.push_back(pWork);
					m_workItems.push_back(pWork);
				}

				m_addedWorkItems.clear();
			}

			// update work items
			bool bHasFailedItems = false;
			TWorkItems completedItems;
			for (TWorkItems::const_iterator it = m_processedWorkItems.begin();
			     it != m_processedWorkItems.end(); ++it)
			{
				IBackgroundScheduleItemWork* pWork = (*it);

				// update given work item
				const EScheduleWorkItemStatus status = pWork->OnUpdate();
				if (status == eScheduleWorkItemStatus_Finished)
				{
					completedItems.push_back(pWork);
					continue;
				}

				// item failed - we need to stop other tasks
				if (status == eScheduleWorkItemStatus_Failed)
				{
					bHasFailedItems = true;
					break;
				}
			}

			// cleanup completed items
			for (TWorkItems::iterator it = completedItems.begin();
			     it != completedItems.end(); ++it)
			{
				IBackgroundScheduleItemWork* pWork = (*it);
				TWorkItems::iterator jt = std::find(m_processedWorkItems.begin(), m_processedWorkItems.end(), pWork);
				pWork->Release();
				m_processedWorkItems.erase(jt);
			}

			if (!bHasFailedItems)
			{
				// all work has finished
				if (m_processedWorkItems.empty())
				{
					retStatus = eScheduleWorkItemStatus_Finished;
					m_state = eScheduleItemState_Completed;
				}

				break;
			}
			else
			{
				// some of the items failed
				m_state = eScheduleItemState_Stopping;
				/* FALL THROUGH TO STOPPING STATE */
			}
		}

	// We are stopping failed work
	case eScheduleItemState_Stopping:
		{
			uint32 curIndex = 0;
			while (curIndex < m_processedWorkItems.size())
			{
				IBackgroundScheduleItemWork* pWork = m_processedWorkItems[curIndex];
				if (pWork->OnStop())
				{
					// if the work was stopped remove it from list
					m_processedWorkItems[curIndex]->Release();
					m_processedWorkItems.erase(m_processedWorkItems.begin() + curIndex);
					continue;
				}
				else
				{
					// this work item cannot be stopped this frame
					curIndex += 1;
				}
			}

			// if all pending work has been stopped we can assume the failed state
			if (m_processedWorkItems.empty())
			{
				m_state = eScheduleItemState_Failed;
				return eScheduleWorkItemStatus_Failed;
			}
		}
	}

	return retStatus;
}

//-----------------------------------------------------------------------------

CSchedule::CSchedule(const char* szName)
	: m_name(szName)
	, m_refCount(1)
	, m_bCanceled(false)
	, m_currentItem(0)
	, m_state(eScheduleState_Pending)
{
}

CSchedule::~CSchedule()
{
	CRY_ASSERT(m_refCount == 0);

	for (TItems::const_iterator it = m_items.begin();
	     it != m_items.end(); ++it)
	{
		CScheduleItem* pItem = *it;
		SAFE_RELEASE(pItem);
	}

	m_items.clear();
}

const char* CSchedule::GetDescription() const
{
	return m_name.c_str();
}

float CSchedule::GetProgress() const
{
	if (m_currentItem >= m_items.size())
	{
		return 1.0f;
	}
	else
	{
		const float itemProgress = 1.0f / (float)(m_items.size());
		const IBackgroundScheduleItem* pItem = m_items[m_currentItem];
		return (m_currentItem + pItem->GetProgress()) * itemProgress;
	}
}

IBackgroundScheduleItem* CSchedule::GetProcessedItem() const
{
	if (m_currentItem >= m_items.size())
	{
		return NULL;
	}
	else
	{
		IBackgroundScheduleItem* pItem = m_items[m_currentItem];
		return pItem;
	}
}

const uint32 CSchedule::GetNumItems() const
{
	return m_items.size();
}

IBackgroundScheduleItem* CSchedule::GetItem(const uint32 index) const
{
	return m_items[index];
}

EScheduleState CSchedule::GetState() const
{
	return m_state;
}

void CSchedule::Cancel()
{
	m_bCanceled = true;
}

bool CSchedule::IsCanceled() const
{
	return m_bCanceled;
}

void CSchedule::AddItem(IBackgroundScheduleItem* pItem)
{
	if (NULL == pItem)
	{
		return;
	}

	// we can add items only in the "pending" state
	if (pItem->GetState() != eScheduleItemState_Pending)
	{
		CryFatalError("Schedule items can be added to schedule only before their work starts");
		return;
	}

	// item has no jobs, do not add
	if (pItem->GetNumWorkItems() == 0)
	{
		return;
	}

	m_items.push_back(static_cast<CScheduleItem*>(pItem));
	pItem->AddRef();
}

void CSchedule::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CSchedule::Release()
{
	const int nCount = CryInterlockedDecrement(&m_refCount);
	assert(nCount >= 0);
	if (nCount == 0)
	{
		delete this;
	}
	else if (nCount < 0)
	{
		assert(0);
		CryFatalError("Deleting Reference Counted Object Twice");
	}
}

EScheduleWorkItemStatus CSchedule::Update()
{
	EScheduleWorkItemStatus retStatus = eScheduleWorkItemStatus_NotFinished;

	// we have a cancel request
	if (m_bCanceled)
	{
		CryLog("Schedule '%s' was canceled", GetDescription());

		if (m_state == eScheduleState_Processing && m_currentItem < m_items.size())
		{
			// stop the current item
			CScheduleItem* pItem = m_items[m_currentItem];
			pItem->RequestStop();
			m_state = eSccheduleState_Stopping;
		}
		else if (m_state != eScheduleState_Completed)
		{
			m_state = eScheduleState_Failed;
			return eScheduleWorkItemStatus_Failed;
		}
	}

	// process internal state machine
	switch (m_state)
	{
	// final state - work failed
	case eScheduleState_Failed:
		{
			retStatus = eScheduleWorkItemStatus_Failed;
			break;
		}

	// final state - work completed
	case eScheduleState_Completed:
		{
			retStatus = eScheduleWorkItemStatus_Finished;
			break;
		}

	// stopping current task
	case eSccheduleState_Stopping:
		{
			if (m_currentItem < m_items.size())
			{
				CScheduleItem* pItem = m_items[m_currentItem];
				if (pItem->Update() != eScheduleWorkItemStatus_NotFinished)
				{
					// task was finally stopped
					m_state = eScheduleState_Failed;
					retStatus = eScheduleWorkItemStatus_Failed;
				}
			}

			break;
		}

	// first update, switch to processing
	case eScheduleState_Pending:
		{
			m_state = eScheduleState_Processing;
			m_currentItem = 0;
			/* FALLS THROUGHT */
		}

	// if we were in the processing phase inform the current schedule item to stop all it's work
	case eScheduleState_Processing:
		{
			// update schedule items
			while (m_currentItem < m_items.size())
			{
				CScheduleItem* pItem = m_items[m_currentItem];

				const EScheduleWorkItemStatus itemStatus = pItem->Update();
				if (itemStatus == eScheduleWorkItemStatus_Finished)
				{
					m_currentItem += 1;
					continue;
				}
				else if (itemStatus == eScheduleWorkItemStatus_Failed)
				{
					m_state = eScheduleState_Failed;
					retStatus = eScheduleWorkItemStatus_Failed;
					gEnv->pLog->LogWarning("Schedule '%s' failed on item '%s'.", GetDescription(), pItem->GetDescription());
				}

				break;
			}

			// all items updated
			if (m_currentItem >= m_items.size())
			{
				// empty schedule, complete in one tick
				m_state = eScheduleState_Completed;
				retStatus = eScheduleWorkItemStatus_Finished;
				CryLog("Schedule '%s' completed", GetDescription());
			}

			break;
		}
	}

	return retStatus;
}

//-----------------------------------------------------------------------------

CScheduleManager::CScheduleManager()
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CScheduleManager::~CScheduleManager()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);

	for (TSchedules::const_iterator it = m_schedules.begin();
	     it != m_schedules.end(); ++it)
	{
		CSchedule* pSchedule = *it;
		SAFE_RELEASE(pSchedule);
	}

	m_schedules.clear();
}

IBackgroundSchedule* CScheduleManager::CreateSchedule(const char* szName)
{
	return new CSchedule(szName);
}

IBackgroundScheduleItem* CScheduleManager::CreateScheduleItem(const char* szName)
{
	return new CScheduleItem(szName);
}

void CScheduleManager::SubmitSchedule(IBackgroundSchedule* pSchedule)
{
	if (NULL != pSchedule)
	{
		if (pSchedule->GetState() != eScheduleState_Pending)
		{
			CryFatalError("Only schedules with pending state can be submitted");
			return;
		}

		pSchedule->AddRef();
		m_schedules.push_back(static_cast<CSchedule*>(pSchedule));
	}
}

const uint32 CScheduleManager::GetNumSchedules() const
{
	return m_schedules.size();
}

IBackgroundSchedule* CScheduleManager::GetSchedule(const uint32 index) const
{
	return m_schedules[index];
}

void CScheduleManager::Update()
{
	while (!m_schedules.empty())
	{
		CSchedule* pSchedule = m_schedules[0];

		const EScheduleWorkItemStatus status = pSchedule->Update();
		if (status == eScheduleWorkItemStatus_NotFinished)
		{
			// we need more work next frame
			break;
		}

		// schedule has finished, remove current reference
		m_schedules.erase(m_schedules.begin());
		SAFE_RELEASE(pSchedule);
	}
}

void CScheduleManager::OnEditorNotifyEvent(EEditorNotifyEvent ev)
{
	switch (ev)
	{
	case eNotify_OnQuit:
		GetIEditorImpl()->UnregisterNotifyListener(this);
		break;
	}
}

//-----------------------------------------------------------------------------

} // BackgroundScheduleManager

