// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETDEBUGINFO_H__
#define __NETDEBUGINFO_H__

#pragma once

#include "Config.h"

#if ENABLE_NET_DEBUG_INFO

	#include "Network.h"

class CNetDebugInfoSection
{
public:
	CNetDebugInfoSection(uint32 page, float xpos, float ypos);
	virtual ~CNetDebugInfoSection();

	virtual void Tick(const SNetworkProfilingStats* const pProfilingStats) {};
	virtual void Draw(const SNetworkProfilingStats* const pProfilingStats) {};

	float        GetHeight(void) const                                     { return m_sectionHeight; }
	uint32       GetPage() const                                           { return m_page; }

protected:

	template<uint32 SIZE> struct SAccumulator
	{
		SAccumulator(CTimeValue period)
			: m_total(0),
			m_min(UINT_MAX),
			m_minIdx(-1),
			m_max(0),
			m_maxIdx(-1),
			m_average(0.0F),
			m_count(0),
			m_head(0),
			m_nextAccept(g_time),
			m_nextAcceptIncrement(period.GetSeconds() / SIZE)
		{
			memset(m_buffer, 0, sizeof m_buffer);
		}

		~SAccumulator()
		{
		}

		void Add(uint32 value)
		{
			bool acceptTime = (g_time >= m_nextAccept);
			bool acceptMin = false;
			bool acceptMax = false;

			if (value <= m_min)
			{
				acceptMin = true;
				m_min = value;
				m_minIdx = m_head;
			}

			if (value >= m_max)
			{
				acceptMax = true;
				m_max = value;
				m_maxIdx = m_head;
			}

			if (acceptTime || acceptMin || acceptMax)
			{
				m_total -= m_buffer[m_head];
				m_buffer[m_head] = value;
				m_total += value;

				if (m_count < SIZE)
				{
					++m_count;
				}

				if (!acceptMin && (m_minIdx == m_head))
				{
					UpdateLimit(m_min, m_minIdx, std::numeric_limits<uint32>::max(), std::less<uint32>());
				}

				if (!acceptMax && (m_maxIdx == m_head))
				{
					UpdateLimit(m_max, m_maxIdx, std::numeric_limits<uint32>::min(), std::greater<uint32>());
				}

				m_nextAccept += m_nextAcceptIncrement;
				m_head = (m_head + 1) % SIZE;
				m_average = static_cast<float>(m_total) / static_cast<float>(m_count);
			}
		}

		template<typename TCmp> void UpdateLimit(uint32& val, int32& idx, uint32 start, const TCmp& cmp)
		{
			uint32 newVal = start;
			uint32 i = (m_head + SIZE - 1) % SIZE;
			uint32 last = (m_head + SIZE + 1 - m_count) % SIZE;

			do
			{
				assert(i < SIZE);
				if (cmp(m_buffer[i], newVal))
				{
					newVal = m_buffer[i];
					idx = i;
				}

				i = (i + SIZE - 1) % SIZE;
			}
			while (i != last);

			val = newVal;
		}

		uint32     m_buffer[SIZE];
		uint32     m_total;
		uint32     m_min;
		int32      m_minIdx;
		uint32     m_max;
		int32      m_maxIdx;
		float      m_average;
		uint32     m_count;
		uint32     m_head;
		CTimeValue m_nextAccept;
		CTimeValue m_nextAcceptIncrement;
	};

	enum EBufferSize
	{
		eBS_Default = 128,
		eBS_Lag     = 32
	};

	float              m_xpos;
	float              m_ypos;
	float              m_fontSize;
	float              m_lineHeight;
	float              m_sectionHeight;
	uint32             m_page;

	static const float s_white[4];
	static const float s_lgrey[4];
	static const float s_yellow[4];
	static const float s_green[4];
	static const float s_red[4];

	void DrawHeader(const char* pHeaderStr);
	void DrawLine(uint32 line, const float* pColor, const char* pFormat, ...);
	void VDrawLine(uint32 line, const float* pColor, const char* pFormat, va_list args);
	void Draw3dLine(const Vec3 pos, uint32 line, const float* pColor, const char* pFormat, ...);
};

class CNetDebugObjectsInfo : public CNetDebugInfoSection
{
public:
	CNetDebugObjectsInfo();
	virtual ~CNetDebugObjectsInfo();

	virtual void Tick(const SNetworkProfilingStats* const pProfilingStats);
	virtual void Draw(const SNetworkProfilingStats* const pProfilingStats);

private:

	SAccumulator<eBS_Default> m_boundObjects;
	SAccumulator<eBS_Default> m_sendTraffic;
	SAccumulator<eBS_Default> m_sendPackets;
	SAccumulator<eBS_Default> m_recvTraffic;
	SAccumulator<eBS_Default> m_recvPackets;

	void DrawObjects(uint32 line, const char* pLabel, uint32 count, uint32 minVal, float average, uint32 maxVal);
};

class CNetDebugInfo
{
public:

	enum EPage
	{
		eP_None,
		eP_Test,
		eP_ProfileViewLastSecond,
		eP_ProfileViewLast5Seconds,
		eP_ProfileViewOnEntities,
		eP_ServerInfo,
		eP_NetChannelOverview,
		eP_NetChannelAccountingGroupsOverview,
		eP_NetChannelAccountingGroupsSends
	};

	CNetDebugInfo();
	~CNetDebugInfo();

	static CNetDebugInfo* Get();
	void                  Tick();

private:

	static CNetDebugInfo*              s_pInstance;

	std::vector<CNetDebugInfoSection*> m_sections;
};

#endif // ENABLE_NET_DEBUG_INFO

#endif
