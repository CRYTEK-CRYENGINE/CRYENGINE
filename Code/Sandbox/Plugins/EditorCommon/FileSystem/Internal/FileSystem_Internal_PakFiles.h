// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Internal_Data.h"

namespace FileSystem
{
namespace Internal
{

class CPakFiles
{
public:
	SArchiveContentInternal GetContents(const QString& keyEnginePath) const;

#ifdef EDITOR_COMMON_EXPORTS
private:
	void GetContentsInternal(const std::string& archiveEnginePath, const QString& fullLocalPath, SArchiveContentInternal& result, QStringList& directoryList) const;
#endif
};

} // namespace Internal
} // namespace FileSystem

