// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ExtensionFilter.h"

#include <QFileInfo>

ExtensionFilterVector CExtensionFilter::Parse(const char* pattern)
{
	ExtensionFilterVector result;

	const auto filterEntries = QString::fromLocal8Bit(pattern).split(QChar('|'), QString::SkipEmptyParts);

	CRY_ASSERT_MESSAGE(filterEntries.size() % 2 == 0, "Invalid extension format");

	const int count = filterEntries.size() / 2;
	for (int i = 0; i < count; i++)
	{
		auto description = filterEntries[i*2];
		auto fileSuffixes = filterEntries[i*2 + 1].split(QChar(';'), QString::SkipEmptyParts);
		for (auto& suffix : fileSuffixes)
		{
			suffix = QFileInfo(suffix).completeSuffix();
		}

		result << CExtensionFilter(description, fileSuffixes);
	}
	return result;
}

