// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryThread_posix.h"

//////////////////////////////////////////////////////////////////////////
// CryEvent(Timed) implementation
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CryEventTimed::Reset()
{
	m_lockNotify.Lock();
	m_flag = false;
	m_lockNotify.Unlock();
}

//////////////////////////////////////////////////////////////////////////
void CryEventTimed::Set()
{
	m_lockNotify.Lock();
	m_flag = true;
	m_cond.Notify();
	m_lockNotify.Unlock();
}

//////////////////////////////////////////////////////////////////////////
void CryEventTimed::Wait()
{
	m_lockNotify.Lock();
	if (!m_flag)
		m_cond.Wait(m_lockNotify);
	m_flag = false;
	m_lockNotify.Unlock();
}

//////////////////////////////////////////////////////////////////////////
bool CryEventTimed::Wait(const uint32 timeoutMillis)
{
	bool bResult = true;
	m_lockNotify.Lock();
	if (!m_flag)
		bResult = m_cond.TimedWait(m_lockNotify, timeoutMillis);
	m_flag = false;
	m_lockNotify.Unlock();
	return bResult;
}

///////////////////////////////////////////////////////////////////////////////
// CryCriticalSection implementation
///////////////////////////////////////////////////////////////////////////////
typedef CryLockT<CRYLOCK_RECURSIVE> TCritSecType;

void CryDeleteCriticalSection(void* cs)
{
	delete ((TCritSecType*)cs);
}

void CryEnterCriticalSection(void* cs)
{
	((TCritSecType*)cs)->Lock();
}

bool CryTryCriticalSection(void* cs)
{
	return false;
}

void CryLeaveCriticalSection(void* cs)
{
	((TCritSecType*)cs)->Unlock();
}

void CryCreateCriticalSectionInplace(void* pCS)
{
	new(pCS) TCritSecType;
}

void CryDeleteCriticalSectionInplace(void*)
{
}

void* CryCreateCriticalSection()
{
	return (void*) new TCritSecType;
}
