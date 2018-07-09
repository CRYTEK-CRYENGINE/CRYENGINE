// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Item Scheduler

-------------------------------------------------------------------------
History:
- 1:9:2005   11:33 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMSCHEDULER_H__
#define __ITEMSCHEDULER_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <vector>
#include <CrySystem/ITimer.h>
#include <CryMemory/PoolAllocator.h>


class CItem;

// never inherit directly from ISchedulerAction, instead write a class that looks like:
// class Blah { void execute(CItem *) { ... } };
// and use a pAction = CSchedulerAction<Blah>::Create(Blah(...));
struct ISchedulerAction
{
	virtual	~ISchedulerAction(){}
	virtual void execute(CItem *_this) = 0;
	virtual void destroy() = 0;
	virtual void GetMemoryStatistics(ICrySizer * s) = 0;
};

class CSchedulerActionPoolBase
{
public:
	static CSchedulerActionPoolBase* s_poolListHead;

public:
	static void ResetAll()
	{
		for (CSchedulerActionPoolBase* it = s_poolListHead; it; it = it->m_pNext)
		{
			it->Reset();
		}
	}

public:
	CSchedulerActionPoolBase()
	{
		m_pNext = s_poolListHead;
		m_pPrev = NULL;
		s_poolListHead = this;
	}

	virtual ~CSchedulerActionPoolBase()
	{
		if (m_pNext)
			m_pNext->m_pPrev = m_pPrev;
			
		if (this == s_poolListHead)
			s_poolListHead = m_pNext;
		else
			m_pPrev->m_pNext = m_pNext;
	}

public:
	virtual void Reset() = 0;

private:
	CSchedulerActionPoolBase(const CSchedulerActionPoolBase&);
	CSchedulerActionPoolBase& operator = (const CSchedulerActionPoolBase&);

private:
	CSchedulerActionPoolBase* m_pPrev;
	CSchedulerActionPoolBase* m_pNext;
};

// second sizeof(void*) is just for safety!
template <typename T>
class CSchedulerActionPool : public CSchedulerActionPoolBase, public stl::PoolAllocator<sizeof(T) + sizeof(void*) + sizeof(void*), stl::PoolAllocatorSynchronizationSinglethreaded>
{
public: // CSchedulerActionPoolBase Members
	void Reset()
	{
		assert (this->GetTotalMemory().nUsed == 0);
		this->FreeMemory();
	}
};

template <class T>
class CSchedulerAction : public ISchedulerAction
{
	typedef CSchedulerActionPool<T> Alloc;
	static Alloc m_alloc;

public:
	static CSchedulerAction * Create()
	{
		return new (m_alloc.Allocate()) CSchedulerAction();
	}
	static CSchedulerAction * Create( const T& from )
	{
		return new (m_alloc.Allocate()) CSchedulerAction(from);
	}

	void execute(CItem * _this) { m_impl.execute(_this); }
	void destroy() 
	{ 
		this->~CSchedulerAction();
		m_alloc.Deallocate(this);
	}
	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

private:
	T m_impl;

	CSchedulerAction() {}
	CSchedulerAction( const T& from ) : m_impl(from) {}
	~CSchedulerAction() {}
};

template <class T>
typename CSchedulerAction<T>::Alloc CSchedulerAction<T>::m_alloc;


class CItemScheduler
{
	struct SScheduledAction
	{
		ISchedulerAction	*action;
		bool							persist;
		void GetMemoryUsage(ICrySizer *pSizer) const{}
	};
	struct STimerAction
	{
		ISchedulerAction	*action;
		float							time;
		bool							persist;
		void GetMemoryUsage(ICrySizer *pSizer) const{}
	};

	typedef std::vector<STimerAction>									TTimerActionVector;
	typedef std::vector<SScheduledAction>							TScheduledActionVector;

	struct compare_timers
	{
		bool operator() (const STimerAction &lhs, const STimerAction &rhs ) const
		{
			return lhs.time < rhs.time;
		}
	};

public:
	CItemScheduler(CItem *item);
	virtual ~CItemScheduler();
	void Reset(bool keepPersistent=false);
	void Update(float frameTime);
	ILINE void TimerAction(uint32 time, ISchedulerAction *action, bool persistent=false) { TimerAction((float)time*0.001f, action, persistent); }
	ILINE void TimerAction(int time, ISchedulerAction *action, bool persistent=false) { TimerAction((float)time*0.001f, action, persistent); }
	void TimerAction(float fTimeSeconds, ISchedulerAction *action, bool persistent=false);
	void ScheduleAction(ISchedulerAction *action, bool persistent=false);
	void GetMemoryStatistics(ICrySizer * s) const;

	bool ILINE IsBusy() const { return m_busy; };
	void SetBusy(bool busy);

	void Lock(bool lock);
	bool IsLocked();

private:
	bool				m_locked;
	bool				m_busy;
	ITimer			*m_pTimer;
	CItem				*m_pItem;

	TTimerActionVector				m_timers;
	TTimerActionVector				m_actives;
	TScheduledActionVector		m_schedule;
};



#endif //__ITEMSCHEDULER_H__
