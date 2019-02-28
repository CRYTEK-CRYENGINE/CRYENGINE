// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_EnginePath.h"

#include <QDir>

namespace FileSystem
{

QString SEnginePath::ConvertUserToKeyPath(const QString& inputPath)
{
	auto keyEnginePath = QDir::cleanPath(inputPath).toLower();
	if (!keyEnginePath.isEmpty() && keyEnginePath[0] != '/')
	{
		keyEnginePath = QStringLiteral("/") + keyEnginePath; // prepend a slash
	}
	return keyEnginePath;
}

QString SEnginePath::ConvertUserToFullPath(const QString& inputPath)
{
	auto enginePath = QDir::cleanPath(inputPath);
	if (!enginePath.isEmpty() && enginePath[0] != '/')
	{
		enginePath = QStringLiteral("/") + enginePath; // prepend a slash
	}
	return enginePath;
}

QString SEnginePath::ConvertToNormalPath(const QString& enginePath)
{
	if (enginePath.isEmpty())
	{
		return QStringLiteral("/"); // root path is slash
	}
	else if (enginePath[0] == '/')
	{
		return enginePath.mid(1); // cut away first slash
	}
	return enginePath;
}

} // namespace FileSystem

