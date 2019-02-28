// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <chrono> // steady_clock
#include <atomic> 

//! A RAII-style timer.
template<size_t id>
class CScopeTimer
{
public:
	CScopeTimer()
	{
		m_start = clock::now();
	}

	~CScopeTimer()
	{
		GetRecord().Accumulate(GetLifetime());
	}

	//! Returns the total time for all instances of specific 'id'.
	float static Total()
	{
		return std::chrono::duration_cast<std::chrono::duration<float>>(GetRecord().GetDuration()).count();
	}

private:
	typedef std::chrono::steady_clock clock;
	typedef clock::duration duration;
	typedef clock::time_point time_point;

private:
	time_point m_start;

	duration GetLifetime()
	{
		time_point end = clock::now();
		return end - m_start;
	}

	class CRecord
	{
	public:
		CRecord() : m_ticks(0) {}

		void Accumulate(duration d)
		{
			std::atomic_fetch_add(&m_ticks, d.count());
		}

		duration GetDuration()
		{
			return duration(std::atomic_load(&m_ticks));
		}

		std::atomic<duration::rep> m_ticks;
	};

	static CRecord& GetRecord()
	{
		static CRecord g_storage;
		return g_storage;
	}
};
