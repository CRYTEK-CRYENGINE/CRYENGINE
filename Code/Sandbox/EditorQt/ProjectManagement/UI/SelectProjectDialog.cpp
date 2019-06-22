// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectProjectDialog.h"

#include "ProjectManagement/Model/ProjectsModel.h"
#include "ProjectManagement/Model/TemplatesModel.h"
#include "ProjectManagement/UI/CreateProjectPanel.h"
#include "ProjectManagement/UI/OpenProjectPanel.h"

#include <Controls/SandboxWindowing.h>

#include <CryCore/Assert/CryAssert.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySystem/IProjectManager.h>

#include <QDateTime>
#include <QDir>
#include <QVBoxLayout>

namespace Private_SelectProjectDialog
{

void CopyFolderRecursively(const QString& src, const QString& dest)
{
	QDir srcDir(src);
	if (!srcDir.exists())
		return;

	for (const QString& subFolder : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		QString destSubfolder = dest + QDir::separator() + subFolder;
		srcDir.mkpath(destSubfolder);
		CopyFolderRecursively(src + QDir::separator() + subFolder, destSubfolder);
	}

	for (const QString& file : srcDir.entryList(QDir::Files))
	{
		QFile::copy(src + QDir::separator() + file, dest + QDir::separator() + file);
	}
}

} // namespace Private_SelectProjectDialog

CSelectProjectDialog::CSelectProjectDialog(QWidget* pParent, bool runOnSandboxInit, Tab tabToShow)
	: CEditorDialog("SelectProjectDialog", pParent)
{
	setWindowTitle("Project Browser");
	setWindowIcon(QIcon("icons:editor_icon.ico"));

	QTabWidget* pTabs = new QTabWidget(this);

	pTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pTabs->setTabsClosable(false);

	pTabs->addTab(new COpenProjectPanel(this, runOnSandboxInit), "Projects");
	pTabs->addTab(new CCreateProjectPanel(this, runOnSandboxInit), "New Project");

	switch (tabToShow)
	{
	case CSelectProjectDialog::Tab::Open:
		pTabs->setCurrentIndex(0);
		break;
	case CSelectProjectDialog::Tab::Create:
		pTabs->setCurrentIndex(1);
		break;
	default:
		CRY_ASSERT("Unhandled tab mode");
	}

	QVBoxLayout* pMainLayout = new QVBoxLayout;
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->setSpacing(0);
	pMainLayout->setMargin(0);
	pMainLayout->addWidget(pTabs);
	setLayout(pMainLayout);

	s_config[SANDBOX_WRAPPER_CLOSE_ICON] = CryIcon("icons:Window/Window_Close.ico");
}

CSelectProjectDialog::~CSelectProjectDialog()
{
	s_config.clear();
}

CProjectManager& CSelectProjectDialog::GetProjectManager()
{
	return m_projectManager;
}

CTemplateManager& CSelectProjectDialog::GetTemplateManager()
{
	return m_templateManager;
}

void CSelectProjectDialog::OpenProject(const SProjectDescription* pDescr)
{
	if (!pDescr || !pDescr->fullPathToCryProject)
	{
		return;
	}

	if (!m_projectManager.SetCurrentTime(*pDescr))
	{
		return;
	}

	m_pathToProject = pDescr->fullPathToCryProject;
	accept();
}

void CSelectProjectDialog::CreateAndOpenProject(const STemplateDescription& templateDescr, QString strOutputRoot, const QString& projectName)
{
	strOutputRoot = strOutputRoot + QDir::separator() + projectName;

	// 1. Copy content of the folder
	Private_SelectProjectDialog::CopyFolderRecursively(templateDescr.templateFolder.c_str(), strOutputRoot);

	// 2. Modify project file name,
	QString projFileName = strOutputRoot + QDir::separator() + templateDescr.cryprojFileName;

	Cry::SProject proj;
	{
		yasli::JSONIArchive ia;
		ia.load(projFileName.toStdString().c_str());
		ia(proj);
	}

	const string initialTemplateName = proj.name; // Store for Launcher's statistics
	proj.name = projectName.toStdString().c_str();

	{
		yasli::JSONOArchive oa;
		oa(proj);
		oa.save(projFileName.toStdString().c_str());
	}

	// 3. Fill descr
	const SCryEngineVersion engineVersion = GetCurrentCryEngineVersion();
	const uint currTime = QDateTime::currentDateTime().toTime_t();

	SProjectDescription descr;
	descr.id = std::to_string(currTime).c_str();
	descr.name = projectName.toStdString().c_str();
	descr.rootFolder = strOutputRoot.toStdString().c_str();
	descr.state = 1;

	descr.engineKey = engineVersion.id;
	descr.engineVersion = engineVersion.GetVersionShort();
	descr.engineBuild = engineVersion.GetBuild();
	descr.templateName = initialTemplateName;
	descr.templatePath = templateDescr.templatePath;

	descr.lastOpened = currTime;
	descr.language = templateDescr.language;
	descr.fullPathToCryProject = projFileName.toStdString().c_str();
	descr.icon = templateDescr.icon;

	m_projectManager.AddProject(descr);

	// 4. Save for Sandbox start, and close dialog
	m_pathToProject = projFileName.toStdString().c_str();
	accept();
}

string CSelectProjectDialog::GetPathToProject() const
{
	return '\"' + m_pathToProject + '\"';
}
