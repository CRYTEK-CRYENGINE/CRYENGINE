// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __THREADPROFILERSUPPORT_H__
#define __THREADPROFILERSUPPORT_H__

#pragma once

#define ENABLE_THREADPROFILERSUPPORT 0

#if ENABLE_THREADPROFILERSUPPORT

	#include "libittnotify.h"

	#pragma comment( lib, "libittnotify" )

class CThreadProfilerEvent
{
public:
	CThreadProfilerEvent(const char* name) : m_event(__itt_event_create(const_cast<char*>(name), strlen(name))) {}

	class Instance
	{
	public:
		ILINE Instance(const CThreadProfilerEvent& evt, bool enter = true) : m_event(evt.m_event), m_enter(enter)
		{
			if (m_enter)
				__itt_event_start(m_event);
		}
		ILINE ~Instance()
		{
			if (m_enter)
				__itt_event_end(m_event);
		}

	private:
		Instance(const Instance&);
		Instance& operator=(const Instance&);

		__itt_event m_event;
		bool        m_enter;
	};

private:
	__itt_event m_event;
};

#else // ENABLE_THREADPROFILERSUPPORT

class CThreadProfilerEvent
{
public:
	ILINE CThreadProfilerEvent(const char*) {}
	class Instance
	{
	public:
		ILINE Instance(const CThreadProfilerEvent&, bool enter = true) {}

	private:
		Instance(const Instance&);
		Instance& operator=(const Instance&);
	};
};

#endif // ENABLE_THREADPROFILERSUPPORT

#endif
