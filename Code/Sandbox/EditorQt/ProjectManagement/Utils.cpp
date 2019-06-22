// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utils.h"

#include <CryIcon.h>
#include <FileUtils.h>
#include <PathUtils.h>
#include <QtUtil.h>

#include <CrySerialization/yasli/JSONIArchive.h>
#include <CryString/CryStringUtils.h>

#include <QApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

string FindCryEngineRootFolder()
{
	QDir dir(qApp->applicationDirPath());
	while (dir.exists())
	{

		QDirIterator iterator(dir.absolutePath(), QStringList() << "cryengine.cryengine", QDir::Files);
		if (iterator.hasNext())
		{
			return QtUtil::ToString(dir.absolutePath());
		}

		if (!dir.cdUp())
		{
			break;
		}
	}

	return "";
}

bool IsProjectSpecifiedInSystemConfig(const string& engineFolder)
{
	const string fileName = PathUtil::Make(engineFolder, "system.cfg");

	QFile configFile(fileName.c_str());
	if (!configFile.open(QIODevice::ReadOnly))
	{
		return false;
	}

	QTextStream stream(&configFile);
	while (!stream.atEnd())
	{
		QString line = stream.readLine();
		
		//Simplest check: the line should be started from it, and not commented
		if (0 == line.indexOf("sys_project"))
		{
			return true;
		}
	}
	
	return false;
}

string FindProjectInFolder(const string& folder)
{
	QFileInfo fileInfo(PathUtil::AddSlash(folder).c_str());
	QDirIterator iterator(fileInfo.absolutePath(), QStringList() << "*.cryproject", QDir::Files);
	if (!iterator.hasNext())
	{
		return "";
	}

	iterator.next();
	return QtUtil::ToString(iterator.fileInfo().absoluteFilePath());
}

string AskUserToSpecifyProject(QWidget* pParent, bool runOnSandboxInit, CSelectProjectDialog::Tab tabToShow)
{
	CSelectProjectDialog dlg(pParent, runOnSandboxInit, tabToShow);
	if (runOnSandboxInit)
	{
		const string startupProject = dlg.GetProjectManager().GetStartupProject();
		if (!startupProject.empty())
		{
			return startupProject;
		}
	}

	if (dlg.exec() != QDialog::Accepted)
	{
		return "";
	}

	return dlg.GetPathToProject();
}

void AppendProjectPathToCommandLine(const string& projectPath, SSystemInitParams& systemParams)
{
	cry_strcat(systemParams.szSystemCmdLine, " -project ");
	cry_strcat(systemParams.szSystemCmdLine, projectPath);
}

string GetCryEngineProgramDataFolder()
{
	PWSTR programData;
	if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData) != S_OK)
		return false;

	const string cryEngineProgramDataFolder = PathUtil::Make(CryStringUtils::WStrToUTF8(programData), "Crytek/CRYENGINE/");
	CoTaskMemFree(programData);
	return cryEngineProgramDataFolder;
}

SCryEngineVersion::SCryEngineVersion()
	: versionMajor(-1)
	, versionMinor(-1)
	, versionBuild(-1)
{}

void SCryEngineVersion::Serialize(Serialization::IArchive& ar)
{
	ar(id, "id");
	ar(name, "name");
	ar(version, "version");

	if (ar.isInput())
	{
		if (!IsValid())
		{
			CRY_ASSERT(false);
			// all will be -1, and will not match current verions;
			return;
		}

		versionMajor = version[0] - '0';
		versionMinor = version[2] - '0';

		string str = version.substr(4);
		std::string stdStr(str);
		versionBuild = std::stoi(stdStr);
	}
}

int SCryEngineVersion::GetVersionMajor() const
{
	return versionMajor;
}

int SCryEngineVersion::GetVersionMinor() const
{
	return versionMinor;
}

int SCryEngineVersion::GetBuild() const
{
	return versionBuild;
}

string SCryEngineVersion::GetVersionShort() const
{
	if (!IsValid())
	{
		CRY_ASSERT(false);
		// This version will not match all supported versions
		return version;
	}

	return version.substr(0, 3);
}

bool SCryEngineVersion::Equal(const SCryEngineVersion& other, bool exactMatch) const
{
	if (GetVersionMajor() != other.GetVersionMajor())
	{
		return false;
	}

	if (GetVersionMinor() != other.GetVersionMinor())
	{
		return false;
	}

	if (exactMatch)
	{
		return id.CompareNoCase(other.id) == 0;
	}

	return true;
}

bool SCryEngineVersion::IsValid() const
{
	// Format: "5.5.0"
	return !id.empty() && !name.empty() && version.length() == 5 && version[1] == '.' && version[3] == '.';
}

SCryEngineVersion GetCurrentCryEngineVersion()
{
	const string strPathToConfig = PathUtil::Make(FindCryEngineRootFolder(), "cryengine.cryengine");

	yasli::JSONIArchive ia;
	ia.load(strPathToConfig);

	// Need a particular child of a Json file
	struct SWrapper
	{
		void Serialize(Serialization::IArchive& ar)
		{
			ar(version, "info");
		}

		SCryEngineVersion version;
	};

	SWrapper wrapper;
	ia(wrapper);
	return wrapper.version;
}

QString GetDefaultProjectsRootFolder()
{
	QStringList lst = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
	CRY_ASSERT(!lst.empty());

	return lst[0] + "/CRYENGINE Projects";
}

CryIcon GetProjectThumbnail(const string& projectFolder, const char* szDefaultIconFile)
{
	string thumbnailFileName = PathUtil::Make(projectFolder, "Thumbnail.png");
	if (FileUtils::FileExists(thumbnailFileName))
	{
		return CryIcon(thumbnailFileName.c_str());
	}

	return CryIcon(szDefaultIconFile);
}
