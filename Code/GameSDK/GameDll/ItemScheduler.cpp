// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 1:9:2005   11:32 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ItemScheduler.h"
#include "Item.h"
#include "Game.h"
#include "IGameObject.h"

CSchedulerActionPoolBase* CSchedulerActionPoolBase::s_poolListHead;

//------------------------------------------------------------------------
CItemScheduler::CItemScheduler(CItem *item)
: m_busy(false),
	m_pTimer(0),
	m_pItem(item),
	m_locked(false)
{
	m_pTimer = gEnv->pTimer;
}

//------------------------------------------------------------------------
CItemScheduler::~CItemScheduler()
{
	Reset();
}

//------------------------------------------------------------------------
void CItemScheduler::Reset(bool keepPersistent)
{
	for (TTimerActionVector::iterator it = m_timers.begin(); it != m_timers.end();)
	{
		if (!it->persist || !keepPersistent)
		{
			it->action->destroy();
			it = m_timers.erase(it);
		}
		else
			++it;
	}

	for (TScheduledActionVector::iterator it = m_schedule.begin(); it != m_schedule.end();)
	{
		if (!it->persist || !keepPersistent)
		{
			it->action->destroy();
			it = m_schedule.erase(it);
		}
		else
			++it;
	}

	if (m_timers.empty() && m_schedule.empty())
		m_pItem->EnableUpdate(false, eIUS_Scheduler);

  SetBusy(false);
}

//------------------------------------------------------------------------
void CItemScheduler::Update(float frameTime)
{
	if (frameTime > 0.2f)
		frameTime = 0.2f;

	if (!m_schedule.empty())
	{
		while(!m_schedule.empty() && !m_busy)
		{
			SScheduledAction &action = *m_schedule.begin();
			ISchedulerAction *pAction= action.action;
			m_schedule.erase(m_schedule.begin());

			pAction->execute(m_pItem);
			pAction->destroy();
		}
	}

	if (!m_timers.empty())
	{
		uint32 count=0;
		m_actives.swap(m_timers);

		for (TTimerActionVector::iterator it = m_actives.begin(); it != m_actives.end(); ++it)
		{
			STimerAction &action = *it;
			action.time -= frameTime;
			if (action.time <= 0.0f)
			{
				action.action->execute(m_pItem);
				action.action->destroy();
				++count;
			}
		}

		if (count)
			m_actives.erase(m_actives.begin(), m_actives.begin()+count);

		if (!m_timers.empty())
		{
			for (TTimerActionVector::iterator it=m_timers.begin(); it!=m_timers.end(); ++it)
				m_actives.push_back(*it);

			std::sort(m_actives.begin(), m_actives.end(), compare_timers());
		}

		m_actives.swap(m_timers);
		m_actives.resize(0);
	}

	if (m_timers.empty() && m_schedule.empty())
		m_pItem->EnableUpdate(false, eIUS_Scheduler);
}

//------------------------------------------------------------------------
void CItemScheduler::ScheduleAction(ISchedulerAction *action, bool persistent)
{
	if (m_locked)
		return;

	if (!m_busy)
	{
		action->execute(m_pItem);
		return;
	}

	SScheduledAction scheduleAction;
	scheduleAction.action = action;
	scheduleAction.persist = persistent;

	m_schedule.push_back(scheduleAction);

	m_pItem->EnableUpdate(true, eIUS_Scheduler);
}

//------------------------------------------------------------------------
void CItemScheduler::TimerAction(float fTimeSeconds, ISchedulerAction *action, bool persistent/*=false*/)
{
	if (m_locked)
		return;

	STimerAction timerAction;
	timerAction.action = action;
	timerAction.time = fTimeSeconds;
	timerAction.persist = persistent;

	m_timers.push_back(timerAction);
	std::sort(m_timers.begin(), m_timers.end(), compare_timers());

	m_pItem->EnableUpdate(true, eIUS_Scheduler);
}

//------------------------------------------------------------------------
void CItemScheduler::SetBusy(bool busy)
{
	if (m_locked)
		return;

	m_busy = busy;
}

//------------------------------------------------------------------------
void CItemScheduler::Lock(bool lock)
{
	m_locked=lock;
}

//------------------------------------------------------------------------
bool CItemScheduler::IsLocked()
{
	return m_locked;
}

void CItemScheduler::GetMemoryStatistics(ICrySizer * s) const
{
	s->AddContainer(m_timers);
	s->AddContainer(m_actives);
	s->AddContainer(m_schedule);	
}