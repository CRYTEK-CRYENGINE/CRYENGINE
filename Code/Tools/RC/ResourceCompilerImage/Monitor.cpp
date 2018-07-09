// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdafx.h>
#include "Monitor.h"

Monitor::Monitor()
{
	InitializeCriticalSection(&this->criticalSection);
}

Monitor::~Monitor()
{
	DeleteCriticalSection(&this->criticalSection);
}

Monitor::LockHandleInitializer Monitor::Lock()
{
	LockHandleInitializer lock;

	// This code checks to see whether this is null - this is because in one application
	// (at the time of writing the only application) of this class was in a window proc
	// that used GetWindowLong to retrieve the instance pointer, which was sometimes
	// uninitialized. It just is easier to handle this situation here.
	if (this != 0)
		lock.pCriticalSection = &this->criticalSection;
	return lock;
}

Monitor::LockHandleInitializer::LockHandleInitializer()
:	pCriticalSection(0)
{
}

Monitor::LockHandleInitializer::LockHandleInitializer(const LockHandleInitializer& other)
{
	this->pCriticalSection = other.pCriticalSection;
}

Monitor::LockHandle::LockHandle(const LockHandleInitializer& init)
{
	this->pCriticalSection = init.pCriticalSection;
	if (this->pCriticalSection)
		EnterCriticalSection(this->pCriticalSection);
}

Monitor::LockHandle::~LockHandle()
{
	if (this->pCriticalSection != 0)
		LeaveCriticalSection(this->pCriticalSection);
}
