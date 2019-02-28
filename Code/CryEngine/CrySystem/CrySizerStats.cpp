// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/ILog.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryRenderer/IRenderer.h>
#include "CrySizerImpl.h"
#include "CrySizerStats.h"
#include <CrySystem/ITextModeConsole.h>
#include <CryCore/Platform/CryWindows.h>

CrySizerStatsBuilder::CrySizerStatsBuilder(CrySizerImpl* pSizer, int nMinSubcomponentBytes) :
	m_pSizer(pSizer),
	m_nMinSubcomponentBytes(nMinSubcomponentBytes < 0 || nMinSubcomponentBytes > 0x10000000 ? 0 : nMinSubcomponentBytes),
	m_pStats(nullptr)
{

}

// creates the map of names from old (in the sizer Impl) to new (in the Stats)
void CrySizerStatsBuilder::processNames()
{
	size_t numCompNames = m_pSizer->m_arrNames.size();
	m_pStats->m_arrComponents.reserve(numCompNames);
	m_pStats->m_arrComponents.clear();

	m_mapNames.resize(numCompNames, (size_t)-1);

	// add all root objects
	addNameSubtree(0, 0);
}

//////////////////////////////////////////////////////////////////////////
// given the name in the old system, adds the subtree of names to the
// name map and components. In case all the subtree is empty, returns false and
// adds nothing
size_t CrySizerStatsBuilder::addNameSubtree(unsigned nDepth, size_t nName)
{
	assert((int)nName < m_pSizer->m_arrNames.size());

	CrySizerImpl::ComponentName& rCompName = m_pSizer->m_arrNames[nName];
	size_t sizeObjectsTotal = rCompName.sizeObjectsTotal;

	if (sizeObjectsTotal <= m_nMinSubcomponentBytes)
		return sizeObjectsTotal; // the subtree didn't pass

	// the index of the component in the stats object (sorted by the depth-first traverse order)
	size_t nNewName = m_pStats->m_arrComponents.size();
	m_pStats->m_arrComponents.resize(nNewName + 1);

	Component& rNewComp = m_pStats->m_arrComponents[nNewName];
	rNewComp.strName = rCompName.strName;
	rNewComp.nDepth = nDepth;
	rNewComp.numObjects = rCompName.numObjects;
	rNewComp.sizeBytes = rCompName.sizeObjects;
	rNewComp.sizeBytesTotal = sizeObjectsTotal;
	m_mapNames[nName] = nNewName;

	// find the immediate children and sort them by their total size
	typedef std::map<size_t, size_t> UintUintMap;
	UintUintMap mapSizeName; // total size -> child index (name in old indexation)

	for (int i = nName + 1; i < m_pSizer->m_arrNames.size(); ++i)
	{
		CrySizerImpl::ComponentName& rChild = m_pSizer->m_arrNames[i];
		if (rChild.nParent == nName && rChild.sizeObjectsTotal > m_nMinSubcomponentBytes)
			mapSizeName.insert(UintUintMap::value_type(rChild.sizeObjectsTotal, i));
	}

	// add the sorted components
	/*
	   for (unsigned i = nName + 1; i < m_pSizer->m_arrNames.size(); ++i)
	   if (m_pSizer->m_arrNames[i].nParent == nName)
	    addNameSubtree(nDepth+1,i);
	 */

	for (UintUintMap::reverse_iterator it = mapSizeName.rbegin(); it != mapSizeName.rend(); ++it)
	{
		addNameSubtree(nDepth + 1, it->second);
	}

	return sizeObjectsTotal;
}

//////////////////////////////////////////////////////////////////////////
// creates the statistics out of the given CrySizerImpl into the given CrySizerStats
// Maps the old to new names according to the depth-walk tree rule
void CrySizerStatsBuilder::build(CrySizerStats* pStats)
{
	m_pStats = pStats;

	m_mapNames.clear();

	processNames();

	m_pSizer->clear();
	pStats->refresh();
	pStats->m_nAgeFrames = 0;
}

//////////////////////////////////////////////////////////////////////////
// constructs the statistics based on the given cry sizer
CrySizerStats::CrySizerStats(CrySizerImpl* pCrySizer)
{
	CrySizerStatsBuilder builder(pCrySizer);
	builder.build(this);
}

CrySizerStats::CrySizerStats()
	: m_nStartRow(0)
	, m_nAgeFrames(0)
	, m_nMaxNameLength(0)
{
	for (int i = 0; i < g_numTimers; ++i)
	{
		m_fTime[i] = 0.0f;
	}
}

void CrySizerStats::updateKeys()
{
	const unsigned int statSize = size();
	//assume 10 pixels for font
	unsigned int height = gEnv->pRenderer->GetOverlayHeight() / 12;
	if (CryGetAsyncKeyState(VK_UP))
	{
		if (m_nStartRow > 0)
			--m_nStartRow;
	}
	if (CryGetAsyncKeyState(VK_DOWN))
	{
		if (statSize > height + m_nStartRow)
			++m_nStartRow;
	}
	if (CryGetAsyncKeyState(VK_RIGHT) & 1)
	{
		//assume 10 pixels for font
		if (statSize > height)
			m_nStartRow = statSize - height;
	}
	if (CryGetAsyncKeyState(VK_LEFT) & 1)
	{
		m_nStartRow = 0;
	}
}

// if there is already such name in the map, then just returns the index
// of the compoentn in the component array; otherwise adds an entry to themap
// and to the component array nad returns its index
CrySizerStatsBuilder::Component& CrySizerStatsBuilder::mapName(unsigned nName)
{
	assert(m_mapNames[nName] != -1);
	return m_pStats->m_arrComponents[m_mapNames[nName]];
	/*
	   IdToIdMap::iterator it = m_mapNames.find (nName);
	   if (it == m_mapNames.end())
	   {
	   unsigned nNewName = m_arrComponents.size();
	   m_mapNames.insert (IdToIdMap::value_type(nName, nNewName));
	   m_arrComponents.resize(nNewName + 1);
	   m_arrComponents[nNewName].strName.swap(m_pSizer->m_arrNames[nName]);
	   return m_arrComponents.back();
	   }
	   else
	   {
	   assert (it->second < m_arrComponents.size());
	   return m_arrComponents[it->second];
	   }
	 */
}

// refreshes the statistics built after the component array is built
void CrySizerStats::refresh()
{
	m_nMaxNameLength = 0;
	for (size_t i = 0; i < m_arrComponents.size(); ++i)
	{
		size_t nLength = m_arrComponents[i].strName.length() + m_arrComponents[i].nDepth;
		if (nLength > m_nMaxNameLength)
			m_nMaxNameLength = nLength;
	}
}

bool CrySizerStats::Component::GenericOrder::operator()(const Component& left, const Component& right) const
{
	return left.strName < right.strName;
}

CrySizerStatsRenderer::CrySizerStatsRenderer(ISystem* pSystem, CrySizerStats* pStats, unsigned nMaxSubcomponentDepth, int nMinSubcomponentBytes) :
	m_pStats(pStats),
	m_pRenderer(pSystem->GetIRenderer()),
	m_pLog(pSystem->GetILog()),
	m_pTextModeConsole(pSystem->GetITextModeConsole()),
	m_nMinSubcomponentBytes(nMinSubcomponentBytes < 0 || nMinSubcomponentBytes > 0x10000000 ? 0x8000 : nMinSubcomponentBytes),
	m_nMaxSubcomponentDepth(nMaxSubcomponentDepth)
{

}


#include <CryRenderer/IRenderAuxGeom.h>

static void DrawStatsText(float x, float y, float fScale, float color[4], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	IRenderAuxText::DrawText(Vec3(x, y, 0.5f), fScale, color, eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace, format, args);
	va_end(args);
}

void CrySizerStatsRenderer::render(bool bRefreshMark)
{
	if (!m_pStats->size())
		return;

	// left coordinate of the text
	unsigned nNameWidth = (unsigned)(m_pStats->getMaxNameLength() + 1);
	if (nNameWidth < 25)
		nNameWidth = 25;
	float fCharScaleX = 1.2f;
	float fLeft = 0;
	float fTop = 8;
	float fVStep = 9;

#ifdef _DEBUG
	const char* szCountStr1 = "count";
	const char* szCountStr2 = "_____";
#else // _DEBUG
	const char* szCountStr1 = "", * szCountStr2 = "";
#endif // _DEBUG

	float fTextColor[4] = { 0.9f, 0.85f, 1, 0.85f };
	DrawStatsText(fLeft, fTop, fCharScaleX, fTextColor,
	              "%-*s   total  partial  %s", nNameWidth, bRefreshMark ? "Memory usage (refresh*)" : "Memory usage (refresh )", szCountStr1);
	DrawStatsText(fLeft, fTop + fVStep * 0.25f, fCharScaleX, fTextColor,
	              "%*s   _____   _______  %s", nNameWidth, "", szCountStr2);

	unsigned nSubgroupDepth = 1;

	// different colors used to paint the statistics subgroups
	// a new statistic subgroup starts with a new subtree of depth <= specified
	float fGray = 0;//0.45f;
	float fLightGray = 0.5f;//0.8f;
	float fColors[] =
	{
		fLightGray, fLightGray, fGray, 1,
		1,          1,          1,     1,
		fGray,      1,          1,     1,
		1,          fGray,      1,     1,
		1,          1,          fGray, 1,
		fGray,      fLightGray, 1,     1,
		fGray,      1,          fGray, 1,
		1,          fGray,      fGray, 1
	};
	float* pColor = fColors;

	unsigned statSize = m_pStats->size();
	unsigned startRow = m_pStats->row();
	unsigned i = 0;
	for (; i < startRow; ++i)
	{
		const Component& rComp = (*m_pStats)[i];
		if (rComp.nDepth <= nSubgroupDepth)
		{
			//switch the color
			pColor += 4;
			if (pColor >= fColors + CRY_ARRAY_COUNT(fColors))
				pColor = fColors;

			fTop += fVStep * (0.333333f + (nSubgroupDepth - rComp.nDepth) * 0.15f);
		}
	}

	for (unsigned r = startRow; i < statSize; ++i)
	{
		const Component& rComp = (*m_pStats)[i];
		if (rComp.nDepth <= nSubgroupDepth)
		{
			//switch the color
			pColor += 4;
			if (pColor >= fColors + CRY_ARRAY_COUNT(fColors))
				pColor = fColors;

			fTop += fVStep * (0.333333f + (nSubgroupDepth - rComp.nDepth) * 0.15f);
		}

		if (rComp.sizeBytesTotal <= m_nMinSubcomponentBytes || rComp.nDepth > m_nMaxSubcomponentDepth)
			continue;

		fTop += fVStep;

		char szDepth[32] = " ..............................";
		if (rComp.nDepth < sizeof(szDepth))
			szDepth[rComp.nDepth] = '\0';

		char szSize[32];
		if (rComp.sizeBytes > 0)
		{
			if (rComp.sizeBytesTotal > rComp.sizeBytes)
				cry_sprintf(szSize, "%7.3f  %7.3f", rComp.getTotalSizeMBytes(), rComp.getSizeMBytes());
			else
				cry_sprintf(szSize, "         %7.3f", rComp.getSizeMBytes());
		}
		else
		{
			assert(rComp.sizeBytesTotal > 0);
			cry_sprintf(szSize, "%7.3f         ", rComp.getTotalSizeMBytes());
		}
		char szCount[16];
#ifdef _DEBUG
		if (rComp.numObjects)
			cry_sprintf(szCount, "%8" PRISIZE_T, rComp.numObjects);
		else
#endif
		szCount[0] = '\0';

		DrawStatsText(fLeft, fTop, fCharScaleX, pColor,
		              "%s%-*s:%s%s", szDepth, nNameWidth - rComp.nDepth, rComp.strName.c_str(), szSize, szCount);

		if (m_pTextModeConsole)
		{
			string text;
			text.Format("%s%-*s:%s%s", szDepth, nNameWidth - rComp.nDepth, rComp.strName.c_str(), szSize, szCount);
			m_pTextModeConsole->PutText(0, r++, text.c_str());
		}
	}

	float fLTGrayColor[4] = { fLightGray, fLightGray, fLightGray, 1.0f };
	fTop += 0.25f * fVStep;
	DrawStatsText(fLeft, fTop, fCharScaleX, fLTGrayColor,
	              "%-*s %s", nNameWidth, "___________________________", "________________");
	fTop += fVStep;

	const char* szOverheadNames[CrySizerStats::g_numTimers] =
	{
		".Collection",
		".Transformation",
		".Cleanup"
	};
	bool bOverheadsHeaderPrinted = false;
	for (i = 0; i < CrySizerStats::g_numTimers; ++i)
	{
		float fTime = m_pStats->getTime(i);
		if (fTime < 20)
			continue;
		// print the header
		if (!bOverheadsHeaderPrinted)
		{
			DrawStatsText(fLeft, fTop, fCharScaleX, fTextColor,
			              "%-*s", nNameWidth, "Overheads");
			fTop += fVStep;
			bOverheadsHeaderPrinted = true;
		}

		DrawStatsText(fLeft, fTop, fCharScaleX, fTextColor,
		              "%-*s:%7.1f ms", nNameWidth, szOverheadNames[i], fTime);
		fTop += fVStep;
	}
}

void CrySizerStatsRenderer::dump(bool bUseKB)
{
	if (!m_pStats->size())
		return;

	unsigned nNameWidth = (unsigned)(m_pStats->getMaxNameLength() + 1);

	// left coordinate of the text
	m_pLog->LogToFile("Memory Statistics: %s", bUseKB ? "KB" : "MB");
	m_pLog->LogToFile("%-*s   TOTAL   partial  count", nNameWidth, "");

	// different colors used to paint the statistics subgroups
	// a new statistic subgroup starts with a new subtree of depth <= specified

	for (unsigned i = 0; i < m_pStats->size(); ++i)
	{
		const Component& rComp = (*m_pStats)[i];

		if (rComp.sizeBytesTotal <= m_nMinSubcomponentBytes || rComp.nDepth > m_nMaxSubcomponentDepth)
			continue;

		char szDepth[32] = " ..............................";
		if (rComp.nDepth < sizeof(szDepth))
			szDepth[rComp.nDepth] = '\0';

		char szSize[32];
		if (rComp.sizeBytes > 0)
		{
			if (rComp.sizeBytesTotal > rComp.sizeBytes)
				cry_sprintf(szSize, bUseKB ? "%7.2f  %7.2f" : "%7.3f  %7.3f", bUseKB ? rComp.getTotalSizeKBytes() : rComp.getTotalSizeMBytes(), bUseKB ? rComp.getSizeKBytes() : rComp.getSizeMBytes());
			else
				cry_sprintf(szSize, bUseKB ? "         %7.2f" : "         %7.3f", bUseKB ? rComp.getSizeKBytes() : rComp.getSizeMBytes());
		}
		else
		{
			assert(rComp.sizeBytesTotal > 0);
			cry_sprintf(szSize, bUseKB ? "%7.2f         " : "%7.3f         ", bUseKB ? rComp.getTotalSizeKBytes() : rComp.getTotalSizeMBytes());
		}
		char szCount[16];

		if (rComp.numObjects)
			cry_sprintf(szCount, "%8u", (unsigned int)rComp.numObjects);
		else
			szCount[0] = '\0';

		m_pLog->LogToFile("%s%-*s:%s%s", szDepth, nNameWidth - rComp.nDepth, rComp.strName.c_str(), szSize, szCount);
	}
}

void CrySizerStats::startTimer(unsigned nTimer, ITimer* pTimer)
{
	assert(nTimer < g_numTimers);
	m_fTime[nTimer] = pTimer->GetAsyncCurTime();
}

void CrySizerStats::stopTimer(unsigned nTimer, ITimer* pTimer)
{
	assert(nTimer < g_numTimers);
	m_fTime[nTimer] = 1000 * (pTimer->GetAsyncCurTime() - m_fTime[nTimer]);
}
