// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __POLYMORPHICQUEUE_H__
#define __POLYMORPHICQUEUE_H__

#pragma once

#include "Config.h"

#include <CryMemory/STLGlobalAllocator.h>

class CEnsureRealtime
{
public:
	CEnsureRealtime() : m_begin(gEnv->pTimer->GetAsyncTime()) {}
	~CEnsureRealtime()
	{
		if ((gEnv->pTimer->GetAsyncTime() - m_begin).GetSeconds() > 1.0f)
			Failed();
	}

private:
	CTimeValue m_begin;

	void Failed();
};

#define ENSURE_REALTIME /*CEnsureRealtime ensureRealtime*/

#if CHECKING_POLYMORPHIC_QUEUE
	#define POLY_HEADER threadID oldUser = m_user; m_user = GetCurrentThreadId(); if (oldUser) \
	  NET_ASSERT(oldUser == m_user)
	#define POLY_FOOTER NET_ASSERT(m_user == GetCurrentThreadId()); m_user = oldUser
	#ifdef _MSC_VER
extern "C" void* _ReturnAddress(void);
		#pragma intrinsic(_ReturnAddress)
		#define POLY_RETADDR _ReturnAddress()
	#else
		#define POLY_RETADDR (void*)0
	#endif
#else
	#define POLY_HEADER
	#define POLY_FOOTER
	#define POLY_RETADDR (void*)0
#endif

// a 'queue' of variable sized objects that all derive from a common base class
// items can be appended, but all items must be consumed at the same time (see Flush())
// TODO: this is an awesome candidate for unit testing
// TODO: separate the memory management code into a non-templated class, as it's likely useful for other things
template<class B>
class CPolymorphicQueue
{
public:
	CPolymorphicQueue();
	~CPolymorphicQueue();

	template<class T>
	T* Add(const T& value);
	template<class T>
	T* Add();

	template<class F>
	void Flush(F& f);

	template<class F>
	void RealtimeFlush(F& f);

	void Swap(CPolymorphicQueue& pq);
	bool Empty();
	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false);

private:
	void* Grab(size_t sz, void* retaddr);
	bool  BeginFlush();
	B*    Next(B* last);
	void  EndFlush();

#if CHECKING_POLYMORPHIC_QUEUE
	threadID m_user;
#endif

	template<class T>
	ILINE T* Check1(T* p, B* pp);
	template<class T>
	ILINE T* Check(T* p);

	typedef INT_PTR TWord;
	static const size_t WORD_SIZE = sizeof(TWord);
	struct SBlock
	{
		static const size_t BLOCK_SIZE = 16 * 1024; //64*1024;
		static const size_t WORDS = BLOCK_SIZE / WORD_SIZE;
		TWord               cursor;
		TWord               data[WORDS - 1];

		SBlock()
		{
			++g_pObjcnt->queueBlocks;
		}
		~SBlock()
		{
			--g_pObjcnt->queueBlocks;
		}
		SBlock(const SBlock&);
		SBlock& operator=(const SBlock&);
	};
	std::vector<SBlock*, stl::STLGlobalAllocator<SBlock*>> m_freeBlocks;
	std::vector<SBlock*, stl::STLGlobalAllocator<SBlock*>> m_blocks;
	std::vector<SBlock*, stl::STLGlobalAllocator<SBlock*>> m_flushBlocks;
	SBlock* m_pCurBlock;
	size_t  m_flushBlock;
	TWord   m_flushCursor;
#if CHECKING_POLYMORPHIC_QUEUE
	void*   m_lastAdder;
#endif

	struct DoNothing
	{
		ILINE void operator()(B* p) {}
	};
};

template<class B>
CPolymorphicQueue<B>::CPolymorphicQueue()
{
	m_pCurBlock = 0;
	m_flushBlock = 0;
#if CHECKING_POLYMORPHIC_QUEUE
	m_user = 0;
#endif
}

template<class B>
CPolymorphicQueue<B>::~CPolymorphicQueue()
{
	DoNothing f;
	Flush(f);
	NET_ASSERT(m_blocks.empty());
	NET_ASSERT(m_flushBlocks.empty());
	while (!m_freeBlocks.empty())
	{
		delete m_freeBlocks.back();
		m_freeBlocks.pop_back();
	}
}

template<class B> template<class T>
T* CPolymorphicQueue<B >::Add(const T& value)
{
	POLY_HEADER;
	T* out = Check(new(Grab(sizeof(T), POLY_RETADDR))T(value));
	POLY_FOOTER;
	return out;
}

template<class B> template<class T>
T* CPolymorphicQueue<B >::Add()
{
	POLY_HEADER;
	T* out = Check(new(Grab(sizeof(T), POLY_RETADDR))T());
	POLY_FOOTER;
	return out;
}

template<class B> template<class F>
void CPolymorphicQueue<B >::Flush(F& f)
{
	POLY_HEADER;
	while (BeginFlush())
	{
		B* p = 0;
		while (p = Next(p))
		{
			f(p);
		}
		EndFlush();
	}
	POLY_FOOTER;
}

template<class B> template<class F>
void CPolymorphicQueue<B >::RealtimeFlush(F& f)
{
	POLY_HEADER;
	while (BeginFlush())
	{
		B* p = 0;
		while (p = Next(p))
		{
			ENSURE_REALTIME;
			f(p);
		}
		EndFlush();
	}
	POLY_FOOTER;
}

template<class B>
void CPolymorphicQueue<B >::Swap(CPolymorphicQueue& pq)
{
	POLY_HEADER;
	m_freeBlocks.swap(pq.m_freeBlocks);
	m_blocks.swap(pq.m_blocks);
	m_flushBlocks.swap(pq.m_flushBlocks);
	std::swap(m_pCurBlock, pq.m_pCurBlock);
	std::swap(m_flushBlock, pq.m_flushBlock);
	std::swap(m_flushCursor, pq.m_flushCursor);
	POLY_FOOTER;
}

template<class B>
bool CPolymorphicQueue<B >::Empty()
{
	if (m_pCurBlock)
		if (m_pCurBlock->cursor)
			return false;
	return m_blocks.empty();
}

template<class B>
void CPolymorphicQueue<B >::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis /*= false*/)
{
	SIZER_COMPONENT_NAME(pSizer, "CPolymorphicQueue");

	if (countingThis)
		pSizer->Add(*this);

	pSizer->AddContainer(m_freeBlocks);
	pSizer->AddContainer(m_blocks);
	pSizer->AddContainer(m_flushBlocks);

	pSizer->AddObject(&m_freeBlocks, m_freeBlocks.size() * sizeof(SBlock));
	pSizer->AddObject(&m_blocks, m_blocks.size() * sizeof(SBlock));
	pSizer->AddObject(&m_flushBlocks, m_flushBlocks.size() * sizeof(SBlock));
}

template<class B> template<class T>
ILINE T* CPolymorphicQueue<B >::Check1(T* p, B* pp)
{
	return p;
}

template<class B> template<class T>
ILINE T* CPolymorphicQueue<B >::Check(T* p)
{
	return Check1(p, p);
}

template<class B>
void* CPolymorphicQueue<B >::Grab(size_t sz, void* retaddr)
{
	POLY_HEADER;
	if (sz & (WORD_SIZE - 1))
		sz += WORD_SIZE;
	sz /= WORD_SIZE;
	NET_ASSERT(sz > 0);
	NET_ASSERT(sz < SBlock::BLOCK_SIZE - sizeof(TWord));
	if (m_pCurBlock && m_pCurBlock->cursor + sz + 1 + CHECKING_POLYMORPHIC_QUEUE > SBlock::WORDS - 1)
	{
		m_blocks.push_back(m_pCurBlock);
		m_pCurBlock = 0;
	}
	if (!m_pCurBlock)
	{
		if (m_freeBlocks.empty())
		{
			m_pCurBlock = new SBlock;
			NET_ASSERT(m_pCurBlock);
		}
		else
		{
			m_pCurBlock = m_freeBlocks.back();
			m_freeBlocks.pop_back();
			NET_ASSERT(m_pCurBlock);
		}
		m_pCurBlock->cursor = 0;
	}
	NET_ASSERT(m_pCurBlock);
	NET_ASSERT(m_pCurBlock->cursor + sz + 1 + CHECKING_POLYMORPHIC_QUEUE <= SBlock::WORDS - 1);
	NET_ASSERT(m_pCurBlock->cursor >= 0);
	m_pCurBlock->data[m_pCurBlock->cursor++] = sz;
#if CHECKING_POLYMORPHIC_QUEUE
	m_pCurBlock->data[m_pCurBlock->cursor++] = (TWord) retaddr;
#endif
	TWord* out = m_pCurBlock->data + m_pCurBlock->cursor;
	NET_ASSERT(out >= m_pCurBlock->data);
	//	NET_ASSERT(out < m_pCurBlock->data + SBlock::WORDS);
	m_pCurBlock->cursor += sz;
	NET_ASSERT(m_pCurBlock->cursor <= SBlock::WORDS - 1);
	NET_ASSERT(m_pCurBlock->cursor > 0);
	POLY_FOOTER;
	return out;
}

template<class B>
bool CPolymorphicQueue<B >::BeginFlush()
{
	POLY_HEADER;
	if (m_pCurBlock)
	{
		m_blocks.push_back(m_pCurBlock);
		m_pCurBlock = 0;
	}
	if (m_blocks.empty())
	{
		POLY_FOOTER;
		return false;
	}
	NET_ASSERT(m_flushBlocks.empty());
	m_flushBlocks.swap(m_blocks);
	m_flushBlock = 0;
	m_flushCursor = 0;
	POLY_FOOTER;
	return true;
}

template<class B>
B* CPolymorphicQueue<B >::Next(B* last)
{
	POLY_HEADER;
	if (last)
		last->~B();
	if (m_flushBlock == m_flushBlocks.size())
	{
		POLY_FOOTER;
		return 0;
	}
	while (m_flushCursor == m_flushBlocks[m_flushBlock]->cursor)
	{
		m_flushBlock++;
		m_flushCursor = 0;
		if (m_flushBlock == m_flushBlocks.size())
		{
			POLY_FOOTER;
			return 0;
		}
	}
	size_t sz = m_flushBlocks[m_flushBlock]->data[m_flushCursor++];
#if CHECKING_POLYMORPHIC_QUEUE
	m_lastAdder = (void*) m_flushBlocks[m_flushBlock]->data[m_flushCursor++];
#endif
	void* out = &(m_flushBlocks[m_flushBlock]->data[m_flushCursor]);
	m_flushCursor += sz;
	POLY_FOOTER;
	return (B*)out;
}

template<class B>
void CPolymorphicQueue<B >::EndFlush()
{
	POLY_HEADER;
	NET_ASSERT(m_flushBlock == m_flushBlocks.size());
	while (!m_flushBlocks.empty())
	{
		m_freeBlocks.push_back(m_flushBlocks.back());
		m_flushBlocks.pop_back();
	}
	POLY_FOOTER;
}

#endif
