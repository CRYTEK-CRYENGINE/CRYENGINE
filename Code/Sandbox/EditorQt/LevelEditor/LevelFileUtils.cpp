// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "LevelAssetType.h"
#include "LevelFileUtils.h"

#include "CryEditDoc.h"
#include "FilePathUtil.h"

#include <QString>
#include <QDir>
#include <QFileInfo>

#include <CrySystem/IProjectManager.h>

namespace LevelFileUtils
{
namespace
{
const QString kLevelFolders[] =
{
	"Layers",
	"Minimap",
	"LevelData"
};

const QString kLevelFiles[] =
{
	"level.pak",
	"terraintexture.pak",
	"filelist.xml",
	"levelshadercache.pak"
};

const QString kLevelExtension(CLevelType::GetFileExtensionStatic());
const QString kLevelExtensions[] =
{
	kLevelExtension
};

const QStringList kLevelFilters(QStringList() << "*." + kLevelExtension);
const QString kSaveLevelExtension = QStringLiteral(".") + kLevelExtension;
const QString kInvalidPathChars = QStringLiteral("<>.:|?*");
} // namespace

AbsolutePath GetSaveLevelFile(const AbsolutePath& path)
{
	QDir dir(path);
	return dir.absoluteFilePath(dir.dirName() + kSaveLevelExtension);
}

AbsolutePath FindLevelFile(const AbsolutePath& path)
{
	QFileInfo fileInfo(path);
	if (fileInfo.isFile())
	{
		for (auto extension : kLevelExtensions)
		{
			if (fileInfo.suffix() == extension)
			{
				return path;
			}
		}
		return QString();
	}

	QDir dir(path);
	for (auto extension : kLevelExtensions)
	{
		auto fileName = dir.dirName() + "." + extension;
		if (dir.exists(fileName))
		{
			return dir.absoluteFilePath(fileName);
		}
	}
	return FindAnyLevelFile(path);
}

AbsolutePath FindAnyLevelFile(const AbsolutePath& path)
{
	QDir dir(path);
	dir.setNameFilters(kLevelFilters);
	if (0 == dir.count())
	{
		return QString();
	}
	return dir.absoluteFilePath(dir[0]);
}

bool IsPathToLevel(const AbsolutePath& path)
{
	return !FindAnyLevelFile(path).isEmpty();
	//QDir dir(path);
	//for (auto folderCandidate : kLevelFolders)
	//{
	//	if (QFileInfo(dir, folderCandidate).isDir())
	//	{
	//		return true;
	//	}
	//}
	//for (auto fileCandidate : kLevelFiles)
	//{
	//	if (QFileInfo(dir, fileCandidate).isFile())
	//	{
	//		return true;
	//	}
	//}
	//return false;
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

AbsolutePath ConvertEngineToAbsolutePath(const EnginePath& path)
{
	QDir basePath(GetEngineBasePath());
	return basePath.absoluteFilePath(path);
}

AbsolutePath GetGameBasePath()
{
	QDir basePath(GetEngineBasePath());
	return QDir::cleanPath(basePath.absoluteFilePath(QString() + PathUtil::GetGameFolder()));
}

GamePath ConvertAbsoluteToGamePath(const AbsolutePath& path)
{
	QDir basePath(GetGameBasePath());
	auto relativePath = basePath.relativeFilePath(path);
	if (relativePath.startsWith(".."))
	{
		return QString(); // refuse to convert above basePath
	}
	return relativePath;
}

AbsolutePath ConvertGameToAbsolutePath(const GamePath& gamePath)
{
	QDir basePath(GetGameBasePath());
	return basePath.absoluteFilePath(gamePath);
}

AbsolutePath GetUserBasePath()
{
	return QDir::cleanPath(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute());
}

UserPath ConvertAbsoluteToUserPath(const AbsolutePath& path)
{
	QDir basePath(GetUserBasePath());
	auto relativePath = basePath.relativeFilePath(path);
	if (relativePath.startsWith(".."))
	{
		return QString(); // refuse to convert above basePath
	}
	return relativePath;
}

AbsolutePath ConvertUserToAbsolutePath(const UserPath& path)
{
	QDir basePath(GetUserBasePath());
	return basePath.absoluteFilePath(path);
}

bool EnsureLevelPathsValid()
{
	const AbsolutePath levelDir = GetUserBasePath();

	// attempt to create if missing
	if (!QDir(levelDir).exists())
	{
		QDir().mkdir(levelDir);

		// recheck, we may have failed for whatever reason (permissions, error, etc)
		if (!QDir(levelDir).exists())
		{
			CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Level path is missing and could not be created"));
			return false;
		}
	}

	return true;
}

bool IsValidLevelPath(const UserPath& userPath)
{
	foreach(auto ch, userPath)
	{
		if (kInvalidPathChars.contains(ch))
		{
			return false;
		}
	}
	return true;
}

bool IsAnyParentPathLevel(const UserPath& userPath)
{
	QDir path(GetUserBasePath());
	auto parts = userPath.split(QDir::separator());
	parts.pop_back(); // do not include the path itself
	foreach(auto dir, parts)
	{
		auto dirPath = path.absoluteFilePath(dir);
		if (IsPathToLevel(dirPath))
			return true;
		path = dirPath;
	}
	return false;
}

bool IsAnySubFolderLevel(const AbsolutePath& path)
{
	QDir dir(path);
	foreach(auto subInfo, dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		auto subPath = subInfo.absoluteFilePath();
		if (IsPathToLevel(subPath))
		{
			return true;
		}
		if (IsAnySubFolderLevel(subPath))
		{
			return true;
		}
	}
	return false;
}

} // namespace LevelFileUtils

