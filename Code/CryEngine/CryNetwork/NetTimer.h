// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETTIMER_H__
#define __NETTIMER_H__

#pragma once

///////////////////////////////////////////////////////////////////////////////
//
// Old net timers
//

#include <CrySystem/TimeValue.h>
#include <queue>
//#include "VectorMap.h"
#include <CryMemory/STLPoolAllocator.h>
#include "Config.h"

typedef uint32  NetTimerId;
typedef void (* NetTimerCallback)(NetTimerId, void*, CTimeValue);

static const int TIMER_HERTZ = 250;

class CNetTimer
{
public:
	CNetTimer();
#if TIMER_DEBUG
	NetTimerId AddTimer(CTimeValue when, NetTimerCallback callback, void* pUserData, char* pFile, size_t line, char* pName);
#else
	NetTimerId AddTimer(CTimeValue when, NetTimerCallback callback, void* pUserData);
#endif // TIMER_DEBUG
	void*      CancelTimer(NetTimerId& id);
	CTimeValue Update();

	void       GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CNetTimer");
		if (countingThis)
			pSizer->Add(*this);
		pSizer->AddContainer(m_callbacks);
	}

private:
	static const int TIMER_SLOTS = 32; // 0.128 seconds
	int              m_timerCallbacks[TIMER_SLOTS];
	int              m_curSlot;
#if USE_SYSTEM_ALLOCATOR
	std::multimap<CTimeValue, int, std::less<CTimeValue>> m_slowCallbacks;
#else
	std::multimap<CTimeValue, int, std::less<CTimeValue>, stl::STLPoolAllocator<std::pair<const CTimeValue, int>, stl::PoolAllocatorSynchronizationSinglethreaded>> m_slowCallbacks;
#endif

	struct SCallbackInfo
	{
		SCallbackInfo(NetTimerCallback callback, void* pUserData)
		{
			this->callback = callback;
			this->pUserData = pUserData;
			next = -1;
			cancelled = true;
		}
		SCallbackInfo()
		{
			this->callback = 0;
			this->pUserData = 0;
			next = -1;
			cancelled = true;
		}
		NetTimerCallback callback;
		void*            pUserData;
		int              next;
		bool             cancelled;
#if TIMER_DEBUG
		CTimeValue       schedTime;
		int              slotsInFuture;
		int              curSlotWhenScheduled;
#endif
	};

	typedef std::vector<int, stl::STLGlobalAllocator<int>> TGlobalIntVector;

	std::vector<SCallbackInfo, stl::STLGlobalAllocator<SCallbackInfo>> m_callbacks;
	TGlobalIntVector m_freeCallbacks;
	TGlobalIntVector m_execCallbacks;
	CTimeValue       m_wakeup;
	CTimeValue       m_lastExec;
#if TIMER_DEBUG
	CTimeValue       m_epoch; // just for debugging
#endif
};

//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// Accurate net timers
//

class CAccurateNetTimer
{
public:
	CAccurateNetTimer();
#if TIMER_DEBUG
	NetTimerId AddTimer(CTimeValue when, NetTimerCallback callback, void* pUserData, char* pFile, size_t line, char* pName);
#else
	NetTimerId AddTimer(CTimeValue when, NetTimerCallback callback, void* pUserData);
#endif // TIMER_DEBUG
	void*      CancelTimer(NetTimerId& id);
	CTimeValue Update();

	void       GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CAccurateNetTimer");
		if (countingThis)
			pSizer->Add(*this);
		pSizer->AddContainer(m_callbacks);
	}

private:
	struct SCallbackInfo
	{
		SCallbackInfo(NetTimerCallback callback, void* pUserData)
		{
			this->callback = callback;
			this->pUserData = pUserData;
			inUse = true;
		}
		SCallbackInfo()
		{
			this->callback = 0;
			this->pUserData = 0;
			inUse = false;
		}
		NetTimerCallback     callback;
		void*                pUserData;
		bool                 inUse;
		CTimeValue           schedTime;
#if TIMER_DEBUG
		CTimeValue           timeAdded;
		CryFixedStringT<256> location;
		CryFixedStringT<128> name;
#endif
	};

	typedef std::vector<int, stl::STLGlobalAllocator<int>> TGlobalIntVector;

	std::vector<SCallbackInfo, stl::STLGlobalAllocator<SCallbackInfo>> m_callbacks;
	TGlobalIntVector m_freeCallbacks;
	CTimeValue       m_nextUpdate;
};

//
///////////////////////////////////////////////////////////////////////////////

#endif
