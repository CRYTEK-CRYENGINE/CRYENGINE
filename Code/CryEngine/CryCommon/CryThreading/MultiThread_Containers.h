// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MultiThread_Containers.h
//  Version:     v1.00
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MultiThread_Containters_h__
#define __MultiThread_Containters_h__
#pragma once

#include <CrySystem/Pipe.h>
#include <CryCore/StlUtils.h>
#include <CryCore/BitFiddling.h>

#include <queue>
#include <set>
#include <algorithm>

namespace CryMT
{
// Thread Safe wrappers on the standard STL containers.

//! Multi-Thread safe queue container, can be used instead of std::vector.
template<class T, class Alloc = std::allocator<T>>
class queue
{
public:
	typedef T                      value_type;
	typedef std::vector<T, Alloc>  container_type;
	typedef CryAutoCriticalSection AutoLock;

	// std::queue interface
	const T& front() const           { AutoLock lock(m_cs); return v.front(); };
	const T& back() const            { AutoLock lock(m_cs);  return v.back(); }
	void     push(const T& x)        { AutoLock lock(m_cs); return v.push_back(x); };
	void     reserve(const size_t n) { AutoLock lock(m_cs); v.reserve(n); };
	// classic pop function of queue should not be used for thread safety, use try_pop instead
	//void	pop()							{ AutoLock lock(m_cs); return v.erase(v.begin()); };

	CryCriticalSection& get_lock() const { return m_cs; }

	bool                empty() const    { AutoLock lock(m_cs); return v.empty(); }
	int                 size() const     { AutoLock lock(m_cs); return v.size(); }
	void                clear()          { AutoLock lock(m_cs); v.clear(); }
	void                free_memory()    { AutoLock lock(m_cs); stl::free_container(v); }

	template<class Func>
	void sort(const Func& compare_less) { AutoLock lock(m_cs); std::sort(v.begin(), v.end(), compare_less); }

	//////////////////////////////////////////////////////////////////////////
	bool try_pop(T& returnValue)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			returnValue = v.front();
			v.erase(v.begin());
			return true;
		}
		return false;
	};

	//////////////////////////////////////////////////////////////////////////
	bool try_remove(const T& value)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			typename container_type::iterator it = std::find(v.begin(), v.end(), value);
			if (it != v.end())
			{
				v.erase(it);
				return true;
			}
		}
		return false;
	};

	template<typename Sizer>
	void GetMemoryUsage(Sizer* pSizer) const
	{
		pSizer->AddObject(v);
	}

	container_type pop_all()
	{
		container_type result;
		{
			AutoLock lock(m_cs);
			v.swap(result);
		}
		return result;
	}

private:
	container_type             v;
	mutable CryCriticalSection m_cs;
};

//! Multi-Thread safe vector container, can be used instead of std::vector.
template<class T>
class vector
{
public:
	typedef T                      value_type;
	typedef CryAutoCriticalSection AutoLock;

	CryCriticalSection& get_lock() const { return m_cs; }

	void                free_memory()    { AutoLock lock(m_cs); stl::free_container(v); }

	// std::vector interface
	bool     empty() const                { AutoLock lock(m_cs); return v.empty(); }
	int      size() const                 { AutoLock lock(m_cs); return v.size(); }
	void     resize(int sz)               { AutoLock lock(m_cs); v.resize(sz); }
	void     reserve(int sz)              { AutoLock lock(m_cs); v.reserve(sz); }
	size_t   capacity() const             { AutoLock lock(m_cs); return v.size(); }
	void     clear()                      { AutoLock lock(m_cs); v.clear(); }
	T&       operator[](size_t pos)       { AutoLock lock(m_cs); return v[pos]; }
	const T& operator[](size_t pos) const { AutoLock lock(m_cs); return v[pos]; }
	const T& front() const                { AutoLock lock(m_cs); return v.front(); }
	const T& back() const                 { AutoLock lock(m_cs);  return v.back(); }
	T&       back()                       { AutoLock lock(m_cs);  return v.back(); }

	void     push_back(const T& x)        { AutoLock lock(m_cs); return v.push_back(x); }
	void     pop_back()                   { AutoLock lock(m_cs);  return v.pop_back(); }
	//////////////////////////////////////////////////////////////////////////

	template<class Func>
	void sort(const Func& compare_less) { AutoLock lock(m_cs); std::sort(v.begin(), v.end(), compare_less); }

	template<class Iter>
	void append(const Iter& startRange, const Iter& endRange) { AutoLock lock(m_cs); v.insert(v.end(), startRange, endRange); }

	void swap(std::vector<T>& vec)                            { AutoLock lock(m_cs); v.swap(vec); }

	//////////////////////////////////////////////////////////////////////////
	bool try_pop_front(T& returnValue)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			returnValue = v.front();
			v.erase(v.begin());
			return true;
		}
		return false;
	};
	bool try_pop_back(T& returnValue)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			returnValue = v.back();
			v.pop_back();
			return true;
		}
		return false;
	};

	//////////////////////////////////////////////////////////////////////////
	template<typename FindFunction, typename KeyType>
	bool find_and_copy(FindFunction findFunc, const KeyType& key, T& foundValue) const
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			typename std::vector<T>::const_iterator it;
			for (it = v.begin(); it != v.end(); ++it)
			{
				if (findFunc(key, *it))
				{
					foundValue = *it;
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool try_remove(const T& value)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			typename std::vector<T>::iterator it = std::find(v.begin(), v.end(), value);
			if (it != v.end())
			{
				v.erase(it);
				return true;
			}
		}
		return false;
	};

	//////////////////////////////////////////////////////////////////////////
	template<typename Predicate>
	bool try_remove_and_erase_if(Predicate predicateFunction)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			typename std::vector<T>::iterator it = std::remove_if(v.begin(), v.end(), predicateFunction);
			if (it != v.end())
			{
				v.erase(it, v.end());
				return true;
			}
		}
		return false;
	};

	//////////////////////////////////////////////////////////////////////////
	bool try_remove_at(size_t idx)
	{
		AutoLock lock(m_cs);
		if (idx < v.size())
		{
			v.erase(v.begin() + idx);
			return true;
		}
		return false;
	}

	//! Fast remove - just move last elem over deleted element - order is not preserved.
	bool try_remove_unordered(const T& value)
	{
		AutoLock lock(m_cs);
		if (!v.empty())
		{
			typename std::vector<T>::iterator it = std::find(v.begin(), v.end(), value);
			if (it != v.end())
			{
				if (v.size() > 1)
				{
					typename std::vector<T>::iterator it_back = v.end() - 1;

					if (it != it_back)
					{
						*it = *it_back;
					}

					v.erase(it_back);
				}
				else
				{
					v.erase(it);
				}
				return true;
			}
		}
		return false;
	};

	vector() {}

	vector(const vector<T>& rOther)
	{
		AutoLock lock1(m_cs);
		AutoLock lock2(rOther.m_cs);

		v = rOther.v;
	}

	vector& operator=(const vector<T>& rOther)
	{
		if (this == &rOther)
			return *this;

		AutoLock lock1(m_cs);
		AutoLock lock2(rOther.m_cs);

		v = rOther.v;

		return *this;
	}
private:
	std::vector<T>             v;
	mutable CryCriticalSection m_cs;
};

//! Multi-Thread safe set container, can be used instead of std::set.
//! It has limited functionality, but most of it is there.
template<class T>
class set
{
public:
	typedef T                               value_type;
	typedef T                               Key;
	typedef typename std::set<T>::size_type size_type;
	typedef CryAutoCriticalSection          AutoLock;

	// Methods
	void                clear()                              { AutoLock lock(m_cs); s.clear(); }
	size_type           count(const Key& _Key) const         { AutoLock lock(m_cs); return s.count(_Key); }
	bool                empty() const                        { AutoLock lock(m_cs); return s.empty(); }
	size_type           erase(const Key& _Key)               { AutoLock lock(m_cs); return s.erase(_Key); }

	bool                find(const Key& _Key)                { AutoLock lock(m_cs); return (s.find(_Key) != s.end()); }

	bool                pop_front(value_type& rFrontElement) { AutoLock lock(m_cs); if (s.empty()) { return false; } rFrontElement = *s.begin(); s.erase(s.begin()); return true; }
	bool                pop_front()                          { AutoLock lock(m_cs); if (s.empty()) { return false; } s.erase(s.begin()); return true; }

	bool                front(value_type& rFrontElement)     { AutoLock lock(m_cs); if (s.empty()) { return false; } rFrontElement = *s.begin(); return true; }

	bool                insert(const value_type& _Val)       { AutoLock lock(m_cs); return s.insert(_Val).second; }
	size_type           max_size() const                     { AutoLock lock(m_cs); return s.max_size(); }
	size_type           size() const                         { AutoLock lock(m_cs); return s.size(); }
	void                swap(set& _Right)                    { AutoLock lock(m_cs); s.swap(_Right); }

	CryCriticalSection& get_lock()                           { return m_cs; }

private:
	std::set<value_type>       s;
	mutable CryCriticalSection m_cs;
};

//! Multi-thread safe lock-less FIFO queue container for passing pointers between threads.
//! The queue only stores pointers to T, it does not copy the contents of T.
template<class T, class Alloc = std::allocator<T>>
class CLocklessPointerQueue
{
public:
	explicit CLocklessPointerQueue(size_t reserve = 32) { m_lockFreeQueue.reserve(reserve); };
	~CLocklessPointerQueue() {};

	//! Checks if queue is empty.
	bool empty() const;

	//! Pushes item to the queue, only pointer is stored, T contents are not copied.
	void push(T* ptr);

	//! pop can return NULL, always check for it before use.
	T* pop();

private:
	typedef typename Alloc::template rebind<T*> Alloc_rebind;
	queue<T*, typename Alloc_rebind::other> m_lockFreeQueue;
};

//////////////////////////////////////////////////////////////////////////
template<class T, class Alloc>
inline bool CLocklessPointerQueue<T, Alloc >::empty() const
{
	return m_lockFreeQueue.empty();
}

//////////////////////////////////////////////////////////////////////////
template<class T, class Alloc>
inline void CLocklessPointerQueue<T, Alloc >::push(T* ptr)
{
	m_lockFreeQueue.push(ptr);
}

//////////////////////////////////////////////////////////////////////////
template<class T, class Alloc>
inline T* CLocklessPointerQueue<T, Alloc >::pop()
{
	T* val = NULL;
	m_lockFreeQueue.try_pop(val);
	return val;
}

//! Producer/Consumer Queue for 1 to 1 thread communication.
//! Realized with only volatile variables and memory barriers.
//! \note This producer/consumer queue is only thread safe in a 1 to 1 situation
//! and doesn't provide any yields or similar to prevent spinning.
template<typename T>
class CRY_ALIGN(128) SingleProducerSingleConsumerQueue: public CryMT::detail::SingleProducerSingleConsumerQueueBase
{
public:
	SingleProducerSingleConsumerQueue();
	~SingleProducerSingleConsumerQueue();

	void Init(size_t nSize);

	void Push(const T &rObj);
	void Pop(T * pResult);

private:
	T* m_arrBuffer;
	uint32 m_nBufferSize;

	CRY_ALIGN(16) volatile uint32 m_nProducerIndex;
	CRY_ALIGN(64) volatile uint32 m_nComsumerIndex;
};

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline SingleProducerSingleConsumerQueue<T>::SingleProducerSingleConsumerQueue() : m_arrBuffer(NULL), m_nBufferSize(0), m_nProducerIndex(0), m_nComsumerIndex(0)
{
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline SingleProducerSingleConsumerQueue<T>::~SingleProducerSingleConsumerQueue()
{
	CryModuleMemalignFree(m_arrBuffer);
	m_nBufferSize = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void SingleProducerSingleConsumerQueue<T >::Init(size_t nSize)
{
	assert(m_arrBuffer == NULL);
	assert(m_nBufferSize == 0);
	assert((nSize & (nSize - 1)) == 0);

	m_arrBuffer = alias_cast<T*>(CryModuleMemalign(nSize * sizeof(T), 16));
	m_nBufferSize = nSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void SingleProducerSingleConsumerQueue<T >::Push(const T& rObj)
{
	assert(m_arrBuffer != NULL);
	assert(m_nBufferSize != 0);
	SingleProducerSingleConsumerQueueBase::Push((void*)&rObj, m_nProducerIndex, m_nComsumerIndex, m_nBufferSize, m_arrBuffer, sizeof(T));
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void SingleProducerSingleConsumerQueue<T >::Pop(T* pResult)
{
	assert(m_arrBuffer != NULL);
	assert(m_nBufferSize != 0);
	SingleProducerSingleConsumerQueueBase::Pop(pResult, m_nProducerIndex, m_nComsumerIndex, m_nBufferSize, m_arrBuffer, sizeof(T));
}

//! Producer/Consumer Queue for N to 1 thread communication.
//! Lockfree implemenation, to copy with multiple producers,
//! a internal producer refcount is managed, the queue is empty
//! as soon as there are no more producers and no new elements.
template<typename T>
class CRY_ALIGN(128) N_ProducerSingleConsumerQueue: public CryMT::detail::N_ProducerSingleConsumerQueueBase
{
public:
	N_ProducerSingleConsumerQueue();
	~N_ProducerSingleConsumerQueue();

	void Init(size_t nSize);

	void Push(const T &rObj);
	bool Pop(T * pResult);

	//! Needs to be called before using, assumes that there is at least one producer
	//! so the first one doesn't need to call AddProducer, but he has to deregister itself.
	void SetRunningState();

	//! To correctly track when the queue is empty(and no new jobs will be added), refcount the producer.
	void AddProducer();
	void RemoveProducer();

private:
	T* m_arrBuffer;
	volatile uint32* m_arrStates;
	uint32 m_nBufferSize;

	volatile uint32 m_nProducerIndex;
	volatile uint32 m_nComsumerIndex;
	volatile uint32 m_nRunning;
	volatile uint32 m_nProducerCount;
};

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline N_ProducerSingleConsumerQueue<T>::N_ProducerSingleConsumerQueue() : m_arrBuffer(NULL), m_arrStates(NULL), m_nBufferSize(0), m_nProducerIndex(0), m_nComsumerIndex(0), m_nRunning(0), m_nProducerCount(0)
{
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline N_ProducerSingleConsumerQueue<T>::~N_ProducerSingleConsumerQueue()
{
	CryModuleMemalignFree(m_arrBuffer);
	CryModuleMemalignFree((void*)m_arrStates);
	m_nBufferSize = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void N_ProducerSingleConsumerQueue<T >::Init(size_t nSize)
{
	assert(m_arrBuffer == NULL);
	assert(m_arrStates == NULL);
	assert(m_nBufferSize == 0);
	assert((nSize & (nSize - 1)) == 0);

	m_arrBuffer = alias_cast<T*>(CryModuleMemalign(nSize * sizeof(T), 16));
	m_arrStates = alias_cast<uint32*>(CryModuleMemalign(nSize * sizeof(uint32), 16));
	memset((void*)m_arrStates, 0, sizeof(uint32) * nSize);
	m_nBufferSize = nSize;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void N_ProducerSingleConsumerQueue<T >::SetRunningState()
{
#if !defined(_RELEASE)
	if (m_nRunning == 1)
		__debugbreak();
#endif
	m_nRunning = 1;
	m_nProducerCount = 1;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void N_ProducerSingleConsumerQueue<T >::AddProducer()
{
	assert(m_arrBuffer != NULL);
	assert(m_arrStates != NULL);
	assert(m_nBufferSize != 0);
#if !defined(_RELEASE)
	if (m_nRunning == 0)
		__debugbreak();
#endif
	CryInterlockedIncrement((volatile int*)&m_nProducerCount);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void N_ProducerSingleConsumerQueue<T >::RemoveProducer()
{
	assert(m_arrBuffer != NULL);
	assert(m_arrStates != NULL);
	assert(m_nBufferSize != 0);
#if !defined(_RELEASE)
	if (m_nRunning == 0)
		__debugbreak();
#endif
	if (CryInterlockedDecrement((volatile int*)&m_nProducerCount) == 0)
		m_nRunning = 0;
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void N_ProducerSingleConsumerQueue<T >::Push(const T& rObj)
{
	assert(m_arrBuffer != NULL);
	assert(m_arrStates != NULL);
	assert(m_nBufferSize != 0);
	CryMT::detail::N_ProducerSingleConsumerQueueBase::Push((void*)&rObj, m_nProducerIndex, m_nComsumerIndex, m_nRunning, m_arrBuffer, m_nBufferSize, sizeof(T), m_arrStates);
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
inline bool N_ProducerSingleConsumerQueue<T >::Pop(T* pResult)
{
	assert(m_arrBuffer != NULL);
	assert(m_arrStates != NULL);
	assert(m_nBufferSize != 0);
	return CryMT::detail::N_ProducerSingleConsumerQueueBase::Pop(pResult, m_nProducerIndex, m_nComsumerIndex, m_nRunning, m_arrBuffer, m_nBufferSize, sizeof(T), m_arrStates);
}
};

namespace stl
{
template<typename T> void free_container(CryMT::vector<T>& v)
{
	v.free_memory();
}
template<typename T> void free_container(CryMT::queue<T>& v)
{
	v.free_memory();
}
}

#endif // __MultiThread_Containters_h__
