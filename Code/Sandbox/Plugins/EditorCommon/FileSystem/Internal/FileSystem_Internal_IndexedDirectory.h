// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "FileSystem/FileSystem_Directory.h"

namespace FileSystem
{
namespace Internal
{

/// internal extended directory structure
struct SIndexedDirectory : SDirectory
{
	// indices
	QMultiHash<QString, DirectoryPtr> tokenDirectories;
	QMultiHash<QString, FilePtr>      tokenFiles;
	QMultiHash<QString, FilePtr>      extensionFiles;
};

} // namespace Internal
} // namespace FileSystem

