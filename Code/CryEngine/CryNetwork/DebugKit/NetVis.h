// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETVIS_H__
#define __NETVIS_H__

#pragma once

#include "Config.h"

#if ENABLE_DEBUG_KIT

	#include "Network.h"

class CClientContextView;

class CNetVis
{
public:
	CNetVis(CClientContextView* pCtxView) : m_pCtxView(pCtxView) {};
	void AddSample(SNetObjectID obj, uint16 prop, float value);
	void Update();

private:
	struct SSample
	{
		SSample(CTimeValue w, float v) : when(w), value(v) {}
		SSample(float v) : when(g_time), value(v) {}
		SSample() : when(g_time), value(0) {}
		CTimeValue when;
		float      value;
	};

	struct SStatistic
	{
		SStatistic() : total(0), average(0) {}
		std::queue<SSample> samples;
		float               total;
		float               average;
	};

	struct SIndex
	{
		SIndex() : prop(10000) {}
		SIndex(SNetObjectID o, uint16 p) : obj(o), prop(p) {}
		SNetObjectID obj;
		uint16       prop;

		bool operator<(const SIndex& rhs) const
		{
			return prop < rhs.prop || (prop == rhs.prop && obj < rhs.obj);
		}
	};

	typedef std::map<SIndex, SStatistic> TStatsMap;
	TStatsMap           m_stats;
	CClientContextView* m_pCtxView;

	struct SMinMax
	{
		SMinMax() : minVal(1e12f), maxVal(0) {}
		float minVal;
		float maxVal;
	};
	std::map<uint16, SMinMax> m_minmax;
};

#endif // ENABLE_DEBUG_KIT

#endif
