// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   ValueHistory.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 2008				: Created by Mikko Mononen

 *********************************************************************/

#ifndef _VALUEHISTORY_H_
#define _VALUEHISTORY_H_

#if _MSC_VER > 1000
	#pragma once
#endif

// Simple class to track a history of float values. Assumes positive values.
template<typename T>
class CValueHistory
{
public:
	CValueHistory(unsigned s, float sampleIterval) : sampleIterval(sampleIterval), head(0), size(0), t(0), v(0) { data.resize(s); }

	inline void Reset()
	{
		size = 0;
		head = 0;
		v = 0;
		t = 0;
	}

	inline void Sample(T nv)
	{
		data[head] = nv;
		head = (head + 1) % data.size();
		if (size < data.size()) size++;
	}

	inline void Sample(T nv, float dt)
	{
		t += dt;
		v = max(v, nv);

		int iter = 0;
		while (t > sampleIterval && iter < 5)
		{
			data[head] = v;
			head = (head + 1) % data.size();
			if (size < data.size()) size++;
			++iter;
			t -= sampleIterval;
		}
		if (iter == 5)
			t = 0;
		v = 0;
	}

	inline unsigned GetSampleCount() const
	{
		return size;
	}

	inline unsigned GetMaxSampleCount() const
	{
		return data.size();
	}

	inline T GetSampleInterval() const
	{
		return sampleIterval;
	}

	inline T GetSample(unsigned i) const
	{
		const unsigned n = data.size();
		return data[(head + (n - 1 - i)) % n];
	}

	inline T GetMaxSampleValue() const
	{
		T maxVal = 0;
		const unsigned n = data.size();
		for (unsigned i = 0; i < size; ++i)
			maxVal = max(maxVal, data[(head + (n - 1 - i)) % n]);
		return maxVal;
	};

private:
	std::vector<T> data;
	unsigned       head, size;
	T              v;
	float          t;
	const float    sampleIterval;
};

#endif
