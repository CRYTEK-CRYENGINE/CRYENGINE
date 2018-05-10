// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WORKQUEUE_H__
#define __WORKQUEUE_H__

#pragma once

#include <queue>
#include "PolymorphicQueue.h"

template<class T>
inline bool IsDead(const _smart_ptr<T>& pI)
{
	return pI->IsDead();
}

// a queue of things that need to be done
class CWorkQueue
{
public:
	void Flush(bool rt)
	{
		Executer e;
		if (rt)
			m_jobQueue.RealtimeFlush(e);
		else
			m_jobQueue.Flush(e);
	}

	void FlushEmpty();
	void Swap(CWorkQueue& wq)
	{
		m_jobQueue.Swap(wq.m_jobQueue);
	}
	bool Empty()
	{
		return m_jobQueue.Empty();
	}

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CWorkQueue");

		if (countingThis)
			pSizer->Add(*this);
		m_jobQueue.GetMemoryStatistics(pSizer);
	}

private:
	struct IJob
	{
		virtual void Execute() = 0;
		virtual ~IJob() {}
	};
	CPolymorphicQueue<IJob> m_jobQueue;

	template<class T, class U>
	class CClassJob : public IJob
	{
	public:
		CClassJob(void(U::* func)(const T &), U* pCls, const T& data) : m_func(func), m_pCls(pCls), m_data(data)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data);
		}

	private:
		void (U::* m_func)(const T&);
		T             m_data;
		_smart_ptr<U> m_pCls;
	};

	template<class T, class U>
	class CClassJobNoRef : public IJob
	{
	public:
		CClassJobNoRef(void(U::* func)(T), U* pCls, T data) : m_func(func), m_pCls(pCls), m_data(data)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data);
		}

	private:
		void (U::* m_func)(T);
		T             m_data;
		_smart_ptr<U> m_pCls;
	};

	template<class T1, class T2, class U>
	class CClassJobNoRef2 : public IJob
	{
	public:
		CClassJobNoRef2(void(U::* func)(T1, T2), U* pCls, T1 data1, T2 data2) : m_func(func), m_pCls(pCls), m_data1(data1), m_data2(data2)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data1, m_data2);
		}

	private:
		void (U::* m_func)(T1, T2);
		T1            m_data1;
		T2            m_data2;
		_smart_ptr<U> m_pCls;
	};

	template<class T1, class T2, class T3, class U>
	class CClassJobNoRef3 : public IJob
	{
	public:
		CClassJobNoRef3(void(U::* func)(T1, T2, T3), U* pCls, T1 data1, T2 data2, T3 data3) : m_func(func), m_pCls(pCls), m_data1(data1), m_data2(data2), m_data3(data3)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data1, m_data2, m_data3);
		}

	private:
		void (U::* m_func)(T1, T2, T3);
		T1            m_data1;
		T2            m_data2;
		T3            m_data3;
		_smart_ptr<U> m_pCls;
	};

	template<class T1, class T2, class T3, class T4, class U>
	class CClassJobNoRef4 : public IJob
	{
	public:
		CClassJobNoRef4(void(U::* func)(T1, T2, T3, T4), U* pCls, T1 data1, T2 data2, T3 data3, T4 data4) : m_func(func), m_pCls(pCls), m_data1(data1), m_data2(data2), m_data3(data3), m_data4(data4)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data1, m_data2, m_data3, m_data4);
		}

	private:
		void (U::* m_func)(T1, T2, T3, T4);
		T1            m_data1;
		T2            m_data2;
		T3            m_data3;
		T4            m_data4;
		_smart_ptr<U> m_pCls;
	};

	template<class T1, class T2, class T3, class T4, class T5, class U>
	class CClassJobNoRef5 : public IJob
	{
	public:
		CClassJobNoRef5(void(U::* func)(T1, T2, T3, T4, T5), U* pCls, T1 data1, T2 data2, T3 data3, T4 data4, T5 data5) : m_func(func), m_pCls(pCls), m_data1(data1), m_data2(data2), m_data3(data3), m_data4(data4), m_data5(data5)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data1, m_data2, m_data3, m_data4, m_data5);
		}

	private:
		void (U::* m_func)(T1, T2, T3, T4, T5);
		T1            m_data1;
		T2            m_data2;
		T3            m_data3;
		T4            m_data4;
		T5            m_data5;
		_smart_ptr<U> m_pCls;
	};

	template<class T1, class T2, class T3, class T4, class T5, class T6, class U>
	class CClassJobNoRef6 : public IJob
	{
	public:
		CClassJobNoRef6(void(U::* func)(T1, T2, T3, T4, T5, T6), U* pCls, T1 data1, T2 data2, T3 data3, T4 data4, T5 data5, T6 data6) : m_func(func), m_pCls(pCls), m_data1(data1), m_data2(data2), m_data3(data3), m_data4(data4), m_data5(data5), m_data6(data6)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)(m_data1, m_data2, m_data3, m_data4, m_data5, m_data6);
		}

	private:
		void (U::* m_func)(T1, T2, T3, T4, T5, T6);
		T1            m_data1;
		T2            m_data2;
		T3            m_data3;
		T4            m_data4;
		T5            m_data5;
		T6            m_data6;
		_smart_ptr<U> m_pCls;
	};

	template<class U>
	class CClassJob<void, U> : public IJob
	{
	public:
		CClassJob(void(U::* func)(), U* pCls) : m_func(func), m_pCls(pCls)
		{
		}

		void Execute()
		{
			if (!IsDead(m_pCls))
				((*m_pCls).*m_func)();
		}

	private:
		void (U::* m_func)();
		_smart_ptr<U> m_pCls;
	};

	template<class U>
	class CAtSyncItem : public IJob
	{
	public:
		CAtSyncItem() : m_pItem(0) {}
		~CAtSyncItem()
		{
			if (m_pItem)
				m_pItem->DeleteThis();
		}
		void Execute()
		{
			if (m_pItem && !IsDead(m_pCls))
			{
				EDisconnectionCause disconnectCause = eDC_Unknown;
				string message = "Disconnected";
				if (!m_pItem->SyncWithError(disconnectCause, message))
				{
					NetWarning("%s failed during sync, forcefully killing owner", __FUNCTION__);
					m_pCls->Die(disconnectCause, message);
				}
			}
		}
		void SetItem(INetAtSyncItem* pItem, U* pCls)
		{
			NET_ASSERT(!m_pItem);
			m_pItem = pItem;
			m_pCls = pCls;
		}

	private:
		CAtSyncItem(const CAtSyncItem&);
		CAtSyncItem& operator=(const CAtSyncItem&);
		INetAtSyncItem* m_pItem;
		_smart_ptr<U>   m_pCls;
	};

	template<class U, class K>
	class CAtSyncItemWithFollowup : public CAtSyncItem<K>
	{
	public:
		void Execute()
		{
			CAtSyncItem<K>::Execute();
			if (m_pCls && !IsDead(m_pCls))
				((*m_pCls).*m_func)();
		}

		CAtSyncItemWithFollowup* SetFollowup(void (U::* func)(), _smart_ptr<U> pFollowUp)
		{
			m_func = func;
			m_pCls = pFollowUp;
			return this;
		}

	private:
		void (U::* m_func)();
		_smart_ptr<U> m_pCls;
	};

	struct Executer
	{
		ILINE void operator()(IJob* pJob)
		{
			pJob->Execute();
		}
	};
	struct DoNothingOp
	{
		ILINE void operator()(IJob* pJob) {}
	};

public:
	template<class T, class U>
	void Add(void (U::* func)(const T&), U* pCls, const T& data)
	{
		m_jobQueue.Add(CClassJob<T, U>(func, pCls, data));
	}
	template<class T, class U>
	void Add(void (U::* func)(T), U* pCls, T data)
	{
		m_jobQueue.Add(CClassJobNoRef<T, U>(func, pCls, data));
	}
	template<class T1, class T2, class U>
	void Add(void (U::* func)(T1, T2), U* pCls, T1 data1, T2 data2)
	{
		m_jobQueue.Add(CClassJobNoRef2<T1, T2, U>(func, pCls, data1, data2));
	}
	template<class T1, class T2, class T3, class U>
	void Add(void (U::* func)(T1, T2, T3), U* pCls, T1 data1, T2 data2, T3 data3)
	{
		m_jobQueue.Add(CClassJobNoRef3<T1, T2, T3, U>(func, pCls, data1, data2, data3));
	}
	template<class T1, class T2, class T3, class T4, class U>
	void Add(void (U::* func)(T1, T2, T3, T4), U* pCls, T1 data1, T2 data2, T3 data3, T4 data4)
	{
		m_jobQueue.Add(CClassJobNoRef4<T1, T2, T3, T4, U>(func, pCls, data1, data2, data3, data4));
	}
	template<class T1, class T2, class T3, class T4, class T5, class U>
	void Add(void (U::* func)(T1, T2, T3, T4, T5), U* pCls, T1 data1, T2 data2, T3 data3, T4 data4, T5 data5)
	{
		m_jobQueue.Add(CClassJobNoRef5<T1, T2, T3, T4, T5, U>(func, pCls, data1, data2, data3, data4, data5));
	}
	template<class T1, class T2, class T3, class T4, class T5, class T6, class U>
	void Add(void (U::* func)(T1, T2, T3, T4, T5, T6), U* pCls, T1 data1, T2 data2, T3 data3, T4 data4, T5 data5, T6 data6)
	{
		m_jobQueue.Add(CClassJobNoRef6<T1, T2, T3, T4, T5, T6, U>(func, pCls, data1, data2, data3, data4, data5, data6));
	}
	template<class U>
	void Add(void (U::* func)(), U* pCls)
	{
		m_jobQueue.Add(CClassJob<void, U>(func, pCls));
	}
	template<class U>
	void Add(INetAtSyncItem* pItem, U* pCls)
	{
		if (pItem)
			m_jobQueue.Add<CAtSyncItem<U>>()->SetItem(pItem, pCls);
	}
	template<class K, class U>
	void Add(INetAtSyncItem* pItem, K* pCls, U* pCls2, void (U::* func)())
	{
		if (pItem)
			m_jobQueue.Add<CAtSyncItemWithFollowup<U, K>>()->SetFollowup(func, pCls2)->SetItem(pItem, pCls);
	}
};

#endif
