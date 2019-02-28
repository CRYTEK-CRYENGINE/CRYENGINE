// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HISTOGRAM_H__
#define __HISTOGRAM_H__

#pragma once

#include <CryCore/Platform/platform.h>    // uint64


template <size_t BIN_COUNT>
class Histogram
{
public:
	struct Bins
	{
		uint64 b[BIN_COUNT];
	};

public:
	Histogram()
		: m_meanBin(0.0f)
	{
	}

	static void clearBins(Bins& bins)
	{
		memset(&bins.b[0], 0, sizeof(bins.b));
	}

	void set(const Bins& bins)
	{
		m_bins.b[0] = bins.b[0];
		m_binsCumulative.b[0] = bins.b[0];
		double sum = 0.0f;
		for (size_t i = 1; i < BIN_COUNT; ++i)
		{
			m_bins.b[i] = bins.b[i];
			m_binsCumulative.b[i] = m_binsCumulative.b[i - 1] + bins.b[i];
			sum += i * double(bins.b[i]);
		}

		const uint64 totalCount = getTotalSampleCount();
		m_meanBin = (totalCount <= 0) ? 0.0f : float(sum / totalCount);
	}

	uint64 getTotalSampleCount() const
	{
		return m_binsCumulative.b[BIN_COUNT - 1];
	}

	float getPercentage(size_t minBin, size_t maxBin) const
	{
		const uint64 totalCount = getTotalSampleCount();

		if ((totalCount <= 0) || (minBin > maxBin) || (maxBin < 0) || (minBin >= BIN_COUNT))
		{
			return 0.0f;
		}

		Util::clampMin(minBin, size_t(0));
		Util::clampMax(maxBin, BIN_COUNT);

		const uint64 count = m_binsCumulative.b[maxBin] - ((minBin <= 0) ? 0 : m_binsCumulative.b[minBin - 1]);

		return float((double(count) * 100.0) / double(totalCount));
	}

	float getMeanBin() const
	{
		return m_meanBin;
	}

private:
	Bins m_bins;
	Bins m_binsCumulative;
	float m_meanBin;
};


#endif
