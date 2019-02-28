// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include "ParticleProfiler.h"
#include "ParticleComponentRuntime.h"
#include "ParticleEmitter.h"
#include "ParticleSystem.h"

CRY_PFX2_DBG

namespace pfx2
{

const cstr gDefaultEntityName = "- no entity -";
const Vec2 gDisplayPosition = Vec2(0.05f, 0.05f);
const Vec2 gDisplayBarSize = Vec2(0.45f, 0.025f);
const uint gDisplayMaxNumComponents = 8;
const float gDisplayFontSize = 1.2f;
const float gDisplayLineGap = 0.0175f;
const ColorB gBudgetOk = ColorB(16, 92, 198);
const ColorB gBudgetOver = ColorB(255, 0, 0);
const float gBudgetBarSize = 0.015f;
const float gBudgetLineWidth = 1.5f;

struct SStatDisplay
{
	EProfileStat m_stat;
	const char*  m_statName;
	uint         m_budget;
};

const SStatDisplay statisticsOutput[] =
{
	{ EPS_Jobs,                "Jobs",                  0 },
	{ EPS_RendereredParticles, "Rendered",              0 },
	{ EPS_ActiveParticles,     "Active",                0 },
	{ EPS_AllocatedParticles,  "Alloc",                 0 },
	{ EPS_NewBornTime,         "New Borns (ns)",        0 },
	{ EPS_UpdateTime,          "Update (ns)",           0 },
	{ EPS_ComputeVerticesTime, "Compute Vertices (ns)", 0 },
	{ EPS_TotalTiming,         "Total Timing (ns)",     0 },
};

class CParticleProfiler::CCSVFileOutput
{
public:
	CCSVFileOutput(cstr fileName)
	{
		m_file = fxopen(fileName, "w");
	}
	~CCSVFileOutput()
	{
		fclose(m_file);
	}

	operator bool() const { return m_file != nullptr; }

	void WriteHeader()
	{
		Column("Effect");
		Column("Component");
		Column("Entity");
		for (const auto& stat : statisticsOutput)
			Column(stat.m_statName);
		NewLine();
	}

	void WriteStatistics(const CParticleComponentRuntime& runtime, const SStatistics& statistics)
	{
		CParticleComponent* pComponent = runtime.GetComponent();
		CParticleEmitter* pEmitter = runtime.GetEmitter();
		CParticleEffect* pEffect = pEmitter->GetCEffect();
		IEntity* pEntity = pEmitter->GetOwnerEntity();
		cstr effectName = pEffect->GetFullName();
		cstr componentName = pComponent->GetName();
		cstr entityName = pEntity ? pEntity->GetName() : gDefaultEntityName;

		Column(string().Format("%s%s%s",
			pEmitter->IsActive() ? "#Active " : "#Inactive ",
			pEmitter->IsAlive() ? "#E:Alive " : "#E:Dead ",
			effectName));
		Column(string().Format("%s%s%s%s", 
			pComponent->GetParentComponent() ? "#Child " : "",
			pComponent->ComponentParams().IsImmortal() ? "#Immortal " : "",
			runtime.IsAlive() ? "#C:Alive " : "#C:Dead ",
			componentName));
		Column(string().Format("%s%s", entityName, 
			pEmitter->IsIndependent() ? " #Independent" : ""
		));
		for (const auto& stat : statisticsOutput)
			Column(string().Format("%d", statistics.m_values[stat.m_stat]));
		NewLine();
	}

private:
	void Column(cstr text)
	{
		fprintf(m_file, "%s, ", text);
	}
	void NewLine()
	{
		fprintf(m_file, "\n");
	}

	FILE* m_file;
};

class CParticleProfiler::CStatisticsDisplay
{
public:
	CStatisticsDisplay()
	{
		m_pRender = gEnv->pRenderer;
		m_pRenderAux = m_pRender->GetIRenderAuxGeom();
		m_prevFlags = m_pRenderAux->GetRenderFlags();
		m_screenSize = Vec2(float(m_pRenderAux->GetCamera().GetViewSurfaceX()), float(m_pRenderAux->GetCamera().GetViewSurfaceZ()));
		SAuxGeomRenderFlags curFlags = m_prevFlags;
		curFlags.SetMode2D3DFlag(e_Mode2D);
		curFlags.SetDepthTestFlag(e_DepthTestOff);
		curFlags.SetDepthWriteFlag(e_DepthWriteOff);
		m_pRenderAux->SetRenderFlags(curFlags);
	}
	~CStatisticsDisplay()
	{
		m_pRenderAux->SetRenderFlags(m_prevFlags);
	}

	void DrawBudgetBar(Vec2 pos, uint maxValue, uint budget)
	{
		AABB box;
		box.min.x = pos.x;
		box.min.y = pos.y;
		box.max.x = pos.x + gDisplayBarSize.x;
		box.max.y = pos.y + gDisplayBarSize.y;
		box.min.z = box.max.z = 0.0f;

		const Vec3 boxVertices[] =
		{
			Vec3(box.min.x, box.min.y, 0.0f), Vec3(box.max.x, box.min.y, 0.0f),
			Vec3(box.max.x, box.min.y, 0.0f), Vec3(box.max.x, box.max.y, 0.0f),
			Vec3(box.max.x, box.max.y, 0.0f), Vec3(box.min.x, box.max.y, 0.0f),
			Vec3(box.min.x, box.max.y, 0.0f), Vec3(box.min.x, box.min.y, 0.0f),
		};
		m_pRenderAux->DrawLines(boxVertices, 8, ColorB(0, 0, 0));

		const ColorB color = (maxValue <= budget) ? gBudgetOk : gBudgetOver;
		const float linePos = pos.x + gDisplayBarSize.x * (budget / float(maxValue));
		const Vec3 lineVertices[] =
		{
			Vec3(linePos, pos.y - gBudgetBarSize, 0.0f),
			Vec3(linePos, pos.y + gDisplayBarSize.y + gBudgetBarSize, 0.0f),
		};
		m_pRenderAux->DrawLines(lineVertices, 2, color, gBudgetLineWidth);
	}

	void DrawBar(Vec2 pos, Vec2 size, uint maxValue, uint startValue, uint endValue, ColorB color)
	{
		AABB box;
		box.min.x = pos.x + size.x * (startValue / float(maxValue));
		box.min.y = pos.y;
		box.max.x = pos.x + size.x * (endValue / float(maxValue));
		box.max.y = pos.y + size.y;
		box.min.z = box.max.z = 0.0f;

		m_pRenderAux->DrawAABB(box, true, color, eBBD_Faceted);
	}

	void DrawBar(Vec2 pos, uint maxValue, uint startValue, uint endValue, ColorB color)
	{
		DrawBar(pos, gDisplayBarSize, maxValue, startValue, endValue, color);
	}

	void DrawBar(Vec2 pos, uint maxValue, uint curValue, ColorB color)
	{
		DrawBar(pos, gDisplayBarSize, maxValue, 0, curValue, color);
	}

	void DrawText(Vec2 pos, ColorF color, cstr text, ...)
	{
		va_list args;
		va_start(args, text);
		m_pRenderAux->Draw2dLabel(
			pos.x * m_screenSize.x, pos.y * m_screenSize.y,
			gDisplayFontSize, color, false,
			text, args);
		va_end(args);
	}

private:
	IRenderer* m_pRender;
	IRenderAuxGeom* m_pRenderAux;
	SAuxGeomRenderFlags m_prevFlags;
	Vec2 m_screenSize;
};

SStatistics::SStatistics()
{
	memset(m_values, 0, sizeof(m_values));
}

CParticleProfiler::CParticleProfiler()
	: m_entries(gEnv->pJobManager->GetNumWorkerThreads() + 1)
{
}

void CParticleProfiler::Display()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const int anyProfilerFlags = 3 | AlphaBits('f');
	if (GetCVars()->e_ParticlesProfiler & anyProfilerFlags)
	{
		SortEntries();

		if (!m_entries[0].empty())
		{
#ifndef _RELEASE
			if (GetCVars()->e_ParticlesProfiler & AlphaBit('f'))
			{
				SaveToFile();
				if (!(GetCVars()->e_ParticlesProfiler & AlphaBit('s')))
					GetCVars()->e_ParticlesProfiler &= ~AlphaBit('f');
			}
#endif
			if (GetCVars()->e_ParticlesProfiler & 1)
				DrawPerfomanceStats();
			else if (GetCVars()->e_ParticlesProfiler & 2)
				DrawMemoryStats();
		}
	}
	for (auto& entries : m_entries)
		entries.clear();
}

void CParticleProfiler::SaveToFile()
{
	string genName;
	string folderName = GetCVars()->e_ParticlesProfilerOutputFolder->GetString();
	string fileName = GetCVars()->e_ParticlesProfilerOutputName->GetString();
	if (folderName.empty() || fileName.empty())
		return;
	if (folderName[folderName.size() - 1] != '\\' || folderName[folderName.size() - 1] != '/')
		folderName += '/';
	static int fileIndex = 0;
	do
	{
		genName.Format("%s%s%06d.csv", folderName.c_str(), fileName.c_str(), fileIndex);
		++fileIndex;
	} while (gEnv->pCryPak->IsFileExist(genName));

	CCSVFileOutput output(genName.c_str());
	if (output)
		WriteEntries(output);
}

void CParticleProfiler::SortEntries()
{
	auto& finalElements = m_entries[0];
	for (uint i = 1; i < m_entries.size(); ++i)
		finalElements.insert(finalElements.end(), m_entries[i].begin(), m_entries[i].end());
	
	auto predicate = [](const SEntry& entry0, const SEntry& entry1)
	{
		const CParticleEmitter* pEmitter0 = entry0.m_pRuntime->GetEmitter();
		const CParticleEmitter* pEmitter1 = entry1.m_pRuntime->GetEmitter();
		if (pEmitter0->GetEmitterId() != pEmitter1->GetEmitterId())
			return pEmitter0->GetEmitterId() < pEmitter1->GetEmitterId();

		const CParticleComponent* pComponent0 = entry0.m_pRuntime->GetComponent();
		const CParticleComponent* pComponent1 = entry1.m_pRuntime->GetComponent();
		return pComponent0->GetComponentId() < pComponent1->GetComponentId();
	};
	std::sort(finalElements.begin(), finalElements.end(), predicate);
}

void CParticleProfiler::WriteEntries(CCSVFileOutput& output) const
{
	const auto& finalElements = m_entries[0];

	output.WriteHeader();

	const CParticleEmitter* pEmitter = nullptr;
	SStatistics runtimeStats;
	const CParticleComponentRuntime* pCurrentRuntime = finalElements.front().m_pRuntime;

	for (const SEntry& entry : finalElements)
	{
		if (entry.m_pRuntime != pCurrentRuntime)
		{
			output.WriteStatistics(*entry.m_pRuntime, runtimeStats);
			runtimeStats = SStatistics();
			pCurrentRuntime = entry.m_pRuntime;
		}
		runtimeStats.m_values[entry.m_type] += entry.m_value;
	}
	output.WriteStatistics(*pCurrentRuntime, runtimeStats);
}

void CParticleProfiler::DrawPerfomanceStats()
{
	const uint countsBudget = GetCVars()->e_ParticlesProfilerCountBudget;
	const uint timingBudget = GetCVars()->e_ParticlesProfilerTimingBudget;

	const SStatDisplay statisticsDisplay[] =
	{
		{ EPS_ActiveParticles,     "Active",            countsBudget },
		{ EPS_RendereredParticles, "Rendered",          countsBudget },
		{ EPS_TotalTiming,         "Total Timing (ns)", timingBudget },
	};

	CStatisticsDisplay output;
	Vec2 statPos = gDisplayPosition;
	DrawStatsCounts(output, statPos, countsBudget);
	statPos.y += gDisplayBarSize.y + gDisplayLineGap * 4;
	for (const auto& stat : statisticsDisplay)
	{
		DrawStats(output, statPos, stat.m_stat, stat.m_budget, stat.m_statName);
		statPos.y += gDisplayBarSize.y + gDisplayLineGap * (gDisplayMaxNumComponents + 3);
	}
}

void CParticleProfiler::DrawStatsCounts(CStatisticsDisplay& output, Vec2 pos, uint budget)
{
	uint numRendererd = 0;
	uint numActive = 0;
	uint numAllocated = 0;
	for (const SEntry& entry : m_entries[0])
	{
		switch (entry.m_type)
		{
		case EPS_RendereredParticles:
			numRendererd += entry.m_value;
			break;
		case EPS_ActiveParticles:
			numActive += entry.m_value;
			break;
		case EPS_AllocatedParticles:
			numAllocated += entry.m_value;
			break;
		}
	}

	const uint maxValue = max(budget, numAllocated);
	const Vec2 barSize = Vec2(gDisplayBarSize.x, gDisplayBarSize.y / 3.0f);
	Vec2 barPos = pos;
	Vec2 textPos = Vec2(pos.x, pos.y + gDisplayBarSize.y);

	output.DrawBar(barPos, barSize, maxValue, 0, numAllocated, ColorB(255, 0, 0));
	barPos.y += barSize.y;
	output.DrawBar(barPos, barSize, maxValue, 0, numActive, ColorB(255, 255, 0));
	barPos.y += barSize.y;
	output.DrawBar(barPos, barSize, maxValue, 0, numRendererd, ColorB(0, 255, 0));

	output.DrawText(textPos, ColorF(0.0f, 1.0f, 0.0f), "Rendered (%d)", numRendererd);
	textPos.y += gDisplayLineGap;
	output.DrawText(textPos, ColorF(1.0f, 1.0f, 0.0f), "Active (%d)", numActive);
	textPos.y += gDisplayLineGap;
	output.DrawText(textPos, ColorF(1.0f, 0.0f, 0.0f), "Allocated (%d)", numAllocated);

	output.DrawBudgetBar(pos, maxValue, budget);
}

void CParticleProfiler::DrawStats(CStatisticsDisplay& output, Vec2 pos, EProfileStat stat, uint budget, cstr statName)
{
	struct SStatEntry
	{
		SStatEntry()
			: pRuntime(nullptr)
			, value(0) {}
		const CParticleComponentRuntime* pRuntime;
		uint value;
	};

	const CParticleComponentRuntime* pRuntime = nullptr;
	SStatEntry statEntry;
	std::vector<SStatEntry> statEntries;
	uint statTotal = 0;

	for (const SEntry& entry : m_entries[0])
	{
		if (entry.m_type != stat)
			continue;

		if (entry.m_pRuntime != pRuntime)
		{
			pRuntime = entry.m_pRuntime;
			if (pRuntime && statEntry.value != 0.0f)
				statEntries.push_back(statEntry);
			statEntry = SStatEntry();
			statEntry.pRuntime = entry.m_pRuntime;
			pRuntime = entry.m_pRuntime;
		}

		uint value = 0;
		value += entry.m_value;
		statEntry.value += value;
		statTotal += value;
	}
	if (pRuntime && statEntry.value != 0.0f)
		statEntries.push_back(statEntry);

	std::sort(statEntries.begin(), statEntries.end(), [](const SStatEntry& v0, const SStatEntry& v1)
	{
		return v0.value > v1.value;
	});

	const Vec2 barPosition = Vec2(pos.x, pos.y + gDisplayLineGap);
	const uint maxValue = max(statTotal, budget);
	Vec2 textPos = pos;
	uint lastStat = 0;

	output.DrawText(textPos, ColorF(1.0f, 1.0f, 1.0f), "%s Total (%d)", statName, statTotal);
	textPos.y += gDisplayLineGap + gDisplayBarSize.y;

	uint statCount = min(uint(statEntries.size()), gDisplayMaxNumComponents);
	for (uint i = 0; i < statCount; ++i)
	{
		const CParticleComponentRuntime* pRuntime = statEntries[i].pRuntime;
		const CParticleComponent* pComponent = pRuntime->GetComponent();
		const CParticleEmitter* pEmitter = pRuntime->GetEmitter();
		IEntity* pEntity = pEmitter->GetOwnerEntity();

		const ColorF color = pEmitter->GetProfilerColor();
		const uint value = statEntries[i].value;

		output.DrawBar(barPosition, maxValue, lastStat, lastStat + value, ColorB(color));

		const cstr format = "\"%s\" : %s : %s (%d)";
		const cstr componentName = pComponent->GetName();
		const cstr effectName = pComponent->GetEffect()->GetFullName();
		const cstr entityName = pEntity ? pEntity->GetName() : gDefaultEntityName;
		output.DrawText(textPos, color, format, entityName, effectName, componentName, value);
		textPos.y += gDisplayLineGap;

		lastStat += value;
	}
	if ((statTotal - lastStat) != 0)
	{
		output.DrawBar(barPosition, maxValue, lastStat, statTotal, ColorB(92, 92, 92));
		output.DrawText(textPos, ColorF(0.35f, 0.35f, 0.35f), "other (%d)", statTotal - lastStat);
	}

	output.DrawBudgetBar(barPosition, maxValue, budget);
}

void CParticleProfiler::DrawMemoryStats()
{	
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
	CStatisticsDisplay output;

	const float screenWidth  = float(pRenderAux->GetCamera().GetViewSurfaceX());
	const float screenHeight = float(pRenderAux->GetCamera().GetViewSurfaceZ());

	const Vec2 pixSz = Vec2(1.0f / screenWidth, 1.0f / screenHeight);
	const Vec2 offset = Vec2(0.25, 0.025f);
	const float widthPerByte = 1.0f / float(1 << 15);
	const float height = 1.0f / 64.0f;

	int totalBytes = 0;
	uint usedBytes = 0;
	for (CParticleEmitter* pEmitter : GetPSystem()->GetActiveEmitters())
	{
		CParticleEffect* pEffect = pEmitter->GetCEffect();
		for (auto pRuntime : pEmitter->GetRuntimes())
		{
			if (!pRuntime->IsCPURuntime())
				continue;
			const CParticleContainer& container = pRuntime->GetContainer();
			const uint totalNumParticles = container.GetMaxParticles();
			const uint usedNumParticles = container.GetNumParticles();
			for (auto type : EParticleDataType::indices())
			{
				if (!container.HasData(type))
					continue;
				const size_t stride = type.info().typeSize;
				totalBytes += totalNumParticles * stride;
				usedBytes += usedNumParticles * stride;
			}
		}
	}

	output.DrawText(
		Vec2(0.015f, offset.y), ColorF(1.0f, 1.0f, 1.0f),
		"Memory Allocated(kB): %-5d Memory in use(kB): %-5d Utilization: %2d%%",
		totalBytes / 1024, usedBytes / 1024, usedBytes * 100 / totalBytes);

	size_t barIdx = 0;
	for (CParticleEmitter* pEmitter : GetPSystem()->GetActiveEmitters())
	{
		CParticleEffect* pEffect = pEmitter->GetCEffect();
		for (auto pRuntime : pEmitter->GetRuntimes())
		{
			if (!pRuntime->IsCPURuntime())
				continue;

			const CParticleContainer& container = pRuntime->GetContainer();
			const uint totalNumParticles = container.GetMaxParticles();
			const uint usedNumParticles = container.GetNumParticles();

			const ColorB darkRed = ColorB(128, 32, 0);
			const ColorB blue = ColorB(0, 128, 255);

			string name = string(pEffect->GetName()) + " : " + pRuntime->GetComponent()->GetName();
			output.DrawText(
				Vec2(0.015f, offset.y + barIdx * height + height),
				pEmitter->GetProfilerColor(), name.c_str());

			AABB box;
			box.min = box.max = offset + Vec2(0.0f, height * barIdx + height);
			box.max.y += height - pixSz.y;
			box.max.x += totalNumParticles * widthPerByte;
			pRenderAux->DrawAABB(box, true, darkRed, eBBD_Faceted);

			box.max.x = box.min.x + usedNumParticles * widthPerByte;
			pRenderAux->DrawAABB(box, true, blue, eBBD_Faceted);

			++barIdx;
		}
	}
}

}
