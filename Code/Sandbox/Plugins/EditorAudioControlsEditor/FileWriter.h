// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"

namespace ACE
{
class CLibrary;

class CFileWriter final
{
public:

	CFileWriter() = delete;
	CFileWriter(CFileWriter const&) = delete;
	CFileWriter(CFileWriter&&) = delete;
	CFileWriter& operator=(CFileWriter const&) = delete;
	CFileWriter& operator=(CFileWriter&&) = delete;

	explicit CFileWriter(FileNames& previousLibraryPaths)
		: m_previousLibraryPaths(previousLibraryPaths)
	{}

	void WriteAll();

private:

	void WriteLibrary(CLibrary& library, ContextIds& contextIds);

	FileNames& m_previousLibraryPaths;
	FileNames  m_foundLibraryPaths;
};
} // namespace ACE
