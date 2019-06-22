// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<typename TValue>
class CSamplerStats
{
public:

	//! Add a new sample to history.
	template<class UpdateTraits>
	void CommitSample(float blendCur)
	{
		m_latest = UpdateTraits::ToFloat(m_accumulator);
		Blend(m_average, m_latest, blendCur);
		Blend(m_deviationSqr, sqr(m_latest - m_average), blendCur);
		Blend(m_minDecay, m_latest, blendCur);
		if (m_latest <= m_minDecay)
		{
			m_minDecay = m_latest;
			m_min = m_latest;
		}
		Blend(m_maxDecay, m_latest, blendCur);
		if (m_latest >= m_maxDecay)
		{
			m_maxDecay = m_latest;
			m_max = m_latest;
		}

		m_accumulator = 0;
	}
	//! Cleans up the data history.
	void Clear() { *this = {}; }

	operator TValue() const { return m_accumulator; }
	TValue& operator=(TValue val) { return m_accumulator = val; }
	TValue& operator+=(TValue val) { return m_accumulator += val; }
	void operator++() { ++m_accumulator; }
	void operator++(int) { m_accumulator++; }

	TValue CurrentRaw() const { return m_accumulator; }
	float Latest() const { return m_latest; }
	float Average() const { return m_average; }
	float Variance() const { return sqrt(m_deviationSqr); }
	float Max() const { return m_max; }
	float Min() const { return m_min; }

	TValue m_accumulator = 0;
protected:
	float m_latest = 0,
		m_average = 0,
		m_deviationSqr = 0,
		m_minDecay = 0,
		m_maxDecay = 0,
		m_min = 0,
		m_max = 0;

	static void Blend(float& stat, float cur, float blendCur)
	{
		stat += (cur - stat) * blendCur;
	}
};
