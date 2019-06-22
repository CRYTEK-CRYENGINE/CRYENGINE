// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectManager.h"
#include "ProjectManagement/UI/SelectProjectDialog.h"
#include "ProjectManagement/Utils.h"

#include <FileUtils.h>
#include <PathUtils.h>

#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySystem/IProjectManager.h>

#include <QDateTime>
#include <QDirIterator>

namespace Private_ProjectManager
{

string FindFirstFileInFolder(string folder, const char* szTemplate)
{
	//Non-recursive search (in root folder only) for a FIRST file with szTemplate
	const QStringList filter(szTemplate);
	QDirIterator it(folder.c_str(), filter, QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
	while (it.hasNext())
	{
		return it.next().toStdString().c_str();
	}

	return "";
}

struct JsonData
{
	void Serialize(Serialization::IArchive& ar)
	{
		if (!ar.isOutput())
		{
			// Loading
			int version = 0;
			ar(version, "version");
			switch (version)
			{
			case 1:
				ar(projects, "projects");
				break;
			}
		}
		else
		{
			// Saving only in the latest version
			ar(1, "version");
			ar(projects, "projects");
		}
	}

	std::vector<SProjectDescription> projects;
};

}

SProjectDescription::SProjectDescription()
	: state(0)
	, engineBuild(0)
	, lastOpened(0)
	, startupProject(false)
{
}

void SProjectDescription::Serialize(Serialization::IArchive& ar)
{
	ar(id, "id", "id");
	ar(name, "name", "name");

	if (ar.isOutput())
	{
		// Save in Launcher format
		string tmp = PathUtil::ToDosPath(rootFolder);
		ar(tmp, "path", "path");
	}
	else
	{
		// Convert on-load
		string tmp;
		ar(tmp, "path", "path");
		rootFolder = PathUtil::ToUnixPath(tmp);
	}

	ar(state, "state", "state");
	ar(language, "language", "Language");
	ar(templateName, "templateName", "Template Name");
	ar(templatePath, "templatePath", "Template Path");
	ar(engineKey, "engineKey", "Engine Key");
	ar(engineBuild, "engineBuild", "Engine Build");
	ar(engineVersion, "engineVersion", "Engine Version");
	ar(lastOpened, "lastOpened", "Last Opened");

	ar(fullPathToCryProject, "fullPathToCryProject", "Full Path To CryProject");
	if (ar.isInput())
	{
		fullPathToCryProject = PathUtil::ToUnixPath(fullPathToCryProject);
	}

	ar(startupProject, "startupProject", "Startup Project");

	if (ar.isInput())
	{
		string driveLetter;
		string directory;
		string originalFilename;
		string extension;

		PathUtil::SplitPath(fullPathToCryProject, driveLetter, directory, originalFilename, extension);
		icon = GetProjectThumbnail(driveLetter + directory, "icons:General/Project_Default.ico");
	}
}

int SProjectDescription::GetVersionMajor() const
{
	return engineVersion[0] - '0';
}

int SProjectDescription::GetVersionMinor() const
{
	return engineVersion[2] - '0';
}

bool SProjectDescription::IsValid() const
{
	// Format: "5.5"
	return engineVersion.length() == 3 && engineVersion[1] == '.';
}

bool SProjectDescription::FindAndUpdateCryProjPath()
{
	if (FileUtils::FileExists(fullPathToCryProject))
	{
		return true;
	}

	if (!FileUtils::FolderExists(rootFolder))
	{
		return false;
	}

	string foundPath = Private_ProjectManager::FindFirstFileInFolder(rootFolder, "*.cryproject");
	if (foundPath.empty())
	{
		return false;
	}

	// Update only in one case: when we are 100% sure, that the cryproject file exists
	fullPathToCryProject.swap(foundPath);
	return true;
}

//////////////////////////////////////////////////////////////////////////
CProjectManager::CProjectManager()
	: m_pathToProjectList(GetCryEngineProgramDataFolder() + "projects.json")
{
	Private_ProjectManager::JsonData jsonData;
	yasli::JSONIArchive ia;
	if (ia.load(m_pathToProjectList.c_str()))
	{
		ia(jsonData);
	}

	const SCryEngineVersion currEngineVersion = GetCurrentCryEngineVersion();

	for (auto& proj : jsonData.projects)
	{
		if (!proj.IsValid())
		{
			m_hiddenProjects.push_back(proj);
			continue;
		}

		if (currEngineVersion.GetVersionMajor() != proj.GetVersionMajor() || currEngineVersion.GetVersionMinor() != proj.GetVersionMinor())
		{
			m_hiddenProjects.push_back(proj);
			continue;
		}

		if (!proj.FindAndUpdateCryProjPath())
		{
			proj.startupProject = false; // Project will lost ability to be startup project
			m_hiddenProjects.push_back(proj);
			continue;
		}

		bool alreadyExists = false;
		for (const auto& it : m_projects)
		{
			if (it.fullPathToCryProject.compareNoCase(proj.fullPathToCryProject) == 0)
			{
				alreadyExists = true;
				break;
			}
		}

		if (!alreadyExists)
		{
			m_projects.push_back(proj);
		}
	}
}

const std::vector<SProjectDescription>& CProjectManager::GetProjects() const
{
	return m_projects;
}

const std::vector<SProjectDescription>& CProjectManager::GetHiddenProjects() const
{
	return m_hiddenProjects;
}

const SProjectDescription* CProjectManager::GetLastUsedProject() const
{
	auto it = std::max_element(m_projects.cbegin(), m_projects.cend(), [](const SProjectDescription& lhs, const SProjectDescription& rhs)
	{
		return lhs.lastOpened < rhs.lastOpened;
	});

	if (it == m_projects.cend())
	{
		return nullptr;
	}

	return &(*it);
}

string CProjectManager::GetStartupProject() const
{
	for (const auto& project : m_projects)
	{
		if (project.startupProject)
		{
			return project.fullPathToCryProject;
		}
	}

	return "";
}

void CProjectManager::ImportProject(const string& fullPathToProject)
{
	SProjectDescription descr = ParseProjectData(fullPathToProject);

	descr.id = std::to_string(QDateTime::currentDateTime().toTime_t()).c_str();
	descr.lastOpened = QDateTime::currentDateTime().toTime_t();
	descr.fullPathToCryProject = fullPathToProject;

	string driveLetter, directory, originalFilename, extension;
	PathUtil::SplitPath(fullPathToProject, driveLetter, directory, originalFilename, extension);
	descr.icon = GetProjectThumbnail(driveLetter + directory, "icons:General/Project_Default.ico");

	descr.state = 1; // Launcher's value == normal state

	// Can not be determined from existing cryproject file, will be empty.
	// They are used only by Launcher for statistics
	// descr.templateName;
	// descr.templatePath;
	// descr.language;

	AddProject(descr);
}

void CProjectManager::AddProject(const SProjectDescription& projectDescr)
{
	signalBeforeProjectsUpdated();
	m_projects.push_back(projectDescr);
	signalAfterProjectsUpdated();

	SaveProjectDescriptions();
}

SProjectDescription CProjectManager::ParseProjectData(const string& fullPathToProject) const
{
	QFileInfo fileInfo(fullPathToProject.c_str());

	yasli::JSONIArchive ia;
	ia.load(fullPathToProject);

	Cry::SProject proj;
	ia(proj);

	SProjectDescription descr;
	descr.name = proj.name;
	descr.rootFolder = fileInfo.absoluteDir().absolutePath().toStdString().c_str();

	// Fill with current CryEngine version of a Sandbox: user will be able to see it in a Project Browser
	// If project is not compatible, user will be able to remove it from "Projects" panel.
	const SCryEngineVersion currentEngineVersion = GetCurrentCryEngineVersion();
	descr.engineKey = currentEngineVersion.id;
	descr.engineVersion = currentEngineVersion.GetVersionShort();
	descr.engineBuild = currentEngineVersion.GetBuild();

	return descr;
}

void CProjectManager::ToggleStartupProperty(const SProjectDescription& projectDescr)
{
	auto it = std::find_if(m_projects.begin(), m_projects.end(), [projectDescr](SProjectDescription& curr)
	{
		return curr.fullPathToCryProject.CompareNoCase(projectDescr.fullPathToCryProject) == 0;
	});

	if (it == m_projects.end())
	{
		return;
	}

	signalBeforeProjectsUpdated();

	const bool newState = !(it->startupProject);
	if (newState)
	{
		// Reset "startup" flag on all
		for (auto& proj: m_projects)
		{
			proj.startupProject = false;
		}
	}

	it->startupProject = newState;

	signalAfterProjectsUpdated();

	SaveProjectDescriptions();
}

void CProjectManager::DeleteProject(const SProjectDescription& projectDescr, bool removeFromDisk)
{
	const auto it = std::find_if(m_projects.cbegin(), m_projects.cend(), [projectDescr](const SProjectDescription& curr)
	{
		return curr.fullPathToCryProject == projectDescr.fullPathToCryProject;
	});

	if (it == m_projects.cend())
	{
		return;
	}

	signalBeforeProjectsUpdated();
	m_projects.erase(it);
	signalAfterProjectsUpdated();

	SaveProjectDescriptions();

	if (removeFromDisk)
	{
		FileUtils::RemoveDirectory(projectDescr.rootFolder);
	}
}

bool CProjectManager::SetCurrentTime(const SProjectDescription& projectDescr)
{
	auto it = std::find_if(m_projects.begin(), m_projects.end(), [projectDescr](SProjectDescription& curr)
	{
		return curr.fullPathToCryProject.CompareNoCase(projectDescr.fullPathToCryProject) == 0;
	});

	if (it == m_projects.end())
	{
		return false;
	}

	it->lastOpened = QDateTime::currentDateTime().toTime_t();
	SaveProjectDescriptions();
	signalProjectDataChanged(&(*it));
	return true;
}

void CProjectManager::SaveProjectDescriptions()
{
	Private_ProjectManager::JsonData toSave;
	toSave.projects = m_projects;
	toSave.projects.insert(toSave.projects.end(), m_hiddenProjects.begin(), m_hiddenProjects.end());

	yasli::JSONOArchive oa;
	oa(toSave, "projects");
	oa.save(m_pathToProjectList.c_str());
}
