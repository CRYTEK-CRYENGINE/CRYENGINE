// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LevelFileUtils.h"
#include "LevelAssetType.h"

#include <PathUtils.h>

#include <CrySystem/IProjectManager.h>

#include <QDir>
#include <QString>

namespace LevelFileUtils
{
namespace
{

const QString kLevelExtension(CLevelType::GetFileExtensionStatic());
const QString kLevelExtensions[] =
{
	kLevelExtension
};

const QStringList kLevelFilters(QStringList() << "*." + kLevelExtension);

bool IsPathToLevelAbsolute(const AbsolutePath& path)
{
	QDir dir(path);
	dir.setNameFilters(kLevelFilters);
	return dir.count() != 0;
}

bool IsAnySubFolderLevelAbsolute(const AbsolutePath& path)
{
	QDir dir(path);
	foreach(auto subInfo, dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		auto subPath = subInfo.absoluteFilePath();
		if (IsPathToLevelAbsolute(subPath))
		{
			return true;
		}
		if (IsAnySubFolderLevelAbsolute(subPath))
		{
			return true;
		}
	}
	return false;
}

} // namespace


bool IsPathToLevel(const AssetPath& assetPath)
{
	return IsPathToLevelAbsolute(ConvertAssetToAbsolutePath(assetPath));
}

AbsolutePath GetEngineBasePath()
{
	return QDir::currentPath();
}

EnginePath ConvertAbsoluteToEnginePath(const AbsolutePath& path)
{
	QDir basePath(GetEngineBasePath());
	auto relativePath = basePath.relativeFilePath(path);
	if (relativePath.startsWith(".."))
	{
		return QString();
	}
	return relativePath;
}

AbsolutePath ConvertEngineToAbsolutePath(const EnginePath& enginePath)
{
	QDir basePath(GetEngineBasePath());
	return basePath.absoluteFilePath(enginePath);
}


AbsolutePath GetAssetBasePathAbsolute()
{
	return QDir::cleanPath(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute());
}

AssetPath ConvertAbsoluteToAssetPath(const AbsolutePath& path)
{
	QDir basePath(GetAssetBasePathAbsolute());
	auto relativePath = basePath.relativeFilePath(path);
	if (relativePath.startsWith(".."))
	{
		return QString(); // refuse to convert above basePath
	}
	return relativePath;
}

AbsolutePath ConvertAssetToAbsolutePath(const AssetPath& assetPath)
{
	QDir basePath(GetAssetBasePathAbsolute());
	return basePath.absoluteFilePath(assetPath);
}

bool IsAnyParentPathLevel(const AssetPath& assetPath)
{
	QDir path(GetAssetBasePathAbsolute());
	auto parts = assetPath.split('/');
	parts.pop_back(); // do not include the assetPath itself
	foreach(auto dir, parts)
	{
		auto dirPath = path.absoluteFilePath(dir);
		if (IsPathToLevelAbsolute(dirPath))
			return true;
		path = dirPath;
	}
	return false;
}

bool IsAnySubFolderLevel(const AssetPath& assetPath)
{
	return IsAnySubFolderLevelAbsolute(ConvertAssetToAbsolutePath(assetPath));
}

} // namespace LevelFileUtils
