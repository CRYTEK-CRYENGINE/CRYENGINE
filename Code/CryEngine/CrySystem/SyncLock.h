// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNCLOCK_H__
#define __SYNCLOCK_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <semaphore.h>
#elif CRY_PLATFORM_WINDOWS
	#include <CryCore/Platform/CryWindows.h>
#endif

struct SSyncLock
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	typedef sem_t* HandleType;
#else
	typedef HANDLE HandleType;
#endif

	SSyncLock(const char* name, int id, bool own);
	SSyncLock(const char* name, int minId, int maxId);
	~SSyncLock();

	void Own(const char* name);
	bool Open(const char* name);
	bool Create(const char* name);
	void Signal();
	bool Wait(int ms);
	void Close();
	bool IsValid() const;

	HandleType ev;
	int        number;
	string     o_name;
};

#endif // __SYNCLOCK_H__
