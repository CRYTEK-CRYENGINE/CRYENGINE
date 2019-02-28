// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CryGUIDHelper.h"
#include <CryMath/MTPseudoRandom.h>
#include "ThreadUtils.h"

namespace CryGUIDHelper
{

// To prevent collisions the implementation is a clone of the CryGUID::Create() method from Code/CryEngine/CryCommon/CryExtension/CryGUID.h.
// The CryGUID::Create() can't be used in tools since it refers to engine's gEnv->pSystem.
CryGUID Create()
{
	struct TicksTime
	{
		int64  ticks;
		time_t tm;
	};

	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	const TicksTime tt = { li.QuadPart, time(nullptr) };

	CryGUID guid;

	uint32* pWords = reinterpret_cast<uint32*>(&guid);
	const size_t numWords = sizeof(guid) / sizeof(uint32);

	static ThreadUtils::CriticalSection cs;
	{
		ThreadUtils::AutoLock lock(cs);

		static CMTRand_int32 state(reinterpret_cast<const uint32*>(&tt), sizeof(tt) / sizeof(uint32));

		for (uint32 i = 0; i < numWords; ++i)
		{
			pWords[i] = state.GenerateUint32();
		}
	}

	return guid;
}

}
