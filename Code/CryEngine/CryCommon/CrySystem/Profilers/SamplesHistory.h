// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! CSamplesHistory provides information on history of sample values for profiler counters.
template<class T, int TCount>
class CSamplesHistory
{
public:
	CSamplesHistory() : m_nHistoryNext(0), m_nHistoryCount(0) {}

	//! Add a new sample to history.
	void Add(T sample)
	{
		m_history[m_nHistoryNext] = sample;
		m_nHistoryNext = (m_nHistoryNext + TCount - 1) % TCount;
		if (m_nHistoryCount < TCount)
			++m_nHistoryCount;
	}
	
	//! Cleans up the data history.
	void Clear()
	{
		m_nHistoryNext = 0;
		m_nHistoryCount = 0;
	}

	//! Get last sample value.
	T GetLast() const
	{
		if (m_nHistoryCount)
			return m_history[(m_nHistoryNext + 1) % TCount];
		else
			return 0;
	}
	
	//! Calculates average sample value for at most the given number of frames (less if so many unavailable).
	T GetAverage(int nCount = TCount) const
	{
		if (m_nHistoryCount)
		{
			T fSum = 0;
			if (nCount > m_nHistoryCount)
				nCount = m_nHistoryCount;
			for (int i = 1; i <= nCount; ++i)
			{
				fSum += m_history[(m_nHistoryNext + i) % (sizeof(m_history) / sizeof((m_history)[0]))];
			}
			return fSum / nCount;
		}
		else
			return 0;
	}

	//! Calculates max sample value for at most the given number of frames (less if so many unavailable).
	T GetMax(int nCount = TCount) const
	{
		if (m_nHistoryCount)
		{
			T fMax = 0;
			if (nCount > m_nHistoryCount)
				nCount = m_nHistoryCount;
			for (int i = 1; i <= nCount; ++i)
			{
				T fCur = m_history[(m_nHistoryNext + i) % (sizeof(m_history) / sizeof((m_history)[0]))];
				if (i == 1 || fCur > fMax)
					fMax = fCur;
			}
			return fMax;
		}
		else
			return 0;
	}

	//! Calculates min sample value for at most the given number of frames (less if so many unavailable).
	T GetMin(int nCount = TCount) const
	{
		if (m_nHistoryCount)
		{
			T fMin = 0;
			if (nCount > m_nHistoryCount)
				nCount = m_nHistoryCount;
			for (int i = 1; i <= nCount; ++i)
			{
				T fCur = m_history[(m_nHistoryNext + i) % (sizeof(m_history) / sizeof((m_history)[0]))];
				if (i == 1 || fCur < fMin)
					fMin = fCur;
			}
			return fMin;
		}
		else
			return 0;
	}

protected:
	//! The timer values for the last frames.
	T m_history[TCount];

	//! The current pointer in the timer history, decreases.
	int m_nHistoryNext;

	//! The number of currently collected samples in the timer history.
	int m_nHistoryCount;
};