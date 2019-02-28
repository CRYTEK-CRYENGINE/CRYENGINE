// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Coverage.h"

void CLUACodeCoverage::VisitLine(const char* source, int line)
{
	TCoverage::iterator it = m_coverage.find(CONST_TEMP_STRING(source));
	if (it == m_coverage.end())
		it = m_coverage.insert(std::make_pair(string(source), TLines())).first;
	it->second.insert(line);
}

void CLUACodeCoverage::ResetFile(const char* source)
{
	TCoverage::iterator it = m_coverage.find(CONST_TEMP_STRING(source));
	if (it != m_coverage.end())
		it->second = TLines();
}

bool CLUACodeCoverage::GetCoverage(const char* source, int line) const
{
	TCoverage::const_iterator it = m_coverage.find(CONST_TEMP_STRING(source));
	if (it != m_coverage.end())
		return it->second.find(line) != it->second.end();

	return false;
}

void CLUACodeCoverage::DumpCoverage() const
{
	for (TCoverage::const_iterator it = m_coverage.begin(), eit = m_coverage.end(); it != eit; ++it)
	{
		uint32 num = it->second.size();
		if (num)
		{
			CryLog("%s : %u lines", it->first.c_str(), num);
		}
	}
}
