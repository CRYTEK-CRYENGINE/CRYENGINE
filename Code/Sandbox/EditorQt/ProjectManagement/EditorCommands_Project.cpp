// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ProjectManagement/Utils.h"
#include "QT/QtMainFrame.h"

#include <QProcess>

namespace Private_ProjectCommands
{

void RunProjectManager(CSelectProjectDialog::Tab tabToShow)
{
	string newProjectPath = AskUserToSpecifyProject(CEditorMainFrame::GetInstance(), false, tabToShow);
	if (newProjectPath.empty())
	{
		return;
	}

	QString cmd = '\"' + QCoreApplication::applicationDirPath() + "/Sandbox.exe\" -project " + newProjectPath.c_str();
	QProcess::startDetached(cmd);
}

void PyCreateProject()
{
	RunProjectManager(CSelectProjectDialog::Tab::Create);
}

void PyOpenProject()
{
	RunProjectManager(CSelectProjectDialog::Tab::Open);
}

}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ProjectCommands::PyCreateProject, project, new, CCommandDescription("Create New Project..."))
REGISTER_EDITOR_UI_COMMAND_DESC(project, new, "Project...", CKeyboardShortcut("Ctrl+Shift+N"), "", false)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ProjectCommands::PyOpenProject, project, open, CCommandDescription("Open Existing Project..."))
REGISTER_EDITOR_UI_COMMAND_DESC(project, open, "Project...", CKeyboardShortcut("Ctrl+Shift+O"), "", false)
