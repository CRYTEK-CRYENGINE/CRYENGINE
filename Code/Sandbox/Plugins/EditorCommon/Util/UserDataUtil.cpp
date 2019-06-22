// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "FileUtils.h"
#include "UserDataUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include <QtUtil.h>
#include <CrySystem/ISystem.h>

namespace UserDataUtil
{

namespace Private_UserDataUtil
{
	string GetEnginePath(const char* szRelativeFilePath)
	{
		string path;
		path.Format("%s/Editor/%s", PathUtil::GetEnginePath(), szRelativeFilePath);
		return path;
	}

	QVariant Load(const char* szFullPath)
	{
		QFile file(szFullPath);
		if (!file.open(QIODevice::ReadOnly))
		{
			string msg;
			msg.Format("Failed to open path: %s", szFullPath);
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, msg);
			return QVariant();
		}

		QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));

		return doc.toVariant();
	}

	void Merge(const QVariantMap& defaultData, const QVariantMap& preferredData, QVariantMap& mergedData)
	{
		// Merged data should start by containing default data. Items that conflict with preferred data will be replaced
		mergedData = defaultData;

		QList<QString> preferredDataKeys = preferredData.keys();
		for (const QString& preferredDataKey : preferredDataKeys)
		{
			if (defaultData.contains(preferredDataKey))
			{
				QVariant defaultValue = defaultData.value(preferredDataKey);
				QVariant preferredValue = preferredData.value(preferredDataKey);
				
				// if type defers from default type, use preferred value
				if (defaultValue.type() == QMetaType::QVariantMap && preferredValue.type() == QMetaType::QVariantMap)
				{
					QVariantMap& defaultVariantMap = defaultData[preferredDataKey].toMap();
					QVariantMap& preferredVariantMap = preferredData[preferredDataKey].toMap();
					QVariantMap& mergedVariantMap = mergedData[preferredDataKey].toMap();

					Merge(defaultVariantMap, preferredVariantMap, mergedVariantMap);
					continue;
				}

				// Intentional fall-through:
				// If types are different or if variant type is not a map, then proceed to use the preferred data value
			}

			// if not key is not contained in default data, then proceed to insert insert
			mergedData.insert(preferredDataKey, preferredData[preferredDataKey]);
		}
	}

	QVariant LoadMerged(const char* szRelativeFilePath)
	{
		QVariantMap userData = Load(GetUserPath(szRelativeFilePath).c_str()).toMap();
		QVariantMap engineData = Load(GetEnginePath(szRelativeFilePath).c_str()).toMap();
		QVariantMap mergedData;

		Merge(engineData, userData, mergedData);

		return mergedData;
	}
}

const unsigned currentVersion = 1;

bool Migrate(const char* szRelativeFilePath)
{
	unsigned lastVersion = currentVersion;

	QString userDataRootPath(QtUtil::GetAppDataFolder() + "/");
	const QString currentVersionPath(userDataRootPath + QString::number(currentVersion));

	// Get latest file version
	while (!QFileInfo::exists(userDataRootPath + QString::number(lastVersion) + "/" + szRelativeFilePath) && --lastVersion)
		;

	// If there's a version mismatch, we must try to migrate data
	if (lastVersion != currentVersion)
	{
		// If last version is a valid version, then copy it's directory into current version for migration
		QString lastVersionPath;
		if (lastVersion)
		{
			lastVersionPath = userDataRootPath + QString::number(lastVersion) + "/" + szRelativeFilePath;
		}
		else
		{
			lastVersionPath = userDataRootPath + szRelativeFilePath;
		}

		// If directory was copied successfully, then return the current version path
		return FileUtils::CopyDirectory(QtUtil::ToString(lastVersionPath).c_str(), QtUtil::ToString(currentVersionPath).c_str());
	}

	return true;
}

string GetUserPath(const char* szRelativeFilePath)
{
	string userPath;
	userPath.Format("%s/%d/%s", QtUtil::ToString(QtUtil::GetAppDataFolder()), currentVersion, szRelativeFilePath);
	return userPath;
}

QVariant Load(const char* szRelativeFilePath, LoadType loadType)
{
	switch (loadType)
	{
	case LoadType::PrioritizeUserData:
		return Private_UserDataUtil::Load(GetUserPath(szRelativeFilePath).c_str());
	case LoadType::MergeData:
		return Private_UserDataUtil::LoadMerged(szRelativeFilePath);
	}

	CRY_ASSERT_MESSAGE(0, "User Data Util: Unknown loadtype");
	return QVariant();
}

bool Save(const char* szRelativeFilePath, const char* data)
{
	const string filePath(GetUserPath(szRelativeFilePath));
	// Remove filename from path
	QDir dir(QFileInfo(filePath.c_str()).absolutePath());
	// Make sure the directory exists
	dir.mkpath(dir.absolutePath());

	QFile file(filePath.c_str());
	if (!file.open(QIODevice::WriteOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open path: %s", filePath.c_str());
		return false;
	}

	return file.write(data) > 0;
}
} // namespace UserDataUtil

CUserData::CUserData(const std::vector<string>& userDataPaths)
{
	// Automatically migrate any relevant data to new version folder
	for (auto& userDataPath : userDataPaths)
	{
		UserDataUtil::Migrate(userDataPath.c_str());
	}
}
