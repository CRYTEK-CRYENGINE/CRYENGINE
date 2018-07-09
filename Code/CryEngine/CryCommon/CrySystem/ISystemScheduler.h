// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISYSTEMSCHEDULER_H__
#define __ISYSTEMSCHEDULER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#if defined(MAP_LOADING_SLICING)

struct ISystemScheduler
{
	virtual ~ISystemScheduler(){}

	// <interfuscator:shuffle>
	//! Map load slicing functionality support.
	virtual void SliceAndSleep(const char* sliceName, int line) = 0;
	virtual void SliceLoadingBegin() = 0;
	virtual void SliceLoadingEnd() = 0;

	virtual void SchedulingSleepIfNeeded(void) = 0;
	// </interfuscator:shuffle>
};

ISystemScheduler* GetISystemScheduler(void);

class CSliceLoadingMonitor
{
public:
	CSliceLoadingMonitor()  { GetISystemScheduler()->SliceLoadingBegin(); }
	~CSliceLoadingMonitor() { GetISystemScheduler()->SliceLoadingEnd();   }
};

#endif // defined(MAP_LOADING_SLICING)

#endif // __ISYSTEMSCHEDULER_H__
