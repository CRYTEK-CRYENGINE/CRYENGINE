// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem/FileSystem_SubTreeMonitor.h"

namespace FileSystem
{
namespace Internal
{

struct SSnapshotFileUpdate
{
	FilePtr from;
	FilePtr to;
};

struct SSnapshotDirectoryUpdate
{
	DirectoryPtr                      from;
	DirectoryPtr                      to;
	QVector<SSnapshotDirectoryUpdate> directoryUpdates;
	QVector<SSnapshotFileUpdate>      fileUpdates;
};

struct SSnapshotUpdate
{
	SnapshotPtr              from;
	SnapshotPtr              to;
	SSnapshotDirectoryUpdate root;
};

} // namespace Internal
} // namespace FileSystem

