// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __OBJCNT_H__
#define __OBJCNT_H__

#pragma once

#define ENABLE_OBJECT_COUNTING 0

#if !ENABLE_OBJECT_COUNTING
struct SCounter
{
	ILINE void operator++() {}
	ILINE void operator--() {}
	ILINE int  QuickPeek()  { return 0; }
};
#else
struct SCounter
{
public:
	SCounter() : m_cnt(0) {}

	void operator++()
	{
		CryInterlockedIncrement(&m_cnt);
	}

	void operator--()
	{
		CryInterlockedDecrement(&m_cnt);
	}

	int QuickPeek()
	{
		return m_cnt;
	}

private:
	volatile int m_cnt;
};
#endif

struct SObjectCounters
{
#define COUNTER(name) SCounter name
#include "objcnt_defs.h"
#undef COUNTER
};

extern SObjectCounters g_objcnt;
extern SObjectCounters* g_pObjcnt;

#endif
