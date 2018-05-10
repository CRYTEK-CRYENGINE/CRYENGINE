// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class Monitor
{
public:
	class LockHandleInitializer
	{
		friend class Monitor;
		friend class LockHandle;
	public:
		CRITICAL_SECTION* pCriticalSection;
	private:
		LockHandleInitializer();
		LockHandleInitializer(const LockHandleInitializer&);
		LockHandleInitializer& operator=(const LockHandleInitializer&);
	};

public:
	class LockHandle
	{
	public:
		~LockHandle();
		explicit LockHandle(const LockHandleInitializer& init);

	private:
		LockHandle();
		LockHandle(const LockHandle&);
		LockHandle& operator=(const LockHandle&);

		CRITICAL_SECTION* pCriticalSection;
	};

	Monitor();
	virtual ~Monitor();

protected:
	LockHandleInitializer Lock(); 

private:
	CRITICAL_SECTION criticalSection;
};

#define LOCK_MONITOR() LockHandle lh(this->Lock())
