// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QString>

namespace FileSystem
{

/// \brief key and full version of the filesystem engine pathes
/// \note root directory has empty string()
/// \note except root directory all pathes start with '/'
struct SEnginePath
{
	QString key;  ///< key version of the path (all lowercase if filesystem is case insensitive)
	QString full; ///< original version of the path

public:
	inline SEnginePath() {}
	explicit inline SEnginePath(const QString& fullPath) : key(fullPath.toLower()), full(fullPath) {}
	inline SEnginePath(const SEnginePath& other) : key(other.key), full(other.full) {}

public:
	static QString ConvertUserToKeyPath(const QString& inputPath);
	static QString ConvertUserToFullPath(const QString& inputPath);
	static QString ConvertToNormalPath(const QString& enginePath);
};

} // namespace FileSystem

