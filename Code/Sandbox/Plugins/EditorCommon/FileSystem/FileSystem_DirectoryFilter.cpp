// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_DirectoryFilter.h"
#include "FileSystem_EnginePath.h"

namespace FileSystem
{

SDirectoryFilter SDirectoryFilter::ForDirectory(const QString& directory)
{
	SDirectoryFilter filter;
	CRY_ASSERT(!directory.isEmpty());
	filter.directories.push_back(directory);
	return filter;
}

SDirectoryFilter SDirectoryFilter::ForDirectoryTree(const QString& directory)
{
	SDirectoryFilter filter = ForDirectory(directory);
	filter.recursiveSubdirectories = true;
	return filter;
}

void SDirectoryFilter::MakeInputValid()
{
	for (auto& directoryPath : directories)
	{
		directoryPath = SEnginePath::ConvertUserToKeyPath(directoryPath);
	}
	for (auto& token : directoryTokens)
	{
		CRY_ASSERT(token == token.toLower());
	}
	// empty means scan in engine root path
	if (directories.empty())
	{
		directories << QString();
	}
}

} // namespace FileSystem

