// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleCommon.h"

namespace pfx2
{

class CParticleProfiler;
class CParticleComponentRuntime;

enum EProfileStat
{
	EPS_Jobs,
	EPS_AllocatedParticles,
	EPS_ActiveParticles,
	EPS_RendereredParticles,
	EPS_NewBornTime,
	EPS_UpdateTime,
	EPS_ComputeVerticesTime,
	EPS_TotalTiming,

	EPST_Count,
};

class CTimeProfiler
{
public:
	CTimeProfiler(CParticleProfiler& profiler, const CParticleComponentRuntime& runtime, EProfileStat stat);
	~CTimeProfiler();

private:
	CParticleProfiler&               m_profiler;
	const CParticleComponentRuntime& m_runtime;
	int64                            m_startTicks;
	EProfileStat                     m_stat;
};

struct SStatistics
{
	SStatistics();
	uint m_values[EPST_Count];
};

class CParticleProfiler
{
public:
	class CCSVFileOutput;
	class CStatisticsDisplay;

	struct SEntry
	{
		const CParticleComponentRuntime* m_pRuntime;
		EProfileStat                     m_type;
		uint                             m_value;
	};
	typedef std::vector<SEntry> TEntries;

public:
	CParticleProfiler();

	bool IsEnabled() const { return Cry3DEngineBase::GetCVars()->e_ParticlesProfiler != 0; }
	void Reset();
	void Display();
	void SaveToFile();

	void AddEntry(const CParticleComponentRuntime& runtime, EProfileStat type, uint value = 1);

private:
	static CVars* GetCVars() { return Cry3DEngineBase::GetCVars(); }
	void SortEntries();
	void WriteEntries(CCSVFileOutput& output) const;

	void DrawPerfomanceStats();
	void DrawStatsCounts(CStatisticsDisplay& output, Vec2 pos, uint budget);
	void DrawStats(CStatisticsDisplay& output, Vec2 pos, EProfileStat stat, uint budget, cstr statName);
	void DrawMemoryStats();

	std::vector<TEntries> m_entries;
};

}

#include "ParticleProfilerImpl.h"
