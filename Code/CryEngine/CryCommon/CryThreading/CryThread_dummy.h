// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
CryEvent::CryEvent() {}
CryEvent::~CryEvent() {}
void CryEvent::Reset()                                {}
void CryEvent::Set()                                  {}
void CryEvent::Wait() const                           {}
bool CryEvent::Wait(const uint32 timeoutMillis) const {}
typedef CryEvent CryEventTimed;

//////////////////////////////////////////////////////////////////////////
class _DummyLock
{
public:
	_DummyLock();

	void Lock();
	bool TryLock();
	void Unlock();

#ifndef _RELEASE
	bool IsLocked();
#endif
};

template<>
class CryLock<CRYLOCK_FAST>
	: public _DummyLock
{
	CryLock(const CryLock<CRYLOCK_FAST>&);
	void operator=(const CryLock<CRYLOCK_FAST>&);

public:
	CryLock();
};

template<>
class CryLock<CRYLOCK_RECURSIVE>
	: public _DummyLock
{
	CryLock(const CryLock<CRYLOCK_RECURSIVE>&);
	void operator=(const CryLock<CRYLOCK_RECURSIVE>&);

public:
	CryLock();
};

template<>
class CryCondLock<CRYLOCK_FAST>
	: public CryLock<CRYLOCK_FAST>
{
};

template<>
class CryCondLock<CRYLOCK_RECURSIVE>
	: public CryLock<CRYLOCK_FAST>
{
};

template<>
class CryCond<CryLock<CRYLOCK_FAST>>
{
	typedef CryLock<CRYLOCK_FAST> LockT;
	CryCond(const CryCond<LockT>&);
	void operator=(const CryCond<LockT>&);

public:
	CryCond();

	void Notify();
	void NotifySingle();
	void Wait(LockT&);
	bool TimedWait(LockT &, uint32);
};

template<>
class CryCond<CryLock<CRYLOCK_RECURSIVE>>
{
	typedef CryLock<CRYLOCK_RECURSIVE> LockT;
	CryCond(const CryCond<LockT>&);
	void operator=(const CryCond<LockT>&);

public:
	CryCond();

	void Notify();
	void NotifySingle();
	void Wait(LockT&);
	bool TimedWait(LockT &, uint32);
};

class _DummyRWLock
{
public:
	_DummyRWLock() {}

	void RLock();
	bool TryRLock();
	void WLock();
	bool TryWLock();
	void Lock()    { WLock(); }
	bool TryLock() { return TryWLock(); }
	void Unlock();
};
