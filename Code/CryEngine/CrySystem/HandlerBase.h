// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HANDLERBASE_H__
#define __HANDLERBASE_H__

#if _MSC_VER > 1000
	#pragma once
#endif

const int MAX_CLIENTS_NUM = 100;

struct HandlerBase
{
	HandlerBase(const char* bucket, int affinity);
	~HandlerBase();

	void   SetAffinity();
	uint32 SyncSetAffinity(uint32 cpuMask);

	string m_serverLockName;
	string m_clientLockName;
	uint32 m_affinity;
	uint32 m_prevAffinity;
};

#endif // __HANDLERBASE_H__
