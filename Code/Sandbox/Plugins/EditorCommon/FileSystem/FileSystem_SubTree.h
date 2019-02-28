// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_File.h"
#include "FileSystem_Directory.h"

namespace FileSystem
{

struct SSubTree;
typedef QVector<SSubTree> SubTreeVector;

/// \brief sub tree of a directory
///
/// used for hierarchical filter results
struct SSubTree
{
	DirectoryPtr  directory;
	FileVector    files;
	SubTreeVector subDirectories;
	bool          wasConsidered;

public:
	inline SSubTree() : wasConsidered(false) {}
};

} // namespace FileSystem

