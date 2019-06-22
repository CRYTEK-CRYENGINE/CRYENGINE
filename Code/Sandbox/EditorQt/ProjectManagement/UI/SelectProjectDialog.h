// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "ProjectManagement/Data/ProjectManager.h"
#include "ProjectManagement/Data/TemplateManager.h"

#include <Controls/EditorDialog.h>

struct SProjectDescription;
struct STemplateDescription;

class CSelectProjectDialog : public CEditorDialog
{
	Q_OBJECT
public:
	enum class Tab
	{
		Open,
		Create,
	};

	CSelectProjectDialog(QWidget* pParent, bool runOnSandboxInit, Tab tabToShow);
	~CSelectProjectDialog();

	CProjectManager&  GetProjectManager();
	CTemplateManager& GetTemplateManager();

	void              OpenProject(const SProjectDescription* pDescr);
	void              CreateAndOpenProject(const STemplateDescription& templateDescr, QString strOutputRoot, const QString& projectName);

	string            GetPathToProject() const;

private:
	CProjectManager  m_projectManager;
	CTemplateManager m_templateManager;

	// The result of this dialog creation: path to the project, which the user has selected
	string m_pathToProject;
};
